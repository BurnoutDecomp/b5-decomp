#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
// BrnNetwork::NetworkRounder now has a committed declare-only home; StopModeTimer's force
// (online) branch snaps the elapsed finish time + the round-end distance to the network grid
// through it (X360 0x8231F804 / 0x8231F81C). The bodies live in BrnNetworkRounder.cpp; the gate
// here is `cl /c` (no link).
#include "GameSource/Network/Utilities/BrnNetworkRounder.h"

// ============================================================================
// BrnScoringSystem_Timer.cpp
// ============================================================================
// Bodies for the "Timer" group of BrnGameState::ScoringSystem (keystone header
// BrnScoringSystem.h lines 304-336): the mode timer / time-limit setters and queries,
// the medal-mode timer, the per-km time limit, the time-remaining / time-expired
// computations, and the road-rage player-crash counters.
//
// Each landed body is reconstructed from its X360 pseudocode (addresses in the
// per-method comments) and accesses ScoringSystem members BY NAME. The timer is encoded
// in two CgsSystem::Time members: mStartTime holds the mode start (its second count is
// set to -1 when the timer is cleared, so a negative second count == "inactive"), and
// mEndTime holds the time-limit deadline (likewise -1 == inactive). mTimeRemaining is the
// scratch slot GetModeTimeRemaining writes. CgsSystem::Time is a fully inline value type
// (CgsTime.h), so every arithmetic body below is self-contained.
//
// The X360 binary checks "IsTimerActive()" via (mStartTime.miSeconds < 0) and
// "IsTimeLimitActive()" via (mStartTime.miSeconds < 0 || mEndTime.miSeconds < 0); those
// two predicates are themselves declare-only in this range, so they are reconstructed
// here from the assert conditions the addressed setters fire.
//
// LANDED this round (prereqs unblocked them):
//   - StopModeTimer (0x8231F590): now lands. The BaseOnlineModeScoring virtual the force/online
//       branch dispatches (X360 vtable slot 6 == UpdatePlayerPoints) is in the committed base
//       slice, and BrnNetwork::NetworkRounder::Round{Time,Distance}ToNetworkAccuracy now have a
//       committed declare-only home (BrnNetworkRounder.h). The CarScoreData reads/writes
//       (mfDistanceToFinishLive@+0x18, mfDistanceToFinish@+0x48, mTotalTime@+0x00,
//       miCompletedLaps@+0x10) all have NAMED accessors after the CarScoreData grow.
//   - StartOnlineGameModeScoring (0x823126C8): now lands. The keystone now models the FOURTH
//       online scorer mOnlineStuntRunModeScoring (ss+0x4D44) by name, so the Fugitive/FreeBurn/
//       ModeEnd (game-mode 12/14/17) case can target it; mpCurrentOnlineModeScoring (ss+0x4DC8)
//       is the BaseOnlineModeScoring* the switch assigns.
//   - HasStuntAttackModeEnded (0x82326708): now lands. The keystone decl was RETYPED to the real
//       body shape `(const CgsSystem::Time&, EActiveRaceCarIndex, bool)`, the SECOND stunt scorer
//       mOnlineStuntModeScoring (ss+0x2620) is modelled by name, StuntModeScoring::HasStuntModeEnded
//       (vtable slot +0x14) is now a named virtual, and CarScoreData::SetEliminated (the +0xD9 write)
//       exists -- so the online/offline scorer pick + the eliminated-flag write all resolve by name.
//
// BLOCKED (omitted -- left declare-only) and why:
//   - OnRoadRagePlayerCrashed (0x823444B0): dereferences the GameStateModuleIO::OutputBuffer
//       param (forward-declared only in the keystone) and CgsModule::VariableEventQueue<13312,16>
//       (no committed home), to push road-rage crash events.
//
// DEFERRED (no standalone X360 export -- inlined away on X360, so no authoritative body):
//   SetCheckPointsForCarsWithinRace, UpdateTimerForEliminator, HasCrashModeEnded.
//   None has an exported body and none is confidently inferable from named fields + callers:
//     * SetCheckPointsForCarsWithinRace / UpdateTimerForEliminator: no export, no
//       inlined fragment recovered.
//   [x] StartModeTimer LEFT THIS LIST 2026-08-26 (waveB MOUNT-CLOSURE round): its inlined
//       fragment IS recovered now (ModeManager::UpdateCurrentMode @0x823511C8) and its body is
//       landed below, next to ClearModeTimer. ReducePlayerDurability (never on this list, but
//       equally export-less) landed the same way from ModeManager::StartGameMode @0x8234FF80.
//     * HasCrashModeEnded: no ScoringSystem export. The only HasCrashModeEnded export is the
//       SUB-SCORER BrnGameState::CrashModeScoring::HasCrashModeEnded (0x823129A0), whose signature
//       takes a `double` time-delta; ScoringSystem::HasCrashModeEnded is declared `() const` (no
//       params), so the exact forward/argument cannot be proven. Left declare-only rather than
//       guessed.
// ============================================================================

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// timer / time-limit active predicates (reconstructed from the addressed setters'
// assert conditions: *this+0 == mStartTime.miSeconds, *this+8 == mEndTime.miSeconds).
// ----------------------------------------------------------------------------

// A mode timer is running while its start time has a non-negative second count
// (ClearModeTimer / StopModeTimer drive mStartTime's seconds to -1).
bool ScoringSystem::IsTimerActive() const
{
    return mStartTime.GetSeconds() >= 0;
}

// A time limit is active while the timer is running AND a deadline (mEndTime) is set.
bool ScoringSystem::IsTimeLimitActive() const
{
    return (mStartTime.GetSeconds() >= 0) && (mEndTime.GetSeconds() >= 0);
}

// The configured time-limit deadline.
const CgsSystem::Time ScoringSystem::GetTimeLimit() const
{
    return mEndTime;
}

// ----------------------------------------------------------------------------
// time-limit setters (X360-faithful).
// ----------------------------------------------------------------------------

// X360 0x82310838. Set a plain time limit: deadline = start + lfSeconds. Clears medal mode
// and elimination mode and resets the medal targets to NONE (a plain time limit is neither a
// medal run nor an eliminator round).
void ScoringSystem::SetTimeLimitSeconds(f32 lfSeconds)
{
    CGS_ASSERT(IsTimerActive(), "IsTimerActive()");

    mbMedalMode            = false;
    mbEliminationMode      = false;
    meCurrentMedalTarget   = E_CURRENT_MEDAL_TARGET_TIME_NONE;
    meCurrentMedalAchieved = E_CURRENT_MEDAL_TARGET_TIME_NONE;

    mEndTime = mStartTime + CgsSystem::Time(lfSeconds);
}

// X360 0x823108E0. Configure a medal-mode timer: store the gold/silver/bronze targets (plus a
// bronze+10s outer bound), reset the medal target/achieved trackers to the start (gold) bucket,
// clear elimination mode, and set the deadline to start + gold.
void ScoringSystem::SetMedalModeTimer(f32 lfGold, f32 lfSilver, f32 lfBronze)
{
    CGS_ASSERT(IsTimerActive(), "IsTimerActive()");
    CGS_ASSERT(lfGold > 0.0f, "lfGoldTimeLimitSeconds > 0.0f");

    mafMedalTimes[0] = lfGold;
    mafMedalTimes[1] = lfSilver;
    mafMedalTimes[2] = lfBronze;
    mafMedalTimes[3] = lfBronze + 10.0f;

    // NOTE: the X360 body (0x823108E0) does NOT write mbMedalMode -- its only byte store is
    // mbEliminationMode=false (stb r11=0, 0x5D20). mbMedalMode is set on a different path; do
    // not fabricate it here (caught by the body verifier).
    mbEliminationMode      = false;
    meCurrentMedalTarget   = E_CURRENT_MEDAL_TARGET_TIME_START;
    meCurrentMedalAchieved = E_CURRENT_MEDAL_TARGET_TIME_START;

    mEndTime = mStartTime + CgsSystem::Time(lfGold);
}

// X360 0x82312740. Set the time limit from a seconds-per-km allowance scaled by the total race
// distance, rounded UP to the next whole 30-second block: deadline = start + roundedSeconds.
void ScoringSystem::SetTimeLimitPerKm(f32 lfSecondsPerKm)
{
    CGS_ASSERT(IsTimerActive(), "IsTimerActive()");
    CGS_ASSERT(0.0f != mfTotalRaceDistance, "0.0f != mfTotalRaceDistance");

    // distance(m) -> km (* 0.001) -> seconds (* allowance), truncated to whole seconds.
    const s32 liRawSeconds = static_cast<s32>((mfTotalRaceDistance * 0.001f) * lfSecondsPerKm);

    // Round up to the nearest 30-second multiple.
    const s32 liRoundedSeconds = 30 * ((liRawSeconds + 29) / 30);

    CgsSystem::Time lLimit;
    lLimit.SetSeconds(liRoundedSeconds);
    lLimit.SetFraction(0.0f);

    mEndTime = mStartTime + lLimit;
}

// X360 0x823109E8. Extend an already-active time limit by lfSeconds.
void ScoringSystem::IncreaseTimeLimit(f32 lfSeconds)
{
    CGS_ASSERT(IsTimeLimitActive(), "IsTimeLimitActive()");

    mEndTime += CgsSystem::Time(lfSeconds);
}

// ----------------------------------------------------------------------------
// StartModeTimer -- X360: NO standalone export; RECOVERED FROM ITS ONE INLINED CALL SITE.
// (This retires the "DEFERRED -- no export, no inlined fragment recovered" entry in this
// file's header banner: the fragment IS recovered now, and the wave-B mount made the symbol
// a hard LNK2019 -- BrnModeManager_UpdateMode.cpp:400 is the timer-start latch that begins
// every mode's clock.)
//
// EVIDENCE (dumped this session from ModeManager::UpdateCurrentMode @0x82350EC8, the
// timer-start-latch arm at 0x823511C8..0x823511DC):
//     0x823511AC  lwz   r24, 0x10(r27)     ; lSimTimerStatus time: seconds
//     0x823511B0  lfs   f29, 0x14(r27)     ;                       fraction
//     ...
//     0x823511C8  stfs  f29, 4(r25)        ; r25 == &mScoringSystem
//     0x823511CC  stw   r24, 0(r25)
//     0x823511D0  lwz   r11, 0xD98(r31)    ; -> SetTimerStartRequest(false)
//     0x823511E0  bl    ModeManager::StartPlayingMode
// The two stores land on the ScoringSystem's FIRST member (BrnScoringSystem.h:1199,
// mStartTime) at its two fields -- CgsTime.h pins miSeconds@+0 / mfFraction@+4 -- and there
// is no third store, no mEndTime touch and no assert between the load and the next call.
// So the whole inlined function is one assignment. (Contrast ClearModeTimer, which the same
// arm inlines eight instructions later as the single `li r11,-1; stw r11, 0(r25)` at
// 0x823511F0 -- already committed at :163. Two different one-store bodies, same member.)
// ----------------------------------------------------------------------------
void ScoringSystem::StartModeTimer(const CgsSystem::Time& lTime)
{
    mStartTime = lTime;
}

// Drive the timer / time-limit back to the inactive (negative second count) state.
void ScoringSystem::ClearModeTimer()
{
    mStartTime.SetSeconds(-1);
}

void ScoringSystem::ClearTimeLimit()
{
    mEndTime.SetSeconds(-1);
}

// ----------------------------------------------------------------------------
// elapsed / remaining / expired computations.
// ----------------------------------------------------------------------------

// Time elapsed since the mode timer started.
const CgsSystem::Time ScoringSystem::GetElapsedTime(const CgsSystem::Time& lTime) const
{
    return lTime - mStartTime;
}

// X360 0x82310A80. Remaining time until the deadline (mEndTime - lTime); also caches the result
// in mTimeRemaining and returns it by value.
const CgsSystem::Time ScoringSystem::GetModeTimeRemaining(const CgsSystem::Time& lTime)
{
    CGS_ASSERT(IsTimeLimitActive(), "IsTimeLimitActive()");

    mTimeRemaining = mEndTime - lTime;
    return mTimeRemaining;
}

// X360 0x82310B20. True once the mode deadline has passed. The X360 body offsets lTime back by
// 0.99s before measuring the remaining time, then reports expired when the remaining time has
// fallen to zero or below. Returns false outright when no time limit is active.
bool ScoringSystem::HasModeTimeExpired(const CgsSystem::Time& lTime)
{
    if (!IsTimeLimitActive())
    {
        return false;
    }

    CgsSystem::Time lAdjustedTime;
    lAdjustedTime.SetFloatVal(lTime.GetFloatVal() - 0.99f);

    const CgsSystem::Time lZero(0.0f);
    const CgsSystem::Time lRemaining = GetModeTimeRemaining(lAdjustedTime);

    return lRemaining <= lZero;
}

// X360 0x82326708. Decide whether the stunt-attack mode has ended, for one race car. The lbOnline
// flag selects which embedded stunt scorer answers: the online scorer (mOnlineStuntModeScoring,
// ss+0x2620) or the offline scorer (mStuntModeScoring, ss+0x350). Either way the scorer's
// HasStuntModeEnded(bool) virtual is dispatched with "has the mode timer expired" (HasModeTimeExpired
// (lTime)) as its time-up argument. On the ONLINE path only, when the scorer reports the mode ended,
// the looked-up car (GetCarData(leRaceCarIndex)) is flagged eliminated (CarScoreData::SetEliminated,
// the +0xD9 byte the X360 stores 1 into). The offline path returns the verdict without touching the
// eliminated flag.
bool ScoringSystem::HasStuntAttackModeEnded(const CgsSystem::Time& lTime, EActiveRaceCarIndex leRaceCarIndex, bool lbOnline)
{
    if (lbOnline)
    {
        // Online stunt scorer (ss+0x2620). HasModeTimeExpired(lTime) is the bool time-up arg the
        // X360 forwards into the HasStuntModeEnded virtual (vtable slot +0x14).
        const bool lbEnded = mOnlineStuntModeScoring.HasStuntModeEnded(HasModeTimeExpired(lTime));
        if (lbEnded)
        {
            // X360 sub_8231DCD0 == GetCarData; stb 1, 0xD9(r3) == SetEliminated(true) on its score record.
            CarData* lpCarData = GetCarData(leRaceCarIndex);
            lpCarData->GetScoreData()->SetEliminated(true);
        }
        return lbEnded;
    }

    // Offline stunt scorer (ss+0x350): just report the verdict (no eliminated-flag write on this path).
    return mStuntModeScoring.HasStuntModeEnded(HasModeTimeExpired(lTime));
}

// ----------------------------------------------------------------------------
// road-rage player-crash counters (semantics fixed by OnRoadRagePlayerCrashed @ 0x823444B0:
// miMaximumPlayerCrashedNumber is the crash allowance, miCurrentPlayerCrashedNumber the running
// tally, mbPlayerTotalled the "out of crashes" flag).
// ----------------------------------------------------------------------------

void ScoringSystem::ResetRoadRageCrashesForPlayer()
{
    miCurrentPlayerCrashedNumber = 0;
    mbPlayerTotalled             = false;
}

s32 ScoringSystem::GetRoadRagePlayerCrashes()
{
    return miCurrentPlayerCrashedNumber;
}

s32 ScoringSystem::GetPlayerCrashesRemaining()
{
    return miMaximumPlayerCrashedNumber - miCurrentPlayerCrashedNumber;
}

// ----------------------------------------------------------------------------
// ReducePlayerDurability -- X360: NO standalone export; RECOVERED FROM ITS ONE INLINED CALL
// SITE (DWARF BrnScoringSystem.h:1192 names it; the console emitted no call).
//
// EVIDENCE (dumped this session from ModeManager::StartGameMode @0x8234FCE8, the
// unlock-deformation arm at 0x8234FF80..0x8234FFAC):
//     0x8234FF80  lwz    r11, 0x4B58(r26)   ; r26 == &mScoringSystem
//     0x8234FF84  addic. r11, r11, -1
//     0x8234FF88  stw    r11, 0x4B58(r26)
//     0x8234FF8C  bgt    loc_8234FFB0
//     0x8234FF90  bl     CgsDev::Assert::BeginAssert
//     0x8234FF98  li     r5, 0xDE9                       ; BrnScoringSystem.h line 3561
//     0x8234FFA4  addi   r3, r11, aMimaximumplaye@l      ; "miMaximumPlayerCrashedNumber > 0"
// ss+0x4B58 is miMaximumPlayerCrashedNumber (pinned at BrnScoringSystem.h:1213). The
// decrement is the STORED one and the assert tests the POST-decrement value (`addic.` sets
// cr0 from the result, and the `bgt` that skips the assert reads that same cr0) -- so the
// assert fires when the allowance has been spent down to 0, not before. Reproduced exactly:
// store first, then assert on the new value.
//
// Semantics: "durability" is the road-rage crash ALLOWANCE. A car whose progression record
// carries an unlock-deformation amount starts its event one crash short.
// ----------------------------------------------------------------------------
void ScoringSystem::ReducePlayerDurability()
{
    --miMaximumPlayerCrashedNumber;

    CGS_ASSERT(miMaximumPlayerCrashedNumber > 0, "miMaximumPlayerCrashedNumber > 0");
}

// ----------------------------------------------------------------------------
// mode-timer stop + online-scorer selection.
// ----------------------------------------------------------------------------

// X360 0x8231F590. Stop the mode timer for one race car: snapshot the elapsed mode time into the
// system's mTotalTime, capture the car's live distance-to-finish into its round-end distance slot,
// then either (online/force path) round both to the network grid and let the current online scorer
// recompute player points, or (offline path) bank the elapsed time as the car's finish time when it
// has not yet completed every lap. Finally drives mStartTime's seconds to -1 (timer inactive).
//
// The "force" flag (lbForce) selects the online/network path. The CgsSystem::Time the X360 builds
// as Time(0.0) is the comparison zero used to detect a car with no finish time recorded yet.
void ScoringSystem::StopModeTimer(const CgsSystem::Time& lTime, EActiveRaceCarIndex leRaceCarIndex, bool lbForce)
{
    CGS_ASSERT((leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
               "(leLocalPlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

    CarData* lpCarData = GetCarData(leRaceCarIndex);
    CGS_ASSERT(lpCarData != NULL, "lpCarData");

    // Elapsed mode time = now - mode-start. Cached in the system's mTotalTime slot (X360 a1+0x10).
    mTotalTime = lTime - mStartTime;

    GameStateModuleIO::CarScoreData* lpScoreData = lpCarData->GetScoreData();

    // Capture the live distance-to-finish into the round-end distance slot.
    const f32 lfRoundEndDistance = lpScoreData->GetDistanceToFinishLive();   // CarScoreData +0x18
    lpScoreData->SetDistanceToFinish(lfRoundEndDistance);                    // CarScoreData +0x48

    // X360 sanity asserts on the captured round-end distance (diagnostic only; both fire
    // CgsDev::Assert and continue). #1: distance must be above the -1.0 floor.
    CGS_ASSERT(lfRoundEndDistance > -1.0f,
               "Bad distance in scoring system #1, dist from finish at round end");
    // #2: a finite distance must not exceed 200000.0 unless it is the round-end-not-reached
    // sentinel the per-car record is cleared to. That sentinel is flt_82CDB7D0 == FLT_MAX
    // (3.4028235e38), recovered from the PS3 DecFIGS StopModeTimer (0x1FE7C8, same source):
    // the assert fires on `dist > 200000.0 && dist != 3.4028235e38`. The X360 0x8231F590 carries
    // the identical flt_82CDB7D0 compare. Asserted here as the negated (good) condition.
    CGS_ASSERT(lpScoreData->GetDistanceToFinish() <= 200000.0f ||
                   lpScoreData->GetDistanceToFinish() == 3.4028235e38f,
               "Bad distance in scoring system #2, dist from finish at round end");

    if (lbForce)
    {
        // Network/online path. If the car has no finish time banked yet (GetFloatVal() == 0.0),
        // round the elapsed mode time to the network grid and bank it as the car's finish time.
        const CgsSystem::Time lFinishTime = lpScoreData->GetFinishTime();
        if (lFinishTime.GetFloatVal() == 0.0f)
        {
            BrnNetwork::NetworkRounder::RoundTimeToNetworkAccuracy(&mTotalTime);   // X360 a1+0x10
            lpScoreData->SetFinishTime(mTotalTime);
        }

        // Snap the captured round-end distance to the network grid (in place via a local copy --
        // the field has no public address-of accessor; RoundDistanceToNetworkAccuracy rounds *lpf).
        f32 lfRoundedDistance = lpScoreData->GetDistanceToFinish();
        BrnNetwork::NetworkRounder::RoundDistanceToNetworkAccuracy(&lfRoundedDistance);
        lpScoreData->SetDistanceToFinish(lfRoundedDistance);

        CGS_ASSERT(mpCurrentOnlineModeScoring != NULL, "mpCurrentOnlineModeScoring");
        // X360 dispatches BaseOnlineModeScoring vtable slot 6 (== UpdatePlayerPoints) on the current
        // online scorer, passing this scoring system + the active-car count (ss+0x4EE8).
        mpCurrentOnlineModeScoring->UpdatePlayerPoints(this, static_cast<s32>(muCarsInCurrentMode));
    }
    else
    {
        // Offline path. The X360 loops over all E_ACTIVE_RACE_CAR_INDEX_COUNT indices (with the
        // standard EActiveRaceCarIndex bounds assert), but the loop body touches only the single
        // looked-up car -- the conditional finish-time store is idempotent, so it is preserved
        // verbatim. When the car has not completed every lap, bank the elapsed mode time as its
        // finish time.
        for (s32 liEnumIndex = 0; liEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liEnumIndex)
        {
            if (static_cast<u32>(lpScoreData->GetCompletedLaps()) < muTotalLaps)   // +0x10 < ss+0x4ED8
            {
                lpScoreData->SetFinishTime(mTotalTime);
            }
            CGS_ASSERT(liEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT, "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }
    }

    // Drive the timer back to the inactive (negative second count) state (X360 *a1 = -1).
    ClearModeTimer();
}

// X360 0x823126C8. Point mpCurrentOnlineModeScoring (ss+0x4DC8) at the embedded online scorer for
// the given online game mode. The X360 switches on (leGameMode - E_MODE_ONLINE_MODE_START): online
// road-rage -> the road-rage scorer; the stunt-run trio (Fugitive/FreeBurn/ModeEnd) -> the
// stunt-run scorer; burning-home-run -> the burning-home-run scorer; everything else -> the race
// scorer. Each scorer derives from BaseOnlineModeScoring, so the address-of is a valid upcast.
void ScoringSystem::StartOnlineGameModeScoring(GameStateModuleIO::EGameModeType leGameMode)
{
    switch (leGameMode)
    {
        case GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:        // 11 -> ss+0x4BF8
            mpCurrentOnlineModeScoring = &mOnlineRoadRageScoring;
            break;

        case GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:         // 12 -> ss+0x4D44
        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:        // 14 -> ss+0x4D44
        case GameStateModuleIO::E_MODE_ONLINE_MODE_END:         // 17 -> ss+0x4D44
            mpCurrentOnlineModeScoring = &mOnlineStuntRunModeScoring;
            break;

        case GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN: // 13 -> ss+0x4CC0
            mpCurrentOnlineModeScoring = &mOnlineBurningHomeRunScoring;
            break;

        default:                                                // 10/15/16/... -> ss+0x4B74
            mpCurrentOnlineModeScoring = &mOnlineRaceModeScoring;
            break;
    }
}

// ----------------------------------------------------------------------------
// medal query.
// ----------------------------------------------------------------------------

// Gold is achieved while the achieved-medal bucket is the gold (== start) bucket.
bool ScoringSystem::AcheivedGold()
{
    return meCurrentMedalAchieved == E_CURRENT_MEDAL_TARGET_TIME_GOLD;
}

// ----------------------------------------------------------------------------
// race-position snapshot.
// ----------------------------------------------------------------------------

// X360 0x82326690. For every active-race-car slot, copy the car's current race position
// (CarScoreData +0x08) into its highest-race-position slot (CarScoreData +0x0C). The X360
// body walks all E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8) indices via GetCarData (sub_8231DCD0)
// and skips any slot with no matching CarData (NULL). The per-index <= COUNT assert the
// pseudocode shows lives inside GetCarData's own body, not here.
void ScoringSystem::ClearHighestPositions()
{
    for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
    {
        CarData* lpCarData = GetCarData(static_cast<EActiveRaceCarIndex>(liIndex));
        if (lpCarData != NULL)
        {
            GameStateModuleIO::CarScoreData* lpScoreData = lpCarData->GetScoreData();
            lpScoreData->SetHighestRacePosition(lpScoreData->GetRacePosition());
        }
    }
}

}
