#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"   // GameMainFlowController + E_MGE_STATEEND
#include "GameSource/Game/BrnGameModule.hpp"                                    // IsGuiPhaseComplete (the gm+0x9A0648 byte)

// MainGameFlowStateCheckDiskSpace::Update -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU is the DWARF home (BrnGameMainFlowCheckDiskSpace.cpp) of the state's Update;
// the rest of the state's surface (ctor/OnEnter/OnLeave/Render) stays with the sibling
// group in BrnGameMainFlowStates.cpp.
//
// Bodied here (1 ledger function):
//   MainGameFlowStateCheckDiskSpace::Update @ 0x823F2D28

// ⭐ RE-KEYED 2026-08-16 (boot audit F-P4-8). This used to poll a private
// `gBrnDiskSpaceCheckComplete` bool that NOTHING ever wrote, described as "raised by the
// platform storage query" -- a producer that does not exist on either target. The model was
// invented, and it made the state's only exit condition permanently false.
//
// The byte the console actually polls, gm+0x9A0648, is not a disk-space result at all: it is
// the SAME GUI phase-complete byte that InitialLoadingScreen's mbGuiPreloadDone latch reads
// and that BridgeGuiToGame @0x823CB758 raises from GUI command 70. Every one of these states
// keys on it. On PC that is BrnGameModule::mbGuiPhaseComplete, exposed as IsGuiPhaseComplete().
//
// This state is unreachable on both targets (SendEvent's transition set never targets it --
// boot audit P4), so nothing observable changes; what changes is that the condition now names
// a real signal with a real producer instead of a fiction.

// @ 0x823F2D28 -- run the scripted-load base Update, then, once the GUI phase byte is up,
// fire STATEEND at the flow controller (X360: SendEvent(base+0x9A0664, 2)).
void MainGameFlowStateCheckDiskSpace::Update()
{
    LoadingScriptedState::Update();

    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    if (lpGameModule != 0 && lpGameModule->IsGuiPhaseComplete())
    {
        if (BrnGameMainFlowController::gpMainGameFlowController != 0)   // Option B null guard (X360 reads the live module global)
            BrnGameMainFlowController::gpMainGameFlowController->SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
    }
}
