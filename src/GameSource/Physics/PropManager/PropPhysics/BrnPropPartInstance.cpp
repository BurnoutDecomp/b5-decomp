#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::IsValid(Vector3)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ===========================================================================
// BrnPhysics::Props::PropPartInstance out-of-line setters, reconstructed store-for-
// store from BURNOUT_X360_ARTIST.XEX (SetPosition @0x825DE798,
// SetLinearVelocity @0x825DE860). Each runs the SDK's non-gating per-lane finiteness
// tripwire (RwMath::IsValid) then stores the vector at its member offset.
// ===========================================================================

namespace BrnPhysics
{
namespace Props
{
    // X360 0x825DE798. Non-gating finiteness tripwire (per-lane vcmpeqfp self-equality NaN
    // check on x/y/z) then stores the position at this+0.
    void PropPartInstance::SetPosition(Vector3 lPosition)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lPosition), "RwMath::IsValid( lPosition )");
        mPos = lPosition;
    }

    // X360 0x825DE860. Non-gating finiteness tripwire (per-lane vcmpeqfp self-equality NaN
    // check on x/y/z) then stores the linear velocity at this+0x10.
    void PropPartInstance::SetLinearVelocity(Vector3 lLinearVelocity)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lLinearVelocity), "RwMath::IsValid( lLinearVelocity )");
        mLinearVelocity = lLinearVelocity;
    }
}
}
