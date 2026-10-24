#include "device/simulator.hpp"

#include "device/gcode_host.hpp"

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <optional>

namespace tisma::device {
namespace {

// The number after a letter in a command ("S210" in "M104 S210").
std::optional<double> argument(std::string_view command, char letter) {
    for (std::size_t i = 0; i < command.size(); ++i) {
        if (command[i] != letter || (i > 0 && command[i - 1] != ' '))
            continue;
        double v = 0.0;
        const char* begin = command.data() + i + 1;
        const auto r = std::from_chars(begin, command.data() + command.size(), v);
        if (r.ec == std::errc() && r.ptr != begin)
            return v;
    }
    return std::nullopt;
}

std::string word(std::string_view command) {
    const std::size_t space = command.find(' ');
    return std::string(space == std::string_view::npos ? command : command.substr(0, space));
}

std::string temperatures_text(const Temperature& nozzle, const Temperature& bed) {
    char text[96];
    std::snprintf(text, sizeof text, "T:%.1f /%.1f B:%.1f /%.1f @:0 B@:0", nozzle.actual, nozzle.target, bed.actual,
                  bed.target);
    return text;
}

} // namespace

SimulatedPrinter::SimulatedPrinter(std::shared_ptr<ByteStream> stream, SimulatorOptions options)
    : stream_(std::move(stream)), options_(options) {
    thread_ = std::thread([this] { run(); });
}

SimulatedPrinter::~SimulatedPrinter() { stop(); }

void SimulatedPrinter::stop() {
    stop_ = true;
    if (thread_.joinable())
        thread_.join();
    stream_->close();
}

std::vector<std::string> SimulatedPrinter::commands() const {
    std::lock_guard lock(mutex_);
    return commands_;
}

std::size_t SimulatedPrinter::count(std::string_view prefix) const {
    std::lock_guard lock(mutex_);
    std::size_t n = 0;
    for (const std::string& c : commands_)
        n += std::string_view(c).substr(0, prefix.size()) == prefix;
    return n;
}

Temperature SimulatedPrinter::nozzle() const {
    std::lock_guard lock(mutex_);
    return nozzle_;
}

Temperature SimulatedPrinter::bed() const {
    std::lock_guard lock(mutex_);
    return bed_;
}

std::array<double, 4> SimulatedPrinter::position() const {
    std::lock_guard lock(mutex_);
    return position_;
}

void SimulatedPrinter::reply(const std::string& text) { stream_->write(text + "\n"); }

void SimulatedPrinter::tick() {
    std::lock_guard lock(mutex_);
    for (Temperature* t : {&nozzle_, &bed_}) {
        const double goal = t->target > 0.0 ? t->target : 25.0;
        t->actual += (goal - t->actual) * options_.heating;
    }
}

void SimulatedPrinter::run() {
    auto next_tick = std::chrono::steady_clock::now() + options_.tick;
    while (!stop_ && stream_->is_open()) {
        char buffer[512];
        const int n = stream_->read(buffer, sizeof buffer, options_.tick);
        if (n < 0)
            break;
        if (n > 0)
            pending_.append(buffer, std::size_t(n));
        for (std::size_t nl = pending_.find('\n'); nl != std::string::npos; nl = pending_.find('\n')) {
            std::string line = pending_.substr(0, nl);
            pending_.erase(0, nl + 1);
            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
                line.pop_back();
            if (!line.empty())
                handle(line);
        }
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_tick) {
            tick();
            next_tick = now + options_.tick;
        }
        if (report_every_ > 0 && now - last_report_ >= std::chrono::seconds(report_every_)) {
            last_report_ = now;
            std::lock_guard lock(mutex_);
            reply(temperatures_text(nozzle_, bed_));
        }
    }
}

void SimulatedPrinter::handle(const std::string& line) {
    std::string command = line;
    if (line[0] == 'N') {
        // A numbered line: the number must follow the last, the checksum match.
        const std::size_t star = line.rfind('*');
        std::uint32_t number = 0;
        const auto r = std::from_chars(line.data() + 1, line.data() + line.size(), number);
        const std::size_t space = line.find(' ');
        if (r.ec != std::errc() || star == std::string::npos || space == std::string::npos || star < space) {
            reply("Error:No Checksum with line number, Last Line: " + std::to_string(last_line_));
            reply("Resend: " + std::to_string(last_line_ + 1));
            reply("ok");
            return;
        }
        int checksum = -1;
        std::from_chars(line.data() + star + 1, line.data() + line.size(), checksum);
        const bool corrupt = options_.fail_line != 0 && number == options_.fail_line && !failed_once_;
        if (corrupt)
            failed_once_ = true;
        if (corrupt || checksum != int(gcode_checksum(std::string_view(line).substr(0, star)))) {
            reply("Error:checksum mismatch, Last Line: " + std::to_string(last_line_));
            reply("Resend: " + std::to_string(last_line_ + 1));
            reply("ok");
            return;
        }
        command = line.substr(space + 1, star - space - 1);
        if (word(command) != "M110" && number != last_line_ + 1) {
            reply("Error:Line Number is not Last Line Number+1, Last Line: " + std::to_string(last_line_));
            reply("Resend: " + std::to_string(last_line_ + 1));
            reply("ok");
            return;
        }
        last_line_ = number;
    }
    {
        std::lock_guard lock(mutex_);
        commands_.push_back(command);
    }
    const std::string code = word(command);
    if (code == "M110") {
        if (const std::optional<double> n = argument(command, 'N'))
            last_line_ = std::uint32_t(*n);
        reply("ok");
    } else if (code == "M115") {
        reply("FIRMWARE_NAME:TismaSim 1.0 SOURCE_CODE_URL:none PROTOCOL_VERSION:1.0 MACHINE_TYPE:Simulated");
        reply("ok");
    } else if (code == "M105") {
        std::lock_guard lock(mutex_);
        reply("ok " + temperatures_text(nozzle_, bed_));
    } else if (code == "M155") {
        if (!options_.auto_report) {
            reply("echo:Unknown command: \"" + command + "\"");
        } else {
            report_every_ = int(argument(command, 'S').value_or(0.0));
            last_report_ = std::chrono::steady_clock::now();
        }
        reply("ok");
    } else if (code == "M104" || code == "M140" || code == "M109" || code == "M190") {
        {
            std::lock_guard lock(mutex_);
            Temperature& t = code == "M104" || code == "M109" ? nozzle_ : bed_;
            t.target = argument(command, 'S').value_or(argument(command, 'R').value_or(0.0));
        }
        if (code == "M109" || code == "M190") {
            // Waits, saying so, until the temperature is close.
            for (int i = 0; i < 100 && !stop_; ++i) {
                {
                    std::lock_guard lock(mutex_);
                    const Temperature& t = code == "M109" ? nozzle_ : bed_;
                    const double goal = t.target > 0.0 ? t.target : 25.0;
                    if (std::abs(t.actual - goal) < 1.0)
                        break;
                    reply(temperatures_text(nozzle_, bed_));
                }
                reply("echo:busy: processing");
                std::this_thread::sleep_for(options_.tick);
                tick();
            }
        }
        reply("ok");
    } else if (code == "M114") {
        char text[128];
        std::lock_guard lock(mutex_);
        std::snprintf(text, sizeof text, "X:%.2f Y:%.2f Z:%.2f E:%.2f Count X:0 Y:0 Z:0", position_[0], position_[1],
                      position_[2], position_[3]);
        reply(text);
        reply("ok");
    } else if (code == "G28") {
        std::lock_guard lock(mutex_);
        position_ = {0.0, 0.0, 0.0, position_[3]};
        reply("ok");
    } else if (code == "G90" || code == "G91") {
        relative_ = code == "G91";
        relative_e_ = relative_;
        reply("ok");
    } else if (code == "M82" || code == "M83") {
        relative_e_ = code == "M83";
        reply("ok");
    } else if (code == "G92") {
        std::lock_guard lock(mutex_);
        const char axes[4] = {'X', 'Y', 'Z', 'E'};
        for (int i = 0; i < 4; ++i)
            if (const std::optional<double> v = argument(command, axes[i]))
                position_[std::size_t(i)] = *v;
        reply("ok");
    } else if (code == "G0" || code == "G1") {
        std::lock_guard lock(mutex_);
        const char axes[4] = {'X', 'Y', 'Z', 'E'};
        for (int i = 0; i < 4; ++i)
            if (const std::optional<double> v = argument(command, axes[i])) {
                const bool relative = i == 3 ? relative_e_ : relative_;
                position_[std::size_t(i)] = relative ? position_[std::size_t(i)] + *v : *v;
            }
        reply("ok");
    } else if (code == "M27") {
        reply("Not SD printing");
        reply("ok");
    } else if (code == "M106" || code == "M107" || code == "M84" || code == "M18" || code == "M117" ||
               code == "M204" || code == "M900" || code == "M141" || code == "M220" || code == "M221" ||
               code == "G4" || code == "M400") {
        reply("ok");
    } else {
        reply("echo:Unknown command: \"" + command + "\"");
        reply("ok");
    }
}

} // namespace tisma::device
