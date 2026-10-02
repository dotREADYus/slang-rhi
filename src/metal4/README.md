# Additional Metal 4 backend

`DeviceType::Metal4` selects the SDK-backed Metal 4 command queue, command buffers,
render/compute encoders, compiler pipelines and immutable argument tables.
`DeviceType::Metal` continues to select the original Metal implementation.

CMake enables `SLANG_RHI_ENABLE_METAL4` only when Metal is enabled and a macOS 26+
SDK is available. Runtime device discovery requires macOS 26+ and
`MTLGPUFamilyMetal4`; an explicit unavailable Metal4 request returns
`SLANG_E_NOT_AVAILABLE`, without substituting the classic backend. Metal-cpp's
existing header dependency is retained. New API calls live in the Objective-C++
bridge, which includes the SDK declarations directly.

Resource, format, reflection and binding implementations initially follow the
classic backend in a separate namespace. The command bridge translates reflected
buffer addresses and texture/sampler resource IDs into argument tables. Each
snapshot and referenced native resource survives until command completion. This
separation intentionally avoids changing Metal's existing encoder/binder while the
new implementation matures; consolidating identical resource helpers is future
maintenance work, not a prerequisite for selecting either backend.

The queue permits at most 64 live native recordings and caches at most 16 retired
allocator/command-buffer pairs. Submitted allocators are reset only after their
completion event. Abandoned encoders are ended before recycling. Residency sets
cover pointer-bound resources as well as explicit blit/render attachments. Encoder
transitions use consumer queue-stage barriers; dependent blits and compute commands
use intra-encoder barriers. GPU failures return failure from host waits/readback.

The surface preserves the host CAMetalLayer, pixel sizing and VSync configuration.
Drawable acquisition issues the Metal4 queue wait before any write; presentation
signals the drawable after previously submitted work and then presents it.

Supported paths include indexed/instanced rasterization, compute, buffer/texture
copies and uploads, native imports/swizzled views, parameter blocks, dynamic blend
constants/independent constant-alpha factors, precise occlusion queries, and
rectangle/channel/depth/stencil clears (including MSAA preservation). Unsupported
acceleration-structure/ray-query and timestamp-calibration paths are not advertised;
creating an unsupported query pool or acceleration structure returns
`SLANG_E_NOT_AVAILABLE`. Existing classic-backend limitations such as indirect draws
and mesh commands are retained. A new backend does not imply full Metal API coverage.

Run examples with `example-triangle --device=Metal4 --frames=120` (or `Metal` for
the retained control). Examples without these arguments keep their interactive
behavior. Test selection uses `-select-devices=metal4`; `-device=metal4` is not a
recognized test-harness selector. GPU test names end in `.metal4`, so use
`-tc='*.metal4'` or wildcard filters. Selection distinguishes `metal` from `metal4`,
including optional `:adapter` suffixes.

Validation on the development Apple M2 Pro/macOS host with SDK27: the complete
registered Metal4 GPU suite and explicit selector contract passed 239 cases /
38,828,594 assertions. The GLFW triangle presented 120 frames and exited0. The
initial retail b30 Debug run exited0, rendered coherent geometry/Slug text, and
reported no API errors or native draw fallbacks. Older hardware/OS boot testing is
not claimed. Performance conclusions require isolated game runs and GPU timing;
classic Metal submission timings do not measure this queue.


Timestamp queries use Metal4 counter heaps with fenced GPU readback and query
readiness/reset semantics. TimestampFrequency derives from mach_timebase_info;
raw counters are Mach ticks, not nanoseconds (24MHz on the measured M2 Pro).
Reset invalidates retired heap entries and rejects an outstanding submitted
writer. Precise samples can affect performance and include subsequent work;
completion spans are not exclusive GPU costs. Classic Metal does not advertise
timestamp queries and explicitly rejects pool creation.

After timestamp support, both complete backend selections passed over41million
assertions with no failures. Focused query/occlusion tests, repeated pool reuse,
real shader draws, clock-scale validation and classic unsupported-query tests
also passed. Performance diagnostics are opt-in and must use an untimed control.


CPU diagnostics: `SLANG_RHI_CPU_DETAIL=/absolute/path/log.csv` writes aggregate
recording-thread timings at command-buffer finish. Binding-data construction and
resource tracking are measured before finish; native argument-table construction
is nested inside native recording. Keep this off for throughput controls. Uniform
ordinary data uses 256-byte-aligned, immutable slices of command-buffer-owned
64 KiB upload pages (larger blocks receive a larger page). Pages retire with the
binding cache after command-buffer completion; host shader-object mutation cannot
overwrite earlier draws. Parameter-block argument buffers retain their existing
allocation path.

`MTL_CAPTURE_ENABLED=1` starts the existing device capture at creation. Set
`SLANG_RHI_MANUAL_CAPTURE=1` when the application controls MTLCaptureManager
itself, so initialization does not start a competing capture.


Metal4 submit waits are attached to the first real command buffer and emitted as
native queue waits before its commit. Empty submissions use one recording for
both waits and completion signals. Retained event references follow command-buffer
retirement. Cross-encoder timestamp samples on empty compute/blit intervals can
occasionally reverse by a few microseconds; CPU throughput controls should disable
sampling and diagnostic consumers must reject invalid durations.
