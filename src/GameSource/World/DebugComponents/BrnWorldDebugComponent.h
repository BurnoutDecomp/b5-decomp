#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent (real base)

// BrnWorld::WorldDebugComponent - the in-game debug menu/overlay for the world module (target
// markers, debug controller, vehicle-gun, velocity readouts). Derives from the real
// CgsDev::DebugComponent (DecFIGS DWARF BrnWorldDebugComponent.h:46). The full component (the
// WorldModule* tunables + render/update machinery declared in the DWARF, plus the KF_* velocity/
// text constants) is owned by its own dev-UI pass. Incremental: this TU implements ONLY the leaf
// name getter (GetName @0x827DD1F0). It is declared here BY NAME so the body has a real .cpp home.

namespace BrnWorld
{
    class WorldModuleFwd_; // (no-op forward guard)
    class WorldDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // ADDITIVE (WorldModule::Construct @0x827CF540 back-pointer mount).
        void Construct( class WorldModule* lpWorldModule );

    protected:
        // @0x827DD1F0: the debug-menu display name for this component.
        //   asm: lis r11,aWorldModule@ha ; addi r3,r11,aWorldModule@l "World Module" ; blr
        const char* GetName() const override;
    };
}
