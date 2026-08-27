// ============================================================================
// BrnWorld::RaceCarEntityModule -- THE GAME-MODE ARMING SLICE.
//
//   SetAllActiveCarsInGameMode   X360 0x822BE0A0
//
// ⭐ WHY THIS SLICE EXISTS. `RaceCarEntityModule::mbIsInGameMode` (+99140) is the byte the
// whole free-roam gameplay stage turns on: AttachActiveRaceCar @0x822F4DB0 copies it into
// every ActiveRaceCar (`stb r11,0x777(r31)`), and PostSceneUpdate / PostPhysicsUpdate /
// UpdateTrafficAndRaceCarNearMisses / UpdateOutputInterfaces / PlaceRaceCarOnLoad /
// IsRaceCarWrappable / SetupCarColour all early-out while it is false. In the ARTIST image
// it has exactly ONE setter -- HandlePrepareForModeAction @0x823092F0 (`*(a1+99140) = 1`) --
// and one clearer, HandleStopModeAction @0x82307A30. Both hang off game ACTION 23
// (PrepareForModeAction), whose producer is ModeManager::PrepareForMode @0x82342930.
//
// This TU is the leaf end of that chain: the tail call HandlePrepareForModeAction makes
// once the module-level flags are set.
//
// SOURCE: BURNOUT_X360_ARTIST.XEX, raw asm (0x822BE0A0..0x822BE230). The pseudocode for
// this function is misleading -- it reports `IsAttached()` as the loop's `result` and hides
// the inlined RaceCar::SetInCurrentGameMode -- so every claim below is asm-attested.
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint ([scoring-map] diag)

namespace BrnWorld
{

// ----------------------------------------------------------------------------
// HandlePrepareForModeAction @0x823092F0 -- common non-Showtime boost spine.
//
// The full ARTIST function also rebuilds online grids/opponents, publishes an
// AI-control event, configures CrashPlay and copies unrelated mode flags. Those
// branches remain outside this BoostManager pass. The state below is the exact
// subset that selects/preserves the boost strategy, seeds its mode bar, gates
// earning until START_PLAYING_MODE, and arms attached cars. Showtime's
// KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR branch is deliberately not implemented.
// ----------------------------------------------------------------------------
void RaceCarEntityModule::HandlePrepareForModeAction(
    const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPFMAction,
    RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput)
{
    CGS_ASSERT(lpPFMAction != 0, "lpPFMAction != NULL");
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");

    const BrnGameState::GameModeParams* lpGameModeParams =
        lpPFMAction->GetGameModeParams();
    CGS_ASSERT(lpGameModeParams != 0, "lpGameModeParams != NULL");

    ClearAllActiveRaceCarToPlayerScoringMappings();

    if (mbIsInGameMode)
    {
        CGS_ASSERT(mbIsInOnlineGameMode == lpGameModeParams->mbIsOnline,
                   "mbIsInOnlineGameMode == lpGameModeParams->mbIsOnline");
    }
    else
    {
        mbIsInOnlineGameMode = lpGameModeParams->mbIsOnline;
    }

    // ARTIST 0x82309390..0x823093F8: online boost type 1/2/3 maps to
    // Burnout 2/3/5, then the saved +0x454/+0x458 car stats are re-applied.
    if (mbIsInOnlineGameMode)
    {
        switch (static_cast<s32>(lpGameModeParams->meOnlineBoostStrategy))
        {
        case 1:
            mBoostManager.SetBoostStrategy(BoostManager::E_BOOSTSTRATEGY_BURNOUT2);
            mBoostManager.ApplyPreviousCarStats();
            break;
        case 2:
            mBoostManager.SetBoostStrategy(BoostManager::E_BOOSTSTRATEGY_BURNOUT3);
            mBoostManager.ApplyPreviousCarStats();
            break;
        case 3:
            mBoostManager.SetBoostStrategy(BoostManager::E_BOOSTSTRATEGY_BURNOUT5);
            mBoostManager.ApplyPreviousCarStats();
            break;
        default:
            break;
        }
    }

    mbCarSelectAllowedInGameMode =
        lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_CAR_SELECT_ALLOWED);
    meGameModeType = lpGameModeParams->GetGameModeType();
    mbIsInGameMode = true;

    // ARTIST 0x8230995C..0x8230997C. The manager wrapper supplies the B2 flag
    // from its selected strategy id.
    mBoostManager.OnModeStart(meGameModeType);

    if (!lpGameModeParams->GetFlag(
            BrnGameState::GameModeParams::KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR))
    {
        // ARTIST 0x823099D8..0x823099F0: non-Showtime modes cannot earn until
        // game action 34 (START_PLAYING_MODE) reenables it.
        mBoostManager.SetBoostEarningEnabled(false);
    }

    // ========================================================================================
    // ⭐⭐⭐ [stuntrace frontier round 3, 2026-08-27] THE PLAYER-SCORING-SLOT MAP -- the
    // extracted tail of the console's OFFLINE opponents arm, at its own position.
    //
    // ⛔ WHY IT IS HERE AND WHY IT IS ONE STATEMENT. The console reaches it through two
    // functions this tree does not body:
    //     0x82309888  bl SetupOpponents                          (@0x82307DF0)
    //       0x82307E60  bl SetUpPlayerCarForMode                 (@0x823058F8)
    //         0x82305DD4  bl SetActiveRaceCarForPlayerScoringIndex   <-- THIS STATEMENT
    // SetupOpponents' other legs (RemoveRivals, SetUpAIForMode, SetAllCarsOnStartLine) and
    // SetUpPlayerCarForMode's own body (the start-grid respawn / place-on-track arm, the
    // colour-palette asserts, the AI AddCarToCurrentModeEvent) are NOT reproduced here --
    // they need the pre-scene output buffer and the AI interfaces this call site does not
    // hold. Only the mapping store is lifted, and it is lifted because it is the ONE leg of
    // that chain with a live downstream consumer on this build.
    //
    // ⭐ THE STORE IS UNCONDITIONAL INSIDE SetUpPlayerCarForMode. Its whole body sits behind
    // `if (mxGameModeFlags & KU_FLAG_SET_CARS_TO_START_GRID)` (asm 0x82305950..0x82305970,
    // `beq cr6, loc_82305DBC`), and loc_82305DBC IS the mapping block -- so the flag skips
    // the respawn, never the map.
    //
    // ⭐ THE SCORING SLOT IS miNumRivals, and that is asm, not inference (0x82305DBC..0x82305DD4):
    //     lbz   r10, 0(r29)            ; r29 == lpGameModeParams; the BYTE at +0 == miNumRivals
    //     extsb r4, r10                ; -> the EPlayerScoringIndex argument
    //     lwzx  r5, r28, 0x182F8       ; mePlayerActiveRaceCarIndex
    //     bl    SetActiveRaceCarForPlayerScoringIndex
    // GameModeParams+0 is miNumRivals and +1 is miNumNetworkPlayers -- the same two bytes
    // ModeManager::PrepareForGameMode reads as `lbz 0(r29)` / `lbz 1(r29)` to size its player
    // loop (BrnModeManager_Prepare.cpp:583). The player is therefore the LAST grid slot: with N
    // rivals the roster's AddPlayer() walk hands scoring slots 0..N-1 to the rivals and slot N
    // to the player. StuntAttackMode::Start sets miNumRivals to 0 (BrnStuntAttackMode.cpp:249,
    // `stb r23, 0(r31)`), so an offline stunt run maps player scoring slot 0.
    //
    // ⭐ WHAT IT FIXES (run scratch/flow_run/20260827_140514): with this map never written,
    // RaceCarEntityModule::CopyActiveRaceCarToPlayerScoringMappingToOutput publishes the
    // all-sentinel table ClearAllActiveRaceCarToPlayerScoringMappings left three statements
    // above, ModeManager's binding sweep skips every slot, ScoringSystem::maCarData[0] keeps
    // the E_ACTIVE_RACE_CAR_INDEX_INVALID that ScoringSystem::AddPlayer stamped, and
    // ScoringSystem::GetCarData returns NULL at the end of the mode:
    //     [ASSERT 31113] lpCarData (BrnScoringSystem_Timer.cpp:341)
    //     [EXCEPTION] ACCESS_VIOLATION reading 0x18  StopModeTimer + 0xE5
    //         <- FinishCurrentMode + 0x3FC <- ModeManager::PreWorldUpdate
    // -- assert-is-not-a-guard again: StopModeTimer's `lpCarData != NULL` tripwire fires and
    // falls straight through into lpCarData->GetScoreData()->GetDistanceToFinishLive(), which
    // is CarScoreData +0x18 off a null CarData. This is the first of that chain's three
    // missing producers (the other two are in RaceCarEntityModule::PostPhysicsUpdate and
    // GameStateModule::PostWorldUpdateStuntBringUp).
    //
    // [FLAG PC bring-up] THE ONLINE/OFFLINE FORK IS NOT REPRODUCED -- ON PURPOSE, and the
    // reason is already on the record. The console picks between the network grid loop
    // (AddRaceCarToStartingGridOrFreeburnLobby, 0x823097E4/0x8230985C) and this offline
    // SetupOpponents arm on `lbz r11, 0x94(r25)` @0x82309670 -- the SAME unnamed
    // GameModeParams+0x94 byte BrnModeManager_Prepare.cpp's "THE SECOND-PHASE CACHE" banner
    // refuses to name (laying the DWARF member run between the two asm-pinned anchors comes up
    // 24 bytes short, so +0x94 lands inside a float under every consistent reading). That
    // banner's finding applies unchanged here: GameModeParams::Construct zeroes the block, so
    // on the whole offline campaign path the byte is zero and the offline arm is the one the
    // console takes. Taking it unconditionally therefore diverges only on the online paths,
    // which are parked wholesale.
    // DELETE-WHEN GameModeParams+0x94 is a named member: restore the fork.
    //
    // The mode-type half of the console's outer gate (`lwz r11, 0x148(r25)` @0x8230962C, the
    // 15/16 pair, `bne -> loc_8230988C` skips the whole grid/opponents block) IS reproduced --
    // it reads a member this tree does have.
    // ========================================================================================
    if (meGameModeType != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
        meGameModeType != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        SetActiveRaceCarForPlayerScoringIndex(
            static_cast<BrnGameState::GameStateModuleIO::EPlayerScoringIndex>(
                lpGameModeParams->miNumRivals),
            mePlayerActiveRaceCarIndex);

        // [DIAG] NOT IN THE X360 BINARY -- one line per prepare-for-mode, the proof rung for
        // the three-leg chain above. Pairs with the "[scoring-bind]" rung in
        // GameStateModule::PostWorldUpdateStuntBringUp: this one says what the world module
        // mapped, that one says what the scoring system bound.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[scoring-map] prepare-for-mode: player scoring slot "
                << static_cast<s32>(lpGameModeParams->miNumRivals)
                << " -> active race car " << static_cast<s32>(mePlayerActiveRaceCarIndex)
                << " (mode " << static_cast<s32>(meGameModeType) << ")\n";
        }
    }

    SetAllActiveCarsInGameMode(mbCarSelectAllowedInGameMode);
}

// ----------------------------------------------------------------------------
// SetAllActiveCarsInGameMode @ 0x822BE0A0.
//
// Walk all eight active-race-car slots (r30 = this + 0x1A60 == &maActiveRaceCars[0],
// stride 0x1CD0 == sizeof(ActiveRaceCar) == 7376, `li r21,8` iterations) and put every
// ATTACHED slot's global RaceCar into the current game mode.
//
// ⚠️ THE SECOND ARGUMENT IS NOT THE ONLINE FLAG. The console's only caller,
// HandlePrepareForModeAction @0x823092F0, passes `*v13` where `v13 = (a1 + 99143)` ==
// mbCarSelectAllowedInGameMode -- the same byte HandleSetupNetworkCarAction @0x82305688
// hands to SetInCurrentGameMode as its `lbCarSelectAllowed`. Naming it "online" here would
// have quietly swapped two adjacent module bytes (+99141 IS the online flag).
//
// The X360 INLINES RaceCar::SetInCurrentGameMode @0x822B3F08 rather than calling it:
//     lwz r31, 0x6F0(r30)      ; r31 = GetGlobalRaceCar()
//     lbz r11, 0xA4(r31) ...   ; assert muType < E_RACE_CAR_TYPE_COUNT   (BrnRaceCar.h:547)
//     lbz r11, 0xA4(r31) ...   ; assert muType != 3 == IsInWorld()       (BrnRaceCar.h:713)
//     stb r22, 0xA6(r31)       ; mbIsInGameMode               = 1  <-- r22 is the LITERAL 1
//     stb r20, 0xA7(r31)       ; mbCarSelectAllowedInGameMode = a2
// so the in-game flag is a hard-coded true at this site and the argument only reaches the
// car-select-allowed byte. Calling the real (reconstructed, matching) member expresses the
// same stores; the console's `lbInGameMode == true` arm is the one taken, and its
// `!lbInGameMode && mpActiveRaceCar == NULL` index-release arm is dead here by construction.
//
// The trailing range assert is `GetActiveRaceCarIndex()` bounds-checked. The asm tests it
// TWICE -- unsigned `cmplwi r11,0x80` (catches the -1 sentinel, whose byte is 0xFF) then
// sign-extended `cmpwi r11,8` -- which together are exactly "index in [0, 8)".
// ----------------------------------------------------------------------------
void RaceCarEntityModule::SetAllActiveCarsInGameMode(bool lbCarSelectAllowedInGameMode)
{
    for (s32 liActiveRaceCars = 0;
         liActiveRaceCars < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         ++liActiveRaceCars)
    {
        ActiveRaceCar& lrActiveRaceCar = maActiveRaceCars[liActiveRaceCars];

        // asm 0x822BE0F4..0x822BE104: the whole body is skipped for a detached slot.
        if (!lrActiveRaceCar.IsAttached())
        {
            continue;
        }

        // asm 0x822BE108..0x822BE134 -- GetGlobalRaceCar()'s own IsAttached() tripwire
        // (BrnActiveRaceCar.h:1089), re-emitted by the inliner at each of its three reads.
        CGS_ASSERT(lrActiveRaceCar.IsAttached(), "IsAttached()");

        RaceCar* lpGlobalRaceCar = lrActiveRaceCar.GetGlobalRaceCar();

        lpGlobalRaceCar->SetInCurrentGameMode(true, lbCarSelectAllowedInGameMode);

        CGS_ASSERT(lrActiveRaceCar.IsAttached(), "IsAttached()");

        CGS_ASSERT(lpGlobalRaceCar->GetActiveRaceCarIndex() >= E_ACTIVE_RACE_CAR_INDEX_0 &&
                       lpGlobalRaceCar->GetActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "( maActiveRaceCars[liActiveRaceCars].GetGlobalRaceCar()->"
                   "GetActiveRaceCarIndex() >= E_ACTIVE_RACE_CAR_INDEX_0) && "
                   "(maActiveRaceCars[liActiveRaceCars].GetGlobalRaceCar()->"
                   "GetActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT )");
    }
}


// ----------------------------------------------------------------------------
// SetAllCarsOnStartLine @ 0x822A4850.
//
// Put every ATTACHED active-race-car slot into leRaceStartState and clear its start-line
// boost bookkeeping. SetupOpponents @0x82307DF0 is the caller reached on the OFFLINE arm of
// HandlePrepareForModeAction, with lbIncludePlayer == true at both of its call sites.
//
// The player-slot skip is `if (liIndex != mePlayerActiveRaceCarIndex || lbIncludePlayer)`
// -- asm 0x822A48C0..0x822A48D4 reads the player index through the module offset 0x182F8
// (r24 = 0x182F8, `lwzx r11,r29,r24`) and falls through to the store block when the two
// differ OR the flag is set. So the flag only ever ADDS the player's own slot.
//
// The three stores (asm 0x822A4910..0x822A4918), all by name:
//     stfs f31, 0x734(r31)   mfTimeToStartLineBoostChange = -1.0f  (flt_820037C8, dumped)
//     stw  r23, 0x77C(r31)   meRaceStartState             = leRaceStartState
//     stb  r22, 0x780(r31)   mbIsDoingStartLineBoost      = false  (r22 is the literal 0)
//
// ⚠️ THE LOOP BOUND IS DELIBERATELY OFF-BY-ONE-LOOKING. The console increments FIRST, then
// asserts `liIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT` (BurnoutConstants.h:39 -- the
// range-guarded EActiveRaceCarIndex post-increment's own tripwire, which permits reaching
// the one-past-the-end value) and only then tests `< 8` to continue. The body therefore
// runs for indices 0..7 and the assert never fires. Written as a plain 0..8 loop here; the
// assert is the enum increment's, not this function's, so it is not re-emitted.
// ----------------------------------------------------------------------------
void RaceCarEntityModule::SetAllCarsOnStartLine(ActiveRaceCar::ERaceStartState leRaceStartState,
                                                bool lbIncludePlayer)
{
    for (s32 liActiveRaceCarIndex = 0;
         liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         ++liActiveRaceCarIndex)
    {
        const EActiveRaceCarIndex leActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(liActiveRaceCarIndex);

        if (!GetActiveRaceCar(leActiveRaceCarIndex)->IsAttached())
        {
            continue;
        }

        if (leActiveRaceCarIndex == mePlayerActiveRaceCarIndex && !lbIncludePlayer)
        {
            continue;
        }

        ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar(leActiveRaceCarIndex);

        CGS_ASSERT(lpActiveRaceCar->IsAttached(), "IsAttached()");

        lpActiveRaceCar->SetOnStartLine(leRaceStartState);
    }
}

} // namespace BrnWorld
