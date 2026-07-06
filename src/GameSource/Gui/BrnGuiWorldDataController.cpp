#include "GameSource/Gui/BrnGuiWorldDataController.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"    // ResourcePtr<T>::operator->
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"            // BrnWorld::GlobalColourPalette / PlayerCarColourPalette
#include "SharedClasses/Progression/BrnProgressionData.h"             // BrnProgression::ProgressionData
#include "SharedClasses/Trigger/BrnTriggerData.h"                     // BrnTrigger::TriggerData

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

}
