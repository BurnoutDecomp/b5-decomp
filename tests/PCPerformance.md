# PC rendering performance

This work restores original work avoidance, draw ordering and SIMD processing
while preserving resolution, antialiasing, LOD distances, reflection settings and
damage visibility. It does **not** establish original-PC minimum requirements or a locked
165 FPS at 1440p.

## Implemented

| Change | Original evidence and PC implementation |
| --- | --- |
| Shared shader-constant shadow | ARTIST `DrawRenderableMeshZOnly::Interpret`, `0x827F6654..66C0` and `0x827F718C..71F8`, skips unchanged source blocks. All PC float-constant writers now share a register-value cache, including GUI and diagnostics. Value comparison handles mutable PC source buffers and overlapping register ranges. |
| Shared sampler-state shadow | ARTIST `shadow::Device::SetState`, `0x82276A2C..34`, and whole texture-state bind `0x8227D17C..98` avoid duplicate writes. The PC cache covers all native sampler writers and keeps pixel, displacement and vertex sampler IDs separate. Failed writes never establish cached state. |
| Native static geometry | The console binds resident GPU resource memory. PC retained vertex/index mirrors share dynamic `DEFAULT/WRITEONLY` pages. First writes use DISCARD; subsequent writes use NOOVERWRITE into unused or GPU-retired spans. EVENT query completion, not CPU frame count, permits reuse. Unsupported devices or allocation failures fall back to individual MANAGED buffers. Streaming retirement and cache generations are preserved. F11 does not reset the device. |
| Exact indexed-draw ranges | Cache the minimum/maximum indices consumed by the final native topology after primitive-reset conversion. D3D9 receives that mesh range rather than the full shared vertex buffer. Ignore incomplete list tails, and retain nonzero base-vertex addressing. |
| SIMD colour-cube blending | ARTIST `0x82AD2F38` and `0x82AD4170` use vector kernels specialized by source count. SSE2 restores both properties for the PC job. Existing PC truncation/clamping, BGRA layout, source order and destination pitches remain unchanged. |
| Optional diagnostics | Wheel index/bounds scans require `BRN_WHEEL_DIAG=1`. Composite GPU readback sampling requires existing `BRN_RT_PROBE=1`. `BRN_WHEEL_ZALWAYS` remains independent. |
| Diagnostic lookup overhead | GUI routing checks the two eligible trace event IDs before reading the environment. Hot AI speed/fan diagnostics cache their startup switches, matching adjacent diagnostic code. Messages, rate limits and game calculations are unchanged. |
| Complete mesh sort keys | ARTIST `0x827FD4CC..5D4` builds keys up to 44 bits; `Submit` at `0x822A0888` shifts the complete u64 key before appending the 20-bit packet offset. Restore priority, shader/material grouping, Z-depth ordering and 36-bit pre-Z keys. `RadixSortJob::Execute` actually calls `std::_Sort<u64*,int>` (`0x82AD28B0`), now matched with in-place `std::sort`. |
| Buffered 2D preparation | Restore the separate `Im2dRenderBuffer` type and share its stream between APT and FLAPT. ARTIST `0x827F9EBC..9F10` dispatches combined transform/texture/blend/static-draw records; the PC dispatcher now handles them. Native NDC/unit-colour inputs are translated into the existing PC logical/byte-colour command representation. The early APT flush and fake renderer-set cast are removed. Movie/debug draws also record into real buffers. Frame callbacks remain serial while the remaining ownership audit proceeds. |

The native state shadows are invalidated at device creation. Any future raw float
constant/sampler write or state-block restoration must use these wrappers or
invalidate them. A future device Reset must also retire/recreate DEFAULT geometry;
the current renderer resizes through additional swap chains without Reset.

## Verification

From the parent workflow checkout:

```powershell
python b5-decomp/tests/run_pc_shader_constant_cache.py
python b5-decomp/tests/run_pc_geometry_buffer_pool.py
python b5-decomp/tests/run_pc_dispatch_sort.py
python b5-decomp/tests/run_pc_im2d_buffer.py
python b5-decomp/tests/run_pc_world_geometry_buffers.py
python b5-decomp/tests/run_pc_tint_blend.py
python b5-decomp/tests/run_pc_fullscreen.py
python b5-decomp/tests/run_pc_display_resize.py
python b5-decomp/tests/run_world_vertex_lifetime.py
.\build.cmd exe --jobs 4
```

The 2D suite passes 62 native checks at 320x180, 1280x720 and 2560x1440:
recording without a device, frozen-bank independence, copied dynamic vertices,
APT/FLAPT layer order, text-shaped reserved runs, colour shifts over a dark texture,
clipping, independent texture/blend flag behavior, and failure of each required
allocation (partial resources are returned and failed buffers cannot be consumed). `--drop-batches` reproduces
the missing opcode in a temporary consumer and fails 15 pixel checks. A 65-second
1440p Road Rage capture completed without assertions or command-buffer overflows;
HUD, popup, minimap arrow and debug text captures were inspected. This establishes
the buffering prerequisite, not parallel frame execution or a measured FPS gain.
The live display case also completed F11, resize, pause-map, minimize and restore;
boot-video playback and the return to GUI/gameplay were captured on the repaired build.

The first six suites passed 323 checks before the geometry pool change. They include real D3D9 register/sampler reads,
rendered pixel checks after geometry retirement/address reuse, 16/32-bit indices,
primitive resets and incomplete tails, nonzero base/minimum indices, fullscreen
persistence, resize rollback and pixel coverage through 4K. The SIMD test compares
complete output allocations, including padding, against a scalar oracle and uses
guard pages at the final source block. Reintroducing the incomplete-tail mistake
makes three native geometry checks fail.

The canonical 2,654-TU build and focused faithfulness gate pass. Eight C4661
immediate-mode template warnings seen when those two TUs were recompiled also
reproduce when compiling their unmodified parent-commit sources. Fresh-eyes review
passed after fixing the incomplete-tail issue.

The reviewed live build also passes the existing seven-check streaming/crash
scenario: five area transitions, eight declaration retirements, crash-camera entry
and returns to driving, with no assertions, exceptions or stale formats. Captured
driving and crash frames were inspected. Evidence: `geometry_live/REPORT.md`.

The live display run passes repeated F11 toggles, minimization/restore, pause-menu
transitions and six window sizes. Captures include 720p, 1080p and 1440p; the OS
limited the requested 3840x2160 window to a 3840x1575 client, rendering 2800x1575.
Full 4K target/pixel coverage is established by the separate native resize test,
not by that OS-limited window. The private-slot config was restored byte-for-byte.
Evidence: `display_live/result.json`, events and frames. Live build SHA-256 prefix:
`a8a9b682cbee`.

## Measurements

Hardware: i7-14650HX, RTX 4070 **Laptop** GPU, 63.7 GiB RAM. The live device log
confirms the NVIDIA adapter. These are short local captures on a busy desktop,
with roughly 30Ã¢â‚¬â€œ34% aggregate host CPU load, not a hardware requirement benchmark.

Private slot 8; same save seed and assets; VSync off; 2x scene MSAA (console default),
alpha-to-coverage, reflections, coronas and post-processing unchanged. Each listed
capture samples about 22 seconds after warm-up and keeps the game foreground for
every sample. Profiling and frame-dump runs are excluded from FPS comparisons.
CPU time is summed across process threads, so it can exceed wall frame time.

| Scenario | Before FPS | After FPS | Before CPU ms/frame | After CPU ms/frame | Host CPU before/after |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1440p stationary in-game scene, repeat pair | 45.00 | 50.75 | 32.01 | 27.53 | 32.58% / 31.88% |
| 1440p scripted Road Rage, first pair | 44.28 | 44.67 | 31.68 | 29.99 | 30.04% / 30.70% |
| 720p stationary in-game scene | 56.49 | 64.05 | 28.79 | 25.21 | 32.64% / 34.23% |

The stationary pair is about 13% faster, with roughly 49 MiB less resident memory
and 91 MiB less private committed memory. Road Rage shows reduced CPU work but
**no convincing FPS improvement** in this pair. Traffic/contact timing is not a
frame-identical replay, so small differences should not be treated as a win.

In Road Rage, the caches skipped about 39% of float-constant uploads and 97% of
sampler-state writes. These are avoided API calls, not percentages of frame-time
improvement. The four-source 32-cube microbenchmark measured 0.405Ã¢â‚¬â€œ0.453 ms scalar
versus 0.084Ã¢â‚¬â€œ0.093 ms SIMD, about 4.8x for this job alone.

Evidence is under the parent checkout's `scratch/performance_0929/`: paired
`samples.summary.json`/`samples.json`, per-run flow logs and executable SHA-256
provenance. `original1440_static_repeat` / `final1440_static_repeat` and
`original1440_roadrage` / `final1440_roadrage` are the rows above. The saved baseline
is `a8a0924abb70...` (pre-optimization build, with the takedown-cap fix already
present); its historical stamp is `21dc3410+dirty`. The measured optimized build
is `bea3c1a9b5f5...`, `8932ad0b+dirty`. The `original720_static` / `reviewed720_static` pair uses the reviewed build
`581724937222...`, including the allocation fallback and incomplete-tail fix.

## Camera-transition allocation stalls

The first pass's individual DEFAULT buffers improved average costs but caused a
latency regression. A 1440p Road Rage event-camera cut created 1,217 vertex and 665
index buffers in one frame: geometry preparation took 195.78 ms and the complete
frame took 239.30 ms. Switching the same executable to MANAGED reduced the worst
frame to 70.27 ms. The current implementation replaces per-mesh DEFAULT allocation
with pooled pages, and stages packed-normal conversion in ordinary CPU memory
before copying to the GPU mapping.

Pages hold immutable slices of one buffer kind. Source retirement invalidates
draw-cache entries immediately, but storage is reclaimed only after a completed
D3D9 EVENT query. Polling never flushes or waits. An unavailable/failed query
quarantines pending storage and selects the MANAGED fallback. No DISCARD operation
can destroy another live slice. Vertex byte offsets and index start offsets travel
with the draw; the existing base/minimum-index rules still apply.

Same 65-second foreground Road Rage scenario, 2560x1440, unchanged graphics:

| Geometry storage | FPS | p50 frame ms | p95 | p99 | Maximum | Host CPU |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Individual DEFAULT | 49.12 | 18.97 | 30.79 | 44.00 | 239.30 | 32.81% |
| Individual MANAGED control | 48.52 | 19.84 | 27.56 | 30.87 | 70.27 | 30.00% |
| Pooled DEFAULT | 56.20 | 17.19 | 23.17 | 28.81 | 37.66 | 32.51% |

FPS is sampled after warm-up; frame percentiles include gameplay from the first
departure from car select through shutdown. The pooled run includes event, crash
and actual takedown cameras, with no frame exceeding 50 ms. Its largest upload
frame cached 1,800 streams with four native allocations in 7.65 ms.

A separate 240-second pooled run reached 54.79 FPS with 38.40% host CPU load.
Its p99 was 34.62 ms and maximum 90.42 ms, with 13 frames over 50 ms. The worst
frame spent 72.56 ms inside presentation. This is evidence that bulk allocation
stalls are reduced, **not** that all gameplay stutter is fixed. Pool residency
plateaued while streamed live data changed and ended at 70 MiB resident / 67.9 MiB
live with no pending retired bytes; completed holes were reused.

New/expanded checks: 61 controlled asynchronous-pool checks; 31 native geometry
checks (including queued draws across address reuse and a shader-based packed
normal oracle); 16 declaration-lifetime checks; 37 resize/pixel checks. The
2,654-TU canonical build passes without warnings for this batch. Fresh-eyes review
passed. CPU tracing is opt-in with `BRN_FRAME_PROFILE=1`, allocates a bounded capture
once, and performs file I/O only after audio shutdown. The `.frames.json` companion
records dropped frames so truncated traces cannot be treated as complete.

Evidence: parent `scratch/performance_goal_0929/`, especially
`default_roadrage_cuts`, `managed_roadrage_cuts`, `pooled_roadrage_cuts`, and
`pooled_long_roadrage`. Per-run traces, summaries, logs and binary provenance are
retained. Pooled measured binary SHA-256 prefix: `b98ad1cd731e`.

The final pool build (`08848411195c`) also passes the seven-check live
streaming/crash case (five placements and 15 declaration retirements) and the
F11/resize/pause/minimize sequence, with private configuration restored. Inspected
driving and damaged-car frames retain road, foliage, vehicle and HUD rendering.
A profiling-disabled 65-second Road Rage run completed without trace output,
assertions or exceptions, with every sample foreground. It measured 49.86 FPS at
40.17% host CPU load; differing host load prevents an overhead estimate against
the earlier profiled runs. Evidence: `geometry_live`, `display_live`, and
`pooled_profile_disabled` in the same evidence directory.

## Original draw ordering

The old port truncated sort keys to 32 bits and approximated the field packing,
omitting material distinctions and depth. The full signature is attested by the
64-bit PPC register operations and DecFIGS `Submit(uint64_t, DispatchCommand*)`.
Colour, opaque Z, alpha-tested Z and pre-Z key fields now follow those instructions,
including the alpha path's unusual reduced pixel-hash mask. Z depth uses the
transformed packed mesh-box centre and the recovered 32767.0 scale.

When the original skips per-mesh frustum tests, its depth path reads an unwritten
stack matrix. The PC implementation computes the mesh centre whenever depth is
needed, including pre-Z's distance gate, so its ordering is defined. This is
explicitly marked as a host adaptation. Material fields use their DWARF names.

The regression compiles real production bodies and headers. It checks key packing
against an independent PPC mask/shift oracle over 4,096 cases, depth conversion
boundaries, all 44 submitted key bits, packet offsets, multi-block flattening,
unsigned sorting, ties and allocation behavior. It passes 19 checks; the prior
`8f5a6b8e` implementation compiles but fails 14 numerically. The fixture exceeds
MSVC's small stack temporary so it detects stable-sort heap allocation.
The 87 native fullscreen/config/pixel checks and canonical build also pass.
Independent assembly review passes. Work-audit findings for the touched object
interpreter's block allocation/assert are covered by its existing `ReserveKey`
call; unrelated instancing/occlusion/job gaps keep the overall TU incomplete.

Foreground 65-second 1440p Road Rage pair: 51.72 FPS before / 51.45 after,
26.46 / 26.13 CPU ms per frame, host CPU 36.80% / 35.46%. p99 was 34.29 / 33.87 ms;
both had three frames over 50 ms. These runs demonstrate **no convincing overall
FPS gain** from this stage. Scripted traffic/contact paths differ; this is a
verified restoration of original ordering, not evidence that the performance
goal is met. Separate live driving/crash captures were inspected and contain no
assertions or exceptions. Evidence: `sort_control`, `sort_candidate`,
`sort_gpu_visual`, and `dispatch_sort_review.md` under the same parent scratch
directory. Measured candidate binary SHA-256 prefix: `86fec661108b`.

The opt-in trace now separates CPU copy submission, native Present waiting and
dispatch sorting. These are CPU intervals: a fast asynchronous copy call does
not establish its GPU cost. A run that loses foreground focus is excluded from
comparative FPS claims (`pooled_partitioned` is one such run).

## CPU and GPU attribution

A separate 20-second, 16-sample capture (`sort_gpu_attribution`) attributes the
game's 3D work to the RTX: about 13.1% average utilization. Its main CPU thread
uses about 72.5% of one core and another game thread about 47.4%. Intel activity
is largely attributed to Windows presentation/compositing and desktop UI
processes. The internal 2560x1600/165-Hz panel is an Intel output. This is
consistent with CPU/presentation limits; low total CPU utilization does not
exclude a constrained thread. Hybrid graphics can use NVIDIA for rendering
while Intel handles display output ([NVIDIA Optimus](https://www.nvidia.com/en-us/geforce/technologies/optimus/technology/)).

The profiling-disabled `stable_cpu_profile` uses proper StackWalk64 unwinding.
It identifies native presentation and indexed-draw submission as the largest
sampled call sites. Repeated diagnostic environment lookups also appear in GUI
event routing and AI speed/fan routines. After the diagnostic changes,
`diag_hotpath_profile` no longer samples those targeted GUI/AI environment calls.
An enabled run (`diag_hotpath_enabled`) still emits 133 AI-speed, 81 fan and seven
GUI trace lines, without assertions or exceptions. Canonical build and review
pass; no full-game FPS gain is claimed for this small change.

The automated harness adds a separate keyboard-polling cost: missing debug-key
named events are reopened on every frame. This is partly harness-specific and
must be accounted for in future measurements rather than attributed wholesale
to normal gameplay. The current native input path must retain key-edge, modifier,
late-created event and capture semantics when that work is optimized.

## Combat workload qualification

`BRN_AI_PAD_PLAYER=race` follows the racing line and is insufficient evidence of
heavy Road Rage performance. A forced player crash is also insufficient.

`BRN_AI_PAD_PLAYER=combat` is an opt-in benchmark controller. It retains the AI
route planner, selects nearby rivals travelling in the same direction, and commits
ordinary player steering/throttle/boost to a close intercept. Physics, damage,
debris and takedown scoring remain the game paths. Steering uses the game's signed
XZ angle convention. Without the variable, this controller does not run.

`tests/PCPerformanceCombatLive.ps1` qualifies the workload through the existing
case runner and a private slot. It requires five player-credited takedowns across
three rivals plus launch/roll evidence for two rival slots. This is a diagnostic
qualification run, with crash logging and the existing `BRN_AI_MADNESS=1` rival
aggression override enabled, not a clean timing comparison.
One 90-second development capture produced seven credited takedowns across five
rivals with no assertions/exceptions. Two subsequent quiet runs produced only one
each; the controller is not deterministic, and those runs were rejected as insufficient
player combat despite reaching four crashing and two airborne rivals at once.

`BRN_FRAME_PROFILE=1` now records player takedown counts/victim masks, peak rival
crash/airborne counts across simulation steps per rendered frame, and the ending QPC timestamp in its
bounded in-memory trace. Match these timestamps to the foreground measurement
window and qualify every measured run. Exclude post-event free driving. Clean
timing runs disable screenshots and verbose crash witnesses; the counters remain.
These checks do not establish 165 FPS or completion of the optimization work.

## Frame overlap integration

Gameplay now runs the original frame coordinator with a native dispatch worker
by default; `BRN_FRAME_PARALLEL=0` selects the serial control. Startup remains
serial. The joined boundary
publishes world, GUI, movie, shader, simple-particle and diagnostic data, and
services resource updates.

Native adaptations preserve window-message processing during waits, propagate
worker failures, and restore the graphics context around a separately buffered
assertion dialog. Foreign worker assertions retain their originating stack;
when the owner is waiting on their operation, they are logged without attempting
to render an incomplete frame. The wait path never blocks the window owner on
an assertion mutex held by dispatch. Particle RNG draws keep the original
generator under narrow locks; logging assembles and formats each thread's lines
independently.

The native coordinator suite covers simultaneous assertions, synchronous device
waits, window-message dependencies, shutdown and exceptions. Particle, logging,
captured-stack, shader-publication and D3D buffer tests accompany it. Deliberate
negative controls reproduce the old deadlocks, shared particle banks and mixed
log lines. Console-derived wheel-smoke, spark-shower, debris-burst and debris
simulation results still pass (136, 24, 73 and 44 checks respectively). Live modal
assertions render correctly and can resume or close normally. These are
correctness checks, not evidence of a particular FPS gain.

The 2560x1440 combat captures with binary `7f4f30343d15` include a diagnostic
run with eight player credits across five rivals and launch/roll evidence on
three rival slots. A quiet parallel run qualified with seven credits, four
simultaneously crashing rivals and two airborne rivals; it averaged 66.17 FPS
(p99 26.07 ms). The accepted serial control recorded eleven credits and averaged
58.77 FPS (p99 28.50 ms). Both stayed foreground for the entire 90-second window,
kept the event active and had no assertions/exceptions. Exact fights differ, so
this is a preliminary comparison, not a controlled percentage improvement.
An earlier three-credit serial run was rejected. The 165 FPS target remains
unmet. Evidence is under `scratch/performance_goal_0929/combat_*_0930_*` in the
workflow checkout.

Default activation was then built as `24244b930dfe` and verified through actual
worker-start logging, F11, pause/resume, minimize/restore and window resizing
from 1024x768 through ultrawide and the OS-clamped 3840x1575 client. The renderer
retains its 16:9 viewport. The case exited normally with no assertions or
exceptions, and representative world/menu captures were inspected.

The optional frame trace now separates update timing into display preparation,
frame start, simulation, resource processing, publication and timing bookkeeping.
Resource detail includes pool, memory, load, unload, file I/O and attribute work.
These resource timings are inclusive: file I/O is inside load/unload, so do not
sum all resource columns. The six update columns use the same measured ticks as
their aggregate and sum to `update_ms`.

`combat_resource_detail_0930` qualified with seven player credits and full
foreground focus. Its largest resource stall was a 27.12 ms unload, including
12.29 ms of file I/O; other unloads took 14–18 ms. That native unload path
reread the bundle to recover resource IDs. ARTIST's `CheckForUnloads`
(`0x828FB308`) instead uses its loaded-bundle table and entry-list resource.
The diagnostic baseline build is `d4c1d975ad08`.

The synchronous loader now retains the original type-29 entry list in its pool,
using the bit-63 filename-hash marker from ARTIST. Unload releases those member
references and then the list, with no file access. Repeated low-level loads and
shared dependency resources retain matching references. Allocation failures roll
back the entire attempt before fixups; the native path no longer reports a
partially allocated bundle as successfully loaded. The canonical list layout is
also shared with `AllocatePoolModuleState` (owner name, count at 0x100, IDs at
0x108). This does not yet restore the higher-level logical bundle table or the
asynchronous streaming and deallocation state machines.

`run_pc_bundle_ownership.py` passes 46 checks against production loader bodies
with deterministic file/heap/fixup boundaries. Cases include duplicate/shared
loads, removed or changed files, failed allocations, pool reuse, unknown unloads
and fixup ordering. The pre-fix control (`--rev 78122001 --skip-malformed`) fails
28 of 44 checks; malformed-input cases are skipped because that loader does not
validate their bounds. The fixture does not exercise GPU cache invalidation or
defragmentation.

Build `15a0f3131d98` passed the shipping build without warnings. Its quiet
`combat_retained_list_0930_repeat` run qualified with six player credits across
five rivals, peak five crashing/two airborne rivals, an active event and all
883 samples foreground, with no assertions or exceptions. Unload-only frames
have zero file time; the largest unload was 11.57 ms. Loads still reached
23.10 ms. Overall performance was 64.48 FPS, p99 27.29 ms: no overall FPS gain
or 165 FPS lock is established by this change. An earlier two-credit run that
ended the event was rejected. These traces are under
`scratch/performance_goal_0929/` in the workflow checkout.

The separate `bundle_streaming_live` functional capture exercised GPU declaration
retirement and crash-camera recovery with zero stale-format reports, assertions
or exceptions. Four of five requested relocations seated successfully; one
request made during a crash was rejected and is not counted as a valid
transition. Inspected frames show intact scenery, cars and HUD after recovery.
The run contains frame captures and diagnostics and is not timing evidence.

An output-swap-chain DISCARD experiment passed display checks but showed no
convincing benefit in the accepted combat runs; it was removed. Its private
measurements remain under `present_discard_combat_0930_repeat` and
`present_copy_combat_0930_repeat`. No presentation speedup is claimed.

## Staged loading foundations

The next restoration stage supplies real native bundle-loader IO queues and
allocation records, the original batch allocation state and its pool helpers.
Successful memory types survive purgatory retries instead of being allocated
again. The old 32-bit-offset request decoder and 48-byte response buffer have
been replaced by typed records. This is preparation for the original streaming
FSM: the live game still uses `ProcessLoadRequests` and its synchronous loader.

This work also restores `Heap::Free(void*)`, which was empty. It is used by
partial allocation cleanup and live resource replacement. The real-heap suite
`run_pc_resource_batch.py` passes 33 checks, including retained batch allocations,
failure cleanup, the defrag handoff, alias propagation and retirement callbacks
before freeing memory. Its `--old-free` control fails seven checks (32 execute;
the completion-only defrag check is skipped when the prerequisite fails).
`run_pc_resource_streaming_io.py` passes 14 checks over the actual IO buffers;
its `--old-driver` control fails four native-record decoding checks. The earlier
46 bundle ownership checks also pass.

The foundation build `c1c8ecb29d57` passed startup and functional streaming/camera
coverage (`staged_allocation_live`) with zero new assertions, exceptions or stale
vertex-format reports. Three requested relocations seated successfully; two
requests during crashes did not, and are excluded. Inspected frames show the
damaged car, scenery and HUD through crash recovery. This capture is not a combat
timing result. Final build `567cdc9c12da` changes only comments and an assertion
message from that live build and compiles without warnings.

Async activation still requires the original loader and pool dispatch, complete
scratch/defrag initialization, emergency-defrag inputs, and failure teardown.
The allocation defrag test supplies the planner's completion at its boundary;
it does not run the actual planner. The live-replacement unit test covers the
real alias ring and a retirement callback boundary, not D3D cache contents.
Console per-step PerfMon hooks and its debug allocation-failure injector remain
outside this restoration. No new FPS gain or completion of streaming is claimed.

## Resource-list lifecycle and native pool records

The resource lifecycle extension passes 58 checks in `run_pc_resource_batch.py`.
It exercises the recovered partial/final fixup passes with reciprocal imports,
dependency-pool ownership, and actual native pool output queues. Unload replies
are emitted after references are decremented and the original delay is armed;
the test does not claim that the still-unconnected per-pool retirement loop ran.
Pool options and deletion replies now use typed native fields. The
`--old-records` control restores their pre-change offset-based bodies and fails
four of the 58 checks. The 14 streaming IO checks also pass.

This extension remains a prerequisite for the original asynchronous loader.
The game still uses synchronous loading. Pool dispatch, full defragmentation
initialization/progression, and failed-load teardown are activation gates; the
passing unit tests and clean shipping build do not establish an FPS improvement.

Build `5bb1cb0e81a3` passed a quiet 90-second 2560x1440 combat run
(`combat_pool_lifecycle_0930`): six player credits across four rivals, peak four
crashing/two airborne rivals, 1,926 frames with multiple crashing rivals, no
event end, and all 883 focus samples foreground. No assertions or exceptions
were recorded. It averaged 62.93 FPS, with p99 27.22 ms and maximum 38.98 ms.
The fights differ from previous runs, so these numbers establish neither a
speedup nor a regression. The 165 FPS objective remains unmet.

## Remaining original optimization gaps

- Frame overlap is active, but the measured dispatch/presentation path still
  dominates the frame; broader combat and streaming coverage remains useful.
- Original wheel/mesh instancing is expanded into individual native draws. Restoring
  hardware instancing needs the matching shader and instance-stream representation;
  merely changing the draw flag is insufficient.
- Native occlusion queries/conditional rendering remain disabled or incomplete.
  Restore the original visibility work only with a conservative PC backend that
  preserves visible geometry and does not turn query reads into GPU stalls.
- Presentation and resource-update spikes remain after pooling. Separate the
  output copy from the native Present wait, and audit original resource pacing.
- TUB's native device creation (`0x947F10`) selects PUREDEVICE when supported.
  This port uses ordinary hardware vertex processing and relies on native Get-state
  operations. Its missing complete state shadow must be resolved before safely
  enabling the original pure-device path. Its value must then be measured: native
  D3D9 already filters redundant state changes, and pure devices can be slower
  without complete application-side filtering ([Microsoft DirectX FAQ](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/directx-9-frequently-asked-questions)).
- Proper Windows stack unwinding in `final1440_calls_profile` identifies retained
  `DrawIndexedPrimitive` submission and presentation waiting as the dominant remaining
  sampled costs. The early raw stack-scan candidates were not valid call stacks and
  must not be used for attribution.

Static geometry conversion/cache reuse and streaming invalidation already existed
before this pass. They were retained. No graphics-quality reduction or restoration
of the original five-damaged-car visibility cap is used to improve these numbers.
