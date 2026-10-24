#pragma once

#include "device/printer.hpp"
#include "device/stream.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// A printer that answers like a Marlin board over a byte stream, for the
// tests and for trying the interface without hardware (docs/DEVICE.md):
// numbered lines with checksums (a broken one is asked again), "ok" per
// line, M105 and M155 temperatures that warm up and cool down over time,
// M109 and M190 that wait with "echo:busy", M114, M115, M110, G28, G0 and
// G1 positions, M27 without a card. Own design; see PROVENANCE.md.
namespace tisma::device {

struct SimulatorOptions {
    std::uint32_t fail_line = 0;            // this numbered line is answered with a checksum error once (0: never)
    bool auto_report = true;                // whether M155 is known
    std::chrono::milliseconds tick{50};     // how often the temperatures move
    double heating = 0.3;                   // of the way to the target, per tick
};

class SimulatedPrinter {
public:
    explicit SimulatedPrinter(std::shared_ptr<ByteStream> stream, SimulatorOptions options = {});
    ~SimulatedPrinter();
    void stop();

    // Every command received, in order, without numbers and checksums (the
    // broken one not counted).
    std::vector<std::string> commands() const;
    std::size_t count(std::string_view prefix) const;
    Temperature nozzle() const;
    Temperature bed() const;
    std::array<double, 4> position() const;

private:
    void run();
    void handle(const std::string& line);
    void reply(const std::string& text);
    void tick();

    std::shared_ptr<ByteStream> stream_;
    SimulatorOptions options_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    mutable std::mutex mutex_;
    std::vector<std::string> commands_;
    Temperature nozzle_{25.0, 0.0}, bed_{25.0, 0.0};
    std::array<double, 4> position_{};
    bool relative_ = false, relative_e_ = false;
    std::uint32_t last_line_ = 0;
    bool failed_once_ = false;
    int report_every_ = 0; // seconds; 0: no auto report
    std::chrono::steady_clock::time_point last_report_{};
    std::string pending_;
};

} // namespace tisma::device
