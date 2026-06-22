#ifndef BRN_OPPONENT_DATA_H
#define BRN_OPPONENT_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

// =============================================================================
// BrnOpponentData.h  (OWNING HEADER for BrnProgression::CarOpponent / CarOpponentSet)
//
// DWARF home: SharedClasses/Progression/BrnOpponentData.h. A CarOpponentSet is the per-car,
// per-rank table of AI opponents the race setup pulls from. This is a MINIMAL OWNING SLICE:
// the full DWARF data layout (so ProgressionData::FindCarOpponentSet lands mPlayerCarId at
// byte +0x80, miRank at +0x88 and the 144-byte stride lines up by name) plus the GetRank /
// GetPlayerCarId accessors that lookup reads. The remaining attested API is declared-only;
// bodies land with the OpponentData TU. Single owner -- grow here, do not fork.
//
// LAYOUT is X360-faithful (DWARF BrnOpponentData.h). CarOpponent is a CgsID + s32; CgsID's
// 8-byte alignment pads it to a 16-byte stride, so maCarOpponents[8] occupies 0x00..0x7F and
// the trailing scalars follow (mPlayerCarId 0x80, miRank 0x88, miOpponentCount 0x8C ->
// sizeof 0x90 == 144).
// =============================================================================

namespace BrnProgression
{

struct CarOpponent
{
    CgsID GetCarId() const { return mCarId; }
    s32   GetPersonalityIndex() const { return miPersonalityIndex; }

    // ---- Remaining attested API (bodies in the OpponentData TU; declaration-only) ----
    void  Construct(CgsID lCarId, s32 liPersonalityIndex);
    void  FixDown();
    void  FixUp();

private:
    CgsID mCarId;               // 0x00 (DWARF BrnOpponentData.h:67)
    s32   miPersonalityIndex;   // 0x08 (DWARF :68)
};

struct CarOpponentSet
{
    // DWARF BrnOpponentData.h:31. Maximum opponents in a set.
    static const s32 KI_MAXIMUM_NUMBER_OF_CAR_OPPONENTS = 8;

    // X360 ProgressionData::FindCarOpponentSet reads these two fields.
    CgsID GetPlayerCarId() const { return mPlayerCarId; }
    s32   GetRank() const { return miRank; }

    // ---- Remaining attested API (bodies in the OpponentData TU; declaration-only) ----
    void               Construct(CgsID lPlayerCarId, s32 liRank, s32 liOpponentCount);
    void               SetCarOpponent(s32 liIndex, const CarOpponent* lpCarOpponent);
    const CarOpponent* GetCarOpponent(s32 liIndex) const;
    s32                GetOpponentCount() const;

private:
    CarOpponent maCarOpponents[KI_MAXIMUM_NUMBER_OF_CAR_OPPONENTS]; // 0x00 (DWARF :116)
    CgsID       mPlayerCarId;                                       // 0x80 (DWARF :117)
    s32         miRank;                                             // 0x88 (DWARF :118)
    s32         miOpponentCount;                                    // 0x8C (DWARF :119)
};

}

#endif // BRN_OPPONENT_DATA_H
