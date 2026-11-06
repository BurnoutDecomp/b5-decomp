#include "types.hpp"
#define private public
#define protected public
#include "GameSource/World/BrnWorldModule.h"
#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiserFrame.h"
#undef private
#undef protected
#include <stddef.h>
using namespace BrnTraffic;
#define VAL(n,e) extern "C" unsigned int n() { return (unsigned int)(e); }
VAL(manager_size,sizeof(TrafficLightManager))
VAL(manager_align,__alignof(TrafficLightManager))
VAL(module_size,sizeof(TrafficEntityModule))
VAL(light,offsetof(TrafficEntityModule,mTrafficLightManager))
VAL(add_hulls,offsetof(TrafficEntityModule,mHullsToAddTriggersFor))
VAL(crashed,offsetof(TrafficEntityModule,maNewCrashedVehicles))
VAL(removed,offsetof(TrafficEntityModule,maNewRemovedVehicles))
VAL(recovered,offsetof(TrafficEntityModule,maRecentlyRecoveredSlammedTraffic))
VAL(physics,offsetof(TrafficEntityModule,maTrafficPhysicsInfoList))
VAL(bits,offsetof(TrafficEntityModule,maTrafficPhysicsInfoListBits))
VAL(race_state,offsetof(TrafficEntityModule,mRaceCarState))
VAL(data,offsetof(TrafficEntityModule,mpData))
VAL(camera,offsetof(TrafficEntityModule,mCameraLastFrame))
VAL(monitor,offsetof(TrafficEntityModule,miPerfMon_PrePhysicsUpdate))
VAL(streamer,offsetof(TrafficEntityModule,mStreamer))
VAL(specs,offsetof(TrafficEntityModule,maTrafficVehiclePhysicsSpecs))
VAL(types,offsetof(TrafficEntityModule,maVehicleTypeRuntime))
VAL(world_traffic,offsetof(BrnWorld::WorldModule,mTrafficEntityModule))
VAL(replay_snapshot,offsetof(BrnReplays::TrafficEntitySerialiserStaticLayout,maPad22FB0))
VAL(replay_flags,offsetof(BrnReplays::TrafficEntitySerialiserStaticLayout,maSlotTable))
