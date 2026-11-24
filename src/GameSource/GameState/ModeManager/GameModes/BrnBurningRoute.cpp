#include "GameSource/GameState/ModeManager/GameModes/BrnBurningRoute.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/GameModes/BrnBurningRoute.cpp"
namespace BrnGameState
{
const f32 BurningRouteMode::KF_OUTRO_TIME_SECONDS = 0.0f;

// ARTIST 0x82331A80. Use the authored time limit, or the profile's downloaded target.
void BurningRouteMode::Start(const StartGameModeParams* lpStartGameModeParams,
                             GameModeParams* lpGameModeParams, ScoringSystem*)
{
    const BrnProgression::RaceEventData* lpEventData = lpStartGameModeParams->GetEventData();
    CGS_ASSERT(lpEventData, CGS_ASSERT_MESSAGE_AT("lpEventData", CGS_ARTIST_FILE, 67));
    const BrnProgression::ProgressionRankData* lpRank = lpStartGameModeParams->GetProgressionRankData();
    CGS_ASSERT(lpRank, CGS_ASSERT_MESSAGE_AT("lpProgressionRankData", CGS_ARTIST_FILE, 70));
    lpGameModeParams->Construct(lpStartGameModeParams->GetGameModeType());
    lpGameModeParams->muJunctionID = lpStartGameModeParams->GetJunctionID();
    lpGameModeParams->miNumNetworkPlayers = 0;
    lpGameModeParams->SetNumRivals(0);
    lpGameModeParams->muEventJunctionID = lpStartGameModeParams->GetEventJunctionId();

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "lpProgressionRankData->GetTrafficDensityBurningRoute() : "
                                  << lpRank->GetTrafficDensityBurningRoute() << "\n";
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "lpStartGameModeParams->GetTrafficDensity() :             "
                                  << lpStartGameModeParams->GetTrafficDensity() << "\n";
    // Unlike Race/Marked Man, ARTIST does not multiply by the rank's density here.
    lpGameModeParams->SetTrafficDensityScale(lpStartGameModeParams->GetTrafficDensity());
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "lpProgressionRankData->GetLargeVehicleProbability() :    "
                                  << lpRank->GetLargeVehicleProbability() << "\n";
    lpGameModeParams->SetLargeVehicleProbability(lpRank->GetLargeVehicleProbability());
    lpGameModeParams->SetDefaultPlayerRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(1));
    // OR 0x20C02803 at 0x82331C5C..80; the upper word is preserved.
    lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_SET_CARS_TO_START_GRID
        | GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD
        | GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC
        | GameModeParams::KU_FLAG_HAS_ROUTE
        | GameModeParams::KU_FLAG_DISABLE_ALL_TDS
        | GameModeParams::KU_FLAG_DISABLE_CRASH_EXTENSIONS
        | GameModeParams::KU_FLAG_USES_NAVIGATION);
    lpGameModeParams->SetStartMechanism(lpStartGameModeParams->GetStartMechanism());
    lpGameModeParams->SetTrafficLightTriggerId(lpStartGameModeParams->GetTrafficLightTriggerId());
    mpModeManager->SetStartingGrid(lpGameModeParams, lpGameModeParams->GetNumRivals() + 1, false);
    CGS_ASSERT(mpModeManager, CGS_ASSERT_MESSAGE_AT("mpModeManager", CGS_ARTIST_FILE, 106));
    CGS_ASSERT(mpModeManager->GetProgressionManager(), CGS_ASSERT_MESSAGE_AT("mpModeManager->GetProgressionManager()", CGS_ARTIST_FILE, 107));
    BrnProgression::Profile* lpProfile = mpModeManager->GetProgressionManager()->GetProfile();
    CGS_ASSERT(lpProfile, CGS_ASSERT_MESSAGE_AT("mpModeManager->GetProgressionManager()->GetProfile()", CGS_ARTIST_FILE, 108));
    const GameStateModuleIO::TargetEventScore* lpTarget = lpProfile->GetTargetEvent(lpGameModeParams->muJunctionID);
    const f32 lfTime = lpTarget ? static_cast<f32>(lpTarget->miScore) * 0.001f : lpEventData->GetTimeLimitSlow();
    lpGameModeParams->mfNeedForGold = lfTime;
    lpGameModeParams->mfNeedForSilver = lfTime;
    lpGameModeParams->mfNeedForBronze = lfTime;
    lpGameModeParams->mfModeTimeLimit = lfTime;
}

// ARTIST 0x82331D98. All six base-call arguments survive in r4..r9.
void BurningRouteMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
    const GameStateModuleIO::PreWorldInputBuffer* lpInput,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
    bool lbPaused, const ScoringSystem* lpScoringSystem)
{
    CGS_ASSERT(lpOutput, CGS_ASSERT_MESSAGE_AT("lpOutput != NULL", CGS_ARTIST_FILE, 143));
    CGS_ASSERT(lpInput, CGS_ASSERT_MESSAGE_AT("lpInput != NULL", CGS_ARTIST_FILE, 144));
    GameMode::PreWorldUpdate(lpOutput, lpInput, lpGlobalRaceCars, lpActiveRaceCars, lbPaused, lpScoringSystem);
    CGS_ASSERT(lpGlobalRaceCars, CGS_ASSERT_MESSAGE_AT("lpGlobalCarInterface != NULL", CGS_ARTIST_FILE, 155));
    mePlayerActiveIndex = lpGlobalRaceCars->GetActiveRaceCarIndex(lpGlobalRaceCars->GetPlayerGlobalRaceCarIndex());
}

// X360: BrnGameState::BurningRouteMode::GetName.
const char* BurningRouteMode::GetName() const
{
    return "BurningRoute";
}

// X360: BrnGameState::BurningRouteMode::GetOutroTimeout. Returns the fixed constant (0.0). The DWARF
// declaration (virtual / trailing const / f32) is authoritative over the Hex-Rays double.
f32 BurningRouteMode::GetOutroTimeout() const
{
    return KF_OUTRO_TIME_SECONDS;
}

// X360 vtable slot 23 (vtbl+92), folded leaf 0x827E2F38 == `li r3,0; blr` at slot 23 of
// BurningRouteMode's vtable 0x820D06B8; the GameMode base is 0x82C296C8 == `li r3,1`.
// SetupGameMode @0x8234B158 gates the WaitForStreaming path on this.
bool BurningRouteMode::RequiresStreaming() const
{
    return false;
}
}

#undef CGS_ARTIST_FILE
