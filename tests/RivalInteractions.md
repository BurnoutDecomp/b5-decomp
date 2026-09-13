# Rival interaction regressions

Run these from the parent workflow checkout with its MSVC configuration available:

```powershell
python b5-decomp/tests/run_rival_impacts.py
python b5-decomp/tests/run_collision_culling.py
python b5-decomp/tests/run_rival_organic.py
```

The first suite runs 28 checks against production impact classifiers and sensor output.
It covers rear and side contact geometry, attacker attribution, steering, boost,
cooldowns, network-car guards, head-on momentum, stationary targets, domino takedowns,
crashing-car contacts, and sensor record ordering. The second runs 14 checks against
production collision-audio culling bodies, using real collision records and a small
fixture for the sound-state pool. It covers overflow, distance, duplicate strength,
pair ordering, compaction, and suppression of sounds already playing.

The organic case requires a built game and mounted data. Its driver uses the existing
pad-input events and observed rival positions to steer and boost into a Road Rage
pack. It does not inject collisions, crashes, takedowns, or AI aggression. Each run
saves its steering decisions, game log, and report under
`scratch/bugtest/runs/rival_organic/`. The checks require player credit, the HUD
message, AI notification, and takedown camera, with no new assertions or exceptions.
This is a live driving scenario: frame timing can change which collision occurs.

The restored deformation producer is ARTIST `0x8260A508`, reached by
`DeformableObject::UpdateOutputContactSpies` at `0x826251E8`. The audio queue's
emergency culling is `0x826D3CF0`, with duplicate removal at `0x826A00C8`, playing
sound suppression at `0x826BE5E0`, and compaction at `0x826BE910`.

## Rival recovery and camera regression

```powershell
python b5-decomp/tests/run_rival_recovery_camera.py
powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/RivalCamera.ps1
```

The 36 focused checks exercise production recovery geometry, recovery-section selection,
route and wrong-way handling, attachment tracking, spherical interpolation, frustum
prediction, and speed/boost response. Fixtures provide the road network and vehicle
snapshots; the recovered algorithms are extracted unchanged from the production files.

The camera case uses a forced takedown to select the rig deterministically. In addition
to the existing HUD, AI and state-transition checks, it requires multiple camera poses
and nonzero rotation sampled from an authored shake take. Frame captures permit visual
inspection of the cut and return to driving. It does not establish organic collision
attribution; use `run_rival_organic.py` for that separate check.

## Visible rival damage

```powershell
python b5-decomp/tests/run_rival_damage.py
python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/RivalDamage.ps1 --run-name rival_damage
```

The 13 focused checks cover damage-model activation on player takedowns, the five
active damaged-car budget, AI/network ownership, boost rewards and penalties, and
victim lifecycle flags. They execute the production takedown consumers with
controlled vehicle state.

The driving case uses real collisions and links player credit to the same victim's
crash state, enabled damage rendering, nonzero deformation and changing pose.
It also saves frames for visual inspection; a HUD message alone cannot pass it.
The relevant ARTIST paths are `UpdateBoost` at `0x82304BF0..0x82304C90`,
`GetDamagedCarCount` at `0x822A4958`, and `ProcessTakedownEvents` at `0x822F6CF8`.
