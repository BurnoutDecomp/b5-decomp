#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNumeric::CreateFloatVector   @ 0x8289CE20
//   CgsNumeric::CreateIntegerVector @ 0x8289CDE0
//
// Two helpers that assemble a 128-bit VPU/VMX vector register from scalar arguments
// by spilling the scalars to a 16-byte stack slot and reloading it with lvx128.
// Hex-Rays modelled both as `void` (it lost the register arguments), but the
// disassembly shows the inputs:
//
//   CreateFloatVector:   stfs f1 to all four lanes, then lvx128 v1  -> splat(f1)
//   CreateIntegerVector: stw  r3,r4,r5,r6 to the four lanes         -> pack(r3..r6)
//
// Reconstructed as returning a 16-byte POD vector by value (the PC equivalent of the
// v1 result register). The float variant broadcasts its single argument across all
// four lanes; the integer variant packs its four arguments in order.

namespace CgsNumeric
{
    union VpuVector
    {
        f32 maFloats[4];
        s32 maInts[4];
    };

    VpuVector CreateFloatVector(f32 lfValue)
    {
        VpuVector lVec;
        lVec.maFloats[0] = lfValue;
        lVec.maFloats[1] = lfValue;
        lVec.maFloats[2] = lfValue;
        lVec.maFloats[3] = lfValue;
        return lVec;
    }

    VpuVector CreateIntegerVector(s32 liX, s32 liY, s32 liZ, s32 liW)
    {
        VpuVector lVec;
        lVec.maInts[0] = liX;
        lVec.maInts[1] = liY;
        lVec.maInts[2] = liZ;
        lVec.maInts[3] = liW;
        return lVec;
    }
}
