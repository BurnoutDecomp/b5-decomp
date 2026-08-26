#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineRaceMode.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // BrnGameState::ModeManager (GetScoringSystem / GetGameStateModule / GetCurrentGameMode)
#include "GameSource/GameState/BrnGameStateModule.h"                    // BrnGameState::GameStateModule::GetPlayerActiveRaceCarIndex
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // BrnGameState::ScoringSystem / CarData / CarScoreData keystone
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // GameStateModuleIO::E_GMS_IN_PROGRESS, CarScoreData::GetFinishTime
#include "GameSource/BurnoutConstants.h"                                // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_*
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                // CgsSystem::Time
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

namespace BrnGameState
{
namespace
{
    // Seed for the "earliest finish time" scan: larger than any plausible race finish time, so the
    // first finisher always wins the min(). X360 flt_82020E90 == 7200.0 (two hours), the literal the
    // pseudocode decodes for PreWorldUpdate's initial shortest-time value.
    const f32 KF_NO_WINNER_TIME_SECONDS = 7200.0f;

    // Grace window granted to the field after the winner finishes (PreWorldUpdate) / the maximum the
    // local player may finish behind the winner before the diagnostic assert fires (GetOutroTimeout).
    // X360 flt_82022E34 == 20.0 (the `+ 20.0` literal the pseudocode decodes, reused for the
    // `difference > 20.0` test).
    const f32 KF_OUTRO_GRACE_SECONDS = 20.0f;

    // Base outro hold for a finished local player: the returned timeout is this minus how far behind
    // the winner the player finished. X360 flt_82004FD8 -- a shared .data float whose exact value is
    // not carried in the IDA exports (referenced symbolically by many functions). Modelled as a named
    // estimate; INTEGRATOR resolves the literal from the XEX .data section. (Estimate, provably not
    // 0.0f: the body subtracts the 0..20 s behind-winner delta from it and hands back a positive
    // outro hold.)
    const f32 KF_OUTRO_BASE_SECONDS = 30.0f; // .data @0x82004FD8 (value UNRESOLVED; estimate)

    // Outro hold when the local player did not finish the race. X360 flt_82020F98 -- another shared
    // .data float not carried in the exports. Modelled as a named estimate; INTEGRATOR resolves the
    // literal from the XEX .data section.
    const f32 KF_OUTRO_NOT_FINISHED_SECONDS = 5.0f; // .data @0x82020F98 (value UNRESOLVED; estimate)
}

// X360: BrnGameState::OnlineRaceMode::GetName (already owned by the GetName-only sibling TU).
const char* OnlineRaceMode::GetName() const
{
    return "OnlineRace";
}

// ===========================================================================
// X360: BrnGameState::OnlineRaceMode::PreWorldUpdate (0x82330EB0).
//
// Runs the base per-frame tick, then -- while the current game mode is mid-race
// (E_GMS_IN_PROGRESS) -- recomputes the online race's post-finish time limit: once the
// winner has crossed the line, the remaining racers are given the winner's finish time
// plus a fixed grace window to complete.
//
// SIGNATURE WIDENED 2026-08-26 (wave-B fix round): this overrides GameMode vtable slot 2, which
// the console dispatches with SIX arguments (UpdateCurrentMode @0x82350EC8:
// `(*(**(a1+3480)+8))(mode, a2, a3, a8, a28, a30, a1+3504)`). The previous declaration was no-arg,
// matching the then-committed base; once the base was corrected to the console shape, a no-arg
// override would have silently minted a NEW vtable slot instead of binding to slot 2.
//
// The body is unchanged: the ScoringSystem it needs is still reached through the owning
// ModeManager (ModeManager::GetScoringSystem -- the X360 reaches the SAME embedded object as
// *(mpModeManager+0xDB0)), because the base's 6th parameter is `const ScoringSystem*` while the
// winner-time leg calls the non-const SetTimeLimitSeconds. That is the console's own object, not
// a stand-in.
//
// The X360 pseudocode renders this as `float* PreWorldUpdate(...)`; that return value is the
// inherited sret/this artifact of the chained calls and carries no result -- the override is void.
// ===========================================================================
void OnlineRaceMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                    const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                    bool lbPaused,
                                    const ScoringSystem* lpScoringSystem)
{
    // Base per-frame tick (rival-visibility scan + drive the current state's Update()) -- the
    // console forwards its own arguments straight through.
    GameMode::PreWorldUpdate(lpOutput, lpInput, lpGlobalRaceCars, lpActiveRaceCars,
                             lbPaused, lpScoringSystem);

    // Only recompute the time limit while the race is actually in progress. The X360 reads the
    // current mode's state off the ModeManager's current game mode (*(mpModeManager+0xD98), its
    // meCurrentState @+0x28); de-inlined to the named ModeManager / GameMode accessors.
    const GameMode* lpCurrentGameMode = GetModeManager()->GetCurrentGameMode();
    if (lpCurrentGameMode != NULL &&
        lpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS)
    {
        // Named apart from the (const) 6th parameter above so nothing shadows: the base signature
        // hands us `const ScoringSystem*`, and the winner-time leg needs the mutable object.
        // Console reaches the same embedded ScoringSystem either way (*(mpModeManager+0xDB0)).
        ScoringSystem* lpMutableScoringSystem = GetModeManager()->GetScoringSystem();

        // Find the earliest (smallest positive) finish time across all active-race-car slots: the
        // race winner's time. Slots that have not finished report a zero finish time and are skipped.
        // Seed at 7200.0 s (two hours) -- larger than any plausible finish time.
        f32 lfWinnersTime = KF_NO_WINNER_TIME_SECONDS;
        for (s32 leEnumIndex = E_ACTIVE_RACE_CAR_INDEX_0;
             leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             ++leEnumIndex)
        {
            const f32 lfFinishTime =
                lpMutableScoringSystem->GetFinishTime(static_cast<EActiveRaceCarIndex>(leEnumIndex)).GetFloatVal();
            if (lfFinishTime > 0.0f && lfFinishTime < lfWinnersTime)
            {
                lfWinnersTime = lfFinishTime;
            }

            CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }

        // A winner exists -> cap the remaining racers' clock at the winner's time + the grace window.
        if (lfWinnersTime < KF_NO_WINNER_TIME_SECONDS)
        {
            lpMutableScoringSystem->SetTimeLimitSeconds(lfWinnersTime + KF_OUTRO_GRACE_SECONDS);
        }
    }
}

// ===========================================================================
// X360: BrnGameState::OnlineRaceMode::GetOutroTimeout (0x82330FD0).
//
// The post-race outro hold time for an online race, in seconds. The X360 body:
//   - asserts the ModeManager has a GameStateModule, then reads the local player's active-race-car
//     slot (GameStateModule::GetPlayerActiveRaceCarIndex), asserting it is valid;
//   - scans every active-race-car slot for the race winner's finish time (the smallest positive
//     finish time -- empty slots report a zero time);
//   - reads the local player's own finish time;
//   - when the local player has finished (non-zero time), asserts the player did not finish before
//     the winner and that they finished within the grace window, then returns
//     (KF_OUTRO_BASE_SECONDS - (localPlayersTime - winnersTime)) -- i.e. the closer the player
//     finished to the winner, the longer the outro hold;
//   - when the local player has not finished, returns the fixed not-finished outro
//     (KF_OUTRO_NOT_FINISHED_SECONDS).
//
// The X360 pseudocode renders the return as `float*` (the sret/this artifact threaded through the
// chained Time / assert calls); the actual result is the f32 timeout.
// ===========================================================================
f32 OnlineRaceMode::GetOutroTimeout() const
{
    GameStateModule* lpGameStateModule = GetModeManager()->GetGameStateModule();
    CGS_ASSERT(lpGameStateModule, "mpGameStateModule");

    const EActiveRaceCarIndex leLocalPlayerActiveRaceCarIndex =
        lpGameStateModule->GetPlayerActiveRaceCarIndex();
    CGS_ASSERT(leLocalPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
               "leLocalPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");

    ScoringSystem* lpScoringSystem = GetModeManager()->GetScoringSystem();

    // Find the race winner's finish time (the smallest positive finish time across all slots). Empty
    // slots have a NULL CarData and report a zero finish time. The winner is seeded by the first
    // positive time seen (lfWinnersTime == 0.0 means "none seen yet").
    f32 lfWinnersTime = 0.0f;
    for (s32 leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         ++leActiveRaceCarIndex)
    {
        CGS_ASSERT((leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) &&
                   (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        const CarData* lpCarData =
            lpScoringSystem->GetCarData(static_cast<EActiveRaceCarIndex>(leActiveRaceCarIndex));
        const CgsSystem::Time lFinishTime =
            (lpCarData != NULL) ? lpCarData->GetScoreData()->GetFinishTime()
                                : CgsSystem::Time(0.0f);
        const f32 lfFinishTime = lFinishTime.GetFloatVal();

        if (lfWinnersTime == 0.0f || (lfFinishTime > 0.0f && lfFinishTime < lfWinnersTime))
        {
            lfWinnersTime = lfFinishTime;
        }

        CGS_ASSERT(leActiveRaceCarIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    // The local player's own finish time.
    CGS_ASSERT((leLocalPlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) &&
               (leLocalPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
               "(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

    const CarData* lpLocalPlayerCarData = lpScoringSystem->GetCarData(leLocalPlayerActiveRaceCarIndex);
    const CgsSystem::Time lLocalPlayerFinishTime =
        (lpLocalPlayerCarData != NULL) ? lpLocalPlayerCarData->GetScoreData()->GetFinishTime()
                                       : CgsSystem::Time(0.0f);
    const f32 lfLocalPlayersTime = lLocalPlayerFinishTime.GetFloatVal();

    // If the local player has finished, the outro hold scales with how close they finished to the
    // winner. If they have not finished, return the fixed not-finished outro.
    if (lfLocalPlayersTime != 0.0f)
    {
        CGS_ASSERT(lfLocalPlayersTime >= lfWinnersTime, "lfLocalPlayersTime >= lfWinnersTime");

        const f32 lfTimeBehindWinner = lfLocalPlayersTime - lfWinnersTime;
        if (lfTimeBehindWinner > KF_OUTRO_GRACE_SECONDS)
        {
            // Diagnostic assert (X360 builds the message inline from the three times before firing).
            CGS_ASSERT(lfTimeBehindWinner <= KF_OUTRO_GRACE_SECONDS,
                       "lfLocalPlayersTime - lfWinnersTime <= KF_OUTRO_GRACE_SECONDS");
        }

        return KF_OUTRO_BASE_SECONDS - lfTimeBehindWinner;
    }

    return KF_OUTRO_NOT_FINISHED_SECONDS;
}

// X360 vtable slot 24 (vtbl+96): 0x82C296C8 == `li r3,1; blr` at slot 24 of vtable 0x820D07F0,
// against the GameMode base's 0x827E2F38 == `li r3,0; blr`. SetupGameMode @0x8234B158 reads it
// twice and HandleLoadingScreenLoaded @0x8234B8A8 once -- an online race DOES put up a loading
// screen, which is what separates it from every offline mode.
bool OnlineRaceMode::HasLoadingScreen() const
{
    return true;
}
}
