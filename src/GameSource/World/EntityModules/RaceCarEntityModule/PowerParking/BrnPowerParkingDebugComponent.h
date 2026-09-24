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
        // DWARF BrnPowerParkingDebugComponent.cpp:42 / :54. Neither has an X360 symbol: both are
        // inlined through PowerParkingManager::Construct / Destruct into RaceCarEntityModule::
        // Construct (0x822FDB1C) and ::Destruct (0x822F3DC0) -- see the bodies (crash parity
        // FX-SCENEMGR item 4, 2026-09-24).
        void Construct(PowerParkingManager* lpPowerParkingManager);
        void Destruct();

        void Update() override;

    protected:
        const char* GetName() const override;
        // DWARF :95. X360 vtable 0x820CDF20 slot 4 is 0x82312480, the ICF-folded `return "Gameplay"`
        // it shares with CrashPlayDebugComponent::GetPath (PS3 0x126F10 is its own copy).
        const char* GetPath() const override;
        void        OnActivate() override;

    private:
        PowerParkingManager* mpPowerParkingManager;
    };
}

#endif
