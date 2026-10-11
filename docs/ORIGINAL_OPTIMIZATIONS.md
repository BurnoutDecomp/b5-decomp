# Original game optimization reconstruction

Tracking issue: [#41 - Reconstruct and verify all original game optimizations](https://github.com/BurnoutDecomp/b5-decomp/issues/41).

This is the canonical backlog for reconstructing and verifying the original
game's optimization mechanisms on PC. The tracking issue stays open until the
known gaps **and the remaining coverage audit** are complete. Finishing this
document's initial list is not, by itself, evidence that every original
optimization has been recovered.

This work preserves rendering quality, simulation, effect ordering and damage
visibility. A missing original mechanism is not resolved by reducing its workload
or by showing that an unrelated change improves FPS.

Measured PC performance, previous implementations and their retained validation
are recorded in [PC rendering performance](../tests/PCPerformance.md).

## Audit baseline and evidence

The initial audit was read only on 2026-10-11: source inspection, original export
inspection and disassembly of the existing PC executable. It did not build the
game, run gameplay, modify assets or measure a new speedup.

| Input | Audited identity |
| --- | --- |
| Behavioral authority | `BURNOUT_X360_ARTIST.XEX`; qualified names identify functions, addresses below are local to this build |
| Declaration authority | DecFIGS DWARF, gated on ARTIST attestation |
| PC platform reference | TUB/BPR selectively; their policies do not replace ARTIST game behavior |
| Workflow checkout | `a37f665c32da05196be9541b9a0bb46a48efb9ee` |
| Game source audited | `82a3637386cc8cde40ea4d1698367ff031c7ea6e`; affected core paths rechecked through `691e56ecd2627bd18ed484d9f355159c583aebd9` |
| PC executable SHA-256 | `7f82cfd0c5b04b23368df00ee1b8b47e3b19fc672605eeeadf08f2fa225051fb` |
| Executable provenance | `b7f8da1a9c5cb908bb6d6fab678dff251f14fcb8`, dirty build, 2026-10-11; not a claim that the later clean source commit produced this executable |

Original evidence is available locally in the workflow checkout under
`.ida-exports/BURNOUT_X360_ARTIST.XEX/0x<ADDRESS>.json` (pseudocode and assembly).
Those private reference artifacts are not published by this document. Source
links below point to the reconstructed implementation; addresses and names make
the original evidence independently locatable.

The scheduling inventory follows all **21 distinct indexed caller functions** of
ARTIST `EA::Jobs::JobScheduler::AddJobs` (`82BCB498`) and `AddTree` (`82BCB540`).
This is not a count of individual call instructions or all possible indirect
submission routes. Four caller families are restored; seventeen indexed caller
functions still use inline execution or a different blocking host path.

The SIMD screen selected 1,716 ARTIST functions with at least 16 vector
instructions. Exact full linker-map names matched 1,055; 661 lacked an exact
native match. A preliminary binary-map screen flagged 398 matched functions with
substantial scalar arithmetic and no direct packed float arithmetic. These are
**candidates, not 398 confirmed omissions**: calls into an existing SIMD helper,
inlining, folded symbols, copy-only vectors and replacement codecs can explain
differences. In particular, the swept-sphere entry can delegate to its restored
SIMD helper despite containing scalar fallback code.

Stored workflow audit reports targeted an older source/build snapshot. A ledger
status, `NO_BODY` result or missing symbol is not a substitute for inspecting the
current definition and its active caller chain.

## Completion contract

Close the tracking issue only after all of these conditions are satisfied:

- [ ] J01-J09 are reconstructed and verified, including their situational workloads.
- [ ] V01-V07 have recovered the original vector work or a verified native
  implementation of that work; changing a common header alone is insufficient.
- [ ] C01-C05 have complete implementations and evidence for their original
  guards/defaults. Original-disabled paths may remain disabled by default, but
  unfinished bodies do not count as reconstructed.
- [ ] The coverage work in G01-G04 is complete, with no unexplained original
  optimization candidates or open implementation gaps.
- [ ] Previously restored mechanisms in A01-A04 remain verified, and each newly
  recovered mechanism records its source commit, original evidence, native
  implementation, appropriate correctness checks and runtime witness.
- [ ] Final regression evidence uses an attributable build and preserves the
  requested graphics, simulation, effects, save behavior and damage visibility.

An original path genuinely absent from ARTIST, an original trap rather than an
optimization, or an architecture-specific operation supplied by a verified host
equivalent may receive an evidence-backed disposition. A silent stub, unknown
activation, lack of a measured speedup or convenience deferral is not such a
disposition. Any intentional behavior exception must identify the maintainer's
existing requirement; this tracker does not authorize new exceptions.

Restoration and default policy are distinct. A fully implemented and verified
worker path can remain opt-in when native comparisons favor the serial control.
An inline-only substitute for an original worker path remains unfinished.

The separate maximum-settings 165 FPS target is not this issue's closure test.
Reaching it cannot close missing reconstruction, and reconstruction alone does
not prove that performance target.

## Missing original scheduling

All rows below are open at the audit baseline. The work is execution placement
and overlap, not a request to invent different game algorithms.

### J01 Collision and ray-query scheduling

ARTIST submits individual batches at `82814080`, `82814138`, `828141D8`, and
dependency trees at `82810E80`, `82811198`, `82811550`, `828118A8`, `82811C00` and
`82811F58`. These cover line/triangle, line/polygon-soup, sphere/triangle,
swept-sphere/triangle, sphere/sphere and primitive collision streams.

The current [batch dispatchers](../src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp#L303),
[contact streams](../src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator_CollideStreams.cpp#L303)
and [line streams](../src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator_LineStream.cpp#L194)
call `ContactGeneratorEntry` or `PolygonSoupTesterEntry` inline. Callers include
vehicle and detached-part contacts, prop/world contacts and scene line queries.

Recover submission, dependency ownership, independent worker scratch and real
completion before results or batch storage are consumed/reused. Preserve command
claims, result ordering, tags, capacities and empty-stream behavior. Verify both
contact-producing and no-contact workloads, with more than one worker active.

### J02 Triangle-cache fill scheduling

`BaseCollisionGenerator::RunFillTriangleCacheStream` (`82810D38`) submits a parent
tree at `82810E6C`, with up to three fill batches. The current
[dispatcher](../src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator_LineStream.cpp#L305)
runs each entry inline. The later
[triangle-cache wait](../src/GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager_Update.cpp#L375)
therefore provides no overlap.

Recover concurrent fill and its existing join boundary. Prove dirty/cache-reuse
behavior, shared-stream consumption, per-worker polygon-soup scratch, cache-slot
lifetime and identical published triangle lists during movement and retirement.

### J03 Frustum-query jobs

`LooseOctree::StartFrustumTestJobs` (`828B23E0`) submits the original four job
slots; `WaitForFrustumTestJobResults` (`828B2558`) joins them. The current
[start/wait implementation](../src/GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.cpp#L1558)
walks all requested queries inline and later drains already-completed buffers.

Recover owned native job data and actual workers/joins. Preserve query grouping,
entity masks, buffer bounds, tree snapshot lifetime and output batch order.
Validate visibility across ordinary driving, multiple cameras and camera cuts.

### J04 Spark and simple-particle geometry jobs

`ParticleModule::BeginParticleRenderJob` (`8228A7C0`) submits two jobs at
`8228ABF8`/`8228AC0C`; the original renderer consumes them after a wait
(`WaitOnParticleRenderJob`, `8227A320`). The current
[producer](../src/GameSource/Effects/Particles/ParticleModule.cpp#L1248)
builds spark and simple-particle geometry inline. The module still documents
[unfinished job records](../src/GameSource/Effects/Particles/ParticleModule_Lifecycle.cpp#L298).

Recover real job/data records, per-frame snapshots, lock/unlock ownership and
completion before drawing or flipping a bank. Preserve RNG order, spark history,
pre-/post-Lion ordering and crash-bank rules. Use visible spark, smoke/glass and
camera-cut witnesses; successful submission alone is insufficient.

### J05 Debris simulation jobs

`BeginSimulateDebris` (`82289A98`) submits up to five jobs at `82289E60`;
`EndSimulateDebris` (`8227A1F0`) joins them. The current
[simulation loop](../src/GameSource/Effects/Particles/ParticleModule.cpp#L1982)
executes every debris update inline and the end method only clears the count.

Recover submission and real completion with stable debris-bank and triangle-cache
lifetimes. Preserve per-job RNG snapshots, timestep accumulation, expiry and
crash-mode collision rules. Verify simultaneous arrays, motion/spin, contacts and
bank retirement without changing debris visibility or simulation rate.

### J06 Tint-blend job overlap

`BrnPostFx::BeginTintBlend` (`823F8380`) submits at `823F8478`. The current
[method](../src/GameSource/Graphics/PostFx/BrnPostFx.cpp#L1152) calls `TintBlendEntry`
directly. The SIMD blend kernel is already recovered; scheduling remains missing.

Recover asynchronous execution over owned source/weight snapshots and the locked
destination, with completion before tint texture consumption/unlock/reuse. Compare
complete output and publication across source-count changes and effect teardown.

### J07 Resource relocation jobs

`CgsMemory::Relocator::Execute` (`828687D8`) submits at `82868938`; the native
[implementation](../src/GameShared/Jobs/Relocator/CgsRelocator.cpp#L36) copies inline.
Callers include emergency pool defragmentation and replay GPU-disk staging. This is
a situational path, not a demonstrated cost in every frame.

Recover asynchronous execution and correct blocking/nonblocking `Update` behavior.
Preserve op-list, source/destination and bounce-buffer lifetimes, resource
publication and overlapping-copy correctness. Retain the existing repair for
large overlapping copies; reintroducing ARTIST's destructive bounce-copy behavior
is not an optimization restoration.

### J08 Network image compression and decode jobs

`NetworkTextureDXTCompress::Update` (`8287ECC0`, including its decode tail) submits
both DXT lanes. The current [implementation](../src/GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.cpp#L235)
executes both entries on the caller. Scope is network images, camera/gamer-picture
workflows and their callbacks, not an assumed ordinary-driving improvement.

Recover worker execution while preserving ping-pong buffer ownership, lane
priority, callback order and exactly-once completion. Join outstanding work before
release/destruction and validate the actual supported image workflow.

### J09 Movie decode overlap

`MoviePlayer::StartDecodes` (`827EA5F8`) uses `VP6_DecodeFrameToYUV_JOB`
(`82B58350`), whose submission at `82B584E8` starts decoder jobs. The original
start/wait code remains in [the chunk path](../src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer_wBT_01.cpp#L100),
but the active [FFmpeg playback path](../src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.cpp#L554)
performs decode work inside `Update`.

Recover asynchronous preparation/completion in the supported native codec path.
FFmpeg is a platform provider, so copying Xenon codec internals is not required.
Preserve strip/stream decoder independence, PTS, texture publication, audio sync,
pause/skip/EOF and shutdown. Internal codec threading alone does not prove that
the caller's blocking read/decode path has been removed.

### Shared scheduling prerequisites

The native [scheduler already exists](../src/GameShared/GameClasses/System/PC/CgsHardwareInitPC.cpp#L494):
three workers, 128 slots and the original 400 KiB arena. Comments saying it is
missing must not be used to justify new inline-only substitutes.

However, [ContactGeneratorEntry](../src/GameShared/Jobs/ContactGenerator/ContactGenerator.cpp#L73),
[PolygonSoupTesterEntry](../src/GameShared/Jobs/PolygonSoupTester/PolygonSoupTester.cpp#L46)
and [RelocatorEntry](../src/GameShared/Jobs/Relocator/Relocator.cpp#L21) hardcode worker
context zero. Wire correct worker-private contexts before parallel submission.
Also verify native descriptor sizes, shared command streams, frame/resource
ownership, failure/shutdown joins, assertion/message servicing and worker-safe
profiling. Adding `AddJobs` without these prerequisites does not finish a row.

### Indexed scheduler caller inventory

This records the complete direct caller set used for the initial scheduling
audit. An implemented opt-in path is distinguished from an inline-only substitute.

| ARTIST address | Caller | Tracker | Baseline |
| --- | --- | --- | --- |
| `82289A98` | `ParticleModule::BeginSimulateDebris` | J05 | Inline |
| `8228A7C0` | `ParticleModule::BeginParticleRenderJob` | J04 | Inline |
| `823F5898` | `BrnRendererModule::ConvertObjectsToMeshes` | A02 | Restored, opt-in |
| `823F5F70` | `BrnRendererModule::SortDispatchLists` | A02 | Restored, opt-in |
| `823F8380` | `BrnPostFx::BeginTintBlend` | J06 | Inline |
| `82752CB0` | `TrafficJobStub::Execute` | A02 | Restored, opt-in |
| `82810D38` | `BaseCollisionGenerator::RunFillTriangleCacheStream` | J02 | Inline |
| `82810E80` | `BaseCollisionGenerator::RunLineWithTriangleListStream` | J01 | Inline |
| `82811198` | `BaseCollisionGenerator::RunCollideLineAgainstPolySoupStream` | J01 | Inline |
| `82811550` | `BaseCollisionGenerator::RunCollideSphereListWithTriangleListStream` | J01 | Inline |
| `828118A8` | `BaseCollisionGenerator::RunCollideSweptSphereListWithTriangleListStream` | J01 | Inline |
| `82811C00` | `BaseCollisionGenerator::RunCollideSphereListWithSphereListStream` | J01 | Inline |
| `82811F58` | `BaseCollisionGenerator::RunCollidePrimitiveListWithTriangleListStream` | J01 | Inline |
| `82814080` | `BaseCollisionGenerator::CollideSweptSphereListWithTriangleList` | J01 | Inline |
| `82814138` | `BaseCollisionGenerator::CollidePrimitivePairList` | J01 | Inline |
| `828141D8` | `BaseCollisionGenerator::CollidePrimitiveListAgainstTriangleList` | J01 | Inline |
| `828687D8` | `Relocator::Execute` | J07 | Inline |
| `8287ECC0` | `NetworkTextureDXTCompress::Update` (both lanes) | J08 | Inline |
| `828B23E0` | `LooseOctree::StartFrustumTestJobs` | J03 | Inline |
| `828DB2C0` | `DecompressionJobInterface::RunFlushJobs` | A03 | Restored, active |
| `82B58350` | `VP6_DecodeFrameToYUV_JOB` | J09 | Active host playback uses blocking decode preparation |

## Missing vector execution

These rows describe remaining SIMD reconstruction, not permission to change
floating-point behavior. Preserve lane assignment, masks, accepted boundaries,
NaNs/signed zero where relevant, output order and the chosen arithmetic contract.
Compiler-generated SIMD is acceptable when native evidence establishes it.

### V01 Packed triangle collision and rejection

ARTIST `ExecuteLineWithTriangleListStream` (`82921968`),
`IntersectTriangle4Sphere_HackyBurnoutVersion` (`8283D2E0`) and
`TestSphereTriangle4SOA` (`8283FD50`) evaluate multiple triangles in vector lanes.
The current [line kernel](../src/GameShared/Jobs/ContactGenerator/ContactGeneratorJob.cpp#L547),
[sphere contact kernel](../src/GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.cpp#L80)
and [sphere rejection kernel](../src/GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.cpp#L377)
retain scalar lane processing. Include their polygon-soup unpack/pack dependencies
in the review, rather than optimizing only the final test.

Recover packed execution and compare full masks, nearest hits, contact data and
tags against the established scalar control, including partial/invalid triangle
batches and boundary cases. The restored swept-sphere helper is a separate path.

### V02 Four-way coarse ray tests

[TestLineSphere4 and four-box testing](../src/GameShared/GameClasses/Geometric/Intersection/CgsLineTests.cpp#L294)
iterate per lane. ARTIST's octree `LineTestRecursive` contains the packed twins,
including `828BD114..828BD238` and `828BD908..828BD9E8`.

Recover packed coarse rejection while preserving lane masks, remainder handling,
endpoint/touch behavior and result order. Validate through the octree caller,
not only a standalone math helper.

### V03 Packed frustum-plane tests

ARTIST `Frustum::IsSphereInFrustum` (`828AF020`) tests packed plane groups. The
active native [entity tests](../src/GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.cpp#L1881)
loop over eight planes; node-box classification is also scalar-lowered.

Recover vector plane testing and compare complete visible-entity lists, masks and
edge cases. This is separate from J03 scheduling and C02 four-node traversal.

### V04 Paired wheel friction

`VehiclePhysics::HandleWheelPairFriction` (`825FB458`) packs wheel A/B and the four
longitudinal/lateral grip evaluations. The current
[implementation](../src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp#L444)
explicitly lowers that work to per-wheel calculations.

Recover the packed work with unchanged grip selection, slip, adhesive limits,
force/torque write-back and wheel ordering. Validate simulation outputs and
driving/contact behavior; a faster kernel is insufficient if handling changes.

### V05 Solver and constraint-builder vectors

ARTIST `Simulation::ContactBatchBuild` (`82BC14C0`), `Horus_Pipeline` (`82BC1A20`),
`JointJacobian::Build` (`82BC42E8`) and `DriveJacobian::Build` (`82BC5590`) use
vector arithmetic. Native [contact/solver code](../vendor/renderware/src/rw/physics/Simulation.cpp#L1246)
and [constraint helpers](../src/vendor/renderware/physics/JacobianMath.hpp#L7)
retain scalar lowering.

Recover vector work across the applicable pipelines/builders, preserving solver
iteration/order, body states, inverse inertia, impulse clamps and contact
snapshots. Verify real multi-body, jointed/detached-part and crash workloads.

### V06 Particle and debris geometry vectors

ARTIST Lion `RenderSprites` (`82282608`), `RenderQuads` (`82282B28`) and
`RenderTilts` (`82282FC8`) use packed geometry arithmetic. The current
[Lion renderer](../src/GameSource/Effects/Particles/Native/BrnLionBlendRenderer.cpp#L789)
is substantially scalar; the native screen also identifies spark-bank,
simple-particle and debris geometry paths for the same review.

Recover the applicable vector kernels with unchanged vertex order, UV/frame
interpolation, transforms, fades, blur history and bank rules. Validate complete
generated buffers and visible effects across main/reflection cameras. This work
is independent of scheduling J04/J05.

### V07 Shared vector and matrix backend

The [native VPU vocabulary](../vendor/renderware/include/rw/math/vpu/vector4_operation.h#L3)
explicitly uses scalar components. Recovery is selective rather than complete
across game math, cameras, physics and shadow calculations. One concrete remaining
case is [ComputeTSMMatrix](../src/GameSource/World/ShadowMap/BrnShadowMap.cpp#L582)
(`827BFF58`), which is conditional on TSM projection being selected.

Inventory the actual hot/common vector and matrix kernels and recover their
native vector work. Do not mark this complete solely because a common header
contains intrinsics, or count every scalar operation as a missed optimization.
Check compiled callers and preserve layout, operand order and numerical behavior.

### Native assembly witnesses

The inspected executable contains zero direct packed float arithmetic in the
following bodies. Counts are static scalar arithmetic/comparison instructions,
not executed counts, milliseconds or predicted gains.

| Native function | Virtual address | Scalar count |
| --- | --- | ---: |
| `ExecuteLineWithTriangleListStream` | `140232AE0` | 237 |
| `TestSphereTriangle4SOA` | `1404498A0` | 121 |
| `IntersectTriangle4Sphere_HackyBurnoutVersion` | `14044A520` | 207 |
| `HandleWheelPairFriction` | `14025DC50` | 316 |
| `Simulation::ContactBatchBuild` | `140249650` | 278 |
| `Simulation::Horus_Pipeline` | `14024ACE0` | 144 |
| `DriveJacobian::Build` | `14024E6F0` | 403 |
| `JointJacobian::Build` | `14024FCD0` | 416 |
| `LionBlendRenderer::RenderSprites` | `14015ED10` | 90 |
| `LionBlendRenderer::RenderQuads` | `14015E840` | 84 |
| `ShadowMap::ComputeTSMMatrix` | `140188450` | 709 |

`RenderTilts` is partially packed (six packed arithmetic instructions and 352
scalar arithmetic/comparison instructions); it is not a zero-SIMD body. Delegated
helpers must always be inspected before drawing a conclusion from these counts.

## Conditional and incompletely audited original paths

These remain in the completion scope, but must not be advertised as established
ordinary-play performance wins.

### C01 Occlusion and conditional rendering

The [manager](../src/GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h)
and native query/conditional-render chain are incomplete. ARTIST's constructor
(`827F1E00`) loads zero at `827F1E18` and stores it to the master enable byte at
`827F1E58`. Enabled pass options alone do not activate the feature.

Recover the original engine lifecycle and a suitable native implementation,
including generation, consumption and invalidation. Preserve original defaults.
Establish activation and validate visibility, camera cuts, unavailable/stale
results and teardown without adding per-frame GPU waits or hiding geometry.

### C02 Four-node frustum traversal

`FrustumTestVpRecursive4` (`828BE100`) and `NodeInsideFrustumVp4` (`828B1E58`) are
absent from the current implementation. ARTIST references a `FrustumTestVp4`
entry at `828CABC8`; ordinary-play activation was not established by this audit.

Recover the original entry/selection and traversal, preserve its guard/default
policy, and verify result equivalence and ordering. This is not the same work as
J03 or V03 and must not be silently substituted for the ordinary traversal.

### C03 Replay job machinery

The [module's serialization wait](../src/GameSource/Replays/BrnReplayModule.cpp#L162)
defers the actual job wait; embedded jobs remain documented placeholders.
Original waits are visible in `WaitForSerialiseJobs` (`8264B4A8`) and playback
`UpdatePlaying_PreSim` (`826608A8`).

Trace and reconstruct the complete producer/job/consumer chains, native records,
completion and shutdown. Verify recording/playback correctness and timing in the
replay feature; this is not an assumed improvement to ordinary driving.

### C04 Audio CPU-budget culling

[CpuLoadBalancer::CullVoices](../vendor/renderware/src/rw/audio/core/CpuLoadBalancer.cpp#L181)
is a stub. ARTIST `Balance` (`82B6EF00`) calls it at `82B6F014`; the target body
at `82B6E410` was missing from the available per-function export.

Recover the body from authoritative evidence and validate budget/priority
selection. The current [game audio preparation](../src/GameSource/Sound/Module/BrnRootSoundModule.cpp#L462)
sets the load budget to 100, bypassing this culling branch. Preserve that default;
do not enable voice dropping as an FPS workaround. Verify native timing units if
the conditional budget path is exercised.

### C05 Original PC pure-device policy

TUB device creation at `947F10` selects hardware processing plus `PUREDEVICE`
when supported. The [native device](../src/pc/gcm/renderengine/device.cpp#L281)
uses ordinary processing and the backend still relies on native Get-state calls.

Recover the supported PC policy with complete state ownership/shadowing and
compatible getters, or establish a verified native equivalent of its work
avoidance. Validate all writers, assertion/state-block restoration and
device/reset paths. Measure the policy before choosing its default; flag presence
alone is neither safe implementation nor evidence of a gain.

## Previously restored mechanisms

These are implemented at the baseline. Their existing evidence is retained in
[PCPerformance.md](../tests/PCPerformance.md); this audit did not rerun it.

| ID | Mechanism and current policy | Completion note |
| --- | --- | --- |
| A01 | Native frame/update-dispatch overlap, mesh preparation and joined publication | Active. Verify ownership/regressions while adding other jobs. |
| A02 | Object-to-mesh jobs (`823F5898`), sixteen sort jobs (`823F5F70`), four traffic jobs (`82752CB0`) | Implemented opt-in through `BRN_MESH_JOBS`, `BRN_SORT_JOBS`, `BRN_TRAFFIC_JOBS`. Native comparisons did not establish a general default benefit. |
| A03 | Disk-device workers, staged loader and native decompression (`828DB2C0`) | Active. Serialized module queues do not mean main-thread physical disk reads. Relocation J07 remains separate. |
| A04 | Full sort keys, state/binding/constant caches, retained geometry and instancing; colour-cube, swept-sphere and packed-bounds SIMD | Implemented. Verify actual shader/caller coverage and helper delegation rather than assuming all draws/math are covered. |

ARTIST `ThreadLayout::Update` (`828E17D0`) joins dispatch, calls the vsync-completion
hook, then performs the resource update. The [native coordinator](../src/GameShared/GameClasses/System/Threads/CgsThreadLayout.cpp#L229)
follows that ordering. A comment claiming a missing independent resource thread
does not establish a reconstruction gap.

The inspected TUB startup disables its optional GPU query-pacing mechanisms;
generic defaults must not be counted as active original pacing. See the
[retained PC query-policy audit](../tests/PCPerformance.md#original-pc-query-pacing-policy-2026-10-05).

Native draw submission, texture allocation and presentation can still be costly.
Those costs do not, by themselves, identify an unreconstructed original batching,
cache or pacing mechanism. Existing opt-in jobs and experiments are not new gaps
merely because the wider 165 FPS goal remains unmet.

## Coverage work required before closure

### G01 Inventory original mechanisms across the complete runtime

Maintain a function-to-mechanism inventory covering rendering/dispatch,
visibility/spatial queries, physics/deformation, particles/Lion, resource loading
and memory, audio, video, UI, traffic/AI, networking and replays. Follow producers,
consumers, guards, data dependencies and indirect/custom job routes. The 21 direct
scheduler callers and the initial named families are starting points, not the
whole-program denominator.

### G02 Reconcile unmatched and scalar-screen candidates

Give the 661 unmatched vector-heavy functions and relevant scalar-screen hits
explicit dispositions. Examples include inlined/folded implementations, different
names, existing native/provider equivalents, code outside the ARTIST game path,
original-disabled paths, confirmed omissions and unresolved evidence.

A name mismatch or scalar entry that delegates to SIMD is not a missing body.
An external provider classification alone does not prove equivalent performance
mechanisms. Record evidence and add newly confirmed gaps to this tracker. No
unresolved candidates may be hidden behind a completion percentage.

### G03 Establish activation and default policies

For each mechanism record original defaults, writers/selectors, a reachable
caller chain, current native policy and supported workload. Reconstruct original
conditional paths while preserving their defaults. Record deliberate maintainer
exceptions, including the existing requirement not to restore the removed
five-damaged-car visibility cap or reduce requested graphics/simulation work.

### G04 Verify and publish closure evidence

For each completed row, add its implementation commit/PR, ARTIST assembly
evidence, declaration checks, native algorithm/scheduling proof, focused parity
checks and a behavior-specific runtime witness. Verify worker overlap or vector
execution directly; boot success and absence of assertions alone do not prove it.

Use attributable builds and representative driving, streaming, combat/crashes,
multiple cameras, effects, video, replay, network and conditional-feature cases as
applicable. Separate instrumented attribution from clean timing. Record settings,
hardware/background load, failures and limitations. No fixed FPS gain is promised.

## Maintaining this tracker

Stable IDs are used by the umbrella issue and future implementation work. New
discoveries extend the inventory; they do not replace or erase earlier evidence.
Each completed row needs a linked source commit and validation record. Keep
implemented, default-enabled, conditional and unverified states distinct. Before
closing the umbrella issue, publish a final inventory/reconciliation and the
closure build evidence; do not close it from the initial checklist alone.
