#ifndef BRN_RACE_BALANCE_H
#define BRN_RACE_BALANCE_H

#include "types.hpp"

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
    // The "liPointIndex >= 0 && liPointIndex < KI_GRAPH_POINTS" assert lives in these bodies.
    f32  GetAheadTime(s32 liPointIndex) const;
    f32  GetBehindTime(s32 liPointIndex) const;
    void SetAheadTime(s32 liPointIndex, f32 lfTime);
    void SetBehindTime(s32 liPointIndex, f32 lfTime);

    // ---- Layout (X360-faithful, 68 bytes) --------------------------------------------------
    f32 mafAheadGraphPoints[KI_GRAPH_POINTS];   // 0x00 (DWARF BrnRaceBalance.h:87)
    f32 mafBehindGraphPoints[KI_GRAPH_POINTS];  // 0x20 (DWARF :88)
    f32 mfCatchUpCutOffRatio;                   // 0x40 (DWARF :89)
};
}

#endif // BRN_RACE_BALANCE_H
