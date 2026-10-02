#include <mach/mach_time.h>
#include "metal4-bridge.h"
#include "core/common.h"
// All Metal 4 objects are constructed only after isAvailable() verifies the runtime
// and GPU family. Keep deployment targets below 26 usable for the classic backend.
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <algorithm>
#include <cstdio>
#include <unordered_set>

namespace rhi::metal4::api {
#define OB(T,p) ((id<T>)(p))
static MTLSize sz(MTL::Size x) { return MTLSizeMake(x.width,x.height,x.depth); }
static MTLOrigin org(MTL::Origin x) { return MTLOriginMake(x.x,x.y,x.z); }
bool isAvailable(MTL::Device* device)
{
    if (@available(macOS 26.0, iOS 26.0, *))
        return device && [OB(MTLDevice,device) supportsFamily:MTLGPUFamilyMetal4];
    return false;
}
CounterHeap::CounterHeap(MTL::Device* device,uint32_t count) {
    auto desc=[[[MTL4CounterHeapDescriptor alloc] init] autorelease];
    desc.type=MTL4CounterHeapTypeTimestamp;desc.count=count;
    NSError* error=nil;
    native=[OB(MTLDevice,device) newCounterHeapWithDescriptor:desc error:&error];
}
uint64_t timestampFrequency() {mach_timebase_info_data_t info{};mach_timebase_info(&info);return uint64_t(1000000000)*info.denom/info.numer;}
CounterHeap::~CounterHeap() { [OB(NSObject,native) release]; }
bool CounterHeap::read(uint32_t index,uint32_t count,uint64_t* output) {
    auto data=[OB(MTL4CounterHeap,native) resolveCounterRange:NSMakeRange(index,count)];
    if(!data || data.length!=count*sizeof(MTL4TimestampHeapEntry))return false;
    const auto* entries=static_cast<const MTL4TimestampHeapEntry*>(data.bytes);
    for(uint32_t i=0;i<count;++i)output[i]=entries[i].timestamp;
    return true;
}
void CounterHeap::invalidate(uint32_t index,uint32_t count) { [OB(MTL4CounterHeap,native) invalidateCounterRange:NSMakeRange(index,count)]; }
void CommandBuffer::writeTimestamp(CounterHeap* heap,uint32_t index) {
    m_objects.push_back([OB(NSObject,heap->native) retain]);
    Encoder* encoder=m_encoders.empty() ? nullptr : m_encoders.back();
    if(encoder && !encoder->m_ended) {
        if(auto* render=dynamic_cast<RenderCommandEncoder*>(encoder))
            [OB(MTL4RenderCommandEncoder,render->m_native) writeTimestampWithGranularity:MTL4TimestampGranularityPrecise
                afterStage:MTLRenderStageVertex|MTLRenderStageFragment intoHeap:OB(MTL4CounterHeap,heap->native) atIndex:index];
        else
            [OB(MTL4ComputeCommandEncoder,encoder->m_native) writeTimestampWithGranularity:MTL4TimestampGranularityPrecise
                intoHeap:OB(MTL4CounterHeap,heap->native) atIndex:index];
        MTLStages stages=dynamic_cast<RenderCommandEncoder*>(encoder) ? MTLStageVertex|MTLStageFragment : MTLStageDispatch|MTLStageBlit;
        [OB(MTL4CommandEncoder,encoder->m_native) barrierAfterEncoderStages:stages beforeEncoderStages:stages visibilityOptions:MTL4VisibilityOptionDevice];
    } else [OB(MTL4CommandBuffer,m_native) writeTimestampIntoHeap:OB(MTL4CounterHeap,heap->native) atIndex:index];
}
void CommandBuffer::resolveTimestamps(CounterHeap* heap,uint32_t index,uint32_t count,MTL::Buffer* buffer,uint64_t offset) {
    m_objects.push_back([OB(NSObject,heap->native) retain]);retainResource(buffer);
    auto fence=[OB(MTLDevice,m_queue->m_device) newFence];m_objects.push_back(fence);
    auto before=[OB(MTL4CommandBuffer,m_native) computeCommandEncoder];
    [before barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];
    [before updateFence:fence afterEncoderStages:MTLStageBlit|MTLStageDispatch];[before endEncoding];
    [OB(MTL4CommandBuffer,m_native) resolveCounterHeap:OB(MTL4CounterHeap,heap->native) withRange:NSMakeRange(index,count)
        intoBuffer:MTL4BufferRange{buffer->gpuAddress()+offset,count*sizeof(MTL4TimestampHeapEntry)} waitFence:fence updateFence:fence];
    auto after=[OB(MTL4CommandBuffer,m_native) computeCommandEncoder];
    [after waitForFence:fence beforeEncoderStages:MTLStageBlit|MTLStageDispatch];[after endEncoding];
}
CommandQueue* newCommandQueue(MTL::Device* d)
{
    if (!isAvailable(d)) return nullptr;
    auto q = new CommandQueue(d);
    if (!q->m_native) { q->release(); return nullptr; }
    return q;
}
CommandQueue::CommandQueue(MTL::Device* d) : m_device(d)
{
    m_device->retain();
    m_native = [OB(MTLDevice,d) newMTL4CommandQueue];
}
CommandQueue::~CommandQueue()
{
    m_shuttingDown=true;
    for (auto b : m_pending) { if (b->m_committed) b->waitUntilCompleted(); b->release(); }
    for(auto [allocator,buffer]:m_freeAllocators) { [OB(NSObject,buffer) release]; [OB(NSObject,allocator) release]; }
    [OB(NSObject,m_native) release];
    m_device->release();
}
CommandBuffer* CommandQueue::commandBuffer()
{
    // Bound allocator ownership; submitted allocators survive until their completion event.
    // Abandoned recordings are reclaimed once only the queue owns them.
    for (auto i=m_pending.begin(); i!=m_pending.end();)
    {
        auto b=*i;
        if ((b->m_committed && b->status() >= MTL::CommandBufferStatusCompleted) ||
            (!b->m_committed && b->references()==1))
        { b->release(); i=m_pending.erase(i); }
        else ++i;
    }
    if (m_pending.size()>=64)
    {
        auto b=m_pending.front();
        if (!b->m_committed) return nullptr;
        b->waitUntilCompleted(); b->release(); m_pending.erase(m_pending.begin());
    }
    auto b=new CommandBuffer(this);
    if (!b->valid()) { b->release(); return nullptr; }
    m_pending.push_back(b);
    return b;
}
void CommandQueue::addResidencySet(MTL::ResidencySet* s) { [OB(MTL4CommandQueue,m_native) addResidencySet:OB(MTLResidencySet,s)]; }
void CommandQueue::removeResidencySet(MTL::ResidencySet* s) { [OB(MTL4CommandQueue,m_native) removeResidencySet:OB(MTLResidencySet,s)]; }
void CommandQueue::waitForDrawable(CA::MetalDrawable* d) { [OB(MTL4CommandQueue,m_native) waitForDrawable:OB(MTLDrawable,d)]; }
CommandBuffer::CommandBuffer(CommandQueue* q) : m_queue(q)
{
    auto device=OB(MTLDevice,q->m_device);
    if (!q->m_freeAllocators.empty()) {
        auto pair=q->m_freeAllocators.back(); q->m_freeAllocators.pop_back();
        m_allocator=pair.first; m_native=pair.second;
        [OB(MTL4CommandAllocator,m_allocator) reset];
    } else {
        m_allocator=[device newCommandAllocator];
        m_native=[device newCommandBuffer];
    }
    m_done=[device newSharedEvent];
    NSError* error=nil;
    MTLResidencySetDescriptor* rd=[[[MTLResidencySetDescriptor alloc] init] autorelease];
    m_residency=[device newResidencySetWithDescriptor:rd error:&error];
    if (!m_allocator || !m_native || !m_done || !m_residency)
    { [OB(NSObject,m_native) release]; m_native=nullptr; return; }
    [OB(MTL4CommandBuffer,m_native) beginCommandBufferWithAllocator:OB(MTL4CommandAllocator,m_allocator)];
}
CommandBuffer::~CommandBuffer()
{
    const bool recyclable = !m_committed || status()==MTL::CommandBufferStatusCompleted;
    if (!m_committed && m_native) {
        for(auto e:m_encoders) if(!e->m_ended) e->endEncoding();
        [OB(MTL4CommandBuffer,m_native) endCommandBuffer];
    }
    for (auto e:m_encoders) e->release();
    for (auto o:m_objects) [OB(NSObject,o) release];
    [OB(NSObject,m_drawable) release];
    [OB(NSObject,m_residency) release];
    [OB(NSObject,m_done) release];
    if (m_native && m_allocator && !m_queue->m_shuttingDown && m_queue->m_freeAllocators.size()<16 &&
        recyclable) {
        m_queue->m_freeAllocators.emplace_back(m_allocator,m_native);
    } else {
        [OB(NSObject,m_native) release];
        [OB(NSObject,m_allocator) release];
    }
}
void CommandBuffer::retainResource(MTL::Resource* r)
{
    if (!r || !m_resources.insert(r).second) return;
    // Residency sets deduplicate allocations; each encoding reference must also keep its native object alive.
    m_objects.push_back([OB(NSObject,r) retain]);
    [OB(MTLResidencySet,m_residency) addAllocation:OB(MTLAllocation,r)];
}
void* CommandBuffer::snapshot(Bindings& b)
{
    if (!b.dirty) return b.table;
    auto td=[[[MTL4ArgumentTableDescriptor alloc] init] autorelease];
    td.maxBufferBindCount=b.bufferCount; td.maxTextureBindCount=b.textureCount; td.maxSamplerStateBindCount=b.samplerCount;
    NSError* error=nil;
    auto t=[OB(MTLDevice,m_queue->m_device) newArgumentTableWithDescriptor:td error:&error];
    SLANG_RHI_ASSERT(t != nil);
    if (!t) return nullptr;
    for (NSUInteger i=0;i<b.bufferCount;++i) [t setAddress:b.buffers[i] atIndex:i];
    for (NSUInteger i=0;i<b.textureCount;++i) [t setTexture:MTLResourceID{b.textures[i]} atIndex:i];
    for (NSUInteger i=0;i<b.samplerCount;++i) [t setSamplerState:MTLResourceID{b.samplers[i]} atIndex:i];
    m_objects.push_back(t); b.table=t; b.dirty=false;
    return t;
}
RenderCommandEncoder* CommandBuffer::renderCommandEncoder(MTL::RenderPassDescriptor* input)
{
    auto src=(MTLRenderPassDescriptor*)input;
    auto d=[[[MTL4RenderPassDescriptor alloc] init] autorelease];
    for (NSUInteger i=0;i<8;++i) d.colorAttachments[i]=src.colorAttachments[i];
    d.depthAttachment=src.depthAttachment; d.stencilAttachment=src.stencilAttachment;
    d.renderTargetWidth=src.renderTargetWidth; d.renderTargetHeight=src.renderTargetHeight;
    d.renderTargetArrayLength=src.renderTargetArrayLength;
    d.visibilityResultBuffer=src.visibilityResultBuffer;
    if (d.visibilityResultBuffer) retainResource((MTL::Resource*)d.visibilityResultBuffer);
    for (NSUInteger i=0;i<8;++i) {
        retainResource((MTL::Resource*)d.colorAttachments[i].texture);
        retainResource((MTL::Resource*)d.colorAttachments[i].resolveTexture);
    }
    retainResource((MTL::Resource*)d.depthAttachment.texture); retainResource((MTL::Resource*)d.stencilAttachment.texture);
    auto native=[OB(MTL4CommandBuffer,m_native) renderCommandEncoderWithDescriptor:d];
    if (!native) return nullptr;
    auto e=new RenderCommandEncoder(this,native); m_encoders.push_back(e); return e;
}
ComputeCommandEncoder* CommandBuffer::computeCommandEncoder()
{
    auto native=[OB(MTL4CommandBuffer,m_native) computeCommandEncoder];
    if (!native) return nullptr;
    auto e=new ComputeCommandEncoder(this,native); m_encoders.push_back(e); return e;
}
void CommandBuffer::encodeWait(MTL::Event* e,uint64_t v) { m_waits.emplace_back(e,v); m_objects.push_back([OB(NSObject,e) retain]); }
void CommandBuffer::encodeSignalEvent(MTL::Event* e,uint64_t v) { m_signals.emplace_back(e,v); m_objects.push_back([OB(NSObject,e) retain]); }
void CommandBuffer::presentDrawable(CA::MetalDrawable* d) { m_drawable=(CA::MetalDrawable*)[OB(NSObject,d) retain]; }
void CommandBuffer::commit()
{
    SLANG_RHI_ASSERT(!m_committed);
    for (auto e:m_encoders) if (!e->m_ended) e->endEncoding();
    auto cb=OB(MTL4CommandBuffer,m_native); auto q=OB(MTL4CommandQueue,m_queue->m_native);
    [OB(MTLResidencySet,m_residency) commit];
    id<MTLResidencySet> rs=OB(MTLResidencySet,m_residency);
    [cb useResidencySets:&rs count:1];
    [cb endCommandBuffer];
    for (auto [e,v]:m_waits) [q waitForEvent:OB(MTLEvent,e) value:v];
    auto opts=[[[MTL4CommitOptions alloc] init] autorelease];
    auto state=m_feedback;
    [opts addFeedbackHandler:^(id<MTL4CommitFeedback> f) {
        if (f.error) { state->failed=true; std::fprintf(stderr,"Metal 4 command error: %s\n",f.error.localizedDescription.UTF8String); }
    }];
    [q commit:&cb count:1 options:opts];
    for (auto [e,v]:m_signals) [q signalEvent:OB(MTLEvent,e) value:v];
    if (m_drawable) { [q signalDrawable:OB(MTLDrawable,m_drawable)]; [OB(MTLDrawable,m_drawable) present]; }
    [q signalEvent:OB(MTLEvent,m_done) value:1];
    m_committed=true;
}
void CommandBuffer::waitUntilCompleted() {
    while(m_committed && !m_feedback->failed && ![OB(MTLSharedEvent,m_done) waitUntilSignaledValue:1 timeoutMS:250]) {}
}
MTL::CommandBufferStatus CommandBuffer::status() const {
    if (m_feedback->failed) return MTL::CommandBufferStatusError;
    if (!m_committed) return MTL::CommandBufferStatusNotEnqueued;
    return [OB(MTLSharedEvent,m_done) signaledValue]>=1 ? MTL::CommandBufferStatusCompleted : MTL::CommandBufferStatusCommitted;
}
void CommandBuffer::setLabel(NS::String* s) { [OB(MTL4CommandBuffer,m_native) setLabel:(NSString*)s]; }
void CommandBuffer::pushDebugGroup(NS::String* s) { [OB(MTL4CommandBuffer,m_native) pushDebugGroup:(NSString*)s]; }
void CommandBuffer::popDebugGroup() { [OB(MTL4CommandBuffer,m_native) popDebugGroup]; }
Encoder::Encoder(CommandBuffer* b,void* n) : m_native([OB(NSObject,n) retain]), m_buffer(b) {}
Encoder::~Encoder() { [OB(NSObject,m_native) release]; }
void Encoder::endEncoding() { if (!m_ended) { [OB(MTL4CommandEncoder,m_native) endEncoding]; m_ended=true; } }
void Encoder::waitForFence(MTL::Fence* f, MTL::RenderStages) {
    [OB(MTL4CommandEncoder,m_native) barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];
    SLANG_UNUSED(f);
}
void Encoder::updateFence(MTL::Fence* f, MTL::RenderStages) { SLANG_UNUSED(f); }
void Encoder::memoryBarrier(MTL::BarrierScope,MTL::RenderStages,MTL::RenderStages) {
    MTLStages stages=[OB(MTL4CommandEncoder,m_native) conformsToProtocol:@protocol(MTL4RenderCommandEncoder)] ? MTLStageVertex|MTLStageFragment : MTLStageDispatch|MTLStageBlit;
    [OB(MTL4CommandEncoder,m_native) barrierAfterEncoderStages:stages beforeEncoderStages:stages visibilityOptions:MTL4VisibilityOptionDevice];
}
void Encoder::useResources(const MTL::Resource* const* resources,NS::UInteger count,MTL::ResourceUsage) {
    for (NS::UInteger i=0;i<count;++i) m_buffer->retainResource(const_cast<MTL::Resource*>(resources[i]));
}
static void buffers(CommandBuffer* cb, Bindings& b, MTL::Buffer* const* r,const NS::UInteger* offsets,NS::Range range) {
    SLANG_RHI_ASSERT(range.location+range.length<=31);
    for (NS::UInteger i=0;i<range.length;++i) { b.buffers[range.location+i]=r[i]?r[i]->gpuAddress()+offsets[i]:0; cb->retainResource(r[i]); } b.bufferCount=std::max(b.bufferCount,unsigned(range.location+range.length)); b.dirty=true;
}
static void textures(CommandBuffer* cb, Bindings& b,MTL::Texture* const* r,NS::Range range) {
    SLANG_RHI_ASSERT(range.location+range.length<=128);
    for(NS::UInteger i=0;i<range.length;++i) { b.textures[range.location+i]=r[i]?r[i]->gpuResourceID()._impl:0; cb->retainResource(r[i]); } b.textureCount=std::max(b.textureCount,unsigned(range.location+range.length)); b.dirty=true;
}
static void samplers(CommandBuffer* cb, Bindings& b, MTL::SamplerState* const* r,NS::Range range) {
    SLANG_RHI_ASSERT(range.location+range.length<=16);
    for(NS::UInteger i=0;i<range.length;++i) { b.samplers[range.location+i]=r[i]?r[i]->gpuResourceID()._impl:0; if(r[i]) cb->m_objects.push_back([OB(NSObject,r[i]) retain]); } b.samplerCount=std::max(b.samplerCount,unsigned(range.location+range.length)); b.dirty=true;
}
void RenderCommandEncoder::setVertexBuffers(MTL::Buffer* const* r,const NS::UInteger* o,NS::Range n) { buffers(m_buffer,m_vertex,r,o,n); }
void RenderCommandEncoder::setFragmentBuffers(MTL::Buffer* const* r,const NS::UInteger* o,NS::Range n) { buffers(m_buffer,m_fragment,r,o,n); }
void RenderCommandEncoder::setVertexBuffer(MTL::Buffer* r,NS::UInteger o,NS::UInteger i) { buffers(m_buffer,m_vertex,&r,&o,NS::Range(i,1)); }
void RenderCommandEncoder::setVertexTextures(MTL::Texture* const* r,NS::Range n) { textures(m_buffer,m_vertex,r,n); }
void RenderCommandEncoder::setFragmentTextures(MTL::Texture* const* r,NS::Range n) { textures(m_buffer,m_fragment,r,n); }
void RenderCommandEncoder::setVertexSamplerStates(MTL::SamplerState* const* r,NS::Range n) { samplers(m_buffer,m_vertex,r,n); }
void RenderCommandEncoder::setFragmentSamplerStates(MTL::SamplerState* const* r,NS::Range n) { samplers(m_buffer,m_fragment,r,n); }
void RenderCommandEncoder::bind() {
    [OB(MTL4RenderCommandEncoder,m_native) setArgumentTable:OB(MTL4ArgumentTable,m_buffer->snapshot(m_vertex)) atStages:MTLRenderStageVertex];
    [OB(MTL4RenderCommandEncoder,m_native) setArgumentTable:OB(MTL4ArgumentTable,m_buffer->snapshot(m_fragment)) atStages:MTLRenderStageFragment];
}
void RenderCommandEncoder::setRenderPipelineState(MTL::RenderPipelineState* s) { [OB(MTL4RenderCommandEncoder,m_native) setRenderPipelineState:OB(MTLRenderPipelineState,s)]; }
void RenderCommandEncoder::setViewports(const MTL::Viewport* s,NS::UInteger n) { [OB(MTL4RenderCommandEncoder,m_native) setViewports:(const MTLViewport*)s count:n]; }
void RenderCommandEncoder::setScissorRects(const MTL::ScissorRect* s,NS::UInteger n) { [OB(MTL4RenderCommandEncoder,m_native) setScissorRects:(const MTLScissorRect*)s count:n]; }
#define RSET(name,type) void RenderCommandEncoder::name(MTL::type x) { [OB(MTL4RenderCommandEncoder,m_native) name:(MTL##type)x]; }
RSET(setFrontFacingWinding,Winding)
RSET(setCullMode,CullMode)
RSET(setDepthClipMode,DepthClipMode)
RSET(setTriangleFillMode,TriangleFillMode)
void RenderCommandEncoder::setDepthBias(float a,float b,float c) { [OB(MTL4RenderCommandEncoder,m_native) setDepthBias:a slopeScale:b clamp:c]; }
void RenderCommandEncoder::setBlendColor(float a,float b,float c,float d) { [OB(MTL4RenderCommandEncoder,m_native) setBlendColorRed:a green:b blue:c alpha:d]; }
void RenderCommandEncoder::setDepthStencilState(MTL::DepthStencilState* s) { [OB(MTL4RenderCommandEncoder,m_native) setDepthStencilState:OB(MTLDepthStencilState,s)]; }
void RenderCommandEncoder::setStencilReferenceValue(uint32_t v) { [OB(MTL4RenderCommandEncoder,m_native) setStencilReferenceValue:v]; }
void RenderCommandEncoder::setVisibilityResultMode(MTL::VisibilityResultMode m,NS::UInteger o) { [OB(MTL4RenderCommandEncoder,m_native) setVisibilityResultMode:(MTLVisibilityResultMode)m offset:o]; }
void RenderCommandEncoder::drawPrimitives(MTL::PrimitiveType p,NS::UInteger v,NS::UInteger n,NS::UInteger instances,NS::UInteger base) { bind(); [OB(MTL4RenderCommandEncoder,m_native) drawPrimitives:(MTLPrimitiveType)p vertexStart:v vertexCount:n instanceCount:instances baseInstance:base]; }
void RenderCommandEncoder::drawIndexedPrimitives(MTL::PrimitiveType p,NS::UInteger n,MTL::IndexType t,MTL::Buffer* b,NS::UInteger o,NS::UInteger instances,NS::Integer base,NS::UInteger first) { bind(); m_buffer->retainResource(b); [OB(MTL4RenderCommandEncoder,m_native) drawIndexedPrimitives:(MTLPrimitiveType)p indexCount:n indexType:(MTLIndexType)t indexBuffer:b->gpuAddress()+o indexBufferLength:b->length()-o instanceCount:instances baseVertex:base baseInstance:first]; }
void ComputeCommandEncoder::setComputePipelineState(MTL::ComputePipelineState* s) { [OB(MTL4ComputeCommandEncoder,m_native) setComputePipelineState:OB(MTLComputePipelineState,s)]; }
void ComputeCommandEncoder::setBuffers(MTL::Buffer* const* r,const NS::UInteger* o,NS::Range n) { buffers(m_buffer,m_bindings,r,o,n); }
void ComputeCommandEncoder::setTextures(MTL::Texture* const* r,NS::Range n) { textures(m_buffer,m_bindings,r,n); }
void ComputeCommandEncoder::setSamplerStates(MTL::SamplerState* const* r,NS::Range n) { samplers(m_buffer,m_bindings,r,n); }
void ComputeCommandEncoder::setTexture(MTL::Texture* r,NS::UInteger i) { textures(m_buffer,m_bindings,&r,NS::Range(i,1)); }
void ComputeCommandEncoder::setBytes(const void* data,NS::UInteger size,NS::UInteger i) { auto b=m_buffer->m_queue->m_device->newBuffer(data,size,MTL::ResourceStorageModeShared); NS::UInteger offset=0; buffers(m_buffer,m_bindings,&b,&offset,NS::Range(i,1)); b->release(); }
void ComputeCommandEncoder::setAccelerationStructure(MTL::AccelerationStructure* s,NS::UInteger i) { m_bindings.bufferCount=std::max(m_bindings.bufferCount,unsigned(i+1)); m_bindings.buffers[i]=s?s->gpuResourceID()._impl:0; m_bindings.dirty=true; m_buffer->retainResource(s); }
void ComputeCommandEncoder::bind() { [OB(MTL4ComputeCommandEncoder,m_native) setArgumentTable:OB(MTL4ArgumentTable,m_buffer->snapshot(m_bindings))]; }
void ComputeCommandEncoder::dispatchThreadgroups(MTL::Size a,MTL::Size b) { bind(); [OB(MTL4ComputeCommandEncoder,m_native) dispatchThreadgroups:sz(a) threadsPerThreadgroup:sz(b)]; }
void ComputeCommandEncoder::dispatchThreadgroups(MTL::Buffer* a,NS::UInteger o,MTL::Size b) { bind(); m_buffer->retainResource(a); [OB(MTL4ComputeCommandEncoder,m_native) dispatchThreadgroupsWithIndirectBuffer:a->gpuAddress()+o threadsPerThreadgroup:sz(b)]; }
void ComputeCommandEncoder::copyFromBuffer(MTL::Buffer* s,NS::UInteger so,MTL::Buffer* d,NS::UInteger off,NS::UInteger n) { [OB(MTL4ComputeCommandEncoder,m_native) barrierAfterEncoderStages:MTLStageBlit beforeEncoderStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice]; m_buffer->retainResource(s); m_buffer->retainResource(d); [OB(MTL4ComputeCommandEncoder,m_native) copyFromBuffer:OB(MTLBuffer,s) sourceOffset:so toBuffer:OB(MTLBuffer,d) destinationOffset:off size:n]; }
void ComputeCommandEncoder::copyFromBuffer(MTL::Buffer* s,NS::UInteger so,NS::UInteger row,NS::UInteger image,MTL::Size size,MTL::Texture* d,NS::UInteger slice,NS::UInteger level,MTL::Origin o) { [OB(MTL4ComputeCommandEncoder,m_native) barrierAfterEncoderStages:MTLStageBlit beforeEncoderStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice]; m_buffer->retainResource(s); m_buffer->retainResource(d); [OB(MTL4ComputeCommandEncoder,m_native) copyFromBuffer:OB(MTLBuffer,s) sourceOffset:so sourceBytesPerRow:row sourceBytesPerImage:image sourceSize:sz(size) toTexture:OB(MTLTexture,d) destinationSlice:slice destinationLevel:level destinationOrigin:org(o)]; }
void ComputeCommandEncoder::copyFromTexture(MTL::Texture* s,MTL::Texture* d) { [OB(MTL4ComputeCommandEncoder,m_native) barrierAfterEncoderStages:MTLStageBlit beforeEncoderStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice]; m_buffer->retainResource(s); m_buffer->retainResource(d); [OB(MTL4ComputeCommandEncoder,m_native) copyFromTexture:OB(MTLTexture,s) toTexture:OB(MTLTexture,d)]; }
void ComputeCommandEncoder::copyFromTexture(MTL::Texture* s,NS::UInteger slice,NS::UInteger level,MTL::Origin o,MTL::Size size,MTL::Texture* d,NS::UInteger ds,NS::UInteger dl,MTL::Origin dst) { [OB(MTL4ComputeCommandEncoder,m_native) barrierAfterEncoderStages:MTLStageBlit beforeEncoderStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice]; m_buffer->retainResource(s); m_buffer->retainResource(d); [OB(MTL4ComputeCommandEncoder,m_native) copyFromTexture:OB(MTLTexture,s) sourceSlice:slice sourceLevel:level sourceOrigin:org(o) sourceSize:sz(size) toTexture:OB(MTLTexture,d) destinationSlice:ds destinationLevel:dl destinationOrigin:org(dst)]; }
void ComputeCommandEncoder::copyFromTexture(MTL::Texture* s,NS::UInteger slice,NS::UInteger level,MTL::Origin o,MTL::Size size,MTL::Buffer* d,NS::UInteger off,NS::UInteger row,NS::UInteger image) { [OB(MTL4ComputeCommandEncoder,m_native) barrierAfterEncoderStages:MTLStageBlit beforeEncoderStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice]; m_buffer->retainResource(s); m_buffer->retainResource(d); [OB(MTL4ComputeCommandEncoder,m_native) copyFromTexture:OB(MTLTexture,s) sourceSlice:slice sourceLevel:level sourceOrigin:org(o) sourceSize:sz(size) toBuffer:OB(MTLBuffer,d) destinationOffset:off destinationBytesPerRow:row destinationBytesPerImage:image]; }
void ComputeCommandEncoder::fillBuffer(MTL::Buffer* b,NS::Range r,uint8_t v) { [OB(MTL4ComputeCommandEncoder,m_native) barrierAfterEncoderStages:MTLStageBlit beforeEncoderStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice]; m_buffer->retainResource(b); [OB(MTL4ComputeCommandEncoder,m_native) fillBuffer:OB(MTLBuffer,b) range:NSMakeRange(r.location,r.length) value:v]; }
void ComputeCommandEncoder::resolveCounters(MTL::CounterSampleBuffer*,NS::Range,MTL::Buffer*,NS::UInteger) { SLANG_RHI_ASSERT_FAILURE("Metal4 classic timestamp pools are not supported"); }

static id<MTL4Compiler> compiler(MTL::Device* d,NSError** error) {
    auto desc=[[[MTL4CompilerDescriptor alloc] init] autorelease];
    return [OB(MTLDevice,d) newCompilerWithDescriptor:desc error:error];
}
static MTL4LibraryFunctionDescriptor* function(MTL::Library* lib,NSString* name) {
    if (!lib || !name) return nil;
    auto d=[[[MTL4LibraryFunctionDescriptor alloc] init] autorelease]; d.library=OB(MTLLibrary,lib); d.name=name; return d;
}
MTL::RenderPipelineState* newRenderPipeline(MTL::Device* device,MTL::RenderPipelineDescriptor* input,MTL::Library* vertex,MTL::Library* fragment,NS::Error** error)
{
    auto s=(MTLRenderPipelineDescriptor*)input;
    auto d=[[[MTL4RenderPipelineDescriptor alloc] init] autorelease];
    d.label=s.label; d.vertexFunctionDescriptor=function(vertex,s.vertexFunction.name); d.fragmentFunctionDescriptor=function(fragment,s.fragmentFunction.name);
    d.vertexDescriptor=s.vertexDescriptor; d.rasterSampleCount=s.rasterSampleCount; d.inputPrimitiveTopology=s.inputPrimitiveTopology;
    d.alphaToCoverageState=s.alphaToCoverageEnabled?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
    for(NSUInteger i=0;i<8;++i) {
        auto a=s.colorAttachments[i]; auto b=d.colorAttachments[i];
        b.pixelFormat=a.pixelFormat; b.blendingState=a.blendingEnabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
        b.sourceRGBBlendFactor=a.sourceRGBBlendFactor; b.destinationRGBBlendFactor=a.destinationRGBBlendFactor; b.rgbBlendOperation=a.rgbBlendOperation;
        b.sourceAlphaBlendFactor=a.sourceAlphaBlendFactor; b.destinationAlphaBlendFactor=a.destinationAlphaBlendFactor; b.alphaBlendOperation=a.alphaBlendOperation; b.writeMask=a.writeMask;
    }
    auto c=compiler(device,(NSError**)error); if(!c) return nullptr;
    auto p=[c newRenderPipelineStateWithDescriptor:d compilerTaskOptions:nil error:(NSError**)error]; [c release]; return (MTL::RenderPipelineState*)p;
}
MTL::ComputePipelineState* newComputePipeline(MTL::Device* device,MTL::Library* lib,const char* entry,NS::Error** error)
{
    auto d=[[[MTL4ComputePipelineDescriptor alloc] init] autorelease];
    d.computeFunctionDescriptor=function(lib,[NSString stringWithUTF8String:entry]);
    auto c=compiler(device,(NSError**)error); if(!c) return nullptr;
    auto p=[c newComputePipelineStateWithDescriptor:d compilerTaskOptions:nil error:(NSError**)error]; [c release]; return (MTL::ComputePipelineState*)p;
}
} // namespace rhi::metal4::api
