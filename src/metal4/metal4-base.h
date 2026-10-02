#pragma once

#include "../rhi-shared.h"
#include "metal4-api.h"

#include "core/common.h"

namespace rhi::metal4 {

class BackendImpl;
class AdapterImpl;
class DeviceImpl;
class InputLayoutImpl;
class BufferImpl;
class FenceImpl;
class TextureImpl;
class TextureViewImpl;
class SamplerImpl;
class AccelerationStructureImpl;
class RenderPipelineImpl;
class ComputePipelineImpl;
class RayTracingPipelineImpl;
class ShaderObjectLayoutImpl;
class EntryPointLayout;
class RootShaderObjectLayoutImpl;
class ShaderProgramImpl;
class ShaderObjectImpl;
class RootShaderObjectImpl;
class ShaderTableImpl;
class CommandEncoderImpl;
class CommandBufferImpl;
class CommandQueueImpl;
class QueryPoolImpl;
struct BindingDataImpl;
struct BindingCache;

} // namespace rhi::metal4
