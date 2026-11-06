#include "GameSource/World/BrnWorldModule.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameSource/Graphics/BrnCoronaManager.h"
#include "GameSource/Effects/Curves.h"
extern "C" const unsigned int R4LayoutFacts[] = {
 sizeof(BrnWorld::World), __alignof(BrnWorld::World),
 sizeof(BrnWorld::RaceCarEntityModule), __alignof(BrnWorld::RaceCarEntityModule),
 sizeof(BrnReplays::RaceCarEntitySerialiser), __alignof(BrnReplays::RaceCarEntitySerialiser),
 sizeof(BrnWorld::WorldEntityModule), __alignof(BrnWorld::WorldEntityModule),
 sizeof(BrnTraffic::TrafficEntityModule), __alignof(BrnTraffic::TrafficEntityModule),
 sizeof(BrnTraffic::VehicleTypeRuntime),
 sizeof(CgsGraphics::ShaderConstantsExternal), sizeof(CgsGraphics::ShaderConstantsInternal), sizeof(CgsGraphics::ShaderConstantsCPU),
 sizeof(rw::math::fpu::Vector2Template<float>), sizeof(rw::math::fpu::Vector3Template<float>),
 sizeof(Vector2), sizeof(rw::RGBA), sizeof(BrnEffects::Curves::SmoothStep),
 sizeof(renderengine::Corona), sizeof(renderengine::CoronaBuffer), sizeof(renderengine::CoronaBuffer::Iterator),
 sizeof(BrnCoronaTypeParams), sizeof(BrnCoronaManager::BrnSubmissionInterface), sizeof(BrnCoronaManager)
};
CGS_STATIC_ASSERT((sizeof(BrnWorld::World)==0x5e3c80), "R4 measured World");
CGS_STATIC_ASSERT((sizeof(BrnWorld::WorldEntityModule)==0x79e0), "R4 retained child footprint, not native match");
