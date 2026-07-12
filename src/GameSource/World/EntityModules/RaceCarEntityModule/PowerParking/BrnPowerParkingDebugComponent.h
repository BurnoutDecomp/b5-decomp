#ifndef BRN_POWER_PARKING_DEBUG_COMPONENT_H
#define BRN_POWER_PARKING_DEBUG_COMPONENT_H

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

// BrnWorld::PowerParkingDebugComponent -- the in-game debug component for the Power Parking
// mini-game scorer (path "Gameplay", name "PowerParking"). Mirrors the committed sibling
// BrnCrashPlayDebugComponent: it holds a pointer to the PowerParkingManager and registers that
// manager's scoring fields with the debug menu on activation.
//
// PowerParkingManager is homed in BrnPowerParkingManager.h (which embeds this component by value),
// so it is forward-declared here (pointer-only use) to avoid an include cycle; the .cpp includes
// the manager header for member access.

namespace BrnWorld
{
    struct PowerParkingManager;

    class PowerParkingDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Update() override;

    protected:
        const char* GetName() const override;
        void        OnActivate() override;

    private:
        PowerParkingManager* mpPowerParkingManager;
    };
}

#endif
