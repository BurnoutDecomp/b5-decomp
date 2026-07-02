#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"   // VisibilityCollisionPolicy (the committed slice home)

#include "GameShared/GameClasses/Core/CgsAssert.h"

// BrnDirector::Camera::VisibilityCollisionPolicy -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (the asserts name BrnCollisionPolicy.h:489/:425/:431).
//
// Bodied here (3 ledger functions, class:BrnDirector::Camera::VisibilityCollisionPolicy):
//   VisibilityCollisionPolicy::SetDesiredHeight              @0x821F38E0
//   VisibilityCollisionPolicy::TimeUntilCollisionWithGeometry @0x821F37C8
//   VisibilityCollisionPolicy::TimeUntilCollisionWithVehicle  @0x821F3858
//
// The two time queries are h-inline wrappers whose X360 bodies also inline the
// embedded predictor accessors (the second assert each carries); here the
// wrappers call the predictors' own committed/inline accessors, which fire the
// identical inner tripwires (GeometryCollisionPredictor::GetTimeUntilCollision
// @0x821F36C0 -- BrnGeometryCollisionPredictor.cpp -- and
// VehicleCollisionPredictor::TimeUntilCollision, BrnVehicleCollisionPredictor.h:69).
// All callers in this TU's export set: BehaviourGyroCam::Update.

namespace BrnDirector
{
namespace Camera
{

// @ 0x821F38E0 -- h:489. The latch is raised BEFORE the assert (the asm's
// stb 1,0x23C precedes the compare); NaN heights fire the assert like the X360's
// fcmpu (bgt-only skip).
void VisibilityCollisionPolicy::SetDesiredHeight(f32 lfDesiredHeight)
{
    mbHaveDesiredHeight = 1;
    CGS_ASSERT(lfDesiredHeight > 0.0f, "lfDesiredHeight > 0.0f");   // :489 (non-gating)
    mfDesiredHeight = lfDesiredHeight;
}

// @ 0x821F37C8 -- h:425 wrapper assert, then the embedded geometry predictor's
// accessor (its own :206 "mbWillCollide" tripwire, exactly the X360's second
// inlined assert).
f32 VisibilityCollisionPolicy::TimeUntilCollisionWithGeometry() const
{
    CGS_ASSERT(WillCollideWithGeometry(), "WillCollideWithGeometry()");   // :425 (non-gating)
    return mGeometryCollisionPredictor.GetTimeUntilCollision();
}

// @ 0x821F3858 -- h:431 wrapper assert, then the embedded vehicle predictor's
// accessor (its own BrnVehicleCollisionPredictor.h:69 "HasPredictedCollision()"
// tripwire, exactly the X360's second inlined assert).
f32 VisibilityCollisionPolicy::TimeUntilCollisionWithVehicle() const
{
    CGS_ASSERT(WillCollideWithVehicle(), "WillCollideWithVehicle()");     // :431 (non-gating)
    return mVehicleCollisionPredictor.TimeUntilCollision();
}

}
}
