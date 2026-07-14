#include "SDKs/D3D/D3DTimingMarker.h"

#include <cstddef> // offsetof (uncalled _TimingMarkerAssertLayout)

// ===========================================================================
// D3D::TimingMarker -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// D3DTimingMarker.h for the object layout.
//
// Source-of-truth: X360 asm only (no DWARF, no reference source). The single
// member store and the PIX begin-op are reproduced faithfully in the asm's
// order with no added behaviour and no inverted branches.
// ===========================================================================

// --- External X360 platform symbols referenced below -----------------------
// PIX (Performance Investigator for Xbox) profiling op stream. Platform API.
extern "C" int PIXAddPixOpWithData(unsigned int uOp, int a2, int a3);

namespace D3D
{

// --- @ 0x82938630 ----------------------------------------------------------
// Store the marker value (stw r30, 0(this)) and unconditionally emit a PIX
// begin-op with the marker OR'd with 0x800000. Unlike CBlocker, the marker
// value is used directly as the PIX op payload -- there is no lookup table and
// no zero-type guard in the asm.
TimingMarker::TimingMarker(s32 iMarker)
{
    miMarker = iMarker;   // stw r30, 0(r31)

    // oris r3, r30, 0x80 ; li r4,0 ; li r5,0 ; bl PIXAddPixOpWithData
    PIXAddPixOpWithData(static_cast<u32>(iMarker) | 0x800000u, 0, 0);
}

// Layout facts (uncalled). A single 32-bit word, no pointers, so the X360
// offset is pinned exactly.
void _TimingMarkerAssertLayout()
{
    static_assert(offsetof(TimingMarker, miMarker) == 0,
                  "marker value is word[0]");
}

} // namespace D3D
