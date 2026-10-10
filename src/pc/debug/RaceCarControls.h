#pragma once
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleDebugComponent.h"
namespace CgsPC::Debug
{
    // The early INI registrations and the original component share these globals.
    // Keep one set of rows and one metadata record for each original variable.
    inline bool BeginVehicleLodRegistration()
    {
        static bool sbRegistered = false;
        if (sbRegistered) return false;
        sbRegistered = true;
        return true;
    }
    // FLAG PC-platform leaf: the recovered component's full vtable is now
    // available. Supply native ownership at the original Prepare registration
    // point until the owning module's embedded diagnostic storage is homed.
    inline void AttachRaceCarControls(BrnWorld::RaceCarEntityModule& lrModule)
    {
        static BrnWorld::RaceCarEntityModuleDebugComponent sComponent;
        static bool sbAttached = false;
        if (!sbAttached)
        {
            sComponent.Construct(&lrModule);
            sComponent.Register();
            sbAttached = true;
        }
    }
}
