// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_03.cpp
// ============================================================================
// Wave-C partfile for BrnGameState::ChallengeManager -- group G4 (keystone rows 10/12/13).
// Store-for-store reconstruction of three X360 ARTIST methods over the keystone-frozen NAMED
// members (layout evidence lives in BrnChallengeManager.h):
//
//   OutputFreeburnChallengeEveryPlayerStatusEvent @ 0x82348080 (DWARF :260)
//   CheckCurrentCar                               @ 0x823336E8 (DWARF :480)
//   IsSkillScoreCurrentlySuccessful               @ 0x823167A0 (DWARF :671, X360 7-param shape)
//
// SOURCE-OF-TRUTH: the X360 asm is authoritative for every store/branch/early-out. No header
// is edited by this file; every foreign field is reached through its committed accessor.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"

#include "GameSource/GameState/BrnGameActions.h"                 // GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_EVERY_PLAYER_COMPLETION_STATUS
#include "GameSource/GameState/BrnGameStateSharedIO.h"           // GameStateModuleIO::{FburnChallengeEveryPlayerStatusData, CompletedFburnChallenges}
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // TGameActionQueue::AddEvent, CgsModule::Event
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"       // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

#include "SharedClasses/DataLists/ChallengeListEntry.h"          // BrnResource::{ChallengeListEntry, ChallengeListEntryAction}
#include "SharedClasses/DataLists/VehicleList.h"                 // BrnResource::VehicleList::GetVehicleIndex / GetVehicleData
#include "SharedClasses/DataLists/VehicleListEntry.h"            // BrnResource::VehicleListEntry::GetCarType
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

#include <cfloat>                                                // FLT_MAX (flt_82020AFC)

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// OutputFreeburnChallengeEveryPlayerStatusEvent -- X360 0x82348080.
// ----------------------------------------------------------------------------
// Build the aggregated "every player" freeburn-challenge completion block on the stack and
// post it to the game-action queue:
//   * Construct() clears all seven remote slots + the local block.
//   * The X360 `memcpy(event+0x738, this+0xD00, 0x100)` IS the frozen inline member copy
//     FburnChallengeEveryPlayerStatusData::AddLocalPlayerCompletionStatus (the trailing
//     mLocalChallengeCompletionData field).
//   * Every occupied maChallengeCompletionData slot (mNetworkPlayerID != -1) is added.
// The manager's FastBitArray<2000> stores and the SharedIO CompletedFburnChallenges block are
// the identical 256-byte layout; the single documented reinterpret_cast bridges them
// (keystone pitfall F3 / wave-B pitfall 8).
void ChallengeManager::OutputFreeburnChallengeEveryPlayerStatusEvent(TGameActionQueue* lpActionQueue)
{
    GameStateModuleIO::FburnChallengeEveryPlayerStatusData lStatusData;
    lStatusData.Construct();

    lStatusData.AddLocalPlayerCompletionStatus(
        reinterpret_cast<const GameStateModuleIO::CompletedFburnChallenges*>(&mLocalChallengeCompletionData));

    // X360: r31 = this + 0x6C8 (== &maChallengeCompletionData[0].mNetworkPlayerID), 7 iterations,
    // stride 0x108; the status block address passed is r31 - 0x100 (the slot's bit store).
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        if (maChallengeCompletionData[liSlot].mNetworkPlayerID != -1)
        {
            lStatusData.AddCompletionStatus(
                reinterpret_cast<const GameStateModuleIO::CompletedFburnChallenges*>(
                    &maChallengeCompletionData[liSlot].mCompletedChallenges),
                maChallengeCompletionData[liSlot].mNetworkPlayerID);
        }
    }

    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue");   // BrnChallengeManager.cpp:5113

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStatusData),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_EVERY_PLAYER_COMPLETION_STATUS,
                            sizeof(lStatusData));   // X360 li r5,0x9D; li r6,0x838 (2104)
}

// ----------------------------------------------------------------------------
// CheckCurrentCar -- X360 0x823336E8.
// ----------------------------------------------------------------------------
// Does the player's current car satisfy the challenge's car restriction?
//   * A challenge that names an exact car id (GetCarID() != 0) only passes when the player's
//     active car model id matches it (full 64-bit CgsID compare -- X360 `cmpld`).
//   * Otherwise an unrestricted challenge (E_CAR_TYPE_NONE) always passes.
//   * Otherwise the player's car is resolved through the vehicle list and its boost-class byte
//     is mapped onto the challenge's ECarRestrictionType.
// The "Player car index hasn't been set" assert (BrnRaceCarEntityModuleOutputInterface.h:980)
// the X360 emits in the first branch is the interface's own inlined GetPlayerActiveRaceCarIndex
// guard -- it comes from the committed accessor, so it is not duplicated here.
bool ChallengeManager::CheckCurrentCar(
    const BrnResource::ChallengeListEntry* lpChallenge,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface)
{
    if (lpChallenge->GetCarID() != 0)   // X360 ld 0xC8(challenge); cmpldi 0
    {
        return lpActiveRaceCarOutputInterface->GetCarModelId(
                   lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex()) == lpChallenge->GetCarID();
    }

    if (lpChallenge->GetCarType() == BrnResource::ChallengeListEntry::E_CAR_TYPE_NONE)   // X360 lbz 0xD0; cmplwi 0
    {
        return true;
    }

    CGS_ASSERT(mpVehicleList != 0, "mpVehicleList != NULL");   // BrnChallengeManager.cpp:2678

    const s32 liModelIndex = mpVehicleList->GetVehicleIndex(
        lpActiveRaceCarOutputInterface->GetCarModelId(
            lpActiveRaceCarOutputInterface->GetPlayerActiveRaceCarIndex()));
    CGS_ASSERT(liModelIndex >= 0, "liModelIndex >= 0");   // :2681

    const BrnResource::VehicleListEntry* lpListEntry = mpVehicleList->GetVehicleData(liModelIndex);
    CGS_ASSERT(lpListEntry != 0, "lpListEntry != NULL");   // :2685

    // VehicleListEntry::GetCarType() is the raw +0xE8 boost-class byte
    // (0 == DANGER, 1 == AGGRESSION, 2 == STUNTS); the challenge restriction enum is 1-based.
    const u8 luVehicleCarType = lpListEntry->GetCarType();

    if (luVehicleCarType == 0 &&
        lpChallenge->GetCarType() == BrnResource::ChallengeListEntry::E_CAR_TYPE_DANGER)
    {
        return true;
    }
    if (luVehicleCarType == 1 &&
        lpChallenge->GetCarType() == BrnResource::ChallengeListEntry::E_CAR_TYPE_AGGRESSION)
    {
        return true;
    }
    if (luVehicleCarType == 2 &&
        lpChallenge->GetCarType() == BrnResource::ChallengeListEntry::E_CAR_TYPE_STUNT)
    {
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// IsSkillScoreCurrentlySuccessful -- X360 0x823167A0.
// ----------------------------------------------------------------------------
// Has lfScore reached the action's target for this frame? The X360 shape carries the two extra
// middle params (liActionIndex/leActiveRaceCarIndex) used only by the SEQUENCE branch below --
// see the register map documented at the declaration.
//
//   lbLargerScoresAreBetter:
//     score >= target                                                            -> success
//     CUMULATIVE/AVERAGE coop, score > 0, not arbitrating the cumulative total    -> success
//     coop type 6, score > 0 and the score CHANGED since this player's recorded
//       cumulative contribution                                                   -> success (logged)
//     otherwise                                                                   -> failure
//   !lbLargerScoresAreBetter (lower is better -- e.g. timed actions):
//     score <= target                                                            -> success
//     CUMULATIVE/AVERAGE coop, score below the FLT_MAX "unset" marker, not
//       arbitrating the cumulative total                                          -> success
//     otherwise                                                                   -> failure
bool ChallengeManager::IsSkillScoreCurrentlySuccessful(f32 lfScore,
                                                       const BrnResource::ChallengeListEntryAction* lpAction,
                                                       bool lbIsCumulativeArbitration,
                                                       s32 liTargetValueIndex,
                                                       s32 liActionIndex,
                                                       EActiveRaceCarIndex leActiveRaceCarIndex,
                                                       bool lbLargerScoresAreBetter)
{
    typedef BrnResource::ChallengeListEntryAction Action;

    if (lbLargerScoresAreBetter)
    {
        // X360 fcfid/frsp -- the target is an s32 converted to f32.
        const f32 lfTargetValue = static_cast<f32>(lpAction->GetTargetValue(liTargetValueIndex));
        if (lfScore >= lfTargetValue)
        {
            return true;
        }

        const Action::EChallengeCoopType leCoopType = lpAction->GetCoopType();   // X360 lbz +0x01

        if ((leCoopType == Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE ||
             leCoopType == Action::E_CHALLENGE_COOP_TYPE_AVERAGE) &&
            lfScore > 0.0f && !lbIsCumulativeArbitration)
        {
            return true;
        }

        // Coop type 6 == the X360 sequence regime (the committed enum's COUNT slot; same raw
        // value the wB_13 UpdateActionSuccess switch already handles).
        if (leCoopType == Action::E_CHALLENGE_COOP_TYPE_COUNT && lfScore > 0.0f &&
            maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex] != lfScore)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "OUTPUTTING SEQUENCE ACTION SUCCESS. Frame: "
                                           << miFramesSinceNetworkStart << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Active score: " << lfScore << " Score last frame: "
                                           << maafCumulativeContributions[leActiveRaceCarIndex][liActionIndex]
                                           << "\n";
            }
            return true;
        }

        return false;
    }

    const f32 lfTargetValue = static_cast<f32>(lpAction->GetTargetValue(liTargetValueIndex));
    if (lfScore <= lfTargetValue)
    {
        return true;
    }

    const Action::EChallengeCoopType leCoopType = lpAction->GetCoopType();   // X360 lbz +0x01
    if (leCoopType != Action::E_CHALLENGE_COOP_TYPE_CUMULATIVE &&
        leCoopType != Action::E_CHALLENGE_COOP_TYPE_AVERAGE)
    {
        return false;
    }
    if (lfScore >= FLT_MAX)   // X360 flt_82020AFC
    {
        return false;
    }
    if (lbIsCumulativeArbitration)
    {
        return false;
    }

    return true;
}

} // namespace BrnGameState
