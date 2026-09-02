// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_EventStatusGuiEvents.cpp
//
// ⭐⭐ [E1 event-status wave 2026-08-26] THE EVENT SCORE / TIMER FEED, producer half.
//
// The STUNT SLICE of BrnGame::BrnGameModule::BridgeGameStateToGui @0x823EE880 -- the ONLY
// console producer of the three GUI events that carry an event's score and clock:
//
//     GuiEventCurrentStatus   id 492 size 120   AddGuiEvent @0x823D0DF0
//     GuiEventScoreUpdate     id 424 size  20   AddGuiEvent @0x823D0EA8
//     GuiAttackScoreUpdate    id 428 size  40   AddGuiEvent @0x823D1188
//
// Until this wave nothing on PC wrote GuiCache::{mfEventTime, mfTargetTime, miScoreCurrent,
// miScoreTarget, miScoreCombo, miComboMultiplier}, so every in-event readout rendered
// 0 / 0 / x1 / 0m00s forever. The consumer half is the three matching arms of
// GuiCache::RecEvent (GameSource/Gui/BrnGuiCache.cpp) plus the three ids added to
// BrnGuiModule::DispatchInboundGuiEvents' explicit forward list -- both halves are
// required, neither does anything alone.
//
// ---------------------------------------------------------------------------
// ⛔ WHY A SIBLING TU. Exactly the split precedent already carried by
// GameBridgeGameStateToX_StuntGuiEvents.cpp and GameBridgeGameStateToX_TrainingStringIds.cpp:
// the DWARF home GameBridgeGameStateToX.cpp compiles but CANNOT BE MOUNTED (six symbols in
// its other bodies -- BrnGameState::GetTakedownEventOutputCount / GetTakedownEventOutputRecord
// / GetGameStateInputBindRequestQueue / GetGameStateInputUnbindRequestQueue and the two
// CgsInput::InputIO::PostWorldInputBuffer::Post*Request leaves -- have no definition anywhere
// in src, i.e. six LNK2019s). Landing this slice there would make it unreachable. MOVED, not
// copied: folding it back once those six are homed is a delete, not a duplicate-symbol hunt.
//
// ---------------------------------------------------------------------------
// ⚠️ THE SEAM IS A NAMED FREE FUNCTION, AND IT IS NOT CALLED YET.
// The console body is a BrnGameModule member; its caller (BridgeGameStateToGui itself) lands
// with the ModeManager/GameState wave that is running in parallel, so this wave publishes the
// slice as a free function the conductor wires. THE DECLARATION TO ADD AT THE CALL SITE
// (verbatim -- there is deliberately no new header in this wave):
//
//     namespace BrnGame {
//         void BridgeGameStateToGui_EventStatus(
//             const CgsSystem::TimerStatusInterface*               lpTimerStatusInterface,
//             const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
//             CgsGui::CgsGuiModuleIO::InputBuffer*                 lpGuiInput);
//     }
//
// CALL POSITION, from the console body. BridgeGameStateToGui has two callers --
// BrnGameModule::DoUpdate_GUI @0x823F0758 (call site @0x823F081C) and
// LoadingScriptedState::Update @0x823F22D8. In DoUpdate_GUI it is the THIRD bridge of the
// GUI leg, between BridgeControllerToGui and BridgeDirectorToGui:
//     BridgeWorldToGui -> BridgeControllerToGui -> [BridgeGameStateToGui] ->
//     BridgeDirectorToGui -> BridgeReplayToGui -> BridgeNetworkToGui -> ... -> BridgeGameToGui
// INSIDE BridgeGameStateToGui the slice runs after the game-state GUI-event queue Append
// (@0x823EE9C4) and the two race-distance posts, and BEFORE
// TranslateGameActionsToGuiEvents (@0x823EF22C) and the closing GuiEventTimeInfo post.
// On this build that lands it in BrnGameModule.cpp's GUI sub-step, inside the
// mpGuiInputBuffer->LockForWrite() bracket that already hosts BridgeControllerToGui /
// BridgeGameToGui / BridgeWorldToGui, immediately after the BridgeWorldToGui call and before
// the GuiEventTimeInfo publish -- and it needs the game-state output buffer read-locked, the
// way the world leg brackets mpWorldUpdateOutputBuffer.
//
//     lpTimerStatusInterface  = mTimerStatusInterface (the console's r21 == gm+10095372,
//                               reached by the existing public GetTimerStatusInterface()).
//     ⚠️ On this build mTimerStatusInterface is only published by BridgeTimers, which returns
//     early until the director module reports prepared -- see the ⚠️ on the GuiEventTimeInfo
//     publish in BrnGameModule.cpp, which reads the LIVE mGameTimer for exactly that reason.
//     The three time words below will read zero until that is true. Console-faithful as
//     written; if the conductor wants live values it should pass a TimerStatusInterface
//     filled from mGameTimer, NOT change the reads here.
//
// ---------------------------------------------------------------------------
// ⓘ SCOPE. This is the STUNT slice plus (2026-08-29) the SHOWTIME slice, not the whole
// ~17 KB function. Reproduced verbatim: the id-492 build, the id-424 build (including the
// mode-{3,7,12,14,17} time arm), the mode-{7,9,12,14,17} id-428 build and the mode-{2,16}
// id-434 build. NOT reproduced, and each is a real named gap:
//   * GuiEventRaceDistanceRemaining (239) / GuiEventRaceDistanceToCheckpoint (240)
//     @0x823EEA04..0x823EEB1C -- the per-car race-position feed.
//   * ONE remaining SIBLING arm of the same jpt_823EED94 switch: mode 4 ->
//     GuiPursuitScoreUpdate (432), which needs its own payload reshape AND its own
//     GuiCache::RecEvent arm (case 52 of jpt_825101AC). CONSEQUENCE: Pursuit still shows no
//     mode-specific score. (Mode 3 -> GuiRoadRageScoreUpdate (426) LANDED 2026-09-02, road-rage
//     wave: the arm below, RecEvent case 46, and the id on BrnGuiModule's forward list.)
//     ⛔⛔ THE ROAD-RAGE ID IN THIS BANNER WAS WRONG UNTIL 2026-08-29: it said 429 (and the
//     case list said 46/50/52). 429 is the DecFIGS DWARF id of GuiCrashScoreUpdate, not the
//     X360 id of anything on this switch. AddGuiEvent<GuiRoadRageScoreUpdate> @0x823D0F60
//     posts `li r5, 0x1AA` == 426 (-> RecEvent case 46) and
//     AddGuiEvent<GuiPursuitScoreUpdate> @0x823D1018 posts `li r5, 0x1B0` == 432 (-> case 52).
//     Landing the road-rage arm from the old number would have produced a post no consumer
//     could hear -- 429-380 == 49 is inside jpt_825101AC's DEFAULT case list. Take every id
//     from its instantiation's own `li r5`, never from the DWARF.
//   * GuiEventOnlinePostEvent (@0x823EEEAC..0x823EF19C), GuiEventUpdateEventStarts and
//     GuiEventSpecificPresetRaces (the two memcpy arms) -- online / preset-race feeds.
//   * TranslateGameActionsToGuiEvents / TranslateTakedownsToGuiEvents /
//     TranslateGuiInterfaceToGuiEvents and the closing CgsGui::GuiEventTimeInfo post -- the
//     first already has its own sibling TU, the last already has a live stand-in in
//     BrnGameModule.cpp's GUI leg.
//
// ⓘ IDS ARE PINNED AT BOTH ENDS. 492/424/428 are GUI event ids (their own id space; NOT the
// +5 console shift that applies to game ACTION ids). Producer end: the li r5, 0x1EC / 0x1A8 /
// 0x1AC in each AddGuiEvent<T> instantiation. Consumer end: jpt_825101AC cases 112/44/48 of
// GuiCache::RecEvent @0x8250DDF0, whose third sub-switch rebases by 0x17C (380), so
// 380+112 == 492, 380+44 == 424, 380+48 == 428.
//
// ⓘ EVERY GAME-STATE OFFSET IS READ BY NAME. The console's r27 is
// lpGameStateOutput + 173240 == OutputBuffer::GetScoringOutputInterface(), and its r20 is
// + 175976 == GetOnlineScoringOutputInterface(); the committed BrnGameStateSharedIO.h
// ScoringOutputInterface member run reproduces the console's +0xA34..+0xAA8 block exactly
// (mePlayerRaceCarIndex @+0xA34 through mbTimerActive @+0xAA8), so not one raw byte offset
// appears below. No IO header is edited by this wave.
// ============================================================================

#include "GameSource/Game/GameBridgeGameStateToX.h"          // BrnGame::PushGuiEvent<T>
#include "GameSource/BurnoutConstants.h"                     // EActiveRaceCarIndex + the loop-guard operator++
#include "GameSource/GameState/BrnGameStateModuleIO.h"       // OutputBuffer + the two scoring accessors
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // ScoringOutputInterface / OnlineScoringOutputInterface / EGameModeType
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // GuiEventCurrentStatus / GuiEventScoreUpdate / GuiAttackScoreUpdate
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface / TimerStatus
#include "GameShared/GameClasses/System/Timer/CgsTime.h"     // CgsSystem::Time
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"       // CgsGui::CgsGuiModuleIO::InputBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

namespace BrnGame
{
namespace
{
    // The console's f31 throughout BridgeGameStateToGui: lfs f31, flt_82001CC0
    // (@0x823EE9E0). image.bin at VA 0x82001CC0 reads 00 00 00 00 -- 0.0f. It is the
    // fsel fallback in the mode-{3,7,12,14,17} time arm and the zero every skipped
    // per-car lane is filled with.
    const f32 KF_ZERO = 0.0f;

    // The console indexes the 296-byte maCarScoreData with mePlayerRaceCarIndex through a
    // bare mulli r11, r11, 0x128 (@0x823EEB58 / @0x823EEC64) -- no bound test. On this
    // host that index is E_ACTIVE_RACE_CAR_INDEX_INVALID (-1) whenever no event is running,
    // and this bridge runs every GUI sub-step, so the read is bounded here.
    // [PC GUARD] -- not in the X360 binary. Out of range yields 0.0f rather than whatever
    // the console would have read off the end of the array.
    bool IsPlayerRaceCarIndexInRange(s32 liIndex)
    {
        return (liIndex >= static_cast<s32>(E_ACTIVE_RACE_CAR_INDEX_0)) &&
               (liIndex <  static_cast<s32>(E_ACTIVE_RACE_CAR_INDEX_COUNT));
    }
}

    // =========================================================================
    // ⭐⭐ BridgeGameStateToGui, stunt slice  @ X360 0x823EEB20..0x823EEEAC
    //
    // Signature: the three console inputs this slice actually reads. The console's `this`
    // is used for exactly two things in this span -- the embedded CgsGui::GuiModule at
    // +7252512 (which AddGuiEvent<T> never dereferences; see PushGuiEvent's banner in
    // GameBridgeGameStateToX.h) and the timer status at +10095372 -- so the timer status
    // is passed directly and no BrnGameModule dependency is created.
    //
    // The parameter asserts carry their console file/line
    // (GameBridgeGameStateToX.cpp 423 / 424 / 431 / 432 / 481).
    // =========================================================================
    void BridgeGameStateToGui_EventStatus(
        const CgsSystem::TimerStatusInterface*               lpTimerStatusInterface,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
        CgsGui::CgsGuiModuleIO::InputBuffer*                 lpGuiInput)
    {
        CGS_ASSERT(lpGuiInput != 0, "lpGuiInput");                                  // :423
        CGS_ASSERT(lpGameStateOutput != 0, "lpGameStateOutput");                    // :424
        if (lpGuiInput == 0 || lpGameStateOutput == 0 || lpTimerStatusInterface == 0)
        {
            return;
        }

        const BrnGameState::GameStateModuleIO::ScoringOutputInterface* lpScoringOutputInterface =
            lpGameStateOutput->GetScoringOutputInterface();
        const BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface* lpOnlScoringOutputInterface =
            lpGameStateOutput->GetOnlineScoringOutputInterface();

        CGS_ASSERT(lpScoringOutputInterface != 0, "lpScoringOutputInterface");       // :431
        CGS_ASSERT(lpOnlScoringOutputInterface != 0, "lpOnlScoringOutputInterface"); // :432
        if (lpScoringOutputInterface == 0 || lpOnlScoringOutputInterface == 0)
        {
            return;
        }

        const CgsSystem::TimerStatus* lpGameTimerStatus =
            lpTimerStatusInterface->GetGameTimerStatus();
        CGS_ASSERT(lpGameTimerStatus != 0, "mTimerStatusInterface.GetGameTimerStatus()"); // :481
        if (lpGameTimerStatus == 0)
        {
            return;
        }

        const BrnGameState::GameStateModuleIO::EGameModeType leGameMode =
            lpScoringOutputInterface->meGameModeType;
        const s32 liPlayerRaceCarIndex =
            static_cast<s32>(lpScoringOutputInterface->mePlayerRaceCarIndex);

        // -----------------------------------------------------------------------------
        // 1. GuiEventCurrentStatus (492)   @0x823EEB48..0x823EEC20
        //
        // ⚠️ ZERO-INIT IS A DELIBERATE DEVIATION. The console builds this record in the
        // SAME 144-byte stack slot the id-239 GuiEventRaceDistanceRemaining record was
        // posted from moments earlier (both are addi r4, r1, var_2450), rewrites only
        // +0x00..+0x37, and lets GetAllRemainingCheckpointIndexes fill +0x38.. -- so on
        // console the tail is either checkpoint indexes or the previous record's residue.
        // This TU does not build the id-239 record, so the tail would be uninitialised
        // stack. The consumer only walks it while miNumRemainingCheckpoints > 0, which
        // this slice never sets, so zeroing changes no observable behaviour and removes
        // an uninitialised read.
        // -----------------------------------------------------------------------------
        {
            BrnGui::GuiEventCurrentStatus lCurrentStatus;
            for (s32 liByte = 0; liByte < static_cast<s32>(sizeof(lCurrentStatus)); ++liByte)
            {
                reinterpret_cast<u8*>(&lCurrentStatus)[liByte] = 0;
            }

            // r22 == gm+10095388 is the game TimerStatus' mTime (miSeconds @+0x10,
            // mfFraction @+0x14 of the status); r21+4 / r21+8 are mfBaseTimeStep /
            // mfTimeStepMultiplier, and the console's fmuls f0, f0, f13 of that pair IS
            // TimerStatus::GetCurrentTimeStep().
            const CgsSystem::Time lGameTime = lpGameTimerStatus->GetTime();
            lCurrentStatus.miGameTimeSeconds  = lGameTime.GetSeconds();           // stw  +0x00
            lCurrentStatus.mfGameTimeFraction = lGameTime.GetFraction();          // stfs +0x04
            lCurrentStatus.mfGameTimeStep     = lpGameTimerStatus->GetCurrentTimeStep(); // stfs +0x08

            lCurrentStatus.mfDistanceToFinishLive = KF_ZERO;
            if (IsPlayerRaceCarIndexInRange(liPlayerRaceCarIndex))
            {
                lCurrentStatus.mfDistanceToFinishLive =                           // stfs +0x0C
                    lpScoringOutputInterface->maCarScoreData[liPlayerRaceCarIndex]
                        .GetDistanceToFinishLive();
            }

            lCurrentStatus.mfDistanceDrivenInCurrentCar =                         // stfs +0x10
                lpScoringOutputInterface->mfDistanceDrivenInCurrentCar;

            // the 8-word mtctr 8 copy from r7 == onlineScoring+0x60 == maePlayerTeam
            for (EActiveRaceCarIndex leCar = E_ACTIVE_RACE_CAR_INDEX_0;
                 leCar < E_ACTIVE_RACE_CAR_INDEX_COUNT;
                 leCar++)
            {
                lCurrentStatus.maePlayerTeam[leCar] =                             // stw +0x14 + 4*i
                    static_cast<s32>(lpOnlScoringOutputInterface->maePlayerTeam[leCar]);
            }

            // ⛔ FLAG DEFERRED -- the REMAINING-CHECKPOINT arm (@0x823EEBB4..0x823EEBEC and
            // @0x823EECD8..0x823EED40). The console, and ONLY when meGameModeType ==
            // E_MODE_ONLINE_BURNING_HOME_RUN (13), scans the 8 active-race-car lanes for the
            // first one that is both mabValid[i] and maePlayerTeam[i] == E_PLAYER_TEAM_BLUE_TEAM
            // (2) -- the "runner" -- asserts that index in range
            // ("E_ACTIVE_RACE_CAR_INDEX_COUNT > leRunnerActiveRaceCarIndex" :505 /
            // "E_ACTIVE_RACE_CAR_INDEX_0 <= leRunnerActiveRaceCarIndex" :506) and calls
            // CarCheckpointData::GetAllRemainingCheckpointIndexes @0x823C4D50 to fill
            // payload+0x38 and return the count into payload+0x34. Every other path stores 0
            // (the console's own stw r30, var_241C @0x823EEC10).
            //
            // WHY IT IS NOT REPRODUCED: the call target is
            // lpScoringOutputInterface + 8*(index + 0x128) == scoring+0x940+8*index, i.e. a
            // CarCheckpointData[8] array of 64-bit checkpoint bitmasks that the COMMITTED
            // BrnGameStateSharedIO.h does not model (its member run jumps straight from
            // maCarScoreData to maiCumulativeScoreData -- exactly the "internal alignment
            // padding between the array blocks" that header's banner records, and 8*8 == 64
            // is precisely the +0x940..+0x980 gap). This wave may not edit IO headers, and
            // the arm is dead for anything but ONLINE Burning Home Run, so the count is left
            // at the console's own zero rather than faked.
            // TO LAND IT: carve CarCheckpointData maCarCheckpointData[8] at +0x940 in
            // BrnGameStateSharedIO.h with its GetAllRemainingCheckpointIndexes body
            // (16-entry bound, from that function's own cmpwi r10, 0x10), then replace the
            // zero below with the scan.
            lCurrentStatus.miNumRemainingCheckpoints = 0;                         // stw +0x34

            PushGuiEvent(lCurrentStatus, lpGuiInput);                             // id 492, 120 bytes
        }

        // -----------------------------------------------------------------------------
        // 2. GuiEventScoreUpdate (424)   @0x823EEC24..0x823EED6C
        //
        // THE GATE, verbatim from @0x823EEC24..0x823EEC50: the console computes
        // (mode == E_MODE_OFFLINE_SHOWTIME || mode == E_MODE_ONLINE_SHOWTIME) into a byte,
        // takes the build when that byte is set, and OTHERWISE takes it whenever
        // mode != E_MODE_NONE. Both showtime modes are already != E_MODE_NONE, so the whole
        // test collapses to "any mode but NONE" -- reproduced as written, shape and all, so
        // the next reader sees the same thing the asm does.
        // -----------------------------------------------------------------------------
        const bool lbShowtimeMode =
            (leGameMode == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME) ||
            (leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);

        if (lbShowtimeMode || leGameMode != BrnGameState::GameStateModuleIO::E_MODE_NONE)
        {
            BrnGui::GuiEventScoreUpdate lScoreUpdate;

            lScoreUpdate.meCurrentMedalTarget =                                   // stw  +0x00
                static_cast<s32>(lpScoringOutputInterface->meCurrentMedalTarget);
            lScoreUpdate.mfModeTime =                                             // stfs +0x04
                lpScoringOutputInterface->mfModeTimeElapsed;

            // jpt_823EEC98 over (mode - 3), cases 0,4,9,11,14 -- i.e. the five COUNTDOWN
            // modes E_MODE_ROAD_RAGE(3), E_MODE_STUNT_ATTACK(7), E_MODE_ONLINE_FUGITIVE(12),
            // E_MODE_ONLINE_FREE_BURN(14) and E_MODE_ONLINE_MODE_END(17). They publish TIME
            // REMAINING instead of time elapsed, clamped at zero by
            // fsel f0, f0, f0, f31 (f31 == 0.0f), i.e. (x >= 0.0f) ? x : 0.0f. This is the
            // arm the offline Stunt Run runs down.
            switch (leGameMode)
            {
            case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:          // 3
            case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:       // 7
            case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:    // 12
            case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:   // 14
            case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:    // 17
            {
                const f32 lfModeTimeRemaining = lpScoringOutputInterface->mfModeTimeRemaining;
                lScoreUpdate.mfModeTime =
                    (lfModeTimeRemaining >= KF_ZERO) ? lfModeTimeRemaining : KF_ZERO;
                break;
            }
            default:
                break;
            }

            lScoreUpdate.mfCurrentTargetModeTime =                                // stfs +0x08
                lpScoringOutputInterface->mfCurrentTargetModeTime;

            lScoreUpdate.mfDistanceToNextCheckpoint = KF_ZERO;
            if (IsPlayerRaceCarIndexInRange(liPlayerRaceCarIndex))
            {
                lScoreUpdate.mfDistanceToNextCheckpoint =                         // stfs +0x0C
                    lpScoringOutputInterface->maCarScoreData[liPlayerRaceCarIndex]
                        .GetDistanceToNextCheckpointLive();
            }

            lScoreUpdate.mbTimerActive = lpScoringOutputInterface->mbTimerActive;  // stb +0x10
            lScoreUpdate.maPad11[0] = 0;
            lScoreUpdate.maPad11[1] = 0;
            lScoreUpdate.maPad11[2] = 0;

            PushGuiEvent(lScoreUpdate, lpGuiInput);                               // id 424, 20 bytes

            // -------------------------------------------------------------------------
            // 3. the per-mode score record -- jpt_823EED94 over (mode - 2).
            //    The attack arm (cases 5,7,10,12,15 == modes 7,9,12,14,17) and, since
            //    2026-08-29, the SHOWTIME arm (cases 0,14 == modes 2,16). See the SCOPE
            //    note in the file banner for the road-rage / pursuit siblings that are not.
            // -------------------------------------------------------------------------
            switch (leGameMode)
            {
            // ---------------------------------------------------------------------
            // ⭐⭐⭐ [showtime score wave 2026-08-29] THE SHOWTIME SCORE PRODUCER.
            // @0x823EEE18..0x823EEE44 -- "jumptable 823EED94 cases 0,14", i.e. the arm
            // for E_MODE_OFFLINE_SHOWTIME (2) and E_MODE_ONLINE_SHOWTIME (16) once the
            // table's `addi r11, r11, -2` bias is undone.
            //
            // Four loads, four stores, in the console's own (non-field) order:
            //   0x823EEE18  lwz  r11, 0xA54(r27)  -> stw  var_25B0    (payload +0x00)
            //   0x823EEE24  lfs  f0,  0xA60(r27)  -> stfs var_25A4    (payload +0x0C)
            //   0x823EEE34  lwz  r11, 0xA5C(r27)  -> stw  var_25B0+4  (payload +0x04)
            //   0x823EEE3C  lwz  r11, 0xA58(r27)  -> stw  var_25A8    (payload +0x08)
            // r27 is the ScoringOutputInterface, so every source is a named member of it.
            //
            // ⛔ THIS ARM IS WHY SHOWTIME HAD NO SCORE. Everything on both sides of it was
            // already complete and mounted: CrashModeScoring counts the crashes and the
            // distance, ScoringSystem::WriteDataToOutput @0x8232AE98 publishes them into
            // +0xA54..+0xA60 every frame, GuiCache carries the three destination words and
            // EventInfoComponent::Update already dispatches modes 2/16 to UpdateCrash. The
            // ONLY missing links were this post, RecEvent's case-434 arm, and UpdateCrash's
            // body -- all three land together, because none of them does anything alone.
            // [[the missing producer, not the missing consumer]]
            //
            // ⚠️ miScoreMultiplier (+0x08 <- +0xA58) is posted and then NEVER READ: RecEvent's
            // case-54 arm has no store for it and GuiCache has no member for it. Transcribed
            // because the console stores it; do not "optimise" it away and do not invent a
            // cache slot for it.
            // ---------------------------------------------------------------------
            case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME:   // 2
            case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME:    // 16
            {
                BrnGui::GuiCrashScoreUpdate lCrashScore;

                lCrashScore.miCarsCrashed =                                                   // +0x00
                    lpScoringOutputInterface->miShowtimeCarsCrashed;
                lCrashScore.mfDistanceTravelled =                                             // +0x0C
                    lpScoringOutputInterface->mfShowtimeDistanceTravelled;
                lCrashScore.miComboMultiplier =                                               // +0x04
                    lpScoringOutputInterface->miShowtimeComboMultiplier;
                lCrashScore.miScoreMultiplier =                                               // +0x08
                    lpScoringOutputInterface->miShowtimeScoreMultiplier;

                PushGuiEvent(lCrashScore, lpGuiInput);                            // id 434, 16 bytes
                break;
            }

            case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:       // 7
            case BrnGameState::GameStateModuleIO::E_MODE_TRAFFIC_ATTACK:     // 9
            case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:    // 12
            case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:   // 14
            case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:    // 17
            {
                // @0x823EEE4C..0x823EEEA8, store for store. Field order in the record is
                // NOT the field order in the source struct: the console writes the float
                // (+0x18) before the merged 8-byte maStunts[0] pair (+0x1C/+0x20).
                BrnGui::GuiAttackScoreUpdate lAttackScore;

                lAttackScore.miCurrentScore    = lpScoringOutputInterface->miCurrentScore;    // +0x00
                lAttackScore.miTargetScore     = lpScoringOutputInterface->miTargetScore;     // +0x04
                lAttackScore.miComboScore      = lpScoringOutputInterface->miComboScore;      // +0x08
                lAttackScore.miComboMultiplier = lpScoringOutputInterface->miComboMultiplier; // +0x0C
                lAttackScore.muCurrentStunts   = lpScoringOutputInterface->muCurrentStunts;   // +0x10
                lAttackScore.muAllStunts       = lpScoringOutputInterface->muAllStunts;       // +0x14

                lAttackScore.mfComboWarningTimeActive =                                       // +0x18
                    lpScoringOutputInterface->mfComboWarningTimeActive;

                // the console's single ld r11, 0xA7C / std r11, var_2594 -- two adjacent
                // words the compiler merged, not an 8-byte field.
                lAttackScore.meStuntToDisplayType =                                           // +0x1C
                    static_cast<s32>(lpScoringOutputInterface->maStunts[0].meStuntType);
                lAttackScore.miStuntToDisplayScore =                                          // +0x20
                    lpScoringOutputInterface->maStunts[0].miStuntScore;

                lAttackScore.mbComboWarningActive =                                           // +0x24
                    lpScoringOutputInterface->mbComboWarningActive;
                lAttackScore.mbComboInProgress =                                              // +0x25
                    lpScoringOutputInterface->mbComboInProgress;
                lAttackScore.maPad26[0] = 0;
                lAttackScore.maPad26[1] = 0;

                PushGuiEvent(lAttackScore, lpGuiInput);                           // id 428, 40 bytes
                break;
            }
            // ---------------------------------------------------------------------
            // ⭐⭐ [road-rage wave 2026-09-02] THE ROAD RAGE SCORE PRODUCER.
            // @0x823EEDD8..0x823EEDF8 -- "jumptable 823EED94 case 1", i.e. the arm for
            // E_MODE_ROAD_RAGE (3) once the table's `addi r11, r11, -2` bias is undone.
            //
            // Two loads, two stores, in the console's own order:
            //   0x823EEDD8  lwz  r11, 0xA4C(r27)  -> stw  var_25B0    (payload +0x00)
            //   0x823EEDEC  lwz  r11, 0xA50(r27)  -> stw  var_25B0+4  (payload +0x04)
            // r27 is the ScoringOutputInterface, so both sources are named members
            // (BrnGameStateSharedIO.h:1070-1071, published by ScoringSystem::WriteDataToOutput).
            // Then AddGuiEvent<GuiRoadRageScoreUpdate> @0x823D0F60: `li r6, 8 ; li r5, 0x1AA`
            // -- id 426, 8 bytes. Consumer: GuiCache::RecEvent jpt_825101AC case 46
            // (@0x82510888), which stores the pair into miTakedownsCurrent / miTakedownTarget.
            // ---------------------------------------------------------------------
            case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:          // 3
            {
                BrnGui::GuiRoadRageScoreUpdate lRoadRageScore;

                lRoadRageScore.miCurrentTakedowns =                                           // +0x00
                    lpScoringOutputInterface->miRoadRageNumTakedowns;
                lRoadRageScore.miTargetTakedowns =                                            // +0x04
                    lpScoringOutputInterface->miRoadRageTakedownTarget;

                PushGuiEvent(lRoadRageScore, lpGuiInput);                         // id 426, 8 bytes
                break;
            }

            default:
                // [FLAG] mode 4 (GuiPursuitScoreUpdate 432, jumptable case 2 @0x823EEDFC) still
                // falls through with nothing posted -- see the SCOPE note in the file banner.
                // Modes 2/16 (showtime) and 3 (road rage) no longer do.
                break;
            }
        }
    }
} // namespace BrnGame
