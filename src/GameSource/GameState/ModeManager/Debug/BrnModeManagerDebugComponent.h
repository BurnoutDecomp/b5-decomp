#pragma once

#include "types.hpp"
#include "DebugSystem/Core/CgsDebugComponent.h"               // CgsDev::DebugComponent (real base)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"  // BrnGameState::ModeManager

// BrnGameState::ModeManagerDebugComponent - the in-game debug menu for the mode manager. Derives
// from the real CgsDev::DebugComponent and registers the mode-manager tunables + an "end current
// event" action with the debug UI. Recovered from the DecFIGS DWARF (GameState/ModeManager/Debug/
// BrnModeManagerDebugComponent.h). Incremental: only the activation slice this TU implements
// (OnActivate / GetName / the FinshMode action + Construct) is declared; the full component's
// render + online-player-list methods are reconstructed by their own pass.

namespace BrnGameState
{
    class ModeManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(ModeManager* lpModeManager);   // @ BrnModeManagerDebugComponent.cpp:200

    protected:
        const char* GetName() const override;          // @ BrnModeManagerDebugComponent.cpp:231
        void        OnActivate() override;             // @ BrnModeManagerDebugComponent.cpp:243

    private:
        // The "End Current Event" action callback registered with the debug menu; the void*
        // context the menu passes back is this component (registered via RegisterFunction(..., this, ...)).
        static void FinshMode(void* lpContext);        // @ BrnModeManagerDebugComponent.cpp:273

        ModeManager* mpModeManager;
        bool         mbShowModeInfo;
        bool         mbInfiniteLives;
        s32          miFinishPosition;
    };
}
