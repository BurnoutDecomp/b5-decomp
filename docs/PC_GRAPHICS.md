# PC graphics options

Edit `config.ini` beside `Burnout_PC.exe` (normally `build/game/config.ini` in the
workflow checkout), then restart the game. Normal shutdown saves the selected
settings. Existing INI sections, comments and unrelated keys are retained.
`config.ini.example` lists the options with the original graphics defaults.

The new `[Graphics]` options expose the original ARTIST engine tweakables used by
the Breaker Island Xenia graphics patches. They do not need patched game data.

| Key in `[Graphics]` | Default | Effect / patch equivalent |
| --- | --- | --- |
| `BloomLuminanceScale` | `1` | Original bloom luminance multiplier. The Bloom patch uses `0.3`; `0` removes its contribution. Accepts finite values from 0 to 10. |
| `EnvironmentMapLOD` | `2` | World detail in car reflections: `0` highest, `1` intermediate, `2` original. The Env Map Boost patch uses `0`. Requires `[Settings] EnvironmentMap=1`. |
| `TrafficShadows` | `0` | Set `1` to include traffic in the original shadow-map pass, matching the Traffic Shadow patch. |
| `WorldLODOverrideDistance` | `0` | `0` uses the model's original distances. Positive values enable the original override and set the three distance bands to value × 1, 2, 3. High patch: `3000`; Low patch: `1`. |
| `PropLODOverrideDistance` | `0` | Same override for props. High patch: `3000`; Low patch: `50`. |
| `VehicleLODPreset` | `Default` | `Default`, `Potato`, `Low`, `Medium`, `High`, `Ultra`, or `Custom`; names are case-insensitive. Applies to the original vehicle quality table, used by both race cars and traffic. |

World/prop base distances accept integers from 0 to 10000. Higher detail generally
costs more rendering time. These overrides affect LOD selection within the
original streaming, visibility and available-model limits; they do not load the
whole island or create missing high-detail models.

The vehicle presets reproduce the patches' float values exactly:

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
Switching to a named preset retains inactive custom keys. As in the patches, the
aggressive vehicle LOD table and the engine's crowding blend remain original.
The patches' debug-menu range change is unnecessary for INI settings.

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

For the graphics patches currently enabled in the supplied Xenia folder, plus 8× AA:

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

To add the High world/prop patches, change both override distances to `3000`.
Use `VehicleLODPreset=Ultra` for the Ultra vehicle patch.
The `[graphics]` boot line reports the selected values and AA request.

## Verification

From the workflow checkout:

```powershell
python b5-decomp/tests/run_pc_graphics_settings.py
python b5-decomp/tests/run_pc_fullscreen.py
python b5-decomp/tests/run_pc_display_resize.py
powershell -NoProfile -ExecutionPolicy Bypass -File b5-decomp/tests/PCGraphicsSettingsLive.ps1
```

The graphics runner exercises production INI loading/saving, exact presets,
invalid values, engine-global assignment, the original LOD classifier, and native
D3D9 MSAA surface creation, binding, draw and resolve.
The live smoke test uses a private slot with High vehicle/world/prop settings,
the bloom/reflection/traffic-shadow patches, and 8× AA. It checks driving, the
actual AA target and fault-free logs, captures bounded frames for visual review,
and restores the slot's original INI.
