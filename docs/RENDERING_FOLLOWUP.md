# Rendering follow-up

The supplied final crash is a null read in
`WorldModule::GenerateShadowMapDispatchLists`, after the `lpEvent` assert at
`BrnWorldModule.cpp:4450` in the reported executable. It is the last shadow
cascade, whose query id is `FF00000A`. Older emergency-log crashes are separate.

The frustum-result producer stopped emitting batches when its 32 KB event queue
filled. Extended reflection views can reach that limit after coarse u16 indices
become u32 entity ids. Native dispatch now reads a complete 128 KB frame, sized
for the existing coarse buffer and query count, while preserving the original IO
layout and bounded legacy queue. Oversized combined records forward each module's
already-filtered owner IDs into its unchanged input queue; ordinary records stay
byte-for-byte identical. Missing shadow results also stop safely before
dereference. A focused queue regression exercises the missing final cascade,
all entity ids, maximum coarse-buffer payload, single 8,192/9,000-ID batches,
bounded module forwarding, frame reset and thread isolation.
No gameplay reproduction of the supplied crash was attempted.

Selecting a submenu now retains its parent. The regression runs the production
selection method against a parent containing only submenu rows, which previously
closed itself, and against parents containing actions.

`World/Reflections/Player wheels` adds independent distance and LOD controls for
the player's own wheels. Its body and glass stay excluded from its own cube.
`World/Reflections/Decals` separates dynamic tyre marks from other particles, with
independent Fixed/Relative draw distance. Boost/exhaust remain in Particles; the
new `Include player effects` switch includes camera-hidden player effects in
captures without changing normal-camera visibility. Both new categories are
enabled in Relative mode in the example and local launch INIs.

All 482 focused checks pass, and the shipping executable links all 2,947 TUs with
zero warnings or errors. Fresh-eyes review passes. These verify code and native
state behavior; they do not establish visual parity or a gameplay soak. Existing
unreconstructed LION `CELL_RENDER` volume emitters remain unavailable.

## Remaining chrome line

The chrome sphere's four renderables match the original X360 positions, packed
normals and tangents (1,604 vertices). Each has 19 coincident vertex pairs with
identical normals and skinning, but split tangents, differing by up to 127.8 degrees.
This is original asset data. The chrome damage shader uses those tangents for its
crumple normal map, so a surface seam is possible even with smooth base normals.
This does not establish the cause of the line in the supplied screenshot.

The current native cube-camera test passes 1,956 checks. A separate GPU sampling
control confirms that linear filtering blends adjoining faces at their boundary.
The orientation test's missing reflection-context include has been repaired.
A 55-second private-slot ChromeCar orbit captured 13 complete cubes and 32 frames
from executable `a0a0f75c1ec0`, using the main reflection settings. The player's
wheels are visible in its reflections; the exact reported line remains inconclusive.
Evidence is in `scratch/CHROME_SEAM_1010`. Main save, INI and executable hashes
are unchanged. No mesh, shader or production-renderer change was made for this line.

## Road Rage reflection-particle assert

The reflection caller passed the post-effects batch count as `Dispatch`'s ending
index. For example, four pre-effects batches and two post-effects batches requested
`[4,2)` instead of `[4,6)`, causing the supplied `luFirstBatch < luLastBatch`
assert. Other combinations silently skipped effects. The caller now supplies the
full batch-array count as the exclusive end, matching ARTIST and the main-view
caller. The original renderer assertions and pre-effects/sparks/post-effects order
are retained. A production-caller regression covers every face, empty groups,
relative group sizes and allocation/ownership gates. No gameplay reproduction
was attempted; evidence is in `scratch/PARTICLE_RANGE_1011/`.
The corrected caller passes 796/796 checks (original caller: 562/796), and the
particle-shadow regression remains 31/31. The 2,947-TU shipping build links with
zero warnings/errors. Independent review and the faithfulness gate pass.

## Boost length and exhaust motion

Lion's render kernels advance live particle positions, velocities, rotation,
size and locator drift. Rendering six reflection cameras at the same absolute
time repeated those integrations. Native capture now shares each bucket's
simulated particles and transforms within one published frame. Every camera still
builds its own oriented vertices, including player boost/exhaust; emitters first
visible in a reflection are evaluated once when needed. The original kernel stays
active outside capture scopes.

The production integration regression passes 236/236 checks (previous code:
102/236), covering all three transform kinds, sparse/full buckets, new frames,
birth/death, and reflection-only visibility. Dispatch and inherited-velocity
regressions remain 796/796 and 37/37. The 2,948-TU shipping build links with zero
warnings/errors; independent review and faithfulness gate pass. No gameplay
reproduction was attempted. Evidence: `scratch/LION_MULTIVIEW_1011/`.

## Reflection resolution

The original shared reflection cube is 128 pixels per face, with one mip and
linear filtering for both chrome and ordinary paint. Vehicle definitions do not
allocate different-resolution cubes. Flat panels can magnify a small patch of
this texture more than the chrome sphere; material constants affect appearance,
so exact attribution of the supplied screenshot remains an inference.

`World/Reflections/Resolution (restart)` now offers 128, 256, 512, 1024 and 2048.
The example and local INIs select 512. Native colour, depth and feedback resources
use matching dimensions; projection uses the allocated extent after pending
menu changes. Hardware texture limits cap creation. Targets are created once,
so resolution edits apply after restart. Other reflection controls are unchanged.

The new production-allocation, menu/INI and GPU regression passes 116 checks;
orientation remains 1,956/1,956. The 2,949-TU shipping build links with no warnings
or errors, and independent review passes. No gameplay reproduction was used.
Evidence: `scratch/REFLECTION_RESOLUTION_1011/`.
