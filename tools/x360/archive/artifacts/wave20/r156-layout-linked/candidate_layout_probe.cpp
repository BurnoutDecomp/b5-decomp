#include "types.hpp"
#define private public
#define protected public
#include "GameSource/World/BrnWorldModule.h"
#include "GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiserFrame.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
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
VAL(replay_snapshot,offsetof(BrnReplays::TrafficEntitySerialiserStaticLayout,mTrafficLightManager))
VAL(replay_flags,offsetof(BrnReplays::TrafficEntitySerialiserStaticLayout,mTrafficLightManager) + offsetof(TrafficLightRuntimeState,mxStateBits))
VAL(replay_size,sizeof(BrnReplays::TrafficEntitySerialiserStaticLayout))
VAL(replay_align,__alignof(BrnReplays::TrafficEntitySerialiserStaticLayout))
VAL(world_align,__alignof(BrnWorld::WorldModule))
VAL(module_align,__alignof(TrafficEntityModule))
VAL(light_record,sizeof(TrafficLightRuntimeState))
VAL(light_flags,offsetof(TrafficLightRuntimeState,mxStateBits))
VAL(phys_manager_size,sizeof(BrnPhysics::Vehicle::PhysicalTrafficManager))
VAL(phys_entities,offsetof(BrnPhysics::Vehicle::PhysicalTrafficManager,maTrafficEntityIDs))
VAL(phys_vehicles,offsetof(BrnPhysics::Vehicle::PhysicalTrafficManager,mpaTrafficVehicles))
VAL(phys_joints,offsetof(BrnPhysics::Vehicle::PhysicalTrafficManager,mArticulatedJointPool))
VAL(phys_used,offsetof(BrnPhysics::Vehicle::PhysicalTrafficManager,mUsedTrafficVehicles))
VAL(phys_map,offsetof(BrnPhysics::Vehicle::PhysicalTrafficManager,mu8GlobalToPhysicalEntityIndexMap))
VAL(phys_input_articulated,offsetof(BrnPhysics::Vehicle::VehicleInputInterface,mCreateArticulatedTrafficEventQueue))
VAL(phys_input_size,sizeof(BrnPhysics::Vehicle::VehicleInputInterface))

