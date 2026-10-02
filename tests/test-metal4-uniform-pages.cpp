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
