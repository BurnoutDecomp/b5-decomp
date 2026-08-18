#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::IsValid(Vector3)
#include "rw/math/vpu/matrix44affine_operation.h"    // rw::math::vpu::IsValid(Matrix44Affine)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ===========================================================================
// BrnPhysics::Props::PropInstance out-of-line accessors, reconstructed store-for-
// store from BURNOUT_X360_ARTIST.XEX (GetJointIndex @0x825B96B0,
// SetAngularVelocity @0x825DE5F8, SetTransform @0x825DE370).
// ===========================================================================

namespace BrnPhysics
{
namespace Props
{
    // SetTransform @ 0x825DE370
    // Validates the incoming affine (every one of the four rows must be finite -- the asm
    // runs a per-row, per-lane vcmpeqfp self-equality NaN test over xAxis/yAxis/zAxis/wAxis
    // and ANDs the four results, i.e. IsValid(Matrix44Affine)) then copies all four rows
    // into mWorldTransform (+0x00). On failure the X360 streams 'Invalid prop transform: '
    // followed by the transform and a newline into the assert message buffer (the dropped
    // gpcMessageBuffer / BasePriorityQueue::Clear / StrStream operator<< machinery); that
    // collapses to one CGS_ASSERT carrying the static prefix.
    void PropInstance::SetTransform(Matrix44Affine lTransform)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lTransform),
                   "Invalid prop transform: ");
        mWorldTransform = lTransform;
    }

    // SetAngularVelocity @ 0x825DE5F8
    // Validates the incoming angular velocity (per-lane NaN self-equality test over x/y/z,
    // the SDK's RwMath::IsValid) then stores it into mAngularVelocity (+0x50).
    void PropInstance::SetAngularVelocity(Vector3 lAngularVelocity)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lAngularVelocity),
                   "RwMath::IsValid( lAngularVelocity )");
        mAngularVelocity = lAngularVelocity;
    }

    // SetLinearVelocity @ 0x825DE6C8
    // ⭐ ADDED 2026-08-18 (breakable-props keystone). The LINEAR twin of SetAngularVelocity,
    // instruction-for-instruction the same shape: three vspltw128 + vcmpeqfp. self-equality
    // lane tests over x/y/z ANDed together (RwMath::IsValid), the assert, then
    // `li r11,64 ; stvx128 v127, r31, r11` == mLinearVelocity (+0x40).
    //
    // ⚠️ PROVENANCE: 0x825DE6C8 has NO per-address JSON in .ida-exports (an export hole).
    // The 51-instruction body and the assert's own text/line were read out of
    // IDA Files/BURNOUT_X360_ARTIST.XEX.i64 with headless IDA 9.3, where the function is
    // named `BrnPhysics::Props::PropInstance::SetLinearVelocity` and spans 825DE6C8-825DE794.
    // The assert string is the binary's: "RwMath::IsValid( lLinearVelocity )" at
    // d:\p4\b5_main\...\PropPhysics/BrnPropInstance.h:266 -- two lines below the :264-ish
    // slot its angular twin occupies, which is the cross-check that the pair is the pair.
    //
    // Called by PropManager::ReadUpdatedBodies @0x82632918 (the prop leg's velocity write-back).
    void PropInstance::SetLinearVelocity(Vector3 lLinearVelocity)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lLinearVelocity),
                   "RwMath::IsValid( lLinearVelocity )");
        mLinearVelocity = lLinearVelocity;
    }

    // GetJointIndex @ 0x825B96B0
    // Asserts the prop is jointed (mu8JointIndex != KU_NOT_JOINTED) then returns the
    // joint index. The X360 reads the u8 field at +0x6C and returns it zero-extended.
    int32_t PropInstance::GetJointIndex() const
    {
        CGS_ASSERT(IsJointed(), "IsJointed()");
        return mu8JointIndex;
    }
}
}
