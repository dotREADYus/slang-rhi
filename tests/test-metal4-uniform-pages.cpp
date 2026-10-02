#include "testing.h"
using namespace rhi;
using namespace rhi::testing;

GPU_TEST_CASE("metal4-uniform-page-snapshots", ALL)
{
    if (device->getInfo().deviceType != DeviceType::Metal4)
        SKIP("Metal4 upload-page regression");
    ComPtr<IShaderProgram> program;
    REQUIRE_CALL(loadAndLinkProgram(device, "test-metal4-uniform-pages", "computeMain", program.writeRef()));
    ComputePipelineDesc desc = {};
    desc.program = program;
    auto pipeline = device->createComputePipeline(desc);
    REQUIRE(pipeline);
    BufferDesc bufferDesc = {};
    bufferDesc.size = 512 * sizeof(uint32_t);
    bufferDesc.elementSize = sizeof(uint32_t);
    bufferDesc.usage = BufferUsage::UnorderedAccess | BufferUsage::CopySource;
    bufferDesc.defaultState = ResourceState::UnorderedAccess;
    auto output = device->createBuffer(bufferDesc);
    REQUIRE(output);
    auto root = device->createRootShaderObject(pipeline);
    REQUIRE(root);
    ShaderCursor cursor(root);
    REQUIRE_CALL(cursor["output"].setBinding(output));
    auto queue = device->getQueue(QueueType::Graphics);
    for (uint32_t round = 0; round < 2; ++round)
    {
        auto encoder = queue->createCommandEncoder();
        auto pass = encoder->beginComputePass();
        pass->bindPipeline(pipeline, root);
        std::vector<uint32_t> expected;
        // Mutate one root across enough dispatches to cross several upload pages.
        // All recorded snapshots must survive until execution, including the tail
        // of each ordinary-data block and the next command buffer's retirement.
        for (uint32_t index = 0; index < 512; ++index)
        {
            const uint32_t value = index + round * 512;
            const float tail = float(index % 13);
            REQUIRE_CALL(cursor["parameters"]["index"].setData(index));
            REQUIRE_CALL(cursor["parameters"]["value"].setData(value));
            REQUIRE_CALL(cursor["parameters"]["padding"][253].setData(tail));
            pass->dispatchCompute(1, 1, 1);
            expected.push_back(value + uint32_t(tail));
        }
        pass->end();
        REQUIRE_CALL(queue->submit(encoder->finish()));
        REQUIRE_CALL(queue->waitOnHost());
        compareComputeResult(device, output, std::span<uint32_t>(expected));
    }
}

GPU_TEST_CASE("metal4-argument-table-reuse", Metal4)
{
    constexpr uint32_t count=128;
    ComPtr<ITexture> textures[2];
    ComPtr<ITextureView> views[2];
    ComPtr<ISampler> samplers[2];
    for(uint32_t i=0;i<2;++i) {
        const float pixels[8]={float(i*2+1),0,0,1,float(i*2+2),0,0,1};
        TextureDesc td{};
        td.size = {2, 1, 1};
        td.format = Format::RGBA32Float;
        td.usage = TextureUsage::ShaderResource;
        SubresourceData data{pixels, sizeof(pixels), sizeof(pixels)};
        textures[i] = device->createTexture(td, &data);
        REQUIRE(textures[i]);
        views[i] = device->createTextureView(textures[i], {});
        REQUIRE(views[i]);
        SamplerDesc sd{};
        sd.minFilter = sd.magFilter = sd.mipFilter = TextureFilteringMode::Point;
        sd.addressU = i ? TextureAddressingMode::ClampToEdge : TextureAddressingMode::Wrap;
        samplers[i] = device->createSampler(sd);
        REQUIRE(samplers[i]);
    }
    BufferDesc bd{};
    bd.size = count / 2 * 4 * sizeof(float);
    bd.elementSize = 4 * sizeof(float);
    bd.usage = BufferUsage::UnorderedAccess | BufferUsage::CopySource;
    ComPtr<IBuffer> outputs[2] = {device->createBuffer(bd), device->createBuffer(bd)};
    REQUIRE(outputs[0]);
    REQUIRE(outputs[1]);
    ComPtr<IShaderProgram> cp, rp;
    REQUIRE_CALL(loadAndLinkProgram(device, "test-metal4-table-reuse", "computeMain", cp.writeRef()));
    REQUIRE_CALL(loadProgram(device, "test-metal4-table-reuse", {"vertexMain", "fragmentMain"}, rp.writeRef()));
    ComputePipelineDesc cd{};
    cd.program = cp;
    auto compute = device->createComputePipeline(cd);
    REQUIRE(compute);
    ColorTargetDesc target{};
    target.format = Format::RGBA32Float;
    RenderPipelineDesc rd{};
    rd.program = rp;
    rd.targets = &target;
    rd.targetCount = 1;
    rd.depthStencil.depthTestEnable = false;
    rd.depthStencil.depthWriteEnable = false;
    auto render = device->createRenderPipeline(rd);
    REQUIRE(render);
    TextureDesc colorDesc{};
    colorDesc.size = {count, 1, 1};
    colorDesc.format = target.format;
    colorDesc.usage = TextureUsage::RenderTarget | TextureUsage::CopySource;
    auto color = device->createTexture(colorDesc);
    REQUIRE(color);
    auto colorView = device->createTextureView(color, {});
    REQUIRE(colorView);
    auto queue = device->getQueue(QueueType::Graphics);
    // Two command buffers also exercise retirement/recreation. Earlier commands
    // must preserve resources and sampler choices despite later root mutations.
    for (uint32_t round = 0; round < 2; ++round)
    {
        auto encoder = queue->createCommandEncoder();
        auto croot = device->createRootShaderObject(compute);
        auto rroot = device->createRootShaderObject(render);
        REQUIRE(croot);
        REQUIRE(rroot);
        auto update = [&](IShaderObject* root, uint32_t i)
        {
            ShaderCursor c(root);
            const uint32_t index = i / 2;
            const float bias = float(round * count + i);
            REQUIRE_CALL(c["inputTexture"].setBinding(views[(i / 2) % 2]));
            REQUIRE_CALL(c["inputSampler"].setBinding(samplers[i % 2]));
            REQUIRE_CALL(c["parameters"]["index"].setData(index));
            REQUIRE_CALL(c["parameters"]["bias"].setData(bias));
        };
        auto computePass = encoder->beginComputePass();
        computePass->bindPipeline(compute, croot);
        for (uint32_t i = 0; i < count; ++i)
        {
            update(croot, i);
            REQUIRE_CALL(ShaderCursor(croot)["output"].setBinding(outputs[i % 2]));
            computePass->dispatchCompute(1, 1, 1);
        }
        computePass->end();
        RenderPassColorAttachment attachment{};
        attachment.view = colorView;
        attachment.loadOp = LoadOp::Clear;
        attachment.storeOp = StoreOp::Store;
        RenderPassDesc passDesc{};
        passDesc.colorAttachments = &attachment;
        passDesc.colorAttachmentCount = 1;
        auto renderPass = encoder->beginRenderPass(passDesc);
        renderPass->bindPipeline(render, rroot);
        for (uint32_t i = 0; i < count; ++i)
        {
            update(rroot, i);
            RenderState state{};
            state.viewports[0] = Viewport::fromSize(count, 1);
            state.viewportCount = 1;
            state.scissorRects[0] = {i, 0, i + 1, 1};
            state.scissorRectCount = 1;
            renderPass->setRenderState(state);
            DrawArguments draw{};
            draw.vertexCount = 3;
            renderPass->draw(draw);
        }
        renderPass->end();
        REQUIRE_CALL(queue->submit(encoder->finish()));
        REQUIRE_CALL(queue->waitOnHost());
        float computeValues[2][count / 2 * 4]{};
        for (uint32_t i = 0; i < 2; ++i)
            REQUIRE_CALL(device->readBuffer(outputs[i], 0, bd.size, computeValues[i]));
        ComPtr<ISlangBlob> image;
        SubresourceLayout layout{};
        REQUIRE_CALL(device->readTexture(color, 0, 0, image.writeRef(), &layout));
        auto values = static_cast<const float*>(image->getBufferPointer());
        for (uint32_t i = 0; i < count; ++i)
        {
            const float expected = float(round * count + i + (i / 2) % 2 * 2 + 1 + i % 2);
            CHECK_EQ(computeValues[i % 2][i / 2 * 4], doctest::Approx(expected));
            CHECK_EQ(computeValues[i % 2][i / 2 * 4 + 3], 1.f);
            CHECK_EQ(values[i * 4], doctest::Approx(expected));
            CHECK_EQ(values[i * 4 + 3], 1.f);
        }
    }
}
