#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineRaceMode.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // BrnGameState::ModeManager (GetScoringSystem / GetGameStateModule / GetCurrentGameMode)
#include "GameSource/GameState/BrnGameStateModule.h"                    // BrnGameState::GameStateModule::GetPlayerActiveRaceCarIndex
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // BrnGameState::ScoringSystem / CarData / CarScoreData keystone
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // GameStateModuleIO::E_GMS_IN_PROGRESS, CarScoreData::GetFinishTime
#include "GameSource/BurnoutConstants.h"                                // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_*
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                // CgsSystem::Time
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"  // the start events + the round index
#include "SDKs/XboxMath/XMVectorACos.h"                                 // XboxMath::XMVectorACos (X360 0x821F0980)
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"           // BrnTraffic::TrafficData (GetHull)
#include "SharedClasses/Traffic/BrnTrafficHull.h"                       // BrnTraffic::Hull
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"               // BrnTraffic::LightTriggerStartData
#include "SharedClasses/Trigger/BrnLandmark.h"                          // Landmark::GetBoxRegion
#include "SharedClasses/Trigger/BrnRegion.h"                            // BoxRegion::GetPosition
#include "SharedClasses/Trigger/BrnTriggerData.h"                       // TriggerData::GetLandmarkFromRegionIndex
#include "rw/math/vpu/vector3_operation.h"                              // Dot / Normalize / operator-

#include <cmath>                                                        // std::fabs (the |angle| vandc128)

namespace BrnGameState
{
namespace
{
    // flt_82021210 == 0.40000001f, the exact single-precision value Start stores.
    const f32 KF_ONLINE_RACE_TRAFFIC_DENSITY = 0.40000001f;

    // `li r27,1` / `stw r27,0x840` and `stw r27,0x844` @0x82338F18. The console value only; the
    // member is still the ERouteFindingStyle_Stub placeholder, so the enumerator has no name here.
    const s32 KI_ONLINE_RACE_ROUTE_FINDING_STYLE = 1;

    // Assert bounds GetBestStartGridID carries: "liNumLightsFound < KI_MAX_LIGHTS_IN_ONE_JUNCTION"
    // (`cmpwi 8`) and "luHull < KU_MAX_HULLS" (BrnTrafficLightTrigger.h:211, 0x190).
    const s32 KI_MAX_LIGHTS_IN_ONE_JUNCTION = 8;
    const u32 KU_MAX_HULLS                  = 0x190;

    // flt_82020AFC, GetBestStartGridID's search seed.
    const f32 KF_FLT_MAX = 3.4028235e38f;

    // Seed for the "earliest finish time" scan: larger than any plausible race finish time, so the
    // first finisher always wins the min(). X360 flt_82020E90 == 7200.0 (two hours), the literal the
    // pseudocode decodes for PreWorldUpdate's initial shortest-time value.
    const f32 KF_NO_WINNER_TIME_SECONDS = 7200.0f;

    // Grace window granted to the field after the winner finishes (PreWorldUpdate) / the maximum the
    // local player may finish behind the winner before the diagnostic assert fires (GetOutroTimeout).
    // X360 flt_82022E34 == 20.0 (the `+ 20.0` literal the pseudocode decodes, reused for the
    // `difference > 20.0` test).
    const f32 KF_OUTRO_GRACE_SECONDS = 20.0f;

    // Original ARTIST XEX: 0x82004FD8, BE 41c80000 = 25.0f. The finished-player
    // hold subtracts the player's finish-time delta from this base (GetOutroTimeout 82330FD0).
    const f32 KF_OUTRO_BASE_SECONDS = 25.0f;

    // Original ARTIST XEX: 0x82020F98, BE 40a00000 = 5.0f.
    const f32 KF_OUTRO_NOT_FINISHED_SECONDS = 5.0f;
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

// ===========================================================================
// X360: BrnGameState::OnlineRaceMode::Start (0x82338EB8).
//
// lpStartGameModeParams only goes on to GetBestStartGridID, and lpScoringSystem is never read. The
// mode type is the literal `li r4,0xA`, not the start params' own.
//
// The long inlined block under the first-round test is Random::Construct followed by
// Random::SetSeed(muRandomSeedForGame): instruction for instruction the block OnlineStuntRunMode::
// Start @0x82339E70 carries, with the seed `lwz r9, 0xC(r31)` at the same place. Later rounds keep
// the generator where the last round left it.
// ===========================================================================
void OnlineRaceMode::Start(const StartGameModeParams* lpStartGameModeParams,
                           GameModeParams*            lpGameModeParams,
                           ScoringSystem*             lpScoringSystem)
{
    (void)lpScoringSystem;

    const NetworkRoundManager* lpNetworkRoundManager = GetModeManager()->GetNetworkRoundManager();
    const GameStateModuleIO::StartNetworkGameEvent*  lpStartNetworkGameEvent  = lpNetworkRoundManager->GetNetworkGameEvent();
    const GameStateModuleIO::StartNetworkRoundEvent* lpStartNetworkRoundEvent = lpNetworkRoundManager->GetNetworkRoundEvent();

    const s32  liCurrentRound = lpNetworkRoundManager->GetCurrentRound();
    const bool lbIsFirstRound = (liCurrentRound == 0);

    mbIsOnline = true;

    lpGameModeParams->Construct(GameStateModuleIO::E_MODE_ONLINE_RACE);

    lpGameModeParams->SetDefaultPlayerRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(KI_ONLINE_RACE_ROUTE_FINDING_STYLE));
    lpGameModeParams->SetDefaultAIRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(KI_ONLINE_RACE_ROUTE_FINDING_STYLE));
    lpGameModeParams->mbIsOnline = true;
    lpGameModeParams->SetTrafficDensityScale(KF_ONLINE_RACE_TRAFFIC_DENSITY);
    lpGameModeParams->mbInfiniteBoost = false;
    lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_USES_NAVIGATION |
                              GameModeParams::KU_FLAG_HAS_ROUTE |
                              GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC);   // `oris 0x2000 / ori 0x2800`
    lpGameModeParams->SetProgressionRankAsRatio(0.0f);
    lpGameModeParams->SetStartMechanism(E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS);
    lpGameModeParams->meOnlineBoostStrategy = static_cast<EBoostType_Stub>(lpStartNetworkGameEvent->meBoostType);

    CGS_ASSERT(lpStartNetworkRoundEvent->miNumLandmarksInRound > 0,
               "lpStartNetworkRoundEvent->miNumLandmarksInRound > 0");

    // The grid goes on the junction's light that faces the round's first landmark most directly.
    const BrnTrigger::Landmark* lpFirstLandmark = GetModeManager()->GetTriggerData()->GetLandmarkFromRegionIndex(
        static_cast<s32>(lpStartNetworkRoundEvent->maLandmarks[0]));

    lpGameModeParams->SetTrafficLightTriggerId(GetBestStartGridID(lpStartNetworkRoundEvent->mLightTriggerID,
                                                                  lpStartGameModeParams,
                                                                  lpFirstLandmark->GetBoxRegion()->GetPosition()));

    if (!lpStartNetworkGameEvent->mbIsTrafficOn)
    {
        lpGameModeParams->SetTrafficDensityScale(0.0f);
    }
    if (!lpStartNetworkGameEvent->mbIsTrafficCheckingOn)
    {
        lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_TRAFFIC_CHECKING_NOT_ALLOWED);
    }

    if (lbIsFirstRound)
    {
        mRandom.Construct();
        mRandom.SetSeed(lpStartNetworkGameEvent->muRandomSeedForGame);
    }

    for (s32 liPlayer = 0; liPlayer < GameStateModuleIO::KI_MAX_RACE_CARS; ++liPlayer)
    {
        lpGameModeParams->maePlayerTeam[liPlayer] = static_cast<EPlayerTeam_Stub>(lpStartNetworkGameEvent->maePlayerTeam[liPlayer]);
    }

    GetModeManager()->SetOnlineLandmarks(lpGameModeParams, lpStartNetworkRoundEvent, liCurrentRound);

    // The first round shuffles the grid; later rounds line up by the standings so far
    // (`cntlzw r11, r27 / extrwi r7, r11, 1,26` == first-round flag negated).
    GetModeManager()->SetupOnlineStartingGrid(lpGameModeParams, lpStartNetworkGameEvent->miNumRaceCars,
                                              &mRandom, !lbIsFirstRound);
    GetModeManager()->SetOnlineRaceCars(lpGameModeParams, lpStartNetworkGameEvent);
}

// ===========================================================================
// X360: BrnGameState::OnlineRaceMode::GetBestStartGridID (0x82330B88).
//
// The console's instruction stream is the same as OnlineStuntRunMode::GetBestStartGridID
// @0x82331708 apart from the assert line numbers (diffed), so the two bodies agree here too.
//
// lpStartGameModeParams arrives in r6 and is never read.
// ===========================================================================
LightTriggerId OnlineRaceMode::GetBestStartGridID(LightTriggerId             luTriggerId,
                                                  const StartGameModeParams* lpStartGameModeParams,
                                                  Vector3                    lv3Reference)
{
    (void)lpStartGameModeParams;

    const u32 luHull     = (luTriggerId >> 8) & 0xFFFFu;
    const u8  luJunction = static_cast<u8>(luTriggerId & 0xFFu);

    const BrnTraffic::TrafficData* lpTrafficData = GetModeManager()->GetTrafficData();
    const BrnTraffic::Hull*        lpHull        = lpTrafficData->GetHull(luHull);
    const u8 luJunctionGroup = lpHull->mpaLightTriggerJunctionLookup[luJunction];

    CGS_ASSERT(lpHull->muNumLightTriggers == lpHull->muNumLightTriggersStartData,
               "lpHull->muNumLightTriggers == lpHull->muNumLightTriggersStartData");

    LightTriggerId laPackedIds[KI_MAX_LIGHTS_IN_ONE_JUNCTION];
    Vector3        laStartPositions[KI_MAX_LIGHTS_IN_ONE_JUNCTION];
    Vector3        laStartDirections[KI_MAX_LIGHTS_IN_ONE_JUNCTION];
    s32            liNumLightsFound = 0;

    for (u32 luLight = 0; luLight < lpHull->muNumLightTriggers; ++luLight)
    {
        if (luJunctionGroup != lpHull->mpaLightTriggerJunctionLookup[luLight])
        {
            continue;
        }

        CGS_ASSERT(liNumLightsFound < KI_MAX_LIGHTS_IN_ONE_JUNCTION,
                   "liNumLightsFound < KI_MAX_LIGHTS_IN_ONE_JUNCTION");
        CGS_ASSERT(luHull < KU_MAX_HULLS, "luHull < KU_MAX_HULLS");

        const BrnTraffic::LightTriggerStartData& lStartData = lpHull->mpaLightTriggerStartData[luLight];

        laPackedIds[liNumLightsFound]       = (luHull << 8) | 0x39000000u | luLight;
        laStartDirections[liNumLightsFound] = lStartData.GetStartDirection(0);
        laStartPositions[liNumLightsFound]  = lStartData.GetStartPosition(0);
        ++liNumLightsFound;
    }

    CGS_ASSERT(liNumLightsFound > 0, "liNumLightsFound > 0");

    s32 liBestLightIndex = -1;
    f32 lfBestAngle      = KF_FLT_MAX;

    for (s32 liLight = 0; liLight < liNumLightsFound; ++liLight)
    {
        const Vector3 lv3Direction   = rw::math::vpu::Normalize(laStartDirections[liLight]);
        const Vector3 lv3ToReference = rw::math::vpu::Normalize(lv3Reference - laStartPositions[liLight]);

        f32 lfCosAngle = rw::math::vpu::Dot(lv3Direction, lv3ToReference);
        if (lfCosAngle < -1.0f) { lfCosAngle = -1.0f; }   // vmaxfp128 @0x82330E24
        if (lfCosAngle >  1.0f) { lfCosAngle =  1.0f; }   // vminfp128 @0x82330E28

        const f32 lfAngle = std::fabs(XboxMath::XMVectorACos(lfCosAngle));
        if (lfAngle < lfBestAngle)
        {
            lfBestAngle      = lfAngle;
            liBestLightIndex = liLight;
        }
    }

    CGS_ASSERT(liBestLightIndex >= 0, "liBestLightIndex >= 0");

    return laPackedIds[liBestLightIndex];
}
}
