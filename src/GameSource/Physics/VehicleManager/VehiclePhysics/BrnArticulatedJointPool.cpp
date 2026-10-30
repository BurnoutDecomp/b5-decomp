#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"                      // CgsDev::StrStream (RemoveBrokenJointsFromSimulation's streamed assert)
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerIO.h"        // ArticulatedJointCreateBuffer
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleOutputRequestInterface
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                           // CgsPhysics::RigidBodyId
#include "rw/math/vpu/matrix44affine_operation.h"                                  // IsValid(Matrix44Affine)
#include "rw/math/vpu/vector3_operation.h"                                        // IsValid(Vector3)
#include "vendor/renderware/physics/JointFrames.hpp"                              // real AddJoint frame payload
#include "vendor/renderware/physics/JointLimits.hpp"                              // real AddJoint limit payload
#include "SDKs/XboxMath/XMVectorCos.h"                                           // ARTIST 0x821F06B0

#include <cstddef>       // offsetof (_AssertLayout)
#include <cmath>
#include <vectorintrinsics.h>

// BrnPhysics::Vehicle::ArticulatedJointPool -- the bodies. The class itself moved to
// BrnArticulatedJointPool.h this wave (2026-08-03, task #113); read that header's banner for WHY
// (it was a second, forked declaration of a class BrnPhysicalTrafficManager.h also declared, and
// the two were one symbol to the linker).
//
// ==================================================================================================
// ODR FORK #1 RETIRED 2026-08-03 (task #110). This TU used to declare, at namespace scope in
// BrnPhysics::Vehicle, its own
//         class ArticulatedJoint { public: int Construct(); private: u8 mPad0[80]; };
// while the real class lives in BrnArticulatedJoint.h.
//
// IT WAS NOT MERELY UNTIDY, IT WAS UNSATISFIABLE. The fork declared `int Construct()`; the
// DWARF (BrnArticulatedJoint.h:100) declares `void Construct()`. MSVC folds the return type into
// the mangled name, so the fork's call site demanded
//     ?Construct@ArticulatedJoint@Vehicle@BrnPhysics@@QEAAHXZ
// while any faithful body written against the real header defines
//     ?Construct@ArticulatedJoint@Vehicle@BrnPhysics@@QEAAXXZ
// -- two symbols, and no TU could ever have defined the one this file asked for. That is the
// standing "shadowing redeclaration" failure: invisible to every per-TU compile gate, visible only
// to a LINK (measured then as LNK2019 the moment this TU was first mounted).
//
// ODR FORK #2 RETIRED 2026-08-03 (task #113): the pool class ITSELF. See BrnArticulatedJointPool.h.
// The same return-type trap was live here too, in the opposite direction: this file declared
// `int Construct()` purely to agree with the fork in BrnPhysicalTrafficManager.h. It is now `void`,
// per the DWARF (BrnArticulatedJointPool.h:73), and the fork it was agreeing with is gone.
// ==================================================================================================

namespace BrnPhysics
{
namespace Vehicle
{

namespace
{
    // Native VMX operations retain the original rounding and denormal behavior.
    template <typename T> long JointFloatType(T);
    char JointFloatType(f32);
    float JointMultiply(float a, float b)
    {
        const __vector4 va = {a,a,a,a}, vb = {b,b,b,b};
        return __vmulfp(va, vb).x;
    }
    float JointSubtract(float a, float b)
    {
        const __vector4 va = {a,a,a,a}, vb = {b,b,b,b};
        return __vsubfp(va, vb).x;
    }
    float JointMultiplyAdd(float a, float b, float c)
    {
        const __vector4 va = {a,a,a,a}, vb = {b,b,b,b}, vc = {c,c,c,c};
        return __vmaddfp(va, vb, vc).x;
    }
    Vector3 JointRotate(Vector3 lvRow, const Vector3& lvX, const Vector3& lvY, const Vector3& lvZ)
    {
        const Vector3 result = {
            JointMultiplyAdd(lvRow.z, lvZ.x, JointMultiplyAdd(lvRow.y, lvY.x, JointMultiply(lvRow.x, lvX.x))),
            JointMultiplyAdd(lvRow.z, lvZ.y, JointMultiplyAdd(lvRow.y, lvY.y, JointMultiply(lvRow.x, lvX.y))),
            JointMultiplyAdd(lvRow.z, lvZ.z, JointMultiplyAdd(lvRow.y, lvY.z, JointMultiply(lvRow.x, lvX.z))),
            JointMultiplyAdd(lvRow.z, lvZ.w, JointMultiplyAdd(lvRow.y, lvY.w, JointMultiply(lvRow.x, lvX.w)))};
        return result;
    }

    // ARTIST 0x825D8024..0x825D81AC inlines the orthonormal-body inverse and
    // affine product. Keep its z/y/x inverse-translation accumulation and its
    // x/y/z product accumulation; the generic scalar affine helpers do not keep
    // those fused orders. The transpose has zero fourth lanes, as vmrghw/glw do.
    Matrix44Affine JointFrameInBodySpace(const Matrix44Affine& lrJoint,
                                         const Matrix44Affine& lrBody)
    {
        const Vector3 lvX = {lrBody.xAxis.x, lrBody.yAxis.x, lrBody.zAxis.x, 0.0f};
        const Vector3 lvY = {lrBody.xAxis.y, lrBody.yAxis.y, lrBody.zAxis.y, 0.0f};
        const Vector3 lvZ = {lrBody.xAxis.z, lrBody.yAxis.z, lrBody.zAxis.z, 0.0f};
        const Vector3 lvNegativePosition = {
            JointSubtract(0.0f, lrBody.wAxis.x),
            JointSubtract(0.0f, lrBody.wAxis.y),
            JointSubtract(0.0f, lrBody.wAxis.z),
            JointSubtract(0.0f, lrBody.wAxis.w)};
        const Vector3 lvInversePosition = {
            JointMultiplyAdd(lvNegativePosition.x, lvX.x,
                JointMultiplyAdd(lvNegativePosition.y, lvY.x, JointMultiply(lvNegativePosition.z, lvZ.x))),
            JointMultiplyAdd(lvNegativePosition.x, lvX.y,
                JointMultiplyAdd(lvNegativePosition.y, lvY.y, JointMultiply(lvNegativePosition.z, lvZ.y))),
            JointMultiplyAdd(lvNegativePosition.x, lvX.z,
                JointMultiplyAdd(lvNegativePosition.y, lvY.z, JointMultiply(lvNegativePosition.z, lvZ.z))),
            JointMultiplyAdd(lvNegativePosition.x, lvX.w,
                JointMultiplyAdd(lvNegativePosition.y, lvY.w, JointMultiply(lvNegativePosition.z, lvZ.w)))};
        const Vector3 lvPosition = lrJoint.wAxis;
        Matrix44Affine lResult;
        lResult.xAxis = JointRotate(lrJoint.xAxis, lvX, lvY, lvZ);
        lResult.yAxis = JointRotate(lrJoint.yAxis, lvX, lvY, lvZ);
        lResult.zAxis = JointRotate(lrJoint.zAxis, lvX, lvY, lvZ);
        {
            const Vector3 lAggregateValue = {
            JointMultiplyAdd(lvPosition.z, lvZ.x, JointMultiplyAdd(lvPosition.y, lvY.x,
                JointMultiplyAdd(lvPosition.x, lvX.x, lvInversePosition.x))),
            JointMultiplyAdd(lvPosition.z, lvZ.y, JointMultiplyAdd(lvPosition.y, lvY.y,
                JointMultiplyAdd(lvPosition.x, lvX.y, lvInversePosition.y))),
            JointMultiplyAdd(lvPosition.z, lvZ.z, JointMultiplyAdd(lvPosition.y, lvY.z,
                JointMultiplyAdd(lvPosition.x, lvX.z, lvInversePosition.z))),
            JointMultiplyAdd(lvPosition.z, lvZ.w, JointMultiplyAdd(lvPosition.y, lvY.w,
                JointMultiplyAdd(lvPosition.x, lvX.w, lvInversePosition.w)))};
            lResult.wAxis = lAggregateValue;
        }
        return lResult;
    }
}

// ---------------------------------------------------------------------------------------
// ArticulatedJointPool::_AssertLayout   -- the gate that keeps the de-fork honest.
//
// A `sizeof(ArticulatedJoint) == 80` line CANNOT carry this gate and used to be all there was.
// ArticulatedJoint is Matrix44Affine(64) + ArticulatedJointId(8) == 72 and alignas(16) via the
// matrix, so eight bytes of tail padding absorb any small addition and 80 never moves
// (tamper-verified 2026-08-03: adding a u32 did NOT fail that line). ON AN OVER-ALIGNED TYPE,
// GATE OFFSETS, NOT SIZE. Every line below is an OFFSET into this class, plus the three
// sizeof(member) lines that catch a member being retyped in place.
// ---------------------------------------------------------------------------------------
void ArticulatedJointPool::_AssertLayout()
{
    // --- the element stride the console's index math uses -------------------------------
    // Construct @0x82600938 walks maJoints with `slwi r11,r31,2 ; add r11,r31,r11 ; slwi r11,r11,4`
    // == i*80, and GetJoint @0x825C2B40 ends with the same sequence.
    CGS_STATIC_ASSERT((sizeof(ArticulatedJoint) == 80),
                  "maJoints is strided i*80 by Construct @0x82600938 and GetJoint @0x825C2B40");

    // --- the four member offsets PhysicalTrafficManager::Construct's neighbours pin ------
    // The pool sits at PhysicalTrafficManager+103616 and the next member it writes is at
    // +104448, so the pool is exactly 832 bytes; inside it, ArticulatedJointPool::Construct
    // clears two 64-bit masks and then stores four floats at the offsets below.
    CGS_STATIC_ASSERT((offsetof(ArticulatedJointPool, maJoints) == 0),   "maJoints @0");
    CGS_STATIC_ASSERT((offsetof(ArticulatedJointPool, mUsedJoints) == 800),
                  "mUsedJoints @800 -- Construct's first `std 0`, and the word IsJointInUse "
                  "@0x825C29C8 bit-tests");
    CGS_STATIC_ASSERT((offsetof(ArticulatedJointPool, mJointsBrokenThisFrame) == 808),
                  "mJointsBrokenThisFrame @808 -- Construct's second `std 0`");
    CGS_STATIC_ASSERT((offsetof(ArticulatedJointPool, mfSwingAngleDegrees) == 816),       "@816 = 30.0f");
    CGS_STATIC_ASSERT((offsetof(ArticulatedJointPool, mfMaxSwingVelocityDegrees) == 820), "@820 = 10.0f");
    CGS_STATIC_ASSERT((offsetof(ArticulatedJointPool, mfTwistAngleDegrees) == 824),       "@824 = 15.0f");
    CGS_STATIC_ASSERT((offsetof(ArticulatedJointPool, mfMaxTwistVelocityDegrees) == 828), "@828 = 10.0f");

    // --- widths, because an offset gate alone is blind to a member retyped in place ------
    // (the TrafficPhysics tamper test proved this: u8 -> u32 in a 3-byte pad hole is SILENT to
    //  every offset and size assert around it)
    CGS_STATIC_ASSERT((sizeof(ArticulatedJointPool::ArticulatedJointBitArray) == 8), "BitArray<10> is one u64 field");
    // ADDED AFTER THE TAMPER TEST FOUND A HOLE. `sizeof(f32) == 4` is a tautology, and with only
    // that line case 6 (`mfSwingAngleDegrees` f32 -> u32) was **SILENT**: same width, same offset,
    // same class size -- and semantically wrong, because the console stores these four with `stfsx`,
    // a FLOAT store. The type check is what actually catches it.
    CGS_STATIC_ASSERT(((sizeof(JointFloatType(((ArticulatedJointPool*)0)->mfSwingAngleDegrees)) == sizeof(char))),
                  "mfSwingAngleDegrees is written by `stfsx` -- a float, not an integer word");
    CGS_STATIC_ASSERT(((sizeof(JointFloatType(((ArticulatedJointPool*)0)->mfMaxSwingVelocityDegrees)) == sizeof(char))),
                  "mfMaxSwingVelocityDegrees is written by `stfsx`");
    CGS_STATIC_ASSERT(((sizeof(JointFloatType(((ArticulatedJointPool*)0)->mfTwistAngleDegrees)) == sizeof(char))),
                  "mfTwistAngleDegrees is written by `stfsx`");
    CGS_STATIC_ASSERT(((sizeof(JointFloatType(((ArticulatedJointPool*)0)->mfMaxTwistVelocityDegrees)) == sizeof(char))),
                  "mfMaxTwistVelocityDegrees is written by `stfsx`");
    CGS_STATIC_ASSERT((sizeof(((ArticulatedJointPool*)0)->mfSwingAngleDegrees) == 4),
                  "...and a SINGLE-precision one (stfsx, not stfdx)");

    // --- and the total, which is what BrnPhysicalTrafficManager.h embeds ------------------
    // 10*80 + 8 + 8 + 4*4 == 832, with zero slack: 828 + 4 == 832 and the class is 16-aligned
    // via ArticulatedJoint, 832 % 16 == 0, so there is no tail pad hiding a mistake here.
    CGS_STATIC_ASSERT((sizeof(ArticulatedJointPool) == 832),
                  "PhysicalTrafficManager::Construct @0x82636CA8 pins the pool at this+103616 and "
                  "its successor at this+104448");
}

// ---------------------------------------------------------------------------------------
// ArticulatedJointPool::Construct   @ 0x82600938  (144 bytes)
//
// The two masks cleared @800/@808 (BitArray<10>::UnSetAll() emits the same `std 0` as the
// prior mu64=0), then each joint Construct()ed, then the four limit floats keep their VALUES
// by offset (@816=30, @820=10, @824=15, @828=10 -- the asm reuses the same loaded register/
// value for @820 and @828).
//
// RETURN TYPE: `void`, per the DWARF (BrnArticulatedJointPool.h:73). The console ends `blr`
// with r3 still holding the last ArticulatedJoint::Construct's `this`, which is why Hex-Rays
// types it `int`; the value is read by nobody (PhysicalTrafficManager::Construct @0x82636CA8,
// the sole caller, discards it). See the header banner.
// ---------------------------------------------------------------------------------------
void ArticulatedJointPool::Construct()
{
    mUsedJoints.UnSetAll();
    mJointsBrokenThisFrame.UnSetAll();

    for (s32 i = 0; i < KI_NUM_JOINTS; ++i)
        maJoints[i].Construct();

    mfSwingAngleDegrees       = 30.0f;   // @816
    mfMaxSwingVelocityDegrees = 10.0f;   // @820
    mfTwistAngleDegrees       = 15.0f;   // @824
    mfMaxTwistVelocityDegrees = 10.0f;   // @828 (same f0 reg/value reused from @820 in the asm)
}

// GetJoint @ 0x825C2B40 : bounds-/in-use-asserted accessor; returns &maJoints[liJointIndex]
// (the tail slwi/add/slwi computes this + liJointIndex*80). Both asserts are non-gating tripwires.
ArticulatedJoint* ArticulatedJointPool::GetJoint(s32 liJointIndex)
{
    CGS_ASSERT(liJointIndex >= 0 && liJointIndex < KI_NUM_JOINTS, "Invalid joint index: ");
    CGS_ASSERT(IsJointInUse(liJointIndex), "IsJointInUse( liJointIndex )");
    return &maJoints[liJointIndex];
}

// IsJointInUse @ 0x825C29C8 (const) : reads the u64 mUsedJoints word at +800 and bit-tests
// (1 << (index & 63)). Two bounds asserts precede it (signed [0,10) 'Invalid joint index: ';
// inlined CgsBitArray bound 'invalid index : ') -- both non-gating.
bool ArticulatedJointPool::IsJointInUse(s32 liJointIndex) const
{
    CGS_ASSERT(liJointIndex >= 0 && liJointIndex < KI_NUM_JOINTS, "Invalid joint index: ");
    CGS_ASSERT(static_cast<u32>(liJointIndex) < KU_NUM_JOINTS, "invalid index : ");
    return mUsedJoints.IsBitSet(static_cast<u32>(liJointIndex));
}

// ---------------------------------------------------------------------------------------
// ArticulatedJointPool::CreateJoint @ 0x826009C8 (named ARTIST export hole)
//
// The X360 checks the working buffer and both body transforms plus the world hinge point,
// selects the first clear bit from the ten-slot pool, packs the cab/trailer entity ids with
// that slot, marks the slot used, builds the InAddJoint payload, and flags its queue entry.
// The original post-assert path continues even when the pool is full.
// ---------------------------------------------------------------------------------------
s32 ArticulatedJointPool::CreateJoint(
    ArticulatedJointCreateBuffer* lpJointWorkingBuffer,
    CgsPhysics::RigidBodyId lCabRigidBodyId,
    CgsPhysics::RigidBodyId lTrailerRigidBodyId,
    const Matrix44Affine& lrCabWorldTransform,
    const Matrix44Affine& lrTrailerWorldTransform,
    Vector3 lArticulationPointWorldSpace)
{
    CGS_ASSERT(lpJointWorkingBuffer != 0, "lpAddJointEvent != NULL"); // :149
    CGS_ASSERT(rw::math::vpu::IsValid(lrCabWorldTransform),
               "RwMathVPU::IsValid( lCabWorldTransform )");
    CGS_ASSERT(rw::math::vpu::IsValid(lrTrailerWorldTransform),
               "RwMathVPU::IsValid( lTrailerWorldTransform )");
    CGS_ASSERT(rw::math::vpu::IsValid(lArticulationPointWorldSpace),
               "RwMathVPU::IsValid( lArticulationPointWorldSpace )");

    const s32 liNewJointIndex = mUsedJoints.GetFirstZeroBit();
    CGS_ASSERT(liNewJointIndex < KI_NUM_JOINTS,
               "liNewJointIndex < kiMaxArticulatedTrafficVehicles"); // :162

    ArticulatedJointId lJointId;
    const EntityId lCabEntityId = { static_cast<u32>(lCabRigidBodyId.GetEntityId()) };
    const EntityId lTrailerEntityId = { static_cast<u32>(lTrailerRigidBodyId.GetEntityId()) };
    lJointId.Set(static_cast<u16>(liNewJointIndex), lCabEntityId, lTrailerEntityId);

    mUsedJoints.SetBit(static_cast<u32>(liNewJointIndex));

    ArticulatedJointCreateBuffer::InAddJoint lAddJointEvent;
    ConstructArticulatedJoint(&lAddJointEvent, lJointId, lCabRigidBodyId,
                              lTrailerRigidBodyId, lrCabWorldTransform,
                              lrTrailerWorldTransform, lArticulationPointWorldSpace);
    lpJointWorkingBuffer->FlagJointToBeCreated(liNewJointIndex, lAddJointEvent);
    return liNewJointIndex;
}

// ConstructArticulatedJoint @0x825D7DD8. The asm keeps this/output/joint/cab/trailer/
// cab-transform/trailer-transform in r3..r9 and the world hinge in v1. The three
// handles are full u64 values; the apparent Hex-Rays register pairs are spurious.
void ArticulatedJointPool::ConstructArticulatedJoint(
    CgsPhysics::PhysicsSimulationIO::InAddJoint* lpOutAddJointEvent,
    ArticulatedJointId lJointId,
    CgsPhysics::RigidBodyId lCabRigidBodyId,
    CgsPhysics::RigidBodyId lTrailerRigidBodyId,
    const Matrix44Affine& lrCabWorldTransform,
    const Matrix44Affine& lrTrailerWorldTransform,
    Vector3 lWorldSpaceJointPos)
{
    CGS_ASSERT(lpOutAddJointEvent != 0, "lpOutAddJointEvent != NULL"); // :223
    CGS_ASSERT(mfSwingAngleDegrees > 0.0f, "mfSwingAngleDegrees > 0.0f");   // :224

    lpOutAddJointEvent->mu64Id = lJointId.mu64RawId;
    lpOutAddJointEvent->mu64ParentBodyId = static_cast<u64>(lCabRigidBodyId);
    lpOutAddJointEvent->mu64ChildBodyId = static_cast<u64>(lTrailerRigidBodyId);
    lpOutAddJointEvent->mbSpy = true;

    rw::physics::JointLimits& lrLimits = lpOutAddJointEvent->mJointLimits;
    const float kfDegreesToRadians = 0.01745329238474369f; // 0x8208F5F4: 0x3C8EFA35
    lrLimits.mSwingf = rw::physics::SWING_CONE;
    lrLimits.mSwinga = mfSwingAngleDegrees * kfDegreesToRadians;
    lrLimits.mSwingc = XboxMath::XMVectorCos(lrLimits.mSwinga);
    lrLimits.mVswing = mfMaxSwingVelocityDegrees * kfDegreesToRadians;
    lrLimits.mTwistf = mfTwistAngleDegrees == 0.0f
        ? rw::physics::TWIST_LOCKED : rw::physics::TWIST_ARC;
    lrLimits.mTwista = mfTwistAngleDegrees * kfDegreesToRadians;
    lrLimits.mTwistc = XboxMath::XMVectorCos(lrLimits.mTwista);
    {
        const Vector3 lAggregateValue = {0.1f, 0.1f, 0.1f, 0.0f};
        lrLimits.mPprism = lAggregateValue;
    }
    {
        const Vector3 lAggregateValue = {0.1f, 0.1f, 0.1f, 0.0f};
        lrLimits.mVprism = lAggregateValue;
    }
    lrLimits.mVtwist = mfMaxTwistVelocityDegrees * kfDegreesToRadians;

    // 0x825D7EA4 / 0x825D7F30..0x825D8018: vxor negation of cab.At(),
    // y/z/x/w swizzles and a multiply/nmsub cross, then unguarded Normalize.
    const __vector4 lvCabZ = __lvx(&lrCabWorldTransform.zAxis, 0);
    CGS_ALIGN(16) const u32 lauSign[4] = {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u};
    const __vector4 lvSign = __lvx(lauSign, 0);
    Vector3 lvHingeAxis;
    __stvx(__vxor(lvCabZ, lvSign), &lvHingeAxis, 0);
    const Vector3 lvUp = lrCabWorldTransform.yAxis;
    const __vector4 ha = __lvx(&lvHingeAxis, 0), up = __lvx(&lvUp, 0);
    const __vector4 ayzx = {ha.y,ha.z,ha.x,ha.w}, bzxy = {up.z,up.x,up.y,up.w};
    const __vector4 azxy = {ha.z,ha.x,ha.y,ha.w}, byzx = {up.y,up.z,up.x,up.w};
    const __vector4 cross = __vnmsubfp(ayzx, bzxy, __vmulfp(azxy, byzx));
    const __vector4 lengthSquared = __vmsum3fp(cross, cross);
    const __vector4 half = {0.5f,0.5f,0.5f,0.5f}, one = {1.0f,1.0f,1.0f,1.0f};
    __vector4 estimate = __vrsqrtefp(lengthSquared);
    for (s32 i = 0; i < 2; ++i)
    {
        const __vector4 square = __vmulfp(estimate, estimate);
        const __vector4 halfEstimate = __vmulfp(estimate, half);
        const __vector4 residual = __vnmsubfp(lengthSquared, square, one);
        estimate = __vmaddfp(halfEstimate, residual, estimate);
    }
    Vector3 lvCross;
    __stvx(__vmulfp(cross, estimate), &lvCross, 0);

    Matrix44Affine lJointTransform;
    lJointTransform.xAxis = lvHingeAxis;
    lJointTransform.yAxis = lvUp;
    lJointTransform.zAxis = lvCross;
    lJointTransform.wAxis = lWorldSpaceJointPos;
    lJointTransform = rw::math::vpu::OrthoNormalize3x3(lJointTransform); // bl 0x82203B28

    const Matrix44Affine lParentToJointTransform = JointFrameInBodySpace(lJointTransform, lrCabWorldTransform);
    const Matrix44Affine lChildToJointTransform = JointFrameInBodySpace(lJointTransform, lrTrailerWorldTransform);
    rw::physics::JointFrames& lrFrames = lpOutAddJointEvent->mJointFrames;
    const Matrix33 lChildAngular = {lChildToJointTransform.xAxis, lChildToJointTransform.yAxis, lChildToJointTransform.zAxis};
    lrFrames.SetChildAngularFrame(lChildAngular);
    const Matrix33 lParentAngular = {lParentToJointTransform.xAxis, lParentToJointTransform.yAxis, lParentToJointTransform.zAxis};
    lrFrames.SetParentAngularFrame(lParentAngular);
    const Matrix33 lParentLinear = {lrCabWorldTransform.xAxis, lrCabWorldTransform.yAxis, lrCabWorldTransform.zAxis};
    lrFrames.SetParentLinearFrame(lParentLinear);
    lrFrames.SetChildPosition(lChildToJointTransform.wAxis);
    lrFrames.SetParentPosition(lParentToJointTransform.wAxis);

    const u16 lu16JointIndex = static_cast<u16>(lJointId.mu64RawId); // clrlwi r11,r22,16
    maJoints[lu16JointIndex].Set(lParentToJointTransform, lJointId);
}

// ---------------------------------------------------------------------------------------
// ArticulatedJointPool::SendCreateRemoveJointEvents   @ 0x826013C0  (1464 bytes)   NEW 2026-08-03
//
// One frame's batched articulation-joint requests, drained out of the working IO buffer and onto
// the simulation request interface: every slot flagged for creation becomes an AddJoint, every slot
// flagged for removal becomes a RemoveJoint. The asserts are at BrnArticulatedJointPool.cpp:431/432.
//
// THIS FUNCTION NEVER DEREFERENCES `this`. The X360 prologue keeps r4/r5 (the two arguments) and
// simply drops r3; nothing in the 1464 bytes reads a pool member. It is a member only because the
// DWARF declares it as one (BrnArticulatedJointPool.h:104) -- which is also why the retired fork in
// BrnPhysicalTrafficManager.h could carry a WRONG `this` layout and still not crash. What the fork
// got wrong was the SIGNATURE: it declared `(const void*, ArticulatedJointCreateBuffer*)` where the
// DWARF has `(VehicleOutputRequestInterface*, const ArticulatedJointCreateBuffer*)`, so the symbol
// its call site demanded could never have been defined by a faithful body.
//
// BOTH LOOPS ARE THE INLINED CgsBitArray FIRST/NEXT-SET-BIT WALK, not hand-rolled index math:
//   lwz/ld field ; branch if zero ; v - ((v-1) & v) ; cntlzd ; 64*field - clz + 63
// is GetFirstNonZeroBit(), and the `(idx & ~63) + 64 clipped to 10` inner scan with the
// CgsBitArray.h:203 "invalid index : " bound assert is GetNextNonZeroBit(idx). The console then
// re-checks `0 <= idx < 10` at each loop head, which is exactly the -1/out-of-range contract those
// two helpers already carry, so the `>= 0` test below is the whole of it.
//
// The two event accessors are the asserted ones (GetCreateJointEvent @0x825C26F0 /
// GetRemoveJointEvent @0x825C2860, both already bodied in BrnPhysicalTrafficManagerIO.cpp; IDA
// truncates their names to `ArticulatedJointCreateBu` / `ArticulatedJointCreat`, which is name
// truncation, not an export hole).
//
// The create side calls the out-of-line VehicleOutputRequestInterface::AddJoint @0x825E7170; the
// remove side is the INLINED RemoveJoint (its assert carries file SharedIO/BrnVehicleOutputInterface.h
// line 730 and it lands on the queue at interface+41840). Both are modelled as the named methods.
// ---------------------------------------------------------------------------------------
void ArticulatedJointPool::SendCreateRemoveJointEvents(
        VehicleOutputRequestInterface* lpRequestInterface,
        const ArticulatedJointCreateBuffer* lpJointWorkingBuffer)
{
    CGS_ASSERT(lpRequestInterface != 0, "lpRequestInterface != NULL");        // :431
    CGS_ASSERT(lpJointWorkingBuffer != 0, "lpJointWorkingBuffer != NULL");    // :432

    const ArticulatedJointCreateBuffer::CreatedJointBitArray* lpCreatedJoints =
        lpJointWorkingBuffer->GetCreatedJointBitArray();

    for (s32 liJointIndex = lpCreatedJoints->GetFirstNonZeroBit();
         liJointIndex >= 0;
         liJointIndex = lpCreatedJoints->GetNextNonZeroBit(liJointIndex))
    {
        lpRequestInterface->AddJoint(lpJointWorkingBuffer->GetCreateJointEvent(liJointIndex));
    }

    const ArticulatedJointCreateBuffer::RemovedJointBitArray* lpRemovedJoints =
        lpJointWorkingBuffer->GetRemovedJointBitArray();

    for (s32 liJointIndex = lpRemovedJoints->GetFirstNonZeroBit();
         liJointIndex >= 0;
         liJointIndex = lpRemovedJoints->GetNextNonZeroBit(liJointIndex))
    {
        lpRequestInterface->RemoveJoint(lpJointWorkingBuffer->GetRemoveJointEvent(liJointIndex));
    }
}

// ---------------------------------------------------------------------------------------
// ArticulatedJointPool::RemoveJoint   @ 0x825D8248  (580 bytes)   DWARF :98   NEW 2026-09-24
//
//   0x825D8264..0x825D8288  assert "lpJointWorkingBuffer != NULL"                     (.cpp 0x142 = 322)
//   0x825D828C..0x825D82B4  assert "liJointIndex >= 0 && liJointIndex < kiMaxArticulatedTrafficVehicles" (323)
//   0x825D82C8..0x825D836C  the inlined IsBitSet's streamed "invalid index : <i> < 10" (CgsBitArray.h:203)
//   0x825D8370..0x825D8420  mUsedJoints (+800, word 0x64*8) bit test; clear -> streamed
//                           "Trying to remove articulated joint <i> that isn't in use"     (324)
//   0x825D8424..0x825D8448  GetJoint(i) (0x825C2B40) ; `ld r11,0x40` (mJointId) ; `std` into the
//                           stack InRemoveJoint ; lpJointWorkingBuffer->FlagJointToBeRemoved(i, it)
//                           (0x825C24F8)
//   0x825D844C..0x825D8480  assert "luIndex < NUMBITS" (CgsBitArray.h:0xF1 = 241) ; `ldx / andc /
//                           stdx` on +0x320 == mUsedJoints.UnSetBit(i)
// Every assert is a non-gating tripwire: an out-of-use joint is still flagged and cleared.
// ---------------------------------------------------------------------------------------
void ArticulatedJointPool::RemoveJoint(ArticulatedJointCreateBuffer* lpJointWorkingBuffer, s32 liJointIndex)
{
    CGS_ASSERT(lpJointWorkingBuffer != 0, "lpJointWorkingBuffer != NULL");          // :322
    CGS_ASSERT(liJointIndex >= 0 && liJointIndex < KI_NUM_JOINTS,
               "liJointIndex >= 0 && liJointIndex < kiMaxArticulatedTrafficVehicles");    // :323
    CGS_ASSERT(static_cast<u32>(liJointIndex) < KU_NUM_JOINTS, "invalid index : ");       // CgsBitArray.h:203
    CGS_ASSERT(mUsedJoints.IsBitSet(static_cast<u32>(liJointIndex)),
               "Trying to remove articulated joint ");   // :324, streamed "<i> that isn't in use"

    ArticulatedJointCreateBuffer::InRemoveJoint lRemoveJointEvent;
    lRemoveJointEvent.mu64Id = GetJoint(liJointIndex)->GetJointId().mu64RawId;   // ld 0x40 / std
    lpJointWorkingBuffer->FlagJointToBeRemoved(liJointIndex, lRemoveJointEvent);

    CGS_ASSERT(static_cast<u32>(liJointIndex) < KU_NUM_JOINTS, "luIndex < NUMBITS");      // CgsBitArray.h:241
    mUsedJoints.UnSetBit(static_cast<u32>(liJointIndex));
}

// ---------------------------------------------------------------------------------------
// ArticulatedJointPool::RemoveBrokenJointsFromSimulation
//
// Every joint flagged in mJointsBrokenThisFrame (+808) is removed through RemoveJoint, in
// ascending index order (the console walks the set bits with the inlined lowest-set-bit /
// next-set-bit search, whose per-bit IsBitSet carries the "invalid index : " bound), then the
// whole broken set is cleared (`std 0` at +808). A broken joint that is no longer in use only
// trips the streamed assert ; it is still handed to RemoveJoint. Sole caller:
// PhysicalTrafficManager::ProcessCreateEvents.
// ---------------------------------------------------------------------------------------
void ArticulatedJointPool::RemoveBrokenJointsFromSimulation(ArticulatedJointCreateBuffer* lpJointWorkingBuffer)
{
    CGS_ASSERT(lpJointWorkingBuffer != 0, "lpJointWorkingBuffer != NULL");

    for (s32 liJointIndex = mJointsBrokenThisFrame.GetFirstNonZeroBit();
         liJointIndex >= 0;
         liJointIndex = mJointsBrokenThisFrame.GetNextNonZeroBit(liJointIndex))
    {
        if (!IsJointInUse(liJointIndex))
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Trying to remove a broken joint that isn't in use, index: " << liJointIndex;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage,
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.cpp", 402);
            CgsDev::Assert::EndAssert();
        }
        RemoveJoint(lpJointWorkingBuffer, liJointIndex);
    }

    mJointsBrokenThisFrame.UnSetAll();
}

}
}

// ============================================================================
// FOLDED FROM BrnArticulatedJointPool_wG12_OtherHalf.cpp (wave G12) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool_wG12_OtherHalf.cpp
//
// ArticulatedJointPool::GetIndexOfOtherHalf, plus the three packed-id readers the console inlines
// into it (declared in BrnArticulatedJoint.h, bodied here).
//
// The id ArticulatedJointId::Set packs: bits[63:32] the CAB's physics EntityId, bits[31:16] the
// TRAILER's 14-bit entity index, bits[15:0] the joint's own pool index. Asking a CAB for its
// other half returns the trailer index straight out of the packed field; asking a TRAILER returns
// the cab index by unpacking the stored EntityId. That asymmetry is the console's, not a
// transcription slip. All four asserts are non-gating tripwires, and the CAB path really does
// fetch the joint twice.
// ============================================================================


namespace BrnPhysics
{
namespace Vehicle
{
    // ---- the inlined packed-id readers (declared in BrnArticulatedJoint.h) -------------------

    ArticulatedJointId ArticulatedJoint::GetJointId() const
    {
        return mJointId;
    }

    u16 ArticulatedJointId::GetJointPoolIndex() const
    {
        return static_cast<u16>(mu64RawId & 0xFFFFull);
    }

    u16 ArticulatedJointId::GetTrailerVehicleIndex() const
    {
        return static_cast<u16>((mu64RawId >> 16) & 0xFFFFull);
    }

    // FLAG: leArticulatedType is the caller's PhysicalTrafficVehicle::EArticulatedVehicleType
    // taken as s32 -- that enum lives in BrnPhysicalTrafficManager.h, which includes this pool's
    // header, so naming it here would close an include cycle. Live values: CAB(1), TRAILER(2).
    s32 ArticulatedJointPool::GetIndexOfOtherHalf(s32 liJointIndex, s32 liArticulatedType)
    {
        CGS_ASSERT(IsJointInUse(liJointIndex), "IsJointInUse( liJointIndex )");            // :359
        CGS_ASSERT(liArticulatedType > 0 && liArticulatedType < 3,
                   "leArticulatedType > PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_NONE && leArticulatedType < PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_COUNT");  // :361

        if (liArticulatedType == 1)   // E_ARTICULATE_VEHICLE_CAB
        {
            CGS_ASSERT(static_cast<s32>(GetJoint(liJointIndex)->GetJointId().GetJointPoolIndex())
                           == liJointIndex,
                       "GetJointId( liJointIndex ).GetJointPoolIndex() == liJointIndex");   // :365
            return static_cast<s32>(GetJoint(liJointIndex)->GetJointId().GetTrailerVehicleIndex());
        }

        CGS_ASSERT(liArticulatedType == 2,
                   "leArticulatedType == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_TRAILER"); // :371

        const EntityId lCabEntityId = GetJoint(liJointIndex)->GetJointId().GetCabEntityId();
        return static_cast<s32>((lCabEntityId.muValue >> 10) & 0x3FFFu);
    }
}
}
