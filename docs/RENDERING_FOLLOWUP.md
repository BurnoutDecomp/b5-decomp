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
