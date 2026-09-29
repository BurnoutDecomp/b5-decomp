# Traffic render interpolation

PC rendering can run between the fixed simulation ticks. Traffic previously
submitted the latest tick's body and physical-wheel transforms directly while
the camera and race cars interpolated. A traffic car therefore held still on
intermediate frames and jumped to the next physics pose, including after impacts.

The PC presentation history now snapshots live traffic after every
`TrafficEntityModule::PostPhysicsUpdate`. Every render pass samples those endpoints
with the camera's interpolation fraction. Bodies, suspension/steering/spin and
physical wheels use the same fraction. Wheel angles cross the 2-pi boundary by
the short arc. The first physical-wheel sample uses its local placement on the
previous body to keep wheels and chassis aligned during promotion.

Sampling uses local copies; the simulation arrays, collision data, AI, network
output and damage simulation retain their authoritative tick values. Histories
reset on pool reset, death and vehicle-slot initialization. Invisible vehicles
still receive tick snapshots, so becoming visible does not blend across an old
rendered frame. Alpha one preserves the original endpoint exactly.

## Verification

From the workflow checkout:

```powershell
build exe
python b5-decomp/tests/run_traffic_render_interpolation.py
python b5-decomp/tests/run_render_part_interpolation.py
powershell -NoProfile -ExecutionPolicy Bypass -File b5-decomp/tests/TrafficRenderInterpolationLive.ps1
```

The focused regression checks rigid crash rotation, wheel-angle wraparound,
physical promotion/recovery, unchanged ticks, multiple ticks without rendering,
multiple render passes, removed wheels and slot reuse. The live test uses a
private save slot and restores display settings after temporarily selecting
1280x720 with VSync off. It records submitted transforms using the opt-in
`BRN_TRAFFIC_INTERP_DIAG` witness and checks moving physical traffic between ticks.

The September 29 baseline held identical poses on **341/341** eligible
intermediate renders. The fixed run advanced on **204/204**, with 928 physical
traffic samples and zero assertions/exceptions. Both runs lasted 85 seconds and
included traffic checks/slams; the short case did not capture a fatally crashed
traffic flag. Startup CPU load was 11% versus 25%, so these are pose-continuity
results, not a frame-rate benchmark. Original artifacts are under the parent
workspace's `scratch/traffic_interpolation/{baseline,fixed}`.

This change addresses traffic pose presentation. It does not reconstruct missing
traffic detached-panel rendering or interpolate damage vertices or particle
simulation; those remain separate rendering paths.
