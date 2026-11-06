// Private diagnostic only: expose access for offsetof, preserving all real fields/gates.
#define private public
#define protected public
#include "GameSource/World/BrnWorldModule.h"
#include "GameSource/Graphics/BrnShaderConstantsFrame.h"
#include "GameSource/World/BrnWorldModuleIO_DispatchOutputBuffer.h"
#undef protected
#undef private
extern "C" const unsigned int NativeWorldLayout[] = {
 sizeof(BrnWorld::WorldModule), __alignof(BrnWorld::WorldModule),
 offsetof(BrnWorld::WorldModule, mEnvironmentManager),
 offsetof(BrnWorld::EnvironmentSettings::EnvironmentManager, mfCloudDistanceCurve),
 offsetof(BrnWorld::WorldModule, meLocalPlayerActiveRaceCarIndex),
 offsetof(BrnWorld::WorldModule, mfLocalPlayerActiveRaceCarSpeed),
 offsetof(BrnWorld::WorldModule, maeCarControls),
 offsetof(BrnWorld::WorldModule, mbDEBUGPlayerCarAlwaysUnderAIControl),
 offsetof(BrnWorld::WorldModule, mShaderLodInfo),
 offsetof(BrnWorld::WorldModule, mfCarKeyLightMultiplier),
 offsetof(BrnWorld::WorldModule, mfCarAmbientLightMultiplier),
 sizeof(BrnWorld::ShaderLodInfo), __alignof(BrnWorld::ShaderLodInfo),
 sizeof(BrnShaderConstantsFrame), offsetof(BrnShaderConstantsFrame, mbLockedForWriting),
 sizeof(BrnWorldIO::DispatchOutputBuffer),
 offsetof(BrnWorldIO::DispatchOutputBuffer, mmQuadricIrradianceA),
 offsetof(BrnWorldIO::DispatchOutputBuffer, mmQuadricIrradianceB),
 offsetof(BrnWorldIO::DispatchOutputBuffer, mvFogColourPlusWhiteLevel)
};
