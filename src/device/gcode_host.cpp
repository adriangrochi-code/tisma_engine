#include "device/gcode_host.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace tisma::device {
namespace {

using namespace std::chrono_literals;

bool starts_with(std::string_view text, std::string_view head) { return text.substr(0, head.size()) == head; }

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

// A number at `at` (blanks allowed first); where it ends, or nullopt.
std::optional<double> number_at(std::string_view line, std::size_t& at) {
    while (at < line.size() && line[at] == ' ')
        ++at;
    const char* begin = line.data() + at;
    const char* end = line.data() + line.size();
    double v = 0.0;
    const auto r = std::from_chars(begin, end, v);
    if (r.ec != std::errc() || r.ptr == begin)
        return std::nullopt;
    at = std::size_t(r.ptr - line.data());
    return v;
}

// "<key><actual> /<target>" with the key at the start of a word.
std::optional<Temperature> temperature_after(std::string_view line, std::string_view key) {
    for (std::size_t at = line.find(key); at != std::string_view::npos; at = line.find(key, at + 1)) {
        if (at > 0 && line[at - 1] != ' ')
            continue;
        std::size_t p = at + key.size();
        const std::optional<double> actual = number_at(line, p);
        if (!actual)
            continue;
        Temperature t{*actual, 0.0};
        while (p < line.size() && line[p] == ' ')
            ++p;
        if (p < line.size() && line[p] == '/') {
            ++p;
            if (const std::optional<double> target = number_at(line, p))
                t.target = *target;
        }
        return t;
    }
    return std::nullopt;
}

} // namespace

std::uint8_t gcode_checksum(std::string_view line) {
    std::uint8_t cs = 0;
    for (const char c : line)
        cs ^= static_cast<std::uint8_t>(c);
    return cs;
}

std::string numbered_line(std::uint32_t number, std::string_view command) {
    std::string line = "N" + std::to_string(number) + " ";
    line.append(command);
    return line + "*" + std::to_string(int(gcode_checksum(line)));
}

std::string strip_gcode_line(std::string_view line) {
    if (const std::size_t semicolon = line.find(';'); semicolon != std::string_view::npos)
        line = line.substr(0, semicolon);
    return std::string(trim(line));
}

std::optional<Temperatures> parse_temperatures(std::string_view line) {
    Temperatures t;
    t.nozzle = temperature_after(line, "T:");
    if (!t.nozzle)
        t.nozzle = temperature_after(line, "T0:");
    t.bed = temperature_after(line, "B:");
    if (!t.nozzle && !t.bed)
        return std::nullopt;
    return t;
}

std::optional<std::array<double, 4>> parse_position(std::string_view line) {
    std::array<double, 4> p{};
    const char* keys[4] = {"X:", "Y:", "Z:", "E:"};
    std::size_t at = 0;
    for (int i = 0; i < 4; ++i) {
        const std::size_t k = line.find(keys[i], at);
        if (k == std::string_view::npos || (k > 0 && line[k - 1] != ' '))
            return std::nullopt;
        std::size_t q = k + 2;
        const std::optional<double> v = number_at(line, q);
        if (!v)
            return std::nullopt;
        p[std::size_t(i)] = *v;
        at = q;
    }
    return p;
}

std::optional<std::uint32_t> parse_resend(std::string_view line) {
    std::size_t at = 0;
    if (starts_with(line, "Resend:"))
        at = 7;
    else if (starts_with(line, "rs "))
        at = 3;
    else
        return std::nullopt;
    const std::optional<double> n = number_at(line, at);
    // A line number: whole, and one a 32-bit counter holds (NaN, infinity
    // and 1e30 were undefined to convert).
    if (!n || !(*n >= 0.0 && *n <= 4294967295.0) || *n != std::floor(*n))
        return std::nullopt;
    return std::uint32_t(*n);
}

bool is_ok(std::string_view line) {
    return starts_with(line, "ok") && (line.size() == 2 || line[2] == ' ' || line[2] == ':' || line[2] == '\t');
}

// --- the host ---------------------------------------------------------------

GcodeHost::GcodeHost(std::shared_ptr<ByteStream> stream, HostOptions options)
    : stream_(std::move(stream)), options_(options) {}

GcodeHost::~GcodeHost() { disconnect(); }

bool GcodeHost::connect() {
    if (!stream_ || !stream_->is_open())
        return false;
    if (thread_.joinable()) {
        const PrinterState s = status().state;
        if (s != PrinterState::Disconnected && s != PrinterState::Error)
            return true; // already connected or connecting
        thread_.join();
    }
    stop_ = pause_ = cancel_ = false;
    {
        std::lock_guard lock(mutex_);
        status_ = PrinterStatus{};
        status_.state = PrinterState::Connecting;
    }
    thread_ = std::thread([this] { run(); });
    return true;
}

void GcodeHost::disconnect() {
    stop_ = true;
    if (thread_.joinable())
        thread_.join();
}

PrinterStatus GcodeHost::status() const {
    std::lock_guard lock(mutex_);
    return status_;
}

std::vector<PrinterEvent> GcodeHost::take_events() {
    std::lock_guard lock(mutex_);
    return std::exchange(events_, {});
}

bool GcodeHost::start_print(std::string gcode, std::string name) {
    Job job;
    job.name = std::move(name);
    std::size_t at = 0;
    while (at <= gcode.size()) {
        const std::size_t nl = gcode.find('\n', at);
        const std::string_view raw(gcode.data() + at, (nl == std::string::npos ? gcode.size() : nl) - at);
        std::string line = strip_gcode_line(raw);
        if (!line.empty())
            job.lines.push_back(std::move(line));
        if (nl == std::string::npos)
            break;
        at = nl + 1;
    }
    std::lock_guard lock(mutex_);
    if (status_.state != PrinterState::Idle || job_ || next_job_)
        return false;
    next_job_ = std::move(job);
    return true;
}

void GcodeHost::pause() { pause_ = true; }
void GcodeHost::resume() { pause_ = false; }
void GcodeHost::cancel() { cancel_ = true; }

bool GcodeHost::send(std::string command) {
    command = strip_gcode_line(command);
    if (command.empty())
        return false;
    std::lock_guard lock(mutex_);
    if (status_.state == PrinterState::Disconnected || status_.state == PrinterState::Error)
        return false;
    commands_.push_back(std::move(command));
    return true;
}

void GcodeHost::change(PrinterState state, PrinterEvent::Kind kind, std::string text) {
    std::lock_guard lock(mutex_);
    status_.state = state;
    events_.push_back({kind, std::move(text)});
}

void GcodeHost::push_event(PrinterEvent::Kind kind, std::string text) {
    std::lock_guard lock(mutex_);
    events_.push_back({kind, std::move(text)});
}

std::optional<std::string> GcodeHost::read_line(std::chrono::milliseconds timeout) {
    for (;;) {
        if (const std::size_t nl = pending_.find('\n'); nl != std::string::npos) {
            std::string line = pending_.substr(0, nl);
            pending_.erase(0, nl + 1);
            return std::string(trim(line));
        }
        char buffer[512];
        const int n = stream_->read(buffer, sizeof buffer, timeout);
        if (n < 0) {
            // The other side is gone (unplugged): closed, so that is_open()
            // says so, instead of reading nothing forever at full speed.
            stream_->close();
            return std::nullopt;
        }
        if (n == 0)
            return std::nullopt;
        pending_.append(buffer, std::size_t(n));
    }
}

void GcodeHost::handle_report(std::string_view line) {
    if (const std::optional<Temperatures> t = parse_temperatures(line)) {
        std::lock_guard lock(mutex_);
        if (t->nozzle)
            status_.nozzle = *t->nozzle;
        if (t->bed)
            status_.bed = *t->bed;
        last_temperatures_ = std::chrono::steady_clock::now();
        return;
    }
    if (const std::optional<std::array<double, 4>> p = parse_position(line)) {
        std::lock_guard lock(mutex_);
        status_.position = *p;
        return;
    }
    if (starts_with(line, "FIRMWARE_NAME:")) {
        std::string_view name = line.substr(14);
        for (const char* stop : {" SOURCE_CODE_URL", " FIRMWARE_URL", " PROTOCOL_VERSION", " ELECTRONICS"})
            if (const std::size_t at = name.find(stop); at != std::string_view::npos)
                name = name.substr(0, at);
        std::lock_guard lock(mutex_);
        status_.firmware = std::string(trim(name));
        return;
    }
    if (line.find("Unknown command") != std::string_view::npos)
        unknown_command_ = true;
}

GcodeHost::Reply GcodeHost::send_line(const std::string& command) {
    // Heating waits answer only once hot: their temperature reports show the
    // printer is alive.
    const bool heating = starts_with(command, "M109") || starts_with(command, "M190") ||
                         starts_with(command, "M191") || starts_with(command, "M116");
    for (int attempt = 0, probes = 0;;) {
        const std::string line = numbered_line(line_number_, command);
        if (!stream_->write(line + "\n"))
            return Reply::Gone;
        auto deadline = std::chrono::steady_clock::now() + options_.ok_timeout;
        bool resend = false;
        while (!resend) {
            if (stop_)
                return Reply::Gone;
            const std::optional<std::string> got = read_line(100ms);
            if (!got) {
                if (!stream_->is_open())
                    return Reply::Gone;
                if (std::chrono::steady_clock::now() > deadline) {
                    // No "ok": the line or its answer was lost. The same
                    // line again asks: a firmware that never got it runs it
                    // and answers; one that did asks for the next line
                    // (handled below), so nothing runs twice.
                    if (++probes > 2)
                        return Reply::Timeout;
                    if (!stream_->write(line + "\n"))
                        return Reply::Gone;
                    deadline = std::chrono::steady_clock::now() + options_.ok_timeout;
                }
                continue;
            }
            const std::string& r = *got;
            if (const std::optional<std::uint32_t> n = parse_resend(r)) {
                // The firmware acknowledges its own request with an "ok":
                // swallowed, then the line goes again with the number it wants.
                const auto until = std::chrono::steady_clock::now() + 500ms;
                while (std::chrono::steady_clock::now() < until) {
                    const std::optional<std::string> ack = read_line(50ms);
                    if (ack && is_ok(*ack))
                        break;
                    if (ack)
                        handle_report(*ack);
                }
                if (*n == line_number_ + 1) {
                    // It already ran this line (its "ok" was lost) and wants
                    // the next one.
                    ++line_number_;
                    return Reply::Ok;
                }
                if (*n != line_number_) {
                    // Only the line in flight can be sent again.
                    std::lock_guard lock(mutex_);
                    status_.error = "the printer asked for line " + std::to_string(*n) + ", which is no longer at hand";
                    return Reply::Timeout;
                }
                {
                    std::lock_guard lock(mutex_);
                    ++status_.resends;
                }
                if (++attempt > options_.max_resends) {
                    std::lock_guard lock(mutex_);
                    status_.error = "the printer keeps asking for the same line";
                    return Reply::Timeout;
                }
                resend = true;
            } else if (is_ok(r)) {
                handle_report(r);
                ++line_number_;
                return Reply::Ok;
            } else if (starts_with(r, "echo:busy") || (heating && parse_temperatures(r))) {
                if (heating)
                    handle_report(r);
                deadline = std::chrono::steady_clock::now() + options_.ok_timeout;
            } else if (starts_with(r, "Error:")) {
                // A broken line is routine (a Resend follows); anything else is told.
                if (r.find("checksum") == std::string::npos && r.find("Line Number") == std::string::npos &&
                    r.find("No Checksum") == std::string::npos) {
                    {
                        std::lock_guard lock(mutex_);
                        status_.error = r;
                    }
                    push_event(PrinterEvent::Kind::Error, r);
                }
            } else {
                handle_report(r);
            }
        }
    }
}

bool GcodeHost::handshake() {
    // Wake the firmware and restart the numbering: an unnumbered M110 N0,
    // repeated until the first "ok" (a resetting board takes a while).
    stream_->write("\n");
    const auto deadline = std::chrono::steady_clock::now() + options_.connect_timeout;
    auto next_try = std::chrono::steady_clock::now();
    for (;;) {
        if (stop_ || !stream_->is_open())
            return false;
        if (std::chrono::steady_clock::now() >= next_try) {
            if (!stream_->write("M110 N0\n"))
                return false;
            next_try = std::chrono::steady_clock::now() + 1000ms;
        }
        if (const std::optional<std::string> got = read_line(100ms)) {
            if (is_ok(*got))
                break;
            handle_report(*got);
        } else if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
    }
    line_number_ = 1;
    if (send_line("M115") != Reply::Ok)
        return false;
    unknown_command_ = false;
    if (send_line("M155 S2") != Reply::Ok) // temperatures by themselves, every 2 s
        return false;
    auto_report_ = !unknown_command_;
    if (send_line("M105") != Reply::Ok) // the temperatures now, not at the first report
        return false;
    last_temperatures_ = std::chrono::steady_clock::now();
    return true;
}

void GcodeHost::finish_job(PrinterEvent::Kind how) {
    std::string name;
    {
        std::lock_guard lock(mutex_);
        name = job_ ? job_->name : status_.job;
        if (how == PrinterEvent::Kind::Finished)
            status_.progress = 1.0;
        status_.state = PrinterState::Idle;
        events_.push_back({how, name});
        job_.reset(); // under the lock: start_print reads it from the interface's thread
    }
    // A pause or cancel asked as the job ended is not for the next one.
    pause_ = false;
    cancel_ = false;
}

void GcodeHost::drain_commands() {
    std::deque<std::string> commands;
    {
        std::lock_guard lock(mutex_);
        commands.swap(commands_);
    }
    for (const std::string& c : commands)
        if (send_line(c) == Reply::Gone) {
            stop_ = true;
            return;
        }
}

void GcodeHost::run() {
    if (!handshake()) {
        {
            std::lock_guard lock(mutex_);
            status_.error = stop_ ? "" : "no answer from the printer";
            status_.state = stop_ ? PrinterState::Disconnected : PrinterState::Error;
            events_.push_back({stop_ ? PrinterEvent::Kind::Disconnected : PrinterEvent::Kind::Error,
                               "no answer from the printer"});
        }
        stream_->close();
        return;
    }
    change(PrinterState::Idle, PrinterEvent::Kind::Connected, status().firmware);
    bool lost = false;
    while (!stop_) {
        drain_commands();
        if (stop_)
            break;
        {
            std::lock_guard lock(mutex_);
            if (!job_ && next_job_) {
                job_ = std::move(*next_job_);
                next_job_.reset();
                pause_ = false;
                cancel_ = false;
                job_->started = std::chrono::steady_clock::now();
                status_.job = job_->name;
                status_.lines_total = job_->lines.size();
                status_.lines_sent = 0;
                status_.progress = 0.0;
                status_.elapsed = 0.0;
                status_.state = PrinterState::Printing;
                events_.push_back({PrinterEvent::Kind::Started, job_->name});
            }
        }
        if (job_) {
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - job_->started).count();
            if (cancel_) {
                cancel_ = false;
                finish_job(PrinterEvent::Kind::Cancelled);
                for (const char* c : {"M104 S0", "M140 S0", "M107", "M84"})
                    if (send_line(c) == Reply::Gone) {
                        lost = true;
                        break;
                    }
                if (lost)
                    break;
                continue;
            }
            if (pause_) {
                if (status().state != PrinterState::Paused) {
                    change(PrinterState::Paused, PrinterEvent::Kind::Paused, job_->name);
                }
            } else if (status().state == PrinterState::Paused) {
                change(PrinterState::Printing, PrinterEvent::Kind::Resumed, job_->name);
            }
            if (status().state == PrinterState::Printing) {
                if (job_->next >= job_->lines.size()) {
                    finish_job(PrinterEvent::Kind::Finished);
                    continue;
                }
                const Reply r = send_line(job_->lines[job_->next]);
                if (r == Reply::Ok) {
                    ++job_->next;
                    std::lock_guard lock(mutex_);
                    status_.lines_sent = job_->next;
                    status_.progress = double(job_->next) / double(std::max<std::size_t>(job_->lines.size(), 1));
                    status_.elapsed = elapsed;
                    continue;
                }
                if (r == Reply::Gone) {
                    lost = true;
                    break;
                }
                // Timeout, or a line the printer never accepts: the job stops.
                {
                    std::lock_guard lock(mutex_);
                    if (status_.error.empty())
                        status_.error = "the printer stopped answering";
                }
                push_event(PrinterEvent::Kind::Error, status().error);
                finish_job(PrinterEvent::Kind::Cancelled);
                // The heaters off, as a cancel does, without waiting for answers
                // that may not come: firmwares take unnumbered lines at any time.
                if (!stream_->write("M104 S0\nM140 S0\nM107\n")) {
                    lost = true;
                    break;
                }
                continue;
            }
            std::lock_guard lock(mutex_);
            status_.elapsed = elapsed;
        }
        // Idle, or paused: keep the temperatures coming.
        if (!auto_report_ && std::chrono::steady_clock::now() - last_temperatures_ > options_.temperature_period) {
            if (send_line("M105") == Reply::Gone) {
                lost = true;
                break;
            }
            last_temperatures_ = std::chrono::steady_clock::now();
        } else if (const std::optional<std::string> got = read_line(50ms)) {
            handle_report(*got);
        } else if (!stream_->is_open()) {
            lost = true;
            break;
        }
    }
    if (job_)
        finish_job(PrinterEvent::Kind::Cancelled);
    stream_->close();
    {
        std::lock_guard lock(mutex_);
        if (lost && !stop_) {
            status_.error = "the connection was lost";
            status_.state = PrinterState::Error;
            events_.push_back({PrinterEvent::Kind::Error, "the connection was lost"});
        } else {
            status_.state = PrinterState::Disconnected;
            events_.push_back({PrinterEvent::Kind::Disconnected, ""});
        }
    }
}

} // namespace tisma::device
