from pathlib import Path
import json,hashlib
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol');OUT.mkdir(exist_ok=True)
ROOT=Path(r'D:/codex-native/game-module-sol')
fields=[('game_size','sizeof(BrnGame::BrnGameModule)'),('game_align','__alignof(BrnGame::BrnGameModule)')]
for n in ['mAutoTestManager','mRenderModule','mWorldModule','mGameDataModule','mGameStateModule','mDirectorModule','mInputModule','mGuiModule','mEffectsModule','mSoundModule','mReplayModule','mNetworkModule','mpUpdateInputBufferStack','mCpuMonitors','mDebugManager','mMapFileReader','miWorldModuleUpdateTrace','mbHasGameTerminated']:
 fields.append((n,'offsetof(BrnGame::BrnGameModule, '+n+')'))
for ty in ['BrnRendererModule','BrnWorld::WorldModule','BrnResource::GameDataModule','BrnGameState::GameStateModule','BrnDirector::DirectorModule','CgsInput::InputModule','BrnGui::GuiModule','BrnEffects::EffectsModule','BrnSound::Module::RootSoundModule','BrnReplays::ReplayModule','BrnNetwork::BrnNetworkModule','BrnDirector::MainDirector','BrnDirector::Camera::Camera','CgsGraphics::Camera']:
 fields.append((ty+'_size','sizeof('+ty+')'));fields.append((ty+'_align','__alignof('+ty+')'))
for n in ['mDebugComponent','mDirectorResourceManager','mWorldMap','mLineTestFinePostOffice','mLineTestNearestPostOffice','mLineTestFastDoubleSidedPostOffice','mSphereTestFastPostOffice','mVolumeTestFinePostOffice','mVolumeTestDeepestPostOffice','mMainDirector','mCamera','miPostGuiCarIndexLatch','mbReplayStartedThisFrame','mDirectorSerialiser','mCgsCamera','mbIsReplaying','miPerfCount_PreSQUpdate']:
 fields.append(('director.'+n,'offsetof(BrnDirector::DirectorModule, '+n+')'))
for n in ['mICEWrapper','mICESceneSpace','mCameraInterpolationController','mShotSelector','mCrashAnalyser','mCameraFinaliser','mAllVehicleData','mArbitrator','mMomentController','mBehaviourManager','mLastCamera','mCgsCamera','miPrepareStage']:
 fields.append(('main_director.'+n,'offsetof(BrnDirector::MainDirector, '+n+')'))
code='#define private public\n#define protected public\n#include "GameSource/Game/BrnGameModule.hpp"\n#include <stddef.h>\n#undef private\n#undef protected\nextern "C" const unsigned int gGameModuleNativeLayout[] = {\n'+',\n'.join('    '+e for _,e in fields)+'\n};\n'
(OUT/'game-module-layout.cpp').write_text(code,encoding='utf-8');(OUT/'layout-fields.json').write_text(json.dumps(fields,indent=2)+'\n')
(OUT/'BrnGameModule.before.cpp').write_bytes((ROOT/'src/GameSource/Game/BrnGameModule.cpp').read_bytes())
(OUT/'INITIAL_BINDING.json').write_text(json.dumps({'base':'ff58ecb8dd1102fb3a85d70b9e1e6defc66c5b25','worktree':str(ROOT),'source_sha256':hashlib.sha256((ROOT/'src/GameSource/Game/BrnGameModule.cpp').read_bytes()).hexdigest(),'header_sha256':hashlib.sha256((ROOT/'src/GameSource/Game/BrnGameModule.hpp').read_bytes()).hexdigest(),'scope':'whole GameModule native syntax and genuine containment recovery, no gate bypass; probe changes only access visibility for layout inspection'},indent=2)+'\n')
