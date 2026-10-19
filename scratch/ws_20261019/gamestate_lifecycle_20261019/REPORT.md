# GameState lifecycle constructor recovery

Branch: `codex/debug-menu-gamestate-lifecycle-20261019`  
Base: `2aadf4362005479c23a6c8b7c8d245ad8c5b7a54`

## Source result

This source-only batch restores three attested pieces of the GameState constructor chain:

- `OnlineCarSelectManager::Construct(this, &mProgressionManager)` at the X360 call site `0x823805F0`.
- `OfflineFlybyManager::Construct(this, mModeManager.GetScoringSystem())` at the X360 inlined flyby call `0x82380628`; its typed owner body calls the base constructor and clears the two-bit message set matching the inline store at `0x82380634`.
- `AchievementManagerX360::Construct` now delegates to the existing base constructor, then reproduces its X360-specific inline tail from `0x82380740..0x82380764`: user index `-1`, state `0`, null enumerator, `CgsXOverlapped::Construct`, and zeroed earned/queued bit sets. ARTIST does not store `mcbBuffer` in this sequence, so the owner method leaves it alone.

The source-level caller order now matches the relevant ARTIST sequence after Rumble: CarSelect, OnlineCarSelect, Stunt, OfflineFlyby, OnlineFlyby. The embedded OfflineFlybyManager remains an in-module object; the online manager keeps the existing stable pointer adaptation. The GameStateModule header's stale note claiming OnlineCarSelectManager is unconstructed and unmounted was removed.

Owning declarations used for this comparison are `src/GameSource/GameState/BrnGameStateModule.h`, `src/GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h`, `src/GameSource/GameState/FlybyManager/BrnGameStateOfflineFlybyManager.h`, `src/GameSource/GameState/FlybyManager/BrnGameStateFlybyManager.h`, and `src/GameSource/GameState/AchievementManager/X360/BrnGameStateAchievementManagerX360.h`. The still-unselected owner declaration is `src/GameSource/GameState/ModeManager/BrnModeManager.h`; its scorer alias is declared in `src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h`.

## Complete `GameStateModule::Construct` call comparison

I compared the direct calls in the saved ARTIST `0x82380388` assembly through `ClearData` at `0x823807A8` with the existing typed source and its exact owner helpers. The table groups only calls that the same typed owner initializes.

| ARTIST address(es) | Original operation | Current source owner / disposition |
| --- | --- | --- |
| `0x823803B8`, `0x823803DC`, `0x823803EC` | Module base, reset debug component, GameState debug component | `GameStateModule::Construct`, unchanged. |
| `0x82380434`, `0x82380444`, `0x8238044C` | Takedown debug registration, Mugshot, Payback | `ConstructTakedownBringUp`; the takedown owner calls its debug component's typed `Construct`/`Register`, then constructs both pointer-adapted managers. |
| `0x82380458..0x82380478` | Invite event queue and four input/bind queues | `GameStateInviteManager::Construct`, called from the module; this owner constructs its five queues. |
| `0x823804DC` | Full `ModeManager::Construct` | Still intentionally not selected. `ConstructInterModeStateBringUp` remains the PC partial path; see the ABI dependency below. |
| `0x8238050C..0x82380578` | Progression, receiver-queue clear, TriggerQuery, RoadRules, DriveThru | Existing typed calls in `GameStateModule::Construct`; `mReceiverQueue.Construct()` owns its setup/clear. |
| `0x823805C8..0x8238060C` | Rumble, CarSelect, OnlineCarSelect, Stunt | Existing calls plus the restored OnlineCarSelect call at `0x823805F0`, with the original argument order. |
| `0x82380628`, `0x82380644` | Offline then Online Flyby base construction | Offline call restored. The OfflineFlyby owner method reproduces its base stores/random setup and message-set clear; the existing OnlineFlyby call follows it. |
| `0x8238065C` | ContactSpy handle construction | Existing direct call in `GameStateModule::Construct`. |
| `0x82380668..0x823806D0` | Crash/takedown/physical-traffic/impact queues, cached game-event queue, traffic response, GUI queue | Existing typed owners: `TakedownPostWorldCache::Construct` constructs the event queues and its `VehicleOutputInterface`; that interface constructs its traffic, impact and game-event queues. The module carry queue is direct, and `ConstructTakedownBringUp` constructs the output GUI queue. |
| `0x823806FC..0x82380720` | XUserSetContext monitor, presence thread, RichPresence base | Existing source operations in `GameStateModule::Construct`. |
| `0x82380738..0x82380764` | Achievement base, X360-owned inline initialization, overlapped record | Now covered by the typed `AchievementManagerX360::Construct` owner method, in ARTIST order. |
| `0x82380768..0x823807A8` | Street, image manager, DeveloperChallenge, ClearData | Existing typed calls in `GameStateModule::Construct`. |
| `0x823809DC..0x82380C68`, `0x82380CF8..0x82380EC8` | Remaining performance monitors and their assertions | Existing monitor registrations and checks in the module constructor. |

The exact call instruction list and source snapshots are frozen beside the report. `ModeManager::Construct` is the only original constructor body still represented by a PC partial substitute in this comparison; the later monitor/assert tail is already present.

## Remaining lifecycle gates

The `ModeManager::Construct` signature's final input is an achievement-manager pointer. ARTIST `GameStateModule::Construct` places the address of its X360 `AchievementManagerX360` member in the stack argument for the call at `0x823804DC`; ARTIST `ModeManager::Construct` then passes the input to `ScoringSystem::Construct` at `0x8234033C`. The source `StuntModeScoring::AchievementManager` alias still names the PS3 concrete type from DecFIGS, while this GameState module owns the X360 concrete type. No sibling cast or caller switch was introduced. Root will make the alias correction as a separate source follow-on; the full ModeManager constructor remains uncalled in this batch.

The recovered `GameStateModule::Prepare` body contains the original offline flyby Prepare at stage 18, but the selected resource-thread route still uses `PrepareNativePC`; that adapter does not run the original full 28-stage body. The recovered `PreWorldUpdate` body is likewise not the selected route: `BrnGameModule` still calls `DoUpdate_GameStatePreWorldNativePC`. The pre-existing activation handoff records concrete omitted consumers in that partial path, including the full landmark-debug and flyby update legs. This batch does not switch either caller or claim lifecycle activation readiness.

## Evidence and verification

- Behavioral authority: frozen ARTIST exports `0x82380388`, `0x823565C0`, and `0x82340008` under `ARTIST/`; the first contains the inline flyby and achievement-owner sequence, the second is the OnlineCarSelect constructor, and the third documents the unresolved ModeManager path. In `0x82380388`, the stack argument prepared at `0x823804B4` carries the address of the X360 achievement member to `ModeManager::Construct` at `0x823804DC`; `0x82340008` forwards that argument to `ScoringSystem::Construct` at `0x8234033C`.
- Declaration shape: frozen DecFIGS headers under `DecFIGS/`, including GameStateModule, OnlineCarSelectManager, OfflineFlybyManager/FlybyManager, ModeManager, and StuntModeScoring.
- Exact edited-file preimages: `preimage/`, keyed to baseline Git blobs in `manifest.json`. Existing owner-helper snapshots are in `source/`.
- The frozen 25-row GameState packet remains untouched. The existing GlobalGraphics allocator provider was reused as context only; no provider source was duplicated.
- No compile, test, native game/helper, IDA/export, ledger, or caller activation was run. This checkpoint is source-ready for Root's combined review, not compile- or native-verified.
