#include "testing.h"

using namespace rhi;
using namespace rhi::testing;

GPU_TEST_CASE("fence-default-value", ALL & ~D3D11)
{
    FenceDesc fenceDesc = {};
    ComPtr<IFence> fence;
    REQUIRE_CALL(device->createFence(fenceDesc, fence.writeRef()));
    uint64_t value;
    REQUIRE_CALL(fence->getCurrentValue(&value));
    CHECK(value == 0);
}

GPU_TEST_CASE("fence-initial-value", ALL & ~D3D11)
{
    FenceDesc fenceDesc = {};
    fenceDesc.initialValue = 10;
    ComPtr<IFence> fence;
    REQUIRE_CALL(device->createFence(fenceDesc, fence.writeRef()));
    uint64_t value;
    REQUIRE_CALL(fence->getCurrentValue(&value));
    CHECK(value == 10);
}

GPU_TEST_CASE("fence-set-value", ALL & ~D3D11)
{
    FenceDesc fenceDesc = {};
    ComPtr<IFence> fence;
    REQUIRE_CALL(device->createFence(fenceDesc, fence.writeRef()));
    REQUIRE_CALL(fence->setCurrentValue(20));
    uint64_t value;
    REQUIRE_CALL(fence->getCurrentValue(&value));
    CHECK(value == 20);
}

GPU_TEST_CASE("fence-wait-without-timeout", ALL & ~D3D11)
{
    FenceDesc fenceDesc = {};
    ComPtr<IFence> fence1;
    ComPtr<IFence> fence2;
    REQUIRE_CALL(device->createFence(fenceDesc, fence1.writeRef()));
    REQUIRE_CALL(device->createFence(fenceDesc, fence2.writeRef()));

    // Wait for single signaled fence
    {
        IFence* fences[] = {fence1.get()};
        uint64_t values[] = {0};
        CHECK(device->waitForFences(1, fences, values, false, 0) == SLANG_OK);
        CHECK(device->waitForFences(1, fences, values, true, 0) == SLANG_OK);
    }

    // Wait for single unsignaled fence
    {
        IFence* fences[] = {fence1.get()};
        uint64_t values[] = {1};
        CHECK(device->waitForFences(1, fences, values, false, 0) == SLANG_E_TIME_OUT);
        CHECK(device->waitForFences(1, fences, values, true, 0) == SLANG_E_TIME_OUT);
    }

    // Wait for two signaled fences
    {
        IFence* fences[] = {fence1.get(), fence2.get()};
        uint64_t values[] = {0, 0};
        CHECK(device->waitForFences(2, fences, values, false, 0) == SLANG_OK);
        CHECK(device->waitForFences(2, fences, values, true, 0) == SLANG_OK);
    }

    // Wait for two unsignaled fences
    {
        IFence* fences[] = {fence1.get(), fence2.get()};
        uint64_t values[] = {1, 1};
        CHECK(device->waitForFences(2, fences, values, false, 0) == SLANG_E_TIME_OUT);
        CHECK(device->waitForFences(2, fences, values, true, 0) == SLANG_E_TIME_OUT);
    }

    // Wait for one signaled and one unsigned fences
    {
        IFence* fences[] = {fence1.get(), fence2.get()};
        uint64_t values[] = {0, 1};
        CHECK(device->waitForFences(2, fences, values, false, 0) == SLANG_OK);
        CHECK(device->waitForFences(2, fences, values, true, 0) == SLANG_E_TIME_OUT);
    }
}

GPU_TEST_CASE("fence-wait-with-timeout", ALL & ~D3D11)
{
    FenceDesc fenceDesc = {};
    ComPtr<IFence> fence1;
    ComPtr<IFence> fence2;
    REQUIRE_CALL(device->createFence(fenceDesc, fence1.writeRef()));
    REQUIRE_CALL(device->createFence(fenceDesc, fence2.writeRef()));

    // Wait for single signaled fence
    {
        IFence* fences[] = {fence1.get()};
        uint64_t values[] = {0};
        CHECK(device->waitForFences(1, fences, values, false, 1000) == SLANG_OK);
        CHECK(device->waitForFences(1, fences, values, true, 1000) == SLANG_OK);
    }

    // Wait for single unsignaled fence
    {
        IFence* fences[] = {fence1.get()};
        uint64_t values[] = {1};
        CHECK(device->waitForFences(1, fences, values, false, 1000) == SLANG_E_TIME_OUT);
        CHECK(device->waitForFences(1, fences, values, true, 1000) == SLANG_E_TIME_OUT);
    }

    // Wait for two signaled fences
    {
        IFence* fences[] = {fence1.get(), fence2.get()};
        uint64_t values[] = {0, 0};
        CHECK(device->waitForFences(2, fences, values, false, 1000) == SLANG_OK);
        CHECK(device->waitForFences(2, fences, values, true, 1000) == SLANG_OK);
    }

    // Wait for two unsignaled fences
    {
        IFence* fences[] = {fence1.get(), fence2.get()};
        uint64_t values[] = {1, 1};
        CHECK(device->waitForFences(2, fences, values, false, 1000) == SLANG_E_TIME_OUT);
        CHECK(device->waitForFences(2, fences, values, true, 1000) == SLANG_E_TIME_OUT);
    }

    // Wait for one signaled and one unsigned fences
    {
        IFence* fences[] = {fence1.get(), fence2.get()};
        uint64_t values[] = {0, 1};
        CHECK(device->waitForFences(2, fences, values, false, 1000) == SLANG_OK);
        CHECK(device->waitForFences(2, fences, values, true, 1000) == SLANG_E_TIME_OUT);
    }
}

GPU_TEST_CASE("fence-queue-signal", ALL & ~D3D11)
{
    FenceDesc fenceDesc = {};
    ComPtr<IFence> fence1;
    ComPtr<IFence> fence2;
    REQUIRE_CALL(device->createFence(fenceDesc, fence1.writeRef()));
    REQUIRE_CALL(device->createFence(fenceDesc, fence2.writeRef()));

    IFence* signalFences[] = {fence1, fence2};
    uint64_t signalFenceValues[] = {10, 20};

    SubmitDesc submitDesc = {};
    submitDesc.signalFenceCount = 2;
    submitDesc.signalFences = signalFences;
    submitDesc.signalFenceValues = signalFenceValues;
    REQUIRE_CALL(device->getQueue(QueueType::Graphics)->submit(submitDesc));

    REQUIRE_CALL(device->waitForFences(2, signalFences, signalFenceValues, true, kTimeoutInfinite));

    uint64_t fence1Value, fence2Value;
    REQUIRE_CALL(fence1->getCurrentValue(&fence1Value));
    REQUIRE_CALL(fence2->getCurrentValue(&fence2Value));
    CHECK(fence1Value == 10);
    CHECK(fence2Value == 20);
}

GPU_TEST_CASE("fence-queue-wait", ALL & ~D3D11)
{
    FenceDesc fenceDesc = {};
    ComPtr<IFence> fence1;
    ComPtr<IFence> fence2;
    REQUIRE_CALL(device->createFence(fenceDesc, fence1.writeRef()));
    REQUIRE_CALL(device->createFence(fenceDesc, fence2.writeRef()));

    fence1->setCurrentValue(10);
    fence2->setCurrentValue(20);

    IFence* waitFences[] = {fence1, fence2};
    uint64_t waitFenceValues[] = {10, 20};

    SubmitDesc submitDesc = {};
    submitDesc.waitFenceCount = 2;
    submitDesc.waitFences = waitFences;
    submitDesc.waitFenceValues = waitFenceValues;
    REQUIRE_CALL(device->getQueue(QueueType::Graphics)->submit(submitDesc));
    REQUIRE_CALL(device->getQueue(QueueType::Graphics)->waitOnHost());
}

GPU_TEST_CASE("metal4-submit-waits-before-work", ALL)
{
    if (device->getInfo().deviceType != DeviceType::Metal4)
        SKIP("Metal4 queue wait folding regression");
    auto queue = device->getQueue(QueueType::Graphics);
    for (bool empty : {false, true})
    {
        auto gate = device->createFence({});
        auto done = device->createFence({});
        REQUIRE(gate);
        REQUIRE(done);
        uint32_t initial[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        BufferDesc bd = {};
        bd.size = sizeof(initial);
        bd.usage = BufferUsage::CopyDestination | BufferUsage::CopySource;
        auto buffer = device->createBuffer(bd, initial);
        REQUIRE(buffer);
        auto first = queue->createCommandEncoder();
        auto last = queue->createCommandEncoder();
        first->clearBuffer(buffer, {0, sizeof(initial) / 2});
        last->clearBuffer(buffer, {sizeof(initial) / 2, sizeof(initial) / 2});
        auto a = first->finish();
        auto b = last->finish();
        ICommandBuffer* commands[] = {a, b};
        IFence* waits[] = {gate};
        IFence* signals[] = {done};
        const uint64_t value = 1;
        SubmitDesc submit = {};
        submit.commandBuffers = commands;
        submit.commandBufferCount = empty ? 0 : 2;
        submit.waitFences = waits;
        submit.waitFenceValues = &value;
        submit.waitFenceCount = 1;
        submit.signalFences = signals;
        submit.signalFenceValues = &value;
        submit.signalFenceCount = 1;
        REQUIRE_CALL(queue->submit(submit));
        // An unresolved queue wait must hold even an empty submission's signal.
        CHECK(device->waitForFences(1, signals, &value, true, 1000000) == SLANG_E_TIME_OUT);
        REQUIRE_CALL(gate->setCurrentValue(value));
        gate.setNull(); // Submission owns the native event until retirement.
        REQUIRE_CALL(queue->waitOnHost());
        uint64_t completed = 0;
        REQUIRE_CALL(done->getCurrentValue(&completed));
        CHECK(completed == value);
        if (!empty)
            compareComputeResult(device, buffer, makeArray<uint32_t>(0, 0, 0, 0, 0, 0, 0, 0));
    }
}
