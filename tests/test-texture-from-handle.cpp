#include "testing.h"
#if SLANG_RHI_ENABLE_METAL
#include <Metal/Metal.hpp>
#endif

using namespace rhi;
using namespace rhi::testing;

GPU_TEST_CASE("texture-from-native-handle", D3D12 | Vulkan | Metal)
{
    TextureDesc desc = {};
    desc.type = TextureType::Texture2D;
    desc.mipCount = 1;
    desc.size.width = 2;
    desc.size.height = 1;
    desc.size.depth = 1;
    desc.usage = TextureUsage::UnorderedAccess | TextureUsage::CopySource | TextureUsage::CopyDestination;
    desc.defaultState = ResourceState::UnorderedAccess;
    desc.format = Format::RGBA32Float;

    auto initialData = makeArray<float>(0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f);

    SubresourceData subresourceData = {};
    subresourceData.data = initialData.data();
    subresourceData.rowPitch = sizeof(float) * 4 * desc.size.width;
    subresourceData.slicePitch = subresourceData.rowPitch * desc.size.height;

    ComPtr<ITexture> originalTexture;
    REQUIRE_CALL(device->createTexture(desc, &subresourceData, originalTexture.writeRef()));

    NativeHandle handle = {};
    REQUIRE_CALL(originalTexture->getNativeHandle(&handle));

    // Invalid native handles should fail without producing a wrapper object.
    {
        NativeHandle wrongTypeHandle = handle;
        wrongTypeHandle.type = NativeHandleType::Undefined;

        ComPtr<ITexture> invalidTexture;
        CHECK_EQ(
            device->createTextureFromNativeHandle(wrongTypeHandle, desc, invalidTexture.writeRef()),
            SLANG_E_INVALID_HANDLE
        );
        CHECK_EQ(invalidTexture.get(), nullptr);

        NativeHandle zeroValueHandle = handle;
        zeroValueHandle.value = 0;

        CHECK_EQ(
            device->createTextureFromNativeHandle(zeroValueHandle, desc, invalidTexture.writeRef()),
            SLANG_E_INVALID_HANDLE
        );
        CHECK_EQ(invalidTexture.get(), nullptr);
    }

    // D3D12 and Metal can check the texture descriptor from the native handle.
    // Vulkan cannot, so we skip this check.
    if (device->getDeviceType() == DeviceType::D3D12 || (device->getDeviceType() == DeviceType::Metal || device->getDeviceType() == DeviceType::Metal4))
    {
        TextureDesc invalidDesc = desc;
        invalidDesc.size.width++;

        ComPtr<ITexture> invalidTexture;
        CHECK(
            device->createTextureFromNativeHandle(handle, invalidDesc, invalidTexture.writeRef()) == SLANG_E_INVALID_ARG
        );
    }

    ComPtr<ITexture> texture;
    REQUIRE_CALL(device->createTextureFromNativeHandle(handle, desc, texture.writeRef()));

    NativeHandle wrappedHandle = {};
    REQUIRE_CALL(texture->getNativeHandle(&wrappedHandle));
    CHECK_EQ(wrappedHandle.type, handle.type);
    CHECK_EQ(wrappedHandle.value, handle.value);

    // D3D12 and Metal have internal reference counting for resources create from native handles,
    // so we can release the original texture and still use the new one.
    // Vulkan does not have internal reference counting, so we need to keep the original texture alive.
    auto queue = device->getQueue(QueueType::Graphics);
    if (device->getDeviceType() == DeviceType::D3D12 || (device->getDeviceType() == DeviceType::Metal || device->getDeviceType() == DeviceType::Metal4))
    {
        originalTexture = nullptr;
        REQUIRE_CALL(queue->waitOnHost());
    }

    compareComputeResult(device, texture, 0, 0, initialData);

    auto commandEncoder = queue->createCommandEncoder();

    float clearValue[4] = {8.f, 9.f, 10.f, 11.f};
    commandEncoder->clearTextureFloat(texture, kEntireTexture, clearValue);

    queue->submit(commandEncoder->finish());
    queue->waitOnHost();

    compareComputeResult(device, texture, 0, 0, makeArray<float>(8.f, 9.f, 10.f, 11.f, 8.f, 9.f, 10.f, 11.f));
}

#if SLANG_RHI_ENABLE_METAL
GPU_TEST_CASE("texture-from-native-handle-metal-packed-bgra4", Metal)
{
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    DeviceNativeHandles handles{};
    REQUIRE_CALL(device->getNativeDeviceHandles(&handles));
    auto native = reinterpret_cast<MTL::Device*>(handles.handles[0].value);
    auto td = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    td->setTextureType(MTL::TextureType2D);
    td->setPixelFormat(MTL::PixelFormatABGR4Unorm);
    td->setWidth(2);
    td->setHeight(2);
    td->setMipmapLevelCount(2);
    td->setStorageMode(MTL::StorageModeShared);
    td->setUsage(MTL::TextureUsageShaderRead);
    td->setSwizzle(
        {MTL::TextureSwizzleGreen, MTL::TextureSwizzleBlue, MTL::TextureSwizzleAlpha, MTL::TextureSwizzleRed}
    );
    auto texture = NS::TransferPtr(native->newTexture(td.get()));
    REQUIRE(texture);
    const uint16_t mip0[] = {0xF4A7, 0xF4A7, 0xF4A7, 0xF4A7}, mip1 = 0xC963;
    texture->replaceRegion(MTL::Region(0, 0, 2, 2), 0, mip0, 4);
    texture->replaceRegion(MTL::Region(0, 0, 1, 1), 1, &mip1, 2);
    TextureDesc desc{};
    desc.size = {2, 2, 1};
    desc.mipCount = 2;
    desc.format = Format::BGRA4Unorm;
    desc.usage = TextureUsage::ShaderResource | TextureUsage::CopySource;
    desc.defaultState = ResourceState::ShaderResource;
    NativeHandle handle{NativeHandleType::MTLTexture, reinterpret_cast<uint64_t>(texture.get())};
    ComPtr<ITexture> imported;
    REQUIRE_CALL(device->createTextureFromNativeHandle(handle, desc, imported.writeRef()));
    auto invalid = desc;
    invalid.usage |= TextureUsage::RenderTarget;
    ComPtr<ITexture> rejected;
    CHECK_EQ(device->createTextureFromNativeHandle(handle, invalid, rejected.writeRef()), SLANG_E_INVALID_ARG);
    CHECK_FALSE(rejected);
    texture.reset(); // imported alias owns the native allocation
    const char* source = R"(
Texture2D<float4> tex;
RWStructuredBuffer<float4> result;
[shader("compute")][numthreads(1,1,1)]
void computeMain(uint3 id:SV_DispatchThreadID) { result[0]=tex.Load(int3(0,0,0)); }
)";
    ComPtr<IShaderProgram> program;
    REQUIRE_CALL(loadComputeProgramFromSource(device, source, program.writeRef()));
    ComputePipelineDesc pd{};
    pd.program = program;
    auto pipeline = device->createComputePipeline(pd);
    REQUIRE(pipeline);
    BufferDesc bd{};
    bd.size = 16;
    bd.elementSize = 16;
    bd.usage = BufferUsage::UnorderedAccess | BufferUsage::CopySource;
    bd.defaultState = ResourceState::UnorderedAccess;
    auto result = device->createBuffer(bd);
    REQUIRE(result);
    auto queue = device->getQueue(QueueType::Graphics);
    for (unsigned mip = 0; mip < 2; ++mip)
    {
        TextureViewDesc vd{};
        vd.format = desc.format;
        vd.subresourceRange = {0, 1, mip, 1};
        auto view = device->createTextureView(imported, vd);
        REQUIRE(view);
        auto encoder = queue->createCommandEncoder();
        auto pass = encoder->beginComputePass();
        auto root = pass->bindPipeline(pipeline);
        ShaderCursor cursor(root);
        REQUIRE_CALL(cursor["tex"].setBinding(view));
        REQUIRE_CALL(cursor["result"].setBinding(result));
        pass->dispatchCompute(1, 1, 1);
        pass->end();
        REQUIRE_CALL(queue->submit(encoder->finish()));
        REQUIRE_CALL(queue->waitOnHost());
        float values[4]{};
        REQUIRE_CALL(device->readBuffer(result, 0, 16, values));
        const float expected0[] = {4.f / 15, 10.f / 15, 7.f / 15, 1.f},
                    expected1[] = {9.f / 15, 6.f / 15, 3.f / 15, 12.f / 15};
        for (unsigned channel = 0; channel < 4; ++channel)
            CHECK(values[channel] == doctest::Approx((mip ? expected1 : expected0)[channel]).epsilon(.001));
    }
}
#endif
