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

#include <stdlib.h>                                                     // getenv (the witness gate)
#include <string.h>                                                     // memset (the record)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // GameActionQueue::AddEvent

#include "GameSource/GameState/BrnGameActions.h"                        // E_ACTION_IMPACT_TIME_START
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // GameActionQueue
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // E_MODE_OFFLINE_SHOWTIME / E_MODE_ONLINE_SHOWTIME

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

// ============================================================================================
// ONE ARM of X360 BrnGameState::GameStateModule::UpdateRoadRulesManager @0x82381258 (283 insns).
//
// ⚠️ THIS IS A PARTIAL AND THE BOUNDARY IS STATED. The console function has five arms inside its
// one guard. This file lands the guard and arm (c). The others are DEFERRED BY NAME, each one
// written out below rather than omitted, and each is deferred because its callee does not exist
// in this tree:
//
//   (a) the StreetData range assert (`StreetData_::oper(this+291888)` +0x20 vs this+291972).
//       DEFERRED: BrnStreetData::StreetData_::oper is not in the tree. It is an assert only --
//       nothing downstream of it changes state.
//   (b) the action-142 post, 12 bytes, gated on the SAME showtime mode test as (c) but with no
//       edge latch, so it fires EVERY frame of showtime. DEFERRED: its payload is assembled from
//       four unnamed module offsets (+8440, +8404, +8396, +232296) plus
//       RCEntityActiveRaceCarOutputInterface::GetPlay, and this tree has named members for none
//       of them. ⛔ Guessing them would post plausible garbage onto a live wire queue --
//       [[silent-drop-stubs]] in its most expensive form. Not attempted.
//   (c) ⭐ THE ACTION-42 POST -- LANDED BELOW. Self-contained: two module scalars and a literal.
//   (d) `this+208312 = ChallengeManager::GetChalle(this+32288)`. DEFERRED: the export's name is
//       truncated in the IDA set and the destination offset is un-homed here.
//   (e) the tail call RoadRulesManager::Update(this+183592, ...10 arguments...). DEFERRED:
//       BrnGameState::RoadRulesManager DOES NOT EXIST IN THIS TREE AT ALL (the only RoadRules
//       class present is BrnNetwork::NetworkRoadRulesManager, a different class). This is the
//       function's namesake arm and it is the whole reason the function is not landed entire.
//
// ⇒ What this file claims is exactly this: the console's action-42 post, at the console's own
// position in the frame, under the console's own guard and the console's own edge condition.
// It does not claim to be UpdateRoadRulesManager.
//
// THE GUARD (asm 0x823812A8..0x823812C4). ⚠️ NOTE THE INVERSION -- the `beq` on index == -1
// SKIPS the byte load, so an invalid index leaves the register at the `li r11, 0` that preceded
// the compare:
//     lwz r11, 0x2858(r23)   ; iface+10328 == mePlayerActiveRaceCarIndex
//     cmpwi cr6, r11, -1
//     li   r11, 0
//     beq  cr6, loc_823812BC ; <- jumps PAST the lbz
//     lbz  r11, 0x2860(r23)  ; iface+10336 == IsPlayerCarActive
//     ...  beq cr6, <function tail>
// r23 == this + 235488 == mLastActiveRaceCarInterface. ⭐ Both offsets were named by an earlier
// and unrelated wave (BrnStuntModeScoring.h:378-379 spells out "*(a2+10328) ==
// GetPlayerActiveRaceCarIndex ... *(a2+10336) == IsPlayerCarActive"), which is the calibration
// control for this decode: two offsets recovered here matched two names recovered there.
// ============================================================================================
void GameStateModule::UpdateRoadRulesManagerImpactTimeBringUp(
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (lpActionQueue == 0)
    {
        return;
    }

    // ---- the guard, by name ------------------------------------------------------------------
    if (mLastActiveRaceCarInterface.GetPlayerActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        return;
    }
    if (!mLastActiveRaceCarInterface.IsPlayerCarActive())
    {
        return;
    }

    // ---- arm (c): the showtime RISING EDGE ---------------------------------------------------
    // asm 0x8238145C..0x823814C0. `lwz r8, 0x1DB4(r31)` is this+7604, the module's cached
    // current-game-mode-type scalar -- the same read BrnGameStateModule.h:866 already documents
    // behind GetCurrentGameModeType(). It is compared against 2 and 0x10, and this tree already
    // carries both of those values BY NAME (BrnGameStateSharedIO.h:64/80): E_MODE_OFFLINE_SHOWTIME
    // and E_MODE_ONLINE_SHOWTIME. Two independent recoveries, one meaning.
    const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
    const bool lbInShowtime =
        (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME
      || leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);

    // `lbzx r11, r31, r9` @0x82381484 reads the latch, `stbx r11, r31, r9` @0x823814B8 re-stores
    // the CURRENT truth into it -- unconditionally, on every frame the guard passes, whether or
    // not the post fires. The console stores the new value BEFORE testing the edge; the order
    // does not matter to the result but it is kept here so the read matches the asm.
    const bool lbRisingEdge = (lbInShowtime && !mbWasInShowtimeGameMode);
    mbWasInShowtimeGameMode = lbInShowtime;

    if (!lbRisingEdge)
    {
        return;
    }

    ImpactTimeStartActionRecord lRecord;
    std::memset(&lRecord, 0, sizeof(lRecord));   // +0x06..+0x07: the console posts stack residue

    // ⚠️⚠️ BOTH ARMS LOAD THE SAME LITERAL, AND THAT IS THE BINARY'S DOING, NOT HEX-RAYS'.
    // 0x823814CC and 0x823814E4 are both `lfs f0, flt_82001C98@l(r30)` with the SAME r30 --
    // literally the same address, not two constants that happen to print alike. Read out of the
    // image with x360rd: flt_82001C98 == 0x3F800000 == 1.0f. IMAGE-CITED, not guessed
    // [[reconstruction-gotchas]]. The online branch is kept as a branch because the console has
    // it -- collapsing it would erase the evidence that the two arms were once different.
    lRecord.mfImpactTimeDuration = 1.0f;                 // flt_82001C98
    if (IsOnlineGameMode())
    {
        lRecord.mfImpactTimeDuration = 1.0f;             // flt_82001C98 -- the same address
    }

    // `stb r29, var_9C` / `stb r29, var_9B`; r29 is the function's constant 1.
    lRecord.mu8ForceAdditiveAftertouch = 1;
    lRecord.mu8Field05                 = 1;

    lpActionQueue->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lRecord),
        GameStateModuleIO::E_ACTION_IMPACT_TIME_START,
        static_cast<s32>(sizeof(ImpactTimeStartActionRecord)));

    // ---- [DIAG] NOT IN THE X360 BINARY -------------------------------------------------------
    // ⭐ PRINT A VALUE, NOT A FACT. The S3 wave's case-23 read was 404 bytes out of bounds and
    // returned a clean 0x00000000 -- the most plausible-looking wrong answer available -- and the
    // ONLY reason it was caught is that its witness printed the number instead of announcing that
    // the arm had run. So this one prints the mode type it actually saw, the duration it actually
    // wrote and the byte it actually set. The post is edge-gated, so it is naturally one-shot per
    // showtime entry; no first-N cap is needed.
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

}  // namespace BrnGameState
