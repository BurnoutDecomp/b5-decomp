#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// CgsGeometric::PolygonSoupPoly -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   CgsGeometric::PolygonSoupPoly::GetEdgeCosineUncompressed @ 0x82839568
// ============================================================================

namespace CgsGeometric
{
    // ------------------------------------------------------------------------
    // GetEdgeCosineUncompressed @ 0x82839568
    //
    //   ; assert liEdge in [0,4)  (CgsPolygonSoupPoly.h:156)
    //   add   r11, r31, r30        ; r11 = this + liEdge
    //   li    r10, 8
    //   lbz   r11, 8(r11)          ; byte = mauCompressedEdgeCosines[liEdge]
    //   clrldi r11, r11, 59        ; shift = byte & 0x1F  (low 5 bits)
    //   sld   r3, r10, r11         ; lValue = (u64)8 << shift
    //   bl    __u64tod             ; (double)lValue
    //   frsp  f13, f1              ; round to f32
    //   lfs   f0, flt_820DF65C     ; 9.8696051f  (== pi^2; value read from .rdata)
    //   fdivs f13, f0, f13         ; 9.8696051f / value
    //   lfs   f0, flt_82001C98     ; 1.0f
    //   fsubs f1, f0, f13          ; 1.0f - (9.8696051f / value)
    //   blr                        ; returns the f32 result widened to double in f1
    //
    // The single byte at +0x08+liEdge is a compressed exponent: its low 5 bits
    // give a power-of-two scale applied to 8, and the cosine is recovered as
    // 1 - pi^2 / (8 << shift). All float arithmetic is single-precision (frsp/
    // fdivs/fsubs); the result is returned as a double.
    // ------------------------------------------------------------------------
    double PolygonSoupPoly::GetEdgeCosineUncompressed(u32 liEdge) const
    {
        CGS_ASSERT(liEdge < 4u, "liEdge >= 0 && liEdge < 4");

        const u32 luShift = mauCompressedEdgeCosines[liEdge] & 0x1Fu;
        const u64 luValue = static_cast<u64>(8) << luShift;

        const f32 lfValue = static_cast<f32>(luValue);           // u64tod + frsp
        const f32 lfCosine = 1.0f - (9.8696051f / lfValue);      // fdivs + fsubs

        return static_cast<double>(lfCosine);
    }
}
