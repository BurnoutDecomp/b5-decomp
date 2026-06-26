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
    // Construct  @0x826203E8
    //   base Construct, Wheel::Clear each of the 4 wheels (the do/while walks +304 stride 224 until
    //   Wheel::Clear returns the sentinel), SimpleVehicleAttribs::Construct, zero
    //   mHandlingBodyOffset(+1680)/mHalfExtent(+1696)/the two AABBs(@+1392 stride 16, the
    //   `stvx128 v1,r11,r10` pair) and seed the flag words: *(+1430)=0x8000, *(+1428)=-1,
    //   *(+1424)=0.0, *(+1432)=0 -- these console scratch words land in the AABB/flag region; in the
    //   BY-NAME home they are reproduced as the deform/crash bool seeds + the wheel-plane reset, then
    //   Reset, then *(+112)=0 (the base engine-only gate cleared).
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::Construct()
    {
        // The X360 calls the base Construct on the base subobject (`this+16`); the only committed
        // Construct in the base chain is ExternallySimulatedBody::Construct (declared-only sibling).
        ExternallySimulatedBody::Construct();
        for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            maWheels[liWheel].Clear();
        mSimpleAttribs.Construct();

        mHandlingBodyOffset = KV_ZERO;                 // +1680
        mHalfExtent         = KV_ZERO;                 // +1696
        mDeformableAABB.mMin = KV_ZERO;                // +1392 region (the stvx128 v1 zero pair)
        mDeformableAABB.mMax = KV_ZERO;
        mOriginalAABB.mMin   = KV_ZERO;
        mOriginalAABB.mMax   = KV_ZERO;

        // FLAG: the X360 seeds raw scratch words *(+1430)=0x8000 / *(+1428)=-1 / *(+1424)=0.0 /
        // *(+1432)=0 that sit in the deform/crash-flag/wheel-plane region. In the BY-NAME home the
        // faithful intent is the post-construct reset state, applied by Reset() below.
        Reset();
        // *(this+112)=0 -- the base sleep/engine-only-update gate (mbFrozen region), cleared.
        SetFrozen(false);
    }

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
    //   the console layout) Wheel::Reset each wheel; then zero the four velocity/transform-delta
    //   SIMD caches (+1328/+1344/+1360/+1376), splat mfSpeedMPH(+1728) to 0, and clear the crash
    //   bools (+1808 mbCrashing, +1809 mbStartedFatallyCrashing, +1812 mbMinWheelDistValid,
    //   +1813 mbAnyWheelsDetatched).
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::Reset()
    {
        if (!mbStartedDeforming)                        // gate: console reads the deform latch
        {
            for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
                maWheels[liWheel].Reset(KV_ZERO);
        }

        // FLAG: the four zeroed SIMD caches at +1328/+1344/+1360/+1376 are the body's
        // velocity / angular-velocity / transform-delta scratch registers (below mSimpleAttribs in
        // the console layout). The faithful named intent is to zero the body motion + wheel-plane
        // cache; reproduced here on the base motion vectors and the wheel-plane register.
        mLinearVelocity      = KV_ZERO;
        mAngularVelocity     = KV_ZERO;
        mWheelPlanePosAndHeight = { 0.0f, 0.0f, 0.0f, 0.0f };
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
        // CollisionTag is one u32 (BrnCommonTypes: `struct CollisionTag { u32 muValue; }`). The asm
        // stores a3 as the low halfword (+714) and a2 as the high halfword (+715) of that u32.
        mAboveGroundTestResult.mCollisionTag.muValue =
            (static_cast<u32>(lu16TagHi) << 16) | static_cast<u32>(lu16TagLo);

        // mfVerticalDistance = lvPosition.y - <reference y> (the asm `vspltw v13,a3-arg,1 ;
        // lvx128 v0,this,+64 ; vspltw v0,v0,1 ; vsubfp`). FLAG: the +64 reference splat lane is a
        // vehicle-frame height; reproduced as the y delta vs the supplied position's y reference.
        mAboveGroundTestResult.mfVerticalDistance = lvPosition.y - lvNormal.y;
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
