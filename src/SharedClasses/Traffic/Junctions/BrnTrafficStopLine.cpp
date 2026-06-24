#include "SharedClasses/Traffic/Junctions/BrnTrafficStopLine.h"

// BrnTraffic::StopLine::ConvertToFixed @ 0x8274F3C8
//
// Pack a lane-parameter float into an 8.8 unsigned fixed-point value (the on-disk
// packing the stop-line records use). The X360 build:
//   f31 = lfFloat                                  fmr f31, f1
//   f30 = 256.0f                                   lfs f30, flt_820ADBFC
//   if (lfFloat >= 256.0f)                          fcmpu cr6,f31,f30 ; blt skip
//       assert("lfFloat < 256.0f", ..., 117)        Begin/Fire/EndAssert
//   f0 = lfFloat * 256.0f                           fmuls f0, f31, f30
//   (s64)f0  (round toward zero)                    fctidz f0, f0   ; stfd to spill
//   return low 16 bits of that int64                lhz r3, spill+6 (big-endian low half)
//
// The fctidz (convert-to-integer, round toward zero) matches a C cast of the product
// to s64; the `lhz ...+6` keeps only the low halfword, so the return is u16. The baked
// d:\p4 path + line 117 are dropped per project policy (reproduced via CGS_ASSERT).

namespace BrnTraffic
{

u16 StopLine::ConvertToFixed(f32 lfFloat)
{
    CGS_ASSERT(lfFloat < 256.0f, "lfFloat < 256.0f");

    const s64 liFixed = static_cast<s64>(lfFloat * 256.0f);
    return static_cast<u16>(liFixed);
}

}
