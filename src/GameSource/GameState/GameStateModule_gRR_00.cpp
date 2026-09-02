// b5-decomp/src/GameSource/GameState/GameStateModule_gRR_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the
// module's other committed bodies live in BrnGameStateModule.cpp, GameStateModule_gUI_00.cpp
// and GameStateModule_gSR_00.cpp).
//
// ============================================================================================
// ROAD RAGE WAVE, AGENT C -- THE TAKEDOWN FEED INTO GAME STATE.
//
//   GameStateModule::ProcessTakedownEvents  @0x8238FC50   (sole xref-to: PreWorldUpdate @0x823A5328)
//
// This is the per-frame drain of the takedown-event queue: for every takedown the PLAYER
// scored it feeds the road-rage scorer (ScoringSystem::OnPlayerDoesATakedown ->
// RoadRageModeScoring::IncrementPlayerNumTakedowns -- THE road-rage counter), the profile /
// progression tallies, the developer-challenge and achievement hooks, the network telemetry
// hook, and (for any takedown the player SUFFERED in Marked Man) the "you got taken down"
// training tip.
//
// SIGNATURE FROM THE ASM (0x8238FC58..0x8238FC70), the DWARF shape confirmed:
//     r3 this / r4 lpGameActionQueue / r5 lpTakedownEventQueue / r6 lpOutputBuffer
//   DWARF: void ProcessTakedownEvents(InputBuffer::GameActionQueue*,
//                                     const InputBuffer::TakedownEventQueue*,
//                                     GameStateModuleIO::OutputBuffer*);
// The pseudocode's `int result` first parameter is Hex-Rays reusing r3; there is no such argument.
//
// RECORD MAP (TakedownEvent, 40 bytes on the console, BrnTakedownManagerTypes.h):
//     +0x00 meAggressorIndex   +0x04 meVictimIndex   +0x18 meType
//     +0x20 miTakedownChainCount   +0x24 mbMarkedManTakeDown
//
// [FLAG PC signature deviation] a FOURTH parameter, `lrTimerStatusInterface`, carries the
// frame's "now". The console reads it at gsm+208368 -- the module's 48-byte copy of the
// PreWorldInputBuffer's timer block (mSimTimerStatus.mTime), which EmmPreWorldUpdate
// @0x8238EF50 fills and which NOTHING on PC fills. Same deviation, same measured reason and the
// same source as CopyScoringDataToOutput (BrnGameStateModule.h). DELETE-WHEN
// DoUpdate_GameStatePreWorld stages a real PreWorldInputBuffer whose timer block is filled.
//
// FOUR LEGS ARE PARKED, EACH WITH A LOG-ONCE FLAG AT ITS SITE (every park is named and logged):
//   P1  ProgressionManager::OnTakedownTo @0x823666D0 -- neither declared nor bodied in the tree.
//   P2  AchievementManagerBase::OnTakedown / OnTakedownChain / OnCaughtFever -- the base TU is
//       deliberately unmounted (build_game_exe.bat: eight unresolved externals); OnTakedownChain
//       has no body at all.
//   P3  DeveloperChallengeManager::OnTakedownChain / OnTakedown -- bodied in the tree but the TU
//       is unmounted (seven unresolved externals) AND the manager is never Construct()ed, so the
//       real bodies would run against null back-pointers (the OnEventEnd precedent in
//       BrnBaselineLinkStubs.cpp).
//   P4  the E_ACTION_SEND_TELEMETRY post -- BrnNetwork::BrnNetworkModuleIO::TelemetryData
//       (DWARF BrnNetworkSharedIO.h:542, {ETelemetryHook meHook; char macBuffer[16]}) is not in
//       the tree; sub_8236A8B8 is its AddParameter(Vector3) overload.
// ============================================================================================
#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // gpDebugPrint (the log-once FLAGs)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                // CgsModule::EventQueue<TakedownEvent,8>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface / Time

#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer / GameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                        // E_ACTION_NETWORK_CAUGHT_FEVER / E_ACTION_SEND_TELEMETRY
#include "GameSource/GameState/BrnTakedownType.h"                       // ETakedownType
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h" // TakedownEvent
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::GetScoringSystem / GetCurrentGameModeType
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem::GetCarData / OnPlayerDoesATakedown, CarData::HasFever
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"    // the Marked Man tip gauntlet
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::HasPlayerSeenTrainingType / the online vertical tally
#include "SharedClasses/Progression/BrnTrainingTypes.h"                 // E_TRAINING_TYPE_TAKEN_DOWN_IN_MARKED_MAN
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // GetRivalId

namespace BrnGameState
{

namespace
{
    // flt_8200426C -- the console's 5.0 s "settle" gap between two training tips, the same literal
    // BrnDriveThruManager.cpp names KF_TRAINING_TIP_SETTLE_TIME.
    const f32 KF_TRAINING_TIP_SETTLE_TIME = 5.0f;

    // KI_TAKEDOWN_CHAIN_ACHIEVEMENT_MIN -- `cmpwi cr6, r30, 0xA` @0x8238FE84: the takedown-chain
    // length at which the console fires achievement 14 through the achievement manager.
    const s32 KI_TAKEDOWN_CHAIN_ACHIEVEMENT_MIN = 10;

    // The log-once FLAG rung shared by the four parked legs above. Same logger every other
    // GameStateModule partfile uses; each site keeps its own latch so every park is named once.
    void LogParkedLegOnce(bool& lrbLogged, const char* lpcText)
    {
        if (!lrbLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            lrbLogged = true;
            *CgsDev::Log::gpDebugPrint << "[road-rage FLAG] " << lpcText << "\n";
        }
    }
}

// ============================================================================
// ProcessTakedownEvents -- X360 0x8238FC50, whole. Console body in order, per event
// (`TakedownEvent_::GetEvent(queue, i)` @0x8238FCEC, 40-byte stride):
//
//   0x8238FCFC  if (event.meAggressorIndex == GetPlayerActiveRaceCarIndex())      -- the player's kill
//   0x8238FD4C    ldx r6 = gsm + 0x3BE10 + 8*victim   == mLastActiveRaceCarInterface.maRivalIds[victim]
//                 (interface base 235488 + 9776; the two :824/:825 range asserts are GetRivalId's)
//   0x8238FD58    if (!(mpCurrentGameMode && mpCurrentGameMode->IsOnline()))       -- gsm+0x1DB8 / mode+0xAC
//   0x8238FD98      ProgressionManager::OnTakedownTo(actionQ, event.meType, rivalId,
//                                                    meCurrentGameModeType == E_MODE_MARKED_MAN)   [P1]
//                 else
//   0x8238FDA8      lpCarData = mScoringSystem.GetCarData(victim)   (0x8231DCD0, the const twin)
//   0x8238FDBC      CGS_ASSERT(lpCarData, "lpCarData")               (BrnGameStateModule.cpp:4940)
//   0x8238FDD0      if (lpCarData->HasFever())                       (CarData+0x155)
//   0x8238FDE0        if (!IsAchievementEarnt(49)) AchievementEarnt(49)  == OnCaughtFever @0x8235B590 [P2]
//   0x8238FE30        actionQ.AddEvent(<1 byte>, 238 /*E_ACTION_NETWORK_CAUGHT_FEVER*/, 1)
//   0x8238FE54    mScoringSystem.OnPlayerDoesATakedown(gsm+208368 /*Time*/, actionQ)
//   0x8238FE64    if (!IsAchievementEarnt(14) && event.miTakedownChainCount >= 10)
//                   AchievementEarnt(14)                              == AchievementManagerBase::OnTakedownChain [P2]
//   0x8238FEB0    mDeveloperChallengeManager.OnTakedownChain(event.miTakedownChainCount)      [P3]
//   0x8238FEC0    mDeveloperChallengeManager.OnTakedown(meCurrentGameModeType, victim)         [P3]
//   0x8238FEF0    if (!online) mAchievementManager.OnTakedown()                                [P2]
//   0x8238FEF4    telemetry hook: T_BONE(2) -> 13, VERTICAL(3) -> 11 (+ online: profile online
//                 vertical tally++ @gsm+48700 == Profile+412), else 10; AddParameter(player pos);
//                 actionQ.AddEvent(<20 bytes>, 228 /*E_ACTION_SEND_TELEMETRY*/, 20);
//                 if (event.mbMarkedManTakeDown) a second hook 14, same action           [P4]
//   0x8238FFC4  else: the two :824/:825 range asserts on meAggressorIndex (an inlined interface
//               accessor whose value is dead; only its asserts survive)
//   0x82390004  BOTH ARMS: if (event.meVictimIndex == player && mode == E_MODE_MARKED_MAN &&
//               !tip pending && !picture paradise && IsTipAllowedInGameMode(56) &&
//               !profile->HasPlayerSeenTrainingType(56) && since-last-tip >= 5.0)  RequestTip(56)
//
// The loop re-reads the queue length every iteration (0x823900D0); nothing in the body appends
// to that queue, so a hoisted length would be equivalent -- kept as the console has it.
// ============================================================================
void GameStateModule::ProcessTakedownEvents(
        GameStateModuleIO::GameActionQueue*             lpGameActionQueue,
        const CgsModule::EventQueue<TakedownEvent, 8>*  lpTakedownEventQueue,
        GameStateModuleIO::OutputBuffer*                lpOutputBuffer,
        const CgsSystem::TimerStatusInterface&          lrTimerStatusInterface)
{
    static bool sbParkedOnTakedownTo      = false;   // P1
    static bool sbParkedAchievementHooks  = false;   // P2
    static bool sbParkedDevChallengeHooks = false;   // P3
    static bool sbParkedTelemetry         = false;   // P4

    for (s32 liEvent = 0; liEvent < lpTakedownEventQueue->GetLength(); ++liEvent)
    {
        const TakedownEvent& lrEvent = lpTakedownEventQueue->GetEvent(liEvent);

        // ---- the player's own kill --------------------------------------------------------
        if (static_cast<s32>(lrEvent.meAggressorIndex) == static_cast<s32>(GetPlayerActiveRaceCarIndex()))
        {
            const ::EActiveRaceCarIndex leVictimIndex =
                static_cast<::EActiveRaceCarIndex>(lrEvent.meVictimIndex);

            // `ldx r6, (victim + 0x77C2) << 3, this` == the interface's maRivalIds[victim]; the
            // :824/:825 asserts baked here are GetRivalId's own and are reproduced by the call.
            const CgsID lVictimRivalId = mLastActiveRaceCarInterface.GetRivalId(leVictimIndex);

            // gsm+0x1DB8 (mModeManager.mpCurrentGameMode) then mode+0xAC (GameMode::IsOnline) --
            // exactly what the out-of-line IsOnlineGameMode @0x823116D0 evaluates.
            const bool lbOnlineMode = IsOnlineGameMode();

            if (!lbOnlineMode)
            {
                // [P1] FLAG PARKED: BrnProgression::ProgressionManager::OnTakedownTo @0x823666D0
                //     (mProgressionManager, lpGameActionQueue, lrEvent.meType, lVictimRivalId,
                //      GetCurrentGameModeType() == GameStateModuleIO::E_MODE_MARKED_MAN)
                // Asm 0x8238FD7C..0x8238FD98: r3 = this+0xBB30 (mProgressionManager), r4 = r27
                // (the action queue), r5 = event+0x18 (meType), r6 = the ldx above (a 64-bit
                // CgsID -- Hex-Rays rendered only its low word), r7 = (meCurrentGameModeType == 8).
                // DWARF BrnProgressionManager.h:254:
                //     void OnTakedownTo(InputBuffer::GameActionQueue*, BrnGameState::ETakedownType,
                //                       CgsID, bool);
                // Not declared and not bodied anywhere in the tree -> header_request filed. It is
                // the OFFLINE hook (rival-takedown tally, the aggression / takedown training tips
                // 22 and 40), so this park is the one that costs offline road rage something.
                (void)lVictimRivalId;
                LogParkedLegOnce(sbParkedOnTakedownTo,
                    "ProgressionManager::OnTakedownTo @0x823666D0 is not in the tree; the offline "
                    "rival-takedown tally and its training tips are skipped");
            }
            else
            {
                // The console calls the CONST GetCarData twin (0x8231DCD0) on the embedded
                // ScoringSystem (this+0x1DD0 == mModeManager + 0xDB0).
                // ⚠️ THE OVERLOAD IS PINNED THROUGH A MEMBER POINTER. ScoringSystem::GetCarData is
                // overloaded on EActiveRaceCarIndex and on BrnNetwork::NetworkPlayerID (== s32), and
                // two distinct `enum EActiveRaceCarIndex` exist in this tree; an enum that is not
                // the parameter's own would convert to s32 and SILENTLY bind the NetworkPlayerID
                // overload. Naming the by-active-index overload's exact type makes that a compile
                // error instead.
                // (The non-const twin @0x8231DC18 is used because the tree's CarData::HasFever is
                // declared non-const; the two twins are the same search.)
                typedef CarData* (ScoringSystem::*CarDataByActiveIndexFn)(::EActiveRaceCarIndex);
                const CarDataByActiveIndexFn lpfnGetCarData = &ScoringSystem::GetCarData;
                ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
                CarData* lpCarData = (lpScoringSystem->*lpfnGetCarData)(leVictimIndex);
                CGS_ASSERT(lpCarData != 0, "lpCarData");   // BrnGameStateModule.cpp:4940

                // [GUARD] the console asserts and then dereferences anyway; a null here is a crash
                // in a retail build. Early-out added behind the reproduced assert, as the sibling
                // partfiles do.
                //
                // `lbz r11, 0x155(r31)` == CarData+341. The tree's DWARF-ordered CarData puts
                // miCurrentCheckPoint (s8) at +341 and mbHasFever at +342; the action the console
                // [verify V2 2026-09-02] CORRECTION to the two sentences around this line: the tree
                // order is mbIsEliminated +339 / miCurrentCheckPoint +340 / mbHasFever +341, pinned by
                // BrnModeManager_Prepare.cpp:673-681 (PrepareForMode @0x82342930 stores mabPlayerHasFever
                // at car+341) and two Scoring bodies -- so +341 IS mbHasFever and there is no off-by-one.
                // Reads here are by name; nothing behavioural depends on the old claim.
                // posts on this byte -- E_ACTION_NETWORK_CAUGHT_FEVER -- pins its MEANING as "the
                // victim had fever", so the X360 record is one byte off the DecFIGS order here (a
                // merge-window delta of the CarData tail). Read BY NAME as HasFever; the layout
                // question is the ScoringSystem TU's, named here rather than papered over.
                if (lpCarData != 0 && lpCarData->HasFever())
                {
                    // [P2] FLAG PARKED: AchievementManagerBase::OnCaughtFever @0x8235B590 (inlined
                    // here as the vtable pair on id 49, 0x8238FDE0..0x8238FE18). The base TU is
                    // deliberately unmounted (build_game_exe.bat) -- eight unresolved externals.
                    LogParkedLegOnce(sbParkedAchievementHooks,
                        "AchievementManagerBase::OnTakedown / OnTakedownChain / OnCaughtFever are "
                        "unmounted (base TU); the takedown achievements are skipped");

                    // `li r6, 1 / li r5, 0xEE` -- one byte, never initialised by the console (an
                    // empty record); zeroed here so the payload is deterministic.
                    u8 lacCaughtFever[1] = { 0 };
                    lpOutputBuffer->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lacCaughtFever),
                        GameStateModuleIO::E_ACTION_NETWORK_CAUGHT_FEVER, 1);
                }
            }

            // ---- THE ROAD-RAGE COUNTER ------------------------------------------------------
            // 0x8238FE34..0x8238FE54: `lwz r10, 0(this+0x32DF0) / lfs f0, 4(...)` is the 8-byte
            // CgsSystem::Time at gsm+208368 (the sim-timer copy's mTime), passed BY VALUE.
            // See the signature FLAG in the banner for where the PC "now" comes from.
            mModeManager.GetScoringSystem()->OnPlayerDoesATakedown(
                lrTimerStatusInterface.GetSimTimerStatus()->GetTime(), lpGameActionQueue);

            // ---- the chain / developer-challenge / achievement hooks ------------------------
            const s32 liTakedownChainCount = lrEvent.miTakedownChainCount;   // event+0x20

            // [P2] FLAG PARKED: 0x8238FE58..0x8238FEA0 is AchievementManagerBase::OnTakedownChain
            // inlined -- `if (!IsAchievementEarnt(14) && chain >= 10) AchievementEarnt(14)`. The
            // DWARF declares that hook (:168) and the tree's header carries it, but it has NO body
            // and the base TU is unmounted. KI_TAKEDOWN_CHAIN_ACHIEVEMENT_MIN above is the console's
            // 10 so the threshold is not lost with the park.
            (void)KI_TAKEDOWN_CHAIN_ACHIEVEMENT_MIN;

            // [P3] FLAG PARKED: DeveloperChallengeManager::OnTakedownChain(liTakedownChainCount)
            // @0x8238FEB0 and ::OnTakedown(GetCurrentGameModeType(), lrEvent.meVictimIndex)
            // @0x8238FEC0 (this+0x2D570 == mDeveloperChallengeManager). Both bodies exist
            // (BrnDeveloperChallengeManager.cpp:18 / :32) but the TU is unmounted AND the manager is
            // never Construct()ed -- the OnEventEnd precedent in BrnBaselineLinkStubs.cpp.
            LogParkedLegOnce(sbParkedDevChallengeHooks,
                "DeveloperChallengeManager::OnTakedownChain / OnTakedown are unmounted and the "
                "manager is not Construct()ed; the developer-challenge takedown hooks are skipped");
            (void)liTakedownChainCount;

            // [P2] 0x8238FEC4..0x8238FEF0: `if (!online) mAchievementManager.OnTakedown()` -- parked
            // with its siblings above (one latch, one line).

            // ---- the telemetry hook ----------------------------------------------------------
            // 0x8238FEF4..0x8238FF5C selects the hook id from the takedown type: 13 for
            // E_TAKEDOWN_T_BONE, 11 for E_TAKEDOWN_VERTICAL, 10 otherwise. The VERTICAL arm also
            // bumps the profile's online vertical-takedown tally when the mode is online
            // (`addis/addi -> gsm+0x10000-0x4360 == gsm+48288 (Profile) ; lwz/addi/stw 0x19C`).
            // The tally is a real profile store and is kept live; the post itself is parked.
            if (lrEvent.meType == E_TAKEDOWN_VERTICAL && lbOnlineMode)
            {
                mProgressionManager.GetProfile()->IncrementTotalOnlineVerticleTakedownCount();
            }

            // [P4] FLAG PARKED: the E_ACTION_SEND_TELEMETRY post. 0x8238FF60..0x8238FFBC:
            //     TelemetryData lData = { hook, 0 };                      // var_D0 / var_CC
            //     lData.AddParameter(mLastActiveRaceCarInterface.GetPlayerPosition());   // sub_8236A8B8
            //     actionQ.AddEvent(&lData, 228, 20);
            //     if (lrEvent.mbMarkedManTakeDown) { TelemetryData lMarked = { 14, 0 }; AddEvent(228, 20); }
            // BrnNetwork::BrnNetworkModuleIO::TelemetryData (DWARF BrnNetworkSharedIO.h:542, 20
            // bytes: ETelemetryHook meHook + char macBuffer[16]; AddParameter(Vector3) SPrintf's
            // "%i.%i" after range-asserting |x|,|z| < 10000) is not in the tree -> header_request.
            // GetPlayerPosition() is deliberately NOT called while parked: it asserts
            // IsPlayerCarActive() and its only consumer here is the parked record.
            LogParkedLegOnce(sbParkedTelemetry,
                "E_ACTION_SEND_TELEMETRY (228) needs BrnNetwork::BrnNetworkModuleIO::TelemetryData, "
                "not in the tree; the takedown telemetry post is skipped");
        }
        else
        {
            // 0x8238FFC4..0x82390000: the console range-asserts meAggressorIndex against
            // BrnRaceCarEntityModuleOutputInterface.h:824/825 on this arm too -- the residue of an
            // inlined interface accessor whose value is dead. Only the asserts survive; reproduced.
            CGS_ASSERT(static_cast<s32>(lrEvent.meAggressorIndex) >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                       "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(static_cast<s32>(lrEvent.meAggressorIndex) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }

        // ---- BOTH ARMS: the "you were taken down" Marked Man tip -----------------------------
        // 0x82390004..0x823900C0. gsm+0xB630 (46640) is the training manager: +0 meTrainingState
        // (== INACTIVE), +0x14 mbInPictureParadise, +8 mpProgressionManager (+0x170 == Profile,
        // asserted "lpProfile" at BrnTrainingManager.cpp:382), +0x18 mfLastMessageFinishedTime;
        // Profile+0x6C is mfInCarTimePlayed. The whole gauntlet is the inlined tip-request guard
        // the tree already names on TrainingManager (IsTipPending / IsInPictureParadise /
        // GetProfile / GetTimeSinceLastTip / RequestTip -- the BrnDriveThruManager.cpp idiom).
        // [GUARD] mpTrainingManager is a pointer on this build (the console embeds the manager);
        // the null test is the pointer-ness artefact, not a console branch.
        if (static_cast<s32>(lrEvent.meVictimIndex) == static_cast<s32>(GetPlayerActiveRaceCarIndex()) &&
            GetCurrentGameModeType() == GameStateModuleIO::E_MODE_MARKED_MAN &&
            mpTrainingManager != 0 &&
            !mpTrainingManager->IsTipPending() &&
            !mpTrainingManager->IsInPictureParadise() &&
            mpTrainingManager->IsTipAllowedInGameMode(BrnProgression::E_TRAINING_TYPE_TAKEN_DOWN_IN_MARKED_MAN))
        {
            BrnProgression::Profile* lpProfile = mpTrainingManager->GetProfile();
            CGS_ASSERT(lpProfile != 0, "lpProfile");   // BrnTrainingManager.cpp:382
            if (lpProfile != 0 &&
                !lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_TAKEN_DOWN_IN_MARKED_MAN) &&
                mpTrainingManager->GetTimeSinceLastTip() >= KF_TRAINING_TIP_SETTLE_TIME)   // fcmpu/blt @0x823900AC
            {
                mpTrainingManager->RequestTip(BrnProgression::E_TRAINING_TYPE_TAKEN_DOWN_IN_MARKED_MAN);
            }
        }
    }
}

}
