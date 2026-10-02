#include "testing.h"

using namespace rhi;
using namespace rhi::testing;

GPU_TEST_CASE("cmd-dynamic-blend-constants", D3D11 | D3D12 | Vulkan | Metal | WGPU)
{
    const char* source = R"(
[shader("vertex")]
float4 vertexMain(uint id : SV_VertexID) : SV_Position
{
    float2 p = id == 0 ? float2(-1,-1) : id == 1 ? float2(3,-1) : float2(-1,3);
    return float4(p,0,1);
}
[shader("fragment")]
float4 fragmentMain() : SV_Target { return float4(1,1,1,1); }
)";
    ComPtr<IShaderProgram> program;
    REQUIRE_CALL(loadRenderProgramFromSource(device, source, "vertexMain", "fragmentMain", program.writeRef()));
    ColorTargetDesc target = {};
    target.format = Format::RGBA32Float;
    target.enableBlend = true;
    target.color.srcFactor = target.alpha.srcFactor = BlendFactor::BlendColor;
    target.color.dstFactor = target.alpha.dstFactor = BlendFactor::InvBlendColor;
    RenderPipelineDesc pd = {};
    pd.program = program;
    pd.targets = &target;
    pd.targetCount = 1;
    pd.depthStencil.depthTestEnable = false;
    pd.depthStencil.depthWriteEnable = false;
    pd.rasterizer.cullMode = CullMode::None;
    pd.rasterizer.scissorEnable = true;
    auto pipeline = device->createRenderPipeline(pd);
    REQUIRE(pipeline);
    TextureDesc td = {};
    td.size = {8, 8, 1};
    td.format = target.format;
    td.usage = TextureUsage::RenderTarget | TextureUsage::CopySource;
    td.defaultState = ResourceState::RenderTarget;
    auto texture = device->createTexture(td);
    REQUIRE(texture);
    auto view = texture->getDefaultView();
    auto queue = device->getQueue(QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();
    RenderPassColorAttachment attachment = {};
    attachment.view = view;
    attachment.loadOp = LoadOp::Clear;
    RenderPassDesc pass = {};
    pass.colorAttachments = &attachment;
    pass.colorAttachmentCount = 1;
    auto render = encoder->beginRenderPass(pass);
    REQUIRE(render);
    render->bindPipeline(pipeline);
    RenderState state = {};
    state.viewports[0] = Viewport::fromSize(8, 8);
    state.viewportCount = state.scissorRectCount = 1;
    float colors[2][4] = {{.25f, .5f, .75f, .125f}, {.75f, .25f, .125f, .5f}};
    DrawArguments args = {};
    args.vertexCount = 3;
    for (uint32_t i = 0; i < 2; ++i)
    {
        std::memcpy(state.blendColor, colors[i], sizeof(state.blendColor));
        state.scissorRects[0] = i == 0 ? ScissorRect{0, 0, 4, 8} : ScissorRect{4, 0, 8, 8};
        render->setRenderState(state);
        render->draw(args);
    }
    render->end();
    // A new pass must not inherit the last constants. Redraw one corner
    // with the default-zero constants, still using the exact same pipeline.
    attachment.loadOp = LoadOp::Load;
    render = encoder->beginRenderPass(pass);
    REQUIRE(render);
    render->bindPipeline(pipeline);
    state = {};
    state.viewports[0] = Viewport::fromSize(8, 8);
    state.viewportCount = state.scissorRectCount = 1;
    state.scissorRects[0] = {0, 0, 2, 2};
    render->setRenderState(state);
    render->draw(args); // Zero source factor, one destination factor: preserves left color.
    render->end();
    auto commands = encoder->finish();
    REQUIRE(commands);
    REQUIRE_CALL(queue->submit(commands));
    REQUIRE_CALL(queue->waitOnHost());
    ComPtr<ISlangBlob> pixels;
    SubresourceLayout layout;
    REQUIRE_CALL(device->readTexture(texture, 0, 0, pixels.writeRef(), &layout));
    for (uint32_t y = 0; y < 8; ++y)
        for (uint32_t x = 0; x < 8; ++x)
        {
            const auto* pixel = reinterpret_cast<const float*>(
                static_cast<const uint8_t*>(pixels->getBufferPointer()) + y * layout.rowPitch + x * layout.colPitch
            );
            INFO("x=" << x << " y=" << y);
            for (uint32_t channel = 0; channel < 4; ++channel)
                CHECK(pixel[channel] == doctest::Approx(colors[x < 4 ? 0 : 1][channel]));
        }
}

GPU_TEST_CASE("cmd-mixed-constant-alpha-blend", D3D12 | Vulkan | Metal)
{
    if (!device->hasFeature(Feature::ConstantAlphaBlend))
        SKIP("Constant-alpha blending unavailable");
    const char* source = R"(
[shader("vertex")]
float4 vertexMain(uint id : SV_VertexID) : SV_Position
{
    float2 p = id == 0 ? float2(-1,-1) : id == 1 ? float2(3,-1) : float2(-1,3);
    return float4(p,0,1);
}
[shader("fragment")]
float4 fragmentMain() : SV_Target { return float4(.7,.3,.2,.6); }
)";
    ComPtr<IShaderProgram> program;
    REQUIRE_CALL(loadRenderProgramFromSource(device, source, "vertexMain", "fragmentMain", program.writeRef()));
    ColorTargetDesc target{};
    target.format = Format::RGBA32Float;
    target.enableBlend = true;
    target.alpha.srcFactor = BlendFactor::BlendAlpha;
    target.alpha.dstFactor = BlendFactor::InvBlendAlpha;
    RenderPipelineDesc pd{};
    pd.program = program;
    pd.targets = &target;
    pd.targetCount = 1;
    pd.depthStencil.depthTestEnable = pd.depthStencil.depthWriteEnable = false;
    pd.rasterizer.cullMode = CullMode::None;
    pd.rasterizer.scissorEnable = true;
    ComPtr<IRenderPipeline> pipelines[2];
    for (unsigned row = 0; row < 2; ++row)
    {
        target.color.srcFactor = row ? BlendFactor::BlendAlpha : BlendFactor::BlendColor;
        target.color.dstFactor = row ? BlendFactor::InvBlendColor : BlendFactor::InvBlendAlpha;
        pipelines[row] = device->createRenderPipeline(pd);
        REQUIRE(pipelines[row]);
    }
    const float destination[] = {.2f, .4f, .6f, .8f}, src[] = {.7f, .3f, .2f, .6f};
    const float constants[3][4] = {{.25f, .5f, .75f, .125f}, {.75f, .25f, .125f, .5f}, {0, 0, 0, 0}};
    TextureDesc td{};
    td.size = {6, 2, 1};
    td.format = target.format;
    td.usage = TextureUsage::RenderTarget | TextureUsage::CopySource;
    td.defaultState = ResourceState::RenderTarget;
    auto texture = device->createTexture(td);
    REQUIRE(texture);
    auto queue = device->getQueue(QueueType::Graphics);
    auto encoder = queue->createCommandEncoder();
    RenderPassColorAttachment attachment{};
    attachment.view = texture->getDefaultView();
    attachment.loadOp = LoadOp::Clear;
    std::memcpy(attachment.clearValue, destination, sizeof(destination));
    RenderPassDesc pass{};
    pass.colorAttachments = &attachment;
    pass.colorAttachmentCount = 1;
    auto render = encoder->beginRenderPass(pass);
    REQUIRE(render);
    for (unsigned row = 0; row < 2; ++row)
    {
        render->bindPipeline(pipelines[row]);
        for (unsigned col = 0; col < 3; ++col)
        {
            RenderState state{};
            state.viewports[0] = Viewport::fromSize(6, 2);
            state.viewportCount = state.scissorRectCount = 1;
            state.scissorRects[0] = {col * 2, row, col * 2 + 2, row + 1};
            std::memcpy(state.blendColor, constants[col], sizeof(state.blendColor));
            render->setRenderState(state);
            DrawArguments args{};
            args.vertexCount = 3;
            render->draw(args);
        }
    }
    render->end();
    REQUIRE_CALL(queue->submit(encoder->finish()));
    REQUIRE_CALL(queue->waitOnHost());
    ComPtr<ISlangBlob> pixels;
    SubresourceLayout layout;
    REQUIRE_CALL(device->readTexture(texture, 0, 0, pixels.writeRef(), &layout));
    for (unsigned row = 0; row < 2; ++row)
        for (unsigned x = 0; x < 6; ++x)
        {
            const float* c = constants[x / 2];
            const auto* pixel = reinterpret_cast<const float*>(
                static_cast<const uint8_t*>(pixels->getBufferPointer()) + row * layout.rowPitch + x * layout.colPitch
            );
            for (unsigned channel = 0; channel < 4; ++channel)
            {
                const float sourceFactor = channel == 3 || row == 1 ? c[3] : c[channel];
                const float destinationFactor = channel == 3 || row == 0 ? 1 - c[3] : 1 - c[channel];
                INFO("row=" << row << " x=" << x << " channel=" << channel);
                CHECK(
                    pixel[channel] ==
                    doctest::Approx(src[channel] * sourceFactor + destination[channel] * destinationFactor)
                        .epsilon(.001)
                );
            }
        }
}

GPU_TEST_CASE("cmd-constant-alpha-blend-unsupported", D3D11 | D3D12 | Vulkan | Metal | WGPU | DontCreateDevice)
{
    struct Callback : IDebugCallback
    {
        unsigned errors = 0;
        void SLANG_MCALL handleMessage(DebugMessageType type, DebugMessageSource, const char*) override
        {
            if (type == DebugMessageType::Error)
                ++errors;
        }
    } callback;
    for (bool validation : {false, true})
    {
        DeviceDesc dd{};
        dd.deviceType = ctx->deviceType;
        dd.debugCallback = &callback;
        dd.enableValidation = validation;
        REQUIRE_CALL(getRHI()->createDevice(dd, device.writeRef()));
        if (device->hasFeature(Feature::ConstantAlphaBlend))
            SKIP("Constant-alpha blending supported");
        const char* source = R"(
[shader("vertex")] float4 vertexMain(uint id:SV_VertexID):SV_Position { return float4(0,0,0,1); }
[shader("fragment")] float4 fragmentMain():SV_Target { return float4(1,1,1,1); }
)";
        ComPtr<IShaderProgram> program;
        REQUIRE_CALL(loadRenderProgramFromSource(device, source, "vertexMain", "fragmentMain", program.writeRef()));
        ColorTargetDesc target{};
        target.format = Format::RGBA8Unorm;
        target.enableBlend = true;
        RenderPipelineDesc pd{};
        pd.program = program;
        pd.targets = &target;
        pd.targetCount = 1;
        // Deferred programs must reject unsupported factors at creation too.
        pd.compilationPolicy = PipelineCompilationPolicy::Deferred;
        for (auto factor : {BlendFactor::BlendAlpha, BlendFactor::InvBlendAlpha})
        {
            target.color.srcFactor = factor;
            ComPtr<IRenderPipeline> pipeline;
            CHECK_EQ(device->createRenderPipeline(pd, pipeline.writeRef()), SLANG_E_NOT_AVAILABLE);
            CHECK_FALSE(pipeline);
        }
        CHECK_EQ(callback.errors, validation ? 2u : 0u);
    }
}
