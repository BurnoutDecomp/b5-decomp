#ifndef BRN_RACE_BALANCE_H
#define BRN_RACE_BALANCE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the four inline bounds guards)

// =============================================================================
// BrnRaceBalance.h  (OWNING HEADER for BrnProgression::OpponentBalanceData)
//
// The 68-byte AI-balance graph record that ProgressionData::mpaAIBalances points
// at and that ProgressionData::GetInterpolatedAIBalanceGraph returns by value.
// Full layout taken from the DecFIGS DWARF (references/DecFIGS/dwarfdump/
// SharedClasses/Progression/BrnRaceBalance.h:52-89). The X360-baked asserts at
// "..\\..\\..\\SharedClasses\\Progression/BrnRaceBalance.h" lines 144/152/161/170
// originate in the four graph-point accessors below; their bodies (with the
// "liPointIndex >= 0 && liPointIndex < KI_GRAPH_POINTS" assert) are a separate
// BrnRaceBalance TU, so they are declaration-only here.
//
// RaceBalanceData (Array<OpponentBalanceData,7>) is also attested in this DWARF
// but no function in this batch touches it; add it here when a caller needs it --
// single owner, grow in place.
// =============================================================================

namespace BrnProgression
{
struct OpponentBalanceData
{
    // DWARF BrnRaceBalance.h:85. Number of points on each catch-up graph.
    static const s32 KI_GRAPH_POINTS = 8;

    // ---- X360-attested methods (bodies in the BrnRaceBalance TU; declaration-only here) ----
    void Construct();
    void FixUp();
    void FixDown();

    // ---- the four graph-point accessors, DEFINED INLINE ------------------------------------
    // ⭐ CORRECTED 2026-08-01 (were declaration-only, "bodies are a separate BrnRaceBalance TU").
    // There IS no such TU: the X360 image contains NO standalone symbol for any of the four --
    // every call site open-codes the bounds assert plus the indexed load/store. Both known
    // callers prove it, each carrying all four baked lines itself:
    //   ProgressionData::GetInterpolatedAIBalanceGraph @0x82676820
    //   BrnAI::AIModule::SetupRaceBalancingManager     @0x8278A460
    // Same precedent as CarData::GetId / Profile::GetIsNewProfile. The assert string is
    // reproduced VERBATIM including the console's missing space after "liPointIndex".
    f32 GetAheadTime(s32 liPointIndex) const
    {
        CGS_ASSERT(liPointIndex >= 0 && liPointIndex < KI_GRAPH_POINTS,
                   "liPointIndex>= 0 && liPointIndex < KI_GRAPH_POINTS");   // BrnRaceBalance.h:144
        return mafAheadGraphPoints[liPointIndex];
    }
    f32 GetBehindTime(s32 liPointIndex) const
    {
        CGS_ASSERT(liPointIndex >= 0 && liPointIndex < KI_GRAPH_POINTS,
                   "liPointIndex>= 0 && liPointIndex < KI_GRAPH_POINTS");   // BrnRaceBalance.h:152
        return mafBehindGraphPoints[liPointIndex];
    }
    void SetAheadTime(s32 liPointIndex, f32 lfTime)
    {
        CGS_ASSERT(liPointIndex >= 0 && liPointIndex < KI_GRAPH_POINTS,
                   "liPointIndex>= 0 && liPointIndex < KI_GRAPH_POINTS");   // BrnRaceBalance.h:161
        mafAheadGraphPoints[liPointIndex] = lfTime;
    }
    void SetBehindTime(s32 liPointIndex, f32 lfTime)
    {
        CGS_ASSERT(liPointIndex >= 0 && liPointIndex < KI_GRAPH_POINTS,
                   "liPointIndex>= 0 && liPointIndex < KI_GRAPH_POINTS");   // BrnRaceBalance.h:170
        mafBehindGraphPoints[liPointIndex] = lfTime;
    }

    // ---- Layout (X360-faithful, 68 bytes) --------------------------------------------------
    f32 mafAheadGraphPoints[KI_GRAPH_POINTS];   // 0x00 (DWARF BrnRaceBalance.h:87)
    f32 mafBehindGraphPoints[KI_GRAPH_POINTS];  // 0x20 (DWARF :88)
    f32 mfCatchUpCutOffRatio;                   // 0x40 (DWARF :89)
};
}

#endif // BRN_RACE_BALANCE_H
