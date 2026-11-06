#include <cstddef>
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface VehicleSnapshot;

CGS_STATIC_ASSERT((offsetof(VehicleSnapshot, maCarsInTheRace) == 0x0000),
                  "native vehicle snapshot starts with maCarsInTheRace");
CGS_STATIC_ASSERT((sizeof(VehicleSnapshot::RaceCarState) == 0x0460),
                  "native RaceCarState copy stride");
CGS_STATIC_ASSERT((sizeof(VehicleSnapshot) == 0x28F0),
                  "native copied vehicle interface extent");
