// BrnTrafficDebugComponent_wS34_00.cpp -- BrnTraffic::DebugComponent::Destruct, the teardown
// TrafficEntityModule::Destruct runs on its debug component.

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficDebugComponent.h"

namespace BrnTraffic
{
    // Three instructions, identical-code-folded with another component's Destruct: drop the
    // owning-module back pointer (+0x0C), then tail into the base DebugComponent::Destruct.
    // Unlike the race-car components it does not assert the pointer first.
    void DebugComponent::Destruct()
    {
        mpModule = nullptr;
        CgsDev::DebugComponent::Destruct();
    }
}
