#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"   // GameMainFlowController + E_MGE_STATEEND

// MainGameFlowStateCheckDiskSpace::Update -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU is the DWARF home (BrnGameMainFlowCheckDiskSpace.cpp) of the state's Update;
// the rest of the state's surface (ctor/OnEnter/OnLeave/Render) stays with the sibling
// group in BrnGameMainFlowStates.cpp.
//
// Bodied here (1 ledger function):
//   MainGameFlowStateCheckDiskSpace::Update @ 0x823F2D28

namespace BrnGameMainFlowController
{
    // Option B stand-in for the game-module byte the X360 Update polls
    // (off_830102D0 + 0x9A0648, 4 bytes after the +0x9A0644 load-state stamp slot and
    // 28 before the +0x9A0664 flow controller): "the disk-space check has completed" --
    // raised by the platform storage query, consumed here to advance the flow. Follows
    // the gBrnLoadingScreenShouldShow bridge-global pattern (BrnGameMainFlowStates.cpp).
    bool gBrnDiskSpaceCheckComplete = false;
}

// @ 0x823F2D28 -- run the scripted-load base Update, then, once the disk-space check
// has completed, fire STATEEND at the flow controller (X360: SendEvent(base+0x9A0664, 2)).
void MainGameFlowStateCheckDiskSpace::Update()
{
    LoadingScriptedState::Update();

    if (BrnGameMainFlowController::gBrnDiskSpaceCheckComplete)
    {
        if (BrnGameMainFlowController::gpMainGameFlowController != 0)   // Option B null guard (X360 reads the live module global)
            BrnGameMainFlowController::gpMainGameFlowController->SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
    }
}
