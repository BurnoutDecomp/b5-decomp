#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"

// BrnTraffic::TrafficLightRuntimeState::Update @ 0x827515D8
//
// Advance one traffic-light's runtime record by lfTimeDelta. The X360 build:
//   f30 = lfTimeDelta ; f31 = 0.0f
//   if (lfTimeDelta <= 0.0f)                         fcmpu f30,0.0 ; bgt skip
//       assert("lfTimeDelta > 0.0f", ..., 66)         Begin/Fire/EndAssert
//   r11 = (u8)this[+4]                                lbz r11, 4(this)   ; phase
//   switch on phase (cmplwi r11,1 / beq / cmplwi r11,3):
//     phase 0 (E_STATE_IDLE)      -> nothing                          (blt end)
//     phase 1 (E_STATE_COUNTDOWN) -> countdown branch (loc_827516C4)  (beq)
//     phase 2 (E_STATE_HOLD)      -> nothing                          (blt end)
//     phase >=3                   -> assert("Unknown light state ...")(else)
//
//   countdown branch:
//     f0 = this->mfTimer - lfTimeDelta                lfs/fsubs/stfs 0(this)
//     if (f0 <= 0.0f) {                               fcmpu f0,0.0 ; bgt end
//        flags = this[+5]                             lbz r11, 5(this)
//        this->mfTimer = 0.0f                         stfs f31, 0(this)
//        this->muState = 0                            stb 0, 4(this)
//        this->muFlags = (flags & 0xF8) | 1           rlwimi r11,1,0,29,23 ; stb r11,5(this)
//     }
//
// rlwimi r11,1,0,29,23 inserts the source (1) into r11 everywhere except PPC bits
// 24..28 (the high 5 bits of the low byte, 0xF8) -- i.e. it preserves the top 5 bits
// of muFlags and forces the low 3 bits (0x07) to the value 1. Hence (flags & 0xF8) | 1.
//
// The "Unknown light state" branch streamed `state` into the assert buffer via StrStream
// in the X360 build; CGS_ASSERT collapses that to a static message per project policy.
// Baked d:\p4 path + lines 66/94 are dropped per policy (reproduced via CGS_ASSERT).

namespace BrnTraffic
{

void TrafficLightRuntimeState::Update(f32 lfTimeDelta)
{
    CGS_ASSERT(lfTimeDelta > 0.0f, "lfTimeDelta > 0.0f");

    if (muState == E_STATE_COUNTDOWN)
    {
        mfTimer = mfTimer - lfTimeDelta;
        if (mfTimer <= 0.0f)
        {
            mfTimer = 0.0f;
            muState = E_STATE_IDLE;
            muFlags = static_cast<u8>((muFlags & 0xF8) | 1);
        }
    }
    else if (muState >= E_STATE_COUNT)
    {
        CGS_ASSERT(false, "Unknown light state ");
    }
}

}
