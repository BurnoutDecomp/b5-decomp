#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"
// Brings in the real Vector3 typedef (rw::math::vpu::Vector3, via BrnCommonTypes.h), used in
// SelectRandomDestinations' signature, plus the GameMode base.
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"
// The real BrnGameState::LandmarkIndex wrapper class (the SelectRandomDestinations output type).
// INTEGRATION NOTE (consolidated): the worker's original standalone OfflineGameMode.h
// forward-declared `struct Vector3;` and `struct LandmarkIndex;`. The real Vector3 is the
// rw::math::vpu typedef pulled in transitively above; LandmarkIndex is a real wrapper CLASS
// owned by BrnGameStateTypes.h (it used to be a `typedef u32 LandmarkIndex` stub in
// BrnGameModeParams.h — that stub is now removed, and this is the single owner). Include the
// owning header so the class is complete at this declaration site.
#include "GameSource/GameState/BrnGameStateTypes.h"

namespace BrnTrigger
{
// Forward declarations: SelectRandomDestinations takes a const TriggerData* and walks
// its Landmark array by pointer only, so the full layouts are not needed at this
// declaration site. Their owning headers (SharedClasses/Trigger/BrnTriggerData.h,
// BrnLandmark.h) are #included by the .cpp where the bodies dereference them.
struct TriggerData;
}

namespace BrnGameState
{
// Owning header for the OfflineGameMode base. Reconstructed from the DecFIGS DWARF
// (BrnOfflineGameMode.h): derives from GameMode and adds the two debug data members
// below, plus the two X360-attested methods (Construct override @0x8232FE58 and the
// protected SelectRandomDestinations helper @0x82321E38).
//
// GATING (DWARF supplies names; the X360 ledger decides what exists): the DWARF also
// lists a virtual `GetFrameRateType() const`, but it is ABSENT from the X360 ledger
// (progress/identity.json attests only Construct and SelectRandomDestinations for this
// class). It is therefore PS3-only drift and is deliberately left out -- exactly as the
// sibling OnlineGameMode header omits its PS3-only GetFrameRateType. The PS3-only
// EAddingRivalsState enum and the file-scope debug-loop members it drives are likewise
// out of scope for the X360 spine and are not pulled in.
class OfflineGameMode : public GameMode
{
public:
    // X360 0x8232FE58. Forwards to the GameMode base, then resets this mode's
    // construction flag and the debug "always race to one landmark" overrides.
    virtual void Construct(ModeManager* lpModeManager);

protected:
    // X360 0x82321E38. Scans the track's landmark list and gathers every landmark whose
    // straight-line distance from lv3Origin falls in [lfMinDistance, lfMaxDistance] and
    // whose world height sits above KF_HACK_MIN_LANDMARK_HEIGHT; when lbApplyDirectionFilter
    // is set, landmarks lying behind lv3Direction (negative dot) are also rejected. The two
    // parallel output arrays receive the landmark region indices and their AI-section
    // indices, and the count of accepted landmarks is returned. Called by the offline modes'
    // Start() (e.g. FaceOffMode::Start) to pick race destinations.
    u32 SelectRandomDestinations(const BrnTrigger::TriggerData* lpTriggerData,
                                 Vector3 lv3Origin,
                                 Vector3 lv3Direction,
                                 f32 lfMinDistance,
                                 f32 lfMaxDistance,
                                 LandmarkIndex* lpaLandmarkIndicesOut,
                                 u16* lpaAISectionIndicesOut,
                                 bool lbApplyDirectionFilter);

private:
    bool mbDebugAlwaysRaceToSingleLocation;
    s32  miDebugDesignIndexOfLandmarkToAlwaysRaceTo;
};
}
