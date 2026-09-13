// Translation-unit embed check for BrnWorld::RaceCarEntityModule's accessor pair.
// Forces the owning header to compile standalone and exercises both accessors, so
// the array element types and the accessor return types stay wired together.
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"

namespace
{
// No element-size lock lives here. This TU used to assert sizeof(RaceCar) == 0xB0
// and sizeof(ActiveRaceCar) == 0x1CD0: those are the CONSOLE strides the accessor
// arithmetic bakes in, and they were assertable only while this header defined its
// own opaque byte-array stand-ins for the two element types. Those stand-ins were an
// ODR fork and are long deleted -- the arrays now hold the real RaceCar /
// ActiveRaceCar, both of which carry pointers the console stored in four bytes, so
// on the host the element sizes are larger and are not a parity fact to pin. Parity
// for this module is by named member, and the one offset that still holds
// (maRaceCars) is locked in the header, where offsetof has member-scope access.

void EmbedCheck(BrnWorld::RaceCarEntityModule* lpModule)
{
    BrnWorld::ActiveRaceCar* lpActive =
        lpModule->GetActiveRaceCar(E_ACTIVE_RACE_CAR_INDEX_0);
    BrnWorld::RaceCar* lpGlobal =
        lpModule->GetGlobalRaceCar(E_GLOBAL_RACE_CAR_INDEX_0);
    (void)lpActive;
    (void)lpGlobal;
}
}
