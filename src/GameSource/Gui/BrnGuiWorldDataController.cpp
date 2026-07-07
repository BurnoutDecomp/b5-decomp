#include "GameSource/Gui/BrnGuiWorldDataController.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT + BeginAssert/FireAssert/EndAssert
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"    // ResourcePtr<T>::operator->
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (not-found diagnostics)
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"            // BrnWorld::GlobalColourPalette / PlayerCarColourPalette
#include "SharedClasses/Progression/BrnProgressionData.h"             // BrnProgression::ProgressionData
#include "SharedClasses/Progression/BrnRaceEventData.h"               // BrnProgression::EventJunction / RaceEventData
#include "SharedClasses/Trigger/BrnTriggerData.h"                     // BrnTrigger::TriggerData
#include "SharedClasses/Trigger/BrnLandmark.h"                        // BrnTrigger::Landmark (GetId / GetRegionIndex)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                   // BrnTrigger::GenericRegion (GetId / GetBoxRegion)
#include "SharedClasses/Trigger/BrnRegion.h"                          // BrnTrigger::BoxRegion (region copy)
#include "GameSource/GameState/BrnGameStateTypes.h"                   // BrnGameState::LandmarkIndex (operator s32)

// BrnGui::WorldDataController -- readiness accessors reconstructed from BURNOUT_X360_ARTIST.XEX.
// Each asserts the controller has reached the ready state (the X360 compares meState against 11 /
// WFPLAYERCARCOLOURS; the baked rodata text names READY, reproduced verbatim) then forwards to a
// resource pointer via ResourcePtr<T>::operator-> const. Only the four verified-PASS accessors are
// homed here; GetRequiredWinsInRank + the Prepare acquire state machine land in later waves.

namespace BrnGui
{

// X360 0x8248E6D8. Total landmark count in the loaded trigger data (TriggerData::miLandmarkCount @+0x34).
s32 WorldDataController::GetTotalNumberOfLandmarks() const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");
    return mpTriggerData->GetNumLandmarks();
}

// X360 0x824286E0. Number of online landmarks currently available (TriggerData::miOnlineLandmarkCount
// @+0x38). The X360 compares meState against 11 (E_..._WFPLAYERCARCOLOURS); the baked assert text
// names READY -- reproduced as-is.
s32 WorldDataController::GetTotalNumberOfOnlineLandmarks() const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");
    return mpTriggerData->GetOnlineLandmarkCount();
}

// X360 0x824BDA40. Returns the lType'th car-colour palette entry from the loaded global
// colour-palette resource. lType indexes maPalettes[4] (12-byte PlayerCarColourPalette stride).
const BrnWorld::PlayerCarColourPalette*
WorldDataController::GetColourPaletteFromType(BrnWorld::EPalettesTypes lType) const
{
    const BrnWorld::GlobalColourPalette* const lpPalette = mpPlayerCarColours.operator->();
    CGS_ASSERT(lType < BrnWorld::eNumPalettes, "lType < eNumPalettes");
    return &lpPalette->maPalettes[lType];
}

// X360 0x82428818. Accessor for the loaded progression resource. Asserts the controller has
// reached the ready state, then returns the resource pointer (ResourcePtr operator-> const).
const BrnProgression::ProgressionData* WorldDataController::GetProgressionData() const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");
    return mpProgressionData.operator->();
}

// X360 0x82501270. Linear scan of the loaded trigger data's landmark table for the landmark whose
// region index equals lLandmarkIndex, returning it. On an empty table or a miss it builds a
// diagnostic into a local assert buffer and fires (the X360 streams into the global assert buffer;
// matching the committed BrnTriggerData.cpp precedent, we build into a stack buffer instead --
// behaviourally identical), then returns NULL.
const BrnTrigger::Landmark*
WorldDataController::GetLandmarkInfoFromIndex(BrnGameState::LandmarkIndex lLandmarkIndex) const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");
    CGS_ASSERT(static_cast<s32>(lLandmarkIndex) >= 0, "0 <= lLandmarkIndex");

    for (s32 liIndex = 0; liIndex < mpTriggerData->GetLandmarkCount(); ++liIndex)
    {
        if (mpTriggerData->GetLandmark(liIndex)->GetRegionIndex() == static_cast<s32>(lLandmarkIndex))
            return mpTriggerData->GetLandmark(liIndex);
    }

    // Not found: build the diagnostic into a local assert buffer and fire.
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Unable to find landmark with index: " << static_cast<s32>(lLandmarkIndex);
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameSource\\Gui/BrnGuiWorldDataController.cpp",
            414);
        CgsDev::Assert::EndAssert();
    }

    return NULL;
}

// X360 0x82501480. Linear scan of the loaded trigger data's landmark table for the landmark whose
// id equals lLandmarkID, returning it. On an empty table or a miss it builds a diagnostic and fires
// (see GetLandmarkInfoFromIndex on the stack-buffer substitution), then returns NULL.
const BrnTrigger::Landmark*
WorldDataController::GetLandmarkInfoFromID(CgsID lLandmarkID) const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");

    for (s32 liIndex = 0; liIndex < mpTriggerData->GetLandmarkCount(); ++liIndex)
    {
        if (mpTriggerData->GetLandmark(liIndex)->GetId() == lLandmarkID)
            return mpTriggerData->GetLandmark(liIndex);
    }

    // Not found: build the diagnostic into a local assert buffer and fire.
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Unable to find landmark with id: " << lLandmarkID;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameSource\\Gui/BrnGuiWorldDataController.cpp",
            445);
        CgsDev::Assert::EndAssert();
    }

    return NULL;
}

// X360 0x82501698. Returns the OFFLINE race-event record whose owning junction's id equals
// luEventId. Walks the progression event-junction table (the X360 inlined the junction lookup); on
// no match returns NULL. GetOfflineEvent() == the junction's offline event pointer (+0x04).
const BrnProgression::RaceEventData*
WorldDataController::GetEventInfoFromEventId(u32 luEventId) const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");

    const u32 luJunctionCount = mpProgressionData->GetEventJunctionCount();
    for (u32 luIndex = 0; luIndex < luJunctionCount; ++luIndex)
    {
        const BrnProgression::EventJunction* const lpJunction = mpProgressionData->GetEventJunction(luIndex);
        if (lpJunction->GetID() == luEventId)
            return lpJunction->GetOfflineEvent();
    }

    return NULL;
}

// X360 0x82501740. Returns the ONLINE race-event record whose owning junction's id equals
// luEventId. Same junction walk as GetEventInfoFromEventId, returning GetOnlineEvent() (+0x08).
const BrnProgression::RaceEventData*
WorldDataController::GetOnlineEventInfoFromEventId(u32 luEventId) const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");

    const u32 luJunctionCount = mpProgressionData->GetEventJunctionCount();
    for (u32 luIndex = 0; luIndex < luJunctionCount; ++luIndex)
    {
        const BrnProgression::EventJunction* const lpJunction = mpProgressionData->GetEventJunction(luIndex);
        if (lpJunction->GetID() == luEventId)
            return lpJunction->GetOnlineEvent();
    }

    return NULL;
}

// X360 0x82501AC0. Finds the generic region whose id equals lTriggerID and copies its box region
// into *lpRegion. Fires "Can't find any triggers in the data" on an empty table and "requested
// trigger wasn't found" when the scan exhausts without a match. NOTE: unlike the landmark/event
// accessors this one does NOT gate on the ready meState (the X360 asm has no state check here).
void
WorldDataController::GetTriggerVolumeRegion(CgsID lTriggerID, BrnTrigger::BoxRegion* lpRegion) const
{
    s32 liRegionIndex = 0;
    if (mpTriggerData->GetGenericRegionCount() > 0)
    {
        for (;;)
        {
            const BrnTrigger::GenericRegion* const lpGenericRegion =
                mpTriggerData->GetGenericRegion(liRegionIndex);
            if (lpGenericRegion->GetId() == lTriggerID)
            {
                *lpRegion = *lpGenericRegion->GetBoxRegion();
                break;
            }
            ++liRegionIndex;
            if (liRegionIndex >= mpTriggerData->GetGenericRegionCount())
                break;
        }
    }
    else
    {
        CGS_ASSERT(false, "Can't find any triggers in the data\n");
    }

    CGS_ASSERT(liRegionIndex < mpTriggerData->GetGenericRegionCount(), "requested trigger wasn't found");
}

// X360 0x824F3AF0. Accessor for the loaded vehicle-list resource (raw pointer member @X360 +0x464).
const BrnResource::VehicleList* WorldDataController::GetVehicleList() const
{
    return mpVehicleList;
}

// X360 0x824F3AF8. Accessor for the loaded freeburn-challenge-list resource (pointer member @X360 +0x4A8).
const ChallengeList* WorldDataController::GetFreeburnChallengeList() const
{
    return mpChallengeList;
}

}
