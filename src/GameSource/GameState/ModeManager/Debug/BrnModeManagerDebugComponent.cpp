#include "GameSource/GameState/ModeManager/Debug/BrnModeManagerDebugComponent.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The mode-manager debug menu registers a handful of
// mode tunables (its own + the mode manager's + some global marked-man tweaks) and an "end event"
// action with the real CgsDev::DebugComponent debug-menu API (RegisterVariable / SetRange /
// RegisterFunction). The X360 issues these through the (unnamed) DebugComponent registration
// helpers; they are the base-class methods reconstructed in CgsDebugComponent.h, called by name.

namespace BrnGameState
{
    // Global marked-man tweakables exposed by this menu (defined in their own TUs).
    extern s32 giMarkedManMinOpponentCount;
    extern s32 giMarkedManMaxOpponentCount;
    extern f32 gfMarkedManMinRampTime;
    extern f32 gfMarkedManMaxRampTime;

    const char* ModeManagerDebugComponent::GetName() const
    {
        return "Mode Manager";
    }

    void ModeManagerDebugComponent::OnActivate()
    {
        RegisterVariable(&mpModeManager->mbEndlessStuntRun, "Endless Stunt Run");
        RegisterVariable(&mbInfiniteLives, "Infinite lives");
        RegisterVariable(&mbShowModeInfo, "Show mode info");
        RegisterVariable(&mpModeManager->mbWinIfSecond, "Win if second");
        RegisterVariable(&giMarkedManMinOpponentCount, "Marked man tweaks", "Min opponent count");
        RegisterVariable(&giMarkedManMaxOpponentCount, "Marked man tweaks", "Max opponent count");
        RegisterVariable(&gfMarkedManMinRampTime, "Marked man tweaks", "Min ramp time");
        RegisterVariable(&gfMarkedManMaxRampTime, "Marked man tweaks", "Max ramp time");
        RegisterVariable(&miFinishPosition, "Finish Position");
        SetRange(&miFinishPosition, 1, 8);
        RegisterFunction(&ModeManagerDebugComponent::FinshMode, this, "End Current Event");
    }

    // Force the current event to finish at the menu-selected finishing position.
    void ModeManagerDebugComponent::FinshMode(void* lpContext)
    {
        ModeManagerDebugComponent* lpThis = static_cast<ModeManagerDebugComponent*>(lpContext);
        lpThis->mpModeManager->mbFinishCurrentEvent = true;
        lpThis->mpModeManager->miFinishPosition = lpThis->miFinishPosition;
    }
}
