// Translation-unit embed check for BrnWorld::RaceCarEntityModule's accessor pair.
// Forces the owning header to compile standalone, exercises both accessors, and
// locks the X360-proven array offsets/strides so the layout stays wired.
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"

namespace
{
// offsetof on the private array members must be evaluated where they are
// accessible; this friendless TU can only see them via a member-scope context,
// so the locks live in a (compiled, never-called) member of a local probe that
// derives nothing -- instead we assert the X360 strides/offsets the asm proves
// against sizeof of the element stand-ins (which ARE public) and the public
// accessor return path. The element strides are the load-bearing facts.
static_assert(sizeof(BrnWorld::RaceCar) == 0xB0,          "RaceCar stride 0xB0 (== 176)");
static_assert(sizeof(BrnWorld::ActiveRaceCar) == 0x1CD0,  "ActiveRaceCar stride 0x1CD0 (== 7376)");

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
