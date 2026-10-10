# Native reflection and small-caster controls

The optional PC capture code lives in `src/pc/gcm/renderengine/reflections/`.
The related shadow extension lives in `src/pc/gcm/renderengine/shadows/`.
These are native additions, not claims of additional console reconstruction.

`config.ini.example` contains an enabled Relative preset. The launch configuration
is `config.ini` beside the executable. Registered controls use `[Debug]` and the
same full paths as the in-game menu. Source defaults keep the new scene categories
off; wheels default on within an explicitly enabled vehicle capture.

## Reflection categories

The following paths are under `World/Reflections/`:

| Category | Captured content | Relative draw-distance baseline |
|---|---|---|
| Backdrops | Resident backdrop stand-ins beyond the detailed reflection-world cutoff | Each instance's authored maximum draw distance |
| Traffic | Nearest resident traffic, within the normal vehicle budget | The traffic module's live render-cull distance |
| Rivals | Active non-player race cars | The live vehicle LOD3 transition, including quality/aggressive blending and zoom |
| Wheels | Original wheel meshes and transforms | The parent vehicle category's normal cutoff |
| Glass | Intact transparent vehicle surfaces, including window and lamp lenses, and original damaged-pane geometry | The parent vehicle category's normal cutoff |
| Lights | Original race-car and traffic-signal corona submissions | The original 250 m corona cutoff |
| Particles | Published simple particles, Lion effects, sparks, trails and solid debris | The main particle camera's far clip |

Every category has `Enabled`, `Draw distance mode` (`Relative` or `Fixed`),
`Draw distance`, and `Draw distance scale`. Relative multiplies its baseline by
the scale; Fixed uses metres. Distances are bounded to 10,000 m. The capture
projection expands to cover enabled categories. Resident/streaming availability,
authored particle fades and vehicle budgets still apply.

Mesh categories also have `LOD mode` (`Fixed`, `Relative`, `Custom`), `Fixed LOD`,
`LOD distance scale`, `LOD0 distance`, and `LOD1 distance`. Relative follows the
live normal thresholds. Custom uses the two explicit metre thresholds. Sparse
models use an available state instead of disappearing. Vehicle box/proxy states
above LOD2 are excluded; the original wheel LOD1 floor remains.

Wheels and glass are children of an enabled, visible traffic/rival vehicle;
their own controls can shorten their range or change detail. They do not create
a vehicle outside its body category's range. Glass uses an independent transparent
pass with authored materials and damage masks. Fully destroyed panes remain absent.

The old `World/LODs/Environment Map Draw Distance` and World/Props reflection LOD
controls remain available. The backdrop shell starts after the detailed reflection
world cutoff, including its draw-distance extension. It walks resident stand-ins
and does not load new zones or change main-view streaming visibility.

## Frame and device behavior

Face cameras and corona buffers publish with the joined command frame. Traffic
uses each face's scene query rather than mutating main-camera visibility history.
Reflected cars sample a completed previous cube snapshot, avoiding sampling the
texture currently being written. The snapshot releases before device reset.

Particle inputs are snapshots. Camera-dependent vertices are rebuilt for each
face, with separately owned low-address buffers; the shared 4 MB immediate-mode
arena is not used for these captures. Lion's lifecycle update runs once per
presentation. Per-face rendering uses that same time and restores its main camera.
The independent particle cutoff preserves the geometry pass's depth projection.
The original particle and corona master switches remain authoritative. Particle
captures and debris shadows also respect calibration, stall and suspension gates.
Corona alpha keeps the original fade shape scaled to the selected capture distance;
authored size curves and the main-view fade remain unchanged.

Glass and effects preserve depth tests, disable depth writes, and restore device
state before the main view. Mirrored cube winding and native depth-range conversion
are handled in PC support code. Solid-debris shadow rendering restores a complete
device state block and the engine's logical depth comparison.

## Small-object and particle shadows

Small props already submit to the original prop shadow pass. The optional
`World/ShadowMap/Small objects` override adds separate distance and LOD controls
for props and detached prop parts whose model bounding-sphere radius is at most
`Maximum radius` (default 2 m). Other props keep their existing policy.

`World/ShadowMap/Solid debris` adds optional casters for the four opaque debris
mesh families. Relative distance follows `[Graphics] ShadowDistance`; Fixed uses
the category's metre value. Casters use the original particle positions, transforms,
lifetimes and meshes, the published cascade cameras, and the existing atlas.
They preserve cascade depth/scissor/bias and write depth without colour.

The master shadow switches, prop-caster switch and cascade configuration remain
authoritative. This does not add opacity shadows for smoke, sparks, Lion billboards
or trails. Those require a separate opacity/alpha shadow rendering path. Transparent
glass debris is also excluded from solid-debris casters.

## Remaining capture limitations

- Traffic-vehicle corona submission and the prop corona loop are not reconstructed.
  Their missing emitters are not approximated. Race-car lamps, signal coronas and
  existing emissive vehicle materials can appear in captures.
- Glass classification follows authored vehicle transparency flags, so the Glass
  category includes transparent lamp lenses as well as windows.
- No new streaming, dynamic light projection or volumetric particle-shadow system
  is introduced. Existing effect/renderer switches and suspension are respected.

`BRN_REFLECTION_SCENE_TRACE=1` reports bounded per-face glass-mesh, corona and
particle-byte witnesses. `BRN_SMALL_SHADOW_TRACE=1` reports solid-debris caster
eligibility per cascade. These are submission witnesses, not image-parity verdicts.

## Verification

The shipping executable links 2,947 translation units, including all four new PC
source mounts in the workflow build. Targeted regressions cover native cube-copy
isolation, transparent material filtering, depth/winding/clip policies, independent
LOD/distance selection, small-prop shadow packets, original cascade winding and
quality settings, and debug INI handling.

The live reflection run reached driving with glass, corona and particle submissions
and zero asserts. A longer run exposed the immediate arena limit; separate buffer
ownership fixes it, and subsequent longer menu runs have zero arena exhaustion.
These checks do not establish visual parity for every effect or damage state.
The deterministic crash run passes all seven capture/caster checks: opaque debris
reaches all three cascades, reflected particle vertices are produced, and the run
has zero asserts, exceptions or arena exhaustion. Caster counts establish the
submission path; a separate image comparison is needed to judge shadow appearance.
