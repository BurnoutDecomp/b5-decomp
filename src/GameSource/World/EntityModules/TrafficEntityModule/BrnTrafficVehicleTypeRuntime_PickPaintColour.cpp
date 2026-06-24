#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"

// BrnTraffic::VehicleTypeRuntime::PickPaintColourForVehicle @ 0x827049A8
//
// Returns this vehicle type's paint colour (16-byte Vector4) for one spawned car.
// Register map (the 16-byte return goes through the hidden sret pointer in r3):
//   r3  = sret (destination Vector4)           -> the by-value return slot
//   r4  = this (VehicleTypeRuntime*)            -> miNumPaintColours @ +0x5C,
//                                                  maPaintColourIndices @ +0x48
//   r5  = luSeed
//   r6  = liNumAvailableColours
//   r7  = lpaPaintColours (Vector4* palette, 16-byte stride)
//
// X360 body:
//   if (lpaPaintColours == 0)                   cmplwi r28,0 ; bne
//       assert("lpaPaintColours", ..., 134)
//   if (this->miNumPaintColours == 0)           lbz 0x5C(r31) ; cmplwi 0 ; bne
//       assert("miNumPaintColours != 0", ..., 135)
//   count = (s8)this->miNumPaintColours         lbz+extsb 0x5C(this)
//   twllei(count, 0)                            divide-by-non-positive trap (guarded by assert)
//   which = luSeed % count                      divwu/mullw/subf  (unsigned modulo)
//   idx   = (s8)this->maPaintColourIndices[which]   lbz+extsb 0x48(this + which)
//   max   = liNumAvailableColours - 1
//   idx   = min(idx, max)                        srawi/andc/and/or branchless clamp
//   *sret = lpaPaintColours[idx]                 slwi r11,idx,4 ; lvx128/stvx128 (16-byte copy)
//
// The branchless clamp (mask = (idx-max)>>31; res = (max & ~mask) | (mask & idx)) is the
// optimizer's form of `idx > max ? max : idx` == min(idx, max) -- reconstructed as the
// structured min. There is no lower clamp in the asm. The lvx128/stvx128 pair is a single
// 16-byte aligned Vector4 copy (Vector4 is alignas(16)); reconstructed as a value copy.
// Baked d:\p4 path + lines 134/135 dropped per policy (reproduced via CGS_ASSERT).

namespace BrnTraffic
{

Vector4 VehicleTypeRuntime::PickPaintColourForVehicle(u32 luSeed,
                                                      s32 liNumAvailableColours,
                                                      const Vector4* lpaPaintColours) const
{
    CGS_ASSERT(lpaPaintColours != nullptr, "lpaPaintColours");
    CGS_ASSERT(miNumPaintColours != 0, "miNumPaintColours != 0");

    const u32 luCount = static_cast<u32>(miNumPaintColours);   // (s8)->extsb, then unsigned
    const u32 luWhich = luSeed % luCount;

    s32 liIndex = maPaintColourIndices[luWhich];               // (s8)->extsb sign-extend
    const s32 liMaxIndex = liNumAvailableColours - 1;
    if (liIndex > liMaxIndex)
    {
        liIndex = liMaxIndex;                                  // min(idx, max)
    }

    return lpaPaintColours[liIndex];
}

}
