# PC rendering performance

This work restores original work avoidance, draw ordering and SIMD processing
while preserving resolution, antialiasing, LOD distances, reflection settings and
damage visibility. It does **not** establish original-PC minimum requirements or a locked
165 FPS at 1440p.

## Implemented

| Change | Original evidence and PC implementation |
| --- | --- |
| Shared shader-constant shadow | ARTIST `DrawRenderableMeshZOnly::Interpret`, `0x827F6654..66C0` and `0x827F718C..71F8`, skips unchanged source blocks. All PC float-constant writers now share a register-value cache, including GUI and diagnostics. Value comparison handles mutable PC source buffers and overlapping register ranges. |
| Shadow caster cull brackets | Restore ARTIST `Render` `0x8240C84C..CB0C`: the initialized manager flag selects front-cull locks for lists 2/3 by default, or 0/1/4 when cleared. Factory slots at `0x83010A38/3C` pin BACK/FRONT with zero depth biases. Locked groups skip material rasterizer writes; every bracket unlocks and restores BACK before later rendering. |
| Extended-detail mesh-job capacity | The optional parallel conversion path can exceed the original 64 saved chains per list with extended LOD settings. Coalesce additional chains through their relocated tails, preserving every key and the original reconnect order without increasing the list footprint or allocating during a flush. The separate shared-bin memory bound remains in force. This capacity fix alone does not establish a speedup or enable parallel conversion by default. |
| Shared sampler-state shadow | ARTIST `shadow::Device::SetState`, `0x82276A2C..34`, and whole texture-state bind `0x8227D17C..98` avoid duplicate writes. The PC cache covers all native sampler writers and keeps pixel, displacement and vertex sampler IDs separate. Failed writes never establish cached state. |
| Native static geometry | The console binds resident GPU resource memory. PC retained vertex/index mirrors share dynamic `DEFAULT/WRITEONLY` pages. First writes use DISCARD; subsequent writes use NOOVERWRITE into unused or GPU-retired spans. EVENT query completion, not CPU frame count, permits reuse. Unsupported devices or allocation failures fall back to individual buffers (MANAGED on D3D9, DEFAULT on D3D9Ex). Streaming retirement and cache generations are preserved. |
| Exact indexed-draw ranges | Cache the minimum/maximum indices consumed by the final native topology after primitive-reset conversion. D3D9 receives that mesh range rather than the full shared vertex buffer. Ignore incomplete list tails, and retain nonzero base-vertex addressing. |
| SIMD colour-cube blending | ARTIST `0x82AD2F38` and `0x82AD4170` use vector kernels specialized by source count. SSE2 restores both properties for the PC job. Existing PC truncation/clamping, BGRA layout, source order and destination pitches remain unchanged. |
| Optional diagnostics | Wheel index/bounds scans require `BRN_WHEEL_DIAG=1`. Composite GPU readback sampling requires existing `BRN_RT_PROBE=1`. `BRN_WHEEL_ZALWAYS` remains independent. |
| Diagnostic lookup overhead | GUI routing checks the two eligible trace event IDs before reading the environment. Hot AI speed/fan diagnostics cache their startup switches, matching adjacent diagnostic code. Messages, rate limits and game calculations are unchanged. |
| Complete mesh sort keys | ARTIST `0x827FD4CC..5D4` builds keys up to 44 bits; `Submit` at `0x822A0888` shifts the complete u64 key before appending the 20-bit packet offset. Restore priority, shader/material grouping, Z-depth ordering and 36-bit pre-Z keys. `RadixSortJob::Execute` actually calls `std::_Sort<u64*,int>` (`0x82AD28B0`), now matched with in-place `std::sort`. |
| Buffered 2D preparation | Restore the separate `Im2dRenderBuffer` type and share its stream between APT and FLAPT. ARTIST `0x827F9EBC..9F10` dispatches combined transform/texture/blend/static-draw records; the PC dispatcher now handles them. Native NDC/unit-colour inputs are translated into the existing PC logical/byte-colour command representation. The early APT flush and fake renderer-set cast are removed. Movie/debug draws also record into real buffers. The frame overlap integration below publishes them at the joined frame boundary. |

The native state shadows are invalidated at device creation. Any future raw float
constant/sampler write or state-block restoration must use these wrappers or
invalidate them. Legacy D3D9 resizes through additional swap chains without Reset.
The D3D9Ex presentation path uses ResetEx, which preserves resources and device
state; the retained render target is rebound afterward. Replacing it with legacy
Reset would require a separate resource and state recovery implementation.

## Verification

From the parent workflow checkout:

```powershell
python b5-decomp/tests/run_pc_shader_constant_cache.py
python b5-decomp/tests/run_pc_shadow_cull.py
python b5-decomp/tests/run_pc_geometry_buffer_pool.py
python b5-decomp/tests/run_pc_dispatch_sort.py
python b5-decomp/tests/run_pc_im2d_buffer.py
python b5-decomp/tests/run_pc_world_geometry_buffers.py
python b5-decomp/tests/run_pc_instancing.py
python b5-decomp/tests/run_pc_tint_blend.py
python b5-decomp/tests/run_pc_fullscreen.py
python b5-decomp/tests/run_pc_flip_resources.py
python b5-decomp/tests/run_pc_flip_presentation.py
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

The combat fixture now requests the stock PUSPK01 pickup through the existing
debug car-change event. Automatic event start waits until the junkyard has
closed and the requested car is active and loaded in the world snapshot. The
case checks which model was streamed **before** event start: a swap after Road
Rage ends cannot satisfy it. This fixes the old race where event start disabled
the freeburn-only car swap. No physics, damage or scoring parameters change.

The first corrected visual run (`combat_loaded_pickup_visual_0930`, build
`efca51988650`) verifies PUSPK01 before start, driving, zero assertions/exceptions
and damaged-car/HUD recovery. It remains rejected as combat evidence: its
90-second measurement contains three player takedowns and only one airborne
rival at once. The setup correction passes independent review; the 2,668-TU
build is clean. A reliable setup alone does not qualify a performance sample.

The controller now prefers a reachable intercept over an unreachable avoidance
target and commits to a side hit when alongside, keeping its existing speed
dependent steering limits and returning to lead pursuit as the rival moves
ahead. These changes affect only the opted-in benchmark pad. Aiming checks pass
11/11; restoring the previous aiming body fails one. Independent review passes.

Visual run `combat_side_hit_visual_0930` on `42061b5c269c` meets the workload
threshold: eight player-credited takedowns across five rivals, peak four crashing
and two airborne rivals, 1,971 frames with multiple rival crashes, and no asserts
or exceptions. The event remains active and all 883 focus samples are foreground.
Captured frames show an airborne rival, collision debris, damage and the crash
camera. Screenshot capture excludes this run from clean timing comparisons.

The quiet repeat (`combat_side_hit_quiet_0930`, same executable) has four
credited takedowns across two rivals, peak five crashing/four airborne rivals,
1,466 frames with multiple crashes, full foreground and zero asserts/exceptions.
It is rejected by the unchanged five-takedown/three-victim minimum. The fights
remain variable; no new qualified FPS result or causal FPS improvement is claimed.

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

The subsequent defrag completion correction restores `IsDefragmenting()` and
the original idle initialization. Both the intelligent and emergency pollers
had mistaken the retained memory-type selector for the frame counter that marks
completion. `run_pc_resource_defrag_state.py` passes 20 checks; its `--old-poll`
control fails six. The planner and final allocator are counting boundaries in
this test, so it does not exercise actual relocation. The expanded resource
batch suite passes 59 checks; `--old-init` fails the new idle-initialization
check. These fixes remove activation blockers without enabling asynchronous
streaming or establishing a performance gain.

The functional capture `combat_defrag_idle_visual_0930` on build `3b7c5003971b`
records nine player takedowns across six rivals, peak three crashing/three
airborne rivals, and no event end, assertions or exceptions. Captures 2520 and
2880 show a flying rival wreck and a two-in-a-row takedown; 3120 shows resumed
driving with the world, player and HUD, and 3600 covers the player's own wreck.
All 884 focus samples were foreground. Frame captures were enabled, so this
run is functional evidence and is excluded from FPS comparisons.

## Scratch relocation and pool maintenance

The pool manager now runs the original allocation/deallocation, intelligent and
emergency defrag state dispatch and per-pool maintenance after the frame joins.
Scratch storage uses native records, bounded gather/scatter copies, the original
768-entry capacity, and 1 MiB main / 2 MiB graphics staging from the existing
low-4GB resource arena. Default resource rebasing, alias publication, reciprocal
imports, retirement delays and native graphics-cache retirement are restored.

The original emergency bounce-copy loop repeatedly overwrote the start of its
destination for multi-chunk overlaps. Its native repair advances both addresses
and selects the safe copy direction. Converted car texture data includes a
1,398,128-byte resource, larger than the original 1 MiB bounce buffer.

Focused checks pass: scratch copies 28/28, actual silent pool relocation 17/17,
emergency copy job 24/24, driver parameters 11/11, and lifecycle 59/59.
Restoring the old stream, empty rebase, overlapping copy and parameter bodies
causes 7, 3, 9 and 3 failures respectively. The relocation test uses real heaps,
alias rings, byte copies and two staging batches; its emergency dispatcher is
an aborting boundary. It does not establish full emergency pool completion.
Fresh review passed after restoring both original null-import-table branches.

Build `82fb05db0a67` compiles all 2,665 TUs cleanly and ran 90 seconds at
2560x1440 with 883/883 foreground samples and zero assertions or exceptions.
Inspected captures show the damaged player car, world and HUD across a wreck
and recovery. The event ended with zero player takedowns, so this run
(`combat_scratch_reviewed_0930`) is rejected as combat evidence. Its FPS is
excluded; this is functional startup/maintenance coverage only.

The subsequent planner correction explicitly clears the spare record visited
by the original inclusive scans. ARTIST's heap flattening used `dcbz128`
look-ahead clearing; the native path had omitted its observable effect. The
explicit guard also strengthens short-heap behavior where the original cache
line operations did not necessarily clear that particular record. The
addressed-allocation helper also now uses its recovered original capacity
assertion and native fields instead of silently refusing overflow.

The expanded relocation suite passes 30 checks, including graphics staging and
actual emergency planner/job/pool completion. A real 3776-byte allocation fails
against separate 128- and 3712-byte free spans, then succeeds after compaction,
leaving 64 free bytes. It checks payloads, internal and graphics pointers,
alias publication order and completion latches. It uses the same direct job
dispatch as the native runtime; asynchronous scheduler waits abort the fixture.
The `--old-planner` control restores the previous production body and fails
eight of the 30 checks after asserting on the stale one-past node.

Build `2ed5a773248b` passes the canonical build. The quiet 90-second run named
`combat_planner_pickup_0930` actually retained PUSMC02: the requested PUSPK01
debug swap never passed the game's no-active-event gate. It recorded three
player takedowns, peak five crashing/three airborne rivals, full foreground,
and zero assertions/exceptions, but the event ended. It is rejected as a
qualified timing run; its name does not establish which car was tested.

The loader still uses synchronous bundle loading and does not yet produce the
staged allocation-list requests. ResourceModule's emergency stall handling,
staged I/O, failed-load teardown, and live graphics-cache behavior during actual
in-game relocation remain activation gates. No FPS gain is established.

## Staged bundle protocol and native decompression worker

The recovered loader stages now use named native fields, two stream slots,
priority queues, typed allocation/fixup events, partial reads and bounded partial
fixups. The byte-copy cursor includes both the resource disk offset and the
bundle's memory-type data offset, as ARTIST's assembly requires. The native
loaded-bundle table tracks logical references and releases the pool list only
when the final owner unloads it.

Resident reuse deliberately corrects an original inconsistency: ARTIST's
`CheckForLoads` compares an untagged CRC at `828FB934`, while insertion at
`828FB114` and unload use bit 63. The native lookup uses the tagged ID.
Live-update replacement requests bypass reuse, so they still open their data
and cannot report success before replacement. This branch is an explicit bug
correction, not a claim of literal original behavior.

The native decompressor now initializes zlib with `sizeof(z_stream)` and the
vendored version, copies named snapshot fields, and accepts the original second
EA job parameter. Each interface owns a stable worker: vendored zlib binds its
internal state to the stream address, including when another thread resumes it.
The original source-empty initial flush and resume behavior are preserved.

Production protocol tests pass 31/31 and worker/zlib tests 14/14. Restoring the
old header stage fails 11 checks (the earlier 30-check suite); restoring the
untagged lookup fails four, swallowing live replacements fails one, and restoring
the old zlib initialization fails eight. Protocol tests replace the disk reader,
pool replies and base-module lifecycle; worker tests replace heap allocation.
Neither suite establishes the complete runtime pipeline. Independent review
passes, and the shipping build compiles 2,668 TUs without warnings or errors.

Functional run `bundle_stages_layout_0930` on `ee17c7dd4f60` passes seven log
checks with no assertions, exceptions or stale vertex formats. Only three of
five requested relocations actually seat correctly; the two attempted during
crashes are rejected. Captures show normal driving, damage and HUD recovery, but
a recovery capture also shows sparse/white ground. This is not complete visual
coverage or a combat timing result. Final build `7f5bc07983c1` additionally
contains the tested resident-replacement guard; that staged path is still cold.

The compressed stage now restores ARTIST's 512 KiB input batches and
nonblocking job completion. It preserves the staging buffer while a job is
pending and carries an unfinished resource into the next batch. The previously
missing `WaitForFlushJobs` body is at `828DB428`; the exporter omitted it.
Source-present partial entries and a created-but-source-empty next entry have
different completion checks, which the restored code retains.

The combined production stage/interface/worker/zlib fixture passes 15 checks:
a 700,000-byte incompressible resource split across batches, gaps and resident
skips, pending-job protection, completion on another thread, exact output bytes,
guards, allocation cleanup, delayed disk availability and blocking completion.
Restoring the old stage fails ten checks; dropping the carried entry fails three.
Disk, job scheduling and heap allocation remain explicit fixture boundaries.
The uncompressed fixture still passes 31 checks, with aborting compression
boundaries so accidental compressed execution cannot produce a false pass.

Build `fadb41f1fab4` passes the canonical 2,668-TU build and independent review.
Its 90-second visual Road Rage check (`combat_compressed_layout_0930`) records
eight credited takedowns across four rivals, peak five crashing/two airborne
rivals, an active event, and zero assertions/exceptions. This is functional
coverage only: screenshot capture and 821/882 foreground samples exclude FPS
comparisons, and the restored compressed path is still inactive in the game.

At that checkpoint ResourceModule still selected synchronous loading.
Allocator-backed construction, native job execution, failed-load teardown and
staged I/O were activation gates. Original ResourceModule returns the pool's busy result to
GameDataModule and `mbStalled`; it does not spin internally. Restore that return
propagation with the staged pipeline. This batch establishes no FPS gain.

## Native job execution and decompression integration

The recovered EAJobs scheduler now executes jobs through the shipping native
EAThread library. The repairs preserve full-width entry points and arguments,
the original high-word queue states and barriers, event-only jobs, completion
semaphores, profiling records, and worker shutdown. Wait callbacks retain their
pointer context and full 64-bit start timestamp. Worker identity uses the same
native system ID for enumeration and profiling.

`python b5-decomp/tests/run_pc_native_jobs.py` passes 34 checks with actual
native workers. Erasing saved arguments fails eight checks in the original
18-check suite; truncating the wait timestamp fails three in the expanded
32-check suite. The relocation integration still passes 30 checks.

`python b5-decomp/tests/run_pc_native_decompression.py` passes ten checks using
the actual engine heaps, scheduler, EAThread workers, decompression interface,
worker and zlib. The original 400 KiB arena accommodates 128 native job slots;
the 128 KiB inflate arena handles a 700,000-byte resource across two batches.
Polling and blocking completion reproduce exact bytes, preserve guards and
return both heaps' storage after repeated streams and teardown. Disk input is
fixture data; this does not establish native filesystem/loader integration.

The canonical 2,668-TU build and scheduler review pass. Hardware job startup,
allocator-backed loader construction, I/O routing and failure teardown were
still activation work at that checkpoint. The production watchdog installer is unresolved; the
wait-helper test installs its predicate inside the fixture. This component
repair establishes no measured gameplay FPS gain.

Final build `abd4531d1fd0` also passes a 120-second visual Road Rage check
(`combat_native_jobs_reviewed_0930`): 16 credited player takedowns across seven
rivals, peak five crashing/two airborne rivals, and zero assertions/exceptions.
All 1,178 focus samples are foreground. Captured damage, world and HUD frames
were inspected; screenshot capture excludes this run from FPS comparisons.

## Staged loader runtime activation

ResourceModule now drives the original staged loader, pool and memory pipeline.
Hardware initialization creates three native EAThread workers using the original
400 KiB scheduler arena and 128 job slots. GameData supplies the original loader
capacities: two 4 MiB stream buffers, a 512 KiB header buffer, 512 bundle records
and 10,240 resource records. Native pointer widths determine table allocation
sizes. The 512 KiB compressed-data staging buffer belongs to the loader allocator.

Pool replies become visible to the loader on the next update, and the pool's
busy result reaches GameData. Read-ahead retains independent stream-buffer
ownership. Failed opens return a failed bundle response and release their stream
reservation. Cancellation waits for an outstanding decompression job before
releasing its inflate state or resource destinations.

Activating this path exposed two missing runtime details. Vertex-descriptor
resource type 10 was unregistered; it now prepares its native declaration during
FixUp using the existing lifetime cache, without altering the current draw's
published vertex layout. A resident-cache completion could also overtake an
earlier bundle still being fixed up. Cached replies now wait for that completion;
uncached read-ahead remains enabled.

Focused validation passes 87 checks: 35 uncompressed stage checks, 15 compressed
stage checks, 12 native decompression/heap/cancellation checks, 13 native request
shuttle checks, and 12 D3D9 declaration preparation/lifetime checks. Removing the
completion-order guard fails two checks; publishing draw state during declaration
preparation fails four. Independent review and the canonical 2,668-TU build pass.

Build `0926b5ea90c9` completed a 120-second visual Road Rage run with seven player
takedowns across five rivals, peak four crashing/three airborne rivals, and no
assertions or exceptions. Takedown-camera damage and subsequent world/HUD frames
were inspected. The event ended during the measurement window, and screenshots
were enabled, so this run establishes no FPS comparison.

A subsequent pair of 90-second 1440p runs with screenshots disabled both passed
the combat gate and all 884 foreground samples. Control `abd4531d1fd0` recorded
66.63 FPS, p99 25.96 ms and maximum38.89 ms; candidate `0926b5ea90c9` recorded
65.61 FPS, p99 24.60 ms and maximum32.71 ms. Host CPU load was31.12%/31.27%.
Process CPU time was24.25/26.56 ms per frame. Combat differed (six versus ten
player takedowns, peak six versus five crashing rivals), so the pair establishes
neither an average FPS gain nor a causal tail-latency improvement. Both had zero
assertions, exceptions or frames exceeding50 ms. Run directories are
`loader_timing_control_0930` and `loader_timing_candidate_0930`.

Remaining boundaries are explicit: the native adapter still uses serialized
static IO buffers; file/patch request routes 20..23/25 and filesystem-status output
are absent. Corrupt/mid-read failure cleanup is unproved. BrnMain still gates the
full game-module release chain, so ResourceModule cancellation is tested when
invoked but is not claimed as normal-exit cleanup. Hardware shutdown joins its
workers before retiring their allocator. These changes do not establish 165 FPS.

## Native geometry instancing

The original instance groups now reach D3D9 as one indexed draw in both colour
and shadow passes. Dispatch packets retain the original matrix/index snapshots;
the instance stream supplies each matrix and wheel value to a variant of the
existing vertex shader. Shader arithmetic and the pixel shader remain unchanged.
Unsupported register/stream combinations retain individual draws with complete
per-instance constant bindings. `BRN_INSTANCING_EXPAND=1` selects the previous
CPU expansion for comparisons.

The native backend follows the [D3D9 indexed-instancing contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/efficiently-drawing-multiple-instances-of-geometry):
separate geometry and instance streams, reset frequencies after submission,
dynamic buffer retirement through DISCARD/NOOVERWRITE, and declaration-lifetime
cache invalidation. Matrix instructions also retain their implicit consecutive
register reads; mixed uniform/instance spans use contiguous temporary copies.

`run_pc_instancing.py` passes 17 native checks. One five-instance draw exactly
matches five individual draws, including distinct transforms, wheel values and
overlapping blending. Targeted valid `m4x4 c19` and `m3x2 c23` programs test spans
crossing a world matrix at c20..23. Ignoring implicit spans fails both pixel tests;
forcing one matrix for every instance fails the general pixel comparison.
The existing declaration-retirement and dispatch-sort suites pass 12 and 19
checks respectively. Independent review and the canonical build pass.

The initial live build completed a 90-second Road Rage with eight credited
takedowns, four victims, peak four crashing/two airborne rivals and no assertions
or exceptions. Its trace recorded about 96 native batches per frame with instances,
averaging four instances per batch, among roughly 5,090 total draws. Screenshots
exclude its FPS from comparisons. Final build `f9d0599a18dc`, including the reviewed
matrix correction, passes fullscreen, resize, pause/resume and minimize/restore
checks; normal and odd-resolution driving frames were inspected.

The first quiet comparison lost focus and was rejected. No average FPS gain is
claimed yet. Removing roughly 300 draws from a scene with over 5,000 should not
be mistaken for the 2.5x throughput improvement needed to move 66 FPS to 165 FPS.
The next profiling pass must separate remaining shader/state/dispatch costs from
native Present waiting. Existing `geometry_prepare_ms` measures buffer creation,
not the full retained-cache lookup path.

## Original pre-Z range and draw-cost attribution

ARTIST's renderer constructor (`8240A778`, stores at `8240BC78/BC7C`) enables
near-only pre-Z and sets its distance to 200.0 (`8203BA4C = 43480000`). Render
copies that **linear** distance into the object context at `8240C0FC..C130`.
The mesh interpreter compares it to the transformed mesh centre's clip-space W
at `827FD62C..654`. The separate near-only switch controls an occlusion global,
not this context value.

The port previously chose 100000 and squared it, effectively drawing distant
opaque geometry again in the depth-only prepass. The original defaults and
linear context value are restored. Normal colour passes still draw distant
geometry. `BRN_PREZ_ALL=1` selects the previous range for comparisons.

`run_pc_prez_range.py` passes seven checks using production context setup and
the actual admission expression. It covers the 200 boundary, adjacent floats,
distant meshes, the independent switch, nonintegral tuning and frame setup.
The old context body fails five checks. Independent review, scoped faithfulness
and the canonical build pass. A live 1440p capture shows intact near and far
geometry, car, road and sky; the run exits without assertions or exceptions.

Same-binary 1440p comparisons on build `75d6472f5a73`:

| Workload | Previous range | Original range | Interpretation |
| --- | ---: | ---: | --- |
| Fixed scene, 45 seconds | 60.19 FPS | 70.74 FPS | 17.5% higher throughput in this scene |
| Pre-Z meshes in that scene | 1,897 | 297 | 1,600 redundant depth draws removed |
| World opaque meshes in that scene | 2,676 | 2,676 | Colour-list geometry preserved |
| Fixed-scene p99 frame time | 29.61 ms | 25.34 ms | One paired observation |
| Road Rage combat, 90 seconds | 69.71 FPS | 70.17 FPS | No meaningful combat speedup demonstrated |

Both combat runs qualify: eight/seven takedowns, four victims each, multiple
crashing and airborne rivals, 885/885 foreground samples each, no assertions,
exceptions or frames over 50 ms. The fights and visible world mesh counts differ,
so this pair does not isolate a combat gain. The fixed scene also retains moving
traffic; its colour world count and view remain constant. Evidence is under
`scratch/performance_goal_0929/prez_{stationary,combat}_{all,near}_1001` in the
workflow workspace. The visual run is `prez_visual_1001`.

`BRN_FRAME_DETAIL=1` adds opt-in timing for mesh expansion, technique/constants/
buffer binding, full geometry-cache lookup, world/immediate draws and post-FX.
It requires the existing `BRN_FRAME_PROFILE=1`; ordinary traces avoid these
extra per-draw timer reads. Scopes can nest and must not be added together.
Scene-list counts are included in ordinary frame traces. The diagnostic
`renderer_breakdown_1001` identifies submission and native Present waiting as
major costs; the fixed-scene pair still spends about 6.3 ms per frame in Present.
That wait includes queued work and does not by itself establish GPU execution
time. Neither the combat results nor a locked 165 FPS target are solved here.

## Native GPU timing and presentation attribution

`BRN_GPU_PROFILE=1`, together with `BRN_FRAME_PROFILE=1`, records the scene and
output-copy timestamp spans. An eight-packet query ring reads older results with
flags zero, without flushing or waiting. CSV `gpu_status` distinguishes off,
pending, valid, invalid, unavailable and ring-full samples; absent timings are
-1, not zero. These are elapsed GPU timeline spans and can include starvation;
they are not a direct GPU-busy measurement. See the [D3D9 query contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries).

The actual assert-overlay entry hook abandons the current sample, including a
prefix already presented during takeover. Subsequent frames resume normally.
`run_pc_gpu_frame_timing.py` passes 16 checks, including native D3D9 completion,
pixel/state preservation, unavailable/pending/error/disjoint handling and the
production overlay hook. Removing that hook's invalidation fails two checks.
Independent review, the canonical build and the 87 fullscreen/config checks pass.

Focused stationary diagnostic runs (`gpu_timeline_stationary_1001` and
`gpu_timeline_720_1001`) produced 2,073 and 3,215 valid samples respectively:

| Render size | Scene GPU span | Output-copy GPU span | CPU wait in Present |
| --- | ---: | ---: | ---: |
| 1280x720 | 5.70 ms | 0.015 ms | 2.17 ms |
| 2560x1440 | 6.58 ms | 0.042 ms | 6.28 ms |

Final build `2c8cd604aca3` also completed a 90-second Road Rage diagnostic with
eight takedowns across six victims, peak four crashing/two airborne rivals,
885/885 foreground samples and no assertions or exceptions. All 6,416 measured
frames have valid GPU timestamps, including crash/takedown camera changes.
Mean scene/output spans are 5.33/0.046 ms; mean Present wait is 7.37 ms.
The workload qualifies as combat, but GPU-query runs are excluded from clean
FPS comparisons. Evidence: `gpu_timeline_combat_1001`.

The renderer's extra output blit is small; the later native presentation wait
grows much more with resolution. This does not exclude transfer/composition work
inside Present. An opt-in two-buffer DISCARD experiment confirmed two active
output buffers but still waited 6.35 ms; it was removed. It is not a shipped
optimization or a supported configuration option.

A separate synthetic probe on the same RTX adapter compares actual successful
Present calls after a six-millisecond producer interval. All six trials had
100/100 foreground samples and no API errors. At 1440p, legacy COPY averaged
7.83 ms, D3D9Ex COPY 8.55 ms, and D3D9Ex primary FLIPEX 0.52 ms. This is a
presentation-path result, not game FPS evidence. Primary flip presentation is
therefore the next integration candidate; just changing the device to 9Ex did
not help. The [flip-model documentation](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/direct3d-9ex-improvements)
describes handing surfaces to DWM instead of the legacy extra composition copy.
The primary flip path is now integrated and selected by default, with automatic
legacy fallback when the extended API, device or initial output is unavailable.
`BRN_PRESENT_FLIP=0` selects the legacy path explicitly for comparison.

## Primary flip presentation

The D3D9Ex primary chain presents retained scene surfaces through FLIPEX. The
separate retained surface preserves assert overlays and resize fallback. ResetEx
runs only on the creation thread between joined engine frames; failed resize
restores the old output, and an unrecovered failure stops rendering until reset
succeeds. Only successful native presents count toward the presentation counter.

Editable textures use SYSTEMMEM staging paired with DEFAULT GPU storage on 9Ex;
legacy retains MANAGED storage. All mip levels, cube faces, compressed block rows,
GUI alpha, colour-volume slices and runtime edits are preserved. Writable sublevel
edits dirty level zero, including on legacy D3D9. The serialized texture header
is unchanged. Runtime edits publish after the last overlapping CPU lock closes.

The native resource suite passes 25 checks and the presentation suite passes 39,
including full nonuniform scaled-output equivalence against legacy, failure and
rollback paths, retained pixels, F11 and skipped-presentation counters. Removing
uploads fails 10 checks; removing the flip filter policy fails one. The independent
review passes. The canonical final build succeeds with four pre-existing C4661
explicit-template-instantiation warnings in ImRenderBuffer.

Same-binary 45-second stationary runs at 2560x1440, fully foreground, use unchanged
graphics settings and no captures, GPU queries or detailed timers:

| Metric | Legacy | Primary flip |
| --- | ---: | ---: |
| Average FPS | 69.89 | 120.55 |
| Mean native Present wait | 6.38 ms | 0.63 ms |
| 99th-percentile frame time | 26.03 ms | 11.41 ms |
| Maximum frame time | 35.39 ms | 16.01 ms |

This is a 72.5% gain in the fixed scene, not a whole-game claim. Both runs render
2,676 opaque world meshes and 297 pre-Z meshes. Evidence under the parent checkout:
`scratch/performance_goal_0929/flip_pair_{legacy,flip}_static_1001/`.

Live F11, resizing, pause/resume and minimize/restore pass, including odd dimensions
and letterboxing. Captured 1440p combat qualifies with eight takedowns across five
victims and peak five crashing/three airborne rivals, without assertions or
exceptions. Damaged vehicles, HUD and world output were inspected. Both intro
videos were captured on the final default build after delaying the harness's
Accept key. Capture-run FPS is excluded. Earlier 90-second flip combat runs
included the event ending, so their overall FPS is also excluded. These exclusions
must not be converted into a claimed combat speedup or a locked 165 FPS result.

## Opt-in shadow diagnostics

Shadow diagnostic vertex sampling and pixel-count queries are now opt-in through
`BRN_SHADOW_PROBE=1`. Normal cascade rendering, shader/state changes and HUD totals
are unchanged. Existing `BRN_SHADOW_CULL`, `BRN_SHADOW_BIAS`, `BRN_SHADOW_SLOPEBIAS`,
`BRN_SHADOW_FALLBACKVS`, `BRN_SHADOW_ZALWAYS` and `BRN_SHADOW_FORCECWE` experiments
still enable their diagnostic readouts unless `BRN_SHADOW_PROBE=0` overrides them.

Same-binary 45-second foreground stationary 1440p runs measured 115.64 FPS with
the probe enabled and 125.37 FPS with it disabled (+8.4% in this pair). P99 frame
time fell from 12.10 to 10.96 ms. Both render 2,676 opaque world meshes and 297
pre-Z meshes. The enabled run produced 620 shadow reports; the default run
produced none of those reports. Native shadow pixels were inspected in the
separate default capture. Build, bounded independent review and faithfulness pass.
Evidence: `shadow_probe_{on,off}_static_1001`, `shadow_probe_off_visual_1001` under
the parent checkout's performance scratch directory. This does not establish a
Road Rage speedup or locked 165 FPS.

## Native geometry bindings

Native stream-zero and index-buffer bindings now reuse successful bindings.
Whole pooled vertices move into the indexed draw's base where the resulting byte
addresses are identical. Negative/overflowing bases and unusual layouts keep
the prior addressing form. `BRN_GEOMETRY_BIND_CACHE=0` selects the old bindings
and disables rebasing for comparison. Immediate-mode draws invalidate the
bindings they clear, following the [D3D9 UP contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawindexedprimitiveup);
assert restoration, device creation/reset and full geometry release also invalidate.

Native geometry checks pass 46/46, including failed binds, source retirement,
UP draws, real assert state restoration, signed bases, compressed normals and
16/32-bit indices. Instancing passes 18/18 including a rebased pooled source;
the GUI, display and GPU-timing suites pass 95/95, 87/87 and 16/16. Removing UP
invalidation fails four checks; removing assert invalidation produces wrong pixels.

The same-binary 1440p stationary pair skips 27.7% of vertex bindings and 76.9% of
index bindings (about 5,315 API calls per frame), with identical world/pre-Z counts.
Mean geometry submission falls from 2.03 to 1.83 ms. Average FPS is 123.95 versus
126.11; P99 is 10.92 versus 10.91 ms. This small pair is not evidence of another
large FPS gain. Both runs remain fully foreground. Frame CSV fields
`vb_bind_requests`, `vb_bind_skips`, `ib_bind_requests`, `ib_bind_skips` and
`rebased_draws` record actual use. Evidence: `binding_{control,default}_static_1001`.

Live F11/resize/pause/minimize and a 75-second combat/streaming capture pass without
assertions, exceptions or native draw failures. The combat capture has four player
takedowns across three victims, peak four crashing/two airborne rivals and intact
vehicle/world/HUD pixels; it is below the five-takedown benchmark threshold, so
its FPS is excluded. Evidence: `binding_display_live_1001` and
`binding_combat_visual_1001`. Locked 165 FPS and a qualified clean combat comparison
remain unestablished.

## Native shader and declaration bindings

Native vertex shaders, pixel shaders and vertex declarations now share a
successful-binding cache across world, instanced, GUI and debug rendering.
`BRN_SHADER_BIND_CACHE=0` retains the previous native calls for comparison.
Failed binds remain retryable; device creation/reset and assertion state-block
restoration invalidate the cache. FVF selection invalidates the declaration it
replaces. All native writers must use the shared wrappers or invalidate first.
The device owns references to its bound objects; the cache borrows their identity.
Native pixel and state checks are in `run_pc_shader_bindings.py`, including
negative controls for missing FVF and state-block invalidation. The optimization
does not skip draws, shader constants or instanced transforms.

## Mesh data prefetch

The mesh dispatcher now restores ARTIST's two-command lookahead
(`0x827F29A0..0x827F2A8C`): prefetch the packet two entries ahead and the next
mesh's metadata. The host adaptation preserves the dispatch subrange and
64-bit pointers, using 64-byte cache hints for the console's 128-byte spans.
`BRN_MESH_PREFETCH=0` disables it for comparison. The guarded-key and pointer
regression is `python b5-decomp/tests/run_pc_mesh_prefetch.py`.

Repeated same-binary fullscreen 1440p tests measured 153.86/154.55 FPS with the
lookahead disabled and 160.09/161.17 FPS enabled. Both render 2676 world meshes,
297 pre-Z meshes and 84 instances. This is a roughly 4% fixed-scene improvement,
not a whole-game or locked 165 FPS claim. The qualified 90-second pickup Road Rage
run recorded 10 takedowns, six victims and 184.77 FPS average, but its p99 frame time
was 8.79 ms; camera changes and slow frames still require work.

## Object-to-mesh conversion jobs

The original 16-job conversion path is restored, including the four partitions
of world list 11 at 128-command constant-refresh boundaries. Each job owns its
context and output lists, obtains 16 KiB blocks atomically, then flushes its key
chains. The renderer joins every job before reconnecting, merging and reclaiming
unused command memory. Native pointer types replace the old console 32-bit image
accesses, and the formerly unfinished relocation/append functions are recovered
from ARTIST 827EE868/827E9590. Sort keys and visibility policies are preserved.

Enable it with `BRN_MESH_JOBS=1`; `BRN_MESH_JOBS_CHAIN=1` additionally selects the
original serialized dependency chain for comparison. **The native default stays
serial**: two 45-second pairs measured 160.12/154.99 FPS with jobs versus 159.68/156.08
without, while the jobs consumed more total CPU time. That establishes no reliable
FPS improvement. The optional path is restored infrastructure, not a claimed gain.

The enabled 150-second 1440p combat run qualified with 13 takedowns/seven victims,
peak four crashing/four airborne rivals, 1470/1470 focus samples and zero assertions
or exceptions. It averaged 184.79 FPS, p99 8.50 ms, maximum 15.53 ms; 31.60% of frames missed
the 165 FPS budget. Thirty camera changes had a maximum 11.66 ms interval and 13.15 ms
maximum within eight surrounding frames. This is not a matched combat speedup
or a locked 165 FPS result. Captured visual checks are separate from timed runs.

`python b5-decomp/tests/run_pc_object_mesh_jobs.py` checks production allocation,
partitioning, context ownership, walks, merges and completion on actual EAJobs
workers against an independent synthetic packet oracle. All 17 checks pass, as does
`--chain`; `--break-partition` fails five checks and `--drop-final-flush` trips the
reconnection assertion. The synthetic emitter does not certify GPU pixels;
separate live captures exercised damaged cars, airborne rivals, wreck cameras,
shadows, world geometry and HUD rendering. `object_to_mesh_ms` now measures the
owner's whole conversion interval, avoiding shared per-object timer writes.

## Frame-pacing measurements

Packed DEC3N normal expansion now uses a 4 KiB table containing the exact scalar
float result for every signed 10-bit input, including the -512 clamp. Separate
compiled kernels keep the scalar comparison branch outside the vertex loop;
`BRN_GEOMETRY_NORMAL_LUT=0` selects that control. This applies when a new retained
vertex buffer needs expansion on hardware lacking native DEC3N support.

`python b5-decomp/tests/run_pc_packed_normal.py` passes 57 checks in both modes:
all 1,024 values in every channel, unused high bits, varied and unaligned layouts,
multiple packed fields, zero/tail counts, complete record bytes and guard pages.
Removing the clamp fails 28 checks. The native geometry/GPU suite passes 55/55.
Isolated production-kernel measurements for 65,536 vertices were 0.226 versus
1.248 ms for one normal per 16-byte source record, 0.363 versus 2.262 ms for two
per 32-byte record, and 0.636 versus 4.403 ms for four per 48-byte record. These
are conversion timings, not a whole-game FPS multiplier.

The separate clean 150-second 1440p Road Rage run exercised 17 takedowns across
seven rivals, with up to four crashing and two airborne together. All 1,473 focus
samples passed, with no assertions, exceptions or event end. It averaged 188.64
FPS, P99 8.36 ms and maximum 14.49 ms; 26.90% of frames exceeded the 6.06 ms budget.
Thirty-five camera transitions had a maximum 8.84 ms interval and 14.49 ms nearby.
Different combat routes prevent claiming a matched overall FPS gain. Upload and
resource-pool stalls remain, and locked 165 FPS is still unproven.

For frame-pacing measurements, use `BRN_FRAME_PROFILE=1` with
`BRN_FRAME_TIMING_ONLY=1`. This records two clock reads per frame while retaining
draw counts, successful presents, camera transitions and combat qualification
counters. Every section timer is disabled, including those normally active
without `BRN_FRAME_DETAIL`; section-time CSV columns are therefore zero. The
metadata marks `timing_only: true`. Leave this option off for CPU attribution.
GPU timestamp queries and screenshots are separate diagnostics and still add
overhead if enabled. `BRN_FRAME_PROFILE=0` keeps all frame recording disabled.
The recorder regression is `python b5-decomp/tests/run_pc_frame_timing_only.py`.

For attribution without per-draw clocks, use `BRN_FRAME_PROFILE=1` with
`BRN_FRAME_COARSE=1`. The CSV records consecutive renderer stages: setup,
dispatch-list building, tint scheduling, shadow maps, environment maps, particle
preparation, world/car drawing, particles/coronas, composite, GUI and presentation.
`render_composite_ms` includes the scene resolve, sun-corona work and quarter-size
particle rendering as well as the final composite. `dispatch_effects_ms` covers
particle dispatch before the renderer. Together these stages partition the render
thread's dispatch interval, apart from the small entry/exit instrumentation gaps.
Existing aggregate timers may overlap these stages and must not be added to them.

Coarse mode suppresses per-draw timers even if `BRN_FRAME_DETAIL=1` is also set;
unmeasured columns remain zero. Geometry preparation/lock/conversion/unlock timers
remain available for cold cache entries; cached draws incur none of those clocks.
Those cold-entry timings overlap the render stages. `BRN_FRAME_TIMING_ONLY=1` takes precedence and
still records exactly two endpoint clocks per frame. Metadata records `coarse`
separately from `timing_only`. Coarse runs diagnose expensive stages; use a
separate timing-only run for FPS and frame-pacing claims.

To investigate sparse upload/resource stalls, add `BRN_FRAME_CPU_CYCLES=1`.
Native geometry creation, pooled buffer locks, pooled upload copies and resource
pool updates then record raw thread CPU cycles alongside elapsed time. Creation
and pooled copy now also have separate `geometry_create_ms` and
`geometry_copy_ms` columns. These spans overlap the existing geometry/resource
timers; they are not additional frame costs. Individual-buffer fallback locks
and copies are not covered by the new pooled lock/copy probes.

The recorder dynamically resolves `QueryThreadCycleTime` only when requested.
Metadata reports `cpu_cycles` and `cpu_cycles_available`; failed samples increment
`cycle_read_failures` and produce no cycle delta. Timing-only and disabled capture
perform no cycle queries. Raw cycles must not be converted to milliseconds or
clock frequency. Compare them with elapsed time to investigate CPU work versus
waiting or descheduling, without treating this alone as proof of a particular
GPU or scheduler cause. These captures are diagnostic, not clean FPS evidence.

The recorder suite passes 26 checks, including missing APIs, failed reads,
independent elapsed/cycle clocks, CSV alignment and timing-only precedence.
Removing cycle accumulation fails two checks; retaining section timers in
timing-only mode fails three. The native geometry suite still passes 55 checks.

## Whole-vertex geometry alignment

Pooled vertex allocations now satisfy both 16-byte alignment and the expanded
vertex stride. The existing indexed-draw rebasing can therefore keep stream zero
at byte offset zero across compatible meshes. This preserves every vertex address
while avoiding native stream updates. Requested alignments need not be powers of
two; reusable prefix/suffix ranges and GPU-fenced retirement remain intact.
`BRN_GEOMETRY_ALIGN_STRIDE=0` selects the previous allocation policy.

A native D3D9Ex probe with 5,120 indexed draws per frame produced identical full
images across both policies. For strides 20/36, stable stream offsets reduced CPU
submission from about 1.3 ms to 0.3 ms. Already-aligned 32/48-byte controls were
neutral. This is a submission benchmark, not a whole-game multiplier.

The same executable was measured off/on/on/off in a stationary 1440p scene,
45 seconds per run, with full foreground coverage and timing-only recording:

| Pair | Previous policy FPS | Aligned policy FPS | Native vertex binds/frame, previous → aligned |
| --- | ---: | ---: | ---: |
| First | 161.41 | 170.93 | 3,620 → 3,037 |
| Reversed | 164.19 | 166.90 | 3,624 → 2,926 |

The observed gain is 1.7–5.9%; native vertex bindings fall 16–19%. Each run records
about 5,078 draws per frame, with 297 pre-Z meshes and 84 instances. All
four finish with 28 MiB of native geometry pages, so these scenes show no increase
in page residency. Frames outside the 165 FPS budget fall from 51.2% to 30.3% and
47.5% to 38.5%, respectively. P99 improves in the first pair but is slightly worse
in the second; this does not establish a universal tail-latency improvement.

Allocator regression: 334 checks pass, including fragmented mixed-stride reuse,
alignment overflow, failed uploads, live payload integrity and pending GPU fences.
Ignoring alignment fails six checks. Native geometry passes 79 checks; the old
policy passes its 75-check pixel control. Canonical build and independent review
pass. The captured 120-second Road Rage run qualifies with eight takedowns, up to
four crashing/two airborne rivals, no assertions/exceptions, and inspected driving,
crash-camera and damaged-car frames.

A separate clean 150-second Road Rage run qualifies with 13 takedowns across all
seven rivals and three crashing/three airborne together. It averages 170.34 FPS,
P99 8.93 ms, maximum 16.78 ms; 42.66% of frames miss the 165 FPS budget. The 29
camera transitions reach 8.41 ms on the cut and 12.34 ms nearby. This route has
about 24% more draws/frame than the preceding 188.64 FPS combat run, so those
averages are not a matched performance comparison. Allocation/resource stalls and
locked 165 FPS remain unresolved. Evidence: `stride_static_comparison_1001.json`,
`stride_alignment_validation.md` and the six `stride_*_1001` runs under the
parent checkout's `scratch/performance_goal_0929/`.

## Larger retained-geometry pages

Native geometry pages now default to 8 MiB. This reduces native resource creation
and buffer switches while preserving the existing alignment, upload, fallback and
GPU-fenced retirement paths. The startup control `BRN_GEOMETRY_PAGE_MB` accepts
exact values `1`, `2`, `4` or `8`; `1` selects the preceding policy. These are
measured PC allocation choices, not sizes recovered from the console.

Same-executable, full-foreground 1440p stationary runs, 45 seconds each:

| Page size | Observed FPS | Native geometry residency | Vertex / index binds per frame |
| --- | ---: | ---: | ---: |
| 1 MiB | 168.44–169.52 | 27–28 MiB | 2,940–3,027 / 1,107–1,153 |
| 4 MiB | 175.76–177.79 | 32 MiB | 2,381–2,406 / 4 |
| 8 MiB | 179.19–179.54 | 40 MiB | 1,844–1,918 / 4 |

The 1/4/4/1 comparison improves FPS by about 4.3% in both pairs. Subsequent 8/4/8
runs show a smaller additional gain. All runs record about 5,078 draws per frame.
Eight MiB costs more residency; P99 remains variable, and 15–17% of frames in
these 8 MiB runs still exceed the 165 FPS budget.

The two instrumented camera/streaming runs pass the existing seven checks:
five area transitions, declaration retirement, crash-camera entry and returns
to driving, with no assertions, stale formats or exceptions. With roughly
63–65 MiB uploaded during their measured windows, new native buffer creations
fall from 26 to 3. Ending native geometry residency is 48 versus 56 MiB.
Driving/crash captures were inspected. These captured runs diagnose allocation
frequency; their FPS is not clean performance evidence.

The final default also passes a clean 90-second 1440p Road Rage measurement:
885/885 foreground samples, seven takedowns across five rivals, up to four
crashing/two airborne together, and no assertions, exceptions or event end.
It averages 186.06 FPS, P99 7.88 ms and maximum 16.06 ms. The 18 camera cuts reach
8.67 ms on the cut and 16.06 ms nearby. Of 16,738 frames, 3,655 (21.84%) exceed
the 165 FPS budget. Different combat routes prevent using this as a matched
before/after gain. Two earlier combat runs passed their workload requirements
but lost focus briefly; their FPS results were rejected.

The GPU suite passes 91 checks at 1, 4 and 8 MiB and with the final default. It
includes real draws beyond 3 MiB in both vertex and index buffers, using both
index widths. Truncating the native index offset to 16 bits fails four checks,
including both rendered pixels. The canonical build and independent review pass.
Evidence: `page_capacity_validation_1001.md`, `page_capacity_static_comparison_1001.json`,
`page_camera_comparison_1001.json` and the `page_*_1001` runs in the parent
checkout's `scratch/performance_goal_0929/`.

This change reduces geometry allocation frequency. It does not remove all
driver waits, streamed-texture creation costs or CPU resource-retirement spikes.

## Source-address lookup during geometry retirement

The CPU eviction index now groups source addresses into 4 KiB buckets. Its
previous 64 KiB buckets repeatedly scanned unrelated small resource headers as
individual resources were freed. Registration still covers every touched bucket,
and the existing exact byte-range check decides which buffers retire. GPU page
capacity, contents, retirement fences and draw-cache invalidation are unchanged.
`BRN_GEOMETRY_SOURCE_PAGE_KB=16` or `64` selects the comparison policies.

An isolated production-code probe retires 4,096 resource pairs in sequential and
shuffled order. For 512-byte and 4 KiB source blocks, retirement drops from
32–45 ms to 2–3.6 ms. For 32 KiB blocks it drops from 30–38 ms to 6–8 ms, but
registration rises from about 1.7 ms to 4.9 ms. The latter fixture also raises the
cost of scanning unrelated 64 MiB free ranges from about 0.02 ms to 0.65 ms. Finer
lookup tables have a construction and memory cost; this is not an FPS multiplier.

Both live five-area/crash-camera runs pass all seven streaming checks. The new
lookup processes one 6,026-buffer retirement batch in 3.13 ms of resource-update
time; the old lookup takes 4.2–5.2 ms for batches of roughly 3,800–4,200 buffers.
Exact loads differ, and resource updates include other work, so these are useful
runtime observations rather than a matched whole-game speedup. The candidate
still has 8–9 ms resource-update frames with zero geometry retirements.

The new interval suite passes 23 checks at all three bucket sizes, covering
both owner maps, separate header/data notifications, neighbours, exclusive ends,
cross-bucket sources and reused addresses. Dropping the last source bucket fails
eight checks. Native rendering passes 91 checks. The canonical build and
independent review pass. A separate diagnostic Road Rage run qualifies with 14
takedowns across six rivals, up to four crashing/two airborne, and no assertions,
exceptions or event end. It is not a clean FPS measurement.

Evidence: `retirement_buckets_validation_1001.md`,
`retirement_streaming_comparison_1001.json`, and the `retirement_*_1001` runs in
the parent checkout's `scratch/performance_goal_0929/`.

## Resource spikes: native texture allocation

Coarse tracing now separates pool request processing, allocation state updates,
and per-pool updates. It also measures texture realization, GPU and SYSTEMMEM
creation, and uploads. `BRN_FRAME_CPU_CYCLES=1` records raw thread cycles for
these spans. Timing-only and disabled profiling still suppress their clocks.
Runtime texture edits also upload outside resource updates, so upload totals
must not be treated as a strict subset of realization totals.

A five-area diagnostic run passes all seven streaming/camera checks. Its two
largest resource frames take 14.35 and 12.86 ms; texture realization accounts
for 13.53 and 12.08 ms. In the first, native GPU creation takes 5.92 ms,
SYSTEMMEM creation 6.69 ms, and upload 0.15 ms. Realization consumes 30.33
million raw thread cycles. Both frames have zero geometry evictions. This
identifies substantial texture-creation CPU work in addition to the previously
observed driver wait; it is not a clean FPS measurement.

The initial staging-reuse experiment recorded zero hits during both streaming
runs and was excluded at that stage. Investigation found that normal pool retirement
frees raster backing bytes without releasing the separately-created native D3D
texture. FixDown can release it, but is not called on that path. Native raster
relocation also needs review before a lifetime fix: the inherited ReBase passes
address deltas to a PC FixUp that expects absolute pixel data. The lifetime repair described below addresses these defects; the diagnostic
change itself did not fix them.

Recorder tests pass 34 checks; dropping cycle deltas fails nine, including all
seven new columns. Native texture tests pass 25, and the canonical build,
faithfulness gate and bounded independent review pass. Evidence:
`resource_attribution_validation_1001.md` and `resource_attribution_streaming_1001`
in the parent checkout's `scratch/performance_goal_0929/`.

## Native texture lifetime during streaming

The pool now releases each realized raster's native D3D reference before its
heap bytes are freed or replaced. Ownership is tracked by the resource entry,
which remains stable when headers move. Unfixed serialized data is not treated
as a COM pointer. Explicit texture destruction removes registration so a later
pool free cannot release it twice.

Native raster relocation preserves the D3D texture and its editable CPU shadow.
It no longer sends memory-lane deltas into the absolute-data upload path.
Pool retirement also restores ARTIST's two alias-propagation calls, clearing
retained resource pointers after the original identity becomes empty.

The native lifetime suite passes 48 checks across D3D9 and 9Ex, including GPU
mip pixels, partial and unfixed loads, header/pixel relocation, reused addresses,
live replacement, aliases and external COM references. Omitting retirement
fails 16 checks; omitting propagation fails two. General resource allocation
passes 59, actual relocation/jobs 30, existing native textures 25 and the
recorder 35. Build, faithfulness and independent review pass. The build has
eight existing C4661 Im2d/ImRenderBuffer template-instantiation warnings.

The captured five-area streaming run passes all seven checks and records 1,220
new raster owners and 1,296 native-reference releases in its measured window.
Crash and driving images retain world/car textures and HUD. A separate captured
Road Rage run qualifies with nine takedowns across five rivals, up to four
crashing and six airborne, and no assertions, exceptions or event end.
These instrumented captures are not clean FPS measurements.

Texture creation still produces resource spikes: one 139-texture batch takes
9.51 ms to realize. The lifetime-only build excluded staging reuse pending a fresh comparison.
The reuse change below follows that repair. Fixing ownership removes a lifetime leak;
it does not establish locked 165 FPS. Evidence: `texture_lifetime_validation_1001.md`
and `texture_lifetime_*_1001` runs in the parent checkout's
`scratch/performance_goal_0929/`.

## Reusing retired CPU texture storage

D3D9Ex texture creation now reuses compatible retired SYSTEMMEM upload textures.
Live textures keep exclusive editable storage; GPU texture objects are not reused.
Keys include device, type, format, dimensions and resolved mip count. Dirty,
locked, failed or incomplete uploads are discarded normally. The cache is
bounded to 128 objects and 16 MiB of estimated padded pixel storage; driver
metadata is additional. `BRN_TEXTURE_STAGING_CACHE=0` selects the old path.

In a controlled native test of warmed 64-texture batches, full creation/copy/
upload time falls from 1.76 to 0.64 ms at 64×64, 3.70 to 1.25 ms at 256×256,
and 6.55 to 2.84 ms at 512×512. Those are isolated allocation measurements.

Both live five-area runs process 734 raster creations and pass all seven
streaming/camera checks. Reuse avoids 332 CPU staging allocations (45%). CPU
creation/lookup time totals 22.19 versus 13.64 ms; total texture realization
60.64 versus 51.02 ms. Raw thread cycles also fall. Their rendered workloads
and batch timing differ substantially, so overall FPS is not comparable.

A captured 120-second Road Rage run qualifies with six takedowns across four
rivals, up to four crashing/three airborne, and no assertions, exceptions or
event end. It reuses storage for 455 of 517 textures. Driving/crash images retain
textures and UI. This is diagnostic coverage, not a clean FPS benchmark.

Native pixel tests pass 34 checks with reuse on and off, ownership/bounds tests
22, resource lifetime 48, and recorder 36. Removing format matching fails two
checks; caching failed uploads fails two. Build, faithfulness and independent
review pass, with eight existing C4661 build warnings. Evidence:
`staging_reuse_validation_1001.md`, `native_texture_staging_summary_1001.json`,
`staging_fixed_comparison_1001.json` and `staging_reuse_combat_1001` under the
parent checkout's `scratch/performance_goal_0929/`.

GPU allocation, draw submission and other frame spikes remain. This change
reduces texture setup work; it does not establish a locked 165 FPS.

## Remaining original optimization gaps

- Frame overlap is active, but the measured dispatch/presentation path still
  dominates the frame; broader combat and streaming coverage remains useful.
- Native wheel/mesh instancing is active. Further gains need measured reductions
  in the much larger remaining draw-submission and presentation costs.
- Native occlusion queries/conditional rendering remain incomplete. ARTIST's
  master switch actually starts disabled (`827F1E18/827F1E58` store zero at
  manager+0x331), despite enabled world-pass options. The renderer tests both;
  its debug menu exposes the master switch. An ordinary-play activation has not
  been established, so this must not be counted as a proven missing active
  optimization. Any native implementation must preserve visibility without GPU
  waits or stale results after camera cuts.
- Primary flip removes most of the measured presentation wait. Remaining work
  includes draw submission and resource-update spikes under combat and streaming.
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

## Mesh preparation during update (2026-10-03)

The native renderer now expands and sorts the completed update-side object lists
while the render thread submits the previous frame. This is a PC scheduling
adaptation: original conversion, partition boundaries, constant inheritance,
pass routing and sorting are unchanged. A second 12 MiB output bin and a separate
producer interpreter keep the two frames independent. Physical frames never move;
ownership flips with the matching GDL and shader frame, keeping constant pointers
valid.

Resource fixup, post-fixup, import writes, retirement, live replacement and
relocation advance a native generation. Changes to resources or pre-Z controls
force a joined rebuild before publication. Cold and empty frames also produce
valid output. Update-side worker joins service queued assertions and Windows
messages, preserving WM_QUIT. Render-side waits retain their previous behavior.

Preparation during update is enabled by default. `BRN_MESH_PREPARE=0` selects the
previous render-side schedule; `BRN_MESH_JOBS=1` remains an independent opt-in.
The profiler records `update_mesh_prepare_ms`, `mesh_prepared` and `mesh_rebuilt`.
Timing-only mode adds no section clocks.

At the maximum settings captured on October 3 (1440p, 8x MSAA, full-rate
reflections, reflection LOD0, world/prop LOD bases of 3000, Ultra vehicles and
traffic shadows), four 45-second runs of the same executable measured:

| Order | Preparation | Average FPS | 99th percentile frame time |
| --- | --- | ---: | ---: |
| 1 | During rendering | 138.42 | 10.85 ms |
| 2 | During update | 155.80 | 9.63 ms |
| 3 | During update | 158.47 | 9.05 ms |
| 4 | During rendering | 139.58 | 10.14 ms |

All samples were foreground. VSync was disabled only for this diagnostic
comparison. World opaque work remained 3926 meshes, and total draws remained
8452-8490. Mean throughput improved about 13% in this stationary workload. A later
run of the previously published executable measured 143.81 FPS, illustrating
run-to-run variation. A lighter-settings control measured 217.73 FPS without
overlap and 241.17 FPS with it, using the same executable for both 35-second runs.

A clean 120-second Road Rage run at the exact maximum target, including VSync,
qualified with eight takedowns, four distinct victims, up to four rivals crashing
and airborne, and 2758 frames with multiple crashing rivals. All 1181 focus
samples were foreground; no assertions, exceptions or event end occurred.
It averaged 148.95 FPS, with a 10.68 ms 99th percentile and a 15.71 ms maximum.
This establishes a clean combat measurement at these settings, not a combat
speedup or a locked 165 FPS.

Native regressions cover 128 concurrent frame publications, constant lifetimes,
physical-bin reuse, resource/control invalidation, empty/disabled paths, actual
conversion on an explicit input bank, and a real EAJobs worker that asserts and
sends synchronous window messages. Compiled negative controls detect stale
resource generations, wrong-bank output, omitted assertion service and omitted
message pumping.

```powershell
python b5-decomp/tests/run_pc_mesh_preparation.py
python b5-decomp/tests/run_pc_mesh_job_owner_wait.py
python b5-decomp/tests/run_pc_object_mesh_jobs.py --write-bank
```

## NVIDIA depth resource retirement (2026-10-03)

The old NVAPI depth-resolve cache retained eight raw resource addresses without
unregistering them. Target replacement freed those objects, so address reuse
could falsely inherit registration. Registrations beyond eight succeeded without
being tracked and were repeated every frame.

The native registry now tracks every successful registration and holds a COM
reference until paired unregistration succeeds. This includes the source depth
surface, destination texture and its level-zero surface, even when a resolve
variant is unused. Failed registration takes no reference. Failed unregistration
retains the object and blocks resize/reset until retirement can be retried.
Joined target replacement and ResetEx retire registrations before releasing
owners. No per-frame GPU wait or graphics reduction is introduced.

The regression passes 20 checks, including 17 simultaneous resource lifetimes,
address reuse, duplicate suppression and failure handling. Real NVIDIA depth
resolves preserve sampled GPU pixels across twelve generations of 8x MSAA
surfaces, with unregistration and ResetEx occurring before readback. Restoring
untracked eight-entry overflow fails six contract checks before GPU execution.

```powershell
python b5-decomp/tests/run_pc_nvapi_resource_registry.py
```

The combined build passed repeated F11 transitions, six window-size requests,
minimize/restore and pause-menu rendering. The OS constrained oversized requests;
actual captured sizes ranged from 1024x576 to 2564x1442. A separate streaming run
passed all seven existing checks: five area transitions, twelve declaration
retirements, crash-camera entry and four returns to driving, without assertions,
exceptions or stale vertex formats. Captured world, car, reflection, shadow and
HUD images were inspected.

The registration defect was found while investigating one intermittent driver
crash during resize. Subsequent unmodified controls also passed, so the exact
crash cause remains unproved. The lifetime defect and its repair are independently
validated. Evidence is retained in the workflow's
`scratch/performance_max_1003` directory. The original maximum settings remain
the acceptance target; locked 165 FPS is still unproved.
