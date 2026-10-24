#pragma once

#include "device/stream.hpp"

#include <string>
#include <vector>

// A serial port as a byte stream (docs/DEVICE.md): the USB connection of a
// printer, 8 data bits, no parity, one stop bit, no flow control. Own code
// on the operating systems' interfaces (termios on Linux, the COM port on
// Windows); see PROVENANCE.md.
namespace tisma::device {

class SerialPort : public ByteStream {
public:
    SerialPort() = default;
    ~SerialPort() override;
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // Opens `path` ("/dev/ttyUSB0", "COM3") at `baud` (115200 and 250000
    // are the printers' usual ones); false with the reason.
    bool open(const std::string& path, int baud, std::string& error);

    int read(char* data, int size, std::chrono::milliseconds timeout) override;
    bool write(std::string_view data) override;
    void close() override;
    bool is_open() const override;

private:
#ifdef _WIN32
    void* handle_ = nullptr; // a HANDLE; nullptr when closed
#else
    int fd_ = -1;
#endif
};

// The serial ports a printer may be on: /dev/ttyUSB* and /dev/ttyACM* on
// Linux, the COM ports that exist on Windows. Sorted.
std::vector<std::string> list_serial_ports();

} // namespace tisma::device
