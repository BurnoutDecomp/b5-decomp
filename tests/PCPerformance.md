# PC rendering performance

This pass restores lost work avoidance and SIMD processing without changing
resolution, antialiasing, draw order, LOD distances, reflection settings or damage
visibility. It does **not** establish original-PC minimum requirements or a locked
165 FPS at 1440p.

## Implemented

| Change | Original evidence and PC implementation |
| --- | --- |
| Shared shader-constant shadow | ARTIST `DrawRenderableMeshZOnly::Interpret`, `0x827F6654..66C0` and `0x827F718C..71F8`, skips unchanged source blocks. All PC float-constant writers now share a register-value cache, including GUI and diagnostics. Value comparison handles mutable PC source buffers and overlapping register ranges. |
| Shared sampler-state shadow | ARTIST `shadow::Device::SetState`, `0x82276A2C..34`, and whole texture-state bind `0x8227D17C..98` avoid duplicate writes. The PC cache covers all native sampler writers and keeps pixel, displacement and vertex sampler IDs separate. Failed writes never establish cached state. |
| Native static geometry | The console binds resident GPU resource memory. PC retained vertex/index mirrors now prefer `DEFAULT/WRITEONLY`, with the previous `MANAGED` pool retained when native allocation fails. Streaming retirement and cache generations are preserved. F11 does not reset the device. |
| Exact indexed-draw ranges | Cache the minimum/maximum indices consumed by the final native topology after primitive-reset conversion. D3D9 receives that mesh range rather than the full shared vertex buffer. Ignore incomplete list tails, and retain nonzero base-vertex addressing. |
| SIMD colour-cube blending | ARTIST `0x82AD2F38` and `0x82AD4170` use vector kernels specialized by source count. SSE2 restores both properties for the PC job. Existing PC truncation/clamping, BGRA layout, source order and destination pitches remain unchanged. |
| Optional diagnostics | Wheel index/bounds scans require `BRN_WHEEL_DIAG=1`. Composite GPU readback sampling requires existing `BRN_RT_PROBE=1`. `BRN_WHEEL_ZALWAYS` remains independent. |

The native state shadows are invalidated at device creation. Any future raw float
constant/sampler write or state-block restoration must use these wrappers or
invalidate them. A future device Reset must also retire/recreate DEFAULT geometry;
the current renderer resizes through additional swap chains without Reset.

## Verification

From the parent workflow checkout:

```powershell
python b5-decomp/tests/run_pc_shader_constant_cache.py
python b5-decomp/tests/run_pc_world_geometry_buffers.py
python b5-decomp/tests/run_pc_tint_blend.py
python b5-decomp/tests/run_pc_fullscreen.py
python b5-decomp/tests/run_pc_display_resize.py
python b5-decomp/tests/run_world_vertex_lifetime.py
.\build.cmd exe --jobs 4
```

The six suites pass 323 checks. They include real D3D9 register/sampler reads,
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
with roughly 30–34% aggregate host CPU load, not a hardware requirement benchmark.

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
improvement. The four-source 32-cube microbenchmark measured 0.405–0.453 ms scalar
versus 0.084–0.093 ms SIMD, about 4.8x for this job alone.

Evidence is under the parent checkout's `scratch/performance_0929/`: paired
`samples.summary.json`/`samples.json`, per-run flow logs and executable SHA-256
provenance. `original1440_static_repeat` / `final1440_static_repeat` and
`original1440_roadrage` / `final1440_roadrage` are the rows above. The saved baseline
is `a8a0924abb70...` (pre-optimization build, with the takedown-cap fix already
present); its historical stamp is `21dc3410+dirty`. The measured optimized build
is `bea3c1a9b5f5...`, `8932ad0b+dirty`. The `original720_static` / `reviewed720_static` pair uses the reviewed build
`581724937222...`, including the allocation fallback and incomplete-tail fix.

## Remaining original optimization gaps

- The native main loop still executes update and dispatch serially. Original
  `ThreadLayout` overlaps these stages. Parallelizing it requires respecting the
  existing frame buffers and removing shared mutable PC-backend assumptions.
- Original wheel/mesh instancing is expanded into individual native draws. Restoring
  hardware instancing needs the matching shader and instance-stream representation;
  merely changing the draw flag is insufficient.
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
