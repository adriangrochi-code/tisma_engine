// The engine's C API (include/tisma/tisma_engine.h, docs/C_API.md): a
// context with its plate, its settings and its last result; the slice runs
// the command line's `slice` on a thread of the context's own, and reports
// its stages through a JobHook (slice/job.hpp). Own design.
//
// Open, settings, memory estimate, slice with progress and cancel, summary,
// G-code, the preview by layers, and the structural analysis of a project's
// object (the command line's `analyze <project> --sandbox`, its progress lines
// read as they come).

#include "cli/cli.hpp"
#include "config/settings.hpp"
#include "io/model.hpp"
#include "gcode/preview.hpp"
#include "slice/job.hpp"

#include <tisma/tisma_engine.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

struct tisma_ctx {
    // A folder of the context's own for the model given as bytes, the
    // settings and the results; removed with the context.
    fs::path dir;
    std::string model;      // the model's path; empty: nothing open
    std::size_t triangles = 0;
    double size[3] = {0.0, 0.0, 0.0}; // mm, the model's box
    std::optional<tisma::Settings> settings;
    bool project = false; // a 3MF: it can be sliced with its own settings
    uint64_t memory_limit = 0;

    // The job.
    tisma::JobHook hook;
    std::thread worker;
    std::atomic<int> state{TISMA_IDLE};
    std::mutex result_mutex; // what the worker leaves for the caller
    tisma_code job_code = TISMA_OK;
    std::string job_message;
    std::string summary; // JSON, when TISMA_DONE
    bool analysis = false; // the last job was an analysis, not a slice

    // The strings handed out, one of each kind.
    std::string message, settings_text, summary_out, error_out;
    std::string stage_out;

    // The finished slice's preview, read from its G-code the first time it
    // is asked for; dropped when another job starts.
    std::optional<tisma::GcodePreview> preview;
};

namespace {

tisma_status ok() { return {TISMA_OK, nullptr}; }

tisma_status fail(tisma_ctx* ctx, tisma_code code, std::string text) {
    if (!ctx) {
        static thread_local std::string bare;
        bare = std::move(text);
        return {code, bare.c_str()};
    }
    ctx->message = std::move(text);
    return {code, ctx->message.c_str()};
}

bool running(const tisma_ctx* ctx) { return ctx->state.load() == TISMA_RUNNING; }

// A finished slice (not an analysis).
bool slice_done(const tisma_ctx* ctx) { return ctx->state.load() == TISMA_DONE && !ctx->analysis; }

// A finished worker is joined before the context starts another or goes.
void join(tisma_ctx* ctx) {
    if (ctx->worker.joinable())
        ctx->worker.join();
}

fs::path make_dir() {
    std::random_device rd;
    std::error_code ec;
    for (int attempt = 0; attempt < 16; ++attempt) {
        const fs::path dir = fs::temp_directory_path(ec) / ("tisma-ctx-" + std::to_string(rd()) + std::to_string(rd()));
        if (!ec && fs::create_directories(dir, ec) && !ec)
            return dir;
    }
    return {};
}

// The model read to check it and to know its size; the slice reads it again.
tisma_status take_model(tisma_ctx* ctx, const fs::path& path, const tisma::ModelFile& model) {
    if (model.mesh.triangle_count() == 0)
        return fail(ctx, TISMA_BAD_MODEL, "the model has no triangles");
    float lo[3] = {INFINITY, INFINITY, INFINITY}, hi[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (std::size_t t = 0; t < model.mesh.triangle_count(); ++t)
        for (int k = 0; k < 3; ++k) {
            const tisma::Vec3f& c = model.mesh.corner(t, k);
            const float v[3] = {c.x, c.y, c.z};
            for (int a = 0; a < 3; ++a) {
                lo[a] = std::min(lo[a], v[a]);
                hi[a] = std::max(hi[a], v[a]);
            }
        }
    ctx->model = path.string();
    std::string extension = path.extension().string();
    for (char& ch : extension)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    ctx->project = extension == ".3mf";
    ctx->triangles = model.mesh.triangle_count();
    for (int a = 0; a < 3; ++a)
        ctx->size[a] = static_cast<double>(hi[a] - lo[a]);
    ctx->state.store(TISMA_IDLE);
    return ok();
}

// The first "error: ..." line the command wrote, without the prefix.
std::string first_error(const std::string& err) {
    std::istringstream in(err);
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("error: ", 0) == 0)
            return line.substr(7);
    return err.empty() ? "the slice failed" : err.substr(0, err.find('\n'));
}

tisma_code code_of(int exit_code) {
    switch (exit_code) {
    case tisma::kExitInput:
        return TISMA_CANNOT_READ;
    case tisma::kExitModel:
        return TISMA_BAD_MODEL;
    default:
        return TISMA_ERROR;
    }
}

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The summary the header promises, from the command's summary and warnings;
// the cost from the grams and extrusion.cost (money per kg), null when either
// is unknown.
std::string summary_json(const fs::path& summary, const fs::path& warnings, double cost_per_kg) {
    nlohmann::json s = nlohmann::json::parse(read_text(summary), nullptr, false);
    nlohmann::json w = nlohmann::json::parse(read_text(warnings), nullptr, false);
    nlohmann::json out{{"print_time_s", s.value("print_time_s", 0)},
                       {"filament_mm", s.value("filament_mm", 0.0)},
                       {"filament_g", s.contains("filament_g") ? s["filament_g"] : nlohmann::json(nullptr)},
                       {"layers", s.value("layers", 0)},
                       {"cost", s.contains("filament_g") && s["filament_g"].is_number() && cost_per_kg > 0.0
                                    ? nlohmann::json(std::round(s["filament_g"].get<double>() * cost_per_kg / 10.0) / 100.0)
                                    : nlohmann::json(nullptr)},
                       {"warnings", nlohmann::json::array()}};
    if (w.is_object() && w.contains("warnings") && w["warnings"].is_array())
        for (const nlohmann::json& one : w["warnings"])
            out["warnings"].push_back({{"kind", one.value("kind", "")},
                                       {"severity", one.value("severity", "warning")},
                                       {"message", one.value("message", "")},
                                       {"layer", one.contains("layer") ? one["layer"] : nlohmann::json(nullptr)}});
    return out.dump();
}

// The command's output, read line by line as it comes: each "progress N"
// line is the job's percent, and the cancel is looked at there, so a
// cancelled analysis stops at its next progress line.
class ProgressLines : public std::streambuf {
public:
    explicit ProgressLines(tisma::JobHook& hook) : hook_(hook) {}

protected:
    int overflow(int c) override {
        if (c == traits_type::eof())
            return traits_type::not_eof(c);
        if (c != '\n') {
            line_.push_back(static_cast<char>(c));
            return c;
        }
        if (line_.rfind("progress ", 0) == 0) {
            int v = 0;
            const char* begin = line_.data() + 9;
            const auto [end, ec] = std::from_chars(begin, line_.data() + line_.size(), v);
            if (ec == std::errc() && v > hook_.percent.load())
                hook_.percent.store(std::min(v, 99));
        }
        line_.clear();
        tisma::job_check();
        return c;
    }

private:
    tisma::JobHook& hook_;
    std::string line_;
};

void run_job(tisma_ctx* ctx, std::string model, double cost_per_kg) {
    const fs::path config = ctx->dir / "settings.json", gcode = ctx->dir / "result.gcode",
                   summary = ctx->dir / "summary.json", warnings = ctx->dir / "warnings.json";
    // A project without settings given: sliced with its own settings, as
    // the nodes slice it (cloud.md, rule 18), its progress lines read as they
    // come. Otherwise the model with the settings given.
    const bool own_settings = !ctx->settings;
    std::vector<std::string> args_text =
        own_settings ? std::vector<std::string>{"slice", model, "-o", gcode.string(), "--summary", summary.string(), "--sandbox"}
                     : std::vector<std::string>{"slice", model, "--config", config.string(), "-o", gcode.string(),
                                                "--summary", summary.string(), "--warnings", warnings.string()};
    std::vector<std::string_view> args(args_text.begin(), args_text.end());
    ProgressLines lines(ctx->hook);
    std::ostringstream plain;
    std::ostream progress_out(&lines);
    progress_out.exceptions(std::ios::badbit);
    std::ostream& out = own_settings ? progress_out : static_cast<std::ostream&>(plain);
    std::ostringstream err;
    tisma_code code = TISMA_OK;
    std::string message, result;
    int state = TISMA_DONE;
    try {
        tisma::JobScope scope(ctx->hook);
        const int rc = tisma::run_cli(args, out, err);
        if (rc != tisma::kExitOk) {
            state = TISMA_FAILED;
            code = code_of(rc);
            message = first_error(err.str());
        } else {
            result = summary_json(summary, warnings, cost_per_kg);
        }
    } catch (const tisma::JobCancelled&) {
        state = TISMA_STOPPED;
        code = TISMA_CANCELLED;
        message = "cancelled";
    } catch (const std::exception& e) {
        state = TISMA_FAILED;
        code = TISMA_ERROR;
        message = std::string("internal: ") + e.what();
    } catch (...) {
        state = TISMA_FAILED;
        code = TISMA_ERROR;
        message = "internal: unknown";
    }
    if (state != TISMA_DONE) {
        std::error_code ec;
        fs::remove(gcode, ec);
    }
    {
        std::lock_guard lock(ctx->result_mutex);
        ctx->job_code = code;
        ctx->job_message = std::move(message);
        ctx->summary = std::move(result);
    }
    if (state == TISMA_DONE)
        ctx->hook.percent.store(100);
    ctx->hook.stage.store(state == TISMA_DONE ? "done" : state == TISMA_STOPPED ? "cancelled" : "failed");
    ctx->state.store(state);
}

tisma_code analysis_code(const nlohmann::json& summary, int exit_code) {
    const std::string code = summary.is_object() ? summary.value("code", std::string()) : std::string();
    if (code == "needs_conditions")
        return TISMA_NEEDS_CONDITIONS;
    if (code == "too_large")
        return TISMA_TOO_LARGE;
    return code_of(exit_code);
}

void run_analysis_job(tisma_ctx* ctx, std::string project, int object, std::string quality) {
    const fs::path tea = ctx->dir / "result.tea", summary = ctx->dir / "analysis.json";
    std::vector<std::string> text{"analyze", project,  "-o",       tea.string(), "--summary", summary.string(),
                                  "--sandbox", "--mode", "analyze", "--object", std::to_string(object),
                                  "--instance", "0"};
    if (!quality.empty()) {
        text.push_back("--quality");
        text.push_back(quality);
    }
    std::vector<std::string_view> args(text.begin(), text.end());
    ProgressLines lines(ctx->hook);
    std::ostream out(&lines);
    out.exceptions(std::ios::badbit); // the cancel thrown from a line goes through
    std::ostringstream err;
    tisma_code code = TISMA_OK;
    std::string message, result;
    int state = TISMA_DONE;
    try {
        tisma::JobScope scope(ctx->hook);
        const int rc = tisma::run_cli(args, out, err);
        const nlohmann::json s = nlohmann::json::parse(read_text(summary), nullptr, false);
        if (rc != tisma::kExitOk) {
            state = TISMA_FAILED;
            code = analysis_code(s, rc);
            message = s.is_object() && s.contains("error") ? s["error"].get<std::string>() : first_error(err.str());
        } else {
            result = s.is_discarded() ? std::string() : s.dump();
            if (result.empty()) {
                state = TISMA_FAILED;
                code = TISMA_ERROR;
                message = "the analysis wrote no summary";
            }
        }
    } catch (const tisma::JobCancelled&) {
        state = TISMA_STOPPED;
        code = TISMA_CANCELLED;
        message = "cancelled";
    } catch (const std::exception& e) {
        state = TISMA_FAILED;
        code = TISMA_ERROR;
        message = std::string("internal: ") + e.what();
    } catch (...) {
        state = TISMA_FAILED;
        code = TISMA_ERROR;
        message = "internal: unknown";
    }
    {
        std::lock_guard lock(ctx->result_mutex);
        ctx->job_code = code;
        ctx->job_message = std::move(message);
        ctx->summary = std::move(result);
    }
    if (state == TISMA_DONE)
        ctx->hook.percent.store(100);
    ctx->hook.stage.store(state == TISMA_DONE ? "done" : state == TISMA_STOPPED ? "cancelled" : "failed");
    ctx->state.store(state);
}


} // namespace

extern "C" {

tisma_ctx* tisma_create(void) {
    try {
        auto ctx = std::make_unique<tisma_ctx>();
        ctx->dir = make_dir();
        if (ctx->dir.empty())
            return nullptr;
        return ctx.release();
    } catch (...) {
        return nullptr;
    }
}

void tisma_destroy(tisma_ctx* ctx) {
    if (!ctx)
        return;
    ctx->hook.cancel.store(true);
    join(ctx);
    std::error_code ec;
    fs::remove_all(ctx->dir, ec);
    delete ctx;
}

const char* tisma_version(void) { return TISMA_VERSION; }

tisma_status tisma_open_path(tisma_ctx* ctx, const char* path) {
    if (!ctx || !path)
        return fail(ctx, TISMA_BAD_ARGUMENT, "null context or path");
    if (running(ctx))
        return fail(ctx, TISMA_BUSY, "a job is running on this context");
    std::error_code ec;
    if (!fs::is_regular_file(path, ec))
        return fail(ctx, TISMA_CANNOT_READ, std::string("cannot open '") + path + "'");
    try {
        return take_model(ctx, path, tisma::read_model(path));
    } catch (const tisma::ModelError& e) {
        return fail(ctx, TISMA_BAD_MODEL, e.what());
    } catch (const std::exception& e) {
        return fail(ctx, TISMA_ERROR, e.what());
    }
}

tisma_status tisma_open_bytes(tisma_ctx* ctx, const void* data, size_t size, const char* name) {
    if (!ctx || (!data && size > 0) || !name)
        return fail(ctx, TISMA_BAD_ARGUMENT, "null context, data or name");
    if (running(ctx))
        return fail(ctx, TISMA_BUSY, "a job is running on this context");
    // Kept as a file of the context's: the slice reads its model from a path.
    const std::string extension = fs::path(name).extension().string();
    const fs::path path = ctx->dir / ("model" + extension);
    try {
        const std::string_view bytes(static_cast<const char*>(data), size);
        tisma::ModelFile model = tisma::parse_model(bytes, extension);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!file)
            return fail(ctx, TISMA_CANNOT_WRITE, "cannot keep the model in the context's folder");
        return take_model(ctx, path, model);
    } catch (const tisma::ModelError& e) {
        return fail(ctx, TISMA_BAD_MODEL, e.what());
    } catch (const std::exception& e) {
        return fail(ctx, TISMA_ERROR, e.what());
    }
}

tisma_status tisma_set_settings_json(tisma_ctx* ctx, const char* json) {
    if (!ctx || !json)
        return fail(ctx, TISMA_BAD_ARGUMENT, "null context or JSON");
    if (running(ctx))
        return fail(ctx, TISMA_BUSY, "a job is running on this context");
    try {
        ctx->settings = tisma::parse_settings(json);
        return ok();
    } catch (const std::exception& e) {
        return fail(ctx, TISMA_BAD_SETTINGS, e.what());
    }
}

const char* tisma_get_settings_json(tisma_ctx* ctx) {
    if (!ctx)
        return nullptr;
    try {
        ctx->settings_text = tisma::settings_json(ctx->settings ? *ctx->settings : tisma::Settings{});
    } catch (...) {
        ctx->settings_text.clear();
    }
    return ctx->settings_text.c_str();
}

// An upper estimate of the slice's peak memory, fitted to measurements on
// Linux x86_64 (PROGRESS.md, 2026-11-12): a base, the mesh's triangles, and
// the layers by the area of the model's box. 0 without a model.
uint64_t tisma_estimate_memory(tisma_ctx* ctx) {
    if (!ctx || ctx->model.empty())
        return 0;
    const double layer = ctx->settings && ctx->settings->layer_height > 0.0 ? ctx->settings->layer_height : 0.2;
    const double layers = std::ceil(ctx->size[2] / layer) + 1.0;
    const double bytes = 80.0 * 1024 * 1024 + 200.0 * static_cast<double>(ctx->triangles) +
                         16.0 * ctx->size[0] * ctx->size[1] * layers;
    return static_cast<uint64_t>(bytes);
}

void tisma_set_memory_limit(tisma_ctx* ctx, uint64_t bytes) {
    if (ctx)
        ctx->memory_limit = bytes;
}

tisma_status tisma_slice_start(tisma_ctx* ctx) {
    if (!ctx)
        return fail(ctx, TISMA_BAD_ARGUMENT, "null context");
    if (running(ctx))
        return fail(ctx, TISMA_BUSY, "a job is running on this context");
    join(ctx);
    if (ctx->model.empty())
        return fail(ctx, TISMA_NOTHING_TO_SLICE, "no model open");
    if (!ctx->settings && !ctx->project)
        return fail(ctx, TISMA_BAD_SETTINGS, "no settings: call tisma_set_settings_json first (a project has its own)");
    if (ctx->memory_limit > 0 && tisma_estimate_memory(ctx) > ctx->memory_limit)
        return fail(ctx, TISMA_TOO_LARGE, "the slice would take more memory than the limit set");
    if (ctx->settings) {
        // No post-processing scripts: an app has nobody at a terminal to
        // confirm them (gcode_templates.md, rules 16 and 17).
        tisma::Settings settings = *ctx->settings;
        settings.post_process.clear();
        std::ofstream config(ctx->dir / "settings.json", std::ios::binary | std::ios::trunc);
        config << tisma::settings_json(settings);
        if (!config)
            return fail(ctx, TISMA_CANNOT_WRITE, "cannot write the settings in the context's folder");
    }
    {
        std::lock_guard lock(ctx->result_mutex);
        ctx->job_code = TISMA_OK;
        ctx->job_message.clear();
        ctx->summary.clear();
    }
    ctx->hook.cancel.store(false);
    ctx->hook.percent.store(0);
    ctx->hook.stage.store("loading");
    ctx->preview.reset();
    ctx->analysis = false;
    ctx->state.store(TISMA_RUNNING);
    try {
        ctx->worker = std::thread(run_job, ctx, ctx->model, ctx->settings ? ctx->settings->filament_cost : 0.0);
    } catch (const std::exception& e) {
        ctx->state.store(TISMA_FAILED);
        return fail(ctx, TISMA_ERROR, std::string("cannot start the job: ") + e.what());
    }
    return ok();
}

tisma_progress tisma_poll(tisma_ctx* ctx) {
    tisma_progress p{TISMA_IDLE, 0, "idle", {TISMA_OK, nullptr}};
    if (!ctx)
        return p;
    p.state = static_cast<tisma_state>(ctx->state.load());
    p.percent = ctx->hook.percent.load();
    ctx->stage_out = ctx->hook.stage.load();
    p.stage = ctx->stage_out.c_str();
    if (p.state == TISMA_FAILED || p.state == TISMA_STOPPED) {
        std::lock_guard lock(ctx->result_mutex);
        ctx->error_out = ctx->job_message;
        p.error = {ctx->job_code, ctx->error_out.c_str()};
    }
    return p;
}

void tisma_cancel(tisma_ctx* ctx) {
    if (ctx && running(ctx))
        ctx->hook.cancel.store(true);
}

const char* tisma_summary_json(tisma_ctx* ctx) {
    if (!ctx || !slice_done(ctx))
        return nullptr;
    std::lock_guard lock(ctx->result_mutex);
    ctx->summary_out = ctx->summary;
    return ctx->summary_out.c_str();
}

tisma_status tisma_write_gcode(tisma_ctx* ctx, const char* path) {
    if (!ctx || !path)
        return fail(ctx, TISMA_BAD_ARGUMENT, "null context or path");
    if (!slice_done(ctx))
        return fail(ctx, TISMA_NOT_READY, "no finished slice");
    std::error_code ec;
    fs::copy_file(ctx->dir / "result.gcode", path, fs::copy_options::overwrite_existing, ec);
    if (ec)
        return fail(ctx, TISMA_CANNOT_WRITE, std::string("cannot write '") + path + "': " + ec.message());
    return ok();
}

tisma_status tisma_read_gcode(tisma_ctx* ctx, uint64_t offset, char* buffer, size_t capacity, size_t* written) {
    if (!ctx || !written || (!buffer && capacity > 0))
        return fail(ctx, TISMA_BAD_ARGUMENT, "null context, buffer or written");
    *written = 0;
    if (!slice_done(ctx))
        return fail(ctx, TISMA_NOT_READY, "no finished slice");
    std::ifstream in(ctx->dir / "result.gcode", std::ios::binary);
    if (!in)
        return fail(ctx, TISMA_CANNOT_READ, "the G-code is gone from the context's folder");
    in.seekg(static_cast<std::streamoff>(offset));
    if (!in)
        return ok(); // past the end
    in.read(buffer, static_cast<std::streamsize>(capacity));
    *written = static_cast<size_t>(in.gcount());
    return ok();
}

// The preview of the finished slice: its G-code read as the desktop's
// preview reads it (gcode/preview.hpp), once. Null without a finished slice
// or when the G-code cannot be read.
static const tisma::GcodePreview* preview_of(tisma_ctx* ctx) {
    if (!ctx || !slice_done(ctx))
        return nullptr;
    if (!ctx->preview) {
        const std::string text = read_text(ctx->dir / "result.gcode");
        if (text.empty())
            return nullptr;
        const double diameter = ctx->settings ? ctx->settings->filament_diameter : 1.75;
        ctx->preview = tisma::read_gcode_preview(text, diameter > 0.0 ? diameter : 1.75);
    }
    return &*ctx->preview;
}

static uint8_t path_type(tisma::LineKind kind) {
    using K = tisma::LineKind;
    switch (kind) {
    case K::Travel:
        return TISMA_PATH_TRAVEL;
    case K::OuterWall:
        return TISMA_PATH_OUTER_WALL;
    case K::InnerWall:
    case K::OverhangWall:
        return TISMA_PATH_INNER_WALL;
    case K::SparseInfill:
        return TISMA_PATH_SPARSE_INFILL;
    case K::SolidInfill:
        return TISMA_PATH_SOLID_INFILL;
    case K::TopSolidInfill:
        return TISMA_PATH_TOP;
    case K::BridgeInfill:
        return TISMA_PATH_BRIDGE;
    case K::Support:
    case K::SupportInterface:
        return TISMA_PATH_SUPPORT;
    case K::SkirtBrim:
        return TISMA_PATH_SKIRT;
    default:
        return TISMA_PATH_OTHER;
    }
}

int tisma_layer_count(tisma_ctx* ctx) {
    const tisma::GcodePreview* p = preview_of(ctx);
    return p ? static_cast<int>(p->layers.size()) : 0;
}

double tisma_layer_z(tisma_ctx* ctx, int layer) {
    const tisma::GcodePreview* p = preview_of(ctx);
    if (!p || layer < 0 || static_cast<std::size_t>(layer) >= p->layers.size())
        return 0.0;
    return p->layers[static_cast<std::size_t>(layer)].z;
}

tisma_status tisma_preview_segments(tisma_ctx* ctx, int first, int count, size_t skip, tisma_segment* out,
                                    size_t capacity, size_t* written) {
    if (written)
        *written = 0;
    if (!ctx || !written || (!out && capacity > 0) || first < 0 || count < 0)
        return fail(ctx, TISMA_BAD_ARGUMENT, "null context, output or written, or a negative layer or count");
    if (!slice_done(ctx))
        return fail(ctx, TISMA_NOT_READY, "no finished slice");
    const tisma::GcodePreview* p = preview_of(ctx);
    if (!p)
        return fail(ctx, TISMA_CANNOT_READ, "the G-code is gone from the context's folder");
    const std::size_t from = std::min(static_cast<std::size_t>(first), p->layers.size());
    const std::size_t to = std::min(from + static_cast<std::size_t>(count), p->layers.size());
    std::size_t index = 0, n = 0;
    for (std::size_t l = from; l < to && n < capacity; ++l) {
        const tisma::PreviewLayer& layer = p->layers[l];
        if (index + layer.count <= skip) { // the whole layer was handed already
            index += layer.count;
            continue;
        }
        for (std::size_t m = 0; m < layer.count && n < capacity; ++m, ++index) {
            if (index < skip)
                continue;
            const tisma::PreviewMove& move = p->moves[layer.first + m];
            out[n++] = {move.x0, move.y0, move.z0, move.x1, move.y1, move.z1,
                        move.width, move.height, move.feed, path_type(move.kind)};
        }
    }
    *written = n;
    return ok();
}

tisma_status tisma_analyze_start(tisma_ctx* ctx, int object, const char* quality) {
    if (!ctx || object < 0)
        return fail(ctx, TISMA_BAD_ARGUMENT, "null context or a negative object");
    if (running(ctx))
        return fail(ctx, TISMA_BUSY, "a job is running on this context");
    join(ctx);
    if (ctx->model.empty())
        return fail(ctx, TISMA_NOTHING_TO_SLICE, "no project open");
    std::string q = quality ? quality : "";
    if (!q.empty() && q != "fast" && q != "normal" && q != "high" && q != "extreme" && q != "ultra")
        return fail(ctx, TISMA_BAD_ARGUMENT, "the quality is fast, normal, high or extreme");
    {
        std::lock_guard lock(ctx->result_mutex);
        ctx->job_code = TISMA_OK;
        ctx->job_message.clear();
        ctx->summary.clear();
    }
    ctx->hook.cancel.store(false);
    ctx->hook.percent.store(0);
    ctx->hook.stage.store("solving");
    ctx->preview.reset();
    ctx->analysis = true;
    ctx->state.store(TISMA_RUNNING);
    try {
        ctx->worker = std::thread(run_analysis_job, ctx, ctx->model, object, q);
    } catch (const std::exception& e) {
        ctx->state.store(TISMA_FAILED);
        return fail(ctx, TISMA_ERROR, std::string("cannot start the job: ") + e.what());
    }
    return ok();
}

const char* tisma_analysis_json(tisma_ctx* ctx) {
    if (!ctx || ctx->state.load() != TISMA_DONE || !ctx->analysis)
        return nullptr;
    std::lock_guard lock(ctx->result_mutex);
    ctx->summary_out = ctx->summary;
    return ctx->summary_out.c_str();
}

} // extern "C"
