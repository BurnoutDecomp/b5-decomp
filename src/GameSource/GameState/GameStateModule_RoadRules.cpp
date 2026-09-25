// b5-decomp/src/GameSource/GameState/GameStateModule_RoadRules.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h).
//
// ⭐⭐⭐ [bounce wave, 2026-08-27] THE PRODUCER OF GAME ACTION 42 -- the one link that kept the
// entire showtime bounce chain from ever executing on this build.
//
// THE CHAIN, END TO END. Every link but the first was already landed and bodied in this tree;
// each was verified by an earlier wave from its own end, and the first is what this file adds:
//
//   [THIS FILE]  GameStateModule::UpdateRoadRulesManager @0x82381258
//                  -> posts game action 42 with {f32 1.0f @+0, u8 1 @+4}
//   VariableEventQueue<13312,16>              (the shared game-action queue)
//   PhysicsModule::HandleGameActions @0x825A72F0, case 42     (landed by the S3 wave)
//                  -> VehicleManager::StartImpactTime(duration, additive)
//                  -> mbImpactTime = 1 ; mbAftertouchIsForceAdditive = ARG
//   VehiclePhysics::UpdateCrashing @0x82638810, asm 0x82638E30 `beq`
//                  -> the branch that SKIPS UpdateAftertouch unless that byte is set
//   VehiclePhysics::UpdateAftertouch @0x8262EBE8, tail
//                  -> RaceCarPhysics::UpdateShowtimePhysics @0x825FFBD8
//                  -> mfTimeUntilPush, maBounceSensors[20], mfBounceBoostTimer, the aftertouch
//                     tilt channels -- the ~1900 instructions of P6 physics that had NEVER RUN.
//
// HOW THE PRODUCER WAS FOUND (method, because the previous three sweeps missed it). The image was
// scanned for `li r5,<id>` (0x38A000xx) and the hits filtered to those carrying a `li r6,<size>`
// within five instructions BEFORE and a `bl` within five AFTER -- i.e. real AddEvent post sites
// rather than the ~100 places where 42 is simply a constant. Exactly ONE survivor posts 42 into
// the same queue, through the same AddEvent (0x8233FAE8), as the two already-proven action-43
// posts: `li r6,8` @0x823814FC + `li r5,0x2A` @0x82381500 + `bl` @0x82381508.
//
// ⛔ AND IT IS NOT WHERE THE PREVIOUS WAVE'S LEAD POINTED. That lead was
// BrnGui::CrashedHudState::EnterImpactTimeScreen @0x824738C0, on the reasoning that the class was
// absent from the tree. The class is NOT absent (BrnCrashedHudState.cpp, landed by the endcrash
// wave) and that function is a pure apt page-changer: it calls AddOutputAptViewState and
// SetButton and posts nothing at all. The producer is on the GameState side, not the GUI side.
// [[a-state-leaves-itself]] cut the other way here -- the lead was INFERRED, and it was wrong.
#include "GameSource/GameState/BrnGameStateModule.h"

#include <string.h>                                                     // memset (the records)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // GameActionQueue::AddEvent, the game-event walk

#include "GameSource/GameState/BrnGameActions.h"                        // actions 42 / 142 / 275
#include "GameSource/GameState/BrnGameEvents.h"                         // the road-rules event records
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer / ControllerInput
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // E_MODE_OFFLINE_SHOWTIME / E_MODE_ONLINE_SHOWTIME
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // GetCrashScorer / GetPlayerNoInputTime
#include "SharedClasses/StreetData/BrnStreetData.h"                     // StreetData::GetRoadCount

namespace BrnGameState
{

// ============================================================================================
// THE 8-BYTE WIRE RECORD.
//
// The console builds it on the stack at var_A0 and posts `li r6,8` bytes of it (asm
// 0x823814C4..0x82381508):
//     0x823814CC  lfs  f0, flt_82001C98@l(r30)   ; stfs f0, var_A0        -> +0x00  f32
//     0x823814E4  lfs  f0, flt_82001C98@l(r30)   ; stfs f0, var_A0        -> +0x00  (online arm)
//     0x823814F0  stb  r29, var_9C                                        -> +0x04  u8
//     0x823814F4  stb  r29, var_9B                                        -> +0x05  u8
//     0x82381504  addi r4, r1, var_A0                                     -> the record base
// var_9A and var_99 are NEVER WRITTEN by the console -- the last two bytes of the post are stack
// residue. Zeroed here for the same reproducibility reason ImpactTimeEndActionRecord's payload is
// (BrnModeManager_Start.cpp), and named as residue rather than pretended to be data.
//
// ⭐⭐ THIS LAYOUT IS NOT ASSERTED FROM THIS END ALONE. The S3 wave decoded the CONSUMER's arm
// from PhysicsModule::HandleGameActions' asm -- `lfs f1, 0(r29)` and `lbz r5, 4(r29)` -- and
// recorded it as KU_EV_IMPACT_DURATION = 0 / KU_EV_IMPACT_ADDITIVE = 4 in
// BrnPhysicsModuleGameActions.cpp. Producer and consumer were recovered from opposite ends by
// different waves and they meet on the byte.
// ============================================================================================
struct ImpactTimeStartActionRecord
{
    f32 mfImpactTimeDuration;        // +0x00  consumer: StartImpactTime's f1
    u8  mu8ForceAdditiveAftertouch;  // +0x04  consumer: StartImpactTime's r5 -> the gate byte
    u8  mu8Field05;                  // +0x05  written 1 by the console; NO consumer arm reads it
    u8  maResidue06[2];              // +0x06  never written by the console (stack residue)
};

// X360 0x823814FC `li r6,8` -- WIRE FORMAT, the record crosses into the shared 13312-byte
// VariableEventQueue. Pinned here rather than passed as a magic number at the call site.
static_assert(sizeof(ImpactTimeStartActionRecord) == 8,
              "X360 UpdateRoadRulesManager posts action 42 with size 8");

namespace
{
    // Metres to yards, the showtime score's distance unit. The same image word ModeManager's
    // results arm reads (0x3F8BFB85), dumped there.
    const f32 KF_METRES_TO_YARDS = 1.0936132669448853f;
}

// ============================================================================================
// GameStateModule::UpdateRoadRulesManager -- the whole function (283 instructions).
//
// Caller: EmmPreWorldUpdate, on its not-sim-paused arm, straight after the ModeManager tick
// (GameStateModule_gUI_00.cpp leg 1c). Arguments: the output buffer, and the pre-world input
// buffer's ControllerInput (read under the buffer's read lock).
//
// Everything runs under one guard, the inlined IsPlayerCarActive() of the module's cached
// active-race-car snapshot. Inside it, in the console's order:
//   (a) the StreetData range assert on the player's current road index;
//   (b) every frame of a showtime mode: the showtime score into the road-rules manager and
//       action 142 (the showtime score update);
//   (c) on the frame showtime starts: action 42 (impact time);
//   (d) meFreeburnChallengeStyle from the ChallengeManager;
//   (e) RoadRulesManager::Update with its twelve arguments.
//
// Module members the arms read, by console offset:
//   +0x1DB4          the current game mode type (GetCurrentGameModeType)
//   +0x20CC/+0x20D4/+0x20F8  the offline CrashModeScoring's miBaseScore / miScoreMultiplier /
//                    mfDistanceTravelled (ModeManager +0xDB0 -> ScoringSystem +0x20)
//   +0x7AC4          ScoringSystem::mfPlayerTimeWithoutInput
//   +0x2CDC0         mCarSelectManager.mJunkyardId (IsInJunkyard)
//   +0x2CE34         mOnlineCarSelectManager.mbIsInOnlineCarSelect
//   +0x32DB8         meFreeburnChallengeStyle
//   +0x32DC5         mbFreeburnChallengeSelectorVisible
//   +0x38B64         meControllerState (IsControllerActive)
//   +0x38B68         mLocalPlayerNetworkID
//   +0x45761         mbWasInShowtimeGameMode
//   +0x47430/+0x47484  mStreetManager's StreetData and current player road index
//   +0x475BC         mfSimTimeStep
// ============================================================================================
void GameStateModule::UpdateRoadRulesManager(GameStateModuleIO::OutputBuffer*          lpOutputBuffer,
                                             const GameStateModuleIO::ControllerInput* lpControllerInput)
{
    if (!mLastActiveRaceCarInterface.IsPlayerCarActive())
    {
        return;
    }

    // ---- (a) ------------------------------------------------------------------------------
    // The console streams "<index> < <count>" into the assert buffer; the literal part is " < ".
    const BrnStreetData::RoadIndex liCurrentRoadIndex = mStreetManager.GetCurrentPlayerRoadIndex();
    CGS_ASSERT(liCurrentRoadIndex < mStreetManager.GetStreetData()->GetRoadCount(), " < ");

    // ---- (b) ------------------------------------------------------------------------------
    // Every frame of a showtime mode, no edge latch. The score is computed the way ModeManager's
    // results arm computes it: metres to whole yards, a hundred points a yard, plus the damage
    // score, all times the crash multiplier. The console evaluates the expression twice (once for
    // the store, once for the record) from the same three members; it is evaluated once here.
    {
        const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
        if (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
            leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            const CrashModeScoring* lpCrashScorer = mModeManager.GetScoringSystem()->GetCrashScorer();
            const s32 liYards = static_cast<s32>(lpCrashScorer->GetDistanceTravelled() * KF_METRES_TO_YARDS);
            const s32 liShowtimeScore =
                (liYards * 100 + lpCrashScorer->miBaseScore) * lpCrashScorer->GetScoreMultiplier();

            mRoadRulesManager.SetShowtimeScore(liShowtimeScore);

            GameStateModuleIO::ShowtimeUpdateAction lShowtimeUpdate;
            lShowtimeUpdate.meActiveRaceCarIndex = mLastActiveRaceCarInterface.GetPlayerActiveRaceCarIndex();
            lShowtimeUpdate.mNetworkPlayerID     = mLocalPlayerNetworkID;
            lShowtimeUpdate.miShowtimeScore      = liShowtimeScore;
            // Console size 12 (pointer-free record, host size equal).
            lpOutputBuffer->GetGameActionQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lShowtimeUpdate),
                GameStateModuleIO::E_ACTION_SHOWTIME_UPDATE,
                static_cast<s32>(sizeof(lShowtimeUpdate)));
        }
    }

    // ---- (c) ------------------------------------------------------------------------------
    // The showtime rising edge. The latch is re-stored with the current truth on every frame the
    // guard passes, before the edge is tested.
    {
        const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
        const bool lbInShowtime =
            (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME
          || leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);
        const bool lbRisingEdge = (lbInShowtime && !mbWasInShowtimeGameMode);
        mbWasInShowtimeGameMode = lbInShowtime;

        if (lbRisingEdge)
        {
            ImpactTimeStartActionRecord lRecord;
            std::memset(&lRecord, 0, sizeof(lRecord));   // +0x06..+0x07: the console posts stack residue

            // Both arms load the same 1.0f literal; the online branch is the console's.
            lRecord.mfImpactTimeDuration = 1.0f;
            if (IsOnlineGameMode())
            {
                lRecord.mfImpactTimeDuration = 1.0f;
            }
            lRecord.mu8ForceAdditiveAftertouch = 1;
            lRecord.mu8Field05                 = 1;

            lpOutputBuffer->GetGameActionQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord),
                GameStateModuleIO::E_ACTION_IMPACT_TIME_START,
                static_cast<s32>(sizeof(ImpactTimeStartActionRecord)));

            // [DIAG] NOT IN THE CONSOLE BINARY. One line per showtime entry (the post is edge-gated).
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[bounce] POSTED game action 42 IMPACT_TIME_START: modeType "
                    << static_cast<s32>(leGameModeType)
                    << " online "        << static_cast<s32>(IsOnlineGameMode() ? 1 : 0)
                    << " duration "      << lRecord.mfImpactTimeDuration
                    << " additive "      << static_cast<s32>(lRecord.mu8ForceAdditiveAftertouch)
                    << " field05 "       << static_cast<s32>(lRecord.mu8Field05)
                    << " size "          << static_cast<s32>(sizeof(ImpactTimeStartActionRecord))
                    << "\n";
            }
        }
    }

    // ---- (d) ------------------------------------------------------------------------------
    // The embedded ChallengeManager (gsm +0x7E20) style, handed to Update below.
    meFreeburnChallengeStyle = mModeManager.GetChallengeStyle();

    // ---- (e) ------------------------------------------------------------------------------
    // lbShowtimeActive: a showtime mode that is not in one of its post-event states (the inlined
    // ModeManager::IsInPostEvent, states 3 / 5 / 4).
    const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
    const bool lbShowtimeActive =
        (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME
      || leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        && !mModeManager.IsInPostEvent();

    // lbDisableSwitchingOnline: the freeburn challenge selector is up, or the controller is not
    // active (the inlined IsControllerActive, states 3 / 0).
    const bool lbDisableSwitchingOnline = mbFreeburnChallengeSelectorVisible || !IsControllerActive();

    // lbCarSelectActive: in a junkyard, or in the online car select.
    const bool lbCarSelectActive =
        mCarSelectManager.IsInJunkyard() || mOnlineCarSelectManager.IsInOnlineCarSelect();

    // The player's RaceCarState, fetched twice by the console (once per read).
    const bool lbInAir =
        mLastActiveRaceCarInterface.GetRaceCarStateMutable(GetPlayerActiveRaceCarIndex())->mfTimeInAir > 0.0f;
    const f32  lfPlayerNoInputTime = mModeManager.GetScoringSystem()->GetPlayerNoInputTime();
    const bool lbPlayerIsCrashing =
        mLastActiveRaceCarInterface.GetRaceCarStateMutable(GetPlayerActiveRaceCarIndex())->mbCrashing;

    mRoadRulesManager.Update(lpControllerInput,
                             liCurrentRoadIndex,
                             mfSimTimeStep,
                             lbInAir,
                             lbPlayerIsCrashing,
                             lpOutputBuffer,
                             lbShowtimeActive,
                             leGameModeType,
                             lfPlayerNoInputTime,
                             lbCarSelectActive,
                             lbDisableSwitchingOnline,
                             meFreeburnChallengeStyle);
}

// ============================================================================================
// ProcessGameEventsRoadRulesBringUp -- the road-rules and street-manager arms of
// GameStateModule::ProcessGameEvents, as one walk over the merged game-event queue (the tree's
// per-family split of the console's single switch; see PreWorldUpdateStuntBringUp).
//
//   case  96  road-rules data request (CgsID)      -> RoadRulesManager::OnRoadRulesDataRequest
//   case  97  road score request                   -> StreetManager::ProcessScoreRequestEvent
//   case  98  batch road-rules query               -> StreetManager::FillInRoadRulesQuery, action 275
//   case  99  road-rule interaction change (u8)    -> RoadRulesManager::SetSwitchingActive
//   case 100  road-rule mode switch (u8)           -> RoadRulesManager::SetRoadRulesMode
//   case 103  GUI switches the road-rule state (u32) -> RoadRulesManager::SetActiveRoadRule
//   case 130  online personal best received        -> StreetManager::ProcessNetworkHighScoreEvent
//   case 131  road-rules scores uploaded           -> StreetManager::ProcessUploadEvent
//   case 132  road-rules scores downloaded         -> StreetManager::ProcessDownloadEvent
//   case 133  road-rules server connect info       -> StreetManager::ProcessConnectedOnlineEvent
//   case 150  buddy removed                        -> StreetManager::ProcessBuddyRemoved
//
// Event ids 96..103 are written as literals: BrnGameEvents.h has no enumerators for them (their
// names are the reference enum's, one higher there), and its road-score-request enumerator
// carries a placeholder value.
// ============================================================================================
void GameStateModule::ProcessGameEventsRoadRulesBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue*            lpActionQueue,
        GameStateModuleIO::OutputBuffer*               lpOutputBuffer)
{
    if (lpGameEventQueue == 0 || lpActionQueue == 0 || lpOutputBuffer == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        switch (liType)
        {
        case GameStateModuleIO::E_EVENT_ROAD_RULE_DATA_REQUEST:   // the road's CgsID (0 == the current road)
            mRoadRulesManager.OnRoadRulesDataRequest(*reinterpret_cast<const CgsID*>(lpEvent), lpActionQueue);
            break;

        case GameStateModuleIO::E_EVENT_ROAD_RULE_ROAD_SCORE_REQUEST:
            mStreetManager.ProcessScoreRequestEvent(
                lpOutputBuffer, reinterpret_cast<const GameStateModuleIO::RoadRulesScoreRequestEvent*>(lpEvent));
            break;

        case GameStateModuleIO::E_EVENT_ROAD_RULE_BATCH_DATA_REQUEST:
        {
            GameStateModuleIO::RoadRulesBatchQueryAction lQuery;
            std::memset(&lQuery, 0, sizeof(lQuery));   // the console posts the stack record as filled
            mStreetManager.FillInRoadRulesQuery(&lQuery);
            // Console size 776 (pointer-free record, host size equal).
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lQuery),
                                    GameStateModuleIO::E_ACTION_ROAD_RULES_BATCH_QUERY,
                                    static_cast<s32>(sizeof(lQuery)));
            break;
        }

        case GameStateModuleIO::E_EVENT_ROAD_RULE_INTERACTION_CHANGE:   // one byte
            mRoadRulesManager.SetSwitchingActive(*reinterpret_cast<const bool*>(lpEvent));
            break;

        case GameStateModuleIO::E_EVENT_ROAD_RULE_MODE_SWITCH:   // one byte, true == online
            mRoadRulesManager.SetRoadRulesMode(lpOutputBuffer, *reinterpret_cast<const bool*>(lpEvent));
            break;

        case GameStateModuleIO::E_EVENT_GUI_SWITCHES_ROAD_RULE_STATE:   // 0 none, 1 time, 2 crash
        {
            const u32 luRoadRuleState = *reinterpret_cast<const u32*>(lpEvent);
            if (luRoadRuleState == 0)
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_NONE);
            }
            else if (luRoadRuleState == 1)
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue,
                    mModeManager.IsOnlineGameMode() ? E_ACTIVE_ROAD_RULE_ONLINE_TIME
                                                    : E_ACTIVE_ROAD_RULE_OFFLINE_TIME);
            }
            else if (luRoadRuleState == 2)
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue,
                    mModeManager.IsOnlineGameMode() ? E_ACTIVE_ROAD_RULE_ONLINE_CRASH
                                                    : E_ACTIVE_ROAD_RULE_OFFLINE_CRASH);
            }
            else
            {
                CGS_ASSERT(false, "Unknown road rule");
            }
            break;
        }

        case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_PB_RECV:   // 130
            mStreetManager.ProcessNetworkHighScoreEvent(
                lpOutputBuffer,
                reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent*>(lpEvent));
            break;

        case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_UPLOADED:   // 131
            mStreetManager.ProcessUploadEvent(
                reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesUploadedEvent*>(lpEvent));
            break;

        case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED:   // 132
            mStreetManager.ProcessDownloadEvent(
                reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesDownloadedEvent*>(lpEvent));
            break;

        case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO:   // 133
        {
            const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent* lpRRConnectedOnlineEvent =
                reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent*>(lpEvent);
            CGS_ASSERT(lpRRConnectedOnlineEvent, "lpRRConnectedOnlineEvent");
            mStreetManager.ProcessConnectedOnlineEvent(lpOutputBuffer, lpRRConnectedOnlineEvent);
            break;
        }

        case GameStateModuleIO::E_EVENT_BUDDY_REMOVED:   // 150
        {
            const GameStateModuleIO::BuddyRemovedEvent* lpBuddyRemovedEvent =
                reinterpret_cast<const GameStateModuleIO::BuddyRemovedEvent*>(lpEvent);
            CGS_ASSERT(lpBuddyRemovedEvent, "lpBuddyRemovedEvent");
            mStreetManager.ProcessBuddyRemoved(lpOutputBuffer, lpBuddyRemovedEvent);
            break;
        }

        default:
            break;
        }

        const CgsModule::Event* lpNext = 0;
        liType  = lpGameEventQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
}

}  // namespace BrnGameState
