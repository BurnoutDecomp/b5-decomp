#include <stddef.h>
#define private public
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#undef private
#define CHECK(name, expr) typedef char name[(expr)?1:-1]
using namespace BrnTraffic;
CHECK(vehicles, offsetof(TrafficEntityModule,maVehicles)==0x2a80);
CHECK(axles, offsetof(TrafficEntityModule,maVehicleAxles)==0x15680);
CHECK(purgatory, offsetof(TrafficEntityModule,mTrailerPurgatoryList)==0x3e9f8);
CHECK(free_stack, offsetof(TrafficEntityModule,mFreeTrailerStack)==0x3ea00);
CHECK(crash_queue, offsetof(TrafficEntityModule,maEmergencyCrashingVehicles)==0x572f0);
CHECK(info, offsetof(TrafficEntityModule,maTrafficPhysicsInfoList)==0x58210);
CHECK(info_bits, offsetof(TrafficEntityModule,maTrafficPhysicsInfoListBits)==0x713a0);
CHECK(specs, offsetof(TrafficEntityModule,maTrafficVehiclePhysicsSpecs)==0x75b58);
CHECK(runtime, offsetof(TrafficEntityModule,maVehicleTypeRuntime)==0x76380);
CHECK(physics_event_queue, offsetof(BrnPhysics::Vehicle::VehicleInputInterface,mCreateArticulatedTrafficEventQueue)==0x21590);
