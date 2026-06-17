#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

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
// BLOCKED (omitted -- left declare-only) and why:
//   - OnRoadRagePlayerCrashed (0x823444B0): dereferences the GameStateModuleIO::OutputBuffer
//       param (forward-declared only in the keystone) and CgsModule::VariableEventQueue<13312,16>
//       (no committed home), to push road-rage crash events.
//   - StopModeTimer (0x8231F590): invokes a BaseOnlineModeScoring virtual (vtable slot 6,
//       not in that type's committed minimal slice) and the uncommitted BrnNetwork::NetworkRounder
//       statics. (Its CarScoreData reads -- miRacePosition@+0x08 and miHighestRacePosition@+0x0C --
//       now have committed accessors (GetRacePosition / GetHighestRacePosition) after the
//       CarScoreData grow, so the field access is no longer a blocker; the virtual + NetworkRounder
//       statics are.)
//   - HasStuntAttackModeEnded (0x82326708): the X360 body dispatches through a VIRTUAL at
//       vtable slot +0x14 of two polymorphic embedded sub-scorers (this+0x350 and this+0x2620,
//       both constructed via virtual ctors in ScoringSystem::Construct); the committed
//       StuntModeScoring slice is a plain non-polymorphic struct with no vtable, so that virtual
//       cannot be named. Its parameter shape (this, unused a2, race-car index a3, char force-flag
//       a4) also does not match the keystone's single CgsSystem::Time-by-value decl, and the
//       HasModeTimeExpired() it calls itself needs a CgsSystem::Time& the body does not carry.
//       FLAGGED rather than mis-implemented. (The +0xD9 mbEliminated write IS now reachable via
//       the Foundation-added GetEliminated/SetEliminated accessor -- the vtable + param-shape
//       mismatch is the real blocker.)
//   - StartOnlineGameModeScoring (0x823126C8): the Fugitive/FreeBurn/ModeEnd case targets a
//       fourth embedded online-mode scorer that the keystone (3 online scorers) does not model
//       by name.
//
// DEFERRED (no standalone X360 export -- inlined away on X360, so no authoritative body):
//   StartModeTimer, SetCheckPointsForCarsWithinRace, UpdateTimerForEliminator,
//   HasCrashModeEnded. Left declare-only rather than guessed.
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
