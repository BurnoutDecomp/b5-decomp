#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"

#include <cstddef>   // offsetof, for the never-called _AssertLayout pin below

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags

// BrnTraffic::VehicleTypeRuntime::PickPaintColourForVehicle @ 0x827049A8
//
// Register map, with the 16-byte return going through the hidden sret pointer in r3:
//   r3 sret (destination Vector4), r4 this, r5 luSeed, r6 liNumAvailableColours,
//   r7 lpaPaintColours (Vector4 palette, 16-byte stride). The record reads are
//   miNumPaintColours @+0x5C and maPaintColourIndices @+0x48, both lbz+extsb.
//
// The asm's branchless clamp (mask = (idx-max)>>31; res = (max & ~mask) | (mask & idx)) is
// min(idx, max), reconstructed as the structured min. There is no lower clamp. The
// lvx128/stvx128 pair is one 16-byte aligned Vector4 copy, reconstructed as a value copy.
// Assert lines 134 and 135 are reproduced via CGS_ASSERT; the baked d:\p4 path is dropped.

namespace BrnTraffic
{

// Layout pins. NEVER CALLED -- a static member function so offsetof() reaches the private
// members. Only the four leading lane members take absolute offsets; they are pointer-free
// and 16-byte aligned, so console and host agree. Each is attested:
//   +0x00 mBBoxOffset / +0x10 mBBoxHalfSize   Prepare @0x82761B10 `stvx128 v0, r0, r16` and
//                                             `stvx128 v0, r16, r27` (r27 == 0x10)
//   +0x20 mCabPivot_TrailerPivot_BackAxle_FwdAxle   VehicleAxles::SetFromVehicleTransform
//                                             @0x82756738 splats lanes 3 and 2 from r5+0x20
//   +0x30 mMass_WheelRadius_Z_W               Prepare's `vrlimi128` into r16+0x30
// The trailing three sit at X360 +0x40/+0x48/+0x5C but are pinned by ORDER only, because the
// console's 8-byte attrib key does not match this tree's 4-byte Attribute::Key alias.
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

Vector4 VehicleTypeRuntime::PickPaintColourForVehicle(u32 luSeed,
                                                      s32 liNumAvailableColours,
                                                      const Vector4* lpaPaintColours) const
{
    CGS_ASSERT(lpaPaintColours != nullptr, "lpaPaintColours");

    // FLAG PC bring-up gate: miNumPaintColours is seeded by VehicleTypeRuntime::Prepare's
    // ATTRIB half, still gated on Attrib::Gen::burnoutcargraphicsasset. Until it lands every
    // type has ZERO paint colours and the assert below would halt the first rendered traffic
    // car, so degrade to a one-shot log plus palette entry 0.
    // DELETE-WHEN Prepare's attrib half seeds the table; the assert then guards real data.
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
