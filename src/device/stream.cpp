#include "device/stream.hpp"

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>

namespace tisma::device {
namespace {

struct PipeState {
    std::mutex mutex;
    std::condition_variable changed;
    std::string buffer[2]; // what side 0 reads, what side 1 reads
    bool closed = false;
};

class PipeEnd : public ByteStream {
public:
    PipeEnd(std::shared_ptr<PipeState> state, int side) : state_(std::move(state)), side_(side) {}
    ~PipeEnd() override { close(); }

    int read(char* data, int size, std::chrono::milliseconds timeout) override {
        std::unique_lock lock(state_->mutex);
        std::string& in = state_->buffer[side_];
        state_->changed.wait_for(lock, timeout, [&] { return !in.empty() || state_->closed; });
        if (in.empty())
            return state_->closed ? -1 : 0;
        const int n = int(std::min<std::size_t>(in.size(), std::size_t(std::max(size, 0))));
        std::memcpy(data, in.data(), std::size_t(n));
        in.erase(0, std::size_t(n));
        return n;
    }
    bool write(std::string_view data) override {
        std::lock_guard lock(state_->mutex);
        if (state_->closed)
            return false;
        state_->buffer[1 - side_].append(data);
        state_->changed.notify_all();
        return true;
    }
    void close() override {
        std::lock_guard lock(state_->mutex);
        state_->closed = true;
        state_->changed.notify_all();
    }
    bool is_open() const override {
        std::lock_guard lock(state_->mutex);
        return !state_->closed;
    }

private:
    std::shared_ptr<PipeState> state_;
    int side_;
};

} // namespace

std::pair<std::shared_ptr<ByteStream>, std::shared_ptr<ByteStream>> make_pipe() {
    auto state = std::make_shared<PipeState>();
    return {std::make_shared<PipeEnd>(state, 0), std::make_shared<PipeEnd>(state, 1)};
}

} // namespace tisma::device
