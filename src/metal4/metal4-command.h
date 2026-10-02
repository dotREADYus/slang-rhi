#pragma once

#include "metal4-base.h"
#include "metal4-shader-object.h"

#include "core/ring-queue.h"

namespace rhi::metal4 {

/// Metal 4 commands use queue-stage barriers at every encoder transition and
/// explicit intra-encoder blit/compute barriers for dependent operations.
/// Submission events drive query readiness, deferred deletion and host waits.
/// The SDK bridge retains command-owned residency and immutable argument tables;
/// allocators are recycled only after completion, with at most 64 live recordings
/// and 16 retired allocator/command-buffer pairs cached on a queue.
class CommandQueueImpl : public CommandQueue
{
public:
    api::Ptr<api::CommandQueue> m_commandQueue;

    /// Compatibility token for shared encoding helpers. The Metal 4 bridge
    /// implements the transition as a queue-stage barrier, rather than a fence chain.
    NS::SharedPtr<MTL::Fence> m_queueFence;

    /// Tracking event for deferred deletion and CPU-side waiting.
    /// Signaled on the last CB of each submit() with m_lastSubmittedID.
    NS::SharedPtr<MTL::SharedEvent> m_trackingEvent;
    NS::SharedPtr<MTL::SharedEventListener> m_trackingEventListener;
    uint64_t m_lastSubmittedID;
    uint64_t m_lastFinishedID;
    std::list<InternalRefPtr<CommandBufferImpl>> m_commandBuffersInFlight;

    // Deferred delete queue for GPU resources.
    // Resources are held here until the GPU has finished using them.
    struct DeferredDelete
    {
        uint64_t submissionID;
        Resource* resource;
    };
    std::mutex m_deferredDeleteQueueMutex;
    RingQueue<DeferredDelete> m_deferredDeleteQueue;

    CommandQueueImpl(Device* device, QueueType type);
    ~CommandQueueImpl();

    void init(api::Ptr<api::CommandQueue> commandQueue);
    // Wait for GPU work and release command buffers before releasing device-owned heaps.
    void waitAndReleaseCommandBuffers();
    // Drain deferred deletes and destroy native queue services after command buffers and device-owned heaps are
    // released.
    void shutdown();

    void retireCommandBuffers();
    uint64_t updateLastFinishedID();

    /// Queue a resource for deferred deletion. The resource will be deleted
    /// once the GPU has finished all work submitted up to this point.
    void deferDelete(Resource* resource);

    /// Delete deferred resources that are no longer in use by the GPU.
    void executeDeferredDeletes();

    // ICommandQueue implementation
    virtual SLANG_NO_THROW Result SLANG_MCALL createCommandEncoder(
        const CommandEncoderDesc& desc,
        ICommandEncoder** outEncoder
    ) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL submit(const SubmitDesc& desc) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL waitOnHost() override;
    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(NativeHandle* outHandle) override;
};

class CommandEncoderImpl : public CommandEncoder
{
public:
    CommandQueueImpl* m_queue;
    RefPtr<CommandBufferImpl> m_commandBuffer;

    CommandEncoderImpl(Device* device, CommandQueueImpl* queue, const CommandEncoderDesc& desc);
    ~CommandEncoderImpl();

    Result init();

    virtual Result getBindingData(RootShaderObject* rootObject, BindingData*& outBindingData) override;

    // ICommandEncoder implementation
    virtual SLANG_NO_THROW Result SLANG_MCALL finish(
        const CommandBufferDesc& desc,
        ICommandBuffer** outCommandBuffer
    ) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(NativeHandle* outHandle) override;
};

class CommandBufferImpl : public CommandBuffer
{
public:
    CommandQueueImpl* m_queue;
    api::Ptr<api::CommandBuffer> m_commandBuffer;
    BindingCache m_bindingCache;
    uint64_t m_submissionID;

    CommandBufferImpl(Device* device, CommandQueueImpl* queue);
    ~CommandBufferImpl();

    Result init();
    virtual Result reset() override;

    // ICommandBuffer implementation
    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(NativeHandle* outHandle) override;
};

} // namespace rhi::metal4
