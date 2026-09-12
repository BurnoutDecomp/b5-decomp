// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_OnlineGrid.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
//
// The two online-only grid legs the Online*Mode::SetupGameModeParams overrides drive
// (BrnOnlineFreeBurnLobbyMode / BrnOnlineShowtimeMode / BrnOnlineStuntRunMode and their
// siblings), both reached only once a network round manager has produced a
// StartNetworkGameEvent:
//
//   ModeManager::SetOnlineRaceCars        -- copy the event's per-car roster into GameModeParams
//   ModeManager::SetupOnlineStartingGrid  -- order the roster into the junction's start block
//
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"                              // GetProgressionManager (assert operand)
#include "GameSource/GameState/BrnGameEvents.h"                                  // StartNetworkGameEvent / KI_MAX_RACE_CARS
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"  // TQM::GetTrafficData
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"     // GameModeParams members / AddStartLocation
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"                 // TrafficData::GetHull
#include "SharedClasses/Traffic/BrnTrafficHull.h"                             // Hull::mpaLightTriggerStartData
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"                     // LightTriggerStartData::GetStart*
#include "GameShared/GameClasses/Algorithms/CgsBubbleSort.h"                  // CgsAlgorithms::BubbleSort
#include "GameShared/GameClasses/Algorithms/CgsShuffle.h"                     // CgsAlgorithms::Shuffle
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                         // CgsNumeric::Random

namespace BrnGameState
{
namespace
{
    // The console's grid is bucketed per player team, plus ONE tail bucket for cars whose scoring
    // record says the player has dropped out -- ten fixed-capacity stack buckets in all, cleared
    // together.
    //
    // [!] VALUE DIVERGENCE, ASM WINS (the same one BrnModeManager_Prepare.cpp already records at
    // its `< 9` team assert): the console's GameStateModuleIO::E_PLAYER_TEAM_COUNT is 9, while the
    // tree's enum still says 3. The literal is used here so the bucket table keeps the console's
    // ten slots; filed for the enum's owner rather than fixed behind its back.
    const s32 KI_PLAYER_TEAM_BUCKET_COUNT = 9;

    // Index of that tail bucket. NAME IS INFERRED -- there is no separate console symbol for it;
    // what IS attested is the test that fills it (CarScoreData::GetDisconnected, the +0x69 flag the
    // network-results update owns), so the bucket is named for its own predicate.
    const s32 KI_DISCONNECTED_BUCKET = KI_PLAYER_TEAM_BUCKET_COUNT;
}

// ============================================================================
// ModeManager::SetOnlineRaceCars
// ============================================================================
// A flat roster copy: the network round manager's StartNetworkGameEvent already carries every
// car's identity, livery, network id and player rating, and this publishes them into the event's
// GameModeParams. Fixed eight-slot copy -- the console unrolls the whole loop and copies all
// KI_MAX_RACE_CARS entries regardless of miNumRaceCars; only the head count is clamped.
//
// maePlayerTeam is deliberately NOT copied here: each Online*Mode::SetupGameModeParams sets the
// team runs by name before calling this, and SetupOnlineStartingGrid below reads them back.
//
// The four guards are the console's own, in its order, and the third/fourth are two separate tests
// of the same pointer -- the mode-is-online check, then the downcast the body's caller-facing name
// belongs to.
// ============================================================================
void ModeManager::SetOnlineRaceCars(GameModeParams* lpGameModeParams,
                                    const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent) const
{
    // The console folds GetProgressionManager() into a fixed offset off mpGameStateModule and tests
    // the folded address against null, i.e. this is a plain "the module has a progression manager"
    // guard; the answer is never used.
    CGS_ASSERT(mpGameStateModule->GetProgressionManager() != nullptr,
               "mpGameStateModule->GetProgressionManager()");

    CGS_ASSERT(lpStartNetworkGameEvent->miNumRaceCars > 0,
               "lpStartNetworkGameEvent->miNumRaceCars > 0");

    // The RIVAL count, not the car count: the local player is not one of the network players the
    // roster loops over. Stored as a byte.
    lpGameModeParams->miNumNetworkPlayers =
        static_cast<s8>(lpStartNetworkGameEvent->miNumRaceCars - 1);

    lpGameModeParams->mLocalNetworkPlayerID = lpStartNetworkGameEvent->mLocalNetworkPlayerID;

    CGS_ASSERT(mpCurrentGameMode != nullptr, "mpCurrentGameMode");
    CGS_ASSERT(mpCurrentGameMode->IsOnline(), "mpCurrentGameMode->IsOnline()");

    const OnlineGameMode* lpCurrentOnlineGameMode =
        static_cast<const OnlineGameMode*>(mpCurrentGameMode);
    CGS_ASSERT(lpCurrentOnlineGameMode != nullptr, "lpCurrentOnlineGameMode");

    for (s32 liRaceCar = 0; liRaceCar < GameStateModuleIO::KI_MAX_RACE_CARS; ++liRaceCar)
    {
        // Per-slot copy order is the console's: id, model, colour, finish, rating.
        lpGameModeParams->maNetworkPlayerID[liRaceCar] =
            lpStartNetworkGameEvent->maNetworkPlayerID[liRaceCar];
        lpGameModeParams->maModelIds[liRaceCar] =
            lpStartNetworkGameEvent->maCarIds[liRaceCar];
        lpGameModeParams->mau16CarColourIndex[liRaceCar] =
            lpStartNetworkGameEvent->mau16CarColourIndex[liRaceCar];
        lpGameModeParams->mau16CarPaintFinishIndex[liRaceCar] =
            lpStartNetworkGameEvent->mau16CarPaintFinishIndex[liRaceCar];

        // The event's per-player rating lands in the mode's per-car overtaking difficulty -- the
        // only f32[8] on either record, and the two slots the console copies between are the ones
        // GameModeParams::Construct seeds with the same default in the same eight-step loop.
        lpGameModeParams->mfOvertakingDifficulty[liRaceCar] =
            lpStartNetworkGameEvent->mafPlayerData[liRaceCar];
    }
}

// ============================================================================
// ModeManager::SetupOnlineStartingGrid
// ============================================================================
// Seats an online field on the start grid of the event's traffic-light junction, TEAM BLOCK BY
// TEAM BLOCK rather than in raw car order (which is what makes this a different function from
// SetStartingGrid rather than a fork inside it).
//
//   1. Bucket every car by its GameModeParams team, EXCEPT cars whose scoring record says the
//      player has disconnected -- those go to the tail bucket and therefore to the back of the
//      grid, whatever team they were on.
//   2. Order each team bucket: by cumulative points when the caller wants the current standings
//      preserved, otherwise a shuffle off the caller's Random. The tail bucket is left alone.
//   3. Flatten the buckets into one grid order, HIGHEST team index first, tail bucket last.
//   4. Walk the cars in car-index order, look each one's position up in that grid order, and
//      publish the junction's start block entry for that position as the car's start location.
//
// Step 4 is the inversion that matters: the flattened array maps GRID SLOT -> CAR, and the search
// turns it back into CAR -> GRID SLOT, because GameModeParams::maStartLocations is indexed by car.
// ============================================================================
void ModeManager::SetupOnlineStartingGrid(GameModeParams* lpGameModeParams, s32 liCarCount,
                                          CgsNumeric::Random* lpRandom, bool lbUseCurrentStandings) const
{
    Array<GridPositionAndScoreData, 8> laaiTeamMembers[KI_PLAYER_TEAM_BUCKET_COUNT + 1];
    for (s32 liBucket = 0; liBucket <= KI_DISCONNECTED_BUCKET; ++liBucket)
    {
        laaiTeamMembers[liBucket].Construct();
    }

    // The traffic resource read is the TriggerQueryManager's, NOT ModeManager::GetTrafficData() --
    // the console dereferences the resolved pointer unguarded here, and GetTrafficData() carries an
    // "lpTrafficData" assert that this function never fires.
    const BrnTraffic::TrafficData* lpTrafficData = mpTriggerQueryManager->GetTrafficData();

    // A LightTriggerId packs { hull index = bits 8..23, light-trigger index = bits 0..7 }. This
    // resolution is the raw one -- straight into the hull's start-block table, no validity or
    // bounds assert (unlike TrafficData::GetStartDataForTrafficLight, which the offline grid uses).
    const LightTriggerId lTriggerId = lpGameModeParams->mTrafficLightTriggerId;
    const BrnTraffic::Hull* lpHull = lpTrafficData->GetHull((lTriggerId >> 8) & 0xFFFFu);
    const BrnTraffic::LightTriggerStartData* lpStartData =
        &lpHull->mpaLightTriggerStartData[lTriggerId & 0xFFu];

    // ---- 1. bucket -------------------------------------------------------------------------
    for (s32 liCarIndex = 0; liCarIndex < liCarCount; ++liCarIndex)
    {
        const CarData* lpCarData = mScoringSystem.GetCarDataFromPlayerScoringIndex(
            static_cast<GameStateModuleIO::EPlayerScoringIndex>(liCarIndex));

        GridPositionAndScoreData lPosAndScore;
        lPosAndScore.lpCarData      = lpCarData;
        lPosAndScore.liGridPosition = liCarIndex;

        if (lpCarData != nullptr && lpCarData->GetScoreData()->GetDisconnected())
        {
            laaiTeamMembers[KI_DISCONNECTED_BUCKET].Append(lPosAndScore);
        }
        else
        {
            const s32 liPlayerTeam = static_cast<s32>(lpGameModeParams->maePlayerTeam[liCarIndex]);
            laaiTeamMembers[liPlayerTeam].Append(lPosAndScore);
        }
    }

    // ---- 2. order each team bucket (the tail bucket is NOT ordered) -------------------------
    // The fork is around the loop, not inside it, which is how the console emits it.
    if (lbUseCurrentStandings)
    {
        for (s32 liTeam = 0; liTeam < KI_PLAYER_TEAM_BUCKET_COUNT; ++liTeam)
        {
            // A bucket of 0 or 1 is skipped outright -- BubbleSort's own entry would assert on an
            // empty one.
            if (laaiTeamMembers[liTeam].GetLength() > 1)
            {
                CgsAlgorithms::BubbleSort(laaiTeamMembers[liTeam]);
            }
        }
    }
    else
    {
        for (s32 liTeam = 0; liTeam < KI_PLAYER_TEAM_BUCKET_COUNT; ++liTeam)
        {
            CgsAlgorithms::Shuffle<GridPositionAndScoreData>(laaiTeamMembers[liTeam], *lpRandom);
        }
    }

    // ---- 3. flatten, highest team index first, disconnected players last --------------------
    Array<s32, 8> laiRaceCarOrder;
    laiRaceCarOrder.Construct();

    for (s32 liTeam = KI_PLAYER_TEAM_BUCKET_COUNT - 1; liTeam >= 0; --liTeam)
    {
        for (s32 liMember = 0;
             liMember < static_cast<s32>(laaiTeamMembers[liTeam].GetLength());
             ++liMember)
        {
            laiRaceCarOrder.Append(
                laaiTeamMembers[liTeam][static_cast<u32>(liMember)].liGridPosition);
        }
    }

    for (s32 liMember = 0;
         liMember < static_cast<s32>(laaiTeamMembers[KI_DISCONNECTED_BUCKET].GetLength());
         ++liMember)
    {
        laiRaceCarOrder.Append(
            laaiTeamMembers[KI_DISCONNECTED_BUCKET][static_cast<u32>(liMember)].liGridPosition);
    }

    // ---- 4. publish one start location per car ---------------------------------------------
    for (s32 liCarIndex = 0;
         liCarIndex < static_cast<s32>(laiRaceCarOrder.GetLength());
         ++liCarIndex)
    {
        const u32 luStartPositionIndex =
            static_cast<u32>(laiRaceCarOrder.FindFirstInstanceOf(liCarIndex));

        // Direction first: that is the console's order, and the direction is what the start
        // location's own IsNormal guard (inside AddStartLocation) tests.
        const Vector3 lDirection = lpStartData->GetStartDirection(luStartPositionIndex);
        const Vector3 lPosition  = lpStartData->GetStartPosition(luStartPositionIndex);

        lpGameModeParams->AddStartLocation(lPosition, lDirection);
    }
}

}
