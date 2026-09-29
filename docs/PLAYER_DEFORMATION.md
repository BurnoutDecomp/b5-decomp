# Player-car deformation audit, 2026-09-29

The PC presentation code interpolated the car body, wheels and hinged/detached
panel transforms between physics ticks, but rendered the newest deformation rows
immediately. A mesh vertex and a panel anchor could therefore separate between
ticks even when they agreed in both simulation snapshots.

`ActiveRaceCar` now retains the previous and current 128 deformation/scratch rows
alongside its pose history and samples them with the same interpolation alpha.
The existing restore/latch/apply sequence prevents rendered values feeding back
into the next snapshot. Repair, resource replacement and car-slot resets discard
the old history. Alpha 1 presents the original current rows unchanged.

This is a PC presentation correction. Sensor impulses, compression limits, IK,
hinge dynamics, asset weights and the pinned `RenderParams` layout are unchanged.
It does not establish a cause for a distorted shape that persists at rest.

## Console comparison

The ARTIST X360 assembly was the behavioural reference. The checked path includes:

- `IKDrivenPoint::Construct` / `Update` (`0x82615468` / `0x825E7608`):
  rest-side constraint direction, sequential endpoint solves and scratch blend.
- `DeformableObject::UpdateIK`, `UpdateSkinningOffsets` and
  `UpdateIKAndLocators` (`0x82608858`, `0x825DFA90`, `0x82642230`):
  tag gather, detached-part exclusion, sill clamp and four solver passes.
- `DeformationSensor::ApplyLocalImpulse` / `RecievePassedOnImpulse`
  (`0x825E1320` / `0x825E11F8`): absorption, remaining compression room and
  propagation. The negative remaining-room case is present in the original.
- `PhysicalBodyPart::UpdateJoint` (`0x8260B0F8`): hinge integration, contact
  resolution and rotation about the deformed anchor.
- `DeformableObject::UpdateLocator` (`0x825E0EC8`) and the player render-data
  transfer (`0x822E8E54..0x822E9164`): local/world transforms, skin rows and
  physical-panel events.

No new spatial-deformation discrepancy was established in these comparisons.
The interpolation discrepancy belongs to the PC code added around that path.

## Verification

- `python b5-decomp/tests/run_render_part_interpolation.py`: compiles the
  production presentation methods. The new two-influence vertex/panel-anchor
  test fails before the fix (12.200001 versus 12.000000 at alpha zero), and
  passes afterwards across five alphas. It also checks all 128 rows, all four
  components, repeated render passes, unwritten ticks, repair and slot reuse.
- `python tools/re/ik_rest_identity.py build/game/VEHICLES`: 429 available
  deformation resources, 41,360 tags, worst two-sensor rest error 4.172e-7 m;
  no invalid sensor indices or weight pairs.
- `python tools/assets/bundles/vehicle_skin_audit.py --car PUSMC01 --rows
  --tags --selftest`: the car's 45,613 vertices retain the original paired
  indices/weights; the 430-car row/tag checks pass. Corrupted-input controls
  fail as expected.
- Focused faithfulness lint: no new findings. `build.cmd exe --jobs 4` passed:
  53 affected translation units rebuilt, 2,601 up to date, link successful,
  no warnings or errors.
- Fresh-eyes review passed the production call order, initialization, repair,
  repeated-pass/unwritten-tick handling and alpha-1 preservation.

This audit uses source, assembly, asset checks and executable regression tests;
it does not claim a fresh visual comparison with console gameplay.
