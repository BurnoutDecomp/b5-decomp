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

// Stunt classification id (DWARF BrnGameStateTypes.h:58). Identifies the kind of stunt the
// stunt-mode scorer is rating/scoring. The first 15 values (E_STUNT_TYPE_SPIN .. _COUNT) are the
// real per-stunt categories that index StuntModeScoring::mStuntTypeInfo[E_STUNT_TYPE_COUNT]; the
// values past _COUNT are rating/error sentinels and packed bit-mask aliases the scorer ORs into
// its muStuntTypesInProgress field. Values are X360-authoritative. This is the single owner
// (DWARF home BrnGameStateTypes.h); grow here, do not fork.
enum EStuntType : s32
{
    E_STUNT_TYPE_INVALID          = -1,
    E_STUNT_TYPE_SPIN             = 0,
    E_STUNT_TYPE_BARREL_ROLL      = 1,
    E_STUNT_TYPE_AIR              = 2,
    E_STUNT_TYPE_DRIFT            = 3,
    E_STUNT_TYPE_SUPER_JUMP       = 4,
    E_STUNT_TYPE_SUPER_SMASH      = 5,
    E_STUNT_TYPE_BILLBOARD        = 6,
    E_STUNT_TYPE_BURNOUT          = 7,
    E_STUNT_TYPE_BOOST            = 8,
    E_STUNT_TYPE_REVERSE_DRIVING  = 9,
    E_STUNT_TYPE_HANDBRAKE_TURN   = 10,
    E_STUNT_TYPE_POWER_PARK       = 11,
    E_STUNT_TYPE_CRASH_FINISH     = 12,
    E_STUNT_TYPE_PROP             = 13,
    E_STUNT_TYPE_REVERSE_TAKEOFF  = 14,
    E_STUNT_TYPE_COUNT            = 15,
    E_STUNT_TYPE_ERROR_REPETITION = 15,
    E_STUNT_TYPE_ERROR_CRASHED    = 16,
    E_STUNT_TYPE_RATING_GOOD      = 17,
    E_STUNT_TYPE_RATING_AWESOME   = 18,
    E_STUNT_TYPE_NON_REPEATABLE   = 776,
    E_STUNT_TYPE_MAJOR            = 7927,
    E_STUNT_TYPE_ERRORS           = 98304,
};

// Stunt-element classification id (DWARF BrnGameStateTypes.h:58). Identifies the kind of
// world stunt element (a jump/smash/billboard trigger) that the stunt sub-systems
// complete and the action queue reports. DealWithStunt (X360 0x8232CEB0) uses this as the
// WorldStuntAction discriminant -- it branches on JUMP(0)/SMASH(1)/BILLBOARD(2) and asserts
// "Unknown world stunt type." for any value >= COUNT(3). Values are X360-attested by that
// branch. This is the single owner (DWARF home BrnGameStateTypes.h); grow here, do not fork.
// NB: declared WITHOUT a fixed underlying type to stay compatible with the existing opaque
// `enum StuntElementType;` forward declarations in BrnStuntManager.h /
// BrnStuntManagerDebugComponent.h (a fixed-underlying-type definition would clash with those).
enum StuntElementType
{
    E_STUNT_ELEMENT_TYPE_JUMP      = 0,
    E_STUNT_ELEMENT_TYPE_SMASH     = 1,
    E_STUNT_ELEMENT_TYPE_BILLBOARD = 2,
    E_STUNT_ELEMENT_TYPE_COUNT     = 3,
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
