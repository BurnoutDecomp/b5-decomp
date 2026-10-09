# PC graphics options

Edit `config.ini` beside `Burnout_PC.exe` (normally `build/game/config.ini` in the
workflow checkout), then restart the game. Normal shutdown saves the selected
settings. Existing INI sections, comments and unrelated keys are retained.
`config.ini.example` lists the options with the original graphics defaults.

The `[Graphics]` section controls bloom strength, reflection detail, traffic
shadows, world/prop LOD distances, and vehicle LOD presets.

| Key in `[Graphics]` | Default | Effect |
| --- | --- | --- |
| `BloomLuminanceScale` | `1` | Bloom luminance multiplier. `0.3` reduces its contribution; `0` removes it. Accepts finite values from 0 to 10. |
| `EnvironmentMapLOD` | `2` | World detail in car reflections: `0` highest, `1` intermediate, `2` original. Requires `[Settings] EnvironmentMap=1`. |
| `TrafficShadows` | `0` | Set `1` to include traffic in the shadow-map pass. |
| `AnisotropicFiltering` | `1` | World texture filtering: `1` preserves original trilinear filtering; `2`, `4`, `8`, or `16` improve detail on surfaces viewed at shallow angles. Clamped to the device's supported level. |
| `ShadowResolutionScale` | `1` | `1`: original 1280×1920 atlas (1280×640 per cascade). `2`: 2560×3840 (2560×1280 per cascade), when supported by the adapter. |
| `ShadowDistance` | `120` | Shadow view distance in metres, from 30 to 500. Scales all three cascade ranges and the fade distance together. |
| `ShadowSlopeBias` | `0` | Extra native caster slope bias, from 0 to 4. `1` reduces striped self-shadowing from comparison filtering; `0` retains the original material bias. |
| `WorldLODOverrideDistance` | `0` | `0` uses the model's original distances. Positive values enable the distance override and set the three bands to value × 1, 2, 3. Example bases: `3000` for extended distances, `1` for short distances. |
| `PropLODOverrideDistance` | `0` | Same override for props. Example bases: `3000` for extended distances, `50` for short distances. |
| `VehicleLODPreset` | `Default` | `Default`, `Potato`, `Low`, `Medium`, `High`, `Ultra`, or `Custom`; names are case-insensitive. Applies to the original vehicle quality table, used by both race cars and traffic. |

World/prop base distances accept integers from 0 to 10000. Higher detail generally
costs more rendering time. These overrides affect LOD selection within the
original streaming, visibility and available-model limits; they do not load the
whole island or create missing high-detail models.

For sharper roads, use `[Graphics] AnisotropicFiltering=16`. This controls texture
filtering independently of the mesh LOD settings. It preserves authored mipmaps
and their original LOD biases; it does not force full-resolution textures at every
distance. Missing or invalid values use `1`. Cube reflections, shadow maps and
post-processing samplers keep their own filtering.

For sharper shadows and reduced striping, try `ShadowResolutionScale=2` and
`ShadowSlopeBias=1`. Shadow resolution is independent of display resolution.
The larger atlas uses about 38 MiB of depth storage instead of about 9 MiB;
unsupported dimensions fall back to the original atlas. The original atlas
layout, cascade aspect ratios and shader programs are retained, with the
half-texel sampling offset adjusted to the actual atlas size.

Increasing `ShadowDistance` spreads the same shadow texels over a larger area,
so it can reduce nearby detail. It uses the existing cascade fitting and only
includes geometry available to the original streaming and visibility paths.
Higher slope bias can move shadow edges away from their casters, so use the
smallest value that removes visible striping. These options require a restart.

The vehicle presets use these LOD switch distances:

| Preset | LOD 0 | LOD 1 | LOD 2 | LOD 3 | LOD 4 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Default | 10 | 22 | 35 | 50 | 70 |
| Potato | 1 | 2 | 4 | 6 | 10 |
| Low | 5 | 11 | 17 | 25 | 35 |
| Medium | 20 | 44 | 70 | 100 | 140 |
| High | 30 | 66 | 105 | 150 | 210 |
| Ultra | 50 | 110 | 175 | 250 | 350 |

`Custom` reads `VehicleLOD0Distance` through `VehicleLOD4Distance` from `[Graphics]`.
They accept finite values from 0 to 10000 in nondecreasing order. Missing/invalid
numbers use their original defaults; a resulting decreasing table falls back to
`Default`. Unknown presets and invalid other graphics values use original defaults.
Switching to a named preset retains inactive custom keys. Vehicle presets set the
quality LOD table; the engine still blends it with the original aggressive LOD
table according to vehicle crowding.

The player car always uses LOD0. Vehicle presets affect traffic and other race
cars, so changing a preset does not change the detail of your own car. World/prop
distance settings change when authored meshes switch detail; nearby objects
already using LOD0 can look identical. Prop distances also affect distance culling.
World instances beyond all three override bands retain their authored distance
LOD, matching the original engine's fallback.

`EnvironmentMapLOD` selects world meshes drawn into reflections. It does not
increase the reflection texture resolution, which remains 128×128 per cube face,
or change the reflective properties of the car's materials. Traffic shadows use
the original cascade passes, including the near traffic caster list.

Vehicle paint, body panels and window glass sample the same dynamic reflection
cube. Props use their current world transforms, including during motion, and the
original reflection LOD2 selection. `EnvironmentMapLOD` changes world geometry
detail only. The PC backend translates the original inverted depth range and sky
depth test so scenery and props survive the final sky draw in each cube face.

## Anti-aliasing

`[Settings] AntiAliasing` already drives native D3D9 scene MSAA:

| Value | Requested mode |
| --- | --- |
| `0` | Original console mode: 2× for the scene target |
| `1` | Off |
| `2`, `4`, `8` | That many samples |

Both colour and depth formats must support the requested count. The renderer
falls back to the highest supported lower count if needed. The boot log records
the actual count as `[postfx-rt] ... colourTexture=MSAA msaa=8`, or records
`msaa=8 unsupported -> ...` when it falls back. `msaa=1` means single-sampled.
Only targets the console already multisamples receive the override; post-processing
textures and the final output remain single-sampled after the scene resolve.
The sample count survives window resizing and F11 fullscreen changes.

Example configuration with 8× MSAA:

```ini
[Settings]
AntiAliasing=8

[Graphics]
BloomLuminanceScale=0.3
EnvironmentMapLOD=0
TrafficShadows=1
WorldLODOverrideDistance=0
PropLODOverrideDistance=0
VehicleLODPreset=Medium
```

For extended world/prop LOD distances, set both override distances to `3000`.
Use `VehicleLODPreset=Ultra` to keep detailed vehicle models over longer distances.
The `[graphics]` boot line reports the selected values and AA request.

## Verification

From the workflow checkout:

```powershell
python b5-decomp/tests/run_pc_graphics_settings.py
python b5-decomp/tests/run_pc_reflection_depth.py
python b5-decomp/tests/run_pc_reflection_orientation.py
python b5-decomp/tests/run_pc_fullscreen.py
python b5-decomp/tests/run_pc_display_resize.py
powershell -NoProfile -ExecutionPolicy Bypass -File b5-decomp/tests/PCGraphicsSettingsLive.ps1
```

The graphics runner exercises production INI loading/saving, exact presets,
invalid values, engine-global assignment, the original LOD classifier, and native
D3D9 MSAA surface creation, binding, draw and resolve.
The reflection runner checks native pixels through the production viewport and
depth-state setters, clears, final sky draw and main-scene target handoff. It
covers moving fixture transforms and both D24S8 and stencil-less D16 depth.
The orientation runner renders a continuous scene through the actual cube-face
cameras and samples it on the GPU across all twelve cube edges. It includes the
original and PC producer sky projection rebuilds and the reflection cull state.
The live smoke test uses a private slot with High vehicle LOD, extended world/prop
LOD distances, reduced bloom, detailed reflections, traffic shadows, and 8× AA.
It checks driving, the actual AA target and fault-free logs, captures bounded
frames for visual review, and restores the slot's original INI.

For runtime evidence, set `BRN_GRAPHICS_DIAG=1` before launching. The optional
`[graphics-effect]` log records actual submitted LOD distributions for world,
props, reflection geometry, traffic and race cars, plus traffic caster records.
Each five-value distribution is LOD0 through LOD4. `distinct` counts submissions
whose model offers different renderables across its LOD states. These counters
describe submitted geometry; camera and GPU culling still determine visible pixels.
