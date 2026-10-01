#include "testing.h"

using namespace rhi;
using namespace rhi::testing;

static ComPtr<ITextureView> occlusionTarget(IDevice* device, Format format, TextureUsage usage)
{
    TextureDesc desc = {};
    desc.size = {16, 16, 1};
    desc.format = format;
    desc.usage = usage;
    auto texture = device->createTexture(desc);
    REQUIRE(texture);
    auto view = texture->createView({});
    REQUIRE(view);
    return view;
}

GPU_TEST_CASE("cmd-occlusion-visibility-and-reuse", ALL)
{
    if (!device->hasFeature(Feature::OcclusionQuery))
        SKIP("Occlusion queries not supported");

    auto queue = device->getQueue(QueueType::Graphics);
    auto color = occlusionTarget(device, Format::RGBA8Unorm, TextureUsage::RenderTarget);
    auto depth = occlusionTarget(device, Format::D32Float, TextureUsage::DepthStencil);
    ComPtr<IShaderProgram> program;
    REQUIRE_CALL(loadProgram(device, "test-cmd-occlusion", {"vertexMain", "fragmentMain"}, program.writeRef()));
    ColorTargetDesc target = {};
    target.format = Format::RGBA8Unorm;
    RenderPipelineDesc pipelineDesc = {};
    pipelineDesc.program = program;
    pipelineDesc.targets = &target;
    pipelineDesc.targetCount = 1;
    pipelineDesc.rasterizer.cullMode = CullMode::None;
    pipelineDesc.depthStencil.format = Format::D32Float;
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = true;
    pipelineDesc.depthStencil.depthFunc = ComparisonFunc::Less;
    auto pipeline = device->createRenderPipeline(pipelineDesc);
    REQUIRE(pipeline);

    RenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = color;
    colorAttachment.loadOp = LoadOp::Clear;
    RenderPassDepthStencilAttachment depthAttachment = {};
    depthAttachment.view = depth;
    depthAttachment.depthLoadOp = LoadOp::Clear;
    depthAttachment.depthClearValue = 1;
    RenderPassDesc passDesc = {};
    passDesc.colorAttachments = &colorAttachment;
    passDesc.colorAttachmentCount = 1;
    passDesc.depthStencilAttachment = &depthAttachment;

    for (auto type : {QueryType::Occlusion, QueryType::OcclusionPrecise})
    {
        if (type == QueryType::OcclusionPrecise && !device->hasFeature(Feature::PreciseOcclusionQuery))
            continue;
        QueryPoolDesc poolDesc = {};
        poolDesc.type = type;
        poolDesc.count = 3;
        ComPtr<IQueryPool> pool;
        REQUIRE_CALL(device->createQueryPool(poolDesc, pool.writeRef()));
        passDesc.occlusionQueryPool = pool;
        QueryResultState state = QueryResultState::Resolved;
        REQUIRE_CALL(pool->getResultState(0, 3, &state));
        CHECK(state == QueryResultState::Reset);
        uint64_t values[3] = {};
        CHECK(pool->getResult(0, 3, values) == SLANG_FAIL);

        auto encoder = queue->createCommandEncoder();
        auto pass = encoder->beginRenderPass(passDesc);
        REQUIRE(pass);
        auto root = pass->bindPipeline(pipeline);
        REQUIRE(root);
        RenderState render = {};
        render.viewportCount = render.scissorRectCount = 1;
        render.viewports[0] = {0, 0, 16, 16, 0, 1};
        render.scissorRects[0] = {0, 0, 16, 16};
        pass->setRenderState(render);
        float z = .25f;
        REQUIRE_CALL(ShaderCursor(root)["queryDepth"].setData(&z, sizeof(z)));
        REQUIRE_CALL(pass->beginOcclusionQuery(0));
        DrawArguments draw = {};
        draw.vertexCount = 3;
        pass->draw(draw);
        REQUIRE_CALL(pass->endOcclusionQuery());
        z = .75f;
        REQUIRE_CALL(ShaderCursor(root)["queryDepth"].setData(&z, sizeof(z)));
        REQUIRE_CALL(pass->beginOcclusionQuery(1));
        pass->draw(draw);
        REQUIRE_CALL(pass->endOcclusionQuery());
        REQUIRE_CALL(pass->beginOcclusionQuery(2));
        REQUIRE_CALL(pass->endOcclusionQuery()); // Empty query must resolve to zero.
        pass->end();
        auto commands = encoder->finish();
        REQUIRE(commands);
        REQUIRE_CALL(pool->getResultState(0, 3, &state));
        CHECK(state == QueryResultState::Reset); // Recording is not submission.
        REQUIRE_CALL(queue->submit(commands));
        REQUIRE_CALL(pool->getResultState(0, 3, &state));
        CHECK(state != QueryResultState::Reset);     // Pending or already resolved is valid.
        REQUIRE_CALL(pool->getResult(0, 3, values)); // Read without a prior explicit wait.
        CHECK(values[0] > 0);
        if (type == QueryType::OcclusionPrecise)
            CHECK(values[0] == 256);
        CHECK(values[1] == 0);
        CHECK(values[2] == 0);
        REQUIRE_CALL(pool->getResultState(0, 3, &state));
        CHECK(state == QueryResultState::Resolved);
        REQUIRE_CALL(pool->reset(0, 1));
        REQUIRE_CALL(pool->getResultState(0, 1, &state));
        CHECK(state == QueryResultState::Reset);
        CHECK(pool->getResult(0, 1, values) == SLANG_FAIL);
        REQUIRE_CALL(pool->getResultState(1, 2, &state));
        CHECK(state == QueryResultState::Resolved);

        // Slot reuse in another submitted command buffer, including reset/reuse.
        auto repeat = queue->createCommandEncoder();
        auto repeatPass = repeat->beginRenderPass(passDesc);
        REQUIRE(repeatPass);
        REQUIRE_CALL(repeatPass->beginOcclusionQuery(0));
        REQUIRE_CALL(repeatPass->endOcclusionQuery());
        repeatPass->end();
        // A different slot can be written in a second pass of the same command buffer.
        repeatPass = repeat->beginRenderPass(passDesc);
        REQUIRE(repeatPass);
        REQUIRE_CALL(repeatPass->beginOcclusionQuery(1));
        REQUIRE_CALL(repeatPass->endOcclusionQuery());
        repeatPass->end();
        ComPtr<IBuffer> resolved;
        if (device->getDeviceType() != DeviceType::D3D11)
        {
            BufferDesc bufferDesc = {};
            bufferDesc.size = 4 * sizeof(uint64_t);
            bufferDesc.usage = BufferUsage::CopyDestination | BufferUsage::CopySource;
            resolved = device->createBuffer(bufferDesc);
            REQUIRE(resolved);
            repeat->resolveQuery(pool, 0, 2, resolved, sizeof(uint64_t));
        }
        auto repeatCommands = repeat->finish();
        repeat.setNull(); // Command buffers must retain the declared pool and resolve destination.
        REQUIRE_CALL(queue->submit(repeatCommands));
        repeatCommands.setNull();

        REQUIRE_CALL(pool->getResult(0, 1, values));
        CHECK(values[0] == 0);
        if (resolved)
        {
            REQUIRE_CALL(queue->waitOnHost());
            uint64_t gpuValues[2] = {UINT64_MAX, UINT64_MAX};
            REQUIRE_CALL(device->readBuffer(resolved, sizeof(uint64_t), sizeof(gpuValues), gpuValues));
            CHECK(gpuValues[0] == 0);
            CHECK(gpuValues[1] == 0);
        }
        REQUIRE_CALL(pool->reset());
    }
}

GPU_TEST_CASE("cmd-occlusion-unavailable-capability", ALL)
{
    for (auto type : {QueryType::Occlusion, QueryType::OcclusionPrecise})
    {
        auto feature = type == QueryType::Occlusion ? Feature::OcclusionQuery : Feature::PreciseOcclusionQuery;
        if (device->hasFeature(feature))
            continue;
        QueryPoolDesc desc = {};
        desc.type = type;
        desc.count = 1;
        ComPtr<IQueryPool> pool;
        CHECK(device->createQueryPool(desc, pool.writeRef()) == SLANG_E_NOT_AVAILABLE);
        CHECK(!pool);
    }
}

class OcclusionValidationCallback : public IDebugCallback
{
public:
    uint32_t errors = 0;
    SLANG_NO_THROW void SLANG_MCALL handleMessage(
        DebugMessageType type,
        DebugMessageSource source,
        const char* message
    ) override
    {
        if (type == DebugMessageType::Error)
        {
            CHECK(source == DebugMessageSource::Layer);
            CHECK(message != nullptr);
            ++errors;
        }
    }
};

GPU_TEST_CASE("cmd-occlusion-invalid-scope-and-pool", ALL | DontCreateDevice)
{
    // Capture expected validation errors instead of the test harness's fail-on-error callback.
    OcclusionValidationCallback callback;
    DeviceDesc deviceDesc = {};
    deviceDesc.deviceType = ctx->deviceType;
    deviceDesc.debugCallback = &callback;
    deviceDesc.enableValidation = true;
    REQUIRE_CALL(getRHI()->createDevice(deviceDesc, device.writeRef()));
    if (!device->hasFeature(Feature::OcclusionQuery))
        SKIP("Occlusion queries not supported");
    auto queue = device->getQueue(QueueType::Graphics);
    auto color = occlusionTarget(device, Format::RGBA8Unorm, TextureUsage::RenderTarget);
    RenderPassColorAttachment attachment = {};
    attachment.view = color;
    attachment.loadOp = LoadOp::Clear;
    RenderPassDesc passDesc = {};
    passDesc.colorAttachments = &attachment;
    passDesc.colorAttachmentCount = 1;
    QueryPoolDesc poolDesc = {};
    poolDesc.type = QueryType::Occlusion;
    poolDesc.count = 2;
    ComPtr<IQueryPool> pool;
    REQUIRE_CALL(device->createQueryPool(poolDesc, pool.writeRef()));

    auto encoder = queue->createCommandEncoder();
    auto pass = encoder->beginRenderPass(passDesc);
    REQUIRE(pass);
    CHECK(pass->beginOcclusionQuery(0) == SLANG_E_INVALID_ARG); // No declared pool.
    CHECK(pass->endOcclusionQuery() == SLANG_FAIL);
    pass->end();

    passDesc.occlusionQueryPool = pool;
    pass = encoder->beginRenderPass(passDesc);
    REQUIRE(pass);
    CHECK(pass->beginOcclusionQuery(2) == SLANG_E_INVALID_ARG);
    REQUIRE_CALL(pass->beginOcclusionQuery(0));
    CHECK(pass->beginOcclusionQuery(1) == SLANG_FAIL); // Nested query.
    REQUIRE_CALL(pass->endOcclusionQuery());
    CHECK(pass->beginOcclusionQuery(0) == SLANG_FAIL); // Duplicate write.
    CHECK(pass->endOcclusionQuery() == SLANG_FAIL);
    REQUIRE_CALL(pass->beginOcclusionQuery(1));
    pass->end(); // Invalid unbalanced scope is diagnosed and recovered for safe recording.
    CHECK(pass->endOcclusionQuery() == SLANG_FAIL);
    pass = encoder->beginRenderPass(passDesc);
    REQUIRE(pass);
    CHECK(pass->beginOcclusionQuery(1) == SLANG_FAIL); // Cannot carry/reuse a query across passes.
    pass->end();
    REQUIRE(encoder->finish());
    CHECK(callback.errors >= 9);

    // Declaring a non-occlusion pool or a pool from another device must reject the pass.
    if (device->hasFeature(Feature::TimestampQuery))
    {
        poolDesc.type = QueryType::Timestamp;
        ComPtr<IQueryPool> timestampPool;
        REQUIRE_CALL(device->createQueryPool(poolDesc, timestampPool.writeRef()));
        auto invalid = queue->createCommandEncoder();
        passDesc.occlusionQueryPool = timestampPool;
        CHECK(invalid->beginRenderPass(passDesc) == nullptr);
        REQUIRE(invalid->finish());
    }
    ComPtr<IDevice> other;
    REQUIRE_CALL(getRHI()->createDevice(deviceDesc, other.writeRef()));
    poolDesc.type = QueryType::Occlusion;
    ComPtr<IQueryPool> foreignPool;
    REQUIRE_CALL(other->createQueryPool(poolDesc, foreignPool.writeRef()));
    auto invalid = queue->createCommandEncoder();
    passDesc.occlusionQueryPool = foreignPool;
    CHECK(invalid->beginRenderPass(passDesc) == nullptr);
    REQUIRE(invalid->finish());
}
