#pragma once

#include "types.hpp"
#include "DebugSystem/Core/CgsDebugComponent.h"                              // CgsDev::DebugComponent (real base)
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"   // EActiveRaceCarIndex

// BrnGameState::TakedownManagerDebugComponent - the in-game debug menu for the takedown manager.
// Derives from the real CgsDev::DebugComponent; registers three display toggles + a "force
// takedown" action with the debug UI. Recovered from the DecFIGS DWARF (GameState/TakedownManager/
// BrnTakedownManagerDebugComponent.h). Incremental: the activation slice this TU implements
// (OnActivate / GetName / the ForceTakedown action) is reconstructed; the render + record methods
// are reconstructed by their own pass.

namespace BrnGameState
{
    struct TakedownManager;   // pointer member; full definition in BrnTakedownManager.h (the .cpp includes it)

    class TakedownManagerDebugComponent : public CgsDev::DebugComponent
    {
    protected:
        const char* GetName() const override;   // @ 0x823596C8
        void        OnActivate() override;       // @ 0x82366378

    private:
        // The "Force takedown" action callback registered with the debug menu; the void* context
        // the menu passes back is this component (registered via RegisterFunction(..., this, ...)).
        static void ForceTakedownCallback(void* lpContext);   // @ 0x823597F8

        TakedownManager*    mpTakedownManager;
        EActiveRaceCarIndex meLastAggressorIndex;
        EActiveRaceCarIndex meLastVictimIndex;
        f32                 mfLastTakedownContactTime;
        bool                mbShowVulnerability;
        bool                mbShowLastTakedownInfo;
        bool                mbShowRevengeTakedownInfo;
    };
}
