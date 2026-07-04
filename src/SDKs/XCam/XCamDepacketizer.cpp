#include "types.hpp"

// ===========================================================================
// XCAM depacketizer helpers, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU homes the self-contained sequence-number comparison the CDepacketizer
// uses to order frame/packet counters; the rest of the depacketizer (frame
// reassembly state) is DEFERRED to its own pass. `XCAM` is an X360 SDK boundary,
// so its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

// @ 0x829863F0 -- circular/wraparound comparison of two counters (uA, uB) within
// a modulus. Returns 0 if equal, else classifies the difference against
// +/-(modulus/2) so wraparound orders correctly. Used by the depacketizer to
// order frame/packet sequence numbers.
int RoundCompare(u32 uA, u32 uB, u32 uModulus)
{
    if (uA == uB)                                 // cmplw eq -> return 0
        return 0;

    if (uA < uB)                                  // unsigned less-than (fall-through of bge)
    {
        // subf r11,r3,r4 = uB-uA ; srwi r10,r5,1 = uModulus>>1 ; cmpw (SIGNED) ; bltlr
        if (static_cast<s32>(uB - uA) < static_cast<s32>(uModulus >> 1))
            return -1;
        return 1;
    }

    // uA > uB: neg(uModulus>>1) ; cmpw (SIGNED) ; blelr
    if (static_cast<s32>(uB - uA) <= -static_cast<s32>(uModulus >> 1))
        return -1;
    return 1;
}

} // namespace XCAM
