#include "testing.h"

#include "rhi-shared.h"

#include <cstring>
#include <vector>

using namespace rhi;
using namespace rhi::testing;

GPU_TEST_CASE("buffer-init-data-staging-lifetime", Vulkan)
{
    auto queue = device->getQueue(QueueType::Graphics);
    REQUIRE_CALL(queue->waitOnHost());

    StagingHeap& heap = getUnderlyingDevice(device)->m_uploadHeap;
    CHECK_EQ(heap.getUsed(), 0);

    std::vector<uint32_t> data(1024);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = uint32_t(i * 1664525u + 1013904223u);

    BufferDesc desc = {};
    desc.size = data.size() * sizeof(uint32_t);
    desc.memoryType = MemoryType::DeviceLocal;
    desc.usage = BufferUsage::CopySource;

    ComPtr<IBuffer> buffer;
    REQUIRE_CALL(device->createBuffer(desc, data.data(), buffer.writeRef()));

    // Waiting on the public queue also retires work submitted through Vulkan's
    // internal queue wrapper, which must release the shared staging allocation.
    REQUIRE_CALL(queue->waitOnHost());
    CHECK_EQ(heap.getUsed(), 0);

    ComPtr<ISlangBlob> blob;
    REQUIRE_CALL(device->readBuffer(buffer, 0, desc.size, blob.writeRef()));
    CHECK_EQ(memcmp(blob->getBufferPointer(), data.data(), desc.size), 0);

    // Releasing the destination immediately must not destroy its native buffer
    // while the internal initialization copy is still in flight.
    buffer.setNull();
    REQUIRE_CALL(device->createBuffer(desc, data.data(), buffer.writeRef()));
    buffer.setNull();
    REQUIRE_CALL(queue->waitOnHost());
    CHECK_EQ(heap.getUsed(), 0);
}

#if defined(__APPLE__)
#include <unistd.h>
#include <cstdlib>
GPU_TEST_CASE("metal-host-memory-buffer", Metal | Metal4)
{
    const size_t size = size_t(sysconf(_SC_PAGESIZE));
    void* allocation = nullptr;
    REQUIRE_EQ(posix_memalign(&allocation, size, size), 0);
    struct Owner
    {
        void* data;
        ~Owner() { std::free(data); }
    } owner{allocation};
    std::memset(allocation, 0x35, size);
    MetalBufferHostMemoryDesc host{};
    host.data = allocation;
    BufferDesc desc{};
    desc.next = &host;
    desc.size = size;
    desc.memoryType = MemoryType::Upload;
    desc.usage = BufferUsage::CopySource | BufferUsage::ShaderResource;
    auto buffer = device->createBuffer(desc);
    REQUIRE(buffer);
    void* mapped = nullptr;
    REQUIRE_CALL(device->mapBuffer(buffer, CpuAccessMode::Write, &mapped));
    CHECK_EQ(mapped, allocation);
    std::memset(mapped, 0xA7, size);
    REQUIRE_CALL(device->unmapBuffer(buffer));
    std::vector<uint8_t> result(size);
    REQUIRE_CALL(device->readBuffer(buffer, 0, size, result.data()));
    CHECK_EQ(std::memcmp(result.data(), allocation, size), 0);
    CHECK_EQ(buffer->getDesc().next, nullptr);
    REQUIRE_CALL(device->getQueue(QueueType::Graphics)->waitOnHost());
    buffer.setNull();
    std::memset(allocation, 0x12, size);
    ComPtr<IBuffer> invalid;
    CHECK_EQ(device->createBuffer(desc, allocation, invalid.writeRef()), SLANG_E_INVALID_ARG);
    host.data = static_cast<uint8_t*>(allocation) + 1;
    CHECK_EQ(device->createBuffer(desc, nullptr, invalid.writeRef()), SLANG_E_INVALID_ARG);
    host.data = allocation;
    desc.size = size - 1;
    CHECK_EQ(device->createBuffer(desc, nullptr, invalid.writeRef()), SLANG_E_INVALID_ARG);
    desc.size = size;
    desc.memoryType = MemoryType::DeviceLocal;
    CHECK_EQ(device->createBuffer(desc, nullptr, invalid.writeRef()), SLANG_E_INVALID_ARG);
}
#endif

GPU_TEST_CASE("metal-swizzle-ordered-upload", Metal | Metal4)
{
    ComPtr<IShaderProgram> program;
    REQUIRE_CALL(loadAndLinkProgram(device, "test-metal-resource-swizzle", "computeMain", program.writeRef()));
    ComputePipelineDesc pd{};
    pd.program = program;
    auto pipeline = device->createComputePipeline(pd);
    REQUIRE(pipeline);
    MetalTextureSwizzleDesc swizzle{};
    swizzle.components[0] = 4;
    swizzle.components[1] = 3;
    swizzle.components[2] = 2;
    swizzle.components[3] = 1;
    TextureDesc td{};
    td.size = {2, 2, 1};
    td.mipCount = 2;
    td.format = Format::RGBA8Unorm;
    td.usage = TextureUsage::ShaderResource | TextureUsage::CopyDestination | TextureUsage::CopySource;
    td.next = &swizzle;
    auto texture = device->createTexture(td);
    REQUIRE(texture);
    CHECK_EQ(texture->getDesc().next, nullptr);
    BufferDesc bd{};
    bd.size = 16;
    bd.elementSize = 16;
    bd.usage = BufferUsage::UnorderedAccess | BufferUsage::CopySource;
    auto output = device->createBuffer(bd);
    REQUIRE(output);
    auto view = device->createTextureView(texture, {});
    REQUIRE(view);
    auto root = device->createRootShaderObject(pipeline);
    REQUIRE(root);
    ShaderCursor cursor(root);
    REQUIRE_CALL(cursor["inputTexture"].setBinding(view));
    REQUIRE_CALL(cursor["output"].setBinding(output));
    auto queue = device->getQueue(QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();
    uint8_t source[4] = {32, 64, 128, 192};
    SubresourceData data{source, 4, 4};
    REQUIRE_CALL(encoder->uploadTextureData(texture, {0, 1, 1, 1}, {0, 0, 0}, {1, 1, 1}, &data, 1));
    // Upload must snapshot these bytes; later CPU mutation cannot change the GPU command.
    std::memset(source, 0, sizeof(source));
    auto pass = encoder->beginComputePass();
    pass->bindPipeline(pipeline, root);
    pass->dispatchCompute(1, 1, 1);
    pass->end();
    REQUIRE_CALL(queue->submit(encoder->finish()));
    float values[4]{};
    REQUIRE_CALL(device->readBuffer(output, 0, sizeof(values), values));
    CHECK_EQ(values[0], doctest::Approx(128.f / 255));
    CHECK_EQ(values[1], doctest::Approx(64.f / 255));
    CHECK_EQ(values[2], doctest::Approx(32.f / 255));
    CHECK_EQ(values[3], 1.f);
    ComPtr<ISlangBlob> bytes;
    SubresourceLayout layout{};
    REQUIRE_CALL(device->readTexture(texture, 0, 1, bytes.writeRef(), &layout));
    const uint8_t expected[] = {32, 64, 128, 192};
    CHECK_EQ(std::memcmp(bytes->getBufferPointer(), expected, 4), 0);
    swizzle.components[0] = 6;
    ComPtr<ITexture> invalid;
    CHECK_EQ(device->createTexture(td, nullptr, invalid.writeRef()), SLANG_E_INVALID_ARG);
}

GPU_TEST_CASE("metal-packed-bgra4-upload", Metal | Metal4)
{
    ComPtr<IShaderProgram> program;
    REQUIRE_CALL(loadAndLinkProgram(device, "test-metal-resource-swizzle", "computeMain", program.writeRef()));
    ComputePipelineDesc pd{};
    pd.program = program;
    auto pipeline = device->createComputePipeline(pd);
    REQUIRE(pipeline);
    TextureDesc td{};
    td.size = {2, 2, 1};
    td.mipCount = 2;
    td.format = Format::BGRA4Unorm;
    td.usage = TextureUsage::ShaderResource | TextureUsage::CopyDestination | TextureUsage::CopySource;
    auto texture = device->createTexture(td);
    REQUIRE(texture);
    BufferDesc bd{};
    bd.size = 16;
    bd.elementSize = 16;
    bd.usage = BufferUsage::UnorderedAccess | BufferUsage::CopySource;
    auto output = device->createBuffer(bd);
    REQUIRE(output);
    auto root = device->createRootShaderObject(pipeline);
    REQUIRE(root);
    ShaderCursor cursor(root);
    auto view = device->createTextureView(texture, {});
    REQUIRE(view);
    REQUIRE_CALL(cursor["inputTexture"].setBinding(view));
    REQUIRE_CALL(cursor["output"].setBinding(output));
    auto queue = device->getQueue(QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();
    uint16_t pixel = 0xABCD;
    SubresourceData data{&pixel, 2, 2};
    REQUIRE_CALL(encoder->uploadTextureData(texture, {0, 1, 1, 1}, {0, 0, 0}, {1, 1, 1}, &data, 1));
    auto pass = encoder->beginComputePass();
    pass->bindPipeline(pipeline, root);
    pass->dispatchCompute(1, 1, 1);
    pass->end();
    REQUIRE_CALL(queue->submit(encoder->finish()));
    float result[4]{};
    REQUIRE_CALL(device->readBuffer(output, 0, sizeof(result), result));
    CHECK_EQ(result[0], doctest::Approx(11.f / 15));
    CHECK_EQ(result[1], doctest::Approx(12.f / 15));
    CHECK_EQ(result[2], doctest::Approx(13.f / 15));
    CHECK_EQ(result[3], doctest::Approx(10.f / 15));
    ComPtr<ISlangBlob> bytes;
    SubresourceLayout layout{};
    REQUIRE_CALL(device->readTexture(texture, 0, 1, bytes.writeRef(), &layout));
    CHECK_EQ(std::memcmp(bytes->getBufferPointer(), &pixel, 2), 0);
    ComPtr<ITexture> unsupported;
    td.usage |= TextureUsage::RenderTarget;
    CHECK_EQ(device->createTexture(td, nullptr, unsupported.writeRef()), SLANG_E_NOT_AVAILABLE);
}
