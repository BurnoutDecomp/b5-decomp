// BurnoutConstants.cpp
// Definition/anchor home for GameSource/BurnoutConstants.h. The header is otherwise
// pure constants + inline enum operators; this translation unit gives the header a real
// .cpp home and ODR-uses the reconstructed post-increment operator (X360 @0x821F1DB8)
// plus a header constant so the TU is a genuine compilable unit (not a vacuous header).
//
// X360 source-of-truth: operator++(EActiveRaceCarIndex&, int) @0x821F1DB8.

#include "GameSource/BurnoutConstants.h"

namespace
{
    // ODR-use the post-increment operator and a header constant so this object file
    // carries the operator's emitted body. brnActiveRaceCarSlotCount equals
    // E_ACTIVE_RACE_CAR_INDEX_COUNT; brnWalkActiveRaceCarIndex advances and returns the
    // pre-advance value exactly as the X360 operator does.
    const s32 KI_ACTIVE_RACE_CAR_SLOT_COUNT = E_ACTIVE_RACE_CAR_INDEX_COUNT;

    EActiveRaceCarIndex WalkActiveRaceCarIndex(EActiveRaceCarIndex &leIndex)
    {
        return leIndex++;
    }
}

// Expose the anchored helpers through external linkage so the optimiser cannot discard
// the TU's emitted operator body. Not called by game code; this is the constants-header
// anchor required by the per-TU build.
extern const s32 g_brnActiveRaceCarSlotCount;
const s32 g_brnActiveRaceCarSlotCount = KI_ACTIVE_RACE_CAR_SLOT_COUNT;

EActiveRaceCarIndex BrnAnchorWalkActiveRaceCarIndex(EActiveRaceCarIndex &leIndex);
EActiveRaceCarIndex BrnAnchorWalkActiveRaceCarIndex(EActiveRaceCarIndex &leIndex)
{
    return WalkActiveRaceCarIndex(leIndex);
}
