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
    td.size = {8,8,1};
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
    state.viewports[0] = Viewport::fromSize(8,8);
    state.viewportCount = state.scissorRectCount = 1;
    float colors[2][4] = {{.25f,.5f,.75f,.125f},{.75f,.25f,.125f,.5f}};
    DrawArguments args = {};
    args.vertexCount = 3;
    for (uint32_t i = 0; i < 2; ++i)
    {
        std::memcpy(state.blendColor, colors[i], sizeof(state.blendColor));
        state.scissorRects[0] = i == 0 ? ScissorRect{0,0,4,8} : ScissorRect{4,0,8,8};
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
    state.viewports[0] = Viewport::fromSize(8,8);
    state.viewportCount = state.scissorRectCount = 1;
    state.scissorRects[0] = {0,0,2,2};
    render->setRenderState(state);
    render->draw(args); // Zero source factor, one destination factor: preserves left color.
    render->end();
    auto commands = encoder->finish();
    REQUIRE(commands);
    REQUIRE_CALL(queue->submit(commands));
    REQUIRE_CALL(queue->waitOnHost());
    ComPtr<ISlangBlob> pixels;
    SubresourceLayout layout;
    REQUIRE_CALL(device->readTexture(texture,0,0,pixels.writeRef(),&layout));
    for (uint32_t y = 0; y < 8; ++y)
        for (uint32_t x = 0; x < 8; ++x)
        {
            const auto* pixel = reinterpret_cast<const float*>(static_cast<const uint8_t*>(pixels->getBufferPointer()) + y*layout.rowPitch + x*layout.colPitch);
            INFO("x=" << x << " y=" << y);
            for (uint32_t channel = 0; channel < 4; ++channel)
                CHECK(pixel[channel] == doctest::Approx(colors[x < 4 ? 0 : 1][channel]));
        }
}
