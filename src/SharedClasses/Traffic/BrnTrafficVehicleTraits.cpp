// =============================================================================
// BrnTrafficVehicleTraits.cpp -- owning .cpp for BrnTraffic::VehicleTraits. The two
// getters (:49 / :50) are inline in the header, as the console inlines them too.
// What lands here is the never-called layout pin, a member function because every
// data member is private. PARK: EndianSwap (:55) is declared-only; see the header.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficVehicleTraits.h"
#include <cstddef>   // offsetof

namespace BrnTraffic
{
    void VehicleTraits::_AssertLayout()
    {
        static_assert(offsetof(VehicleTraits, mfSwervingAmountModifier) == 0x00, "mfSwervingAmountModifier");
        static_assert(offsetof(VehicleTraits, mfAcceleration)           == 0x04, "mfAcceleration");
        static_assert(offsetof(VehicleTraits, muCuttingUpChance)        == 0x08, "muCuttingUpChance");
        static_assert(offsetof(VehicleTraits, muTailgatingChance)       == 0x09, "muTailgatingChance");
        static_assert(offsetof(VehicleTraits, muPatience)               == 0x0A, "muPatience");
        static_assert(offsetof(VehicleTraits, muTantrumAttackCumProb)   == 0x0B, "muTantrumAttackCumProb");
        static_assert(offsetof(VehicleTraits, muTantrumStopCumProb)     == 0x0C, "muTantrumStopCumProb");

        // GetVehicleTraitsForVehicleType @0x82705DF0 addresses the table as
        // `mpaVehicleTraits + traitsId*16` (`slwi r11,r24,4`). Pointer-free record, so
        // the same 16 bytes on the host.
        static_assert(sizeof(VehicleTraits) == 16, "VehicleTraits stride");
    }
}
