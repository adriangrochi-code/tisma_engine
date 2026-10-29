// Built when the GPU backend is off (TISMA_GPU=OFF): there is never a device.

#include "gpu/compute.hpp"

namespace tisma::gpu {

struct Context::Impl {};

bool load_vulkan() { return false; }

std::unique_ptr<Context> Context::create(const ContextOptions&) { return nullptr; }
Context::Context(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Context::~Context() = default;
std::string Context::device_name() const { return {}; }
void Context::triangle_level_ranges(const IndexedMesh&, std::span<const float>, std::span<const float>,
                                    std::vector<std::uint32_t>&, std::vector<std::uint32_t>&) {}
void Context::level_segments_by_triangle(const IndexedMesh&, std::span<const float>, std::span<const float>,
                                         std::span<const std::uint32_t>, std::span<const std::uint32_t>,
                                         std::vector<std::uint32_t>&, std::vector<LevelSegment>&) {}
void Context::support_column(BitGrid&, std::span<const BitGrid>, std::span<const BitGrid>, std::span<BitGrid>, int) {}

} // namespace tisma::gpu
