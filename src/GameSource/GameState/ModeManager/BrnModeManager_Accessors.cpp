// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_Accessors.cpp
//
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// AGENT 9 of the stuntrace wave-B ModeManager keystone: THE ACCESSOR CLOSURE -- every small
// query the spine calls that the console INLINES (so it has no export of its own), plus the two
// X360 symbols small enough to live with them (GetRoadRageTakedownTarget @0x82327518 and
// GetNumberOfCarsInFlyby @0x82311E38).
//
// ============================================================================================
// THE RULE THIS FILE IS WRITTEN UNDER
// ============================================================================================
// Every cross-object read the console open-codes as a raw offset off mpGameStateModule /
// mpTriggerQueryManager / mpNetworkRoundManager is reached HERE BY NAME, through an accessor that
// exists on the owning class. Nothing in this file fabricates an offset. Where the owning class
// did not have the accessor, agent 9 grew it in the owning header (that grow is agent 9's own
// lane -- BrnGameStateModule.{h,cpp} and BrnTriggerQueryManager.h -- and each carries its own
// X360 attestation banner at the declaration):
//   * GameStateModule::GetLastGlobalRaceCarInterface()  (new member + accessor, X360 gsm+245968)
//   * GameStateModule::GetNetworkRandomSeed()           (new member + accessor, X360 gsm+208300)
//   * TriggerQueryManager::GetTrafficData()             (new inline over the existing
//                                                        mpTrafficData @TQM+1600)
// and three that needed NO grow at all, because the accessor was already there:
//   * GameStateModule::GetLastActiveRaceCarInterface()  (X360 gsm+235488) -- existing
//   * TriggerQueryManager::GetTriggerData()             (X360 TQM+1568)   -- existing inline
//   * NetworkRoundManager::GetCurrentRound()            (X360 NRM+300 - NRM+296 - 1)
//     [!] CORRECTED 2026-08-26 by the wave-B fix round: this one was NOT "existing" -- it was
//     DECLARE-ONLY, and two groups of wave-B bodies were spelling the same console expression
//     with opposite meanings. The fix round ruled the semantics (this file's reading won, on the
//     RichPresenceManagerBase::SetRoundParameter precedent) and landed the body plus a
//     GetRoundsRemaining() sibling. See the ruling banner in BrnNetworkRoundManager.cpp.
//
// ============================================================================================
// FOUR CORRECTIONS TO header_grow_spec SECTION 6, ALL ASM-PINNED THIS WAVE
// ============================================================================================
// (A) SPEC ITEMS (1) AND (2) ARE THE SAME OFFSET. "gsm+0x3C0D0" and "gsm+0x245968" are one number
//     written two ways: 0x3C0D0 == 245968. It is the GLOBAL race-car output interface, and
//     GameStateModule has NO live-active interface member at all -- the live one arrives through
//     the world module's input buffer and is only ever COPIED into the two cached snapshots
//     (PostWorldUpdate @0x8238F358: `XMemCpy(this+235488, <active>, 10480);
//      XMemCpy(this+245968, <global>, 2416);`). A sweep of every ModeManager export for those two
//     numbers (10 hits) says the same thing:
//        245968 -> WriteDataToOutput, HasRaceCarHitValidCheckpoint,
//                  TransmitAndIncrementCheckPointsReached, TransmitAndIncrementFinishReached,
//                  HandleBurningHomeRunRunnerSwitch      (all the GLOBAL interface)
//        235488 -> StartGameMode, RemoteRaceCarHitsCheckpoint  (mLastActiveRaceCarInterface)
//     => ModeManager::GetActiveRaceCarOutputInterface() HAS NO CONSOLE CALL SITE IN THIS TU and is
//     deliberately NOT bodied here (see section 8); it is filed as a header_request to retire or
//     re-point the declaration. Bodying it would have to invent a source, and conductor decision
//     #7 forbids collapsing it into either real embed.
// (B) SPEC ITEM (4) NEEDS NO GameStateModule GROW. The console's
//     `*(gsm+0xBB24) - *(gsm+0xBB20) - 1` is computed off gsm+0xB9F8 (== gsm+47608) at +0x12C and
//     +0x128, and gsm+47608 IS the NetworkRoundManager: GameStateModule::Construct @0x82380388
//     passes `a1 + 47608` as ModeManager::Construct's NetworkRoundManager argument. So those two
//     words are NRM::miTotalRounds (+300) and NRM::miRoundsRemaining (+296), and the whole
//     expression is the already-committed NetworkRoundManager::GetCurrentRound() -- the same
//     de-inlining RichPresenceManagerBase::SetRoundParameter @0x8235A6C8 already landed, and the
//     same quantity HandleLoadingScreenLoaded computes for its round argument. ModeManager reaches
//     it through the mpNetworkRoundManager it already holds; mpGameStateModule is not involved.
// (C) SPEC ITEM (5) WAS ALREADY LANDED. TriggerQueryManager::GetTriggerData() exists as an inline
//     over mpTriggerData @TQM+1568. What was missing is its traffic sibling at TQM+1600, grown
//     this wave (see the banner there).
// (D) THE HEADER'S GlobalToActiveRaceCarIndex FLAG IS REFUTED. It says "the interface names only
//     the active->global direction". It does not:
//     BrnRaceCarEntityModuleOutputInterface.h:327 declares
//     `RCEntityGlobalRaceCarOutputInterface::GetActiveRaceCarIndex(EGlobalRaceCarIndex) const`
//     and BrnRCEntityGlobalRaceCarOutputInterface.cpp:129 bodies it (X360 0x821F46C8) -- and its
//     two range asserts are baked at BrnRaceCarEntityModuleOutputInterface.h:1512/1513, which are
//     EXACTLY the two assert lines HasRaceCarHitValidCheckpoint @0x82329910 fires around its
//     `*(4*(global+525)+iface)` read. 4*525 == 2100 == the interface's own maeActiveRaceCarIndices
//     at +0x834. So that read is an inlined GetActiveRaceCarIndex and nothing else. Bodied below;
//     no fabrication, no grow.
//
// ============================================================================================
// DO-NOT-REIMPLEMENT (hazards H2)
// ============================================================================================
// The sixteen committed bodies in BrnModeManager.cpp -- IsOnlineGameMode, IsInPostEvent,
// GetNextLandmarkIndex, CountCheckpointsRemaining, MarkCarHittingCheckpoint,
// ResetCheckpointDataForNextLap, HasRaceCarHitValidCheckpoint, HasPlayerWon, SetupStuntChallenge,
// EndStuntChallenge, SetNetworkStuntScore, GetCheckpointPosition, WriteDataToOutput,
// FillInRaceDistanceInterface, SendModeResults, TellGuiToShowOnlineFinalStandings -- are CALLED,
// never redefined. GetScoringSystem's two overloads stay in ModeManager_gUI_00.cpp (hazards H1).
// ============================================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"                         // mpGameStateModule accessors
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"          // GameMode::GetCurrentState
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h" // NetworkRoundManager::GetCurrentRound
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // TQM::GetTriggerData / GetTrafficData
// The race-car output interfaces are completed here so GetActiveRaceCarIndex can be called by name.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // the two parked one-shot logs

// [X][X] DELIBERATELY NOT INCLUDED: GameSource/GameState/BrnGameStateModuleIO.h. It reaches a
// SECOND `enum EActiveRaceCarIndex : s32` inside namespace BrnGameState (through
// GameSource/Network/BrnNetworkModuleIO.h -> BrnTakedownManagerTypes.h), which would silently
// re-bind the unqualified EActiveRaceCarIndex in GlobalToActiveRaceCarIndex's signature below and
// re-mangle an already-committed declaration. Nothing here needs it -- every enum used in this
// file comes from BrnGameStateSharedIO.h or BurnoutConstants.h through the owning header.

namespace BrnGameState
{

// ============================================================================================
// 1. CURRENT-MODE STATE QUERIES
// ============================================================================================

// PINNED FROM ASM. FinishOfflineModeIntro @0x823119B0 opens with
//   `if ( !*(a1 + 3480) ) { ... FireAssert("IsInGameMode()", ".../BrnModeManager.cpp", 2521); }`
// -- i.e. the whole of IsInGameMode() is the null test on mpCurrentGameMode (+0xD98). The same
// shape leads IsOnlineGameMode @0x82311410 and IsInPostEvent @0x823113D0.
bool ModeManager::IsInGameMode() const
{
    return (mpCurrentGameMode != nullptr);
}

// PINNED FROM ASM (two independent inlined call sites, both rendering identically):
//   GameStateModule::ShouldStartShowtimeMode @0x82356B18:
//       `v11 = *(a1 + 7608); if ( !v11 || *(v11 + 40) == 2 )`  == !IsInGameMode() || IsInProgress()
//   TrainingManager::RequestTraining @0x82365B20:
//       `v8 = *(v7 + 7608); v9 = v8 && *(v8 + 40) == 2;`       == IsInProgress()
// (gsm+7608 IS mModeManager.mpCurrentGameMode -- gsm+4128 + 0xD98 -- previously proven; mode+40 is
// GameMode::meCurrentState, the same word IsInPostEvent reads.) 2 == E_GMS_IN_PROGRESS.
bool ModeManager::IsInProgress() const
{
    if (mpCurrentGameMode == nullptr)
    {
        return false;
    }
    return (mpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS);
}

// [!] PROVISIONAL -- SEMANTICS DERIVED, NOT ASM-PINNED, AND SAID SO OUT LOUD.
// Unlike its two siblings, IsInPreEvent() has NO surviving out-of-line copy and NO inlined call
// site anywhere in the export set (searched: the "IsInPreEvent" assert literal -- absent; and every
// export mentioning ModeManager+3480 or GameStateModule+7608 together with a `*(mode + 40) ==`
// comparison -- the only constants that turn up are 2 (IsInProgress, above) and 3/5/4
// (IsInPostEvent, committed)). So the console's exact state SET is not recovered.
// What IS certain is the enum partition (BrnGameStateSharedIO.h:151-163): the eight states are
// COUNTDOWN(0) INTRO(1) IN_PROGRESS(2) OUTRO(3) RESULTS(4) QUIT(5) ONLINE_LOADING(6)
// ONLINE_SPLASH(7), and the two PINNED predicates claim exactly {2} and {3,4,5}. This body is
// therefore written as the complement of those two inside a live mode -- which resolves to
// {COUNTDOWN, INTRO, ONLINE_LOADING, ONLINE_SPLASH} -- rather than by listing invented
// enumerators. THE ONE THING A VERIFIER MUST SETTLE: whether the console's IsInPreEvent also counts
// the two ONLINE_* states (6/7). If it does not, this answers true for the online loading/splash
// frames as well; no offline path is affected, because an offline mode never enters 6 or 7.
bool ModeManager::IsInPreEvent() const
{
    if (mpCurrentGameMode == nullptr)
    {
        return false;
    }
    return (!IsInProgress() && !IsInPostEvent());
}

// The mode's live state word (X360 mode+0x28 == GameMode::meCurrentState, the same word all three
// predicates above read). [!] PROVISIONAL null arm: the console has no out-of-line copy of this
// accessor either, so which value it answers with no current mode is not recovered. E_GMS_INVALID
// (-1) is used because it is the enum's own "no state" member and because GameMode::meCurrentState
// itself idles at -1 (BrnGameMode.h:164) -- the same answer a freshly-constructed mode gives. It is
// NOT a placeholder zero: E_GMS_COUNTDOWN is 0, and answering that with no mode running would be a
// real lie.
GameStateModuleIO::EGameModeState ModeManager::GetCurrentGameModeState() const
{
    if (mpCurrentGameMode == nullptr)
    {
        return GameStateModuleIO::E_GMS_INVALID;
    }
    return static_cast<GameStateModuleIO::EGameModeState>(mpCurrentGameMode->GetCurrentState());
}

// (mode == E_MODE_ONLINE_FREE_BURN_LOBBY(15) || mode == E_MODE_ONLINE_SHOWTIME(16)) && +0x9508.
// PINNED SEMANTICS (header_grow_spec section 2, byte +38152): the same composite appears
// open-coded in StopModeIntro @0x82343F38's action-30 payload byte and as the PrepareForModeAction
// argument, and StartModeIntro @0x82343018 tests the mode half of it on its own
// (`v10 = *(a1 + 3476); if ( v10 == 15 || v10 == 16 ) ...`).
bool ModeManager::IsOnlineModeWithInstantIntro() const
{
    if ((meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
        && (meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME))
    {
        return false;
    }
    return mbInstantIntroSplash;
}

// DWARF BrnModeManager.h:429/:1207 -- the accessor's whole body is the byte at +38145 (0x9501).
bool ModeManager::IsWaitingForModeDataToLoad() const
{
    return mbModeDataIsLoading;
}

// DWARF BrnModeManager.h:700/:1502 over the member DWARF :1009 names mbHasPlayerFinished
// (X360 +38143, 0x94FF; Construct @0x82340008 zeroes it). Name-for-name accessor.
bool ModeManager::HasPlayerFinished() const
{
    return mbHasPlayerFinished;
}

// ============================================================================================
// 2. THE FINISH-NEXT-UPDATE REQUEST PAIR (DWARF :245 / :250)
// ============================================================================================
// Both are inlined on the console; the byte and the word they write are BOTH pinned from their
// CONSUMER instead. SendModeResults @0x82343438 makes exactly three loads in the whole
// 0x94F0..0x9520 region -- `lbzx r11, r29, 0x94F7`, `lbzx r11, r29, 0x94FD` and
// `lwzx r11, r29, 0x9518` -- and the committed body (BrnModeManager.cpp:373) consumes the first
// and third TOGETHER as "an explicit finish position was requested":
//     if (mbFinishCurrentModeNextUpdate && miDebugFinishPosition > 0) -> use miDebugFinishPosition
// which is only reachable if one setter writes both. Hence the pair below.
// [!!] THE "NOT PROVEN" NOTE THAT USED TO SIT HERE IS REFUTED, AND THE RESET IS NOW LANDED
// (2026-08-26, wave-B fix round; this closes verify batch 4's DROPPED-STORE must-fix at its root
// rather than at one call site). The old note said "no store to 0x9518 exists anywhere outside
// Construct". There is one, and it is decisive -- ModeManager::PlayerFinishedMode @0x823280D8, tail:
//     0x823281BC  ori   r11, r11, 0x9518        ; r11 = 0x9518
//     0x823281C0  li    r9, -1
//     0x823281C4  ori   r8, r8,  0x94F7         ; r8  = 0x94F7
//     0x823281D0  stwx  r9,  r31, r11           ; miDebugFinishPosition      = -1
//     0x823281D4  stbx  r28, r31, r8            ; mbFinishCurrentModeNextUpdate = true   (r28 == 1)
// Two stores, adjacent, one register pair -- that IS the inlined no-position overload, and it makes
// the pair coherent: WithFinishPosition(n) ARMS an override, the plain form CLEARS it. The consumer
// proves why that matters: the committed SendModeResults (BrnModeManager.cpp:378) gates on
// `mbFinishCurrentModeNextUpdate && (miDebugFinishPosition > 0)`, so WITHOUT the reset any earlier
// FinishCurrentModeNextUpdateWithFinishPosition(n) survives every later plain finish and forces a
// stale finish position into the results record.
// (De-inlining it HERE rather than at PlayerFinishedMode's call site fixes it for every caller and
// is idempotent if that call site also spells the -1 out.)
void ModeManager::FinishCurrentModeNextUpdate()
{
    miDebugFinishPosition         = -1;
    mbFinishCurrentModeNextUpdate = true;
}

void ModeManager::FinishCurrentModeNextUpdateWithFinishPosition(s32 liFinishPosition)
{
    miDebugFinishPosition         = liFinishPosition;
    mbFinishCurrentModeNextUpdate = true;
}

// ============================================================================================
// 3. THE INJECTED-MANAGER ACCESSORS (this repo's de-inlining of Construct's four stores)
// ============================================================================================
// Construct @0x82340008 stores its arguments at +27992/+27996/+28000/+28004 and every console
// consumer then reads the raw word; there is no GetXxx symbol in the image for any of them. These
// bodies are exactly those reads. The GameStateModule one keeps the console's own assert literal,
// which several committed bodies (BrnModeManager.cpp:359/412) already fire inline at their own
// call sites.

GameStateModule* ModeManager::GetGameStateModule()
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");
    return mpGameStateModule;
}

// DWARF BrnModeManager.h:387 (the const overload). X360 +0x6D5C.
BrnProgression::ProgressionManager* ModeManager::GetProgressionManager() const
{
    return mpProgressionManager;
}

// DWARF BrnModeManager.h:384. X360 +0x6D64.
const NetworkRoundManager* ModeManager::GetNetworkRoundManager() const
{
    return mpNetworkRoundManager;
}

// ============================================================================================
// 4. THE TRIGGER / TRAFFIC RESOURCE READS (through mpTriggerQueryManager -- accessor grow (5))
// ============================================================================================

// DWARF BrnModeManager.h:381. The track TriggerData resource, resolved through the
// TriggerQueryManager's ResourcePtr at TQM+1568 (0x620).
const BrnTrigger::TriggerData* ModeManager::GetTriggerData() const
{
    CGS_ASSERT(mpTriggerQueryManager != nullptr, "mpTriggerQueryManager");
    return mpTriggerQueryManager->GetTriggerData();
}

// PINNED FROM ASM. This repo's GetCheckpointTriggerData() (the DWARF has no such method -- it is
// the name the committed GetCheckpointPosition body already uses) is the SAME resource:
// GetCheckpointPosition @0x82327388 does
//     `lwz r11, 0x6D60(r31); addi r3, r11, 0x620; bl BrnTrigger::TriggerData_::GetMemor`
// i.e. mpTriggerQueryManager (+0x6D60 == +28000) + 0x620 (== +1568) -- byte-identical to
// GetTriggerData above. The mode's checkpoint landmarks are region indexes INTO the one track
// TriggerData; there is no separate checkpoint resource. This is what retired the old
// mpCheckpointTriggerData stand-in member.
const BrnTrigger::TriggerData* ModeManager::GetCheckpointTriggerData() const
{
    return GetTriggerData();
}

// PINNED FROM ASM. GetStartDataForTrafficLight @0x82327310 opens with
//     `lwz r11, 0x6D60(r3); addi r3, r11, 0x640; bl BrnTraffic::TrafficData_::GetMemor`
// and then fires its own `"lpTrafficData"` assert (baked file BrnModeManager.h, line 1631) when
// the answer is null -- i.e. THIS function, inlined. TQM+0x640 == TQM+1600 == mpTrafficData, the
// ResourcePtr sibling of mpTriggerData; the accessor over it was grown this wave.
// [stuntrace waveB fix round, 2026-08-26] FABRICATED ASSERT REMOVED. This body also carried
// `CGS_ASSERT(mpTriggerQueryManager != nullptr, "mpTriggerQueryManager")`. I dumped 0x82327310 end
// to end: it fires EXACTLY ONE assert, "lpTrafficData" (string @aLptrafficdata, `li r5,0x65F` ==
// BrnModeManager.h:1631), and there is no manager null-check anywhere in the function -- the
// console loads `lwz r11, 0x6D60(r3)` and dereferences it unguarded. An invented assert string is a
// wave-rule violation and it poisons the H10 assert-storm oracle, whose whole value is that every
// line it prints names a real console wire.
// [!] STILL OPEN, for the intro/play partfile's owner: ModeManager::GetStartDataForTrafficLight
// (BrnModeManager_IntroPlay.cpp:143) fires "lpTrafficData" a SECOND time at its call site. The
// console fires it once, inside this inlined accessor -- which is this body. Drop the call-site
// copy, not this one.
const BrnTraffic::TrafficData* ModeManager::GetTrafficData() const
{
    const BrnTraffic::TrafficData* lpTrafficData = mpTriggerQueryManager->GetTrafficData();
    CGS_ASSERT(lpTrafficData != nullptr, "lpTrafficData");
    return lpTrafficData;
}

// ============================================================================================
// 5. THE RACE-CAR OUTPUT-INTERFACE READS (through mpGameStateModule -- accessor grows (1)/(2))
// ============================================================================================

// (1) X360 gsm+245968 (0x3C0D0). See correction (A) at the top of this file: this IS the offset
// header_grow_spec listed twice, and it is the GLOBAL interface. Five ModeManager bodies read it.
const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*
    ModeManager::GetGlobalRaceCarOutputInterface() const
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");
    return mpGameStateModule->GetLastGlobalRaceCarInterface();
}

// (2b) X360 gsm+235488 (0x397E0) -- the module's cached end-of-last-world-update ACTIVE snapshot,
// the one StartGameMode's wreck-count leg and RemoteRaceCarHitsCheckpoint read. Conductor decision
// #7, resolved without a new name and without a grow: the member and the accessor already existed
// (GameStateModule::mLastActiveRaceCarInterface / GetLastActiveRaceCarInterface). NEVER collapsed
// into (1) -- different interface, different offset, different content.
const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
    ModeManager::GetLastActiveRaceCarOutputInterface() const
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");
    return mpGameStateModule->GetLastActiveRaceCarInterface();
}

// PINNED FROM ASM -- and the header's "do not fabricate the walk" FLAG is DISCHARGED, not obeyed by
// omission. HasRaceCarHitValidCheckpoint @0x82329910 does
//     v10 = gsm + 245968;                                    // the GLOBAL interface
//     <assert leGlobalRaceCarIndex >= 0 / < 35, baked at
//      BrnRaceCarEntityModuleOutputInterface.h:1512/1513>     // the INTERFACE's own asserts
//     leActiveRaceCarIndex = *(4 * (leGlobalRaceCarIndex + 525) + v10);
// 4*525 == 2100 == offsetof(RCEntityGlobalRaceCarOutputInterface, maeActiveRaceCarIndices) (pinned
// by that interface's own static_assert, BrnRCEntityGlobalRaceCarOutputInterface.cpp:32), and those
// two baked assert lines are GetActiveRaceCarIndex's OWN. So the console read is an inlined
// RCEntityGlobalRaceCarOutputInterface::GetActiveRaceCarIndex (X360 0x821F46C8) and this is its
// de-inlining. The bounds asserts are NOT duplicated here -- they belong to the callee, and the
// committed caller (BrnModeManager.cpp:138) already carries the ModeManager-side pair.
EActiveRaceCarIndex ModeManager::GlobalToActiveRaceCarIndex(EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    return GetGlobalRaceCarOutputInterface()->GetActiveRaceCarIndex(leGlobalRaceCarIndex);
}

// ============================================================================================
// 6. THE TWO ONLINE COUNTERS TellGuiToShowOnlineFinalStandings HANDS TO THE SCORER
// ============================================================================================
// Both are pinned from TellGuiToShowOnlineFinalStandings @0x82329B68, whose whole payload is
//     ScoringSystem::UpdateCumulativeResults(&mScoringSystem,
//                                            *(gsm + 0x32DAC),                      // arg 1
//                                            *(nrm + 0x12C) - *(nrm + 0x128) - 1,   // arg 2
//                                            mpCurrentGameMode ? mode->IsOnline() : false);
// with `nrm` materialised as `addis r10, r11, 1; addi r10, r10, -0x4608` == gsm + 0xB9F8 ==
// gsm + 47608 -- which GameStateModule::Construct @0x82380388 proves is the NetworkRoundManager
// (it passes `a1 + 47608` as ModeManager::Construct's NetworkRoundManager argument).

// (3) X360 gsm+0x32DAC (208300). [!] NAME NOTE, LOUD: the frozen header calls this
// GetOnlineRoundIndex, but the word it reads is NOT a round index -- it is
// GameStateModule::muNetworkGameRandomSeed (DWARF BrnGameStateModule.h:793, written by
// ProcessGameEvents @0x823A0A18 from the incoming StartNetworkGameEvent's word[3], cleared to -1
// by ClearData @0x8236B3A8). The ROUND index is the OTHER argument, below. The frozen declaration
// is honoured exactly as written and the mismatch is filed as a header_request (rename to
// GetNetworkGameRandomSeed) rather than silently swapping the two arguments at the call site --
// swapping them would change what ScoringSystem::UpdateCumulativeResults stamps into
// CarData::miRoundDisconnectedIn.
// [stuntrace waveB fix round, 2026-08-26] The header_request was APPLIED: the declaration is now
// GetNetworkGameRandomSeed() and the name note above is history, kept because it explains why the
// two UpdateCumulativeResults arguments must never be swapped.
s32 ModeManager::GetNetworkGameRandomSeed() const
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");
    return static_cast<s32>(mpGameStateModule->GetNetworkRandomSeed());
}

// (4) X360 (nrm+0x12C) - (nrm+0x128) - 1 == miTotalRounds - miRoundsRemaining - 1. [!] SAME NAME
// NOTE: the frozen header calls this GetOnlineActiveCarCount and header_grow_spec attributes the
// two words to the GameStateModule; they are the NetworkRoundManager's, and the expression is the
// 0-based CURRENT ROUND -- already de-inlined once in this tree, verbatim, by
// RichPresenceManagerBase::SetRoundParameter (X360 0x8235A6C8; see
// BrnGameStateRichPresenceManagerBase.cpp:171-185, which spells out the identical
// "NRM+300 - NRM+296 - 1" derivation). No GameStateModule grow is needed: ModeManager already
// holds mpNetworkRoundManager.
// [stuntrace waveB fix round, 2026-08-26] The header_request was APPLIED (renamed to
// GetOnlineCurrentRound), and NetworkRoundManager::GetCurrentRound() -- which was DECLARE-ONLY
// when this body was written, contrary to the banner at the top of this file -- now has a body,
// landed together with the ruling that fixes its semantics as the 0-based round index. See
// BrnNetworkRoundManager.cpp. The console arithmetic (the -1 included) lives inside it.
s32 ModeManager::GetOnlineCurrentRound() const
{
    CGS_ASSERT(mpNetworkRoundManager != nullptr, "mpNetworkRoundManager");
    return mpNetworkRoundManager->GetCurrentRound();
}

// ============================================================================================
// 7. THE TWO PARKED X360 SYMBOLS -- both blocked on declarations OUTSIDE agent 9's edit lane
// ============================================================================================
// Both are FULLY RECONSTRUCTED below, as an ARMED-BODY block the conductor pastes in one edit the
// moment the named declarations land. Neither is guessed and neither is half-written into the live
// body: an accessor that silently answers a plausible number is exactly the failure mode the
// campaign's "gate-green != closeable" lesson is about.

// --------------------------------------------------------------------------------------------
// GetRoadRageTakedownTarget -- X360 0x82327518.  [X] PARKED: THREE MISSING DECLARATIONS.
// --------------------------------------------------------------------------------------------
// FULL CONSOLE RECONSTRUCTION (asm read end to end this wave; every assert literal and every baked
// line number below is verbatim from 0x82327518):
//
//   u32 ModeManager::GetRoadRageTakedownTarget()
//   {
//       const BrnProgression::ProgressionData* lpProgressionData =
//           mpProgressionManager->GetProgressionData();               // progmgr+133348 ResourcePtr
//       CGS_ASSERT(lpProgressionData != nullptr, "lpProgressionData != NULL");  // BrnModeManager.cpp:1315
//
//       // The X360 re-fetches the resource pointer here and inlines
//       // ProgressionData::GetProgressionRankData (its "luIndex < muProgressionRankCount" assert is
//       // baked at BrnProgressionData.h:330, its 112-byte stride is `mulli r11,r31,0x70`, and the
//       // count word it reads is +0x14 == muProgressionRankCount).
//       const s32 liLastRank = static_cast<s8>(lpProgressionData->GetProgressionRankCount() - 1);
//       const BrnProgression::ProgressionRankData* lpProgressionRankDataLastRank =
//           lpProgressionData->GetProgressionRankData(static_cast<u32>(liLastRank));
//       CGS_ASSERT(lpProgressionRankDataLastRank != nullptr, "lpProgressionRankDataLastRank"); // :1320
//
//       const s32 liThisRankForGameMode = static_cast<s8>(
//           mpProgressionManager->GetProgressionRankForGameMode(GameStateModuleIO::E_MODE_ROAD_RAGE));
//
//       // At or past the last rank: the flat last-rank target, no interpolation.
//       // (The console promotes both sides to f32 -- extsw/extsb -> std -> lfd -> fcfid -> frsp ->
//       //  fcmpu -- before comparing; value-identical over the s8 domain, so the compare is written
//       //  as integers here.)
//       if (liThisRankForGameMode >= liLastRank)
//       {
//           return lpProgressionRankDataLastRank->GetRoadRageTakedownTarget();   // lhz 0x50(rank)
//       }
//
//       const BrnProgression::ProgressionRankData* lpProgressionRankDataThisRank =
//           lpProgressionData->GetProgressionRankData(static_cast<u32>(liThisRankForGameMode));
//       CGS_ASSERT(lpProgressionRankDataThisRank != nullptr, "lpProgressionRankDataThisRank"); // :1331
//       const s32 liNextRankForGameMode = static_cast<s8>(liThisRankForGameMode + 1);
//       const BrnProgression::ProgressionRankData* lpProgressionRankDataNextRank =
//           lpProgressionData->GetProgressionRankData(static_cast<u32>(liNextRankForGameMode));
//       CGS_ASSERT(lpProgressionRankDataNextRank != nullptr, "lpProgressionRankDataNextRank"); // :1335
//
//       if (CgsDev::Message::gxMessageFilterFlags & 1)
//       { *CgsDev::Log::gpDebugPrint << "liThisRankForGameMode: " << liThisRankForGameMode << "\n"; }
//       if (CgsDev::Message::gxMessageFilterFlags & 1)
//       { *CgsDev::Log::gpDebugPrint << "liNextRankForGameMode: " << liNextRankForGameMode << "\n"; }
//
//       const f32 lfThisRankThreshold = static_cast<f32>(
//           mpProgressionManager->GetRankThresholdForEvent(liThisRankForGameMode,
//                                                          GameStateModuleIO::E_MODE_ROAD_RAGE));
//       const f32 lfNextRankThreshold = static_cast<f32>(
//           mpProgressionManager->GetRankThresholdForEvent(liThisRankForGameMode + 1,
//                                                          GameStateModuleIO::E_MODE_ROAD_RAGE));
//       const f32 lfThisRoadRageTakedownTarget =
//           static_cast<f32>(lpProgressionRankDataThisRank->GetRoadRageTakedownTarget());
//       const f32 lfNextRoadRageTakedownTarget =
//           static_cast<f32>(lpProgressionRankDataNextRank->GetRoadRageTakedownTarget());
//       // progmgr+888 == mProfile(+368) + 520 == maiRankWinsPerOfflineGameMode(+508)[3].
//       // NOT maiWinsPerOfflineGameMode(+468): the thresholds it is compared against are per-RANK,
//       // and 888-368 == 520 == 508 + 4*E_MODE_ROAD_RAGE. It is a single `lwz r10, 0x378(progmgr)`,
//       // i.e. the compiler folded Profile::GetNumRankWinsForGameMode(E_MODE_ROAD_RAGE)'s index.
//       const f32 lfCurrentEventWins = static_cast<f32>(
//           mpProgressionManager->GetProfile()->GetNumRankWinsForGameMode(
//               GameStateModuleIO::E_MODE_ROAD_RAGE));
//
//       const f32 lfCurrentRelativeEventRatio =
//           (lfCurrentEventWins - lfThisRankThreshold) / (lfNextRankThreshold - lfThisRankThreshold);
//       const f32 lfRoadRageFinalTarget = Floor(
//           ((lfNextRoadRageTakedownTarget - lfThisRoadRageTakedownTarget) * lfCurrentRelativeEventRatio
//            + lfThisRoadRageTakedownTarget) + 0.5f);
//
//       ... then the six gated debug prints, with the banner "******************************\n"
//           first and last, and in order "lfThisRoadRageTakedownTarget : ",
//           "lfNextRoadRageTakedownTarget: ", "lfCurrentRelativeEventRatio: ",
//           "lfCurrentEventWins: ", "lfRoadRageFinalTarget: " ...
//
//       return static_cast<u32>(lfRoadRageFinalTarget);   // fctiwz f0,f27; stfiwx; lwz r3
//   }
//
// THE `Floor` ABOVE IS NOT A GUESS. The console tail at 0x8232784C..0x82327878 is the textbook PPC
// floor idiom, and all of its magic operands were dumped from the image THIS SESSION
// (scratch/postfx_step9_final/envfix/work/image.bin, big-endian; offset == VA - 0x82000000):
//     flt_82001DA0 = 3F000000          ->  0.5f   (the round-half-up bias, added first)
//     dbl_82001CB8 = C330000000000000  -> -2^52   (fsel'd magic, negative branch)
//     dbl_82001CB0 = 4330000000000000  -> +2^52   (fsel'd magic, positive branch)
//     dbl_82001CA8 = 0000000000000000  ->  0.0
//     dbl_82001CA0 = 3FF0000000000000  ->  1.0    (the overshoot correction)
// i.e. `x -> (x +- 2^52) -+ 2^52`, then subtract 1.0 when the rounded result overshot x, which is
// floor(x). The final fctiwz then truncates the already-integral float into r3.
//
// [!!] STATUS 2026-08-26 (wave-B fix round). ALL THREE DECLARATIONS BELOW HAVE NOW LANDED, each
// re-derived from the asm rather than pasted from the request:
//   (a) BrnProgression::ProgressionRankData::GetRoadRageTakedownTarget()  -> BrnGameModeParams.h
//   (b) BrnProgression::ProgressionManager::GetProgressionRankForGameMode -> BrnProgressionManager.h
//   (c) BrnProgression::ProgressionManager::GetRankThresholdForEvent      -> BrnProgressionManager.h
// THE BODY IS STILL PARKED, and the reason has MOVED: (b) X360 0x8237B4E8 and (c) X360 0x82370260
// are declare-only -- no BODY exists anywhere in src/ -- so un-parking today buys two LNK2019s
// instead of a working takedown target. The remaining blocker is therefore
// "reconstruct ProgressionManager::GetProgressionRankForGameMode + ::GetRankThresholdForEvent",
// which is a ProgressionManager-TU item, and it is on the conductor's BLOCKING list for any
// road-rage boot test (verify batch 5, MF5). Un-park this in the SAME change as those two bodies.
//
// The original filing, kept for the evidence it carries:
//   (a) BrnProgression::ProgressionRankData::GetRoadRageTakedownTarget() -- the u16 at
//       ProgressionRankData+0x50 (`lhz r3, 0x50(r30)` / `lhz r6, 0x50(r26)` / `lhz r9, 0x50(r24)`).
//       DWARF BrnProgressionRankData.h:129 declares it and :47 names the member
//       muRoadRageTakedownTarget. The tree's ProgressionRankData is the declare-only stub at
//       BrnGameModeParams.h:49-57 -- the accessor must be added THERE.
//   (b) BrnProgression::ProgressionManager::GetProgressionRankForGameMode(EGameModeType)
//       (X360 0x8237B4E8) -- ABSENT from the tree entirely. Already a known wave frontier (agent
//       3's SetupGameMode needs the same symbol).
//   (c) BrnProgression::ProgressionManager::GetRankThresholdForEvent(s32, EGameModeType)
//       -- ABSENT from the tree entirely.
// Everything else the body needs already exists: ProgressionManager::GetProgressionData()
// (BrnProgressionManager.h:239), ProgressionManager::GetProfile() (:129),
// Profile::GetNumRankWinsForGameMode() (BrnProfile.h:384 -- NOT the +468 GetNumWinsForGameMode
// inline at :403; see the maiRankWinsPerOfflineGameMode note above),
// ProgressionData::GetProgressionRankData()/GetProgressionRankCount() (BrnProgressionData.h:73/:79).
//
// UNTIL (a)(b)(c) LAND this answers 0 behind a one-shot log. That is a REAL BEHAVIOURAL DIVERGENCE
// and it is stated here rather than hidden: a road-rage event started on this build has a takedown
// target of zero, i.e. its "enough takedowns" condition is satisfied from frame one. Road rage is
// off the stunt-race campaign path, which is why parking it is acceptable; it is NOT acceptable to
// leave it looking finished.
u32 ModeManager::GetRoadRageTakedownTarget()
{
    // The one leg that IS reachable today: the console's leading manager check. Keeping it means
    // the build's first symptom on this path is still the console's own diagnostic.
    CGS_ASSERT(mpProgressionManager != nullptr, "mpProgressionManager");

    static bool sbLoggedRoadRageTakedownTargetParked = false;
    if (!sbLoggedRoadRageTakedownTargetParked && (CgsDev::Message::gxMessageFilterFlags & 1))
    {
        sbLoggedRoadRageTakedownTargetParked = true;
        *CgsDev::Log::gpDebugPrint
            << "[stuntrace] ModeManager::GetRoadRageTakedownTarget PARKED -> 0."
               " Blocked on ProgressionRankData::GetRoadRageTakedownTarget,"
               " ProgressionManager::GetProgressionRankForGameMode (X360 0x8237B4E8) and"
               " ProgressionManager::GetRankThresholdForEvent. The armed body is in"
               " BrnModeManager_Accessors.cpp.\n";
    }
    return 0;
}

// --------------------------------------------------------------------------------------------
// GetNumberOfCarsInFlyby -- X360 0x82311E38.  [X] PARKED: FlybyManager HAS NO OWNING HEADER.
// --------------------------------------------------------------------------------------------
// The console body is four lines and fully recovered:
//
//   s32 ModeManager::GetNumberOfCarsInFlyby()
//   {
//       FlybyManager* lpFlybyManager = mpGameStateModule->GetFlybyManager();
//       return lpFlybyManager->CalculateNumberOfCarsInFlyby();   // vtable slot 1, (*(*v2 + 4))(v2)
//   }
//
// where GetFlybyManager() (DWARF BrnGameStateModule.h:561) is itself inlined at both console call
// sites as
//       `GameStateModule::IsOnlineGameMode(gsm) ? gsm + 186592 : gsm + 185904`
// -- gsm+185904 (0x2D630) mOfflineFlybyManager, gsm+186592 (0x2D8E0) mOnlineFlybyManager, adjacent
// and 688 bytes apart, in the DWARF's own declaration order (BrnGameStateModule.h:248/:251). The
// SECOND console call site is ModeManager::StartModeIntro @0x82343018, which uses the identical
// selector and then memcpy's 592 bytes out of vtable slot 0 (GetFlybyData) -- so agent 5 hits this
// exact wall, and the fix below unblocks both.
//
// [X] THE BLOCKER IS STRUCTURAL, NOT A MISSING ACCESSOR. BrnGameState::FlybyManager has no owning
// header anywhere in the tree: the base class is DEFINED LOCALLY inside
// GameSource/GameState/FlybyManager/BrnGameStateFlybyManager.cpp:45 (together with local
// re-definitions of OnlineFlybyManager and OfflineFlybyManager), and
// BrnGameStateOnlineFlybyManager.h:139 carries a SECOND, byte-exact re-declaration of the same base
// for the derived TU. Neither copy declares any virtual at all (the vtable is modelled as a plain
// `u32 mVTable` word), and CalculateNumberOfCarsInFlyby is declared only on the ONLINE leaf. So
// GameStateModule cannot embed the two managers by value, cannot hand back a usable FlybyManager*,
// and slot 1 cannot be dispatched. Filed as a header_request (the four-part recipe is in agent 9's
// report). Parked here behind a one-shot log; it returns 0 == "no cars in the flyby", which is the
// state the flyby data is in on this build anyway (nothing populates it).
s32 ModeManager::GetNumberOfCarsInFlyby()
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");

    static bool sbLoggedFlybyCarCountParked = false;
    if (!sbLoggedFlybyCarCountParked && (CgsDev::Message::gxMessageFilterFlags & 1))
    {
        sbLoggedFlybyCarCountParked = true;
        *CgsDev::Log::gpDebugPrint
            << "[stuntrace] ModeManager::GetNumberOfCarsInFlyby PARKED -> 0."
               " BrnGameState::FlybyManager has no owning header (base defined inside"
               " BrnGameStateFlybyManager.cpp, re-declared in BrnGameStateOnlineFlybyManager.h),"
               " so GameStateModule cannot embed mOfflineFlybyManager/mOnlineFlybyManager nor"
               " dispatch CalculateNumberOfCarsInFlyby. Same blocker as StartModeIntro's flyby"
               " leg.\n";
    }
    return 0;
}

// ============================================================================================
// 8. DELIBERATELY NOT BODIED IN THIS FILE -- READ BEFORE ADDING ONE
// ============================================================================================
// ModeManager::GetActiveRaceCarOutputInterface() const  (former BrnModeManager.h accessor grow 2a)
//   [stuntrace waveB fix round, 2026-08-26] THE DECLARATION IS NOW RETIRED FROM THE HEADER, which
//   is what this note asked for -- there is no longer anything to leave un-bodied. The reasoning
//   below is kept verbatim so nobody re-adds it.
//   Was left DECLARED-ONLY on purpose. See correction (A) at the top: the offset its comment carries
//   ("mpGameStateModule+0x245968") is the decimal 245968 written as though it were hex, and 245968
//   IS the GLOBAL interface, already served by GetGlobalRaceCarOutputInterface() above. A sweep of
//   every ModeManager export shows NO read of a live-active interface off the GameStateModule --
//   the live active interface is a world-module INPUT buffer that this class only ever receives as
//   a function argument (PreWorldUpdate / UpdateCurrentMode both take one). Giving this accessor a
//   body would mean choosing a source the console never uses, and pointing it at
//   mLastActiveRaceCarInterface would collapse the two embeds conductor decision #7 says must never
//   be collapsed. Filed as a header_request: RETIRE the declaration (callers that want the cached
//   snapshot use GetLastActiveRaceCarOutputInterface(); callers inside the per-frame spine use the
//   interface they were handed).
//
// ModeManager::GetScoringSystem() x2        -> ModeManager_gUI_00.cpp (hazards H1: never twice).
// The sixteen committed bodies                 -> BrnModeManager.cpp (hazards H2).

// ==============================================================================================
// ModeManager::OnDriveThruRepairAvailable
//
// ⭐ THE HEADER FLAG SAID "name/dispatch not in exports". THE DISPATCH IS IN THE EXPORTS -- it
// is just not a symbol, because the console never had a ModeManager method here at all. This
// name was minted by the reconstruction of DriveThruManager::HandleDriveThru, which spelled the
// console's inline forward as a call on the manager. The X360 run it stands for is
// @0x8239B334..0x8239B354, five instructions, verbatim:
//     lwz    r11, 0x92C(r29)     ; DriveThruManager::mpModeManager
//     lwz    r10, 0xD98(r11)     ; ModeManager + 3480 == mpCurrentGameMode
//     cmplwi cr6, r10, 0
//     beq    cr6, skip           ; the NULL guard is the console's, not invented
//     mr     r3, r10             ; `this` = the CURRENT MODE, not the manager
//     lwz    r11, 0(r3)          ; its vtable
//     lwz    r11, 0x64(r11)      ; slot 0x64/4 == 25
//     mtctr  r11 / bctrl         ; no arguments
// GameMode vtable slot 25 (BrnGameMode.h:339, vtbl+100) is OnPlayerUsesPaintShop -- already
// declared in console slot order and already bodied (BrnGameMode.cpp:746, base empty), so this
// forward costs the link nothing and reaches the real overrides.
//
// ⚠️ THE NAME IS A MISNOMER, KEPT ONLY BECAUSE THE CALLER IS COMMITTED. The console hook is
// "the player used a paint shop", and the call site is HandleDriveThru's paint-shop arm (past
// the CanAutoRepair and the GetPlayerBaseDeformAmount > 0 early-outs). RETIRE-WHEN that caller
// is rewritten onto `mpModeManager->GetCurrentGameMode()->OnPlayerUsesPaintShop()`; this
// forward should not outlive it.
// ==============================================================================================
void ModeManager::OnDriveThruRepairAvailable()
{
    if (mpCurrentGameMode != 0)
    {
        mpCurrentGameMode->OnPlayerUsesPaintShop();
    }
}

} // namespace BrnGameState
