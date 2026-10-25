#pragma once

#include "device/printer.hpp"
#include "device/stream.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// The G-code host: drives a printer that takes G-code over a byte stream
// (Marlin and its family, RepRapFirmware over USB), the way the public
// documentation of those firmwares describes the line protocol: numbered
// lines with a checksum, one "ok" per line, "Resend:" when one arrived
// broken, temperature reports, "echo:busy" while a command waits. Runs on
// its own thread; the interface reads the status and takes the events.
// Own design on that documentation; see PROVENANCE.md and docs/DEVICE.md.
namespace tisma::device {

// --- the protocol's texts ---------------------------------------------------

// The checksum of a line: every byte of "N<number> <command>" XOR-ed.
std::uint8_t gcode_checksum(std::string_view numbered_line);
// "N<number> <command>*<checksum>".
std::string numbered_line(std::uint32_t number, std::string_view command);
// A line of G-code without its comment and surrounding blanks; empty when
// there is nothing to send.
std::string strip_gcode_line(std::string_view line);

struct Temperatures {
    std::optional<Temperature> nozzle, bed;
};
// From "ok T:21.3 /0.0 B:22.0 /60.0 @:0 B@:0" or a "T:" report (also "T0:").
std::optional<Temperatures> parse_temperatures(std::string_view line);
// From "X:1.00 Y:2.00 Z:3.00 E:4.00 Count X:0 Y:0 Z:0".
std::optional<std::array<double, 4>> parse_position(std::string_view line);
// From "Resend: 12" or "rs 12": the line number asked for.
std::optional<std::uint32_t> parse_resend(std::string_view line);
// Whether the line is the firmware's "ok" (alone or with more after it).
bool is_ok(std::string_view line);

// --- the host ---------------------------------------------------------------

struct HostOptions {
    std::chrono::milliseconds connect_timeout{10000}; // for the first "ok"
    std::chrono::milliseconds ok_timeout{30000};      // for each line's "ok" (restarts on "busy")
    std::chrono::milliseconds temperature_period{2000}; // M105 when the firmware does not report by itself
    int max_resends = 5;                              // of one line, before giving up
};

class GcodeHost : public Printer {
public:
    explicit GcodeHost(std::shared_ptr<ByteStream> stream, HostOptions options = {});
    ~GcodeHost() override;

    bool connect() override;
    void disconnect() override;
    PrinterStatus status() const override;
    std::vector<PrinterEvent> take_events() override;
    bool start_print(std::string gcode, std::string name) override;
    void pause() override;
    void resume() override;
    void cancel() override;
    bool send(std::string command) override;

private:
    enum class Reply { Ok, Resend, Timeout, Gone };

    void run();
    bool handshake();
    Reply send_line(const std::string& command);
    std::optional<std::string> read_line(std::chrono::milliseconds timeout);
    void handle_report(std::string_view line);
    // A new state and its event, together: whoever sees the state finds the event.
    void change(PrinterState state, PrinterEvent::Kind kind, std::string text = {});
    void push_event(PrinterEvent::Kind kind, std::string text = {});
    void finish_job(PrinterEvent::Kind how);
    void drain_commands();

    std::shared_ptr<ByteStream> stream_;
    HostOptions options_;
    std::thread thread_;
    std::atomic<bool> stop_{false}, pause_{false}, cancel_{false};

    mutable std::mutex mutex_; // status_, events_, commands_, next_job_
    PrinterStatus status_;
    std::vector<PrinterEvent> events_;
    std::deque<std::string> commands_;
    struct Job {
        std::string name;
        std::vector<std::string> lines; // stripped, nothing empty
        std::size_t next = 0;
        std::chrono::steady_clock::time_point started;
    };
    std::optional<Job> next_job_, job_;

    std::string pending_;             // bytes read, not yet a whole line
    std::uint32_t line_number_ = 0;   // of the next numbered line
    bool auto_report_ = false;        // the firmware sends temperatures by itself (M155)
    bool unknown_command_ = false;    // the firmware did not know the last command
    std::chrono::steady_clock::time_point last_temperatures_{};
};

} // namespace tisma::device
