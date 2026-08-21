#ifndef BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H
#define BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H

// ============================================================================
// BrnTraffic::VehicleTypeRuntime -- OWNING HEADER.
//
// One record per traffic vehicle TYPE (96 of them, TrafficEntityModule::
// maVehicleTypeRuntime). Holds everything the spawn and render paths need that is derived
// from the type's physics/deformation spec rather than from the streamed TrafficData:
// the collision box, the four longitudinal pivots, mass + wheel radius, the attribute key,
// and the type's paint-colour palette indices.
//
// Member list and names: DecFIGS DWARF BrnTrafficVehicleTypeRuntime.h:94..:112, every one
// of them attested in the ARTIST asm (see the per-member offsets in _AssertLayout's
// comment in BrnTrafficVehicleTypeRuntime_PickPaintColour.cpp).
//
// ---------------------------------------------------------------------------
// FLAG -- X360 STRIDE 128 vs. HOST FOOTPRINT 96. TrafficEntityModule::Construct
// @0x82740220 walks this array with `addi r26, r26, 0x80` for exactly 0x60 == 96
// iterations from this+0x76380, so sizeof(VehicleTypeRuntime) is 128 ON THE X360. The
// DWARF member list above sums to 0x5D (0x40 mAttribKey + 8, 0x48 paint indices + 20,
// 0x5C count + 1) and rounds to 0x60 == 96 at the Vector3 16-byte alignment. Nothing in
// the ARTIST asm ever touches a byte at or above 0x5D in this record -- Prepare
// @0x82761B10 writes 0x00/0x10/0x20/0x30/0x40/0x48+i/0x5C and nothing else -- so the
// 35-byte tail is DEAD on the console and this header does NOT invent members to fill it.
// It is left as a documented delta rather than padding, because padding it would fake a
// console byte count on a host layout, which is this tree's #1 recurring defect. If a
// consumer ever turns up that reads past 0x5C, the missing member is a merge-window
// addition ARTIST has and DecFIGS does not, and it lands here.
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
        // DWARF :60. X360 @0x827521E8 -- an EXPORT HOLE (no per-function JSON exists for
        // that address, though TrafficEntityModule::Construct @0x82740220 calls it by name
        // in a 96-iteration loop). The body below is the Feb-2007 original verbatim
        // (references/.../BrnTrafficVehicleTypeRuntime.h), which is legitimate here because
        // it is a real leaked body for a function the ledger attests, and its three stores
        // cover exactly the three members that build carried.
        //
        // FLAG (unverifiable, low risk): the ship added mMass_WheelRadius_Z_W, mAttribKey,
        // maiPaintColours and miNumPaintColours, and with the export hole there is no way to
        // tell whether ship-Construct also seeds those four. All four are written by
        // Prepare @0x82761B10 before any consumer reads them, so leaving them untouched here
        // matches the leak and cannot produce a stale read on the reconstructed paths.
        void Construct()
        {
            mBBoxOffset.SetZero();
            mBBoxHalfSize.SetZero();
            mCabPivot_TrailerPivot_BackAxle_FwdAxle = Vector4{ -1.5f, 1.5f, -1.1f, 1.1f };
        }

        // DWARF :66 `void Prepare(const StreamedDeformationSpec *, Attribute::Key)`.
        // X360 @0x82761B10 -- ⭐ RECONSTRUCTED 2026-08-21 (wave T1 round 3, closure item 3),
        // PARTIAL; body in BrnTrafficVehicleTypeRuntime_Prepare.cpp, which spells out exactly
        // which of the seven stores are real and which two legs are named gates.
        //
        // ⚠️ SECOND PARAMETER RETYPED, DELIBERATELY, u32 -> u64. The DWARF spells it
        // `Attribute::Key`, and this tree's `Attribute::Key` is `typedef u32 Key`
        // (SDKs/Packages/AttribSys/.../AttributeKey.h) -- a tree-wide alias this header does
        // not own. But the value is 64 BITS on the console and there is no ambiguity about it:
        //   * the DWARF's own companion constant is `KU_ATTRIB_KEY_SIZE = 8` (:102);
        //   * Prepare's FIRST instruction pair is `mr r4,r5 ; std r4, 0x40(r16)` -- a 64-bit
        //     store of the whole incoming register into mAttribKey;
        //   * the producer, TrafficEntityModule::FindVehicleTypeAttribKey_EXPENSIVE
        //     @0x8273F0B8, tail-returns CgsAttribSys::AttribSysCollectionKey::GetHashKey,
        //     which this tree ALREADY models as returning u64 (widened 2026-08-01 by the
        //     physics wave, for exactly this reason: "a 32-bit key can never match").
        // Passing it through the u32 alias would silently truncate a 64-bit collection key --
        // the console-value-narrowed-on-the-host defect class this project keeps re-finding.
        // So the parameter and the member below take the console's real width, and the
        // `Attribute::Key` alias is left alone. AttribKey is the local spelling.
        typedef u64 AttribKey;

        void Prepare(const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec,
                     AttribKey lAttribKey);

        // DWARF :75. X360 @0x827049A8, bodied in
        // BrnTrafficVehicleTypeRuntime_PickPaintColour.cpp.
        Vector4 PickPaintColourForVehicle(u32 luSeed,
                                          s32 liNumAvailableColours,
                                          const Vector4* lpaPaintColours) const;

        // ---- accessors -----------------------------------------------------------
        // DWARF :78..:89. None of these appears in the ARTIST ledger because the console
        // inlines every one of them at its call site; each is attested by the inlined read
        // it produces (e.g. Prepare stage 3 of TrafficEntityModule reads mBBoxHalfSize at
        // element+0x10, VehicleAxles::SetFromVehicleTransform @0x82756738 splats lanes 2
        // and 3 of mCabPivot_TrailerPivot_BackAxle_FwdAxle at element+0x20).
        //
        // FLAG (host lowering): the DWARF types the four pivot getters and GetMass /
        // GetWheelRadius as `VecFloat` -- the console's broadcast 16-byte lane. They are
        // spelled f32 here, matching this tree's established lowering for scalar lane
        // reads (Vehicle::GetSwerveTime, GetRandomVal, ...) and matching every reconstructed
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
        // 8-byte key -- see the width note on Prepare above. No consumer exists in this tree
        // yet (the console inlines every read), so the retype orphans nothing.
        AttribKey GetAttribKey() const { return mAttribKey; }

        static void _AssertLayout();   // never called; body in the .cpp

    private:
        Vector3 mBBoxOffset;                              // :94   +0x00
        Vector3 mBBoxHalfSize;                            // :95   +0x10
        Vector4 mCabPivot_TrailerPivot_BackAxle_FwdAxle;  // :97   +0x20
        Vector4 mMass_WheelRadius_Z_W;                    // :99   +0x30
        // :104  +0x40. ⭐ WIDTH DIVERGENCE CLOSED 2026-08-21 (wave T1 round 3): this is the
        // console's real EIGHT-byte key (`std r4, 0x40(r16)`, DWARF `KU_ATTRIB_KEY_SIZE = 8`
        // at :102), carried through the local AttribKey typedef rather than the tree-wide
        // u32 `Attribute::Key` alias -- see the full argument on Prepare's declaration above.
        // _AssertLayout still pins only the four leading 16-byte members by OFFSET and the
        // rest by ORDER, because the host record is not required to match the console's
        // 128-byte stride (see the header banner).
        AttribKey mAttribKey;
        s8 maiPaintColours[KU_NUM_PAINT_COLOURS_PER_VEHICLE];  // :111 +0x48
        s8 miNumPaintColours;                             // :112  +0x5C
    };
}

#endif // BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H
