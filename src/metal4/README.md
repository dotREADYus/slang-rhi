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

Metal4 argument tables are encoder-owned and reused between draws/dispatches.
Metal snapshots their resource entries when each command is encoded; uniform
storage itself still uses immutable command-buffer-owned slices. Tables grow
when later bindings require more slots, and old tables/resources remain retained
through completion. `argument_table_ms` now measures updates including occasional
growth; `argument_table_alloc_ms` and its call count isolate actual table allocation.
The draw/dispatch snapshot regressions mutate textures, samplers, output buffers
and uniforms across commands and command buffers.

Metal4 binding snapshots allocate buffer/offset and texture arrays from reflected
layout counts instead of reserving 256 slots for every draw. Sparse bindings grow
the arrays in the command-buffer arena, retaining zeroed holes and all populated
entries. The existing 256-slot limit is unchanged. Snapshot and uniform data remain
independent for each recorded draw. `SLANG_RHI_COMPACT_BINDINGS=0` restores fixed
256-slot allocation for same-binary performance controls.

Metal4 argument-table updates compare buffer addresses (including offsets),
texture IDs and sampler IDs with the current table's last written entries. Equal
entries skip native setters; newly allocated or grown tables initialize every
active slot, including zero entries. Resource retention and immutable draw
snapshots are unchanged. `SLANG_RHI_ARGUMENT_DELTAS=0` restores full entry writes
for same-binary controls. CPU detail CSV includes `argument_entry_write_calls`
and `argument_entry_skip_calls`; keep diagnostics off for throughput controls.

Metal4 native binding setters cache object pointers per encoder slot. An unchanged
buffer reuses its GPU base address while still applying the current byte offset;
unchanged textures/samplers reuse their resource IDs. Changed resources still pass
through command-buffer retention. Samplers are retained once per native object per
command buffer, including when switching away and back or using another encoder.
All retained objects survive command-buffer completion. `SLANG_RHI_BINDING_REUSE=0`
restores repeated queries and sampler retains for same-binary controls. Detailed
CSV adds `binding_reuse_calls`, `sampler_retain_calls` and
`sampler_retain_skip_calls` (the last counts cross-slot/encoder deduplication,
not same-slot skips, which are included in binding reuse).

Metal4 rasterizer and depth-stencil state is reapplied when the render pipeline
changes or a new render encoder invalidates cached state. Viewport, scissor, blend
color and stencil reference retain their existing independent change checks.
`SLANG_RHI_RENDER_STATE_DELTAS=0` restores rasterizer/depth writes on every draw.
CPU detail now reports `render_state_ms` (including binding setters),
`raster_state_ms` (nested state writes) and `draw_native_ms` (including table binds).
These scopes overlap native recording; do not sum them as exclusive costs.
