#include "GameSource/GameState/ModeManager/GameModes/BrnFaceOffMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                     // CgsNumeric::Random::RandomUInt
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GameStateModuleIO::EGameModeType
#include "GameSource/GameState/ModeManager/BrnModeManager.h"              // ModeManager::GetTriggerData / GetRandom
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // GameModeParams / StartGameModeParams

namespace BrnGameState
{
namespace
{
    // DWARF BrnFaceOffMode.cpp:58.
    const s32 KI_NUM_RIVALS_IN_FACE_OFF = 1;

    // 0x823304A0 `lfs f0, flt_82020E88 -> stfs 0x30(r31)`. Hex-Rays prints 0.75. Name not attested.
    const f32 KF_FACE_OFF_TRAFFIC_DENSITY_SCALE = 0.75f;

    // 0x82330484/0x82330498 `oris 1 / ori 0x2002` on the 64-bit word at params+0x860.
    const u64 KU_FACE_OFF_MODE_FLAGS =
        GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD |
        GameModeParams::KU_FLAG_HAS_ROUTE                |
        GameModeParams::KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL;

    // 0x823304AC..0x823304BC: `li r11,1` stored to +0x848/+0x840/+0x844/+0x84C.
    const s32 KI_FACE_OFF_PLAYER_ROUTE_FINDING_STYLE = 1;
    const s32 KI_FACE_OFF_AI_ROUTE_FINDING_STYLE     = 1;
    const s32 KI_FACE_OFF_AI_SPEED_SELECTION_METHOD  = 1;
    const s32 KI_FACE_OFF_AI_AGGRESSIVE_CAR_COUNT    = 1;

    // SelectRandomDestinations' distance bands (`f1` min, `f2` max, helper 0x82321FF4/0x82321FFC:
    // accepted when dist > f1 && dist < f2). The names are not attested; the values are read at the
    // two call sites:
    //   first  call: 0x8233051C f1 = flt_820266C4 == 1500.0, 0x823304F0 f2 = flt_82020FA4 == 1000.0
    //   second call: 0x8233055C f1 = flt_8200A034 ==  500.0, 0x82330550 f2 = flt_82004D08 == 2000.0
    // WARNING: THE FIRST BAND IS EMPTY ON THE CONSOLE: 1500 < dist < 1000 cannot hold, so that call
    // only ever returns through the helper's debug-landmark override. Unless that override is on,
    // every face off comes from the second, unfiltered band. Reproduced as written, not "fixed" by
    // swapping the bounds.
    const f32 KF_DIRECTED_SEARCH_MIN_DISTANCE   = 1500.0f;
    const f32 KF_DIRECTED_SEARCH_MAX_DISTANCE   = 1000.0f;
    const f32 KF_UNDIRECTED_SEARCH_MIN_DISTANCE = 500.0f;
    const f32 KF_UNDIRECTED_SEARCH_MAX_DISTANCE = 2000.0f;

    // The two stack arrays handed to SelectRandomDestinations are 0x400 bytes each (0x8F0+var_850 and
    // 0x8F0+var_450) of 16-bit elements. The helper does not bound-check them; it trusts this size.
    const u32 KU_MAX_LANDMARKS_IN_RANGE = 512u;
}

const f32 FaceOffMode::KF_OUTRO_TIME_SECONDS = 0.0f;

// X360: BrnGameState::FaceOffMode::GetName.
const char* FaceOffMode::GetName() const
{
    return "FaceOff";
}

// X360: BrnGameState::FaceOffMode::GetOutroTimeout. Returns the fixed constant (0.0). The DWARF
// declaration (virtual / trailing const / f32) is authoritative over the Hex-Rays double.
f32 FaceOffMode::GetOutroTimeout() const
{
    return KF_OUTRO_TIME_SECONDS;
}

// X360: slot 8 of FaceOffMode's vtable 0x820D0500 = 0x827EAB30 (ICF-shared body). Its only load is
// flt_82001CC0, which x360rd reads as 0x00000000 == 0.0f: no intro (see the header).
f32 FaceOffMode::GetIntroDurationSeconds() const
{
    return 0.0f;
}

// X360 0x82330670: `b GameMode::PreWorldUpdate`, with all six arguments forwarded untouched.
void FaceOffMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                 const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                 const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                 const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                 bool lbPaused,
                                 const ScoringSystem* lpScoringSystem)
{
    GameMode::PreWorldUpdate(lpOutput, lpInput, lpGlobalRaceCars, lpActiveRaceCars, lbPaused, lpScoringSystem);
}

// X360 0x82330440 (BrnFaceOffMode.cpp:81). Fills the params block, then picks the one destination
// of the race: a random landmark in range of the player, drawn from the ModeManager's own RNG and
// appended as the event's only checkpoint.
//
// NO START GRID, ON THE CONSOLE TOO: SetStartingGrid's only ARTIST callers are the BurningRoute, Race,
// RoadRage, StuntAttack and Survivor Start bodies, and it and SetupOnlineStartingGrid are the only
// callers of the start-location Append. This mode therefore reaches RaceCarEntityModule::
// SetUpAIForMode with no start location. Nothing on this build can select the mode either (see
// HarnessForceFaceOffEnabled in GameStateModule_gSR_00.cpp): it is dead code, kept as written.
//
// CONSOLE OFFSET -> NAME for the copies below (r30 = start params, r31 = params):
//   start +0x2D0 meGameModeType  +0x2E0 mPlayerPosition  +0x2F0 mStartDirection
//         +0x304 mePursuedCarGlobalIndex  +0x310 meStartMechanism
//         +0x328 muEventJunctionId        +0x330 muJunctionID
//   param +0x00 miNumRivals  +0x01 miNumNetworkPlayers  +0x30 mfTrafficDensityScale
//         +0x3C meStartMechanism  +0x44 muEventJunctionID  +0x48 muJunctionID
//         +0x54 mePursuedCarGlobalIndex  +0x260 maCheckpointDataArray  +0x860 muFlags
void FaceOffMode::Start(const StartGameModeParams* lpStartGameModeParams,
                        GameModeParams*            lpGameModeParams,
                        ScoringSystem*             /*lpScoringSystem*/)
{
    lpGameModeParams->Construct(lpStartGameModeParams->GetGameModeType());

    lpGameModeParams->muJunctionID        = lpStartGameModeParams->GetJunctionID();
    lpGameModeParams->miNumNetworkPlayers = 0;
    lpGameModeParams->SetFlag(KU_FACE_OFF_MODE_FLAGS);
    lpGameModeParams->SetTrafficDensityScale(KF_FACE_OFF_TRAFFIC_DENSITY_SCALE);
    lpGameModeParams->SetNumRivals(KI_NUM_RIVALS_IN_FACE_OFF);
    lpGameModeParams->SetAISpeedSelectionMethod(static_cast<EAISpeedSelMethod_Stub>(KI_FACE_OFF_AI_SPEED_SELECTION_METHOD));
    lpGameModeParams->SetDefaultPlayerRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(KI_FACE_OFF_PLAYER_ROUTE_FINDING_STYLE));
    lpGameModeParams->muEventJunctionID   = lpStartGameModeParams->GetEventJunctionId();
    lpGameModeParams->SetDefaultAIRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(KI_FACE_OFF_AI_ROUTE_FINDING_STYLE));
    lpGameModeParams->SetAIAggresiveCarCount(KI_FACE_OFF_AI_AGGRESSIVE_CAR_COUNT);
    lpGameModeParams->SetStartMechanism(lpStartGameModeParams->GetStartMechanism());
    lpGameModeParams->mePursuedCarGlobalIndex = lpStartGameModeParams->GetPursuedCarGlobalIndex();

    miNumRivalsInRace = KI_NUM_RIVALS_IN_FACE_OFF;

    // 0x823304DC..0x823304E4 `lwz 0x6D60(mode manager) ; addi 0x620 ; bl TriggerData_::GetMemor`.
    const BrnTrigger::TriggerData* lpTriggerData = GetModeManager()->GetTriggerData();

    LandmarkIndex lauLandmarkIndices[KU_MAX_LANDMARKS_IN_RANGE];
    u16           lauAISectionIndices[KU_MAX_LANDMARKS_IN_RANGE];

    u32 luNumLandmarksInRange = SelectRandomDestinations(lpTriggerData,
                                                         lpStartGameModeParams->GetPlayerPosition(),
                                                         lpStartGameModeParams->GetStartDirection(),
                                                         KF_DIRECTED_SEARCH_MIN_DISTANCE,
                                                         KF_DIRECTED_SEARCH_MAX_DISTANCE,
                                                         lauLandmarkIndices,
                                                         lauAISectionIndices,
                                                         true);
    if (luNumLandmarksInRange == 0u)
    {
        luNumLandmarksInRange = SelectRandomDestinations(lpTriggerData,
                                                         lpStartGameModeParams->GetPlayerPosition(),
                                                         lpStartGameModeParams->GetStartDirection(),
                                                         KF_UNDIRECTED_SEARCH_MIN_DISTANCE,
                                                         KF_UNDIRECTED_SEARCH_MAX_DISTANCE,
                                                         lauLandmarkIndices,
                                                         lauAISectionIndices,
                                                         false);
    }

    // Console line 150.
    CGS_ASSERT(luNumLandmarksInRange != 0u, "Still no landmarks found, please make a note of the world location");

    // 0x82330598..0x82330614: Random::RandomUInt(0, n - 1) inlined with its -1 / +1 folded, i.e. the
    // CgsRandom.h:303 "luMod > 0" assert, the seed step and draw % n (CgsRandom.cpp RandomUInt note).
    const u32 luChosenLandmarkLocalIndex = GetModeManager()->GetRandom()->RandomUInt(0u, luNumLandmarksInRange - 1u);

    lpGameModeParams->AddCheckpoint(lauLandmarkIndices[luChosenLandmarkLocalIndex],
                                    lauAISectionIndices[luChosenLandmarkLocalIndex]);

    // Console line 157, after the Append as on the console.
    CGS_ASSERT(luChosenLandmarkLocalIndex < luNumLandmarksInRange,
               "luChosenLandmarkLocalIndex < luNumLandmarksInRange");
}

// X360 0x82315CC0 (BrnFaceOffMode.cpp:195). Console lines 197/198 are the whole body: no field of
// the action is written on this build.
void FaceOffMode::FillInGameModeSpecificResults(const ScoringSystem* lpScoringSystem,
                                                GameStateModuleIO::FinishedModeAction* lpAction)
{
    CGS_ASSERT(lpScoringSystem != nullptr, "lpScoringSystem != NULL");
    CGS_ASSERT(lpAction != nullptr, "lpModeResultsAction != NULL");
}
}
