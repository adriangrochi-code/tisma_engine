// Built when the GPU backend is off (TISMA_GPU=OFF): there is never a device.

#include "gpu/compute.hpp"

namespace tisma::gpu {

struct Context::Impl {};

std::unique_ptr<Context> Context::create(const ContextOptions&) { return nullptr; }
Context::Context(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Context::~Context() = default;
std::string Context::device_name() const { return {}; }
void Context::triangle_level_ranges(const IndexedMesh&, std::span<const float>, std::span<const float>,
                                    std::vector<std::uint32_t>&, std::vector<std::uint32_t>&) {}

} // namespace tisma::gpu
