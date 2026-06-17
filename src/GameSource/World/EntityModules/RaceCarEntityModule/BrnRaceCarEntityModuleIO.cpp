// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.cpp
//
// Out-of-line bodies for the BrnWorld::RaceCarEntityModuleIO IO-buffer accessors that the
// X360 build emitted out-of-line (the const/non-const overloads that did not get inlined).
// Every body asserts the buffer's lock bit then returns &member:
//   write-lock (eStatusLockedForWrite, status>>3 &1) => IsBufferLockedForWriting(), non-const
//   read-lock  (eStatusLockedForRead,  status>>4 &1) => IsBufferLockedForReading(),  const
// Offsets are layout-derived (return &member), not hardcoded. CGS_ASSERT stamps __FILE__/__LINE__,
// so the X360-baked d:\p4 path/line are intentionally not reproduced.
//
// CORRECTION (1): X360 0x8279E310 is the non-const InputBuffer_PostPhysics::GetSceneInputInterface()
//   (write-lock, returns &mSceneInputInterface at this+29856), NOT a non-const GetContactSpyInterface.
// CORRECTION (2): there is no non-const GetContactSpyInterface in the DWARF, so none is emitted.
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{

// ---- OutputBuffer_Prepare ---------------------------------------------------

// X360 0x8279CDF0 (R, :126) -- const resource-request accessor.
const OutputBuffer_Prepare::ResourceRequestInterface*
OutputBuffer_Prepare::GetResourceRequestInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mResourceRequestInterface;
}

// X360 0x822B4990 (W, :127) -- mutable resource-request accessor.
OutputBuffer_Prepare::ResourceRequestInterface*
OutputBuffer_Prepare::GetResourceRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mResourceRequestInterface;
}

// ---- InputBuffer_PreScene ---------------------------------------------------

// X360 0x8279D060 (W, :164) -- mutable game-action queue accessor.
InputBuffer_PreScene::GameActionQueue*
InputBuffer_PreScene::GetGameActionQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mGameActionQueue;
}

// ---- OutputBuffer_PreScene --------------------------------------------------

// X360 0x822B4ED0 (W, :283) -- mutable vehicle-input accessor.
OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PreScene::GetVehicleInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mVehicleInputInterface;
}

// X360 0x822B4F78 (W, :286) -- mutable scene-input accessor.
OutputBuffer_PreScene::SceneInputInterface*
OutputBuffer_PreScene::GetSceneInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mSceneInputInterface;
}

// X360 0x8279D6F8 (R, :300) -- const race-car AI interface accessor.
const OutputBuffer_PreScene::RaceCarAIInterface*
OutputBuffer_PreScene::GetRaceCarAIInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mRaceCarAIInterface;
}

// ---- InputBuffer_PostScene --------------------------------------------------

// X360 0x822B5410 (R, :346) -- const PreScene traffic->racecar accessor.
const InputBuffer_PostScene::TrafficToRaceCarInterface_PreScene*
InputBuffer_PostScene::GetTrafficToRaceCarInterface_PreScene() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTrafficToRaceCarInterface_PreScene;
}

// ---- OutputBuffer_PostScene -------------------------------------------------

// X360 0x822B5608 (W, :380) -- mutable AI module-request accessor.
OutputBuffer_PostScene::AIModuleRequestInterface*
OutputBuffer_PostScene::GetAIModuleRequestInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mAIModuleRequestInterface;
}

// X360 0x822B56B0 (W, :383) -- mutable race-car->traffic accessor.
RaceCarToTrafficInterface*
OutputBuffer_PostScene::GetRaceCarToTrafficInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mRaceCarToTrafficInterface;
}

// ---- InputBuffer_PrePhysics -------------------------------------------------

// X360 0x822B5800 (R, :427) -- const online-scoring accessor.
const InputBuffer_PrePhysics::OnlineScoringInterface*
InputBuffer_PrePhysics::GetOnlineScoringInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mOnlineScoringInterface;
}

// X360 0x822B5950 (R, :430) -- const PostScene traffic->racecar accessor.
const InputBuffer_PrePhysics::TrafficToRaceCarInterface_PostScene*
InputBuffer_PrePhysics::GetTrafficToRaceCarInterface_PostScene() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mTrafficToRaceCarInterface_PostScene;
}

// ---- OutputBuffer_PrePhysics ------------------------------------------------

// X360 0x822B5C00 (W, :471) -- mutable vehicle-input accessor.
OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PrePhysics::GetVehicleInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mVehicleInputInterface;
}

// X360 0x8279E070 (R, :482) -- const game-event queue accessor.
const OutputBuffer_PrePhysics::GameEventQueue*
OutputBuffer_PrePhysics::GetGameEventQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mGameEventQueue;
}

// X360 0x822B5CA8 (W, :483) -- mutable game-event queue accessor.
OutputBuffer_PrePhysics::GameEventQueue*
OutputBuffer_PrePhysics::GetGameEventQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mGameEventQueue;
}

// ---- InputBuffer_PostPhysics ------------------------------------------------

// X360 0x8279E310 (W, :520) -- CORRECTION (1): the NON-const GetSceneInputInterface().
// Write-lock (status>>3 &1) => mutable getter; returns &mSceneInputInterface (this+29856).
// (This is the function the byfile's line-1006 FN mislabeled as GetContactSpyInterface.)
OutputBuffer_PreScene::SceneInputInterface*
InputBuffer_PostPhysics::GetSceneInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mSceneInputInterface;
}

// X360 0x822B5F48 (R, :522) -- const per-entity-module deformation-output accessor.
const InputBuffer_PostPhysics::DeformationOutputInterfaceForEntityModules*
InputBuffer_PostPhysics::GetDeformationOutputInterfaceForEntityModules() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDeformationOutputInterfaceForEntityModules;
}

// X360 0x822B5FF0 (R, :525) -- const deformation-output accessor (distinct from the
// AI-racecar accessor at 0x822B6290).
const InputBuffer_PostPhysics::DeformationOutputInterface*
InputBuffer_PostPhysics::GetDeformationOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mDeformationOutputInterface;
}

// X360 0x822B6140 (R, :528) -- const contact-spy accessor.
// CORRECTION (2): this is the ONLY GetContactSpyInterface getter (no non-const overload).
const InputBuffer_PostPhysics::ContactSpyInterface*
InputBuffer_PostPhysics::GetContactSpyInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mContactSpyInterface;
}

// X360 0x822B6290 (R, :531) -- const AI race-car interface accessor (last member).
const InputBuffer_PostPhysics::AIRaceCarInterface*
InputBuffer_PostPhysics::GetAIRaceCarInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mAIRaceCarInterface;
}

// ---- OutputBuffer_PostPhysics -----------------------------------------------

// X360 0x822B5D50 (W, :567) -- mutable scene-input accessor.
OutputBuffer_PreScene::SceneInputInterface*
OutputBuffer_PostPhysics::GetSceneInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mSceneInputInterface;
}

// X360 0x8279E678 (R, :578) -- const replay active-race-car output accessor.
const RCEntityActiveRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetReplayActiveRaceCarOutputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mReplayActiveRaceCarOutputInterface;
}

// X360 0x822B6488 (W, :579) -- mutable replay active-race-car output accessor
// (pairs with the const overload above at the identical +826992 offset).
RCEntityActiveRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetReplayActiveRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mReplayActiveRaceCarOutputInterface;
}

// X360 0x822B6920 (W, :582) -- mutable replay global-race-car output accessor.
RCEntityGlobalRaceCarOutputInterface*
OutputBuffer_PostPhysics::GetReplayGlobalRaceCarOutputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mReplayGlobalRaceCarOutputInterface;
}

// X360 0x8279E9C0 (R, :590) -- const vehicle-input accessor (re-exposed on PostPhysics).
const OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PostPhysics::GetVehicleInputInterface() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mVehicleInputInterface;
}

// X360 0x822B6878 (W, :591) -- mutable vehicle-input accessor (pairs with the const
// overload above at the identical +855152 offset).
OutputBuffer_PreScene::VehicleInputInterface*
OutputBuffer_PostPhysics::GetVehicleInputInterface()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    return &mVehicleInputInterface;
}

// ---- InputBuffer_GenerateDispatchLists --------------------------------------

// X360 0x822B69C8 (R, :627) -- const camera-input accessor.
const BrnDirector::Camera::Camera*
InputBuffer_GenerateDispatchLists::GetCameraInput() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return &mCameraInput;
}

// X360 0x822B6B20 (R) -- const trailing dispatch flag A (placeholder name; byte at +8289).
bool
InputBuffer_GenerateDispatchLists::GetDispatchFlagA() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mbDispatchFlagA;
}

// X360 0x822B6BD0 (R) -- const trailing dispatch flag B (placeholder name; byte at +8290).
bool
InputBuffer_GenerateDispatchLists::GetDispatchFlagB() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mbDispatchFlagB;
}

}
}
