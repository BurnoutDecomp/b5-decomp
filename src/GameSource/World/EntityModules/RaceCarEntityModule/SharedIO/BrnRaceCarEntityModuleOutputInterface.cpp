// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/
//   BrnRaceCarEntityModuleOutputInterface.cpp
//
// The two per-index element accessors the X360 build emitted out-of-line on the
// race-car output interfaces.
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/BurnoutConstants.h"            // EActiveRaceCarIndex enumerators
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{

// X360 0x8227D690 -- per-index MUTABLE element accessor into maRaceCarStates[8]
// (RaceCarState is 1120 bytes; array starts at this+816). Bounds-checks the index then
// gates on IsRaceCarActive before returning &maRaceCarStates[idx]. DISTINCT from the const
// GetRaceCarState (returns const RaceCarState*); named GetRaceCarStateMutable to avoid the
// const/non-const name collision. EActiveRaceCarIndex is an enum, so it indexes directly.
RCEntityActiveRaceCarOutputInterface::RaceCarState*
RCEntityActiveRaceCarOutputInterface::GetRaceCarStateMutable(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,    "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(IsRaceCarActive(leActiveRaceCarIndex),               "IsRaceCarActive( leActiveRaceCarIndex )");
    return &maRaceCarStates[leActiveRaceCarIndex];
}

// X360 0x823101C0 -- hidden per-index element-address accessor on the GLOBAL race-car
// output interface. Bounds-checks the index (NO IsRaceCarActive gate, NO IOBuffer lock bit
// -- this interface is a plain payload struct), then returns the address of a 36-byte
// per-car window at 36*idx + 528 from the interface base. The struct is a struct-of-
// parallel-35-element-arrays, so this stride-36 window is a synthesized view the scoring/
// challenge callers index opaquely; per the DWARF signature it is returned as const void*.
const void*
RCEntityGlobalRaceCarOutputInterface::GetActiveCarDataElementAddress(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,    "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    const u8* lpcBase = reinterpret_cast<const u8*>(this);
    return static_cast<const void*>(lpcBase + (36 * static_cast<s32>(leActiveRaceCarIndex)) + 528);
}

}
}
