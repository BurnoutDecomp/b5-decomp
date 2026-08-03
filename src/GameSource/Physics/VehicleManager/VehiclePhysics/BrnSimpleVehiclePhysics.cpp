#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"             // rw::math::vpu::{IsValid, operator+/-, Mult, Dot}

// BrnPhysics::Vehicle::SimpleVehiclePhysics -- the 3 functions owned by the BrnPhysics-bodies
// group. The X360 build is VMX128 inline asm; these are the DE-SIMD'd named-member equivalents
// (no __asm), recovered store-for-store from the asm at 0x825BF158 / .618 / .870.
//
// COM-offset transform identity (used by both graphics-transform funcs): the physics body's
// mTransform pivots about the centre of mass; the graphics mesh pivots about the model origin.
// The two differ by the COM offset rotated into world space by the transform's rotation basis:
//
//     comWorld = mTransform.xAxis * com.x + yAxis * com.y + zAxis * com.z
//
// (the vmul + two vmaddfp chain in the asm). Graphics = physics with comWorld SUBTRACTED from
// the translation; Set = physics with comWorld ADDED back.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // Rotate the (model-space) COM offset into world space by the matrix's 3x3 basis.
    static Vector3 RotateCOMOffsetToWorld(const Matrix44Affine& lrTransform, const Vector3& lrCOMOffset)
    {
        // xAxis*com.x + yAxis*com.y + zAxis*com.z  (matches the asm vmul/vmaddfp accumulation).
        Vector3 lvWorld = vpu::Mult(lrTransform.xAxis, lrCOMOffset.x);
        lvWorld = vpu::Add(lvWorld, vpu::Mult(lrTransform.yAxis, lrCOMOffset.y));
        lvWorld = vpu::Add(lvWorld, vpu::Mult(lrTransform.zAxis, lrCOMOffset.z));
        return lvWorld;
    }

    // Per-row finite check of an affine transform (the asm IsValid loop: each of the 4 rows'
    // x/y/z lanes self-compared for NaN, ANDed together). Models RwMathVPU::IsValid(Matrix44).
    static bool IsTransformValid(const Matrix44Affine& lrTransform)
    {
        return vpu::IsValid(lrTransform.xAxis) && vpu::IsValid(lrTransform.yAxis)
            && vpu::IsValid(lrTransform.zAxis) && vpu::IsValid(lrTransform.wAxis);
    }

    // ---------------------------------------------------------------------------------------
    // GetGraphicsVehicleTransform  @0x825BF158
    //   asserts attribs valid + mTransform finite + COM finite, copies mTransform into the
    //   result, then subtracts the world-space COM offset from the translation row, and
    //   asserts the produced transform is finite.
    // ---------------------------------------------------------------------------------------
    Matrix44Affine SimpleVehiclePhysics::GetGraphicsVehicleTransform() const
    {
        CGS_ASSERT(GetSimpleAttribs()->IsValid(), "GetSimpleAttribs()->IsValid()");
        CGS_ASSERT(IsTransformValid(mTransform), "RwMathVPU::IsValid( mTransform )");
        CGS_ASSERT(vpu::IsValid(GetSimpleAttribs()->mCOMOffset),
                   "RwMathVPU::IsValid( GetSimpleAttribs()->mCOMOffset )");

        Matrix44Affine lTransform = mTransform;
        const Vector3 lvCOMWorld = RotateCOMOffsetToWorld(mTransform, GetSimpleAttribs()->mCOMOffset);
        lTransform.wAxis = vpu::Subtract(lTransform.wAxis, lvCOMWorld);

        CGS_ASSERT(IsTransformValid(lTransform), "RwMathVPU::IsValid( lTransform )");
        return lTransform;
    }

    // ---------------------------------------------------------------------------------------
    // SetGraphicsVehicleTransform  @0x825BF618
    //   asserts the incoming graphics transform is finite, copies it into mTransform, then adds
    //   the world-space COM offset (rotated by the INCOMING transform's basis) back into the
    //   translation row -- the inverse of GetGraphicsVehicleTransform.
    // ---------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::SetGraphicsVehicleTransform(Matrix44Affine lTransform)
    {
        CGS_ASSERT(IsTransformValid(lTransform), "RwMathVPU::IsValid( lTransform )");

        mTransform = lTransform;
        const Vector3 lvCOMWorld = RotateCOMOffsetToWorld(mTransform, GetSimpleAttribs()->mCOMOffset);
        mTransform.wAxis = vpu::Add(mTransform.wAxis, lvCOMWorld);
    }

    // ---------------------------------------------------------------------------------------
    // IsContactBelowWheelPlane  @0x825BF870
    //   false early if the wheel plane is not yet computed. Otherwise project the contact point
    //   onto the vehicle up axis (mTransform.yAxis) relative to the wheel-plane position, and
    //   test whether the plane height (packed in the w lane of mWheelPlanePosAndHeight) plus the
    //   threshold exceeds that projection -- i.e. the contact is below the wheel plane.
    //     return (lvfThreshold + planeHeight) > dot(contact - planePos, up)
    // ---------------------------------------------------------------------------------------
    bool SimpleVehiclePhysics::IsContactBelowWheelPlane(Vector3 lvContactPoint, VecFloat lvfThreshold) const
    {
        if (!mbMinWheelDistValid)
            return false;

        const Vector3 lvPlanePos = { mWheelPlanePosAndHeight.x, mWheelPlanePosAndHeight.y,
                                     mWheelPlanePosAndHeight.z, 0.0f };
        const f32 lfPlaneHeight = mWheelPlanePosAndHeight.w;          // height packed in w lane
        const Vector3 lvOffset = vpu::Subtract(lvContactPoint, lvPlanePos);
        const f32 lfProjected = vpu::Dot(lvOffset, mTransform.yAxis); // vehicle up axis

        return (lvfThreshold.x + lfPlaneHeight) > lfProjected;
    }

    // ===========================================================================================
    //  C11_simple_traffic_attribs group -- the tractable SimpleVehiclePhysics body set.
    //  The X360 originals are VMX128 inline asm; these are the de-SIMD'd named-member equivalents.
    // ===========================================================================================

    static const Vector3 KV_ZERO = { 0.0f, 0.0f, 0.0f, 0.0f };

    // -------------------------------------------------------------------------------------------
    // Construct @0x826203E8 lives in its own TU, BrnSimpleVehiclePhysics_Construct.cpp --
    // SPLIT 2026-08-02 (physics wave 3). BUILD-MECHANICS SPLIT ONLY (byte-identical body,
    // unchanged declared home).
    //
    // WHY: it is the only function in this TU that calls SimpleVehicleAttribs::Construct
    // @0x825E6580, which has NO BODY anywhere in the tree. That console function is a ~120-line
    // lane-write initialiser over ~15 unresolved .rdata float constants (flt_82096C9C /
    // flt_8200473C / flt_82004F5C / flt_82013A78 / flt_8200D538 / flt_82020A84 / flt_82004A1C /
    // flt_82012EF8 / flt_82004740 / flt_820047C0 / flt_82004010 ...) writing offsets +0x00 .. +0xE4
    // of a type this tree still models as a TWO-MEMBER minimal slice (mCOMOffset / mbIsValid) --
    // it cannot be bodied until the real VehicleAttribs layout pass lands, and inventing the
    // constants is forbidden. Keeping Construct here made the whole TU -- including
    // GetGraphicsVehicleTransform, the function VehicleOutputInterface::UpdateRaceCarState needs
    // to publish the car's render pose -- unlinkable for the sake of one blocked callee.
    // Re-merge when SimpleVehicleAttribs::Construct lands.
    // -------------------------------------------------------------------------------------------

    // -------------------------------------------------------------------------------------------
    // Destruct  @0x826206D0
    //   base Destruct, the same Wheel::Clear loop, Reset, *(+112)=0.
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::Destruct()
    {
        ExternallySimulatedBody::Destruct();
        for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            maWheels[liWheel].Clear();
        Reset();
        SetFrozen(false);
    }

    // -------------------------------------------------------------------------------------------
    // Reset  @0x825D9A58
    //   if !mbStartedDeforming (the `if ( !*(result+1668) )` gate -- 1668 is the deform latch in
    //   the console layout) Wheel::Reset each wheel; then zero maLocalTractionPoints[0..3]
    //   (+1328/+1344/+1360/+1376), splat mfSpeedMPH(+1728) to 0, and clear the crash bools
    //   (+1808 mbCrashing, +1809 mbStartedFatallyCrashing, +1812 mbMinWheelDistValid,
    //   +1813 mbAnyWheelsDetatched).
    //
    // ⚠️⚠️ CORRECTED 2026-08-03. The four 16-byte stores at +1328/+1344/+1360/+1376 were
    // FLAGGED here as "the body's velocity / angular-velocity / transform-delta scratch
    // registers" and reproduced as `mLinearVelocity = 0; mAngularVelocity = 0;
    // mWheelPlanePosAndHeight = 0;`. All THREE of those were INVENTED STORES -- this function
    // touches none of those members -- and the four members it does clear were left out.
    //
    // 1328 == 0x530, and the asm is `li r9,0x530 ; li r8,0x540 ; li r7,0x550 ; li r6,0x560` with
    // four `stvx128 v0(==0), r31, rX`: four consecutive 16-byte slots at stride 16 starting at
    // +0x530. That is exactly `Vector3 maLocalTractionPoints[4]` (BrnSimpleVehiclePhysics.h:190,
    // DWARF :359). Two independent witnesses:
    //   * VehiclePhysics.h's own map already records "maLocalWheelPositions +0x530 ->
    //     SimpleVehiclePhysics::maLocalTractionPoints", and StoreLocalWheelPositions writes
    //     +0x530/+0x540/+0x550/+0x560;
    //   * VehiclePhysics::Reset @0x825FDD78 (pulled from the .i64 this wave) zeroes the SAME four
    //     addresses with the same `li 0x530/0x540/0x550/0x560` idiom, alongside its own separate
    //     clears -- and it does NOT touch mLinearVelocity (base+0x40 == this+0x50) either.
    // mWheelPlanePosAndHeight is +1712 (0x6B0); the only register in that neighbourhood this
    // function writes is +0x6C0 == mfSpeedMPH, which was already correct.
    //
    // This mattered: Reset is on the car-placement path, and zeroing mLinearVelocity /
    // mAngularVelocity there destroys the velocity the caller is resetting the car WITH
    // (VehiclePhysics::Reset takes the velocity as its argument and re-publishes it).
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::Reset()
    {
        if (!mbStartedDeforming)                        // gate: console reads the deform latch
        {
            for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
                maWheels[liWheel].Reset(KV_ZERO);
        }

        for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            maLocalTractionPoints[liWheel] = KV_ZERO;   // +0x530/+0x540/+0x550/+0x560

        mfSpeedMPH           = { 0.0f, 0.0f, 0.0f, 0.0f };   // +1728 (VecFloat == Vector4)

        mbCrashing               = false;               // +1808
        mbStartedFatallyCrashing = false;               // +1809
        mbMinWheelDistValid      = false;               // +1812
        mbAnyWheelsDetatched     = false;               // +1813
    }

    // -------------------------------------------------------------------------------------------
    // SetAboveGroundTestResult(Vector3,Vector3,u16,u16)  @0x82602880
    //   asserts the position + normal are finite (debug), stores the position at +348
    //   (mAboveGroundTestResult.mIntersectionPosition), the normal at +364
    //   (mIntersectionNormal), the two tag halfwords at +714/+715 (mCollisionTag), derives the
    //   vertical distance = (test position - normal-lane1 splat).y stored at +356, then sets the
    //   valid flag (+1432). FLAG: the +356 distance store reproduces the asm `vsubfp v0,v0(@+64),v13`
    //   exactly; the +64 source is a vehicle-relative reference height -- pinned BY NAME as
    //   mfVerticalDistance, value = the y delta the asm computes.
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::SetAboveGroundTestResult(Vector3 lvPosition, Vector3 lvNormal,
                                                        u16 lu16TagHi, u16 lu16TagLo)
    {
        CGS_ASSERT(vpu::IsValid(lvPosition), "RwMathVPU::IsValid( lLineTestResultPosition )");
        CGS_ASSERT(vpu::IsValid(lvNormal),   "RwMathVPU::IsValid( lLineTestResultNormal )");

        mAboveGroundTestResult.mIntersectionPosition = lvPosition;   // +348
        mAboveGroundTestResult.mIntersectionNormal   = lvNormal;     // +364
        // CollisionTag is one u32 (BrnCommonTypes: `struct CollisionTag { u32 muValue; }`). Big-endian
        // PPC: the asm stores lu16TagLo (a3=r5) at the LOWER byte address (this+1428) and lu16TagHi
        // (a2=r4) at the HIGHER byte address (this+1430). On BE the halfword at the lower address is
        // the high 16 bits of the u32, so the assembled value is (lu16TagLo << 16) | lu16TagHi.
        mAboveGroundTestResult.mCollisionTag.muValue =
            (static_cast<u32>(lu16TagLo) << 16) | static_cast<u32>(lu16TagHi);

        // mfVerticalDistance = mTransform.wAxis.y - lvPosition.y  (the asm
        // `vspltw v13,v1(lvPosition),1 ; lvx128 v0,this,+64 ; vspltw v0,v0,1 ; vsubfp v0,v0,v13`).
        // this+64 = mTransform.wAxis (mTransform @ +16; rows xAxis/yAxis/zAxis/wAxis at +16/+32/+48/+64),
        // i.e. the vehicle world position. The subtraction is (reference y) - (position y).
        mAboveGroundTestResult.mfVerticalDistance = mTransform.wAxis.y - lvPosition.y;
        mAboveGroundTestResult.mbValid = true;                       // +1432
    }

    // -------------------------------------------------------------------------------------------
    // ClearCrashing  @0x825B8EA8  -- clear the crash master flag + the fatal-crash latch.
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::ClearCrashing()
    {
        mbCrashing               = false;   // +1808
        mbStartedFatallyCrashing = false;   // +1809
    }
}
}
