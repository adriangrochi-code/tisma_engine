#include "device/serial_port.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <filesystem>
#ifdef __linux__
#include <asm/termbits.h> // termios2: any baud rate, 250000 included
#else
#include <termios.h>
#endif
#endif

namespace tisma::device {

#ifdef _WIN32

namespace {

std::wstring wide(const std::string& text) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring out(n > 0 ? std::size_t(n - 1) : 0, L'\0');
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), n);
    return out;
}

std::string last_error() {
    const DWORD code = GetLastError();
    char text[256];
    const DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code,
                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), text, sizeof text, nullptr);
    std::string s(text, n);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s.empty() ? "error " + std::to_string(code) : s;
}

} // namespace

SerialPort::~SerialPort() { close(); }

bool SerialPort::open(const std::string& path, int baud, std::string& error) {
    close();
    const std::wstring name = L"\\\\.\\" + wide(path);
    HANDLE h = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        error = "cannot open " + path + ": " + last_error();
        return false;
    }
    DCB dcb{};
    dcb.DCBlength = sizeof dcb;
    if (!GetCommState(h, &dcb)) {
        error = "not a serial port: " + path;
        CloseHandle(h);
        return false;
    }
    dcb.BaudRate = DWORD(baud);
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;
    if (!SetCommState(h, &dcb)) {
        error = "cannot set " + std::to_string(baud) + " baud on " + path + ": " + last_error();
        CloseHandle(h);
        return false;
    }
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    handle_ = h;
    return true;
}

int SerialPort::read(char* data, int size, std::chrono::milliseconds timeout) {
    if (!handle_ || size <= 0)
        return -1;
    // Back as soon as something arrived, or after the timeout with nothing.
    COMMTIMEOUTS t{};
    t.ReadIntervalTimeout = MAXDWORD;
    t.ReadTotalTimeoutMultiplier = MAXDWORD;
    t.ReadTotalTimeoutConstant = DWORD(std::max<long long>(timeout.count(), 1));
    t.WriteTotalTimeoutConstant = 5000;
    SetCommTimeouts(handle_, &t);
    DWORD got = 0;
    if (!ReadFile(handle_, data, DWORD(size), &got, nullptr))
        return -1;
    return int(got);
}

bool SerialPort::write(std::string_view data) {
    if (!handle_)
        return false;
    std::size_t done = 0;
    while (done < data.size()) {
        DWORD written = 0;
        if (!WriteFile(handle_, data.data() + done, DWORD(data.size() - done), &written, nullptr) || written == 0)
            return false;
        done += written;
    }
    return true;
}

void SerialPort::close() {
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
}

bool SerialPort::is_open() const { return handle_ != nullptr; }

std::vector<std::string> list_serial_ports() {
    std::vector<std::string> out;
    for (int i = 1; i <= 64; ++i) {
        const std::string name = "COM" + std::to_string(i);
        const std::wstring path = L"\\\\.\\" + wide(name);
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            out.push_back(name);
        } else if (GetLastError() == ERROR_ACCESS_DENIED) {
            out.push_back(name); // there, in use
        }
    }
    return out;
}

#else // POSIX

SerialPort::~SerialPort() { close(); }

bool SerialPort::open(const std::string& path, int baud, std::string& error) {
    close();
    const int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        error = "cannot open " + path + ": " + std::strerror(errno);
        return false;
    }
#ifdef __linux__
    termios2 tio{};
    if (ioctl(fd, TCGETS2, &tio) != 0) {
        error = "not a serial port: " + path;
        ::close(fd);
        return false;
    }
    tio.c_iflag &= ~tcflag_t(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tio.c_oflag &= ~tcflag_t(OPOST);
    tio.c_lflag &= ~tcflag_t(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cflag &= ~tcflag_t(CSIZE | PARENB | CSTOPB | CRTSCTS | CBAUD);
    tio.c_cflag |= tcflag_t(CS8 | CLOCAL | CREAD | BOTHER);
    tio.c_ispeed = tio.c_ospeed = speed_t(baud);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    if (ioctl(fd, TCSETS2, &tio) != 0) {
        error = "cannot set " + std::to_string(baud) + " baud on " + path + ": " + std::strerror(errno);
        ::close(fd);
        return false;
    }
    ioctl(fd, TCFLSH, TCIOFLUSH);
#else
    termios tio{};
    if (tcgetattr(fd, &tio) != 0) {
        error = "not a serial port: " + path;
        ::close(fd);
        return false;
    }
    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~tcflag_t(CRTSCTS);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    speed_t speed = B115200;
    switch (baud) {
    case 9600: speed = B9600; break;
    case 19200: speed = B19200; break;
    case 38400: speed = B38400; break;
    case 57600: speed = B57600; break;
    case 115200: speed = B115200; break;
    case 230400: speed = B230400; break;
    default:
        error = "unsupported baud rate on this system: " + std::to_string(baud);
        ::close(fd);
        return false;
    }
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        error = "cannot set " + std::to_string(baud) + " baud on " + path + ": " + std::strerror(errno);
        ::close(fd);
        return false;
    }
    tcflush(fd, TCIOFLUSH);
#endif
    fd_ = fd;
    return true;
}

int SerialPort::read(char* data, int size, std::chrono::milliseconds timeout) {
    if (fd_ < 0 || size <= 0)
        return -1;
    pollfd p{fd_, POLLIN, 0};
    const int ready = ::poll(&p, 1, int(std::min<long long>(timeout.count(), 1 << 30)));
    if (ready < 0)
        return errno == EINTR ? 0 : -1;
    if (ready == 0)
        return 0;
    if (p.revents & (POLLERR | POLLHUP | POLLNVAL))
        return -1;
    const ssize_t n = ::read(fd_, data, std::size_t(size));
    if (n < 0)
        return errno == EAGAIN || errno == EINTR ? 0 : -1;
    return n == 0 ? -1 : int(n); // nothing to read although readable: unplugged
}

bool SerialPort::write(std::string_view data) {
    if (fd_ < 0)
        return false;
    std::size_t done = 0;
    while (done < data.size()) {
        const ssize_t n = ::write(fd_, data.data() + done, data.size() - done);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                pollfd p{fd_, POLLOUT, 0};
                ::poll(&p, 1, 1000);
                continue;
            }
            return false;
        }
        done += std::size_t(n);
    }
    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialPort::is_open() const { return fd_ >= 0; }

std::vector<std::string> list_serial_ports() {
    std::vector<std::string> out;
    std::error_code ec;
    for (const std::filesystem::directory_entry& e : std::filesystem::directory_iterator("/dev", ec)) {
        const std::string name = e.path().filename().string();
        if (name.rfind("ttyUSB", 0) == 0 || name.rfind("ttyACM", 0) == 0 || name.rfind("cu.usb", 0) == 0)
            out.push_back(e.path().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

#endif

} // namespace tisma::device
