#ifndef GAMESOURCE_DIRECTOR_BRN_REPLAY_DIRECTOR_H
#define GAMESOURCE_DIRECTOR_BRN_REPLAY_DIRECTOR_H

#include <cstddef>
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"
#include "GameSource/Director/Camera/Utils/BrnCamera2DRotationController.h"
#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"

namespace BrnDirector
{
// ARTIST 827E2020 and 82250558/8225BD28/8225C298 establish these actual
// contained objects. Member names describe the native roles; an original
// ReplayDirector DWARF declaration is absent in the PS3 reference build.
class ReplayDirector
{
public:
    ReplayDirector();

    // The frame-method declarations remain existing recovery debt. Their
    // complete implementations/signatures are separate from native containment.
    s32 FillInSharedInfo(s32 liSharedInfoOut, s32 liShotInfoOut, s32 liSerialiserFrame, void* lpInput);
    void* PreSceneQueryUpdate(s32 liSerialiserFrame, void* lpInput);
    void* Update(s32 liSerialiserFrame, void* lpInput);

    Camera::Camera mExternalCamera;                              //0000
    Camera::Camera mBumperCamera;                                //0160
    CgsGraphics::Camera mGraphicsCamera;                         //02C0
    Camera::Camera mLastCamera;                                  //0430
    Camera::BehaviourGameplayExternal::Parameters mExternalParameters; //0590
    Camera::BehaviourGameplayBumper::Parameters mBumperParameters; //0640
    Camera::BehaviourGameplayExternal mExternalBehaviour;        //0690
    Camera::BehaviourGameplayBumper mBumperBehaviour;            //1200
    Camera::Utils::Camera2DRotationController mRotationController; //1A30
    Camera::Utils::CameraSphericalRotationController mSphericalRotationController; //1A60
    Matrix44Affine mSceneSpace;                                  //1A90
    Matrix44Affine mCameraSceneSpace;                            //1AD0
    AllVehicleData mAllVehicleData;                              //1B10
    GameState mGameState;                                        //1C50
    VehicleTracker mVehicleTracker;                              //1E50
    CgsNumeric::Random mRandom;                                  //2100
    DebugLog mDebugLog;                                          //2130
    DebugPrinter mDebugPrinter;                                  //2790
    ICE::CameraSpaceHandler mCameraSpaceHandler;                 //27C0
    s32 miCameraType;                                            //29D0
    u8 mbStartedThisFrame;                                       //29D4
};

#if defined(D_PLATFORM_X360)
CGS_STATIC_ASSERT((sizeof(ReplayDirector) == 0x29E0), "native ReplayDirector extent");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mExternalCamera) == 0x0), "native ReplayDirector mExternalCamera");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mBumperCamera) == 0x160), "native ReplayDirector mBumperCamera");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mGraphicsCamera) == 0x2C0), "native ReplayDirector mGraphicsCamera");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mLastCamera) == 0x430), "native ReplayDirector mLastCamera");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mExternalParameters) == 0x590), "native ReplayDirector mExternalParameters");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mBumperParameters) == 0x640), "native ReplayDirector mBumperParameters");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mExternalBehaviour) == 0x690), "native ReplayDirector mExternalBehaviour");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mBumperBehaviour) == 0x1200), "native ReplayDirector mBumperBehaviour");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mRotationController) == 0x1A30), "native ReplayDirector mRotationController");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mSphericalRotationController) == 0x1A60), "native ReplayDirector mSphericalRotationController");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mSceneSpace) == 0x1A90), "native ReplayDirector mSceneSpace");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mCameraSceneSpace) == 0x1AD0), "native ReplayDirector mCameraSceneSpace");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mAllVehicleData) == 0x1B10), "native ReplayDirector mAllVehicleData");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mGameState) == 0x1C50), "native ReplayDirector mGameState");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mVehicleTracker) == 0x1E50), "native ReplayDirector mVehicleTracker");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mRandom) == 0x2100), "native ReplayDirector mRandom");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mDebugLog) == 0x2130), "native ReplayDirector mDebugLog");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mDebugPrinter) == 0x2790), "native ReplayDirector mDebugPrinter");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mCameraSpaceHandler) == 0x27C0), "native ReplayDirector mCameraSpaceHandler");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, miCameraType) == 0x29D0), "native ReplayDirector miCameraType");
CGS_STATIC_ASSERT((offsetof(ReplayDirector, mbStartedThisFrame) == 0x29D4), "native ReplayDirector mbStartedThisFrame");
#endif
}

#endif
