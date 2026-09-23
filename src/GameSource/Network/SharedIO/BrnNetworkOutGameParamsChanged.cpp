// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkOutGameParamsChanged.cpp
// ============================================================================
// BrnNetwork::BrnNetworkModuleIO::NetworkOutGameParamsChanged::Construct.
//
// The console inlines the default construction of the ten round events (three stores each:
// the trigger id = the light-trigger format tag, the landmark count 0 and the event id 0;
// Event::Construct with no landmarks stores exactly those three words), then stamps the
// scalar defaults.

#include "GameSource/Network/SharedIO/BrnNetworkOutGameParamsChanged.h"

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // The LightTriggerId format tag in the high byte: the default (no-light) trigger handle.
    static const u32 KU_LIGHT_TRIGGER_ID_TAG = 0x39000000u;

    void NetworkOutGameParamsChanged::Construct()
    {
        for (s32 liEvent = 0; liEvent < KI_NUM_EVENTS; ++liEvent)
        {
            maEvents[liEvent].Construct(0, KU_LIGHT_TRIGGER_ID_TAG, 0, 0);
        }

        meSecurity          = 0;
        meBoostType         = 0;
        meVehicleChoice     = 0;
        miTimeLimit         = 0;
        mbRanked            = true;
        miNumRounds         = 3;
        mePreviousGameMode  = 18;
        miVehicleClass      = 9;
        meGameMode          = 10;
        miNumRunnerCrashes  = 3;
        mbInfiniteBoost     = true;
        mbTrafficOn         = true;
        mbTrafficCheckingOn = true;
    }
}
}
