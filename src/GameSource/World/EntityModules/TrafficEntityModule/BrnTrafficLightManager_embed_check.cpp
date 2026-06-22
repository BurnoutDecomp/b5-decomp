// Embed-check / compile tripwire for BrnTraffic::TrafficLightManager (1 func).
// Pins the X360 layout (8-byte record stride, array at offset 0, bound 0x258) and forces
// the body to instantiate. Not a unit test -- a compile/link guard for this TU.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
#include <cstddef>

namespace
{
    using BrnTraffic::TrafficLightManager;
    using BrnTraffic::TrafficLightState;
    using BrnTraffic::KU_MAX_TRAFFIC_LIGHT_INSTANCES;

    // Layout pins (GetLightState asm: slwi instance,3 stride; add this base; cmplwi 0x258 bound).
    // The array is private and is the manager's sole member, so sizeof(manager) == 8*600
    // proves both the 8-byte stride and the array-at-offset-0 base (no header before it).
    static_assert(sizeof(TrafficLightState) == 8, "8-byte record (instance << 3 stride)");
    static_assert(KU_MAX_TRAFFIC_LIGHT_INSTANCES == 0x258, "bound 0x258 (cmplwi)");
    static_assert(sizeof(TrafficLightManager) == 8 * 0x258, "manager == array of 600 records");

    int ExerciseTrafficLightManager(TrafficLightManager* lpManager, u32 luInstance)
    {
        // &record == (u8*)manager + 8*instance.
        TrafficLightState* lpState = lpManager->GetLightState(luInstance);
        return static_cast<int>(lpState->muStateA + lpState->muStateB);
    }
}

int (*g_BrnTrafficLightManagerEmbedCheck)(BrnTraffic::TrafficLightManager*, u32) =
    &ExerciseTrafficLightManager;
