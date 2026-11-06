from pathlib import Path
ROOT=Path(r'D:/codex-native/game-module-sol');OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol')
for rel in ['src/GameSource/Director/BrnReplayDirector.h','src/GameSource/Director/BrnReplayDirector.cpp']:
 p=ROOT/rel;(OUT/(p.name+'.before')).write_bytes(p.read_bytes())
h='''#ifndef GAMESOURCE_DIRECTOR_BRN_REPLAY_DIRECTOR_H
#define GAMESOURCE_DIRECTOR_BRN_REPLAY_DIRECTOR_H

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
// ARTIST827E2020 and82250558/8225BD28/8225C298 establish these actual
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
}

#endif
'''
cpp='''#include "GameSource/Director/BrnReplayDirector.h"

namespace BrnDirector
{
// The real member constructors emit ARTIST827E2020's behaviour/collision-policy
// vptrs, two ICETake constructors, AllVehicleData array sentinel, CarScoreData
// constructor and DebugLog index sentinel. Remaining storage stays untouched.
ReplayDirector::ReplayDirector()
{
}
}
'''
(ROOT/'src/GameSource/Director/BrnReplayDirector.h').write_bytes(h.replace('\n','\r\n').encode('utf-8'))
(ROOT/'src/GameSource/Director/BrnReplayDirector.cpp').write_bytes(cpp.replace('\n','\r\n').encode('utf-8'))
fields=['mExternalCamera','mBumperCamera','mGraphicsCamera','mLastCamera','mExternalParameters','mBumperParameters','mExternalBehaviour','mBumperBehaviour','mRotationController','mSphericalRotationController','mSceneSpace','mCameraSceneSpace','mAllVehicleData','mGameState','mVehicleTracker','mRandom','mDebugLog','mDebugPrinter','mCameraSpaceHandler','miCameraType','mbStartedThisFrame']
items=[('size','sizeof(BrnDirector::ReplayDirector)'),('align','__alignof(BrnDirector::ReplayDirector)')]+[(f,'offsetof(BrnDirector::ReplayDirector, '+f+')')for f in fields]
import json
(OUT/'replay-layout-fields.json').write_text(json.dumps(items,indent=2)+'\n')
probe='#include "GameSource/Director/BrnReplayDirector.h"\n#include <stddef.h>\nextern "C" const unsigned int gReplayNativeLayout[] = {\n'+',\n'.join(e for _,e in items)+'\n};\n'
(ROOT/'.native-probes/replay-layout.cpp').write_text(probe)
