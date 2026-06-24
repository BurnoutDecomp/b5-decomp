#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleTrigger.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. cParticleTrigger::Update @0x82908808.
//
// Byte-offset pins attested by the Update asm (lbz/lwz/stw displacements): the three
// time stamps are single 32-bit words at +0x00/+0x04/+0x08 (in member-declaration order)
// and the running flag is a byte at +0x0C. With cTime == 4 bytes, the four members fall
// at exactly those offsets, so sizeof(cTime) is the load-bearing check here.
namespace
{
    static_assert(sizeof(cTime) == 4, "cTime (trigger stamp) is the 4-byte form");
    static_assert(sizeof(cParticleTrigger) == 0x10,
                  "cParticleTrigger: 3 x 4-byte stamps + bool @ +0x0C, padded to 0x10");
}

// cParticleTrigger::Update @ 0x82908808
//
//   arg2 (auFlags / r4) is used here purely as a boolean "is the effect running this
//   frame?" gate (the asm only branches on it != 0). arg3 (arTime / r5) is read as a
//   single 32-bit word (lwz 0(r5)).
//
//   if (running)                                   // beq cr6 on r4 == 0
//   {
//       if (!mbEnabled || arTime < mTimeStart)     // !mbEnabled OR (cmpw blt)
//           mTimeStart = arTime;                   // stw 4(r3)
//       mTime = arTime;                            // stw 0(r3)
//       mbEnabled = 1;                             // stb 0xC(r3)
//   }
//   else
//   {
//       if (mbEnabled)                             // cmplwi r11,0 ; beq
//           mTimeStop = arTime;                    // stw 8(r3)
//       mbEnabled = 0;                             // stb 0xC(r3)
//   }
//
// The running path captures the earliest start time seen while running and tracks the
// latest current time; the stopped path latches the stop time on the running->stopped
// edge. mbEnabled mirrors the running flag.
void cParticleTrigger::Update(u32 auFlags, const cTime& arTime)
{
    if (auFlags != 0)
    {
        if (!mbEnabled || arTime.mi32Ticks < mTimeStart.mi32Ticks)
            mTimeStart = arTime;
        mTime     = arTime;
        mbEnabled = true;
    }
    else
    {
        if (mbEnabled)
            mTimeStop = arTime;
        mbEnabled = false;
    }
}
