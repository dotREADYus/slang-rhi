# API implementation status

## `IDevice` interface

| API                                | CPU     | CUDA | D3D11 | D3D12 | Vulkan | Metal   | WGPU |
|------------------------------------|---------|------|-------|-------|--------|---------|------|
| `getNativeDeviceHandles`           | :x:     | yes  | :x:   | yes   | yes    | yes     | :x:  |
| `getInfo`                          | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `hasFeature`                       | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `getFeatures`                      | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `getCapabilities`                  | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `hasCapability`                    | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `getFormatSupport`                 | yes     | yes  | yes   | yes   | yes    | yes (1) | yes  |
| `getSlangSession`                  | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `getQueue`                         | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createTexture`                    | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createTextureFromNativeHandle`    | :x:     | :x:  | :x:   | yes   | yes    | yes     | :x:  |
| `createTextureFromSharedHandle`    | :x:     | yes  | :x:   | :x:   | :x:    | :x:     | :x:  |
| `createBuffer`                     | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createBufferFromNativeHandle`     | :x:     | yes  | :x:   | yes   | yes    | yes     | yes  |
| `createBufferFromSharedHandle`     | :x:     | yes  | :x:   | :x:   | :x:    | :x:     | :x:  |
| `mapBuffer`                        | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `unmapBuffer`                      | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createSampler`                    | yes (2) | yes  | yes   | yes   | yes    | yes     | yes  |
| `createTextureView`                | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createSurface`                    | :x:     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createInputLayout`                | :x:     | :x:  | yes   | yes   | yes    | yes     | yes  |
| `createShaderObject`               | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createShaderObjectFromTypeLayout` | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createRootShaderObject`           | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createShaderTable`                | :x:     | yes  | :x:   | yes   | yes    | :x:     | :x:  |
| `createShaderProgram`              | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createRenderPipeline`             | :x:     | :x:  | yes   | yes   | yes    | yes     | yes  |
| `createComputePipeline`            | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createRayTracingPipeline`         | :x:     | yes  | :x:   | yes   | yes    | :x:     | :x:  |
| `readTexture`                      | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `readBuffer`                       | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `createQueryPool`                  | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `getAccelerationStructureSizes`    | :x:     | yes  | :x:   | yes   | yes    | yes     | :x:  |
| `getMicromapSizes`                 | :x:     | yes  | :x:   | yes   | yes    | :x:     | :x:  |
| `getClusterOperationSizes`         | :x:     | yes  | :x:   | yes   | yes    | :x:     | :x:  |
| `createAccelerationStructure`      | :x:     | yes  | :x:   | yes   | yes    | yes     | :x:  |
| `createMicromap`                   | :x:     | yes  | :x:   | yes   | yes    | :x:     | :x:  |
| `createFence`                      | yes     | yes  | :x:   | yes   | yes    | yes     | yes  |
| `waitForFences`                    | yes     | yes  | :x:   | yes   | yes    | yes     | yes  |
| `createHeap`                       | :x:     | yes  | :x:   | yes   | yes    | :x:     | :x:  |
| `getTextureAllocationInfo`         | yes     | yes  | :x:   | yes   | yes    | yes     | :x:  |
| `getTextureRowAlignment`           | yes     | yes  | :x:   | yes   | yes    | yes     | yes  |
| `getCooperativeVectorProperties`   | :x:     | yes  | :x:   | yes   | yes    | :x:     | :x:  |
| `getCooperativeVectorMatrixSize`   | :x:     | yes  | :x:   | yes   | yes    | :x:     | :x:  |
| `convertCooperativeVectorMatrix`   | :x:     | yes  | :x:   | yes   | yes    | :x:     | :x:  |
| `isCooperativeMatrixSupported` (3) | yes     | yes  | yes   | yes   | yes    | yes     | yes  |
| `reportHeaps`                      | yes     | yes  | yes   | yes   | yes    | yes     | yes  |

(1) dummy implementation only
(2) returns nullptr but succeeds
(3) returns false when not supported

## `IBuffer` interface

| API                   | CPU     | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-----------------------|---------|------|-------|-------|--------|-------|------|
| `getDesc`             | yes     | yes  | yes   | yes   | yes    | yes   | yes  |
| `getSharedHandle`     | :x:     | :x:  | :x:   | yes   | yes    | :x:   | :x:  |
| `getDeviceAddress`    | yes (1) | yes  | :x:   | yes   | yes    | yes   | :x:  |
| `getDescriptorHandle` | :x:     | :x:  | :x:   | yes   | yes    | :x:   | :x:  |

(1) returns host address

## `ITexture` interface

| API                    | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|------------------------|-----|------|-------|-------|--------|-------|------|
| `getDesc`              | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getSharedHandle`      | :x: | :x:  | :x:   | yes   | yes    | :x:   | :x:  |
| `createView`           | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getDefaultView`       | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getSubresourceLayout` | yes | yes  | yes   | yes   | yes    | yes   | yes  |

## `ITextureView` interface

| API                                        | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|--------------------------------------------|-----|------|-------|-------|--------|-------|------|
| `getDesc`                                  | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getTexture`                               | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getDescriptorHandle`                      | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `getCombinedTextureSamplerDescriptorHandle`| :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |

## `ISampler` interface

| API                   | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-----------------------|-----|------|-------|-------|--------|-------|------|
| `getDesc`             | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getDescriptorHandle` | :x: | :x:  | :x:   | yes   | yes    | :x:   | :x:  |

## `IFence` interface

| API               | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-------------------|-----|------|-------|-------|--------|-------|------|
| `getCurrentValue` | yes | yes  | :x:   | yes   | yes    | yes   | yes  |
| `setCurrentValue` | yes | yes  | :x:   | yes   | yes    | yes   | yes  |
| `getNativeHandle` | :x: | :x:  | :x:   | yes   | yes    | yes   | :x:  |
| `getSharedHandle` | :x: | :x:  | :x:   | yes   | yes    | :x:   | :x:  |

## `IShaderObject` interface

| API                         | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-----------------------------|-----|------|-------|-------|--------|-------|------|
| `getElementTypeLayout`      | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getContainerType`          | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getEntryPointCount`        | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getEntryPoint`             | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `setData`                   | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getObject`                 | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `setObject`                 | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `setBinding`                | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `setDescriptorHandle`       | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `reserveData`               | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `setSpecializationArgs`     | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getRawData`                | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getSize`                   | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `setConstantBufferOverride` | :x: | :x:  | :x:   | :x:   | :x:    | :x:   | :x:  |
| `finalize`                  | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `isFinalized`               | yes | yes  | yes   | yes   | yes    | yes   | yes  |

## `IShaderTable` interface

## `IPipeline` interface

| API               | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-------------------|-----|------|-------|-------|--------|-------|------|
| `getProgram`      | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getNativeHandle` | :x: | yes  | yes   | yes   | yes    | yes   | yes  |

## `IRenderPipeline` interface

| API               | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-------------------|-----|------|-------|-------|--------|-------|------|
| `getDesc`         | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `getNativeHandle` | :x: | :x:  | :x:   | yes   | yes    | yes   | yes  |

## `IComputePipeline` interface

| API               | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-------------------|-----|------|-------|-------|--------|-------|------|
| `getDesc`         | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getNativeHandle` | :x: | yes  | :x:   | yes   | yes    | yes   | yes  |

## `IRayTracingPipeline` interface

| API               | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-------------------|-----|------|-------|-------|--------|-------|------|
| `getDesc`         | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `getNativeHandle` | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |

## `IQueryPool` interface

| API                         | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-----------------------------|-----|------|-------|-------|--------|-------|------|
| `getDesc`                   | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getResultState`            | yes | yes  | yes   | yes   | yes    | :x:   | :x:  |
| `getResult`                 | yes | yes  | yes   | yes   | yes    | :x:   | :x:  |
| `reset`                     | yes | yes  | yes   | yes   | yes    | :x:   | :x:  |
| `reset(queryIndex, count)`  | yes | yes  | yes   | yes   | yes    | :x:   | :x:  |

## `ICommandEncoder` interface

| API                                    | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|----------------------------------------|-----|------|-------|-------|--------|-------|------|
| `getDesc`                              | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `beginRenderPass`                      | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `beginComputePass`                     | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `beginRayTracingPass`                  | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `copyBuffer`                           | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `copyTexture`                          | :x: | yes  | yes   | yes   | yes    | yes   | yes  |
| `copyTextureToBuffer`                  | :x: | yes  | :x:   | yes   | yes    | yes   | yes  |
| `copyBufferToTexture`                  | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `uploadTextureData`                    | :x: | yes  | :x:   | yes   | yes    | yes   | yes  |
| `uploadBufferData`                     | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `clearBuffer`                          | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `clearTextureView`                     | :x: | :x:  | yes   | yes   | yes    | yes   | :x:  |
| `clearTextureFloat`                    | :x: | yes  | yes   | yes   | yes    | yes   | :x:  |
| `clearTextureUint`                     | :x: | yes  | yes   | yes   | yes    | yes   | :x:  |
| `clearTextureSint`                     | :x: | yes  | yes   | yes   | yes    | yes   | :x:  |
| `clearTextureDepthStencil`             | :x: | :x:  | yes   | yes   | yes    | yes   | :x:  |
| `resolveQuery`                         | yes | :x:  | :x:   | yes   | yes    | yes   | :x:  |
| `buildAccelerationStructure`           | :x: | yes  | :x:   | yes   | yes    | yes   | :x:  |
| `buildMicromap`                        | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `copyAccelerationStructure`            | :x: | yes  | :x:   | yes   | yes    | yes   | :x:  |
| `queryAccelerationStructureProperties` | :x: | :x:  | :x:   | yes   | yes    | :x:   | :x:  |
| `executeClusterOperation`              | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `convertCooperativeVectorMatrix`       | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `setBufferState`                       | :x: | :x:  | :x:   | yes   | yes    | :x:   | :x:  |
| `setTextureState`                      | :x: | :x:  | :x:   | yes   | yes    | :x:   | :x:  |
| `globalBarrier`                        | :x: | :x:  | :x:   | yes   | yes    | :x:   | :x:  |
| `pushDebugGroup`                       | :x: | :x:  | :x:   | yes   | yes    | yes   | yes  |
| `popDebugGroup`                        | :x: | :x:  | :x:   | yes   | yes    | yes   | yes  |
| `insertDebugMarker`                    | :x: | :x:  | :x:   | yes   | yes    | yes   | yes  |
| `writeTimestamp`                       | yes | yes  | yes   | yes   | yes    | :x:   | :x:  |
| `finish`                               | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getNativeHandle`                      | :x: | :x:  | :x:   | :x:   | :x:    | :x:   | :x:  |

## `IPassEncoder` interface

| API                 | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|---------------------|-----|------|-------|-------|--------|-------|------|
| `pushDebugGroup`    | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `popDebugGroup`     | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `insertDebugMarker` | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `writeTimestamp`    | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `end`               | yes | yes  | yes   | yes   | yes    | yes   | yes  |

## `IRenderPassEncoder` interface

| API                   | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-----------------------|-----|------|-------|-------|--------|-------|------|
| `bindPipeline`        | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `setRenderState`      | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `draw`                | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `drawIndexed`         | :x: | :x:  | yes   | yes   | yes    | yes   | yes  |
| `drawIndirect`        | :x: | :x:  | yes   | yes   | yes    | :x:   | yes  |
| `drawIndexedIndirect` | :x: | :x:  | yes   | yes   | yes    | :x:   | yes  |
| `drawMeshTasks`       | :x: | :x:  | :x:   | yes   | yes    | :x:   | :x:  |

## `IComputePassEncoder` interface

| API                       | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|---------------------------|-----|------|-------|-------|--------|-------|------|
| `bindPipeline`            | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `dispatchCompute`         | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `dispatchComputeIndirect` | :x: | yes  | yes   | yes   | yes    | yes   | yes  |

## `IRayTracingPassEncoder` interface

| API            | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|----------------|-----|------|-------|-------|--------|-------|------|
| `bindPipeline` | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `dispatchRays` | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |

## `ICommandBuffer` interface

| API               | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-------------------|-----|------|-------|-------|--------|-------|------|
| `getDesc`         | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getNativeHandle` | :x: | :x:  | :x:   | yes   | yes    | yes   | yes  |

## `ICommandQueue` interface

| API                       | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|---------------------------|-----|------|-------|-------|--------|-------|------|
| `getType`                 | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `createCommandEncoder`    | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `submit`                  | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getNativeHandle`         | :x: | yes  | :x:   | yes   | yes    | yes   | yes  |
| `waitOnHost`              | yes | yes  | yes   | yes   | yes    | yes   | yes  |
| `getTimestampCalibration` | yes | yes  | yes   | yes   | yes    | :x:   | :x:  |

## `ISurface` interface

| API                 | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|---------------------|-----|------|-------|-------|--------|-------|------|
| `getInfo`           | :x: | yes  | yes   | yes   | yes    | yes   | yes  |
| `getConfig`         | :x: | yes  | yes   | yes   | yes    | yes   | yes  |
| `configure`         | :x: | yes  | yes   | yes   | yes    | yes   | yes  |
| `unconfigure`       | :x: | yes  | yes   | yes   | yes    | yes   | yes  |
| `acquireNextImage`  | :x: | yes  | yes   | yes   | yes    | yes   | yes  |
| `present`           | :x: | yes  | yes   | yes   | yes    | yes   | yes  |

Note: CUDA's surface is implemented using a Vulkan swapchain.

## `IAccelerationStructure` interface

| API                   | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-----------------------|-----|------|-------|-------|--------|-------|------|
| `getHandle`           | :x: | yes  | :x:   | yes   | yes    | yes   | :x:  |
| `getDeviceAddress`    | :x: | yes  | :x:   | yes   | yes    | yes   | :x:  |
| `getDescriptorHandle` | :x: | :x:  | :x:   | yes   | yes    | :x:   | :x:  |

## `IHeap` interface

| API                | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|--------------------|-----|------|-------|-------|--------|-------|------|
| `allocate`         | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `free`             | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `report`           | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `flush`            | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |
| `removeEmptyPages` | :x: | yes  | :x:   | yes   | yes    | :x:   | :x:  |


## Occlusion query contract

`RenderPassDesc::occlusionQueryPool` declares optional pass query storage.
`IRenderPassEncoder::beginOcclusionQuery(index)` / `endOcclusionQuery()` delimit
one active query in that pool. Queries are pass-scoped; each slot may be written
only once per command buffer. Separate submitted command buffers may reuse slots.
Nesting, unbalanced scopes, invalid types/ranges and cross-device pools are errors.
A pass ended with an active query reports an error and closes the native query
for recovery; applications must not rely on that recovery.

`QueryType::Occlusion` requires `Feature::OcclusionQuery`: zero means fully
occluded and nonzero means visible, with unspecified nonzero magnitude.
`OcclusionPrecise` requires `Feature::PreciseOcclusionQuery` and returns the exact
number of samples passing per-fragment tests. Results reuse the indexed host
Reset/Pending/Resolved and reset semantics; an unsubmitted query is not a valid
zero result. Queries and their storage are retained by the command buffer.

Initial implementations: D3D11/D3D12 counting and predicate queries; Vulkan
occlusion pools with precise support gated on the enabled device feature; Metal
visibility buffers (initial conservative limit: 8192 slots / 64 KiB), with counting advertised on Apple3
or Mac1 families. CPU/CUDA and WebGPU advertise neither feature and reject these
pool types with `SLANG_E_NOT_AVAILABLE`. WebGPU host result/readiness support is
still unavailable in this library; its native query set is not silently exposed
with inaccurate host semantics. Existing timestamp behavior is unchanged.

| API | CPU | CUDA | D3D11 | D3D12 | Vulkan | Metal | WGPU |
|-----|-----|------|-------|-------|--------|-------|------|
| `beginOcclusionQuery` / `endOcclusionQuery` | :x: | :x: | yes | yes | yes | yes | :x: |
| Host occlusion results / readiness | :x: | :x: | yes | yes | yes | yes | :x: |

This extension adds interface methods and descriptor fields: rebuild consumers
against the matching header/library. Backend implementation status is distinct
from validation status; see the change's test evidence before claiming portability.


## Texture-view rectangle and channel clears

`Feature::TextureViewClear` exposes `ICommandEncoder::clearTextureView(desc)`.
The call is outside all passes; attempting it inside a render, compute or ray
tracing pass fails. Internal clears cannot affect application occlusion queries.
Commands retain the supplied view and do not mutate application encoder state.

The view must select exactly one mip and one layer of a 2D or 2D multisample
texture (including array textures). Its format must support the relevant render
attachment usage. Rectangles are half-open view-pixel coordinates, clipped to
that mip's extent; no rectangles means the entire view. Empty/clipped-away
rectangles are no-ops; inverted rectangles/null arrays are invalid. Layer/mip
selection is independent of application viewport/scissor state. Clears apply to
all samples, preserve other subresources, and retain all unselected channels or
depth/stencil aspects. Color mask None and unselected depth/stencil aspects do
nothing. Depth must be finite in [0,1]; stencil must fit8bits when selected.

Choose `ColorClearValue`'s float member for float/normalized formats (including
sRGB), unsigned member for unsigned integer formats, signed member for signed
formats. Values undergo attachment format conversion. No blending occurs.
Depth/stencil views ignore the color mask; color views reject depth/stencil flags.

D3D11/D3D12/Vulkan/Metal use existing native load clears for whole-view floating
or normalized color/all-channel and depth/stencil operations. Rectangles, masked
channels and exact integer values use a cached internal fullscreen triangle with
scissor/write masks and explicitly scoped depth/stencil state. The public method
can therefore entail first-use pipeline compilation and GPU draw work; it is
not promised to be a free native clear. Pipeline cache entries use internal
references and are retired while the backend device is alive. CPU/CUDA/WebGPU
return NOT_AVAILABLE and do not advertise the feature. Existing clear signatures
and behavior are unchanged.

### Dynamic blend constants

`RenderState::blendColor` supplies RGBA constants for `BlendColor` and
`InvBlendColor` factors. Values default to zero and are recorded with each draw;
changes do not create new pipelines. Metal, D3D11, D3D12, Vulkan and WebGPU apply
these values as dynamic render state and reset them at render-pass boundaries.
The regression test changes constants with the same pipeline, verifies distinct
per-draw results and checks default-zero state in a subsequent pass. CPU/CUDA
backends do not support graphics rasterization.

### Constant-alpha blend factors

`Feature::ConstantAlphaBlend` permits `BlendAlpha` / `InvBlendAlpha` in color
and alpha blend equations. They use `blendColor[3]` replicated to every component
(and its inverse), independently of the RGBA `BlendColor` factors. Metal and Vulkan
report support. D3D12 reports it only when OPTIONS13.AlphaBlendFactorSupported is
true. D3D11, WebGPU, CPU and CUDA do not report it; pipeline creation returns
NOT_AVAILABLE even without the API validation layer and before deferred compilation.
Existing enum values and RenderState layout are unchanged. Mixed-factor GPU tests
check nonzero destination colors, both inverses and dynamic changes on one pipeline.
D3D12 reference: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_blend
