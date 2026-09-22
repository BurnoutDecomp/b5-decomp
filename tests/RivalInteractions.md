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

The driving case also requires the credited victim to crash on the AI_CRASHING (1) or
SHUTDOWN (3) absorption set and never on NORMAL (0), read from the `[absorb]` witness that
`BRN_CRASH_RESPONSE_DIAG=1` arms. The selector is `DeformableObject::UpdateAbsorptionSet` at
ARTIST `0x825DF9A0` (PS3 `0x6BEDEC`): a non-player driver type with a crashing race car takes
set 1, or set 3 plus ten forced hinges in `E_MODE_NONE`; a player takes PLAYER_EXTREME_CRASH
only when the crash speed is within `KVF_SPEED_BELOW_MAX_FOR_EXTREME_DEFORMATION` (5.0) of the
car's top speed. Until 2026-09-21 all three vehicle reads were pinned and every car crashed on
set 0, so victims absorbed on the 80 mph row instead of the 30 mph row: less deformation, and
the unabsorbed impulse launched the car. Measured on `rival_damage_absorb` (same recipe, same
exe apart from the fix): before, 59/59 samples of the credited victim on set 0 and every
crashing race car on set 0; after, 0 race-car samples on set 0 while AI-driven, the credited
victims on set 1 (56 and 98 samples), and the victims' summed sensor displacement squared
0.155 before against 0.21 and 0.41 after (different collisions; the set is the witness, the
displacement is the direction).

## Road steering and wall regression

```powershell
python b5-decomp/tests/run_rival_road_steering.py
powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/RivalRoad.ps1
```

The 32 geometry checks cover fan headings in every quadrant, left/right ordering,
road targets, XZ projection, translation/height invariance and road-direction penalties.
They execute production methods with real AI headers. Before the correction, 22 failed.

The live case uses ordinary player acceleration through the Waterfront Road Rage
start. At least four rivals must drive south into the bend and leave west through
the carriageway at speed. Placement jumps cannot complete the traversal. The
baseline completed zero traversals; the corrected capture completed all five.
Frames and `BRN_AI_ROAD_DIAG` records preserve the actual targets and trajectories.

ARTIST `GenerateFanVectors` at `0x827792C0` stores `{sin(angle), cos(angle)}`:
the shuffle bytes at `0x82CDA3C0` and instructions `0x827796C0..0x827796C8`
pin the sign and lane order. `IncludeCentreLineTracking` flattens world XZ at
`0x82786C3C/0x82786C4C` before subtracting the planar road centre.

## Rival impact events reach the game state

```powershell
python b5-decomp/tests/run_bridge_physics_to_output.py
python b5-decomp/tests/run_rival_organic.py --case b5-decomp/tests/RivalImpactEvents.ps1 --run-name rival_impact_events
```

`WorldModule::BridgePhysicsToOutput` (ARTIST `0x827AEB18`) has six legs. Legs 4-6
(`0x827AEBBC..0x827AEC00`) were parked on PC: the deformation output copy, the append of the
physics game-event queue (`VehicleOutputInterface+0x65F0`) into the world game-event queue, and the
prop update notifications. Leg 5 is the only route by which world event 31 (VEHICLE_IMPACT, posted by
`HandleRaceCarRaceCarContact`) reaches `SendVehicleImpactMessages` (aggressor boost award action 53,
victim action 54), GUI 365 (the TRADING PAINT / NUDGE / SLAM / SHUNT hint) and the impact rumble.
`UpdateOutputBuffer::Construct` must construct the world deformation interface first (`0x827CA2C4`).
The structural check fails 3/10 on the pre-fix bridge; the live case failed with 1 classified
impact and 0 deliveries before the fix and passed with 13 deliveries after it.
