#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"

#include <cstddef>   // offsetof, for the never-called _AssertLayout pin below

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags (the T1 paint bring-up gate's one-shot log)

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

// ---------------------------------------------------------------------------
// Layout pins. NEVER CALLED -- a static member function so offsetof() reaches the private
// members. Only the four leading 16-byte lane members are pinned by absolute offset; they
// are pointer-free and 16-byte aligned, so console and host agree exactly and each one is
// attested:
//   +0x00 mBBoxOffset / +0x10 mBBoxHalfSize   Prepare @0x82761B10 (`stvx128 v0, r0, r16`
//                                             and `stvx128 v0, r16, r27` with r27 == 0x10;
//                                             TrafficEntityModule::Prepare stage 3 reads
//                                             element+0x10 for the scene box half-size)
//   +0x20 mCabPivot_TrailerPivot_BackAxle_FwdAxle   VehicleAxles::SetFromVehicleTransform
//                                             @0x82756738 splats lanes 3 (fwd axle) and 2
//                                             (back axle) from r5+0x20
//   +0x30 mMass_WheelRadius_Z_W               Prepare's `vrlimi128` into r16+0x30
// The trailing three (mAttribKey, maiPaintColours, miNumPaintColours) sit at X360
// +0x40/+0x48/+0x5C but are pinned by ORDER only, because this tree's Attribute::Key is a
// 4-byte alias while the console's is 8 -- see the FLAG on the member in the header.
// ---------------------------------------------------------------------------
void VehicleTypeRuntime::_AssertLayout()
{
    static_assert(offsetof(VehicleTypeRuntime, mBBoxOffset) == 0x00, "mBBoxOffset");
    static_assert(offsetof(VehicleTypeRuntime, mBBoxHalfSize) == 0x10, "mBBoxHalfSize");
    static_assert(offsetof(VehicleTypeRuntime, mCabPivot_TrailerPivot_BackAxle_FwdAxle) == 0x20,
                  "mCabPivot_TrailerPivot_BackAxle_FwdAxle");
    static_assert(offsetof(VehicleTypeRuntime, mMass_WheelRadius_Z_W) == 0x30,
                  "mMass_WheelRadius_Z_W");

    static_assert(offsetof(VehicleTypeRuntime, mAttribKey)
                      > offsetof(VehicleTypeRuntime, mMass_WheelRadius_Z_W),
                  "mAttribKey follows mMass_WheelRadius_Z_W");
    static_assert(offsetof(VehicleTypeRuntime, maiPaintColours)
                      > offsetof(VehicleTypeRuntime, mAttribKey),
                  "maiPaintColours follows mAttribKey");
    static_assert(offsetof(VehicleTypeRuntime, miNumPaintColours)
                      > offsetof(VehicleTypeRuntime, maiPaintColours),
                  "miNumPaintColours follows maiPaintColours");
}

// (include for the bring-up gate's one-shot log lives at the top of the file)
Vector4 VehicleTypeRuntime::PickPaintColourForVehicle(u32 luSeed,
                                                      s32 liNumAvailableColours,
                                                      const Vector4* lpaPaintColours) const
{
    CGS_ASSERT(lpaPaintColours != nullptr, "lpaPaintColours");

    // FLAG PC bring-up gate (wave T1 consolidation 2026-08-21): miNumPaintColours is seeded
    // by VehicleTypeRuntime::Prepare's ATTRIB half, which is still a named gate (it needs the
    // Attrib::Gen::burnoutcargraphicsasset class -- the same G5 attrib-database family the
    // surface-list mitigation documents). Until it lands every type has ZERO paint colours,
    // and the console assert below would halt the FIRST rendered traffic car. Degrade to a
    // one-shot log + palette entry 0 so the car draws (in the palette's first colour);
    // DELETE-WHEN Prepare's attrib half seeds the table -- the assert below then guards
    // real data again.
    if (miNumPaintColours == 0)
    {
        static bool s_bLoggedNoPaint = false;
        if (!s_bLoggedNoPaint)
        {
            s_bLoggedNoPaint = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "[T1-paint] PickPaintColourForVehicle: miNumPaintColours == 0 "
                       "(Prepare attrib half gated) -- palette entry 0 stand-in [FLAG]\n";
        }
        return lpaPaintColours[0];
    }

    CGS_ASSERT(miNumPaintColours != 0, "miNumPaintColours != 0");

    const u32 luCount = static_cast<u32>(miNumPaintColours);   // (s8)->extsb, then unsigned
    const u32 luWhich = luSeed % luCount;

    s32 liIndex = maiPaintColours[luWhich];                    // (s8)->extsb sign-extend
    const s32 liMaxIndex = liNumAvailableColours - 1;
    if (liIndex > liMaxIndex)
    {
        liIndex = liMaxIndex;                                  // min(idx, max)
    }

    return lpaPaintColours[liIndex];
}

}
