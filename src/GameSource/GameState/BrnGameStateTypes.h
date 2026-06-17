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

// Per-player end-of-event online award id (DWARF BrnGameStateTypes.h:86). The award table on
// BaseOnlineModeScoring (maOnlineAwards[8]) stores one of these per slot; the online-mode scorers
// reset every slot to E_ONLINE_AWARD_INVALID in ClearData. Values are X360-authoritative (the
// award priority/rating tables in BrnBaseOnlineModeScoring.cpp index by these). This is the single
// owner (DWARF home BrnGameStateTypes.h); grow here, do not fork.
enum EOnlineAwardID : s32
{
    E_ONLINE_AWARD_INVALID                     = -1,
    E_ONLINE_AWARD_START                       = 0,
    E_ONLINE_AWARD_RACE_WINNER                 = 0,
    E_ONLINE_AWARD_TAKEDOWNS_FOR               = 1,
    E_ONLINE_AWARD_TAKEDOWNS_AGAINST           = 2,
    E_ONLINE_AWARD_MOST_CRASHES                = 3,
    E_ONLINE_AWARD_FASTEST_LAP                 = 4,
    E_ONLINE_AWARD_SHORTEST_DISTANCE           = 5,
    E_ONLINE_AWARD_LONGEST_DISTANCE            = 6,
    E_ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE = 7,
    E_ONLINE_AWARD_LONGEST_TIME_IN_LAST_PLACE  = 8,
    E_ONLINE_AWARD_TIME_SPENT_BOOSTING         = 9,
    E_ONLINE_AWARD_LONGEST_DRIFT               = 10,
    E_ONLINE_AWARD_COUNT                       = 11,
};

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
