#ifndef BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H
#define BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H

// ============================================================================
// BrnTraffic::VehicleTypeRuntime -- OWNING HEADER.
//
// One record per traffic vehicle TYPE (96 of them, TrafficEntityModule::
// maVehicleTypeRuntime). Holds what the spawn and render paths need that comes from the
// type's physics/deformation spec rather than the streamed TrafficData: the collision box,
// the four longitudinal pivots, mass, wheel radius, the attribute key, and the paint palette.
// Member list and names: DecFIGS DWARF BrnTrafficVehicleTypeRuntime.h:94..:112.
//
// FLAG -- X360 STRIDE 128 vs. HOST FOOTPRINT 96. TrafficEntityModule::Construct @0x82740220
// walks this array with `addi r26, r26, 0x80` for 0x60 == 96 iterations from this+0x76380, so
// the console sizeof is 128. The DWARF member list sums to 0x5D and rounds to 0x60 == 96 at
// Vector3 alignment; nothing in the ARTIST asm touches a byte at or above 0x5D, so the 35-byte
// tail is dead and this header invents no members to fill it. If a consumer turns up that
// reads past 0x5C, the missing member is a merge-window addition and it lands here.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"

namespace BrnPhysics { namespace Deformation { class StreamedDeformationSpec; } }

namespace BrnTraffic
{
    class VehicleTypeRuntime
    {
    public:
        // DWARF BrnTrafficVehicleTypeRuntime.h:110.
        static const u32 KU_NUM_PAINT_COLOURS_PER_VEHICLE = 20;

        // ---- lifecycle -----------------------------------------------------------
        // DWARF :60. X360 @0x827521E8 is an EXPORT HOLE: no per-function JSON exists for that
        // address, though TrafficEntityModule::Construct @0x82740220 calls it by name. The
        // body below is the Feb-2007 original verbatim.
        //
        // FLAG (unverifiable, low risk): the ship added mMass_WheelRadius_Z_W, mAttribKey,
        // maiPaintColours and miNumPaintColours, and the export hole hides whether
        // ship-Construct seeds them. Prepare @0x82761B10 writes all four before any consumer
        // reads them, so leaving them untouched cannot produce a stale read.
        void Construct()
        {
            mBBoxOffset.SetZero();
            mBBoxHalfSize.SetZero();
            mCabPivot_TrailerPivot_BackAxle_FwdAxle = Vector4{ -1.5f, 1.5f, -1.1f, 1.1f };
        }

        // DWARF :66 `void Prepare(const StreamedDeformationSpec *, Attribute::Key)`,
        // X360 @0x82761B10. PARTIAL; the body in BrnTrafficVehicleTypeRuntime_Prepare.cpp
        // names which stores are real and which two legs are gated.
        //
        // The second parameter is retyped u32 -> u64 deliberately. The DWARF spells it
        // `Attribute::Key`, which this tree aliases to u32, but the console value is 64 bits:
        // the DWARF's companion constant is `KU_ATTRIB_KEY_SIZE = 8` (:102); Prepare opens
        // with `mr r4,r5 ; std r4, 0x40(r16)`, a 64-bit store into mAttribKey; and the
        // producer FindVehicleTypeAttribKey_EXPENSIVE @0x8273F0B8 tail-returns
        // CgsAttribSys::AttribSysCollectionKey::GetHashKey, which this tree already models as
        // u64. The u32 alias would truncate the key, so AttribKey is the local spelling and
        // the tree-wide alias is left alone.
        typedef u64 AttribKey;

        void Prepare(const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec,
                     AttribKey lAttribKey);

        // DWARF :75. X360 @0x827049A8, bodied in
        // BrnTrafficVehicleTypeRuntime_PickPaintColour.cpp.
        Vector4 PickPaintColourForVehicle(u32 luSeed,
                                          s32 liNumAvailableColours,
                                          const Vector4* lpaPaintColours) const;

        // ---- accessors -----------------------------------------------------------
        // DWARF :78..:89. None is in the ARTIST ledger; the console inlines each at its call
        // site, so each is attested by the read it produces (TrafficEntityModule::Prepare
        // stage 3 reads mBBoxHalfSize at element+0x10; VehicleAxles::SetFromVehicleTransform
        // @0x82756738 splats lanes 2 and 3 of the pivot vector at element+0x20).
        //
        // FLAG (host lowering): the DWARF types the four pivot getters and GetMass /
        // GetWheelRadius as `VecFloat`, the console's broadcast 16-byte lane. They are f32
        // here, matching this tree's lowering for scalar lane reads and every reconstructed
        // consumer, which multiplies a Vector3 by the scalar.
        Vector3 GetBBoxOffset() const { return mBBoxOffset; }
        Vector3 GetBBoxHalfSize() const { return mBBoxHalfSize; }

        f32 GetCabPivotDistance() const { return mCabPivot_TrailerPivot_BackAxle_FwdAxle.x; }
        f32 GetTrailerPivotDistance() const { return mCabPivot_TrailerPivot_BackAxle_FwdAxle.y; }
        f32 GetBackAxleOffset() const { return mCabPivot_TrailerPivot_BackAxle_FwdAxle.z; }
        f32 GetForwardAxleOffset() const { return mCabPivot_TrailerPivot_BackAxle_FwdAxle.w; }

        f32 GetMass() const { return mMass_WheelRadius_Z_W.x; }
        f32 GetWheelRadius() const { return mMass_WheelRadius_Z_W.y; }

        Vector4 GetCabPivot_TrailerPivot_BackAxle_FwdAxle() const
        {
            return mCabPivot_TrailerPivot_BackAxle_FwdAxle;
        }

        // DWARF :89 `const Attribute::Key GetAttribKey() const`. Returns the console's real
        // 8-byte key; see the width note on Prepare above.
        AttribKey GetAttribKey() const { return mAttribKey; }

        static void _AssertLayout();   // never called; body in the .cpp

    private:
        Vector3 mBBoxOffset;                              // :94   +0x00
        Vector3 mBBoxHalfSize;                            // :95   +0x10
        Vector4 mCabPivot_TrailerPivot_BackAxle_FwdAxle;  // :97   +0x20
        Vector4 mMass_WheelRadius_Z_W;                    // :99   +0x30
        // :104  +0x40. The console's real EIGHT-byte key (`std r4, 0x40(r16)`, DWARF
        // `KU_ATTRIB_KEY_SIZE = 8` at :102). _AssertLayout pins only the four leading 16-byte
        // members by OFFSET and the rest by ORDER, since the host record does not have to
        // match the console's 128-byte stride (see the header banner).
        AttribKey mAttribKey;
        s8 maiPaintColours[KU_NUM_PAINT_COLOURS_PER_VEHICLE];  // :111 +0x48
        s8 miNumPaintColours;                             // :112  +0x5C
    };
}

#endif // BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H
