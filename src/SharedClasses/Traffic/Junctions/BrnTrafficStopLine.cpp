#include "SharedClasses/Traffic/Junctions/BrnTrafficStopLine.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

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
// On the XDK, the direct u16 cast uses fctidz and reloads just the low halfword.
// The diagnostic macro preserves the filename and line 117 for matching builds.

namespace BrnTraffic
{

u16 StopLine::ConvertToFixed(f32 lfFloat)
{
    CGS_ASSERT_AT(lfFloat < 256.0f, "lfFloat < 256.0f",
        "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\traffic\\Junctions/BrnTrafficStopLine.h", 117);

    return static_cast<u16>(lfFloat * 256.0f);
}

}
