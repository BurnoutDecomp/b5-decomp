#include "types.hpp"
#include <cstddef>
#define private public
#define protected public
#include "GameSource/Director/BrnDirectorModule.h"
#undef private
#undef protected
extern "C" const unsigned int geometry_containment[] = {
 sizeof(ICE::ICEController), __alignof(ICE::ICEController), sizeof(ICE::ICEManager),
 offsetof(ICE::ICEController,mLogo), offsetof(ICE::ICEController,mfHudRectX),
 offsetof(ICE::ICEController,mfHudRectY), offsetof(ICE::ICEController,mfHudRectWidth),
 offsetof(ICE::ICEController,mfHudRectHeight), offsetof(ICE::ICEController,miStatusCount),
 offsetof(ICE::ICEController,miStatusText), offsetof(ICE::ICEController,miShowUi),
 offsetof(ICE::ICEController,maChangeBuffer), offsetof(ICE::ICEController,miMenuTimer),
 sizeof(BrnDirector::ICEWrapper), __alignof(BrnDirector::ICEWrapper),
 offsetof(BrnDirector::ICEWrapper,mICEManager), offsetof(BrnDirector::ICEWrapper,mICETimer),
 offsetof(BrnDirector::ICEWrapper,mICEFileHandler), offsetof(BrnDirector::ICEWrapper,mVehicleRef),
 sizeof(BrnDirector::MainDirector), offsetof(BrnDirector::MainDirector,mICEWrapper),
 offsetof(BrnDirector::MainDirector,mICESceneSpace), offsetof(BrnDirector::MainDirector,mArbitrator),
 offsetof(BrnDirector::MainDirector,mCgsCamera), sizeof(BrnDirector::DirectorModule),
 offsetof(BrnDirector::DirectorModule,mMainDirector), offsetof(BrnDirector::DirectorModule,mCamera),
 offsetof(BrnDirector::DirectorModule,mDirectorSerialiser)
};
