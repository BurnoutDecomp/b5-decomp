// Embed check for CgsDev::DebugUI::Console (Construct 0x8282E6B0, ToggleShow 0x828292A0,
// Update 0x8282FC68). References the reconstructed surface so the symbols are emitted; the bodies
// reach the debug singleton (GetUI()) so this only exercises the addresses, it is not run.
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Console/CgsConsole.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

namespace
{
    void ExerciseConsole(CgsDev::DebugManagerConstructParameters* lpParameters)
    {
        CgsDev::DebugUI::Console lConsole;

        lConsole.Construct(lpParameters);
        lConsole.ToggleShow();
        lConsole.Update(0.016f, CgsDev::DebugUI::E_INPUTEVENT_NONE);

        lConsole.Enable();
        lConsole.Disable();
        (void)lConsole.IsVisible();
        (void)lConsole.IsEnabled();

        lConsole.Destruct();
    }
}

extern void CgsConsole_embed_check_anchor(CgsDev::DebugManagerConstructParameters* lpParameters);
void CgsConsole_embed_check_anchor(CgsDev::DebugManagerConstructParameters* lpParameters) { ExerciseConsole(lpParameters); }
