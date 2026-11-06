#define private public
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include <stddef.h>
#define PROBE(n, field) extern "C" unsigned n() { return offsetof(BrnTraffic::TrafficEntityModule, field); }
PROBE(camera, mCameraLastFrame)
PROBE(worst, mbDEBUGWorstCase)
PROBE(pre, miPerfMon_PreSceneUpdate)
PROBE(physics, miPerfMon_PrePhysicsUpdate)
PROBE(streamer, mStreamer)
PROBE(specs, maTrafficVehiclePhysicsSpecs)
PROBE(runtime, maVehicleTypeRuntime)
PROBE(info, maTrafficPhysicsInfoList)
PROBE(bits, maTrafficPhysicsInfoListBits)
extern "C" unsigned camera_size() { return sizeof(BrnDirector::Camera::Camera); }
extern "C" unsigned streamer_size() { return sizeof(BrnTraffic::TrafficCarStreamer); }
extern "C" unsigned spec_size() { return sizeof(CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>); }
