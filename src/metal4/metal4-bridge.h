#pragma once

// SDK-backed bridge for Metal 4 only. Classic Metal keeps its existing metal-cpp headers.
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <atomic>
#include <vector>
#include <memory>
#include <string>
#include <unordered_set>

namespace rhi::metal4::api {
class Object
{
public:
    Object* retain() { ++m_refs; return this; }
    void release() { if (--m_refs == 0) delete this; }
    unsigned references() const { return m_refs; }
    virtual ~Object() = default;
private:
    std::atomic<unsigned> m_refs{1};
};
template<typename T> class Ptr
{
public:
    Ptr() = default;
    explicit Ptr(T* p, bool addRef) : m_ptr(p) { if (m_ptr && addRef) m_ptr->retain(); }
    Ptr(const Ptr& p) : Ptr(p.m_ptr,true) {}
    Ptr(Ptr&& p) noexcept : m_ptr(p.m_ptr) { p.m_ptr=nullptr; }
    ~Ptr() { if(m_ptr) m_ptr->release(); }
    Ptr& operator=(Ptr p) { std::swap(m_ptr,p.m_ptr); return *this; }
    T* get() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    explicit operator bool() const { return m_ptr!=nullptr; }
    void reset() { Ptr().swap(*this); }
    void swap(Ptr& p) { std::swap(m_ptr,p.m_ptr); }
private: T* m_ptr=nullptr;
};
template<typename T> Ptr<T> adopt(T* p) { return Ptr<T>(p,false); }
template<typename T> Ptr<T> retain(T* p) { return Ptr<T>(p,true); }
uint64_t timestampFrequency();
class CounterHeap : public Object
{
public:
    CounterHeap(MTL::Device*,uint32_t);
    ~CounterHeap();
    void* native=nullptr;
    bool read(uint32_t,uint32_t,uint64_t*);
    void invalidate(uint32_t,uint32_t);
};
class CommandBuffer;
class CommandQueue;
class Encoder : public Object
{
public:
    Encoder* retain() { Object::retain(); return this; }
    Encoder(CommandBuffer* buffer, void* native);
    ~Encoder();
    void endEncoding();
    void waitForFence(MTL::Fence*, MTL::RenderStages = MTL::RenderStages(0));
    void updateFence(MTL::Fence*, MTL::RenderStages = MTL::RenderStages(0));
    void memoryBarrier(MTL::BarrierScope, MTL::RenderStages = MTL::RenderStages(0), MTL::RenderStages = MTL::RenderStages(0));
    void useResources(const MTL::Resource* const*, NS::UInteger, MTL::ResourceUsage);
    void* m_native;
    CommandBuffer* m_buffer;
    bool m_ended = false;
};
struct Bindings
{
    uint64_t buffers[31]{};
    uint64_t textures[128]{};
    uint64_t samplers[16]{};
    // Mirror entries last written into the current native argument table.
    uint64_t tableBuffers[31]{}, tableTextures[128]{}, tableSamplers[16]{};
    unsigned bufferCount=0, textureCount=0, samplerCount=0;
    unsigned tableBufferCount = 0, tableTextureCount = 0, tableSamplerCount = 0;
    bool dirty = true;
    void* table = nullptr;
};
class RenderCommandEncoder : public Encoder
{
public:
    RenderCommandEncoder* retain() { Object::retain(); return this; }
    using Encoder::Encoder;
    Bindings m_vertex, m_fragment;
    void setRenderPipelineState(MTL::RenderPipelineState*);
    void setVertexBuffers(MTL::Buffer* const*, const NS::UInteger*, NS::Range);
    void setFragmentBuffers(MTL::Buffer* const*, const NS::UInteger*, NS::Range);
    void setVertexBuffer(MTL::Buffer*, NS::UInteger, NS::UInteger);
    void setVertexTextures(MTL::Texture* const*, NS::Range);
    void setFragmentTextures(MTL::Texture* const*, NS::Range);
    void setVertexSamplerStates(MTL::SamplerState* const*, NS::Range);
    void setFragmentSamplerStates(MTL::SamplerState* const*, NS::Range);
    void setViewports(const MTL::Viewport*, NS::UInteger);
    void setScissorRects(const MTL::ScissorRect*, NS::UInteger);
    void setFrontFacingWinding(MTL::Winding);
    void setCullMode(MTL::CullMode);
    void setDepthClipMode(MTL::DepthClipMode);
    void setDepthBias(float,float,float);
    void setTriangleFillMode(MTL::TriangleFillMode);
    void setBlendColor(float,float,float,float);
    void setDepthStencilState(MTL::DepthStencilState*);
    void setStencilReferenceValue(uint32_t);
    void setVisibilityResultMode(MTL::VisibilityResultMode, NS::UInteger);
    void drawPrimitives(MTL::PrimitiveType, NS::UInteger, NS::UInteger, NS::UInteger, NS::UInteger);
    void drawIndexedPrimitives(MTL::PrimitiveType, NS::UInteger, MTL::IndexType, MTL::Buffer*, NS::UInteger, NS::UInteger, NS::Integer, NS::UInteger);
    void bind();
};
class ComputeCommandEncoder : public Encoder
{
public:
    ComputeCommandEncoder* retain() { Object::retain(); return this; }
    using Encoder::Encoder;
    Bindings m_bindings;
    void setComputePipelineState(MTL::ComputePipelineState*);
    void setBuffers(MTL::Buffer* const*, const NS::UInteger*, NS::Range);
    void setTextures(MTL::Texture* const*, NS::Range);
    void setSamplerStates(MTL::SamplerState* const*, NS::Range);
    void setTexture(MTL::Texture*, NS::UInteger);
    void setBytes(const void*, NS::UInteger, NS::UInteger);
    void setAccelerationStructure(MTL::AccelerationStructure*, NS::UInteger);
    void dispatchThreadgroups(MTL::Size,MTL::Size);
    void dispatchThreadgroups(MTL::Buffer*,NS::UInteger,MTL::Size);
    void copyFromBuffer(MTL::Buffer*,NS::UInteger,MTL::Buffer*,NS::UInteger,NS::UInteger);
    void copyFromBuffer(MTL::Buffer*,NS::UInteger,NS::UInteger,NS::UInteger,MTL::Size,MTL::Texture*,NS::UInteger,NS::UInteger,MTL::Origin);
    void copyFromTexture(MTL::Texture*,MTL::Texture*);
    void copyFromTexture(MTL::Texture*,NS::UInteger,NS::UInteger,MTL::Origin,MTL::Size,MTL::Texture*,NS::UInteger,NS::UInteger,MTL::Origin);
    void copyFromTexture(MTL::Texture*,NS::UInteger,NS::UInteger,MTL::Origin,MTL::Size,MTL::Buffer*,NS::UInteger,NS::UInteger,NS::UInteger);
    void fillBuffer(MTL::Buffer*,NS::Range,uint8_t);
    void resolveCounters(MTL::CounterSampleBuffer*,NS::Range,MTL::Buffer*,NS::UInteger);
    void bind();
};
using BlitCommandEncoder = ComputeCommandEncoder;
class CommandBuffer : public Object
{
public:
    CommandBuffer* retain() { Object::retain(); return this; }
    CommandBuffer(CommandQueue*);
    ~CommandBuffer();
    bool valid() const { return m_native != nullptr; }
    RenderCommandEncoder* renderCommandEncoder(MTL::RenderPassDescriptor*);
    ComputeCommandEncoder* computeCommandEncoder();
    BlitCommandEncoder* blitCommandEncoder() { return computeCommandEncoder(); }
    void writeTimestamp(CounterHeap*,uint32_t);
    void resolveTimestamps(CounterHeap*,uint32_t,uint32_t,MTL::Buffer*,uint64_t);
    void encodeWait(MTL::Event*,uint64_t);
    void encodeSignalEvent(MTL::Event*,uint64_t);
    void presentDrawable(CA::MetalDrawable*);
    void commit();
    void waitUntilCompleted();
    MTL::CommandBufferStatus status() const;
    void setLabel(NS::String*);
    void pushDebugGroup(NS::String*);
    void popDebugGroup();
    void* nativeHandle() const { return m_native; }
    void retainResource(MTL::Resource*);
    void* snapshot(Bindings&);
    CommandQueue* m_queue;
    void* m_native = nullptr;
    void* m_allocator = nullptr;
    void* m_done = nullptr;
    void* m_residency = nullptr;
    std::vector<Encoder*> m_encoders;
    std::vector<void*> m_objects;
    std::unordered_set<MTL::Resource*> m_resources;
    std::vector<std::pair<MTL::Event*,uint64_t>> m_waits, m_signals;
    CA::MetalDrawable* m_drawable = nullptr;
    bool m_committed = false;
    struct Feedback { std::atomic<bool> failed{false}; };
    std::shared_ptr<Feedback> m_feedback = std::make_shared<Feedback>();
};
class CommandQueue : public Object
{
public:
    CommandQueue* retain() { Object::retain(); return this; }
    explicit CommandQueue(MTL::Device*);
    ~CommandQueue();
    CommandBuffer* commandBuffer();
    void addResidencySet(MTL::ResidencySet*);
    void removeResidencySet(MTL::ResidencySet*);
    void waitForDrawable(CA::MetalDrawable*);
    void* nativeHandle() const { return m_native; }
    MTL::Device* m_device;
    void* m_native = nullptr;
    std::vector<CommandBuffer*> m_pending;
    std::vector<std::pair<void*,void*>> m_freeAllocators;
    bool m_shuttingDown=false;
};
bool isAvailable(MTL::Device*);
CommandQueue* newCommandQueue(MTL::Device*);
MTL::RenderPipelineState* newRenderPipeline(MTL::Device*,MTL::RenderPipelineDescriptor*,MTL::Library*,MTL::Library*,NS::Error**);
MTL::ComputePipelineState* newComputePipeline(MTL::Device*,MTL::Library*,const char*,NS::Error**);
} // namespace rhi::metal4::api
