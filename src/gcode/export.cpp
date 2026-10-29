// Writing G-code that is checked before it gets its name. Own design; see
// PROVENANCE.md.

#include "gcode/export.hpp"

#include "gcode/binary.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"

#include <chrono>
#include <exception>
#include <fstream>
#include <system_error>

namespace tisma {
namespace {

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

} // namespace

GcodeExport export_gcode(const std::filesystem::path& path, const SlicedModel& model, const Settings& settings,
                         std::vector<Warning>* warnings) {
    GcodeExport out;
    std::filesystem::path part = path, rejected = path;
    part += ".part";
    rejected += ".rejected";
    std::error_code ignored;
    // Whatever happens next, nothing is left under the requested name: an
    // older G-code there would be printed by mistake (docs/GCODE_CHECK.md).
    std::filesystem::remove(path, ignored);
    const auto start = std::chrono::steady_clock::now();
    {
        std::ofstream file(part, std::ios::binary);
        if (!file) {
            out.error = "cannot write '" + part.string() + "'";
            return out;
        }
        try {
            write_gcode(file, model, settings, warnings);
        } catch (const std::exception& e) {
            file.close();
            std::filesystem::remove(part, ignored);
            out.error = std::string("writing the G-code failed: ") + e.what();
            return out;
        }
        file.close();
        if (!file) {
            std::filesystem::remove(part, ignored);
            out.error = "writing '" + part.string() + "' failed (is the disk full?)";
            return out;
        }
    }
    out.write_seconds = seconds_since(start);

    // Check what is on the disk, not what was meant to be.
    const auto check_start = std::chrono::steady_clock::now();
    std::string text;
    {
        // In one read: character by character took a tenth of the export.
        std::ifstream file(part, std::ios::binary | std::ios::ate);
        const std::streamoff size = file ? std::streamoff(file.tellg()) : -1;
        if (size >= 0) {
            text.resize(std::size_t(size));
            file.seekg(0);
            file.read(text.data(), size);
        }
        if (!file || size < 0) {
            out.error = "cannot read back '" + part.string() + "'";
            return out;
        }
    }
    out.bytes = text.size();
    out.check = check_gcode(text, gcode_limits(model, settings));
    out.check_seconds = seconds_since(check_start);

    std::error_code ec;
    // The binary format (gcode_extras.md, rule 9): from the G-code that passed
    // its check, read back too, and its G-code the same text.
    if (out.check.ok() && settings.gcode_binary) {
        const std::string binary = binary_gcode(text, settings);
        {
            std::ofstream file(part, std::ios::binary | std::ios::trunc);
            file.write(binary.data(), std::streamsize(binary.size()));
            file.close();
            if (!file) {
                std::filesystem::remove(part, ignored);
                out.error = "writing '" + part.string() + "' failed (is the disk full?)";
                return out;
            }
        }
        std::string back;
        {
            std::ifstream file(part, std::ios::binary | std::ios::ate);
            const std::streamoff size = file ? std::streamoff(file.tellg()) : -1;
            back.resize(size > 0 ? std::size_t(size) : 0);
            file.seekg(0);
            file.read(back.data(), std::streamsize(back.size()));
        }
        try {
            const BinaryGcode read = read_binary_gcode(back);
            if (!text.starts_with(read.gcode) || read.gcode.empty())
                throw std::runtime_error("its G-code is not the one checked");
        } catch (const std::exception& e) {
            std::filesystem::rename(part, rejected, ec);
            out.error = std::string("the binary G-code does not read back: ") + e.what() + "; kept as '" + rejected.string() + "'";
            return out;
        }
        out.bytes = back.size();
    }
    if (!out.check.ok()) {
        std::filesystem::rename(part, rejected, ec);
        out.error = "the G-code failed its check (" + std::to_string(out.check.problem_count) + " problems; first, line " +
                    std::to_string(out.check.problems.front().line) + ": " + out.check.problems.front().message +
                    "); kept as '" + rejected.string() + "'";
        return out;
    }
    std::filesystem::rename(part, path, ec);
    if (ec) {
        std::filesystem::remove(part, ignored);
        out.error = "cannot rename '" + part.string() + "' to '" + path.string() + "': " + ec.message();
        return out;
    }
    out.written = true;
    return out;
}

} // namespace tisma
