# X360 stunt-scorer achievement alias

Branch: `codex/debug-menu-gamestate-lifecycle-20261019`  
Baseline: `4d2bb2e60` (the pushed constructor checkpoint)

## Change

`BrnGameState::StuntModeScoring::AchievementManager` keeps its recovered alias name and now names the pointer-only forward declaration `BrnGameState::AchievementManagerX360`. The previous alias named `AchievementManagerPS3`, copied from the DecFIGS declaration. ARTIST is the behavioral target here and the owning GameState module has the X360 concrete object. The header remains independent of the GameStateModule include graph.

Only `src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h` changes. The shared-base `GameStateModule::GetAchievementManager()` API is untouched. No sibling cast, caller switch, or restored `OnStuntRunMultiplier` body was added.

## ARTIST argument chain

The saved exports establish the pointer type and propagation:

1. In `GameStateModule::Construct` (`../ARTIST/0x82380388.json`), `r31` is the module. At `0x8238047C..0x82380488`, `r27` is formed as module+`0x2C5B0`, the X360 achievement-manager member. `r27` is stored into the stack argument slot at `0x823804B4` before the call to `ModeManager::Construct` at `0x823804DC`. This is the eighth explicit argument: the first seven are supplied in `r4..r10` alongside the `this` pointer in `r3`.
2. In `ModeManager::Construct` (`../ARTIST/0x82340008.json`), `0x823402B0` loads that stack argument into `r4`. The function sets `r3` to its embedded ScoringSystem at `0x823402F8` and calls `ScoringSystem::Construct` at `0x8234033C` without overwriting `r4`.
3. In `ScoringSystem::Construct` (`ARTIST/0x82337FE0.json`), `0x82337FFC` saves the input pointer in `r28`. It places that same pointer in `r4` for the offline stunt-scorer virtual call at `0x823380D8`, then again at `0x823380E4` for the online stunt-scorer call at `0x823380F0`.

The current typed source preserves this flow: `ModeManager::Construct` forwards the argument to `mScoringSystem.Construct`, and `ScoringSystem::Construct` passes it to both stunt scorers. `StuntModeScoring::Construct` stores the pointer in `mpAchievementManager`; the alias correction does not invent additional behavior.

## Scope and readiness

The pointer type is now aligned with the X360 owner and the original register/stack path. The full `ModeManager::Construct` caller is still intentionally unselected; this follow-on corrects declaration shape only. It does not claim the GameState lifecycle is active or native-verified. No compile, test, native game/helper, IDA export, or caller activation was run.

The exact header preimage, postimage, ARTIST `ScoringSystem::Construct` export, and DecFIGS declaration are frozen in this directory. The two upstream call-site exports and source snapshots are referenced from the parent lifecycle packet.
