from pathlib import Path
import json
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol');ROOT=Path(r'D:/codex-native/game-module-sol');items=[]
for ty in ['BrnDirector::ICEWrapper','ICE::ICEMemory','ICE::ICEManager','ICE::ICEController','ICE::ICEAuthor','ICE::ICETake','ICE::ICECameraMover','ICE::ICECamera','ICE::ICECameraAnchor','BrnDirector::VehicleRef','ICE::ICEWidgetLogo']:
 items.append((ty+'.size','sizeof('+ty+')'));items.append((ty+'.align','__alignof('+ty+')'))
for ty,names in [
 ('BrnDirector::ICEWrapper','mICEMemory mICEManager mICETimer mICEFileHandler mpResourceManager mActionQueue mCameraMover mICECamera mICECameraAnchor miICELoadStateA miICELoadStateB mVehicleRef mCurrentMovieID mbAcceptInput mbWasEditorActive'),
 ('ICE::ICEManager','mTake mCopyTake mShakeTake mPlaybackTake mbPlaybackDataSet mfOldTimeElapsed mController'),
 ('ICE::ICEController','mRenderContext mTimeBar mTimeBar2 mInfoList mColorPicker mTargetBox mWideScrMarker mLogo mfHudRectX mfHudRectY mfHudRectWidth mfHudRectHeight miStatusCount miStatusMode miStatusText miEditFlag724C miShowUi mfCursorParam mfCursorRatio mfBarFade mi16BubbleYaw mi16BubblePitch mfBubbleRadius maChangeBuffer maRampAccum miMenuTimer miBarFlipLatch miBarFlipSub mfMarkIn mfMarkOut')]:
 for name in names.split():items.append((ty+'.'+name,'offsetof('+ty+', '+name+')'))
code='#define private public\n#define protected public\n#include "GameSource/Game/BrnGameModule.hpp"\n#undef private\n#undef protected\nextern "C" const unsigned int gIceNativeLayout[] = {\n'+',\n'.join(e for _,e in items)+'\n};\n'
p=ROOT/'.native-probes/ice-layout.cpp';p.write_text(code);(OUT/'ice-layout-fields.json').write_text(json.dumps(items,indent=2)+'\n')
