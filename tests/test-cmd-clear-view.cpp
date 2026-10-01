#include "testing.h"

using namespace rhi;
using namespace rhi::testing;

static ComPtr<ITextureView> clearView(ITexture* texture, uint32_t mip = 0, uint32_t layer = 0)
{
    TextureViewDesc desc = {};
    desc.subresourceRange = {layer, 1, mip, 1};
    auto view = texture->createView(desc);
    REQUIRE(view);
    return view;
}

static void checkPixel(
    IDevice* device,
    ITexture* texture,
    uint32_t mip,
    uint32_t layer,
    uint32_t x,
    uint32_t y,
    const void* expected,
    size_t size
)
{
    ComPtr<ISlangBlob> data;
    SubresourceLayout layout;
    REQUIRE_CALL(device->readTexture(texture, layer, mip, data.writeRef(), &layout));
    auto pixel = static_cast<const uint8_t*>(data->getBufferPointer()) + y * layout.rowPitch + x * layout.colPitch;
    INFO("mip=" << mip << " layer=" << layer << " x=" << x << " y=" << y);
    CHECK(std::memcmp(pixel, expected, size) == 0);
}

GPU_TEST_CASE("cmd-clear-view-channels-rectangles-subresources", ALL)
{
    if (!device->hasFeature(Feature::TextureViewClear))
        SKIP("Texture view clears unavailable");
    TextureDesc td = {};
    td.type = TextureType::Texture2DArray;
    td.size = {8, 8, 1};
    td.arrayLength = 2;
    td.mipCount = 2;
    td.format = Format::RGBA32Float;
    td.usage = TextureUsage::RenderTarget | TextureUsage::CopySource;
    auto texture = device->createTexture(td);
    REQUIRE(texture);
    auto queue = device->getQueue(QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();
    float initial[] = {.125f, .25f, .5f, .75f};
    for (uint32_t layer = 0; layer < 2; ++layer)
        for (uint32_t mip = 0; mip < 2; ++mip)
        {
            auto view = clearView(texture, mip, layer);
            TextureViewClearDesc clear = {};
            clear.view = view;
            std::memcpy(&clear.color, initial, sizeof(initial));
            REQUIRE_CALL(encoder->clearTextureView(clear));
        }
    auto view = clearView(texture, 1, 1);
    TextureViewClearDesc clear = {};
    clear.view = view;
    clear.color.floatValues[3] = .875f;
    clear.colorWriteMask = RenderTargetWriteMask::Alpha;
    ScissorRect rects[] = {{1, 1, 3, 3}, {3, 3, 99, 99}, {2, 2, 2, 3}, {99, 99, 100, 100}};
    clear.rectangles = rects;
    clear.rectangleCount = 4;
    REQUIRE_CALL(encoder->clearTextureView(clear));
    // Each individual color channel, preserving the earlier alpha-only clear.
    for (auto mask : {RenderTargetWriteMask::Red, RenderTargetWriteMask::Green, RenderTargetWriteMask::Blue})
    {
        clear.colorWriteMask = mask;
        clear.color.floatValues[0] = clear.color.floatValues[1] = clear.color.floatValues[2] = 1.f;
        clear.rectangleCount = 1;
        REQUIRE_CALL(encoder->clearTextureView(clear));
    }
    auto commands = encoder->finish();
    REQUIRE(commands);
    encoder.setNull();
    view.setNull(); // Recorded view/pipeline/binding data must survive.
    REQUIRE_CALL(queue->submit(commands));
    REQUIRE_CALL(queue->waitOnHost());
    float inside[] = {1, 1, 1, .875f};
    float corner[] = {.125f, .25f, .5f, .875f};
    for (uint32_t layer = 0; layer < 2; ++layer)
        for (uint32_t mip = 0; mip < 2; ++mip)
        {
            auto size = 8u >> mip;
            for (uint32_t y = 0; y < size; ++y)
                for (uint32_t x = 0; x < size; ++x)
                {
                    const float* expected = initial;
                    if (mip == 1 && layer == 1)
                    {
                        if (x >= 1 && x < 3 && y >= 1 && y < 3)
                            expected = inside;
                        else if (x == 3 && y == 3)
                            expected = corner;
                    }
                    checkPixel(device, texture, mip, layer, x, y, expected, sizeof(initial));
                }
        }
}

GPU_TEST_CASE("cmd-clear-view-integer-typed-values", ALL)
{
    if (!device->hasFeature(Feature::TextureViewClear))
        SKIP("Texture view clears unavailable");
    for (auto format : {Format::RGBA32Uint, Format::RGBA32Sint})
    {
        TextureDesc td = {};
        td.size = {4, 4, 1};
        td.format = format;
        td.usage = TextureUsage::RenderTarget | TextureUsage::CopySource;
        auto texture = device->createTexture(td);
        REQUIRE(texture);
        auto view = clearView(texture);
        auto queue = device->getQueue(QueueType::Graphics);
        auto encoder = queue->createCommandEncoder();
        TextureViewClearDesc clear = {};
        clear.view = view;
        uint32_t expected[] = {17, 23, 0x80000000u, 0xffffffffu};
        std::memcpy(&clear.color, expected, sizeof(expected));
        REQUIRE_CALL(encoder->clearTextureView(clear));
        clear.color.uintValues[0] = 0x7fffffffu;
        clear.colorWriteMask = RenderTargetWriteMask::Red;
        REQUIRE_CALL(encoder->clearTextureView(clear));
        expected[0] = 0x7fffffffu;
        auto commands = encoder->finish();
        REQUIRE(commands);
        REQUIRE_CALL(queue->submit(commands));
        REQUIRE_CALL(queue->waitOnHost());
        checkPixel(device, texture, 0, 0, 1, 1, expected, sizeof(expected));
    }
}

GPU_TEST_CASE("cmd-clear-view-depth-stencil-and-following-draw", ALL)
{
    if (!device->hasFeature(Feature::TextureViewClear))
        SKIP("Texture view clears unavailable");
    TextureDesc td = {};
    td.size = {8, 8, 1};
    td.format = Format::RGBA32Float;
    td.usage = TextureUsage::RenderTarget | TextureUsage::CopySource;
    auto color = device->createTexture(td);
    REQUIRE(color);
    auto colorView = clearView(color);
    td.format = Format::D32FloatS8Uint;
    td.usage = TextureUsage::DepthStencil;
    auto depth = device->createTexture(td);
    REQUIRE(depth);
    auto depthView = clearView(depth);
    auto queue = device->getQueue(QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();
    TextureViewClearDesc clear = {};
    clear.view = colorView;
    REQUIRE_CALL(encoder->clearTextureView(clear));
    clear.view = depthView;
    clear.clearDepth = clear.clearStencil = true;
    clear.depthStencil = {.25f, 3};
    REQUIRE_CALL(encoder->clearTextureView(clear));
    // Native whole-view stencil clear must preserve depth despite the unused value.
    clear.clearDepth = false;
    clear.depthStencil = {.9f, 3};
    REQUIRE_CALL(encoder->clearTextureView(clear));
    clear.clearDepth = true;
    clear.depthStencil = {.25f, 3};

    ScissorRect rect = {1, 1, 5, 5};
    clear.rectangles = &rect;
    clear.rectangleCount = 1;
    clear.depthStencil = {.75f, 7};
    REQUIRE_CALL(encoder->clearTextureView(clear));
    // Change depth alone in a subset; it must preserve stencil7.
    ScissorRect subset = {2, 2, 4, 4};
    clear.rectangles = &subset;
    clear.clearStencil = false;
    clear.depthStencil.depth = .625f;
    REQUIRE_CALL(encoder->clearTextureView(clear));
    // Change stencil alone in a corner; it must preserve depth.25 there.
    ScissorRect corner = {0, 0, 1, 1};
    clear.rectangles = &corner;
    clear.clearDepth = false;
    clear.clearStencil = true;
    clear.depthStencil.stencil = 7;
    REQUIRE_CALL(encoder->clearTextureView(clear));
    ComPtr<IShaderProgram> program;
    REQUIRE_CALL(loadProgram(device, "test-cmd-occlusion", {"vertexMain", "fragmentMain"}, program.writeRef()));
    ColorTargetDesc target = {};
    target.format = Format::RGBA32Float;
    RenderPipelineDesc pd = {};
    pd.program = program;
    pd.targets = &target;
    pd.targetCount = 1;
    pd.depthStencil.format = Format::D32FloatS8Uint;
    pd.depthStencil.depthTestEnable = true;
    pd.depthStencil.depthFunc = ComparisonFunc::Less;
    pd.depthStencil.depthWriteEnable = false;
    pd.depthStencil.stencilEnable = true;
    pd.depthStencil.frontFace.stencilFunc = ComparisonFunc::Equal;
    pd.depthStencil.backFace.stencilFunc = ComparisonFunc::Equal;
    auto pipeline = device->createRenderPipeline(pd);
    REQUIRE(pipeline);
    RenderPassColorAttachment ca = {};
    ca.view = colorView;
    ca.loadOp = LoadOp::Load;
    RenderPassDepthStencilAttachment da = {};
    da.view = depthView;
    da.depthLoadOp = da.stencilLoadOp = LoadOp::Load;
    RenderPassDesc passDesc = {};
    passDesc.colorAttachments = &ca;
    passDesc.colorAttachmentCount = 1;
    passDesc.depthStencilAttachment = &da;
    auto pass = encoder->beginRenderPass(passDesc);
    REQUIRE(pass);
    auto root = pass->bindPipeline(pipeline);
    REQUIRE(root);
    float z = .5f;
    REQUIRE_CALL(ShaderCursor(root)["queryDepth"].setData(&z, sizeof(z)));
    RenderState state = {};
    state.viewportCount = state.scissorRectCount = 1;
    state.viewports[0] = Viewport::fromSize(8, 8);
    state.scissorRects[0] = {0, 0, 8, 8};
    state.stencilRef = 7;
    pass->setRenderState(state);
    DrawArguments draw = {};
    draw.vertexCount = 3;
    pass->draw(draw);
    pass->end();
    auto commands = encoder->finish();
    REQUIRE(commands);
    REQUIRE_CALL(queue->submit(commands));
    REQUIRE_CALL(queue->waitOnHost());
    float white[] = {1, 1, 1, 1};
    float black[] = {0, 0, 0, 0};
    for (uint32_t y = 0; y < 8; ++y)
        for (uint32_t x = 0; x < 8; ++x)
            checkPixel(device, color, 0, 0, x, y, x >= 1 && x < 5 && y >= 1 && y < 5 ? white : black, sizeof(white));
}

GPU_TEST_CASE("cmd-clear-view-unsupported", CPU | CUDA | WGPU)
{
    CHECK_FALSE(device->hasFeature(Feature::TextureViewClear));
    auto queue = device->getQueue(QueueType::Graphics);
    if (!queue)
        SKIP("No graphics queue");
    auto encoder = queue->createCommandEncoder();
    CHECK(encoder->clearTextureView({}) == SLANG_E_NOT_AVAILABLE);
}

struct ClearViewValidationCallback : IDebugCallback
{
    uint32_t errors = 0;
    void SLANG_MCALL handleMessage(DebugMessageType type, DebugMessageSource source, const char*) override
    {
        if (type == DebugMessageType::Error)
        {
            CHECK(source == DebugMessageSource::Layer);
            ++errors;
        }
    }
};

GPU_TEST_CASE("cmd-clear-view-validation", ALL | DontCreateDevice)
{
    ClearViewValidationCallback callback;
    DeviceDesc dd = {};
    dd.deviceType = ctx->deviceType;
    dd.debugCallback = &callback;
    dd.enableValidation = true;
    REQUIRE_CALL(getRHI()->createDevice(dd, device.writeRef()));
    if (!device->hasFeature(Feature::TextureViewClear))
        SKIP("Texture view clears unavailable");
    TextureDesc td = {};
    td.size = {8, 8, 1};
    td.mipCount = 2;
    td.format = Format::RGBA32Float;
    td.usage = TextureUsage::RenderTarget;
    auto texture = device->createTexture(td);
    REQUIRE(texture);
    auto view = clearView(texture);
    auto queue = device->getQueue(QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();
    CHECK(encoder->clearTextureView({}) == SLANG_E_INVALID_ARG);
    TextureViewClearDesc clear = {};
    clear.view = view;
    clear.rectangleCount = 1;
    CHECK(encoder->clearTextureView(clear) == SLANG_E_INVALID_ARG);
    ScissorRect inverted = {3, 0, 2, 1};
    clear.rectangles = &inverted;
    CHECK(encoder->clearTextureView(clear) == SLANG_E_INVALID_ARG);
    clear.rectangleCount = 0;
    clear.colorWriteMask = RenderTargetWriteMask(0x80);
    CHECK(encoder->clearTextureView(clear) == SLANG_E_INVALID_ARG);
    clear.colorWriteMask = RenderTargetWriteMask::All;
    clear.clearDepth = true;
    CHECK(encoder->clearTextureView(clear) == SLANG_E_INVALID_ARG);
    auto fullView = texture->getDefaultView();
    REQUIRE(fullView);
    clear.clearDepth = false;
    clear.view = fullView;
    CHECK(encoder->clearTextureView(clear) == SLANG_E_INVALID_ARG); // Multiple mips.
    clear.view = view;
    RenderPassColorAttachment ca = {};
    ca.view = view;
    RenderPassDesc pd = {};
    pd.colorAttachments = &ca;
    pd.colorAttachmentCount = 1;
    auto pass = encoder->beginRenderPass(pd);
    REQUIRE(pass);
    CHECK(encoder->clearTextureView(clear) == SLANG_FAIL);
    pass->end();
    auto compute = encoder->beginComputePass();
    REQUIRE(compute);
    CHECK(encoder->clearTextureView(clear) == SLANG_FAIL);
    compute->end();
    // Valid no-op must not open a pass, alter state, or prevent future operations.
    clear.colorWriteMask = RenderTargetWriteMask::None;
    REQUIRE_CALL(encoder->clearTextureView(clear));
    clear.colorWriteMask = RenderTargetWriteMask::All;
    ScissorRect empty = {1, 1, 1, 2};
    clear.rectangles = &empty;
    clear.rectangleCount = 1;
    REQUIRE_CALL(encoder->clearTextureView(clear));
    clear.rectangleCount = 0;
    REQUIRE_CALL(encoder->clearTextureView(clear));
    auto commands = encoder->finish();
    REQUIRE(commands);
    REQUIRE_CALL(queue->submit(commands));
    REQUIRE_CALL(queue->waitOnHost());
    CHECK(callback.errors >= 8);
}

GPU_TEST_CASE("cmd-clear-view-msaa-preserves-samples", ALL)
{
    if (!device->hasFeature(Feature::TextureViewClear))
        SKIP("Texture view clears unavailable");
    TextureDesc td = {};
    td.size = {8, 8, 1};
    td.format = Format::RGBA8Unorm;
    td.type = TextureType::Texture2DMS;
    td.sampleCount = 4;
    td.usage = TextureUsage::RenderTarget | TextureUsage::ResolveSource;
    auto texture = device->createTexture(td);
    REQUIRE(texture);
    auto view = clearView(texture);
    td.type = TextureType::Texture2D;
    td.sampleCount = 1;
    td.usage = TextureUsage::RenderTarget | TextureUsage::ResolveDestination | TextureUsage::CopySource;
    auto resolved = device->createTexture(td);
    REQUIRE(resolved);
    auto resolveView = clearView(resolved);
    auto queue = device->getQueue(QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();
    TextureViewClearDesc clear = {};
    clear.view = view;
    clear.color.floatValues[0] = clear.color.floatValues[1] = clear.color.floatValues[2] = 1.f;
    REQUIRE_CALL(encoder->clearTextureView(clear));
    clear.colorWriteMask = RenderTargetWriteMask::Alpha;
    clear.color.floatValues[3] = 1.f;
    ScissorRect rect = {2, 2, 6, 6};
    clear.rectangles = &rect;
    clear.rectangleCount = 1;
    REQUIRE_CALL(encoder->clearTextureView(clear));
    RenderPassColorAttachment ca = {};
    ca.view = view;
    ca.resolveTarget = resolveView;
    ca.loadOp = LoadOp::Load;
    RenderPassDesc pd = {};
    pd.colorAttachments = &ca;
    pd.colorAttachmentCount = 1;
    auto pass = encoder->beginRenderPass(pd);
    REQUIRE(pass);
    pass->end();
    auto commands = encoder->finish();
    REQUIRE(commands);
    REQUIRE_CALL(queue->submit(commands));
    REQUIRE_CALL(queue->waitOnHost());
    for (uint32_t y = 0; y < 8; ++y)
        for (uint32_t x = 0; x < 8; ++x)
        {
            uint8_t expected[] = {255, 255, 255, uint8_t(x >= 2 && x < 6 && y >= 2 && y < 6 ? 255 : 0)};
            checkPixel(device, resolved, 0, 0, x, y, expected, sizeof(expected));
        }
}
