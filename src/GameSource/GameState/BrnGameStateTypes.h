#pragma once

#include "types.hpp"

// Owning header for BrnGameState::LandmarkIndex.
//
// Reconstructed from the DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/
// GameState/BrnGameStateTypes.h and the _compile/BrnGameStateScoringAndModesUnity.cpp
// body dump), which is authoritative for the shape. The DWARF shows LandmarkIndex is a
// real class -- a thin wrapper over a signed 16-bit trigger-region index -- NOT the
// `typedef u32 LandmarkIndex` stub that previously sat in
// GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h. That stub has been
// removed; this header is now the single owner of the type.
//
// SCOPE (MINIMAL): the canonical Feb-2007 BrnGameStateTypes.h also declares several
// enums (EModulePreparedForInvite, StuntElementType, EShowtimeBehaviour, EStuntType,
// EOnlineAwardID, EActiveRoadRule), the `RoamingSections` CgsArray typedef, and the
// K_INVALID_LANDMARK / K_MULTIPLE_LANDMARKS sentinels. None of those are touched by
// BrnGameState::OfflineGameMode::SelectRandomDestinations, which only CONSTRUCTS a
// LandmarkIndex from a 32-bit region index (DWARF body: the int32 ctor is invoked from
// the value returned by BrnTrigger::TriggerRegion::GetRegionIndex()). So only the class
// itself is declared here. The integrator can grow this header with the enums/typedef
// when a TU that needs them is reconstructed -- this is the single owner, do not fork.

namespace BrnGameState
{

// X360-attested handle type (DWARF BrnGameStateTypes.h:57). A 2-byte signed wrapper over
// a trigger-region index. SelectRandomDestinations builds one of these from the s32 it
// gets out of TriggerRegion::GetRegionIndex() and stores it through its output array.
class LandmarkIndex
{
public:

    inline
    LandmarkIndex() {}

    // DWARF BrnGameStateTypes.h:67 -- explicit, takes the region index as a 32-bit int and
    // narrows it to the 16-bit store. This is the ctor OfflineGameMode invokes.
    explicit inline
    LandmarkIndex(s32 lTriggerRegionIndex);

    // DWARF BrnGameStateTypes.h:70 -- implicit conversion back to the 32-bit region index.
    inline operator s32() const;

private:

    // DWARF BrnGameStateTypes.h:74 -- the sole data member; sizeof(LandmarkIndex) == 2.
    s16 mTriggerRegionIndex;

};


inline
LandmarkIndex::LandmarkIndex(s32 lTriggerRegionIndex)
{
    mTriggerRegionIndex = static_cast<s16>(lTriggerRegionIndex);
}


inline
LandmarkIndex::operator s32() const
{
    return static_cast<s32>(mTriggerRegionIndex);
}

}
