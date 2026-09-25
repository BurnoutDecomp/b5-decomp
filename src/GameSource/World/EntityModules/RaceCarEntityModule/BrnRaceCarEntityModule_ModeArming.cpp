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
// [stuntrace start-grid wave] SetupOpponents / SetUpPlayerCarForMode reach these by name.
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // GameModeParams / StartLocation / CheckpointData
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h" // OutputBuffer_PreScene::GetRaceCarAIInterface
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"          // RaceCarAIInterface / AddCarToCurrentModeEvent / E_EVENT_ADD_CAR_TO_MODE
#include "GameSource/Math/BrnMathUtils.h"                                 // BrnMath::BuildTransform
#include "SharedClasses/World/BrnWorldRegion.h"                           // BrnWorld::E_DISTRICT_INVALID
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.h"  // GetBoostAmount / GetMaxBoost
#include "rw/math/fpu/scalar_operation.h"                                 // rw::math::fpu::IsZero
#include "rw/math/vpu/vector3_operation.h"                                // rw::math::vpu::Add (the grid look-at target)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                 // BrnDirector::Camera::Utils::CreateLookAt
#include "SharedClasses/DataLists/VehicleList.h"                          // VehicleList::GetVehicleIndex / GetVehicleData
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // VehicleListEntry::GetCarType / GetLiveryType
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"                // GlobalColourPalette (the colour assert)
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"            // [net] witness lines (PC harness)

namespace BrnWorld
{

// --------------------------------------------------------------------------------------------
// [stuntrace start-grid wave 2026-08-27] The three literals SetupOpponents / SetUpPlayerCarForMode
// carry, all X360-attested at their use sites rather than named constants in the console source.
// --------------------------------------------------------------------------------------------

// `li r31, 0x7FFF` @0x82307EB8 (SetupOpponents' no-checkpoint fallback) and `li r5, 0x7FFF`
// @0x82305D70 (SetUpOutOfRangeRaceCar's section argument). The same 32767 the [rot-ring]
// diagnostic prints as the player's aiSection while no AI section is resolved.
static const u16 KU_NO_AI_SECTION = 0x7FFFu;

// `li r4, 0` at both StartLocation_8_::Ge call sites (0x82305AAC / 0x82305AC8 online,
// 0x82305BCC / 0x82305BE8 offline): the PLAYER always takes grid slot 0. ModeManager::
// SetStartingGrid seats the grid front-to-back, so slot 0 is pole.
static const s32 KI_PLAYER_START_GRID_SLOT = 0;

// `fmr f1, f30` @0x82305C04, where f30 was loaded from flt_82001CC0 == 0.0f at 0x82305AA0.
// The car arrives AT REST: PlaceOnTrackManager::PlaceCarOnTrack multiplies the reset direction
// by this speed, and VehiclePhysics::Reset re-seeds every motion register from that zero.
static const f32 KF_START_GRID_PLACE_ON_TRACK_SPEED = 0.0f;


// ----------------------------------------------------------------------------
// HandlePrepareForModeAction @0x823092F0 -- common non-Showtime boost spine.
//
// The full ARTIST function also rebuilds online grids/opponents and copies
// unrelated mode flags. Those
// branches remain outside this BoostManager pass. The state below is the exact
// subset that selects/preserves the boost strategy, seeds its mode bar, gates
// earning until START_PLAYING_MODE, and arms attached cars. Showtime's
// KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR branch is reproduced too (the showtime arm below,
// 0x82309980..0x823099D4).
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

    // ⭐ [stuntrace start-grid wave 2026-08-27] THE FLAG MIRROR -- landed here because without it
    // every GetGameModeFlag() reader on this module answers "no flag set, ever".
    //     0x82309480  ld   r11, 0x860(r25)          ; GameModeParams::muFlags
    //     0x82309484  stdx r11, r31, 0x18358        ; RaceCarEntityModule::mxGameModeFlags
    // One 64-bit copy, at its console position (immediately before the car-select-allowed byte
    // this function already writes). mxGameModeFlags had NO writer anywhere on this tree -- a
    // tree-wide grep, not an assumption -- so it read as zero for the whole process, which is
    // what made SetUpPlayerCarForMode's KU_FLAG_SET_CARS_TO_START_GRID gate unsatisfiable and
    // SetupOpponents' RemoveRivals gate always-false.
    // ⭐ [aiwave lane A9, 2026-09-03] THE PARK ON THE LINE BELOW IS RETIRED. It read:
    //   "[FLAG] the console's companion `stwx <miNumRivals>, r31, 0x18340` @0x8230947C is NOT
    //    reproduced: +0x18340 lands inside maTailPadA1b and this header names no member there.
    //    Nothing reconstructed reads it. DELETE-WHEN that word is named."
    // The word IS named now -- miOpponentCount, DWARF BrnRaceCarEntityModule.h:367, the only
    // int32 between mSurfaceList (:365) and mbIsInGameMode (:370 @+99140) and exactly the four
    // bytes in front of it (see the header's own banner). AND SOMETHING DOES READ IT:
    // PlaceRaceCarOnLoad @0x822CE588's start-grid arm divides by it
    // (`lwzx r11, r31, 0x18340` @0x822CE928 followed by `fdivs` @0x822CE944), so leaving it
    // unwritten would have been a DIVIDE BY ZERO on the arm this wave lands, not merely a
    // dropped store.
    //     0x82309464  lbz   r11, 0(r25)             ; GameModeParams+0 == the opponent count byte
    //     0x82309470  extsb r11, r11                ; sign-extended -- it is an s8 on the params
    //     0x8230947C  stwx  r11, r31, 0x18340       ; RaceCarEntityModule::miOpponentCount
    miOpponentCount = lpGameModeParams->GetOpponentCount();

    mxGameModeFlags = lpGameModeParams->GetFlags();

    mbCarSelectAllowedInGameMode =
        lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_CAR_SELECT_ALLOWED);

    // ⭐ [aiwave lane A9, 2026-09-03] THE THREE START-STYLE BYTES, at their console positions,
    // between the car-select byte above and the mode type below. All three are read by
    // PlaceRaceCarOnLoad @0x822CE588 -- they are what selects which of its five non-player arms
    // a freshly loaded RIVAL takes -- and none of them had a writer anywhere in this tree, so
    // every one of those arms would have answered with a zeroed module's "false".
    //   0x823094A4  ld     r11, 0x860(r25)          ; muFlags
    //   0x823094A8  rlwinm r11, r11, 0,17,17        ; bit 1 << 14 == KU_FLAG_AI_DRIVE_BY_START
    //   0x823094C4  stbx   r11, r31, 0x18350        ; mbSpawnAIBehindStartGrid
    //   0x823094CC  rlwinm r11, r11, 0, 4, 4        ; bit 1 << 27 == KU_FLAG_DONUT_START
    //   0x823094EC  stbx   r10, r31, 0x18352        ; mbPlayerDonutsOnEventStart
    //   0x823094F4  rlwinm r11, r11, 0, 5, 5        ; bit 1 << 26 == KU_FLAG_ROLLING_START
    //   0x8230950C..0x82309524                      ; rolling || (the donut byte just stored)
    //   0x82309534  stbx   r11, r31, 0x18351        ; mbPlayerRollsOnEventStart
    // ⚠ THE OR IS THE ASM'S, NOT A PARAPHRASE: 0x82309510 branches to the "true" arm on the
    // ROLLING bit, and 0x82309518 re-tests the byte stored at +0x18352 one instruction earlier,
    // so a DONUT start also counts as "the player rolls". Reproduced through the member rather
    // than re-reading the flag, because that is what the console reloads.
    mbSpawnAIBehindStartGrid =
        lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_AI_DRIVE_BY_START);
    mbPlayerDonutsOnEventStart =
        lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_DONUT_START);
    mbPlayerRollsOnEventStart =
        lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_ROLLING_START)
        || mbPlayerDonutsOnEventStart;

    // ⭐ [crash parity 2026-09-23, G68-D6] THE BASE-DEFORMATION STASH, interleaved by the console
    // with the mode-type store below:
    //     0x82309554  lwzx  r9, r31, 0x184D0   ; miPlayerBaseDeformationTypeMirror
    //     0x82309564  lfsx  f0, r31, 0x184D8   ; mfPlayerBaseDeformAmountMirror
    //     0x82309568  stwx  r9, r31, 0x184D4   ; -> miPlayerBaseDeformationTypeSaved
    //     0x8230956C  stwx  r10, r31, 0x18368  ; meGameModeType (below)
    //     0x82309570  stfsx f0, r31, 0x184E0   ; -> mfPlayerBaseDeformAmountSaved
    // The saved pair is what HandleStopModeAction pops back and what action 205's TOTALLED arm
    // restores; without this push both restored Construct's -1 / 0.0f.
    miPlayerBaseDeformationTypeSaved = miPlayerBaseDeformationTypeMirror;
    mfPlayerBaseDeformAmountSaved    = mfPlayerBaseDeformAmountMirror;

    meGameModeType = lpGameModeParams->GetGameModeType();
    // ARTIST 0x82309580..0x823095D8: modes which take the wheel must also
    // switch the player's AI routing state before the first driving update.
    if (lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL))
    {
        BrnAI::AIModuleIO::PlayerControlChangedEvent lChanged;
        lChanged.mbPlayerIsInControl = false;
        lpOutput->GetRaceCarAIInterface()->mManagementQueue.AddEvent(
            &lChanged, BrnAI::AIModuleIO::E_EVENT_PLAYER_TAKEN_OVER);
    }
    mbIsInGameMode = true;
    // The two stores/asserts the console runs straight after the in-game byte: the rival range
    // loop's gate is re-armed (action 34, START_PLAYING_MODE, sets it again), and a network roster
    // only on an online mode.
    mbModeStartedPlaying = false;
    CGS_ASSERT(lpGameModeParams->mbIsOnline || lpGameModeParams->miNumNetworkPlayers == 0,
               "lpGameModeParams->mbIsOnline || (lpGameModeParams->miNumNetworkPlayers == 0 )");

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        // [FLAG PC witness] [ai-attach] ONE LINE PER PREPARE-FOR-MODE (not per frame). The
        // start-style inputs PlaceRaceCarOnLoad's rival arms branch on, printed where they are
        // latched, so the next run's log says WHICH arm every rival should be taking before the
        // per-car lines below it say which one it took.
        // DELETE-WHEN rivals drive and the AI lane closes.
        *CgsDev::Log::gpDebugPrint
            << "[ai-attach] mode armed: opponents " << miOpponentCount
            << " mode " << static_cast<s32>(meGameModeType)
            << " aiDriveByStart " << static_cast<s32>(mbSpawnAIBehindStartGrid)
            << " playerRolls " << static_cast<s32>(mbPlayerRollsOnEventStart)
            << " donut " << static_cast<s32>(mbPlayerDonutsOnEventStart)
            << " resetBehind "
            << static_cast<s32>(GetGameModeFlag(
                   BrnGameState::GameModeParams::KU_FLAG_AI_RESET_ON_TRACK_BEHIND))
            << " [FLAG PC witness]\n";
    }

    // ========================================================================================
    // ARTIST 0x823098D8..0x82309958 -- THE CRASH-PLAY ARM, immediately before OnModeStart.
    // (crash-play wave 2026-08-29, landing with BrnCrashPlayManager.cpp.)
    //
    // The gate is KU_FLAG_ALLOW_CRASH_PLAY_CONTROLS (0x100), NOT the showtime flag below --
    // pseudocode `if ( (*(a2 + 2196) & 0x100) != 0 )`, and 2196 is GameModeParams::muFlags.
    // The boost fraction is built from the selected strategy's own two accessors, guarded
    // against a zero max: `v40 = 1.0; if (!IsZero(GetMaxBoost())) v40 = GetBoostAmount() /
    // GetMaxBoost();` (vtable slots 112 and 116 == GetBoostAmount / GetMaxBoost).
    //
    // WARN Activate IGNORES BOTH ARGUMENTS on this build -- across all 54 of its instructions
    // ARTIST never reads r4, r5 or f1, and the initial meter comes from the file-scope
    // KF_INITIAL_MIN_BOOST (51.0f) instead. The fraction is still computed and passed, exactly as
    // the console does, because the two vtable calls are real side effects on the strategy's own
    // call path and because the argument shape is the DWARF's.
    // ========================================================================================
    if (lpGameModeParams->GetFlag(
            BrnGameState::GameModeParams::KU_FLAG_ALLOW_CRASH_PLAY_CONTROLS))
    {
        BoostStrategy* lpBoostStrategy = mBoostManager.GetBoostStrategy();

        f32 lfInitialBoostPercentage = 1.0f;
        if (lpBoostStrategy != 0 && !rw::math::fpu::IsZero(lpBoostStrategy->GetMaxBoost()))
        {
            lfInitialBoostPercentage =
                lpBoostStrategy->GetBoostAmount() / lpBoostStrategy->GetMaxBoost();
        }

        mCrashPlayManager.Activate(GetActiveRaceCar(mePlayerActiveRaceCarIndex),
                                   lfInitialBoostPercentage);
    }

    // ARTIST 0x8230995C..0x8230997C. The manager wrapper supplies the B2 flag
    // from its selected strategy id.
    mBoostManager.OnModeStart(meGameModeType);

    // ========================================================================================
    // ARTIST 0x82309980..0x823099D4 -- THE SHOWTIME ARM.
    //
    // Under KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR (0x200) the console does three things:
    //     (*(**(a1 + 97504) + 52))(*(a1 + 97504));   // the strategy's OnShowtimeStart virtual
    //     *(a1 + 98877) = 1;                         // CrashPlayManager::mbIsInShowtime
    //     *(GetActiveRaceCar(...) + 1922) = 1;       // ActiveRaceCar::mbIsWrecked
    //
    // ⛔ +1922 == +0x782 IS mbIsWrecked, NOT mbIsInShowtime (+0x788 == 1928): `stb r18, 0x782(r3)`
    // @0x823099D0, r18 == the literal 1 (`li r18, 1` @0x82309408, never rewritten in this body).
    // Until 2026-09-22 (crash-parity FX-RCEM) this arm stored ActiveRaceCar::SetInShowtime(true)
    // instead -- a store the console never makes here (+0x788's only 1-writer is action 143,
    // HandleGameActions @0x8230D724) -- and never latched the wreck, so IsWrecked() @0x822BFDA0
    // did not short-circuit true for the Showtime car: ProcessPlayerVehicleInput's
    // `IsCrashing() && IsWrecked()` pin (zero gas, full brake + handbrake in Showtime) and the
    // crash verdict's wreck arm then depended on the physics snapshot instead of the latch.
    //
    // module+98877 == 98544 + 0x14D == mCrashPlayManager.mbIsInShowtime, and it had NO writer
    // anywhere in this tree until now -- which is why CrashPlayManager::Update's showtime branch
    // (UpdateTrafficStomp + UpdateBounceBoost) could never run and nothing could ever spend
    // showtime boost.
    //
    // The vtable BYTE offset 52 is SLOT 13, which BrnBoostStrategy.h:202 already declares and
    // BoostBurnout2/3/5 already body: OnStartCrashPlay. It drops mfMinBoostAllowedAmount from
    // mfMaxBoost * 0.15 to FLT_EPSILON, i.e. "during crash play the bar may be spent all the way
    // to zero" -- and IsBoosting() is `mfBoostAmount > mfMinBoostAllowedAmount`, so that floor is
    // load-bearing. It had no dispatcher anywhere in this tree until now.
    // ========================================================================================
    if (lpGameModeParams->GetFlag(
            BrnGameState::GameModeParams::KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR))
    {
        BoostStrategy* lpShowtimeStrategy = mBoostManager.GetBoostStrategy();
        if (lpShowtimeStrategy != 0)
        {
            lpShowtimeStrategy->OnStartCrashPlay();
        }

        mCrashPlayManager.mbIsInShowtime = true;                           // stbx r18, this+0x1823D
        WreckLatchWitness("HandlePrepareForModeAction.showtime@0x823099D0",
                          static_cast<s32>(mePlayerActiveRaceCarIndex), true);
        GetActiveRaceCar(mePlayerActiveRaceCarIndex)->mbIsWrecked = true;  // stb r18, 0x782(car)
    }

    if (!lpGameModeParams->GetFlag(
            BrnGameState::GameModeParams::KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR))
    {
        // ARTIST 0x823099D8..0x823099F0: non-Showtime modes cannot earn until
        // game action 34 (START_PLAYING_MODE) reenables it.
        mBoostManager.SetBoostEarningEnabled(false);
    }

    // ========================================================================================
    // ⭐⭐⭐ THE OFFLINE OPPONENTS / START-GRID ARM  (0x82309888  bl SetupOpponents @0x82307DF0)
    //
    // ⛔ THE LIFT IS RETIRED (start-grid wave, 2026-08-27). Frontier round 3 lifted ONE statement
    // out of this arm -- SetActiveRaceCarForPlayerScoringIndex, the tail of
    // SetUpPlayerCarForMode @0x823058F8 -- on the stated premise that the rest of the chain
    // "needs the pre-scene output buffer and the AI interfaces this call site does not hold".
    // THAT PREMISE WAS FALSE: this function already receives lpOutput, and
    // OutputBuffer_PreScene::GetRaceCarAIInterface() is the same accessor
    // HandleResetPlayerCarAction has called since the reset-player-car wave. Both callees are
    // bodied now (below in this file) and the console's own call replaces the lift, so the
    // mapping store is back at its own console position inside SetUpPlayerCarForMode.
    //
    // ⭐ THE SCORING SLOT IS miNumRivals -- asm, not inference (0x82305DBC..0x82305DD4):
    //     lbz   r10, 0(r29)            ; r29 == lpGameModeParams; the BYTE at +0 == miNumRivals
    //     extsb r4, r10                ; -> the EPlayerScoringIndex argument
    //     lwzx  r5, r28, 0x182F8       ; mePlayerActiveRaceCarIndex
    //     bl    SetActiveRaceCarForPlayerScoringIndex
    // GameModeParams+0 is miNumRivals and +1 is miNumNetworkPlayers -- the same two bytes
    // ModeManager::PrepareForGameMode reads as `lbz 0(r29)` / `lbz 1(r29)` to size its player
    // loop (BrnModeManager_Prepare.cpp:583). The player is therefore the LAST grid slot: with N
    // rivals the roster's AddPlayer() walk hands scoring slots 0..N-1 to the rivals and slot N
    // to the player. StuntAttackMode::Start sets miNumRivals to 0 (BrnStuntAttackMode.cpp:249,
    // `stb r23, 0(r31)`), so an offline stunt run maps player scoring slot 0. Kept here because
    // it is the derivation the moved statement rests on.
    //
    // ⭐ WHAT THE MAPPING LEG FIXES (run scratch/flow_run/20260827_140514): with it never
    // written, RaceCarEntityModule::CopyActiveRaceCarToPlayerScoringMappingToOutput publishes
    // the all-sentinel table ClearAllActiveRaceCarToPlayerScoringMappings left above,
    // ModeManager's binding sweep skips every slot, ScoringSystem::maCarData[0] keeps the
    // E_ACTIVE_RACE_CAR_INDEX_INVALID that ScoringSystem::AddPlayer stamped, and
    // ScoringSystem::GetCarData returns NULL at the end of the mode:
    //     [ASSERT 31113] lpCarData (BrnScoringSystem_Timer.cpp:341)
    //     [EXCEPTION] ACCESS_VIOLATION reading 0x18  StopModeTimer + 0xE5
    //         <- FinishCurrentMode + 0x3FC <- ModeManager::PreWorldUpdate
    // -- assert-is-not-a-guard again.
    //
    // The online/offline fork is reproduced below: the params' mbIsOnline byte (+0x94) picks
    // the network roster (RemoveAllRaceCars + AddRaceCarToStartingGridOrFreeburnLobby per grid
    // slot) over the offline SetupOpponents arm. On the offline campaign path the byte is zero.
    //
    // The mode-type half of the console's outer gate (`lwz r11, 0x148(r25)` @0x8230962C, the
    // 15/16 pair, `bne -> loc_8230988C` skips the whole grid/opponents block) IS reproduced --
    // it reads a member this tree does have.
    //
    // ⚠️ ORDERING NOTE. The console runs SetupOpponents (0x82309888) BEFORE
    // BoostStrategy::OnModeStart (0x8230997C); this body has carried OnModeStart earlier since
    // the boost pass. The call sits where the lift sat -- after the boost spine and before
    // SetAllActiveCarsInGameMode (0x82309A20), which is the ordering constraint that matters:
    // mbIsInGameMode is already true above, so SetUpPlayerCarForMode's `SetInGameMode` copy
    // reads the armed value.
    // ========================================================================================
    // THE FORK, as the console has it. The whole grid block is skipped only when the record says
    // the game is moving between the two lobby modes (+0x8D0) AND the mode is one of them; then
    // the params' mbIsOnline byte (+0x94) picks the online roster over SetupOpponents.
    const bool lbMovingBetweenLobbyModes =
        lpPFMAction->IsMovingBetweenOnlineLobbyModes() &&
        BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(lpGameModeParams->GetGameModeType());

    if (!lbMovingBetweenLobbyModes && lpGameModeParams->mbIsOnline)
    {
        const bool lbIsFreeburnLobby =
            BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(lpGameModeParams->GetGameModeType());

        // The local car's pose and model, read before anything is removed.
        const Vector3 lLocalPosition  = GetActiveRaceCar(mePlayerActiveRaceCarIndex)->GetPosition();
        const Vector3 lLocalDirection = GetActiveRaceCar(mePlayerActiveRaceCarIndex)->GetDirection();
        const EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex = mePlayerActiveRaceCarIndex;
        const CgsID lLocalCarModelId =
            GetActiveRaceCar(leLocalPlayerActiveRaceCarIndex)->GetGlobalRaceCar()->GetModelId();

        // Respawn the local car everywhere but the lobby; in the lobby only when the roster names
        // a different car for it. The console ORs in one more byte, module +0x17D06 == the
        // RaceCarEntityModuleDebugComponent's +0x16 toggle (its Construct zeroes it and only the
        // debug menu writes it); the component is not embedded on this build, so the byte is its
        // constructed false.
        const bool kbDebugComponentLobbyToggle = false;
        bool lbRespawnLocalCar = !lbIsFreeburnLobby || kbDebugComponentLobbyToggle;
        s32  liLocalPlayerGridPosition = -1;
        const s32 liGridCount = static_cast<s32>(lpGameModeParams->miNumNetworkPlayers) + 1;
        for (s32 liGrid = 0; liGrid < liGridCount; ++liGrid)
        {
            if (lpGameModeParams->maNetworkPlayerID[liGrid] == lpGameModeParams->mLocalNetworkPlayerID)
            {
                if (!lbRespawnLocalCar)
                {
                    lbRespawnLocalCar = (lLocalCarModelId != lpGameModeParams->maModelIds[liGrid]);
                }
                liLocalPlayerGridPosition = liGrid;
                break;
            }
        }

        RemoveAllRaceCars(lpOutput, lbRespawnLocalCar);

        CGS_ASSERT(liLocalPlayerGridPosition > -1 && liLocalPlayerGridPosition < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "( liLocalPlayerGridPosition > -1 ) && ( liLocalPlayerGridPosition < BrnWorld::KI_MAX_ACTIVE_RACE_CARS )");

        BrnNetHarnessPC::Witness("world", "online grid mode=%d cars=%d lobby=%d localGrid=%d respawnLocal=%d",
                                 static_cast<s32>(lpGameModeParams->GetGameModeType()), liGridCount,
                                 lbIsFreeburnLobby ? 1 : 0, liLocalPlayerGridPosition,
                                 lbRespawnLocalCar ? 1 : 0);

        AddRaceCarToStartingGridOrFreeburnLobby(lpPFMAction, lpOutput->GetRaceCarAIInterface(),
                                                lpGameModeParams, lbIsFreeburnLobby, lbRespawnLocalCar,
                                                liLocalPlayerGridPosition, leLocalPlayerActiveRaceCarIndex,
                                                lLocalPosition, lLocalDirection);

        for (s32 liGrid = 0; liGrid < liGridCount; ++liGrid)
        {
            if (liGrid == liLocalPlayerGridPosition)
            {
                continue;
            }
            CGS_ASSERT(liGrid > -1 && liGrid < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "( liGridPosition > -1 ) && ( liGridPosition < BrnWorld::KI_MAX_ACTIVE_RACE_CARS )");
            AddRaceCarToStartingGridOrFreeburnLobby(lpPFMAction, lpOutput->GetRaceCarAIInterface(),
                                                    lpGameModeParams, lbIsFreeburnLobby, lbRespawnLocalCar,
                                                    liGrid, leLocalPlayerActiveRaceCarIndex,
                                                    lLocalPosition, lLocalDirection);
        }
    }
    else if (!lbMovingBetweenLobbyModes)
    {
        SetupOpponents(lpGameModeParams, lpOutput);

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
                << " (mode " << static_cast<s32>(meGameModeType)
                << ", start-grid flag "
                << static_cast<s32>(GetGameModeFlag(
                       BrnGameState::GameModeParams::KU_FLAG_SET_CARS_TO_START_GRID))
                << ")\n";
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


// ============================================================================================
// ⭐⭐⭐ SetupOpponents @0x82307DF0 -- THE MODE-START SPINE, landed 2026-08-27 (start-grid wave).
//
// WHY IT EXISTS NOW. The banner on HandlePrepareForModeAction above used to say that
// SetupOpponents' legs "need the pre-scene output buffer and the AI interfaces this call site
// does not hold". That premise was wrong: HandlePrepareForModeAction already RECEIVES
// lpOutput (the pre-scene OutputBuffer_PreScene), and OutputBuffer_PreScene::
// GetRaceCarAIInterface() is the same accessor HandleResetPlayerCarAction has been calling
// since the reset-player-car wave. So only the OFFLINE grid seat was actually missing, and its
// absence is what left a stunt race's countdown holding for ever: nothing ever put the player's
// car on the event's start grid, so StuntAttackMode::PreWorldUpdate's facing test
// (BrnStuntAttackMode.cpp:458) could only pass if the player happened to drive up pointing
// within 5 degrees of the junction's start direction.
//
// THE CONSOLE BODY, instruction for instruction (asm 0x82307DF0..0x82307FBC):
//   0x82307E10  r4 = *(this + 0x182F8)              == mePlayerActiveRaceCarIndex
//   0x82307E14  GetActiveRaceCar / IsAttached tripwire (BrnActiveRaceCar.h:1089)
//   0x82307E4C  r31 = *(car + 0x6F0)                == ActiveRaceCar::GetGlobalRaceCar()
//   0x82307E54  RaceCar::GetModelId
//   0x82307E60  RaceCar::GetWheelModelId
//   0x82307E64  r31 = lpGameModeParams + 0x260      == maCheckpointDataArray
//   0x82307E6C  lwz r11, 0x2C0(r31)                 == the array's count word (params+0x520),
//               i.e. GetCheckpointCount() INLINED -- its CgsArray.h:336 constructed-assert
//               included, which is why that assert is NOT restated at this call site
//   0x82307EAC  Array<CheckpointData,16>::Ge(0) then `lhz r31, 2(r3)`
//                                                   == CheckpointData::muAISectionIndex
//   0x82307EB8  else r31 = 0x7FFF                   == the "no AI section" sentinel
//   0x82307EC4  ldx  this + 0x18358                 == mxGameModeFlags (the MODULE's copy)
//   0x82307EC8  rlwinm 0,30,30 -> bit 0x2           == KU_FLAG_REMOVE_RIVALS_FROM_WORLD
//   0x82307EF4  bl RemoveRivals(lpOutput, false)
//   0x82307F10  bl SetUpPlayerCarForMode(params, lpOutput, modelId, wheelId, aiSection)
//   0x82307F24  bl SetUpAIForMode(params, lpOutput, aiSection)
//   0x82307F28  ld 0x860(params)                    == GameModeParams::muFlags (the PARAMS' copy)
//   0x82307F2C  rlwinm 0,22,22 -> bit 0x200         == KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR
//   0x82307F58  bl SetAllCarsOnStartLine(2 /* RACING */, true)
//   0x82307F64  rlwinm 0,5,5   -> bit 0x4000000     == KU_FLAG_ROLLING_START
//   0x82307F84  rlwinm 0,4,4   -> bit 0x8000000     == KU_FLAG_DONUT_START
//   0x82307FB4  bl SetAllCarsOnStartLine((rolling||donut) ? 1 : 0, true)
//
// [!] THE TWO FLAG WORDS ARE DIFFERENT OBJECTS AND THE CONSOLE READS BOTH. The RemoveRivals
// gate reads the MODULE's mirror (`ldx r27, 0x18358`), the three tail gates read the PARAMS'
// own word (`ld 0x860(r30)`). Reproduced as GetGameModeFlag() / lpGameModeParams->GetFlag()
// respectively -- they hold the same bits only because HandlePrepareForModeAction copies one
// into the other three statements earlier, and that copy is itself only landed this wave.
//
// [x] THE TWO RIVAL LEGS ARE LIVE (rival-spawn wave R, 2026-09-02):
//     RaceCarEntityModule::RemoveRivals   @0x82305E00   -> BrnRaceCarEntityModule_Rivals.cpp
//     RaceCarEntityModule::SetUpAIForMode @0x82301620   -> BrnRaceCarEntityModule_Rivals.cpp
// Both calls below are exactly as quoted in the asm table above.
// ============================================================================================
void RaceCarEntityModule::SetupOpponents(
    const BrnGameState::GameModeParams* lpGameModeParams,
    RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput)
{
    CGS_ASSERT(lpGameModeParams != 0, "lpGameModeParams != NULL");
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");

    ActiveRaceCar* lpPlayerActiveRaceCar = GetActiveRaceCar(mePlayerActiveRaceCarIndex);

    // 0x82307E1C..0x82307E48 -- GetGlobalRaceCar()'s own tripwire, re-emitted by the inliner.
    CGS_ASSERT(lpPlayerActiveRaceCar->IsAttached(), "IsAttached()");

    RaceCar* lpPlayerRaceCar = lpPlayerActiveRaceCar->GetGlobalRaceCar();

    const CgsID lCarModelId   = lpPlayerRaceCar->GetModelId();
    const CgsID lWheelModelId = lpPlayerRaceCar->GetWheelModelId();

    // 0x82307E64..0x82307EBC. The event's FIRST checkpoint supplies the AI section the car is
    // driving towards; with no checkpoints at all (an offline stunt run has none) the console
    // uses the 0x7FFF sentinel -- the same 32767 the [rot-ring] diagnostic already prints as
    // the player's aiSection on this build.
    u16 lu16StartAISectionIndex = KU_NO_AI_SECTION;
    if (lpGameModeParams->GetCheckpointCount() > 0)
    {
        lu16StartAISectionIndex = lpGameModeParams->GetCheckpointData(0)->GetAISectionIndex();
    }

    if (GetGameModeFlag(BrnGameState::GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD))
    {
        RemoveRivals(lpOutput, false);                                   // 0x82307EF4, r5 == 0
    }

    SetUpPlayerCarForMode(lpGameModeParams, lpOutput, lCarModelId, lWheelModelId,
                          lu16StartAISectionIndex);

    SetUpAIForMode(lpGameModeParams, lpOutput, lu16StartAISectionIndex);   // 0x82307F24

    // 0x82307F28..0x82307FB4. Showtime drives with the cars already RACING; every other mode
    // holds them on the line (or rolling) until game action 34 (START_PLAYING_MODE) flips them
    // back to RACING -- BrnRaceCarEntityModule.cpp:2392, the symmetric half of this call.
    if (lpGameModeParams->GetFlag(
            BrnGameState::GameModeParams::KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR))
    {
        SetAllCarsOnStartLine(ActiveRaceCar::E_RACE_START_STATE_RACING, true);
    }
    else
    {
        const bool lbRollingOrDonutStart =
            lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_ROLLING_START) ||
            lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_DONUT_START);

        SetAllCarsOnStartLine(lbRollingOrDonutStart
                                  ? ActiveRaceCar::E_RACE_START_STATE_ROLLING_START
                                  : ActiveRaceCar::E_RACE_START_STATE_ON_START_LINE,
                              true);
    }
}


// ============================================================================================
// ⭐⭐⭐ SetUpPlayerCarForMode @0x823058F8 -- THE FUNCTION THAT SEATS THE PLAYER ON THE GRID.
//
// ⭐ WHAT IT FIXES. On console the player never has to drive up facing the right way: starting
// an event PLACES the car on the event's start grid. Offline, that placement is ONE call --
// ActiveRaceCar::RequestPlaceOnTrack(startPosition, startDirection, 0.0f) -- into the same
// place-on-track chain the harness BRN_CAR_TELEPORT trigger already drives and that is already
// proven live ("[teleport] ResetActiveRaceCar RE-RESET car 0 -> road ... seated ..."):
//     ActiveRaceCar::RequestPlaceOnTrack @0x822BFB58     (the request latch)
//       -> PlaceOnTrackManager::PostSceneUpdate @0x822D3168 (the 100 m world line test; the
//            PC-only WORLDCOL.BIN answer that stood here is retired, FX-GEOMETRIC 2026-09-24)
//       -> PlaceOnTrackManager::PrePhysicsUpdate @0x822F6DF8 (the scene's answer, walked)
//       -> ComputeBestPlaceOnT @0x822BE238               (candidate ranking)
//       -> PlaceOnTrackManager::PlaceCarOnTrack          (BrnMath::BuildTransform(pos, at, up))
//       -> RaceCarEntityModule::ResetActiveRaceCar @0x822F4880
//       -> VehicleInputInterface::ResetRaceCar @0x822CC2A0
//       -> VehicleManager::ProcessResetEvents @0x82617820
//       -> VehiclePhysics::SetTransformFromPositionOnRoad @0x825D1C00 + VehiclePhysics::Reset
// NOTHING here writes a transform, a velocity or a physics field: this function only asks.
//
// THE FLAG GATE, and why it is the module's word and not the params' (asm 0x82305924..0x82305970):
//     lis r11,1 ; ori r11,r11,0x8358 ; ldx r11,r28,r11 ; clrldi r11,r11,63
// i.e. `(mxGameModeFlags & KU_FLAG_SET_CARS_TO_START_GRID)` on the MODULE's +0x18358 mirror,
// which HandlePrepareForModeAction fills from GameModeParams::muFlags (+0x860) at 0x82309480.
// StuntAttackMode::Start sets that bit (BrnStuntAttackMode.cpp:40), and RaceMode::Start sets it
// too (BrnRaceMode.cpp:116).
//
// ⭐ THE SCORING-SLOT STORE IS OUTSIDE THE GATE. `beq cr6, loc_82305DBC` jumps PAST the whole
// body to the SetActiveRaceCarForPlayerScoringIndex tail -- so a mode without the flag still
// gets its player mapping. That statement used to be LIFTED into HandlePrepareForModeAction
// (frontier round 3) precisely because this function had no body; it now lives at its own
// console position and the lift is retired.
//
// THE TWO ARMS (`lbz` on the module's +0x18345 == mbIsInOnlineGameMode, asm 0x82305A88):
//   ONLINE  (0x82305AA8..0x82305BB0): build a transform from grid slot 0 with
//           BrnMath::BuildTransform(pos, dir, up), RemoveRaceCar the player's car, SpawnRaceCar
//           a fresh one there and AttachActiveRaceCar it back into the same slot.
//   OFFLINE (0x82305BB4..0x82305C2C): read the car's CURRENT transform, then
//           RequestPlaceOnTrack(gridPos, gridDir, 0.0f) and copy the module's mbIsInGameMode
//           into the slot (`stb r11, 0x777(r26)`).
// The online arm is landed rather than parked -- every callee exists -- but the flow does not
// reach it: HandlePrepareForModeAction latches mbIsInOnlineGameMode from the params' mbIsOnline
// byte, and only a params block with that byte clear takes the SetupOpponents arm that calls
// this function (an online mode takes the network roster instead).
//
// THE v125/v126 PAIR. Both arms load two vectors (transform +0x20 == zAxis/At and +0x30 ==
// wAxis/Pos) into v126/v125, but the ONLY consumer is the online SetUpOutOfRangeRaceCar call at
// 0x82305D7C. The offline arm still does the GetTransform read (0x82305BBC) -- a pure read,
// reproduced so the shape stays the console's rather than an optimised paraphrase.
//
// [FLAG] THE COLOUR-RANGE ASSERT PAIR IS THE SAME STAND-IN HandleResetPlayerCarAction USES.
// The console's second test is `colourIndex >= GlobalColourPalette::operator[](palette).count`
// (asm 0x82305CC8..0x82305CEC, a read through the module's palette table at +0x1843C that this
// tree does not model), so the tripwire here keeps the console's STRING and file/line but tests
// the reproducible half -- byte-for-byte the precedent set at BrnRaceCarEntityModule.cpp:7556.
// DELETE-WHEN GlobalColourPalette's per-palette count is reachable: restore the real compare.
// ============================================================================================
void RaceCarEntityModule::SetUpPlayerCarForMode(
    const BrnGameState::GameModeParams* lpGameModeParams,
    RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
    CgsID lCarModelId,
    CgsID lWheelModelId,
    u16 lu16StartAISectionIndex)
{
    CGS_ASSERT(lpGameModeParams != 0, "lpGameModeParams != NULL");
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");

    // [PC GUARD] THE START GRID MUST ACTUALLY BE POPULATED. The console's own tripwire
    // ("Missing Starting grid for mode", fired in the refusal arm just below) is an ASSERT, and
    // an assert is not a guard: on console it falls straight through into
    // StartLocation_8_::Ge(0) and reads an
    // EMPTY array's slot 0. That is harmless on a console whose ModeManager::SetStartingGrid
    // @0x82328608 always seats the grid; it is NOT harmless here, because SetStartingGrid does
    // NOTHING when the event's GameModeParams carries an invalid LightTriggerId
    // (BrnModeManager_IntroPlay.cpp:215 -- the console's own authored "started away from lights"
    // path), and this build has junctions whose trigger data is not yet resolved. Falling
    // through there would hand RequestPlaceOnTrack an UNINITIALISED StartLocation and teleport
    // the player to garbage coordinates -- a far worse failure than the hold this wave fixes,
    // and one that would look like a physics bug rather than a data hole.
    // The gate tests EXACTLY the predicate the console's assert names, so the log says which
    // wire is missing instead of the assert storming; same shape and same reasoning as the
    // event-25 gate at GameStateModule_gUI_00.cpp:1166. The scoring-slot tail below still runs,
    // which is what the console does when the flag is clear anyway.
    // DELETE-WHEN every junction that can start an event resolves a LightTriggerStartData: drop
    // the refusal arm and the `&& lbHasStartGrid`, and move the assert back to its console
    // position (after the colour reads, 0x82305A34) where it stands alone.
    const bool lbSetCarsToStartGrid =
        GetGameModeFlag(BrnGameState::GameModeParams::KU_FLAG_SET_CARS_TO_START_GRID);
    const bool lbHasStartGrid = (lpGameModeParams->GetStartLocationCount() > 0);

    if (lbSetCarsToStartGrid && !lbHasStartGrid)
    {
        // The console's own tripwire, HOISTED three statements earlier than 0x82305A34 so the
        // guard can act on the same predicate. GetStartLocationCount() carries the
        // CgsArray.h:336 "Array used before Construct/Clear was called" assert the console
        // inlines alongside it, so that one is NOT restated; this string's trailing newline is
        // part of the console's streamed message.
        CGS_ASSERT(lbHasStartGrid, "Missing Starting grid for mode\n");    // X360 :8614

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[start-grid] REFUSED: mode " << static_cast<s32>(meGameModeType)
                << " asked for the start grid but GameModeParams carries ZERO start locations "
                   "-- ModeManager::SetStartingGrid seated nothing (invalid LightTriggerId, or "
                   "no LightTriggerStartData for this junction). The player car is NOT moved.\n";
        }
    }

    if (lbSetCarsToStartGrid && lbHasStartGrid)
    {
        // ---- 0x82305974..0x823059E0 : the player slot, its car, and its colour -------------
        const EActiveRaceCarIndex lePlayerIndexOnEntry = mePlayerActiveRaceCarIndex;

        ActiveRaceCar* lpPlayerActiveRaceCar = GetActiveRaceCar(lePlayerIndexOnEntry);
        CGS_ASSERT(lpPlayerActiveRaceCar->IsAttached(), "IsAttached()");   // :1089

        RaceCar* lpPlayerRaceCar = lpPlayerActiveRaceCar->GetGlobalRaceCar();

        // Read BEFORE the online arm can replace the car -- `lwz r14,0x94(r25)` /
        // `lwz r17,0x98(r25)` at 0x823059DC/0x823059E0, off the ORIGINAL RaceCar.
        const s32 liColourIndex   = lpPlayerRaceCar->GetColourIndex();
        const s32 liColourPalette = lpPlayerRaceCar->GetColourPalette();

        // ---- 0x82305A88.. : grid slot 0 (the console's assert for this array is above) ------
        const Vector3 lStartPosition  = lpGameModeParams->GetStartPosition(KI_PLAYER_START_GRID_SLOT);
        const Vector3 lStartDirection = lpGameModeParams->GetStartDirection(KI_PLAYER_START_GRID_SLOT);

        // The pair the online SetUpOutOfRangeRaceCar consumes (see the banner).
        Vector3 lSeatPosition  = { 0.0f, 0.0f, 0.0f, 0.0f };
        Vector3 lSeatDirection = { 0.0f, 0.0f, 0.0f, 0.0f };

        EGlobalRaceCarIndex leGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_INVALID;

        if (mbIsInOnlineGameMode)
        {
            // ---- 0x82305AA8..0x82305BB0 : ONLINE -- respawn the car ON the grid slot --------
            // v3 == unk_82181510, the same world UP axis row HandleResetPlayerCarAction feeds
            // BuildTransform (BrnRaceCarEntityModule.cpp:2088).
            Matrix44Affine lGridTransform;
            BrnMath::BuildTransform(lGridTransform, lStartPosition, lStartDirection,
                                    Vector3{ 0.0f, 1.0f, 0.0f, 0.0f });

            lSeatDirection = lGridTransform.zAxis;   // stack copy read back at 0x82305BAC
            lSeatPosition  = lGridTransform.wAxis;   // ...and at 0x82305BA4

            RemoveRaceCar(lpPlayerRaceCar->GetGlobalRaceCarIndex(), lpOutput);

            leGlobalRaceCarIndex =
                SpawnRaceCar(lpOutput->GetRaceCarAIInterface(), lGridTransform,
                             E_RACE_CAR_TYPE_PLAYER, lCarModelId, false /* r8 == 0 */,
                             lWheelModelId, 0 /* r10 == 0, no rival id */,
                             -1 /* sp+0x54 == -1, no opponent index */);

            // `stw r3, 0(r20)` -- the returned ACTIVE index goes straight back into the
            // module's player slot word.
            mePlayerActiveRaceCarIndex =
                AttachActiveRaceCar(GetGlobalRaceCar(leGlobalRaceCarIndex), lePlayerIndexOnEntry);
        }
        else
        {
            // ---- 0x82305BB4..0x82305C2C : OFFLINE -- ask for the place-on-track ------------
            const Matrix44Affine lCurrentTransform = lpPlayerActiveRaceCar->GetTransform();
            lSeatDirection = lCurrentTransform.zAxis;   // lvx128 v126, r11, 0x20
            lSeatPosition  = lCurrentTransform.wAxis;   // lvx128 v125, r11, 0x30

            // `fmr f1, f30` where f30 == flt_82001CC0 == 0.0f: the car arrives AT REST, and
            // PlaceCarOnTrack multiplies the reset direction by that zero.
            lpPlayerActiveRaceCar->RequestPlaceOnTrack(lStartPosition, lStartDirection,
                                                       KF_START_GRID_PLACE_ON_TRACK_SPEED);

            leGlobalRaceCarIndex =
                lpPlayerActiveRaceCar->GetGlobalRaceCar()->GetGlobalRaceCarIndex();

            // `lbzx r11, r28, 0x18344 ; stb r11, 0x777(r26)` -- the module's mbIsInGameMode
            // into the slot's own copy, by name at both ends.
            lpPlayerActiveRaceCar->SetInGameMode(mbIsInGameMode);

            // [DIAG] NOT IN THE X360 BINARY -- the proof rung for this wave. It names the pose
            // the console's own chain was asked for; the "[teleport] ResetActiveRaceCar" line
            // that follows a frame or two later is the place-on-track chain answering it.
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[start-grid] SetUpPlayerCarForMode: player car "
                    << static_cast<s32>(leGlobalRaceCarIndex)
                    << " -> RequestPlaceOnTrack pos (" << lStartPosition.x << ", "
                    << lStartPosition.y << ", " << lStartPosition.z << ") dir ("
                    << lStartDirection.x << ", " << lStartDirection.y << ", "
                    << lStartDirection.z << ") from (" << lSeatPosition.x << ", "
                    << lSeatPosition.y << ", " << lSeatPosition.z << "); grid slots "
                    << lpGameModeParams->GetStartLocationCount() << "\n";
            }
        }

        // ---- 0x82305C30..0x82305D4C : re-read the (possibly NEW) slot and carry the colour --
        ActiveRaceCar* lpSeatedActiveRaceCar = GetActiveRaceCar(mePlayerActiveRaceCarIndex);
        CGS_ASSERT(lpSeatedActiveRaceCar->IsAttached(), "IsAttached()");   // :1089

        RaceCar* lpSeatedRaceCar = lpSeatedActiveRaceCar->GetGlobalRaceCar();

        CGS_ASSERT(liColourPalette < 4, "Invalid Number of Palettes: ");   // X360 :8659
        CGS_ASSERT(liColourIndex >= 0, "Invalid car colour: ");            // X360 :8660

        lpSeatedRaceCar->SetColourIndex(liColourIndex);       // stw r14, 0x94(r26)
        lpSeatedRaceCar->SetColourPalette(liColourPalette);   // stw r17, 0x98(r26)

        // ---- 0x82305D50..0x82305D7C : online-only AI seeding -------------------------------
        if (mbIsInOnlineGameMode)
        {
            lpOutput->GetRaceCarAIInterface()->SetUpOutOfRangeRaceCar(
                leGlobalRaceCarIndex, lSeatPosition, lSeatDirection,
                KU_NO_AI_SECTION,                       // li r5, 0x7FFF -- a LITERAL here
                E_DISTRICT_INVALID,                     // li r6, 0x12 == 18
                0);                                     // li r7, 0
        }

        // ---- 0x82305D80..0x82305DB8 : "this car is in the current mode" --------------------
        // Five stores plus two floats, in the console's own order: the global index, a zero
        // opponent index, the section SetupOpponents resolved, a false deviate flag, the
        // params' mfProgressionRankAsRatio (`lfs f31, 4(r29)`) and f30 == 0.0f.
        BrnAI::AIModuleIO::AddCarToCurrentModeEvent lAddCarEvent;
        lAddCarEvent.meGlobalRaceCarIndex     = leGlobalRaceCarIndex;
        lAddCarEvent.miOpponentIndex          = 0;
        lAddCarEvent.muDestinationAISection   = lu16StartAISectionIndex;
        lAddCarEvent.mbDeviateFromRoute       = false;
        lAddCarEvent.mfProgressionRankAsRatio = lpGameModeParams->mfProgressionRankAsRatio;
        lAddCarEvent.mfOvertakingDifficulty   = 0.0f;

        lpOutput->GetRaceCarAIInterface()->mManagementQueue
            .AddEvent<BrnAI::AIModuleIO::AddCarToCurrentModeEvent>(
                &lAddCarEvent, BrnAI::AIModuleIO::E_EVENT_ADD_CAR_TO_MODE);
    }

    // ---- 0x82305DBC..0x82305DD4 : the mapping store, OUTSIDE the start-grid gate ------------
    // `lbz r10, 0(r29)` == GameModeParams::miNumRivals, sign-extended into the
    // EPlayerScoringIndex argument: the player is the LAST grid slot. See the retired lift's
    // derivation in HandlePrepareForModeAction above for why that is the right slot.
    SetActiveRaceCarForPlayerScoringIndex(
        static_cast<BrnGameState::GameStateModuleIO::EPlayerScoringIndex>(
            lpGameModeParams->miNumRivals),
        mePlayerActiveRaceCarIndex);
}


// ============================================================================================
// RemoveAllRaceCars -- the online roster's clean slate.
//
//   for every global slot (35):
//       [BrnRaceCar.h:547 type assert]  if (muType != INACTIVE)            == IsInWorld()
//       [BrnRaceCar.h:577 type assert]      if (muType != PLAYER || lbRemovePlayerCar)
//                                               RemoveRaceCar(slot, lpOutput);
// Unlike RemoveRivals it removes NETWORK cars too: every roster car is re-added by the caller.
// ============================================================================================
void RaceCarEntityModule::RemoveAllRaceCars(RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
                                            bool lbRemovePlayerCar)
{
    for (EGlobalRaceCarIndex leGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leGlobalRaceCarIndex++)
    {
        if (GetGlobalRaceCar(leGlobalRaceCarIndex)->IsInWorld())
        {
            if (!GetGlobalRaceCar(leGlobalRaceCarIndex)->IsPlayerDriven() || lbRemovePlayerCar)
            {
                RemoveRaceCar(leGlobalRaceCarIndex, lpOutput);
            }
        }
    }
}


// ============================================================================================
// AddRaceCarToStartingGridOrFreeburnLobby -- seat one roster car.
//
// The console, in order:
//   1. the free-burn lobby spawn table, eight positions built on the stack (below; flt rodata);
//   2. assert the local player's active slot is a real slot;
//   3. lbIsLocal = (maNetworkPlayerID[grid] == mLocalNetworkPlayerID);
//   4. the transform: off-lobby, the grid slot's start position/direction; in the lobby, the
//      local car's own position (the spawn table only under the debug component's toggle, see
//      the caller) or, for a network car, the ORIGIN (a zero splat) -- the network car's first
//      update snaps it to its owner's pose -- looking along the local car's direction. Either way
//      CreateLookAt(position, position + direction);
//   5. the local car on an online mode: boost strategy from the vehicle-list entry's car type
//      (0 -> B2, 1 -> B3, 2 -> B5, else the "Could not get the correct car type" assert) and
//      HandleCarStatsUpdate from its gameplay bytes;
//   6. keep the local car (clear the latched boost/accelerate/brake pad state) or SpawnRaceCar
//      (type PLAYER for the local car, NETWORK for the rest) + AttachActiveRaceCar (the local car
//      back into its own slot, the rest into any free slot); a network car in the lobby starts
//      CONNECTING with no driver controls received yet;
//   7. the grid slot's online deformation -> the car's base deformation pair (type -1 for none,
//      1 otherwise), mirrored into the module for the local car;
//   8. AddCarToCurrentModeEvent {global slot, grid, first checkpoint's AI section or 0x7FFF};
//   9. colour: team colour (flag bit 36 on a car whose livery type is 0 or 1) or the roster's
//      colour, then the roster's palette, both range-asserted;
//  10. the scoring map, the disconnected flag, and on online modes 11/13 the local car's
//      team boost (segments / amount / infinite).
// ============================================================================================
namespace
{
    // The free-burn lobby spawn table (rodata floats, big-endian bit patterns in brackets).
    const Vector3 KAV_FREEBURN_LOBBY_SPAWN_POSITIONS[E_ACTIVE_RACE_CAR_INDEX_COUNT] =
    {
        { 3008.169921875f,  -1.159999966621399f, -1874.300048828125f,  0.0f },   // [453C02B8 BF947AE1 C4EA499A]
        { 3172.3798828125f, -3.359999895095825f, -2006.1700439453125f, 0.0f },   // [45464614 C0570A3D C4FAC571]
        { 3248.090087890625f, -3.0f,             -1900.8699951171875f, 0.0f },   // [454B0171 C0400000 C4ED9BD7]
        { 3053.64990234375f, -0.9399999976158142f, -1764.550048828125f, 0.0f },  // [453EDA66 BF70A3D7 C4DC919A]
        { 3014.389892578125f, -4.130000114440918f, -2100.0400390625f,  0.0f },   // [453C663D C08428F6 C50340A4]
        { 3103.840087890625f, -2.140000104904175f, -1906.3599853515625f, 0.0f }, // [4541FD71 C008F5C3 C4EE4B85]
        { 3007.97607421875f, -2.490000009536743f, -1945.1099853515625f, 0.0f },  // [453BFF9E C01F5C29 C4F32385]
        { 3057.06005859375f, -3.6500000953674316f, -1990.1800537109375f, 0.0f }, // [453F10F6 C069999A C4F8C5C3]
    };

    // Team -> colour index (rodata u16 table): NONE 6, RED 0, BLUE 1.
    const u16 KAU16_TEAM_COLOUR_INDEX[3] = { 6u, 0u, 1u };

    // muFlags bit 36: team colours.
    const u64 KX_FLAG_USE_TEAM_COLOURS = 1ull << 36;

    // The "no AI section" sentinel when the params carry no checkpoint.
    const u16 KU_NO_START_AI_SECTION = 0x7FFFu;
}

void RaceCarEntityModule::AddRaceCarToStartingGridOrFreeburnLobby(
        const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPFMAction,
        BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface,
        const BrnGameState::GameModeParams* lpGameModeParams,
        bool lbIsFreeburnLobby,
        bool lbRespawnLocalCar,
        s32 liGridPosition,
        EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex,
        Vector3 lLocalPosition,
        Vector3 lLocalDirection)
{
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                   leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "( leLocalPlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

    const BrnNetwork::NetworkPlayerID lGridNetworkPlayerID = lpGameModeParams->maNetworkPlayerID[liGridPosition];
    const bool lbIsLocal = (lGridNetworkPlayerID == lpGameModeParams->mLocalNetworkPlayerID);

    // ---- 4. the transform -------------------------------------------------------------------
    Vector3 lPosition;
    Vector3 lDirection;
    if (!lbIsFreeburnLobby)
    {
        lPosition  = lpGameModeParams->GetStartPosition(liGridPosition);
        lDirection = lpGameModeParams->GetStartDirection(liGridPosition);
    }
    else
    {
        if (lbIsLocal)
        {
            lPosition = lLocalPosition;
            // Module +0x17D06, the debug component's toggle -- constructed false, see the caller.
            const bool kbDebugComponentLobbyToggle = false;
            if (kbDebugComponentLobbyToggle)
            {
                lPosition = KAV_FREEBURN_LOBBY_SPAWN_POSITIONS[liGridPosition];
            }
        }
        else
        {
            lPosition = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        }
        lDirection = lLocalDirection;
    }

    const CgsID lModelId = lpGameModeParams->maModelIds[liGridPosition];
    const Matrix44Affine lTransform =
        BrnDirector::Camera::Utils::CreateLookAt(lPosition, rw::math::vpu::Add(lPosition, lDirection));

    // ---- 5. the local car's boost strategy + stats -----------------------------------------
    ERaceCarType leRaceCarType = E_RACE_CAR_TYPE_NETWORK;
    if (lbIsLocal)
    {
        leRaceCarType = E_RACE_CAR_TYPE_PLAYER;
        if (mbIsInOnlineGameMode)
        {
            const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex(lModelId);
            const BrnResource::VehicleListEntry* lpListEntry =
                (liVehicleIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liVehicleIndex);
            CGS_ASSERT(lpListEntry != 0, "lpListEntry != NULL");

            const u8 luCarType = lpListEntry->GetCarType();
            if (luCarType == 0)
            {
                mBoostManager.SetBoostStrategy(BoostManager::E_BOOSTSTRATEGY_BURNOUT2);
            }
            else if (luCarType == 1)
            {
                mBoostManager.SetBoostStrategy(BoostManager::E_BOOSTSTRATEGY_BURNOUT3);
            }
            else if (luCarType < 3)
            {
                mBoostManager.SetBoostStrategy(BoostManager::E_BOOSTSTRATEGY_BURNOUT5);
            }
            else
            {
                // The console streams the type after the text; CGS_ASSERT forwards the literal.
                CGS_ASSERT(false, "Could not get the correct car type: ");
            }

            // The entry's gameplay block (+0x90); the two stats bytes are +0x98 / +0x9A, the same
            // pair HandleGameActions' car-select-finished arm hands to HandleCarStatsUpdate.
            const u8* lpcEntryBytes = reinterpret_cast<const u8*>(lpListEntry);
            CGS_ASSERT(lpcEntryBytes + 0x90 != 0, "lpVehicleListEntryGamePlayData");
            HandleCarStatsUpdate(static_cast<BrnResource::ECarType>(luCarType),
                                 static_cast<s32>(lpcEntryBytes[0x98]),
                                 static_cast<s32>(lpcEntryBytes[0x9A]));
        }
    }

    // ---- 6. keep the local car, or spawn + attach -------------------------------------------
    EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    RaceCar*            lpRaceCar            = 0;
    if (!lbRespawnLocalCar && lbIsLocal)
    {
        mPlayerVehicleControls.mbBoost        = false;
        mPlayerVehicleControls.mfAcceleration = 0.0f;
        mPlayerVehicleControls.mfBraking      = 0.0f;
        leActiveRaceCarIndex = mePlayerActiveRaceCarIndex;
        lpRaceCar            = GetPlayerRaceCar();
    }
    else
    {
        const EGlobalRaceCarIndex leGlobalRaceCarIndex =
            SpawnRaceCar(lpRaceCarAIInterface, lTransform, leRaceCarType, lModelId, false,
                         0 /* lWheelModelId: resolved from the model */,
                         0 /* lpRivalId: none */,
                         -1 /* liOpponentIndex */);
        lpRaceCar = GetGlobalRaceCar(leGlobalRaceCarIndex);
        AttachActiveRaceCar(lpRaceCar, lbIsLocal ? leLocalPlayerActiveRaceCarIndex
                                                 : E_ACTIVE_RACE_CAR_INDEX_INVALID);
        leActiveRaceCarIndex = lpRaceCar->GetActiveRaceCarIndex();

        if (lbIsFreeburnLobby && !lbIsLocal)
        {
            ActiveRaceCar* lpActiveRaceCar = lpRaceCar->GetActiveRaceCar();
            lpActiveRaceCar->meOnlineState                   = ActiveRaceCar::E_ONLINE_STATE_CONNECTING;
            lpActiveRaceCar->mbReceivedNetworkDriverControls = false;
        }
    }

    // ---- 7. the base deformation pair -------------------------------------------------------
    // GameModeParams' inlined reader asserts `>= 0.0f` (the branch is taken on unordered too,
    // so a NaN does not fire).
    CGS_ASSERT(!(lpGameModeParams->mafOnlineDeformationAmount[liGridPosition] < 0.0f),
               "mafOnlineDeformationAmount[ liOpponentIndex ] >= 0.0f");
    const f32 lfDeformAmount = lpGameModeParams->mafOnlineDeformationAmount[liGridPosition];
    const s32 liDeformType   = (lfDeformAmount == 0.0f) ? -1 : 1;
    GetActiveRaceCar(leActiveRaceCarIndex)->mfBaseDeformAmount = lfDeformAmount;
    GetActiveRaceCar(leActiveRaceCarIndex)->meBaseDeformationType =
        static_cast<BrnPhysics::Deformation::DeformationResetType>(liDeformType);

    // ---- 8. "this car is in the current mode" -----------------------------------------------
    u16 lu16StartAISectionIndex = KU_NO_START_AI_SECTION;
    if (lpGameModeParams->GetCheckpointCount() > 0)
    {
        lu16StartAISectionIndex = lpGameModeParams->GetCheckpointData(0)->GetAISectionIndex();
    }
    BrnAI::AIModuleIO::AddCarToCurrentModeEvent lAddCarEvent;
    lAddCarEvent.meGlobalRaceCarIndex     = lpRaceCar->GetGlobalRaceCarIndex();
    lAddCarEvent.miOpponentIndex          = liGridPosition;
    lAddCarEvent.muDestinationAISection   = lu16StartAISectionIndex;
    lAddCarEvent.mbDeviateFromRoute       = false;
    lAddCarEvent.mfProgressionRankAsRatio = 0.0f;
    lAddCarEvent.mfOvertakingDifficulty   = 0.0f;
    lpRaceCarAIInterface->mManagementQueue.AddEvent<BrnAI::AIModuleIO::AddCarToCurrentModeEvent>(
        &lAddCarEvent, BrnAI::AIModuleIO::E_EVENT_ADD_CAR_TO_MODE);

    if (lbIsLocal)
    {
        mePlayerActiveRaceCarIndex         = leActiveRaceCarIndex;
        mfPlayerBaseDeformAmountMirror     = lfDeformAmount;
        miPlayerBaseDeformationTypeMirror  = liDeformType;
    }

    // ---- 9. colour ----------------------------------------------------------------------------
    bool lbTeamColour = false;
    if ((lpGameModeParams->GetFlags() & KX_FLAG_USE_TEAM_COLOURS) != 0)
    {
        const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex(lModelId);
        const BrnResource::VehicleListEntry* lpEntry =
            (liVehicleIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liVehicleIndex);
        lbTeamColour = (lpEntry->GetLiveryType() == 0) || (lpEntry->GetLiveryType() == 1);
    }
    if (lbTeamColour)
    {
        const s32 liTeam = static_cast<s32>(lpGameModeParams->maePlayerTeam[liGridPosition]);
        GetActiveRaceCar(leActiveRaceCarIndex)->GetGlobalRaceCar()->SetColourIndex(
            static_cast<s32>(KAU16_TEAM_COLOUR_INDEX[liTeam]));
    }
    else
    {
        ActiveRaceCar* lpColourCar = GetActiveRaceCar(leActiveRaceCarIndex);
        CGS_ASSERT(lpColourCar->IsAttached(), "IsAttached()");
        lpColourCar->GetGlobalRaceCar()->SetColourIndex(
            static_cast<s32>(lpGameModeParams->mau16CarColourIndex[liGridPosition]));
    }
    {
        ActiveRaceCar* lpPaletteCar = GetActiveRaceCar(leActiveRaceCarIndex);
        CGS_ASSERT(lpPaletteCar->IsAttached(), "IsAttached()");
        const s32 liPalette = static_cast<s32>(lpGameModeParams->mau16CarPaintFinishIndex[liGridPosition]);
        lpPaletteCar->GetGlobalRaceCar()->SetColourPalette(liPalette);
        CGS_ASSERT(liPalette < E_NUM_PALETTES, "Invalid Number of Palettes: ");
        // The colour check indexes the palette table by that palette; it is only evaluated for an
        // in-range palette so a failed first assert cannot read past the four-entry resource.
        if (liPalette < E_NUM_PALETTES)
        {
            CGS_ASSERT(static_cast<s32>(lpGameModeParams->mau16CarColourIndex[liGridPosition]) <
                           mCarColoursResource->maPalettes[liPalette].GetNumColours(),
                       "Invalid car colour: ");
        }
    }

    // ---- 10. scoring map, disconnect, team boost ---------------------------------------------
    SetActiveRaceCarForPlayerScoringIndex(lpPFMAction->GetPlayerScoringIndex(liGridPosition),
                                          leActiveRaceCarIndex);

    if (lpPFMAction->GetPlayerDisconnected(lGridNetworkPlayerID))
    {
        CGS_ASSERT(mePlayerActiveRaceCarIndex != leActiveRaceCarIndex,
                   "Should not be setting the local player to be disconnected");
        maActiveRaceCars[leActiveRaceCarIndex].mbIsDisconnectedFromNetwork = true;
    }

    if (lbIsLocal &&
        (lpGameModeParams->GetGameModeType() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE ||
         lpGameModeParams->GetGameModeType() == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN))
    {
        BoostStrategy* lpBoostStrategy = mBoostManager.GetBoostStrategy();
        if (lpGameModeParams->mbInfiniteBoost &&
            static_cast<s32>(lpGameModeParams->maePlayerTeam[liGridPosition]) == 1)
        {
            lpBoostStrategy->SetBoostSegments(3);
            lpBoostStrategy->SetBoostAmount(100.0f);   // flt rodata 0x42C80000
            lpBoostStrategy->SetInfiniteBoost(true);
        }
        else
        {
            lpBoostStrategy->SetBoostSegments(0);
            lpBoostStrategy->SetBoostAmount(0.0f);
            lpBoostStrategy->SetInfiniteBoost(false);
        }
    }

    BrnNetHarnessPC::Witness("world", "grid car %d net=%d local=%d type=%d active=%d model=%016llX",
                             liGridPosition, static_cast<s32>(lGridNetworkPlayerID), lbIsLocal ? 1 : 0,
                             static_cast<s32>(lpRaceCar->GetType()), static_cast<s32>(leActiveRaceCarIndex),
                             static_cast<unsigned long long>(lModelId));
}

} // namespace BrnWorld
