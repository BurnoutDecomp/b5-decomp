#pragma once

#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"

namespace BrnReplays { struct DirectorSerialiserStaticLayout; }
namespace BrnDirector
{
struct DirectorInputOutput;

// ARTIST 827E2020/82250558/8225BD28/8225C298. Console displacements
// below identify original members; native pointer-bearing members widen on PC.
// The complete console owner spans0x29E0 bytes, from DirectorModule+0x35F50
// through+0x38930. Its final camera/selection/first-frame members were formerly
// modelled as separate DirectorModule fields at+0x36380/+0x38920/+0x38924.
class ReplayDirector
{
public:
    ReplayDirector();
    void FillInSharedInfo(Camera::BehaviourSharedInfo* lpBehaviourInfo,
                          Camera::CollisionPolicySharedInfo* lpPolicyInfo,
                          BrnReplays::DirectorSerialiserStaticLayout* lpFrame,
                          const DirectorInputOutput* lpIO);
    void PreSceneQueryUpdate(BrnReplays::DirectorSerialiserStaticLayout* lpFrame,
                             const DirectorInputOutput* lpIO);
    void Update(BrnReplays::DirectorSerialiserStaticLayout* lpFrame,
                const DirectorInputOutput* lpIO);

    Camera::Camera& GetFinalCamera() { return mFinalCamera; }
    const CgsGraphics::Camera& GetCgsCamera() const { return mCgsCamera; }
    void SetFirstFrame(bool lbFirstFrame) { mbFirstFrame = lbFirstFrame; }
    void SetCameraSelection(s32 liSelection) { miCameraSelection = liSelection; }

private:
    Camera::Camera mExternalCamera;                   // +0x000
    Camera::Camera mBumperCamera;                     // +0x160
    CgsGraphics::Camera mCgsCamera;                    // +0x2C0
    Camera::Camera mFinalCamera;                      // +0x430
    Camera::BehaviourGameplayExternal::Parameters mExternalParameters; // +0x590
    Camera::BehaviourGameplayBumper::Parameters mBumperParameters;     // +0x640
    Camera::BehaviourGameplayExternal mExternalBehaviour;              // +0x690
    Camera::BehaviourGameplayBumper mBumperBehaviour;                  // +0x1200
    Camera::Utils::Camera2DRotationController mRotationController;      // +0x1A30
    Camera::Utils::CameraSphericalRotationController mSphericalRotationController; // +0x1A60
    Matrix44Affine mSceneSpace;                       // +0x1A90
    Matrix44Affine mTrafficLightSpace;                // +0x1AD0
    AllVehicleData mAllVehicleData;                   // +0x1B10
    GameState mGameState;                             // +0x1C50
    VehicleTracker mVehicleTracker;                   // +0x1E50
    CgsNumeric::Random mRandom;                       // +0x2100
    DebugLog mDebugLog;                               // +0x2130
    DebugPrinter mDebugPrinter;                       // +0x2790
    ICE::CameraSpaceHandler mCameraSpaceHandler;      // +0x27C0
    s32 miCameraSelection;                           // +0x29D0
    bool mbFirstFrame;                               // +0x29D4
};
}
