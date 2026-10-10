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
