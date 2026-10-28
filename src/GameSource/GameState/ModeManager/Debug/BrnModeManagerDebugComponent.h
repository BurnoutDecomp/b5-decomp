#pragma once

#include "types.hpp"
#include "DebugSystem/Core/CgsDebugComponent.h"               // CgsDev::DebugComponent (real base)
// [stuntrace waveB fix round, 2026-08-26] INCLUDE -> FORWARD DECLARATION, to break a cycle.
// BrnModeManager.h must embed ModeManagerDebugComponent BY VALUE (console ModeManager+28112)
// and cannot while this header includes it back. Nothing below needs the definition: the class
// only stores and calls through a ModeManager*, and BrnModeManagerDebugComponent.cpp now
// includes the owning header itself.
namespace BrnGameState { class ModeManager; }

// BrnGameState::ModeManagerDebugComponent - the in-game debug menu for the mode manager. Derives
// from the real CgsDev::DebugComponent and registers the mode-manager tunables + an "end current
// event" action with the debug UI. Recovered from the DecFIGS DWARF (GameState/ModeManager/Debug/
// BrnModeManagerDebugComponent.h). Incremental: only the activation slice this TU implements
// (OnActivate / GetName / the FinshMode action + Construct) is declared; the full component's
// render + online-player-list methods are reconstructed by their own pass.

namespace BrnGameState
{
    // ARTIST globals at 0x82CDB7BC..0x82CDB7C8. The debug menu and SurvivorMode::Start
    // share these same backing values; ARTIST's image defaults are 2, 4, 5.0f, and 5.0f.
    extern s32 giMarkedManMinOpponentCount;
    extern s32 giMarkedManMaxOpponentCount;
    extern f32 gfMarkedManMinRampTime;
    extern f32 gfMarkedManMaxRampTime;

    class ModeManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(ModeManager* lpModeManager);   // @ BrnModeManagerDebugComponent.cpp:200

    protected:
        const char* GetName() const ;          // @ BrnModeManagerDebugComponent.cpp:231
        // ARTIST vtable 0x820CDDEC, slot +0x10: original menu hierarchy.
        const char* GetPath() const  { return "Gameplay"; }
        void        OnActivate() ;             // @ BrnModeManagerDebugComponent.cpp:243
        void        RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) ; // X360 0x8231EC30

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
