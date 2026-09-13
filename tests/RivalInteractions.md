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
