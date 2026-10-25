#pragma once

#include <chrono>
#include <memory>
#include <string_view>
#include <utility>

// The byte streams the device layer talks over (docs/DEVICE.md): a serial
// port, or the two ends of an in-memory pipe for the simulator and the
// tests. Own design; see PROVENANCE.md.
namespace tisma::device {

class ByteStream {
public:
    virtual ~ByteStream() = default;
    // Reads up to `size` bytes, waiting at most `timeout` for the first one:
    // how many came, 0 on timeout, -1 once the other side is gone.
    virtual int read(char* data, int size, std::chrono::milliseconds timeout) = 0;
    // Writes all of `data`; false once the stream is closed or broken.
    virtual bool write(std::string_view data) = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;
};

// Two streams joined end to end: what one writes, the other reads. Closing
// either end ends both.
std::pair<std::shared_ptr<ByteStream>, std::shared_ptr<ByteStream>> make_pipe();

} // namespace tisma::device
