#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

// A printer the interface and the mobile app follow and drive (docs/DEVICE.md):
// what every connection offers, whatever it talks (G-code over a serial port
// today; the network protocols of the Device spec later). Nothing blocks:
// the interface asks for the status and takes the events. Own design; see
// PROVENANCE.md.
namespace tisma::device {

enum class PrinterState { Disconnected, Connecting, Idle, Printing, Paused, Error };

struct Temperature {
    double actual = 0.0, target = 0.0; // C
};

struct PrinterStatus {
    PrinterState state = PrinterState::Disconnected;
    std::string firmware;                 // what the printer said it runs (M115)
    Temperature nozzle, bed;
    std::array<double, 4> position{};     // X, Y, Z, E as last reported (M114)
    std::string job;                      // the name of what is printing
    std::size_t lines_sent = 0, lines_total = 0; // of the job (comments and blank lines not counted)
    double progress = 0.0;                // 0 to 1
    double elapsed = 0.0;                 // seconds since the job started
    std::size_t resends = 0;              // lines the printer asked again
    std::string error;                    // the last error, if any
};

struct PrinterEvent {
    enum class Kind { Connected, Disconnected, Started, Paused, Resumed, Finished, Cancelled, Error, Message };
    Kind kind;
    std::string text;
};

class Printer {
public:
    virtual ~Printer() = default;

    // Starts connecting (the status says when it is done); false if it
    // cannot even start (no port). Disconnecting cancels any job.
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual PrinterStatus status() const = 0;
    // The events since the last call, oldest first.
    virtual std::vector<PrinterEvent> take_events() = 0;

    // A job: the G-code text, streamed line by line. False when not idle.
    virtual bool start_print(std::string gcode, std::string name) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void cancel() = 0;

    // One command line, sent at the next free moment (also while printing).
    virtual bool send(std::string command) = 0;

    // The usual controls, as commands.
    bool set_nozzle(double celsius);
    bool set_bed(double celsius);
    bool home();
    bool move(char axis, double mm, double feed_mm_s); // relative, one axis
    bool extrude(double mm, double feed_mm_s);         // relative E; negative retracts
    bool fan(double percent);
};

const char* printer_state_name(PrinterState state); // "disconnected", "connecting", ...

} // namespace tisma::device
