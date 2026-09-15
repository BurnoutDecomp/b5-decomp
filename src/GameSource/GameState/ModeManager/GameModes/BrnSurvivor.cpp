#include "GameSource/GameState/ModeManager/GameModes/BrnSurvivor.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdlib>   // [DIAG] getenv -- BRN_MM_DIAG only

namespace BrnGameState
{
const f32 SurvivorMode::KF_OUTRO_TIME_SECONDS = 0.0f;

// ARTIST 0x823322B8. The mutable tuning constants at 0x82CDB7BC..C8 are
// 2, 4, 5.0f, 5.0f respectively in the original image.
void SurvivorMode::Start(const StartGameModeParams* lpStartGameModeParams,
                         GameModeParams* lpGameModeParams, ScoringSystem*)
{
    CGS_ASSERT(lpStartGameModeParams->GetEventData(), "lpEventData");
    const BrnProgression::ProgressionRankData* lpRank = lpStartGameModeParams->GetProgressionRankData();
    CGS_ASSERT(lpRank, "lpProgressionRankData");
    lpGameModeParams->Construct(lpStartGameModeParams->GetGameModeType());

    miMaxOpponentCount = static_cast<s32>(std::fma(3.0f, lpStartGameModeParams->GetProgressionRankAsRatio(), 2.0f));
    if (miMaxOpponentCount >= 4)
        miMaxOpponentCount = 4;
    lpGameModeParams->muJunctionID = lpStartGameModeParams->GetJunctionID();
    lpGameModeParams->miNumNetworkPlayers = 0;
    lpGameModeParams->muEventJunctionID = lpStartGameModeParams->GetEventJunctionId();
    lpGameModeParams->SetNumRivals(miMaxOpponentCount);

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "lpProgressionRankData->GetTrafficDensitySurvival() :     "
                                  << lpRank->GetTrafficDensitySurvival() << "\n";
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "lpStartGameModeParams->GetTrafficDensity() :             "
                                  << lpStartGameModeParams->GetTrafficDensity() << "\n";
    lpGameModeParams->SetTrafficDensityScale(lpStartGameModeParams->GetTrafficDensity() * lpRank->GetTrafficDensitySurvival());
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "lpProgressionRankData->GetLargeVehicleProbability() :    "
                                  << lpRank->GetLargeVehicleProbability() << "\n";
    lpGameModeParams->SetLargeVehicleProbability(lpRank->GetLargeVehicleProbability());
    lpGameModeParams->SetProgressionRankAsRatio(lpStartGameModeParams->GetProgressionRankAsRatio());
    lpRank->GetOvertakingDifficulty(lpGameModeParams->mfOvertakingDifficulty);
    lpGameModeParams->SetDefaultPlayerRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(1));
    lpGameModeParams->SetDefaultAIRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(6));
    // 0x823325A0..D0: OR 0x00000006E4016823; Hex-Rays corrupts both halves.
    lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_SET_CARS_TO_START_GRID
        | GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD
        | GameModeParams::KU_FLAG_WRAP_AI_CARS_WHEN_OUT_OF_RANGE
        | GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC
        | GameModeParams::KU_FLAG_HAS_ROUTE
        | GameModeParams::KU_FLAG_AI_DRIVE_BY_START
        | GameModeParams::KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL
        | GameModeParams::KU_FLAG_ROLLING_START
        | GameModeParams::KU_FLAG_USES_NAVIGATION
        | GameModeParams::KU_FLAG_AI_PERSISTENT_DAMAGE
        | GameModeParams::KU_FLAG_AI_RESET_ON_TRACK_BEHIND
        | GameModeParams::KU_FLAG_ENFORCE_SOFT_TAKEDOWNS
        | GameModeParams::KU_FLAG_SET_OPPONENTS_TO_COPS);
    lpGameModeParams->SetStartMechanism(lpStartGameModeParams->GetStartMechanism());
    lpGameModeParams->SetTrafficLightTriggerId(lpStartGameModeParams->GetTrafficLightTriggerId());
    mpModeManager->SetStartingGrid(lpGameModeParams, lpGameModeParams->GetNumRivals() + 1, false);
    lpGameModeParams->SetAISpeedSelectionMethod(static_cast<EAISpeedSelMethod_Stub>(2));
    lpGameModeParams->SetAIAggresiveCarCount(miMaxOpponentCount);
    lpGameModeParams->SetPlayerWreckCount(CalculateMaxPlayerWrecks(lpStartGameModeParams));
    mfRampTimer = 0.0f;
    mfTimeInReverse = 0.0f;
    miBroadcastOpponentCount = 0;
    mbInShortcut = false;
    // VMX vmaddfp encodes A*C+B: (max-min)*rank + min.
    mfMaxRampTimer = std::fma(5.0f - 5.0f, lpGameModeParams->mfProgressionRankAsRatio, 5.0f);

    // ---- [DIAG] NOT IN THE X360 BINARY -- issue #24, BRN_MM_DIAG=1 ---------------------------
    // The rival COUNT is arithmetic on the profile's rank ratio (ARTIST 0x823323A0:
    // (dword_82CDB7C0 + 1 - dword_82CDB7BC) * ratio + dword_82CDB7BC, capped at dword_82CDB7C0;
    // the image holds 2 and 4 there, so this is fma(3, ratio, 2) capped at 4). Printing the ratio
    // AND the result is the only way to say whether "barely any enemies" is the console's own
    // number or a lost rival. DELETE-WHEN issue #24 is closed.
    if (std::getenv("BRN_MM_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[mm-start] MARKED MAN rankRatio " << lpStartGameModeParams->GetProgressionRankAsRatio()
            << " maxOpponents " << miMaxOpponentCount
            << " numRivals " << lpGameModeParams->GetNumRivals()
            << " startLocations " << lpGameModeParams->GetStartLocationCount()
            << " maxRampTimer " << mfMaxRampTimer
            << "\n";
    }
    // ---- end [DIAG] --------------------------------------------------------------------------
}

// ARTIST 0x8234D188. Update the threat ramp before publishing it to the HUD.
void SurvivorMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
    const GameStateModuleIO::PreWorldInputBuffer* lpInput,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
    bool lbPaused, const ScoringSystem* lpScoringSystem)
{
    const GameStateModuleIO::TimerStatusInterface* lpTimer = lpInput->GetTimerStatusInterface();
    const f32 lfTimeStep = lpTimer->maEntries[1].mfValue08 * lpTimer->maEntries[1].mfValue04;
    if (GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS && !(mfRampTimer >= mfMaxRampTimer))
        mfRampTimer += lfTimeStep;
    GameMode::PreWorldUpdate(lpOutput, lpInput, lpGlobalRaceCars, lpActiveRaceCars, lbPaused, lpScoringSystem);
    if (lpActiveRaceCars->GetPlayerRaceCarState()->mbCrashing)
        mfRampTimer = 0.0f;

    s32 liOpponentCount = 0;
    if (!mbInShortcut)
    {
        const f32 lfRamp = mfRampTimer / mfMaxRampTimer;
        const f32 lfDistance = lpScoringSystem->GetRaceCarDistanceToFinish(lpActiveRaceCars->GetPlayerActiveRaceCarIndex());
        liOpponentCount = static_cast<s32>(static_cast<f32>(miMaxOpponentCount) * lfRamp);
        if (!(lfDistance >= 2000.0f))
        {
            // The two fctiwz conversions happen BEFORE their integer sum.
            liOpponentCount += static_cast<s32>(std::fma(-lfDistance, 0.0005f, 1.0f)
                                                  * static_cast<f32>(miMaxOpponentCount + 1));
        }
        if (liOpponentCount > miMaxOpponentCount)
            liOpponentCount = miMaxOpponentCount;
    }
    // ---- [DIAG] NOT IN THE X360 BINARY -- issue #24, BRN_MM_DIAG=1 ---------------------------
    // Why HERE: `UpdateOpponents` is the ONLY producer of action 129, and action 129 is the ONLY
    // writer of RaceCar::mbIsAllowedInRoadRage, which IsRaceCarWrappable @0x822E9E18 tests third.
    // Run 1 measured allowedRR 0 for the whole event and rivals 2.7-5.6 km away, so the question
    // is exactly which term of this arithmetic is pinning liOpponentCount to miBroadcastOpponent-
    // Count. Sampled once per second off the mode's own time step.
    // DELETE-WHEN issue #24 is closed.
    {
        static const bool sbMarkedManDiag = (std::getenv("BRN_MM_DIAG") != 0);
        static f32 sfDiagClock = 0.0f;
        static s32 siDiagCalls = 0;
        if (sbMarkedManDiag && CgsDev::Log::gpDebugPrint != 0)
        {
            // The first 20 calls ALWAYS print. A purely dt-driven sample cannot distinguish
            // "this function never ran" from "dt is 0" -- and dt being 0 was the whole defect.
            ++siDiagCalls;
            sfDiagClock += lfTimeStep;
            if (siDiagCalls <= 20 || sfDiagClock >= 1.0f
                || miBroadcastOpponentCount != liOpponentCount)
            {
                sfDiagClock = 0.0f;
                *CgsDev::Log::gpDebugPrint
                    << "[mm-pre] call " << siDiagCalls
                    << " state " << GetCurrentState()
                    << " dt " << lfTimeStep
                    << " rate " << lpTimer->maEntries[1].mfValue04
                    << " scale " << lpTimer->maEntries[1].mfValue08
                    << " ramp " << mfRampTimer << "/" << mfMaxRampTimer
                    << " maxOpp " << miMaxOpponentCount
                    << " distToFinish "
                    << lpScoringSystem->GetRaceCarDistanceToFinish(
                           lpActiveRaceCars->GetPlayerActiveRaceCarIndex())
                    << " count " << liOpponentCount
                    << " bcast " << miBroadcastOpponentCount
                    << " shortcut " << static_cast<s32>(mbInShortcut ? 1 : 0)
                    << " playerCrashing "
                    << static_cast<s32>(lpActiveRaceCars->GetPlayerRaceCarState()->mbCrashing ? 1 : 0)
                    << " playerActive "
                    << static_cast<s32>(lpActiveRaceCars->IsPlayerCarActive() ? 1 : 0)
                    << "\n";
            }
        }
    }
    // ---- end [DIAG] --------------------------------------------------------------------------

    mbInShortcut = false;
    if (miBroadcastOpponentCount != liOpponentCount)
    {
        UpdateOpponents(lpOutput, liOpponentCount);
        miBroadcastOpponentCount = liOpponentCount;
    }
    if (lpActiveRaceCars->IsPlayerCarActive() && !lpActiveRaceCars->IsPlayerCarCrashing()
        && lpActiveRaceCars->GetPlayerRaceCarState()->mi8Gear == 0)
        mfTimeInReverse += lfTimeStep;
    else
        mfTimeInReverse = 0.0f;
}

// ARTIST 0x823451F8. Slot zero is the player, hence count + 1.
void SurvivorMode::UpdateOpponents(GameStateModuleIO::OutputBuffer* lpOutput, s32 liOpponentCount)
{
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "<AI> Allowed in Marked man :";
    for (::EActiveRaceCarIndex leIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; leIndex++)
    {
        GameStateModuleIO::AllowCarToJoinRoadRageAction lAction;
        lAction.mActiveRaceCarIndex = leIndex;
        lAction.mbAllowedInRoadRage = leIndex < liOpponentCount + 1;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << (lAction.mbAllowedInRoadRage ? "Yes," : "No,");
        lpOutput->GetGameActionQueue()->AddEvent(&lAction, GameStateModuleIO::E_ACTION_ALLOW_CAR_TO_JOIN_ROAD_RAGE);
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "\n";
}

// ARTIST 0x82316318: preserve the ordered comparisons in the original exit ladder.
bool SurvivorMode::ShouldExit(const ScoringSystem* lpScoringSystem) const
{
    const f32 lfNoInput = lpScoringSystem->GetPlayerNoInputTime();
    const f32 lfStationary = lpScoringSystem->GetPlayerStationaryTime();
    if (!(lfNoInput < 3.0f) && !(lfStationary < 3.0f))
        return true;
    if (mfTimeInReverse > 7.0f)
        return true;
    if (lfNoInput <= 4.0f || lfStationary <= 3.0f)
        return false;
    return !mbVisibleCars || lfStationary > 10.0f;
}

// ARTIST 0x82316398 / 0x827E2580.
void SurvivorMode::OnPlayerInShortCut() { mbInShortcut = true; }
void SurvivorMode::OnPlayerUsesPaintShop() { mfRampTimer = 0.0f; }

// ARTIST 0x823163A8: r4 is scoring, r5 is the result action (not r3/r4).
void SurvivorMode::FillInGameModeSpecificResults(const ScoringSystem* lpScoringSystem,
                                                GameStateModuleIO::FinishedModeAction* lpAction)
{
    lpAction->miFinishPosition = lpScoringSystem->IsPlayerTotalled() ? 2 : 1;
}

// X360: BrnGameState::SurvivorMode::GetName.
const char* SurvivorMode::GetName() const
{
    return "Survivor";
}

// X360: BrnGameState::SurvivorMode::GetOutroTimeout. Returns the fixed constant (0.0). The DWARF
// declaration (virtual / trailing const / f32) is authoritative over the Hex-Rays double.
f32 SurvivorMode::GetOutroTimeout() const
{
    return KF_OUTRO_TIME_SECONDS;
}
}
