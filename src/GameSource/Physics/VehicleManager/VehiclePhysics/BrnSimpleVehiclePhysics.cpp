#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"  // the full VehicleAttribs (SwitchAttribs reads its base/suspension lanes)

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include <cmath>                                        // std::sqrt (AddTractionPoint's line distance)
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
    // Construct @0x826203E8 -- ⭐ RE-MERGED 2026-08-09 (attribs-setup wave), exactly per the
    // split TU's own contract ("TO RE-MERGE: body SimpleVehicleAttribs::Construct, then move
    // this body back and delete the TU"). SimpleVehicleAttribs::Construct @0x825E6580 is now
    // BODIED in VehicleAttribs.cpp (every constant image-read), so the 2026-08-02 build-mechanics
    // split (BrnSimpleVehiclePhysics_Construct.cpp, never mounted) is retired. The body below is
    // the split TU's, byte-identical.
    //
    //   base Construct, Wheel::Clear each of the 4 wheels (the do/while walks +304 stride 224
    //   until Wheel::Clear returns the sentinel), SimpleVehicleAttribs::Construct, zero
    //   mHandlingBodyOffset(+1680)/mHalfExtent(+1696)/the two AABBs, then Reset, then the
    //   base frozen gate cleared.
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::Construct()
    {
        // The X360 calls the DIRECT base's Construct on the base subobject (`this+16`):
        // `bl BrnPhysics__ExternalPhysicsBody__Construct`.
        ExternalPhysicsBody::Construct();
        for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            maWheels[liWheel].Clear();
        mSimpleAttribs.Construct();

        mHandlingBodyOffset = KV_ZERO;                           // +1680
        mHalfExtent         = KV_ZERO;                           // +1696
        mDeformableAABB.mMin = KV_ZERO;                          // the stvx128 v1 zero pair
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

    // -------------------------------------------------------------------------------------------
    // ⛔⛔ SetCrashing -- NOT THE CONSOLE BODY. VTABLE-CLOSURE GATE ONLY, added 2026-08-03.
    //
    // WHY IT EXISTS AT ALL. `SimpleVehiclePhysics::SetCrashing()` is one of this group's
    // fidelity:BLOCKED entries (BrnSimpleVehiclePhysics.h: "deep VMX128 routines ... whose
    // pseudocode is the degenerate 'local variable allocation has failed' form ... no fabricated
    // math is committed"). It was DECLARE-ONLY and that cost nothing, because nothing mounted ever
    // instantiated this class's vtable.
    //
    // ⭐⭐ THAT CHANGED THE MOMENT BrnVehicleManager.h STOPPED USING A BYTE-PINNED STAND-IN. With
    // `RaceCarPhysics maRaceCarVehicles[8]` embedded BY VALUE, the already-mounted
    // BrnPhysicsModule.cpp -- which embeds a VehicleManager by value -- odr-uses the implicit
    // constructor, which writes eight vptrs, which requires the WHOLE vtable to be defined.
    // Exactly the standing lesson that a mount's closure is its STATIC reference graph and not its
    // live-call graph: this function has no caller anywhere in the mounted tree and is still
    // link-required. It and VehiclePhysics::IsIgnoringPassedOnImpulses were the only two symbols
    // of the entire RaceCarPhysics vtable still missing.
    //
    // ⛔ WHAT IT MUST NOT BECOME. A quiet `{}` here is the silent-drop-stub failure class this
    // project keeps paying for: crash arming would be dropped and every downstream reader would see
    // a plausible "not crashing". So it asserts. It is UNREACHABLE today -- the only callers are
    // TrafficPhysics::Update and VehiclePhysics::SetCrashing's base entry, both unmounted -- and the
    // assert is the thing that says so out loud if that ever stops being true.
    //
    // ⚠️ IT DELIBERATELY WRITES NOTHING. Setting mbCrashing here would be inventing the one part of
    // the body that happens to be guessable and would make the assert look survivable.
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::SetCrashing()
    {
        CGS_ASSERT(false,
                   "SimpleVehiclePhysics::SetCrashing is a vtable-closure gate, not a body -- "
                   "reconstruct it before anything calls it");
    }
    // -------------------------------------------------------------------------------------------
    // AboveGroundTestResult::Reset  (declared "owned by this TU" at the struct; the byte source is
    // the whole-block inline inside VehicleManager::UpdateVehiclePhysics @0x82645430..0x82645444:
    // stvx128 zero at +0x00 (position) and +0x10 (normal), stfs 0 at +0x20 (distance), the two
    // CollisionTag halfwords 0xFFFF/0x8000 at +0x24/+0x26 -- the same "invalid surface" default
    // CollisionTag::Construct stamps -- and the valid byte cleared at +0x28.)
    // -------------------------------------------------------------------------------------------
    void AboveGroundTestResult::Reset()
    {
        mIntersectionPosition = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // stvx128 v127(0), +0x00
        mIntersectionNormal   = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // stvx128 v127(0), +0x10
        mfVerticalDistance    = 0.0f;                                // stfs f31(0), +0x20
        // Big-endian halfword pair {hi=0xFFFF, lo=0x8000} packs to the invalid-surface tag word
        // (same packing SetAboveGroundTestResult documents).
        mCollisionTag.muValue = (0xFFFFu << 16) | 0x8000u;           // sth 0xFFFF/+0x24, 0x8000/+0x26
        mbValid               = false;                               // stb 0, +0x28
    }

    // -------------------------------------------------------------------------------------------
    // SimpleVehiclePhysics::ResetAboveGroundTestResult  (DWARF h:193)
    // Byte source: the inlined copy in VehicleManager::UpdateVehiclePhysics' live-car loop
    // (asm 0x826453B0..0x82645444; the identical stores appear per wheel at +0x130/+0x210/
    // +0x2F0/+0x3D0 sub-offsets 0x28..0x2B / 0xD4 / 0xD6, then the AboveGroundTestResult
    // block at +0x570). PhysicalTrafficManager::ResetAboveGroundTestResults @0x825E8808 runs
    // the same reset per used traffic vehicle (its deferred hook -- see that body's FLAG).
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::ResetAboveGroundTestResult()
    {
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];
            const bool lbWasOnGround = lrWheel.mRoadContact.mbIsOnGround;   // lbz +0x28
            lrWheel.mi8NumContacts                     = 0;                 // stb 0, +0xD4
            lrWheel.mRoadContact.mbIsOnGround          = false;             // stb 0, +0x28
            lrWheel.mRoadContact.mbIsCloseToGround     = false;             // stb 0, +0x2A
            lrWheel.mRoadContact.mbLineTestIsValid     = false;             // stb 0, +0x2B
            lrWheel.mRoadContact.mbWasOnGroundLastUpdate = lbWasOnGround;   // stb prev, +0x29
            lrWheel.mbHasTraction                      = false;             // stb 0, +0xD6
        }
        mAboveGroundTestResult.Reset();
    }

    // ===========================================================================================
    //  SimpleVehiclePhysics::AddTractionPoint   @0x825D9608  (185 insns)
    // ===========================================================================================
    // ⭐ BODIED 2026-08-07 (orchestrator wave) -- this had sat in this header's BLOCKED list as
    // "deep VMX whose math cannot be faithfully reproduced BY NAME"; the claim was UNVERIFIED
    // and false: every lane it touches was already a named member. The traction-line
    // ingestion point: VehicleManager::ReadRaceCarTractionLineTestResults /
    // DoPlayerTractionLineTestsPostSimulation feed each wheel's line-test hit through
    // RaceCarPhysics::AddTractionPoint (a register-transparent chain into here).
    //
    //   0x825D9638  skip entirely while maWheels[leWheel].mu8State (+0xD7) == 2
    //   0x825D9654  split the collision tag: hi = tag >> 16 (r23), lo = tag & 0xFFFF (r22)
    //   0x825D965C  debug NaN sweep over the wheel's local mPosition (xyz vcmpeqfp chain);
    //               on failure the console streams "Invalid wheel position: " << pos <<
    //               ", please tell Graham D." and fires (VehiclePhysics.h-adjacent file, :412)
    //   0x825D9764  worldWheelPos = mTransform * wheel.mPosition (the three vmaddfp rows +
    //               translation, vmaddfp vD,vA,vB,vC == vA*vC + vB with the wAxis seed)
    //   0x825D97CC  lineDist = |worldWheelPos - lvPosition|   (vmsum3fp128 + rsqrt NR chain,
    //               vsel 0-guard; rounded through the var_90 store -- kept f32 here)
    //   0x825D9834  lbIsOnGround      = (wheel.mPosition.y
    //                                    - wheel.mSuspensionAndInertiaVariables.x
    //                                    + wheel.mSlipVariables.w) > lineDist
    //   0x825D98AC  lbIsCloseToGround = (same sum + mSimpleAttribs.mCOMOffset.w) > lineDist
    //               (the +0x5A0 register's .w lane -- the attribs block's leading vector; the
    //               committed slice names it mCOMOffset, and the console reads ITS spare .w)
    //   0x825D98D0  Wheel::SetRoadContact(onGround, close, position, normal, tagHi, tagLo,
    //               lineDist)  -- f1 still holds the distance at the bl, v1/v2 pass through
    void SimpleVehiclePhysics::AddTractionPoint(EVehicleDrivenWheel leWheel, Vector3 lvPosition,
                                                Vector3 lvNormal, u32 lu32CollisionTag)
    {
        Wheel& lrWheel = maWheels[leWheel];

        if (lrWheel.mu8State == 2)   // lbz +0x207 (wheel*0xE0 + 0xD7)
            return;

        const u16 lu16TagHi = static_cast<u16>(lu32CollisionTag >> 16);
        const u16 lu16TagLo = static_cast<u16>(lu32CollisionTag & 0xFFFFu);

        CGS_ASSERT(lrWheel.mPosition.x == lrWheel.mPosition.x &&
                   lrWheel.mPosition.y == lrWheel.mPosition.y &&
                   lrWheel.mPosition.z == lrWheel.mPosition.z,
                   "Invalid wheel position: , please tell Graham D.");   // vcmpeqfp NaN sweep

        // world position of the wheel: the three rotation rows scaled by the local lanes,
        // seeded with the translation row (exactly the console's vmaddfp cascade).
        const Vector3 lvWorldWheelPos{
            mTransform.xAxis.x * lrWheel.mPosition.x + mTransform.yAxis.x * lrWheel.mPosition.y
                + mTransform.zAxis.x * lrWheel.mPosition.z + mTransform.wAxis.x,
            mTransform.xAxis.y * lrWheel.mPosition.x + mTransform.yAxis.y * lrWheel.mPosition.y
                + mTransform.zAxis.y * lrWheel.mPosition.z + mTransform.wAxis.y,
            mTransform.xAxis.z * lrWheel.mPosition.x + mTransform.yAxis.z * lrWheel.mPosition.y
                + mTransform.zAxis.z * lrWheel.mPosition.z + mTransform.wAxis.z,
            0.0f };

        const Vector3 lvDelta{ lvWorldWheelPos.x - lvPosition.x,
                               lvWorldWheelPos.y - lvPosition.y,
                               lvWorldWheelPos.z - lvPosition.z, 0.0f };
        const f32 lfDistSq   = vpu::MagnitudeSquared(lvDelta);
        const f32 lfLineDist = (lfDistSq > 0.0f) ? std::sqrt(lfDistSq) : 0.0f;   // vsel 0-guard

        // the two reach tests: the wheel's local height minus its suspension seat plus its
        // radius lane, against the measured line distance.
        const f32 lfReach = lrWheel.mPosition.y
                          - lrWheel.mSuspensionAndInertiaVariables.x
                          + lrWheel.mSlipVariables.w;
        const bool lbIsOnGround      = lfReach > lfLineDist;
        const bool lbIsCloseToGround = (lfReach + mSimpleAttribs.mCOMOffset.w) > lfLineDist;

        lrWheel.SetRoadContact(lbIsOnGround, lbIsCloseToGround, lvPosition, lvNormal,
                               lu16TagHi, lu16TagLo, lfLineDist);
    }

    // ===========================================================================================
    //  SimpleVehiclePhysics::CalculateNewWheelPlane   @0x82602CB8  (171 insns)
    // ===========================================================================================
    // ⭐ BODIED 2026-08-07 (wheel-cluster wave). Derive the frame's wheel/road plane from the
    // four wheels' line-test contacts: keep the contact whose wheel-bottom sits LOWEST above it
    // (measured along the body up axis) and publish {contact.xyz, max(minHeight, 0)} into
    // mWheelPlanePosAndHeight (+0x6B0) -- the register IsContactBelowWheelPlane consumes.
    //
    // ⚠️ The exported pseudocode mis-renders the whole loop (base "this+108", stride 56); the
    // ASM is unambiguous: `addi r29, r20, 0x1B0` (= &maWheels[0].mPosition) and `addi r29, r29,
    // 0xE0` -- the standard wheel stride. Decoded from the asm:
    //
    //   0x82602CCC  best = splat(1000.0 [flt_82009E10]); bestPoint = 0; v125 = mTransform.Up
    //               (+0x20); mbAnyWheelsDetatched (+0x715) = 0
    //   0x82602D48  per wheel (0..3): mu8State == 2 -> EARLY OUT: mbMinWheelDistValid = 0,
    //               mbAnyWheelsDetatched = 1, return (one detached wheel aborts the whole fit)
    //   0x82602D54  copy the 48-byte RoadContact to the stack (the ld/std x6 loop), then:
    //               [mbLineTestIsValid +0x2B] -> the wheel VOTES (r14 latches 1):
    //                 [mbIsOnGround +0x28]  best = 0, bestPoint = contact.mPosition
    //                 else: NaN sweep of wheel.mPosition (the same "Invalid wheel position: "
    //                       << pos << ", please tell Graham D." stream, Wheel.h:412 == 0x19C);
    //                       localBottom = mPosition with .y -= mSlipVariables.w (radius --
    //                       vrlimi mask 4 == lane y); worldBottom = mTransform * localBottom;
    //                       h = dot3(worldBottom - contact.mPosition, Up);
    //                       h <= best (the vnot'd vcmpgtfp + two vsel) -> best = h,
    //                       bestPoint = contact.mPosition
    //   0x82602F14  mWheelPlanePosAndHeight = {bestPoint.xyz, w = max(best, 0)} (two stvx: the
    //               first preserves the old .w, the second inserts the vmaxfp'd height --
    //               vrlimi mask 1 == lane w); mbMinWheelDistValid (+0x714) = the vote latch
    void SimpleVehiclePhysics::CalculateNewWheelPlane()
    {
        static const f32 KF_NO_CONTACT_HEIGHT = 1000.0f;   // flt_82009E10

        const Vector3& lvUp = mTransform.yAxis;   // v125 = lvx this+0x20

        f32     lfBestHeight = KF_NO_CONTACT_HEIGHT;   // v127
        Vector3 lvBestPoint{ 0.0f, 0.0f, 0.0f, 0.0f }; // v126
        bool    lbAnyValid = false;                    // r14

        mbAnyWheelsDetatched = false;   // stb 0 -> +0x715 (before the loop)

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];

            if (lrWheel.mu8State == 2)   // a detached wheel aborts the whole plane fit
            {
                mbMinWheelDistValid  = false;   // stb 0 -> +0x714
                mbAnyWheelsDetatched = true;    // stb 1 -> +0x715
                return;
            }

            const Wheel::RoadContact& lrContact = lrWheel.GetRoadContact();   // the 48-byte copy
            if (!lrContact.mbLineTestIsValid)
                continue;

            lbAnyValid = true;

            if (lrContact.mbIsOnGround)
            {
                lfBestHeight = 0.0f;                  // vmr128 v127, v124 (zero)
                lvBestPoint  = lrContact.mPosition;   // lvx of the stack copy
                continue;
            }

            // close-but-not-touching: how high does this wheel's lowest point sit above its
            // contact, along the body up axis?
            CGS_ASSERT(lrWheel.mPosition.x == lrWheel.mPosition.x &&
                       lrWheel.mPosition.y == lrWheel.mPosition.y &&
                       lrWheel.mPosition.z == lrWheel.mPosition.z,
                       "Invalid wheel position: , please tell Graham D.");   // Wheel.h:412

            // localBottom = wheel.mPosition with the y lane dropped by the radius
            // (mSlipVariables.w) -- the vsubfp + vrlimi(mask 4) pair.
            const Vector3 lvLocalBottom{ lrWheel.mPosition.x,
                                         lrWheel.mPosition.y - lrWheel.mSlipVariables.w,
                                         lrWheel.mPosition.z, 0.0f };

            // worldBottom = mTransform * localBottom (the same vmaddfp cascade as
            // AddTractionPoint above).
            const Vector3 lvWorldBottom{
                mTransform.xAxis.x * lvLocalBottom.x + mTransform.yAxis.x * lvLocalBottom.y
                    + mTransform.zAxis.x * lvLocalBottom.z + mTransform.wAxis.x,
                mTransform.xAxis.y * lvLocalBottom.x + mTransform.yAxis.y * lvLocalBottom.y
                    + mTransform.zAxis.y * lvLocalBottom.z + mTransform.wAxis.y,
                mTransform.xAxis.z * lvLocalBottom.x + mTransform.yAxis.z * lvLocalBottom.y
                    + mTransform.zAxis.z * lvLocalBottom.z + mTransform.wAxis.z,
                0.0f };

            const f32 lfHeight =
                (lvWorldBottom.x - lrContact.mPosition.x) * lvUp.x +
                (lvWorldBottom.y - lrContact.mPosition.y) * lvUp.y +
                (lvWorldBottom.z - lrContact.mPosition.z) * lvUp.z;   // vmsum3fp128 vs v125

            if (!(lfHeight > lfBestHeight))   // the vnot'd vcmpgtfp: keep the MINIMUM
            {
                lfBestHeight = lfHeight;
                lvBestPoint  = lrContact.mPosition;
            }
        }

        // publish: {bestPoint.xyz, w = max(best, 0)} -- the two-store vrlimi sequence.
        mWheelPlanePosAndHeight.SetVector3(lvBestPoint);
        mWheelPlanePosAndHeight.SetPlus((lfBestHeight > 0.0f) ? lfBestHeight : 0.0f);   // vmaxfp 0
        mbMinWheelDistValid = lbAnyValid;   // stb r14 -> +0x714
    }

    // ===========================================================================================
    //  SimpleVehiclePhysics::SwitchAttribs   @0x82601978   (458 insns)
    // ===========================================================================================
    // ⭐ BODIED 2026-08-09 (attribs-setup wave) -- the "BLOCKED on the 240-byte
    // SimpleVehicleAttribs" stub is retired: the full type now lives in the header. Decoded
    // store-for-store from the X360 asm. Four phases:
    //   1. mfMass = splat(new attribs Mass), asserted positive        (0x826019B0..0x82601A18)
    //   2. mLocalInverseInertia = the solid-box diagonal inverse tensor from
    //      lBoxExtent = 2 * mHalfExtent (flt_82001D9C == 2.0, flt_82094724 == 1/12), with
    //      IsValid + per-axis positivity asserts on the extent        (0x82601A1C..0x82601F10)
    //   3. per-wheel Wheel::SwitchAttribs with a POSITION DELTA built against the OLD
    //      mSimpleAttribs (COM delta + per-axle height-offset delta), the wheel's own current
    //      radius, the .data integration seed, and the new suspension travel bounds
    //                                                                 (0x82601F14..0x82602074)
    //   4. mSimpleAttribs.SetupAttribs(lpAttribs) -- the old set is only replaced AFTER the
    //      deltas were computed from it                               (0x82602078..0x82602080)
    //
    // The X360 computes the three tensor diagonals with vrefp + two Newton refines; the host
    // spells the same quantity as a division (established convention, see UpdateWheels).
    void SimpleVehiclePhysics::SwitchAttribs(VehicleAttribs* lpAttribs)
    {
        // .data scalars (static-init'd BSS, zero in the image; writers found in the
        // 0x82C5Cxxx initializer bank and their .rdata sources image-read, x360rd 10/10):
        //   unk_82FB8BB0 <- flt_82004F5C == 30.0   (writer @0x82C5D190)
        //   unk_82FB8440 <- flt_8209AE88 == 0.025  (writer @0x82C5D1B8)
        static const f32 KF_WHEEL_INTEGRATION_SEED     = 30.0f;
        static const f32 KF_MIN_SUSPENSION_TRAVEL_DOWN = 0.025f;
        static const f32 KF_ONE_TWELFTH = 0.0833333358f;   // flt_82094724 (the box-tensor 1/12)

        // ---- phase 1: the mass ----------------------------------------------------------------
        const f32 lfMass = lpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;
        mfMass = VecFloat{ lfMass, lfMass, lfMass, lfMass };   // stvx128 -> +0xE0 (base mfMass)
        CGS_ASSERT(lfMass > 0.0f, "mfMass > 0.0f");            // console line 0xA5

        // ---- phase 2: the solid-box inverse inertia --------------------------------------------
        const Vector3 lBoxExtent = vpu::Mult(mHalfExtent, 2.0f);   // flt_82001D9C
        CGS_ASSERT(vpu::IsValid(lBoxExtent), "RwMath::IsValid( lBoxExtent )");   // 0xAB
        CGS_ASSERT(lBoxExtent.x > 0.0f, "lBoxExtent.X() > 0.0f");                // 0xAD
        CGS_ASSERT(lBoxExtent.y > 0.0f, "lBoxExtent.Y() > 0.0f");                // 0xAE
        CGS_ASSERT(lBoxExtent.z > 0.0f, "lBoxExtent.Z() > 0.0f");                // 0xAF

        {
            const f32 lfMassOver12 = lfMass * KF_ONE_TWELFTH;   // v126 = splat(1/12) * splat(mass)
            const f32 lfX2 = lBoxExtent.x * lBoxExtent.x;
            const f32 lfY2 = lBoxExtent.y * lBoxExtent.y;
            const f32 lfZ2 = lBoxExtent.z * lBoxExtent.z;
            // vrefp + two Newton refines on each -> the diagonal inverse tensor.
            const f32 lfIxx = 1.0f / (lfMassOver12 * (lfY2 + lfZ2));
            const f32 lfIyy = 1.0f / (lfMassOver12 * (lfX2 + lfZ2));
            const f32 lfIzz = 1.0f / (lfMassOver12 * (lfX2 + lfY2));
            mLocalInverseInertia.xAxis = Vector3{ lfIxx, 0.0f, 0.0f, 0.0f };   // -> +0x80
            mLocalInverseInertia.yAxis = Vector3{ 0.0f, lfIyy, 0.0f, 0.0f };   // -> +0x90
            mLocalInverseInertia.zAxis = Vector3{ 0.0f, 0.0f, lfIzz, 0.0f };   // -> +0xA0
        }
        CGS_ASSERT(vpu::IsValid(mLocalInverseInertia.xAxis) &&
                   vpu::IsValid(mLocalInverseInertia.yAxis) &&
                   vpu::IsValid(mLocalInverseInertia.zAxis),
                   "RwMath::IsValid( mLocalInverseInertia )");                   // 0xB9

        // ---- phase 3: the per-wheel re-seat, deltas computed against the OLD simple set --------
        // COM delta: old - new (`vsubfp v0, [this+0x670], [attribs+0x20]`).
        const Vector3 lvCOMDelta =
            vpu::Subtract(mSimpleAttribs.mCOMOffset, lpAttribs->mBaseAttribs.mCOMOffset);

        // Per-axle height-offset deltas: new (VehicleAttribs suspension lanes x/y) minus old
        // (the CURRENT mSimpleAttribs lanes z/w) -- read BEFORE SetupAttribs overwrites them.
        const f32 lfFrontHeightDelta =
            lpAttribs->mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.x
            - mSimpleAttribs.mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.z;
        const f32 lfRearHeightDelta =
            lpAttribs->mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.y
            - mSimpleAttribs.mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.w;

        // The new suspension travel bounds; travel-down floored at the .data 0.025 (vmaxfp).
        const f32 lfTravelUp = lpAttribs->mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.z;
        const f32 lfTravelDownRaw = lpAttribs->mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.w;
        const f32 lfTravelDown = (lfTravelDownRaw > KF_MIN_SUSPENSION_TRAVEL_DOWN)
                                     ? lfTravelDownRaw : KF_MIN_SUSPENSION_TRAVEL_DOWN;

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            // Front pair gets the front height delta + front tire attribs; rear pair the rear's
            // (the caller's stacked pointer table var_110..var_104: 2D0, 2D0, 310, 310).
            const bool lbFront = (liWheel < 2);
            Vector3 lvDelta = lvCOMDelta;
            lvDelta.y += lbFront ? lfFrontHeightDelta : lfRearHeightDelta;   // vrlimi(4)

            maWheels[liWheel].SwitchAttribs(
                lvDelta,
                maWheels[liWheel].mSlipVariables.w,   // the wheel's CURRENT radius, preserved
                KF_WHEEL_INTEGRATION_SEED,
                lfTravelUp,
                lfTravelDown,
                lbFront ? &lpAttribs->mFrontTireAttribs : &lpAttribs->mRearTireAttribs);
        }

        // ---- phase 4: replace the simple set ---------------------------------------------------
        mSimpleAttribs.SetupAttribs(lpAttribs);   // this+0x5A0, the LAST thing the function does
    }
}
}
