#include "testing.h"

#if SLANG_RHI_ENABLE_D3D12
#include "d3d12/d3d12-descriptor-heap.h"

using namespace rhi;
using namespace rhi::testing;

GPU_TEST_CASE("d3d12-descriptor-arena-exhaustion", D3D12)
{
    DeviceNativeHandles handles;
    REQUIRE_CALL(device->getNativeDeviceHandles(&handles));
    auto nativeDevice = reinterpret_cast<ID3D12Device*>(handles.handles[0].value);
    RefPtr<d3d12::GPUDescriptorHeap> heap;
    REQUIRE_CALL(d3d12::GPUDescriptorHeap::create(
        nativeDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 8, 8, heap.writeRef()
    ));
    d3d12::GPUDescriptorArena arena;
    REQUIRE_CALL(arena.init(heap, 4));
    REQUIRE(arena.allocate(3));
    CHECK_FALSE(arena.allocate(5));
    // Failure must preserve the unused descriptor in the current chunk.
    CHECK(arena.allocate(1));
    CHECK(arena.allocate(4));
    CHECK_FALSE(arena.allocate(1));
    // Failed chunks must never reach free(), and valid chunks must be reusable.
    arena.reset();
    arena.reset();
    CHECK(arena.allocate(8));
}
#endif
