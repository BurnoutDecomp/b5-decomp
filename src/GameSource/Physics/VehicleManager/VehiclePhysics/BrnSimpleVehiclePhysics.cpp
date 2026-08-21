#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"            // KVF_MAX_BUCKLE_ANGLE_CRASHING / KAVF_WHEEL_TWIST_DIRECTIONS (crash arm)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"  // the full VehicleAttribs (SwitchAttribs reads its base/suspension lanes)
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"           // the car-asset wrapper (SetAttributes' key chase)
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"    // the handling wrapper + its checked copy ctor
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"              // CgsGeometric::Box::Set

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include <cmath>                                        // std::sqrt (AddTractionPoint's line distance)
#include "rw/math/vpu/vector3_operation.h"             // rw::math::vpu::{IsValid, operator+/-, Mult, Dot}
#include "rw/math/vpu/matrix44affine_operation.h"      // rw::math::vpu::IsValid(Matrix44Affine) (Prepare's :98 assert)

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

    // m/s -> MPH. The console constant is flt_830180B0, whose static initialiser is a DIVISION
    // (0x82C6D0C0: f0 = flt_82001C98 == 1.0f, f13 = flt_82F31928 == 0.447039992f, `fdivs`), i.e.
    // 1 / 0.44704 == 2.2369363f. VehiclePhysics.cpp homes the same value under KF_MPS_TO_MPH with
    // that derivation written out; it is TU-static there, so the same literal is re-stated here
    // rather than reached across a translation unit. Prepare @0x8262FC10 loads it by symbol.
    static const f32 KF_SVP_MPS_TO_MPH = 2.2369363f;   // flt_830180B0 == 1/flt_82F31928

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

    // ⭐ The two GetTractionLine @0x825D85C0 scalars, READ OUT OF THE X360 IMAGE at the addresses
    // its own `lfs` instructions name (x360rd, 10/10 self-calibration) rather than guessed:
    //   0x825D87D4  lfs f0, 7320(0x82000000)   -> *0x82001C98 == 1.0f
    //   0x825D87E8  lfs f0, 18236(0x82000000)  -> *0x8200473C == 0.4f
    // Descriptive names: the console gives them none (they are anonymous .rdata floats), and the
    // roles are unambiguous from the two expressions they enter.
    static const f32 KF_TRACTION_LINE_EXTRA_LENGTH = 1.0f;   // added to the probe's reach
    static const f32 KF_TRACTION_LINE_START_LIFT   = 0.4f;   // start point, up the body Y axis

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
    //   mHandlingBodyOffset(+1680)/mHalfExtent(+1696), reset mAboveGroundTestResult, then run
    //   the source-level zero-argument Reset wrapper (Vector3 zero reset + frozen clear).
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::Construct()
    {
        // The X360 calls the DIRECT base's Construct on the base subobject (`this+16`):
        // `bl BrnPhysics__ExternalPhysicsBody__Construct`.
        ExternalPhysicsBody::Construct();
        for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            maWheels[liWheel].Clear();
        mSimpleAttribs.Construct();

        mHandlingBodyOffset = KV_ZERO;                           // +0x690
        mHalfExtent         = KV_ZERO;                           // +0x6A0

        // 0x82620454..0x8262047C initialises the above-ground result at +0x570.  These are not
        // AABB stores: +0x590/+0x594/+0x596/+0x598 are the distance, tag halves and valid byte.
        mAboveGroundTestResult.Reset();
        Reset();
    }

    // -------------------------------------------------------------------------------------------
    // Destruct  @0x826206D0
    //   base Destruct, the same Wheel::Clear loop, Reset, *(+112)=0.
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::Destruct()
    {
        ExternalPhysicsBody::Destruct();
        for (int liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            maWheels[liWheel].Clear();
        Reset();
    }

    // -------------------------------------------------------------------------------------------
    // Reset(Vector3)  @0x825D9A58
    //   if !mSimpleAttribs.IsValid() (`lbz this+0x684`, exactly +0x5A0+0xE4) Wheel::Reset each
    //   wheel; then zero maLocalTractionPoints[0..3]
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
    void SimpleVehiclePhysics::Reset(Vector3 lInitialVelocity)
    {
        // The parameter is part of the DecFIGS-mangled source signature and Breaker ABI, but is
        // dead in this build: @0x825D9A80 constructs its own zero before every Wheel::Reset.
        (void)lInitialVelocity;

        if (!mSimpleAttribs.IsValid())                  // lbz this+0x684
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

    // DecFIGS @0x6D8824.  Breaker inlines this wrapper in Construct @0x82620430..47C and
    // Destruct @0x8262070C..718: splat zero into v1, call @0x825D9A58, clear +0x70.
    void SimpleVehiclePhysics::Reset()
    {
        Reset(KV_ZERO);
        SetFrozen(false);
    }

    // DecFIGS @0x6B4400 is an emitted empty body. UpdatePostSimulation is deliberately
    // non-virtual; full-physics callers select the VehiclePhysics implementation statically.
    void SimpleVehiclePhysics::UpdatePostSimulation(VecFloat lvfTimeStep)
    {
        (void)lvfTimeStep;
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

    // The base vtable's first slot is the ICF-folded zero-return implementation.  DecFIGS names
    // it at BrnSimpleVehiclePhysics.cpp:768; VehiclePhysics overrides it with @0x825D4028.
    VecFloat SimpleVehiclePhysics::GetSteeringAngle() const
    {
        return VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };
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
    // SetCrashing @0x825D98F0.  Arm the two car-level latches, clear each wheel's accumulated
    // torque/broken-adhesive state and match its angular speed to the body's forward speed.
    // -------------------------------------------------------------------------------------------
    void SimpleVehiclePhysics::SetCrashing()
    {
        mbCrashing         = true;    // +0x710
        mbCrashedThisFrame = true;    // +0x713

        const f32 lfForwardSpeed = vpu::Dot(mLinearVelocity, mTransform.zAxis);
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];
            lrWheel.mIntegrationVariables.y = 0.0f; // wheel+0x30.y
            lrWheel.mIntegrationVariables.x =
                lfForwardSpeed / lrWheel.mSlipVariables.w; // wheel angular velocity = v/r
            lrWheel.mbBrokenAdhesiveLimit = false;  // wheel+0xD5
        }
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
    //   0x825D98AC  lbIsCloseToGround = (same sum + the +0x5A0 register's .w lane) > lineDist
    //
    // ⛔⛔ CORRECTED 2026-08-11 (lifetime wave) -- A LIVE STALE-MEMBER BUG, not a comment fix.
    // This body used to spell that .w lane `mSimpleAttribs.mCOMOffset.w`, and the note here said
    // "the attribs block's leading vector; the committed slice names it mCOMOffset". That was
    // true when it was written: SimpleVehicleAttribs was then a 20-byte slice whose LEADING
    // member was mCOMOffset. The attribs-setup wave (2026-08-09) grew the type to the full
    // 240-byte DWARF layout and moved mCOMOffset to +0xD0 -- so from that day this line has been
    // reading `this + 0x5A0 + 0xD0 + 12`, **208 bytes past the seat the console reads**. It
    // compiled, linked and ran; the member still existed and was still a Vector3, so nothing
    // could catch it.
    // The console is unambiguous (0x825D9880 `lvx128 v13, r0, r10` with r10 = this+0x5A0, then
    // 0x825D988C `vspltw v13, v13, 3`): it is the LEADING vector's .w, which the grown type
    // names `mvUpwardMovement_DownwardMovement_Mass_TractionLineLength` -- i.e. the attribute is
    // literally TractionLineLength. ⭐ Independently corroborated by
    // SimpleVehiclePhysics::GetTractionLine @0x825D85C0 (bodied this wave), which adds the SAME
    // lane to the SAME three-term reach to size the suspension probe it shoots.
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
        const bool lbIsCloseToGround =
            (lfReach +
             mSimpleAttribs.mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.w)
            > lfLineDist;

        lrWheel.SetRoadContact(lbIsOnGround, lbIsCloseToGround, lvPosition, lvNormal,
                               lu16TagHi, lu16TagLo, lfLineDist);
    }

    // ===========================================================================================
    //  SimpleVehiclePhysics::GetTractionLine   @0x825D85C0  (174 insns)   ⭐⭐ THE SUSPENSION PROBE
    // ===========================================================================================
    // BODIED 2026-08-11 (lifetime wave). One wheel's downward traction line, in WORLD space --
    // the two points VehicleManager::AddRaceCarTractionLineTests drops into the stream command's
    // maLineStart[w] / maLineEnd[w].
    //
    // ⚠️ EXPORT HOLE: no per-function JSON in the 30,084-file X360 set (directory gap
    // 0x825D8490+76 -> 0x825D8878), re-verified this wave by a name index over all of them.
    // Recovered TWO ways that agree, neither of them guesswork:
    //
    //  (1) THE IMAGE BYTES, 0x825D85C0..0x825D8874, read with x360rd (10/10 calibration) and
    //      decoded with a VMX128 decoder FITTED AND SELF-TESTED against an exported twin
    //      (VehicleManager::UpdateTriangleCache @0x82615C38 carries full VMX128 mnemonics in its
    //      export, so its 24 vector instructions are ground truth for the field layout:
    //      VD128 = D|(bits28..29<<5), VA128 = A|(bit26<<5)|(bit21<<6), VB128 = B|(bits30..31<<5);
    //      op5 opcode bits {22,23,24,25,27}: 0x010 vaddfp128 / 0x050 vsubfp128 / 0x090 vmulfp128 /
    //      0x0D0 vmaddfp128 / 0x150 vnmsubfp128 / 0x190 vmsum3fp128 / 0x2D0 vor128).
    //      ⚠️⚠️ AND THE TRAP THAT COMES WITH IT, because it changes the arithmetic: for the op-4
    //      A-form IDA prints operands in ENCODING order vD,vA,vB,vC, not the assembler's
    //      vD,vA,vC,vB. `vmaddfp v13, v7, v13, v6` (0x11A769AE: A=7,B=13,C=6) is
    //      v13 = v7*v6 + v13, NOT v7*v13 + v6. Read the natural way it turns the standard
    //      vrsqrtefp Newton-Raphson refinement into nonsense -- which is how it was caught.
    //
    //  (2) THE PS3 EXPORT @0x6E894C (251 insns), which supplies the signature the X360 image
    //      cannot: `_ZNK..SimpleVehiclePhysics15GetTractionLineENS0_19EVehicleDrivenWheelE
    //      RN2rw4math3vpu7Vector3ES7_` -- CONST, (wheel, Vector3& lOutSusLineStart,
    //      Vector3& lOutSusLineEnd), and the parameter names are its own. Its member offsets are
    //      the X360's byte for byte (maWheels +0x130 stride 0xE0, mTransform +0x10,
    //      mSimpleAttribs +0x5A0 with the IsValid byte at +0xE4 inside it).
    //
    // ⭐⭐ THE PROBE LENGTH IS CORROBORATED BY A DIFFERENT CONSOLE FUNCTION ALREADY IN THIS TREE.
    // AddTractionPoint above (bodied 2026-08-07 from @0x825D9608, an unrelated body) computes
    // exactly `mPosition.y - mSuspensionAndInertiaVariables.x + mSlipVariables.w` and then adds
    // the same mSimpleAttribs leading-vector .w lane for its close-to-ground test. This function
    // adds that same sum, that same lane, and one metre. Two console functions agreeing on a
    // four-term expression, with the attribute's own generated name reading
    // ...Mass_TractionLineLength, is as strong as recovery gets here.
    //
    // BOTH FLOAT CONSTANTS READ OUT OF THE IMAGE, NOT GUESSED:
    //   0x825D87D4  lfs f0, 0x82001C98   ==  1.0f   -> added to the probe length
    //   0x825D87E8  lfs f0, 0x8200473C   ==  0.4f   -> the start point's lift up the body Y axis
    // ⚠️ FLAG -- A BUILD DIVERGENCE, reproduced as the X360 has it: the PS3 body uses 0.4f for
    // BOTH terms (it materialises one stack vector, 0x3ECCCCCD in all lanes, and uses it twice).
    // The X360 loads two DIFFERENT .rdata floats. The X360 image is the reconstruction target.
    //
    // AS SHIPPED, all four lanes travel: the console loads/stores with lvx128/stvx128 and this
    // tree's Vector3 is the same 16-byte four-lane register, so the w lane is carried rather
    // than dropped (it falls out as mTransform.wAxis.w, exactly as on the console).
    void SimpleVehiclePhysics::GetTractionLine(EVehicleDrivenWheel leWheel,
                                               Vector3& lOutSusLineStart,
                                               Vector3& lOutSusLineEnd) const
    {
        // 0x825D85E8 / 0x825D8610 -- the two range tripwires, baked lines 501 and 502.
        CGS_ASSERT(leWheel >= eFrontLeftWheel,  "leWheel >= eFrontLeftWheel");     // :501
        CGS_ASSERT(leWheel <  eNumDrivenWheels, "leWheel < eNumDrivenWheels");     // :502

        const Wheel& lrWheel = maWheels[leWheel];

        // 0x825D8650..0x825D86B8 -- the same three-lane NaN sweep AddTractionPoint runs, with
        // the same console-authored message (Wheel.h:368).
        CGS_ASSERT(lrWheel.mPosition.x == lrWheel.mPosition.x &&
                   lrWheel.mPosition.y == lrWheel.mPosition.y &&
                   lrWheel.mPosition.z == lrWheel.mPosition.z,
                   "Invalid wheel position: , please tell Graham D.");

        // 0x825D8774 `lbz r9, 1668(r26)` == mSimpleAttribs (+0x5A0) + mbIsValid (+0xE4).
        CGS_ASSERT(GetSimpleAttribs()->IsValid(), "GetSimpleAttribs()->IsValid()");  // :511

        // 0x825D8778..0x825D8798 -- worldWheelPos = mTransform * wheel.mPosition. Same cascade
        // as AddTractionPoint (three rows scaled by the local lanes, seeded with the wAxis row).
        const Vector3 lvWorldWheelPos{
            mTransform.xAxis.x * lrWheel.mPosition.x + mTransform.yAxis.x * lrWheel.mPosition.y
                + mTransform.zAxis.x * lrWheel.mPosition.z + mTransform.wAxis.x,
            mTransform.xAxis.y * lrWheel.mPosition.x + mTransform.yAxis.y * lrWheel.mPosition.y
                + mTransform.zAxis.y * lrWheel.mPosition.z + mTransform.wAxis.y,
            mTransform.xAxis.z * lrWheel.mPosition.x + mTransform.yAxis.z * lrWheel.mPosition.y
                + mTransform.zAxis.z * lrWheel.mPosition.z + mTransform.wAxis.z,
            mTransform.xAxis.w * lrWheel.mPosition.x + mTransform.yAxis.w * lrWheel.mPosition.y
                + mTransform.zAxis.w * lrWheel.mPosition.z + mTransform.wAxis.w };

        // 0x825D87CC..0x825D8854 -- the probe length: the wheel's own reach (hub height above
        // its lowest suspension seat, plus the tyre radius), plus the car's TractionLineLength
        // attribute, plus one metre of margin.
        const f32 lfProbeLength =
            (lrWheel.mPosition.y
             - lrWheel.mSuspensionAndInertiaVariables.x
             + lrWheel.mSlipVariables.w)
            + mSimpleAttribs.mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.w
            + KF_TRACTION_LINE_EXTRA_LENGTH;

        // 0x825D8850 `vmaddfp128 v8, v0, v9` : start = worldWheelPos + yAxis * 0.4
        lOutSusLineStart.x = lvWorldWheelPos.x + mTransform.yAxis.x * KF_TRACTION_LINE_START_LIFT;
        lOutSusLineStart.y = lvWorldWheelPos.y + mTransform.yAxis.y * KF_TRACTION_LINE_START_LIFT;
        lOutSusLineStart.z = lvWorldWheelPos.z + mTransform.yAxis.z * KF_TRACTION_LINE_START_LIFT;
        lOutSusLineStart.w = lvWorldWheelPos.w + mTransform.yAxis.w * KF_TRACTION_LINE_START_LIFT;

        // 0x825D885C/0x825D8860 `vmulfp128` + `vsubfp128` : end = worldWheelPos - yAxis * length
        lOutSusLineEnd.x = lvWorldWheelPos.x - mTransform.yAxis.x * lfProbeLength;
        lOutSusLineEnd.y = lvWorldWheelPos.y - mTransform.yAxis.y * lfProbeLength;
        lOutSusLineEnd.z = lvWorldWheelPos.z - mTransform.yAxis.z * lfProbeLength;
        lOutSusLineEnd.w = lvWorldWheelPos.w - mTransform.yAxis.w * lfProbeLength;
    }

    // ===========================================================================================
    //  SimpleVehiclePhysics::GetWheelsWorldTransfrom   @0x825D8878  (868 insns)
    // ===========================================================================================
    // ⭐⭐ BODIED 2026-08-13 (wheel-transform wave) from the operand-level decode bank
    // (scratchpad wheeltransform_bank.md -- X360 export 0x825D8878 + PS3 twin 0x6E78CC + image
    // reads for every constant). This is the reader that turns wheel physics state into the four
    // render transforms: WriteOutVehicleStats' SetWheelTransform loop consumes it once per wheel.
    //
    // Composition (row-vector convention, v' = v * M -- proven by the compose at 0x825D9490 and
    // the body multiply at 0x825D94F0):
    //   local = mbCrashing ? RotZ(buckle) * RotX(spin) * RotY(twist)          (steer NEVER runs)
    //                      : RotX(spin) [ * RotY(GetSteeringAngle()) iff front wheel ]
    //   out   = local * mTransform;   left wheels (0/2) mirrored pi-about-Y when the bool is
    //   false (rows X and Z negated, 0x825D95AC);  out.wAxis = mTransform.wAxis +
    //   RotateVector(wheel.mPosition, mTransform)  (0x825D95C4..0x825D95F4).
    //
    // The console does all five rotation builds through one shared VMX SinCos kernel
    // (range-reduce + 12-term Taylor pair, coefficients at 0x82000BD0..0x82000C2F, read from the
    // image); SvpSinCos below is that kernel de-SIMD'd with the SAME coefficients (the committed
    // rw::math::vpu tree flags SinCos as not-committed, so the kernel lives here TU-static).
    // ===========================================================================================

    // The exact sin/cos Taylor coefficient rows the X360 kernel loads (bank §5.1; each value
    // re-checked as the correctly-rounded 1/n! -- e.g. 0x3638EF1D == 2.7557319e-06f == 1/9!).
    //   sin: 0x82000BD0/BE0/BF0     cos: 0x82000C00/C10/C20
    static const f32 KAF_SVP_SIN_COEFFS[12] =
    {
        1.0f,            -1.66666672e-1f,  8.33333377e-3f,  -1.98412701e-4f,   // 1, -1/3!, 1/5!, -1/7!
        2.75573188e-6f,  -2.50521080e-8f,  1.60590438e-10f, -7.64716373e-13f,  // 1/9! .. -1/15!
        2.81145725e-15f, -8.22063525e-18f, 1.95729411e-20f, -3.86817017e-23f   // 1/17! .. -1/23!
    };
    static const f32 KAF_SVP_COS_COEFFS[12] =
    {
        1.0f,            -5.0e-1f,         4.16666679e-2f,  -1.38888892e-3f,   // 1, -1/2!, 1/4!, -1/6!
        2.48015876e-5f,  -2.75573188e-7f,  2.08767570e-9f,  -1.14707456e-11f,  // 1/8! .. -1/14!
        4.77947733e-14f, -1.56192070e-16f, 4.11031762e-19f, -8.89679139e-22f   // 1/16! .. -1/22!
    };
    static const f32 KF_SVP_TWO_PI     = 6.28318548f;   // 0x40C90FDB @0x82000C64
    static const f32 KF_SVP_INV_TWO_PI = 0.159154937f;  // 0x3E22F983 @0x82000C6C

    // The shared SinCos kernel: r = angle - 2pi*round(angle/2pi) (the console's vrfin
    // round-to-nearest; floor(x+0.5) here -- differs only at exact .5 ties on an already-inexact
    // angle), then the two 12-term series over r^2 (the console's splat-and-madd power lattice,
    // evaluated Horner-wise here -- same terms, float-rounding-equivalent order).
    static void SvpSinCos(f32 lfAngle, f32& lrfSin, f32& lrfCos)
    {
        const f32 lfR  = lfAngle
                       - KF_SVP_TWO_PI * std::floor(lfAngle * KF_SVP_INV_TWO_PI + 0.5f);
        const f32 lfR2 = lfR * lfR;

        f32 lfSinPoly = KAF_SVP_SIN_COEFFS[11];
        f32 lfCosPoly = KAF_SVP_COS_COEFFS[11];
        for (s32 li = 10; li >= 0; --li)
        {
            lfSinPoly = lfSinPoly * lfR2 + KAF_SVP_SIN_COEFFS[li];
            lfCosPoly = lfCosPoly * lfR2 + KAF_SVP_COS_COEFFS[li];
        }
        lrfSin = lfR * lfSinPoly;
        lrfCos = lfCosPoly;
    }

    // ------------------------------------------------------------------------------------------
    // ⭐ THE ONE PLACE THE ROTATION HANDEDNESS IS DECIDED (bank §7.1). The asm proves, per
    // build, WHICH row is the untouched axis row and that one polynomial output is sign-flipped
    // into an off-diagonal -- but not which output is sine. All five rotation builds in
    // GetWheelsWorldTransfrom route through these three builders so the sign convention is a
    // single decision, checked VISUALLY (boot witness, 2026-08-13): under `-Drive` the wheels
    // must roll FORWARD (top of wheel toward the nose = local +z), under `-Steer right` the
    // front wheels must yaw RIGHT (nose of wheel toward local +x).
    //
    // Convention as written (row-vector, x=right / y=up / z=forward):
    //   RotX(+a): (0,1,0) -> (0, c, +s): top of wheel moves FORWARD for +a.
    //   RotY(+a): (0,0,1) -> (+s, 0, c): wheel nose moves RIGHT for +a.
    //   RotZ(+a): (1,0,0) -> (c, +s, 0): right side of wheel moves UP for +a (crash buckle).
    // ------------------------------------------------------------------------------------------
    static Matrix44Affine SvpRotationAboutX(f32 lfAngle)
    {
        f32 lfSin, lfCos;
        SvpSinCos(lfAngle, lfSin, lfCos);
        Matrix44Affine lM;
        lM.xAxis = { 1.0f,   0.0f,  0.0f, 0.0f };   // 0x825D92B4: X row untouched [V]
        lM.yAxis = { 0.0f,  lfCos, lfSin, 0.0f };
        lM.zAxis = { 0.0f, -lfSin, lfCos, 0.0f };
        lM.wAxis = { 0.0f,   0.0f,  0.0f, 1.0f };
        return lM;
    }

    static Matrix44Affine SvpRotationAboutY(f32 lfAngle)
    {
        f32 lfSin, lfCos;
        SvpSinCos(lfAngle, lfSin, lfCos);
        Matrix44Affine lM;
        lM.xAxis = { lfCos, 0.0f, -lfSin, 0.0f };
        lM.yAxis = {  0.0f, 1.0f,   0.0f, 0.0f };   // 0x825D9460: Y row untouched [V]
        lM.zAxis = { lfSin, 0.0f,  lfCos, 0.0f };
        lM.wAxis = {  0.0f, 0.0f,   0.0f, 1.0f };
        return lM;
    }

    static Matrix44Affine SvpRotationAboutZ(f32 lfAngle)
    {
        f32 lfSin, lfCos;
        SvpSinCos(lfAngle, lfSin, lfCos);
        Matrix44Affine lM;
        lM.xAxis = {  lfCos, lfSin, 0.0f, 0.0f };
        lM.yAxis = { -lfSin, lfCos, 0.0f, 0.0f };
        lM.zAxis = {   0.0f,  0.0f, 1.0f, 0.0f };   // 0x825D8CFC: Z row untouched [V]
        lM.wAxis = {   0.0f,  0.0f, 0.0f, 1.0f };
        return lM;
    }

    // Row-vector rotation compose, C = A * B (out row_i = A_i.x*B.x + A_i.y*B.y + A_i.z*B.z) --
    // the console's splat-and-madd lattice at 0x825D9490 (operand-verified there; the two
    // crash-path composes are the identical shape). Rotation-only: wAxis is not composed.
    static Matrix44Affine SvpMulRotation(const Matrix44Affine& lrA, const Matrix44Affine& lrB)
    {
        Matrix44Affine lOut;
        const Vector3* lpaARows[3] = { &lrA.xAxis, &lrA.yAxis, &lrA.zAxis };
        Vector3*       lpaORows[3] = { &lOut.xAxis, &lOut.yAxis, &lOut.zAxis };
        for (s32 li = 0; li < 3; ++li)
        {
            const Vector3& lrRow = *lpaARows[li];
            lpaORows[li]->x = lrRow.x * lrB.xAxis.x + lrRow.y * lrB.yAxis.x + lrRow.z * lrB.zAxis.x;
            lpaORows[li]->y = lrRow.x * lrB.xAxis.y + lrRow.y * lrB.yAxis.y + lrRow.z * lrB.zAxis.y;
            lpaORows[li]->z = lrRow.x * lrB.xAxis.z + lrRow.y * lrB.yAxis.z + lrRow.z * lrB.zAxis.z;
            lpaORows[li]->w = 0.0f;
        }
        lOut.wAxis = { 0.0f, 0.0f, 0.0f, 1.0f };
        return lOut;
    }

    Matrix44Affine SimpleVehiclePhysics::GetWheelsWorldTransfrom(
        EVehicleDrivenWheel leWheel, bool lbHackDontReverseRightWheels) const
    {
        const Wheel& lrWheel = maWheels[leWheel];

        // 0x825D88B8..0x825D8924 -- Wheel::GetPosition() inlined: the three-lane NaN
        // self-compare with the console-authored message (Wheel.h:412, `li r5, 0x19C`).
        CGS_ASSERT(lrWheel.mPosition.x == lrWheel.mPosition.x &&
                   lrWheel.mPosition.y == lrWheel.mPosition.y &&
                   lrWheel.mPosition.z == lrWheel.mPosition.z,
                   "Invalid wheel position: , please tell Graham D.");

        // 0x825D89C8/0x825D89D8 -- the spin angle: mIntegrationVariables lane .z (the
        // accumulated wheel rotation the drivetrain integrates at 0x8261F494).
        const f32 lfSpinAngle = lrWheel.mIntegrationVariables.z;

        Matrix44Affine lLocal;
        if (mbCrashing)   // 0x825D89D4 `lbz r11, 0x710(r16)`
        {
            // 0x825D8A08..0x825D8A70 -- the console runs GetPosition() (and its NaN assert)
            // a SECOND time on this path.
            CGS_ASSERT(lrWheel.mPosition.x == lrWheel.mPosition.x &&
                       lrWheel.mPosition.y == lrWheel.mPosition.y &&
                       lrWheel.mPosition.z == lrWheel.mPosition.z,
                       "Invalid wheel position: , please tell Graham D.");

            // Buckle angle (0x825D8B10..0x825D8B9C): (2*|posZ - streamedZ|)^2, clamped.
            // ⚠️ The SQUARE is the angle in radians -- faithful on both platforms (bank §7.3);
            // it looks like a bug a porter would "fix". Do not.
            const f32 lfDeltaZ =
                lrWheel.mPosition.z - lrWheel.mStreamedPositionPlusTwistAmount.z;
            f32 lfBuckleAngle = 2.0f * ((lfDeltaZ >= 0.0f) ? lfDeltaZ : -lfDeltaZ);
            lfBuckleAngle = lfBuckleAngle * lfBuckleAngle;
            if (lfBuckleAngle > KVF_MAX_BUCKLE_ANGLE_CRASHING)         // vminfp @0x825D8B98
            {
                lfBuckleAngle = KVF_MAX_BUCKLE_ANGLE_CRASHING;
            }

            // Twist angle (0x825D8B30/0x825D8EEC): the streamed twist amount (.w lane) signed
            // by the per-wheel direction table (+1/-1/+1/-1).
            const f32 lfTwistAngle =
                lrWheel.mStreamedPositionPlusTwistAmount.w * KAVF_WHEEL_TWIST_DIRECTIONS[leWheel];

            // 0x825D8CFC (RotZ) -> 0x825D8E98 (RotX) -> 0x825D9034 (RotY), composed
            // buckle-first in row-vector order. Steering is NEVER applied while crashing
            // (0x825D90EC branches straight to the common body multiply).
            lLocal = SvpMulRotation(
                SvpMulRotation(SvpRotationAboutZ(lfBuckleAngle), SvpRotationAboutX(lfSpinAngle)),
                SvpRotationAboutY(lfTwistAngle));
        }
        else
        {
            // Normal path: spin about the axle (0x825D9298..0x825D92D0)...
            lLocal = SvpRotationAboutX(lfSpinAngle);

            // ...then, for the two FRONT wheels only (0x825D9254/0x825D92D8 -- REGARDLESS of
            // the bool argument), steer about up: spin FIRST, then steer (the compose at
            // 0x825D9490 splats the spin rows onto the steer rows).
            if (leWheel == eFrontLeftWheel || leWheel == eFrontRightWheel)
            {
                // The console dispatches vtable slot 0 and consumes the broadcast VecFloat.
                const f32 lfSteerAngle = GetSteeringAngle().x;
                lLocal = SvpMulRotation(lLocal, SvpRotationAboutY(lfSteerAngle));
            }
        }

        // 0x825D94F0..0x825D9594 -- out = local * mTransform (rows only; local row3 is zero, so
        // the translation row starts as mTransform.wAxis and is overwritten below anyway).
        Matrix44Affine lWheelTransform = SvpMulRotation(lLocal, mTransform);

        // 0x825D9560..0x825D95C0 -- the left-wheel mirror, gated by the bool: rows X and Z of
        // the WORLD matrix negated = a pi rotation about Y (the wheel model faces the other
        // side). Wheels 0/2 == FrontLeft/RearLeft per the DWARF enum (the parameter name says
        // "Right" -- console misnomer, kept as the ABI name). WriteOutVehicleStats passes
        // false, so the mirror is ACTIVE in the render publish.
        if (!lbHackDontReverseRightWheels &&
            (leWheel == eFrontLeftWheel || leWheel == eRearLeftWheel))
        {
            lWheelTransform.xAxis.x = -lWheelTransform.xAxis.x;
            lWheelTransform.xAxis.y = -lWheelTransform.xAxis.y;
            lWheelTransform.xAxis.z = -lWheelTransform.xAxis.z;
            lWheelTransform.xAxis.w = -lWheelTransform.xAxis.w;
            lWheelTransform.zAxis.x = -lWheelTransform.zAxis.x;
            lWheelTransform.zAxis.y = -lWheelTransform.zAxis.y;
            lWheelTransform.zAxis.z = -lWheelTransform.zAxis.z;
            lWheelTransform.zAxis.w = -lWheelTransform.zAxis.w;
        }

        // 0x825D95C4..0x825D95F4 -- translation: the body position plus the BODY-ROTATED wheel
        // position (mPosition is car-space with the suspension travel already integrated into
        // its .y by ApplyWheelWeight -- rotation-only transform, then add the wAxis row).
        lWheelTransform.wAxis.x = mTransform.wAxis.x
            + mTransform.xAxis.x * lrWheel.mPosition.x
            + mTransform.yAxis.x * lrWheel.mPosition.y
            + mTransform.zAxis.x * lrWheel.mPosition.z;
        lWheelTransform.wAxis.y = mTransform.wAxis.y
            + mTransform.xAxis.y * lrWheel.mPosition.x
            + mTransform.yAxis.y * lrWheel.mPosition.y
            + mTransform.zAxis.y * lrWheel.mPosition.z;
        lWheelTransform.wAxis.z = mTransform.wAxis.z
            + mTransform.xAxis.z * lrWheel.mPosition.x
            + mTransform.yAxis.z * lrWheel.mPosition.y
            + mTransform.zAxis.z * lrWheel.mPosition.z;
        lWheelTransform.wAxis.w = 1.0f;

        // PS3 DWARF names the sret local `lWheelTransform` -- kept.
        return lWheelTransform;
    }

    // ===========================================================================================
    // SimpleVehiclePhysics::GetSimpleVehicleBox @0x82602A20
    //
    // Build the local bounds from the streamed body half-extent and the bottom of every wheel,
    // move the graphics transform to the local bounds' centre, then publish an oriented box with
    // zero fatness.  This is the exact named-member form of the vminfp/vmaxfp loop in Breaker.
    // ===========================================================================================
    void SimpleVehiclePhysics::GetSimpleVehicleBox(CgsGeometric::Box& lrOutBox) const
    {
        Vector3 lvMin{ -mHalfExtent.x, -mHalfExtent.y, -mHalfExtent.z, 0.0f };
        Vector3 lvMax{  mHalfExtent.x,  mHalfExtent.y,  mHalfExtent.z, 0.0f };

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const Wheel& lrWheel = maWheels[liWheel];
            CGS_ASSERT(vpu::IsValid(lrWheel.mPosition),
                       "Invalid wheel position: , please tell Graham D.");

            // The wheel's lowest point in graphics/model space:
            //     COM offset + wheel centre - (0, radius, 0).
            const Vector3 lvWheelBottom{
                mSimpleAttribs.mCOMOffset.x + lrWheel.mPosition.x,
                mSimpleAttribs.mCOMOffset.y + lrWheel.mPosition.y - lrWheel.mSlipVariables.w,
                mSimpleAttribs.mCOMOffset.z + lrWheel.mPosition.z,
                0.0f
            };
            lvMin = vpu::Min(lvMin, lvWheelBottom);
            lvMax = vpu::Max(lvMax, lvWheelBottom);
        }

        const Vector3 lvCentre{
            (lvMin.x + lvMax.x) * 0.5f,
            (lvMin.y + lvMax.y) * 0.5f,
            (lvMin.z + lvMax.z) * 0.5f,
            0.0f
        };
        const Vector3 lvDimensions{
            lvMax.x - lvCentre.x,
            lvMax.y - lvCentre.y,
            lvMax.z - lvCentre.z,
            0.0f
        };

        Matrix44Affine lBoxTransform = GetGraphicsVehicleTransform();
        lBoxTransform.wAxis = vpu::Add(lBoxTransform.wAxis,
                                       RotateCOMOffsetToWorld(lBoxTransform, lvCentre));
        lBoxTransform.wAxis.w = 1.0f;
        lrOutBox.Set(lBoxTransform, lvDimensions, VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });
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

    // ===========================================================================================
    //  SimpleVehiclePhysics::SetAttributes()   @0x82620498   (142 insns)
    // ===========================================================================================
    // ⭐ BODIED 2026-08-09 (attribs-setup wave). This was the unnamed `sub_82620498`; identity
    // recovered from its caller set (VehiclePhysics::SetAttributes' first call, `this` in r3)
    // and its own "mSimpleAttribs.IsValid()" assert -- it is the DWARF's 0-arg overload
    // (BrnSimpleVehiclePhysics.h:163, PS3 mangled 7355CC).
    //
    // Refresh mSimpleAttribs from the car's AttribSys handling record, then chain to the 2-arg
    // with wheel positions/radii captured from the CURRENT (pre-refresh) state:
    //   positions[i] = maWheels[i].mStreamedPositionPlusTwistAmount + OLD mSimpleAttribs.mCOMOffset,
    //                  .y -= OLD height offset (front lanes for wheels 0/1, rear for 2/3)
    //   radii[i]     = maWheels[i].mSlipVariables.w  (the current radius, preserved)
    // The AttribSys chase is the inlined generated-ctor chain: burnoutcarasset(key) ->
    // PhysicsVehicleHandlingAsset RefSpec (data+0x158) -> physicsvehiclehandling -> the checked
    // COPY (@0x825BDB88, the by-value argument) -> SimpleVehicleAttribs::SetupAttribs.
    bool SimpleVehiclePhysics::SetAttributes()
    {
        // ---- capture the pre-refresh state ----------------------------------------------------
        f32 lafRadii[eNumDrivenWheels];
        Vector3 laPositions[eNumDrivenWheels];
        {
            const f32 lfFrontOffset =
                mSimpleAttribs.mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.z;
            const f32 lfRearOffset =
                mSimpleAttribs.mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.w;
            for (s32 li = 0; li < eNumDrivenWheels; ++li)
            {
                lafRadii[li] = maWheels[li].mSlipVariables.w;
                laPositions[li] = vpu::Add(maWheels[li].mStreamedPositionPlusTwistAmount.GetVector3(),
                                           mSimpleAttribs.mCOMOffset);
                laPositions[li].y -= (li < 2) ? lfFrontOffset : lfRearOffset;   // vrlimi(4)
            }
        }

        CGS_ASSERT(mSimpleAttribs.IsValid(), "mSimpleAttribs.IsValid()");   // console line 0x10D

        // ---- the AttribSys chase (each generated ctor is the console's inlined sequence) ------
        {
            Attrib::Gen::burnoutcarasset lCarAsset(mSimpleAttribs.mAttribsKey, NULL);
            Attrib::Gen::physicsvehiclehandling lHandling(
                const_cast<Attrib::Collection*>(
                    lCarAsset.GetPhysicsVehicleHandlingRefSpec()->GetCollection()), NULL);
            // The checked copy @0x825BDB88 -- the console's by-value argument.
            Attrib::Gen::physicsvehiclehandling lHandlingCopy(lHandling);
            mSimpleAttribs.SetupAttribs(lHandlingCopy);
        }

        // mCOMOffset += mHandlingBodyOffset ([this+0x670] += [this+0x690]).
        mSimpleAttribs.mCOMOffset = vpu::Add(mSimpleAttribs.mCOMOffset, mHandlingBodyOffset);

        return SetAttributes(laPositions, lafRadii);
    }

    // ===========================================================================================
    //  SimpleVehiclePhysics::SetAttributes(const Vector3*, const f32*)  @0x826020A0  (503 insns)
    // ===========================================================================================
    // ⭐ BODIED 2026-08-09 (attribs-setup wave). The shared tail of every SetAttributes wrapper:
    // mass + solid-box inverse inertia from the (freshly re-streamed) mSimpleAttribs, then a
    // per-wheel Wheel::Prepare. The Wheel.h Prepare lane map (proven 2026-08-05) named this
    // function's asm (0x826027F4..0x82602840) as its caller witness; the register roles here
    // match it exactly.
    bool SimpleVehiclePhysics::SetAttributes(const Vector3* lpaWheelPositions,
                                             const f32* lpafWheelRadii)
    {
        // .data scalars -- same two as SwitchAttribs (bank writers @0x82C5D190/@0x82C5D1B8,
        // sources image-read: flt_82004F5C == 30.0, flt_8209AE88 == 0.025).
        static const f32 KF_WHEEL_INTEGRATION_SEED     = 30.0f;
        static const f32 KF_MIN_SUSPENSION_TRAVEL_DOWN = 0.025f;
        static const f32 KF_ONE_TWELFTH = 0.0833333358f;   // flt_82094724

        CGS_ASSERT(lpaWheelPositions != NULL, "lpaWheelPositions");   // 0x149
        CGS_ASSERT(lpafWheelRadii != NULL,    "lpafWheelRadii");      // 0x14A

        // ---- the mass, with the console's zero-mass diagnostic --------------------------------
        // On mass <= 0 the X360 dumps the vault array through a stack StrStream into
        // gpcMessageBuffer and FireAsserts it (console line 0x152); modelled as the assert it
        // fires, with the console's own message text.
        const f32 lfMass = mSimpleAttribs.mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.z;
        mfMass = VecFloat{ lfMass, lfMass, lfMass, lfMass };          // stvx128 -> +0xE0
        CGS_ASSERT(lfMass > 0.0f,
                   "\n\nZero vehicle mass. Please tell Graham D and include the TTY!");

        // ---- the solid-box inverse inertia (identical machinery to SwitchAttribs) -------------
        const Vector3 lBoxExtent = vpu::Mult(mHalfExtent, 2.0f);      // flt_82001D9C
        CGS_ASSERT(vpu::IsValid(lBoxExtent), "RwMath::IsValid( lBoxExtent )");   // 0x15A
        CGS_ASSERT(lBoxExtent.x > 0.0f, "lBoxExtent.X() > 0.0f");                // 0x15C
        CGS_ASSERT(lBoxExtent.y > 0.0f, "lBoxExtent.Y() > 0.0f");                // 0x15D
        CGS_ASSERT(lBoxExtent.z > 0.0f, "lBoxExtent.Z() > 0.0f");                // 0x15E
        {
            const f32 lfMassOver12 = lfMass * KF_ONE_TWELFTH;
            const f32 lfX2 = lBoxExtent.x * lBoxExtent.x;
            const f32 lfY2 = lBoxExtent.y * lBoxExtent.y;
            const f32 lfZ2 = lBoxExtent.z * lBoxExtent.z;
            // vrefp + two Newton refines each -> the diagonal inverse tensor.
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
                   "RwMath::IsValid( mLocalInverseInertia )");                   // 0x168

        // ---- the per-wheel prepare, from the NEW mSimpleAttribs -------------------------------
        // Local position copy (the 8 x ld/std loop), then the height-offset raise on the y lane
        // (front lanes for wheels 0/1, rear for 2/3) -- the RIDE-HEIGHT ADD that the callers'
        // capture step subtracted.
        Vector3 laPositions[eNumDrivenWheels];
        {
            const f32 lfFrontOffset =
                mSimpleAttribs.mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.z;
            const f32 lfRearOffset =
                mSimpleAttribs.mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.w;
            for (s32 li = 0; li < eNumDrivenWheels; ++li)
            {
                laPositions[li] = lpaWheelPositions[li];
                laPositions[li].y += (li < 2) ? lfFrontOffset : lfRearOffset;   // vrlimi(4)
            }
        }

        const f32 lfTravelUp = mSimpleAttribs.mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.x;
        const f32 lfTravelDownRaw = mSimpleAttribs.mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.y;
        const f32 lfTravelDown = (lfTravelDownRaw > KF_MIN_SUSPENSION_TRAVEL_DOWN)
                                     ? lfTravelDownRaw : KF_MIN_SUSPENSION_TRAVEL_DOWN;   // vmaxfp

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            // The stacked tire-pointer table: 5C0, 5C0, 600, 600 == front, front, rear, rear.
            const Wheel::TireAttribs* lpTire = (liWheel < 2) ? &mSimpleAttribs.mFrontTireAttribs
                                                             : &mSimpleAttribs.mRearTireAttribs;
            maWheels[liWheel].Prepare(
                vpu::Subtract(laPositions[liWheel], mSimpleAttribs.mCOMOffset),  // vsubfp v1
                lpafWheelRadii[liWheel],
                KF_WHEEL_INTEGRATION_SEED,
                lfTravelUp,
                lfTravelDown,
                lpTire);
        }

        return true;   // li r3, 1
    }

    // ===============================================================================================
    // SimpleVehiclePhysics::SetAttributes(VehicleAttribs*, const Vector3*, const f32*)
    //
    // ⭐ BODIED 2026-08-11 (the VehiclePhysics::Prepare wave). This header's own note said
    // "3-arg: declared for the class surface (the PS3 attests it); no X360 body recovered to it
    // yet" -- i.e. a DECLARED-BUT-UNDEFINED member, which is the [[shadowing-redeclarations]]
    // landmine: it mangles fine, no per-TU gate can see it, and the first real caller turns into
    // an LNK2019. VehiclePhysics::SetAttributes @0x8262E140 is exactly that first real caller,
    // so the symbol is closed here rather than routed around.
    //
    // ⚠️ There is no out-of-line X360 body to read: the console INLINES this at every site. It is
    // recovered from TWO independent inlined copies that agree statement for statement, which is
    // the same evidence standard the VehicleDriver::ClearControls recovery used last wave:
    //   * VehiclePhysics::SetAttributes @0x8262E16C..0x8262E1A0 --
    //         assert(lpAttribs) [aLpattribs, aDP4B5MainBurno_219, line 0x12B == 299]
    //         bl SimpleVehicleAttribs::SetupAttribs   r3 = this+0x5A0 == &mSimpleAttribs, r4 = lpAttribs
    //         bl SimpleVehiclePhysics::SetAttributes  r4 = positions, r5 = radii  (the 2-arg)
    //   * SimpleVehiclePhysics::Prepare @0x8262FC90..0x8262FCC4 -- the SAME three, same string,
    //     same file symbol, same line 0x12B. That copy is already spelled out inline in Prepare
    //     below (it was written before this overload had a body); it is left as it is, because
    //     what the console emits there IS the inline.
    // ⭐ The PS3 build carries the overload out of line and NAMED (0x734B10,
    // `_ZN10BrnPhysics7Vehicle20SimpleVehiclePhysics13SetAttributesEPNS0_14VehicleAttribsE...`),
    // which is what identifies the three-statement run as one function rather than three
    // statements of the caller.
    // ===============================================================================================
    bool SimpleVehiclePhysics::SetAttributes(VehicleAttribs* lpAttribs,
                                             const Vector3* lpWheelPositions,
                                             const f32* lpafWheelRadii)
    {
        CGS_ASSERT(lpAttribs != 0, "lpAttribs");                 // BrnSimpleVehiclePhysics.cpp:299
        mSimpleAttribs.SetupAttribs(lpAttribs);                  // bl @0x8262E190
        return SetAttributes(lpWheelPositions, lpafWheelRadii);  // bl @0x8262E1A0 (the 2-arg)
    }

    // ===============================================================================================
    // SimpleVehiclePhysics::Prepare  @0x8262F620 (554 insns)
    //
    // OUT OF THE "BLOCKED (fidelity:blocked)" LIST, 2026-08-11 (the create-drain wave). The header
    // blocked-list banner said this was "deep VMX128 ... cannot be faithfully reproduced BY NAME".
    // That was assessed against the 20-byte SimpleVehicleAttribs slice and before the member map was
    // pinned. RE-READ FROM THE ASM: the great majority of the 554 instructions are the inlined
    // RwMathVPU::IsValid NaN cascades of the nine debug asserts (each lane is a vspltw + vcmpeqfp. +
    // branch triple), and every one of the remaining stores lands on a member this header ALREADY
    // NAMES. Nothing here is de-SIMD'd by guess.
    //
    // THE SIGNATURE IS DWARF/PS3-ATTESTED, not inferred. PS3 export 0x734D58:
    //   _ZN10BrnPhysics7Vehicle20SimpleVehiclePhysics7PrepareE
    //     N2rw4math3vpu14Matrix44AffineE NS4_7Vector3E S6_ S6_ S6_
    //     RKN12CgsGeometric14AxisAlignedBoxE PNS0_14VehicleAttribsE PKS6_ PKf
    // which is exactly the declaration this header already carried, parameter for parameter. The
    // X360 register map confirms it: r4 = lTransform, v1..v4 = the four vectors in declaration
    // order, r5 = lAABB, r6/r7/r8 = attribs / wheel positions / wheel radii.
    //
    // Asserts in the console order (lines from the baked __FILE__/__LINE__ pairs): :98
    // IsValid(lTransform) - :99 IsValid(lLinearVelocity) - :100 IsValid(lAngularVelocity) - :101
    // IsValid(lHandlingBodyOffset) - :102 IsValid(lHalfExtent) - :103 lpAttribs != NULL - :104
    // lpaWheelPositions != NULL - :105 lpafWheelRadii != NULL - :110 IsValid(lpaWheelPositions[w])
    // per wheel - :299 the second, bare lpAttribs one - :134 IsValid(mLocalInverseInertia).
    //
    // Store map, every offset read off the asm and reached BY NAME below:
    //   0x8262FB94  bl ExternalPhysicsBody::Prepare      r3 = this + 0x10 (the base sub-object)
    //   0x8262FBA0  bl SimpleVehiclePhysics::Reset(Vector3) @0x825D9A58. The caller forwards
    //               lLinearVelocity in v1; that source/ABI argument is real even though this
    //               Breaker body never consumes it.
    //   0x8262FBB0  bl SimpleVehicleAttribs::SetupAttribs  r3 = this + 0x5A0 == &mSimpleAttribs
    //   0x8262FBC8  stvx128 v127, this+0x50    mLinearVelocity     = lLinearVelocity
    //   0x8262FBD0  stvx128 v125, this+0x690   mHandlingBodyOffset = lHandlingBodyOffset
    //   0x8262FBD8  stvx128 v126, this+0x60    mAngularVelocity    = lAngularVelocity
    //   0x8262FBE8/F8/FC04/FC0C   the four rows of lTransform -> mTransform (+0x10/0x20/0x30/0x40)
    //   0x8262FC38  stvx128 v124, this+0x6A0   mHalfExtent         = lHalfExtent
    //   0x8262FC28/FC44/FC48  mfSpeedMPH = vmsum3fp(mTransform.zAxis, mLinearVelocity) *
    //               KF_MPS_TO_MPH -- the forward-axis component of the velocity, in mph. The
    //               constant is the same flt_830180B0 == 2.2369363f VehiclePhysics.cpp already homes
    //               (1 / 0.44704); the asm splats it out of a 16-byte stack temp whose other three
    //               lanes are zeroed (`stw r30` x3), which is why the multiply is a lane-0 splat.
    //   0x8262FC4C..0x8262FC88  mDeformableAABB = *lAABB and mOriginalAABB = *lAABB -- four ld/std
    //               pairs each, from the SAME source. Both AABBs start life as the streamed box;
    //               VehicleManager::SetRaceCarCrashing's inlined ResetDeformableAABB copies +0x6F0
    //               back over +0x6D0, which is only meaningful if the two start equal.
    //   0x8262FCB4  bl SimpleVehicleAttribs::SetupAttribs   AGAIN, after the bare :299 assert. NOT a
    //               transcription slip -- the console has two distinct `bl` sites (0x8262FBB0 and
    //               0x8262FCB4), the second guarded by its own assert. Reproduced as-is.
    //   0x8262FCC4  bl SimpleVehiclePhysics::SetAttributes(lpaWheelPositions, lpafWheelRadii)
    //   0x8262FE88..0x8262FEB4  mbCrashing = false - mbStartedDeforming = false - and the
    //               mAboveGroundTestResult block at +0x570: position/normal zeroed, distance
    //               flt_82001CC0 (READ FROM THE IMAGE with x360rd: 0x00000000 == 0.0f), the two
    //               CollisionTag halfwords 0xFFFF/0x8000, valid byte cleared. That is
    //               AboveGroundTestResult::Reset() store for store -- the body already in this TU,
    //               recovered independently from a DIFFERENT console function's inlined copy, so it
    //               is called by name rather than re-spelled.
    //   0x8262FE94  li r3, 1   -> returns true.
    // ===============================================================================================
    bool SimpleVehiclePhysics::Prepare(Matrix44Affine lTransform, Vector3 lLinearVelocity,
                                       Vector3 lAngularVelocity, Vector3 lHandlingBodyOffset,
                                       Vector3 lHalfExtent,
                                       const CgsGeometric::AxisAlignedBox& lrAABB,
                                       VehicleAttribs* lpAttribs, const Vector3* lpWheelPositions,
                                       const f32* lpafWheelRadii)
    {
        CGS_ASSERT(vpu::IsValid(lTransform),          "RwMathVPU::IsValid( lTransform )");          // :98
        CGS_ASSERT(vpu::IsValid(lLinearVelocity),     "RwMathVPU::IsValid( lLinearVelocity )");     // :99
        CGS_ASSERT(vpu::IsValid(lAngularVelocity),    "RwMathVPU::IsValid( lAngularVelocity )");    // :100
        CGS_ASSERT(vpu::IsValid(lHandlingBodyOffset), "RwMathVPU::IsValid( lHandlingBodyOffset )"); // :101
        CGS_ASSERT(vpu::IsValid(lHalfExtent),         "RwMathVPU::IsValid( lHalfExtent )");         // :102
        CGS_ASSERT(lpAttribs != 0,                    "lpAttribs != NULL");                         // :103
        CGS_ASSERT(lpWheelPositions != 0,             "lpaWheelPositions != NULL");                 // :104
        CGS_ASSERT(lpafWheelRadii != 0,               "lpafWheelRadii != NULL");                    // :105
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            CGS_ASSERT(vpu::IsValid(lpWheelPositions[liWheel]),
                       "RwMathVPU::IsValid( lpaWheelPositions[lu8Wheel] )");                        // :110
        }

        ExternalPhysicsBody::Prepare();          // bl @0x8262FB94, r3 = this + 0x10
        Reset(lLinearVelocity);                  // v1 @0x8262FB98; source arg is real but callee-dead
        mSimpleAttribs.SetupAttribs(lpAttribs);  // bl @0x8262FBB0

        mLinearVelocity     = lLinearVelocity;     // +0x50
        mHandlingBodyOffset = lHandlingBodyOffset; // +0x690
        mAngularVelocity    = lAngularVelocity;    // +0x60
        mTransform          = lTransform;          // +0x10..+0x40, four stvx128
        mHalfExtent         = lHalfExtent;         // +0x6A0

        // +0x6C0: forward speed in mph -- vmsum3fp128(mTransform.zAxis, mLinearVelocity) broadcast,
        // times the lane-0 splat of KF_MPS_TO_MPH.
        const f32 lfForwardSpeedMPH =
            vpu::Dot(mTransform.zAxis, mLinearVelocity) * KF_SVP_MPS_TO_MPH;
        mfSpeedMPH = VecFloat{ lfForwardSpeedMPH, lfForwardSpeedMPH,
                               lfForwardSpeedMPH, lfForwardSpeedMPH };

        mDeformableAABB = lrAABB;                // +0x6D0, four ld/std out of lAABB
        mOriginalAABB   = lrAABB;                // +0x6F0, the SAME source

        CGS_ASSERT(lpAttribs != 0, "lpAttribs");                                                    // :299
        mSimpleAttribs.SetupAttribs(lpAttribs);            // bl @0x8262FCB4 -- the second call
        SetAttributes(lpWheelPositions, lpafWheelRadii);   // bl @0x8262FCC4

        CGS_ASSERT(vpu::IsValid(mLocalInverseInertia.xAxis) &&
                   vpu::IsValid(mLocalInverseInertia.yAxis) &&
                   vpu::IsValid(mLocalInverseInertia.zAxis),
                   "RwMathVPU::IsValid( mLocalInverseInertia )");                                   // :134

        mbCrashing         = false;              // stb 0, +0x710
        mbStartedDeforming = false;              // stb 0, +0x712
        mAboveGroundTestResult.Reset();          // the +0x570 block, store for store

        return true;                             // li r3, 1
    }
}
}
