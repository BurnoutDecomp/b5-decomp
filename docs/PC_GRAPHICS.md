# PC graphics options

Edit `config.ini` beside `Burnout_PC.exe` (normally `build/game/config.ini` in the
workflow checkout), then restart. `config.ini.example` lists the original defaults.
Normal shutdown saves native settings and retains unrelated INI sections and keys.

## Generic engine and debug controls

`[Debug]` accepts a registered writable variable's full debug menu path and name.
The INI and debug menu use the same engine variable, metadata and change callback.
There is no per-setting shadow copy. Paths are case-insensitive, a leading slash
is optional, and `/` and backslash separators are equivalent. Spaces and the
literal `...` in existing menu names are significant.

```ini
[Debug]
Environment/Bloom luminance scale=0.3
World/LODs/Environment Map LOD=0
World/LODs/Prop Environment Map LOD=0
World/ShadowMap/Cast shadows from traffic=true
```

Overrides are initial values. They apply once when a registration batch has
finished attaching its metadata. Variables registered later receive their initial
values then. A lazy debug component is activated when the INI names one of its
child controls; functions and actions are not executed by the INI. Subsequent
menu or console edits remain effective and are not reapplied every frame. Shutdown
retains the text in `[Debug]`; it does not replace startup preferences with temporary
menu edits.

Supported variables are engine booleans, signed/unsigned 32-bit integers and floats.
Booleans accept `true`/`false` or `1`/`0`. Enum controls accept their option name or
an existing numeric option value. Malformed numbers, overflow, non-finite floats,
read-only values and opaque pointers are rejected. The normal engine change path
clamps numeric values to registered menu ranges and invokes change callbacks.
Unknown paths remain pending for later registration and are logged as unmatched
at normal shutdown. The loader cannot invent a control that the current build
never registers. Do not put trailing comments on value lines.

## Reflections

`World/LODs/Environment Map LOD` selects **world geometry** detail in reflections:
0 highest, 1 intermediate, 2 original. `World/LODs/Prop Environment Map LOD` separately
selects **prop and detached prop-part** detail using the same 0/1/2 values. Missing
prop configuration preserves LOD2. The original `Override Prop LOD` control has
higher precedence if enabled. Models lacking the selected LOD are still skipped.

Both controls require `[Settings] EnvironmentMap=1`. The cube remains 128 x 128 per
face and uses the props' current transforms. These controls do not change texture
resolution, streaming or material reflectivity. They do not add traffic or rivals
to the reflection pass. Vehicle paint and glass sample the same player-centered cube.

```ini
[Debug]
; All six faces each frame (original default).
World Module/Graphics/Render environment map at 30hz=false
; Use true instead for alternating groups of three faces.
```

The previous `[Settings] EnvironmentMap30Hz` seed has been removed. The registered
world control now drives the actual scheduler and remains editable in its debug page.

## World, prop and vehicle detail

For extended world/prop distances, use the engine's real three distances and enable flags:

```ini
[Debug]
World/LODs/OverrideDistances=true
World/LODs/LOD0Distance=3000
World/LODs/LOD1Distance=6000
World/LODs/LOD2Distance=9000
World/LODs/OverridePropDistances=true
World/LODs/PropLOD0Distance=3000
World/LODs/PropLOD1Distance=6000
World/LODs/PropLOD2Distance=9000
```

Disable the corresponding flag for authored distances. The original world debug
range is 25..10000 and the prop range is 1..10000. These distances only affect
available geometry; they do not load the entire island. Prop distances also affect
culling. World instances beyond all override bands retain their authored LOD fallback.

Vehicle preset/custom parsing has been removed. Set the original quality table
entries directly. These values reproduce the former High preset:

```ini
[Debug]
Graphics/Vehicles.../LODs.../Quality LOD 0=30
Graphics/Vehicles.../LODs.../Quality LOD 1=66
Graphics/Vehicles.../LODs.../Quality LOD 2=105
Graphics/Vehicles.../LODs.../Quality LOD 3=150
Graphics/Vehicles.../LODs.../Quality LOD 4=210
```

The original defaults are 10,22,35,50,70. The original debug range is 0..300 metres;
the former Ultra preset's final 350 m entry is therefore 300 m through the real
engine setter. The aggressive table is separately registered under the same group
as `Aggressive LOD 0` through `Aggressive LOD 4`. The engine blends the two tables
according to vehicle crowding. The player car retains LOD0.

Other existing registered controls can be added without more INI-specific code,
for example `Environment/Bloom threshold scale`, `Environment/Specular scale`,
`World/LODs/Override Prop LOD` and `World/LODs/Prop LOD number`.

## Native quality settings

These options retain their dedicated `[Graphics]` code because they configure PC
rendering behavior that has no equivalent registered engine variable:

| Key | Default | Effect |
| --- | --- | --- |
| `AnisotropicFiltering` | `1` | `1` original trilinear; `2`, `4`, `8`, `16` anisotropic world filtering, clamped to adapter support. |
| `ShadowResolutionScale` | `1` | `1` original 1280 x 1920 atlas; `2` 2560 x 3840, falling back if unsupported. |
| `ShadowDistance` | `120` | 30..500 m; scales the three existing cascade ranges and fade together. |
| `ShadowSlopeBias` | `0` | 0..4 extra native caster slope bias; try 1 to reduce striped self-shadows. |

For sharper roads, use `AnisotropicFiltering=16`. It preserves authored mips and
biases; cube, shadow and post-processing samplers retain their own filtering.
For sharper shadows, use `ShadowResolutionScale=2` and `ShadowSlopeBias=1`.
Shadow resolution is independent of display resolution. The larger atlas uses
about 38 MiB of depth storage instead of about 9 MiB. Increasing shadow distance
reduces texel density; excessive slope bias can detach edges from their casters.
These native options require a restart.

## Migration from dedicated engine settings

The following former keys are no longer parsed or saved. Move their values into
`[Debug]`; legacy aliases and vehicle preset tables are not retained in code.

| Removed key | Registered replacement |
| --- | --- |
| `[Graphics] BloomLuminanceScale` | `Environment/Bloom luminance scale` |
| `[Graphics] EnvironmentMapLOD` | `World/LODs/Environment Map LOD` |
| `[Graphics] TrafficShadows` | `World/ShadowMap/Cast shadows from traffic` |
| `[Graphics] WorldLODOverrideDistance` | `World/LODs/OverrideDistances` plus `LOD0Distance`, `LOD1Distance`, `LOD2Distance` |
| `[Graphics] PropLODOverrideDistance` | `World/LODs/OverridePropDistances` plus `PropLOD0Distance`, `PropLOD1Distance`, `PropLOD2Distance` |
| `[Graphics] VehicleLODPreset`, `VehicleLOD0Distance`..`VehicleLOD4Distance` | `Graphics/Vehicles.../LODs.../Quality LOD 0`..`Quality LOD 4` |
| `[Settings] EnvironmentMap30Hz` | `World Module/Graphics/Render environment map at 30hz` |

For a positive former world/prop base distance, enable its override and copy base,
base x 2 and base x 3 to the three controls, subject to their original ranges.
A former base of zero maps to a disabled override.

## Anti-aliasing

`[Settings] AntiAliasing` drives native D3D9 scene MSAA: 0 uses the original 2x,
1 disables MSAA, and 2/4/8 request those sample counts. Both colour and depth must
support the request; the renderer falls back to a supported lower count. The boot
log records the actual count. The setting also applies to the environment-map
scratch target, and survives resizing and F11 fullscreen changes.

## Validation

From the workflow checkout:

```powershell
python b5-decomp/tests/run_pc_debug_ini.py
python b5-decomp/tests/run_pc_prop_reflection_lod.py
python b5-decomp/tests/run_pc_graphics_settings.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/PCDebugIniLive.ps1 -Slot 9
```

The native case uses a private slot and profile fixture. It checks initial registry
overrides, actual prop capture LODs and later console edits without startup reapplication.
Set `BRN_TEST_PROP_REFLECTION_LOD=2` for the missing-prop-option default case.
