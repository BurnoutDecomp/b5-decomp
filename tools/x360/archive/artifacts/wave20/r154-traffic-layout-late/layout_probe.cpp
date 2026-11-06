#define private public
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include <stddef.h>
#define PROBE(n, field) extern "C" unsigned n() { return offsetof(BrnTraffic::TrafficEntityModule, field); }
PROBE(camera, mCameraLastFrame)
PROBE(worst, mbDEBUGWorstCase)
PROBE(avoidptr, mpaDEBUGVehicleAvoidance)
PROBE(avoidcount, muDEBUGVehicleAvoidanceCount)
PROBE(result, mfDEBUGAvoidance_LineTestResultR)
PROBE(score, mfDEBUGAvoidance_LineTestScore)
PROBE(pass, mfDEBUGAvoidance_PassScore)
PROBE(running, mbDEBUGRunningWorstCase)
PROBE(pre, miPerfMon_PreSceneUpdate)
PROBE(physics, miPerfMon_PrePhysicsUpdate)
PROBE(streamer, mStreamer)
PROBE(specs, maTrafficVehiclePhysicsSpecs)
PROBE(runtime, maVehicleTypeRuntime)
extern "C" unsigned camera_size() { return sizeof(BrnDirector::Camera::Camera); }
extern "C" unsigned streamer_size() { return sizeof(BrnTraffic::TrafficCarStreamer); }
extern "C" unsigned spec_size() { return sizeof(CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>); }
