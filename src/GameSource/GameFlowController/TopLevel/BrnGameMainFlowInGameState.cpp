#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"   // GameMainFlowController + EMainGameFlowState
#include "GameSource/Game/BrnGameModule.hpp"                                    // BrnGame::GetMainGameModule / DoUpdate / DoDispatch
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT

// MainGameFlowStateInGame per-frame surface -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU is the DWARF home (BrnGameMainFlowInGameState.cpp) for the in-game flow state's
// OnEnter/OnLeave/Render/Update; the ctor stays with the sibling group in
// BrnGameMainFlowStates.cpp.
//
// Bodied here (4 ledger functions):
//   MainGameFlowStateInGame::OnEnter  @ 0x823AACF8
//   MainGameFlowStateInGame::OnLeave  @ 0x823AAD28
//   MainGameFlowStateInGame::Render   @ 0x823E79B8
//   MainGameFlowStateInGame::Update   @ 0x823F3048

namespace BrnGameMainFlowController
{
    // Option B stand-ins for the game-module aggregate fields the X360 body accesses by
    // absolute offset (off_830102D0 == BrnGame::GetMainGameModule()); those fields are not
    // mapped in the incremental game-module layout, so -- following the
    // gBrnLoadingScreenShouldShow / gBrnDiskSpaceCheckComplete bridge-global precedent --
    // they are modelled here:
    //   gBrnInGameStateActive       stands in for *(base + 0x9A06BA) (byte) -- "in-game state active"
    //   gBrnGameModuleLoadState     stands in for *(base + 0x9A0644) (word) -- game-module load-state slot
    //   gBrnReturnToFrontEndRequested stands in for *(base + 0x9A0626) (byte) -- "return to front-end" poll
    bool gBrnInGameStateActive = false;
    s32  gBrnGameModuleLoadState = 0;
    bool gBrnReturnToFrontEndRequested = false;
}

// @ 0x823AACF8 -- entering the in-game flow state. Two absolute-offset stores into the
// game-module aggregate: raise the "in-game state active" flag and stamp the load-state slot to 5.
void MainGameFlowStateInGame::OnEnter()
{
    BrnGameMainFlowController::gBrnInGameStateActive = true;
    BrnGameMainFlowController::gBrnGameModuleLoadState = 5;   // X360 immediate: 5 (E_MGS_COMPLETE_LOADING)
}

// @ 0x823AAD28 -- leaving the in-game flow state: clear the "in-game state active" flag OnEnter
// raised (*(base + 0x9A06BA) = 0). The load-state word (+0x9A0644) is NOT touched here.
void MainGameFlowStateInGame::OnLeave()
{
    BrnGameMainFlowController::gBrnInGameStateActive = false;
}

// @ 0x823E79B8 -- in-game render is a straight tail-forward to the game module's render-dispatch
// spine (BrnGame::BrnGameModule::DoDispatch, off_830102D0 == BrnGame::GetMainGameModule()). The
// void Render slot discards DoDispatch's int.
void MainGameFlowStateInGame::Render()
{
    BrnGame::GetMainGameModule()->DoDispatch();
}

// @ 0x823F3048 -- in-game per-frame update. Run the game module's update spine; then, if the
// "return to front-end" byte is set, clear it and drive an inline IN_GAME -> START_SCREEN
// transition through the flow controller.
//
// The X360 reads meCurrentState (controller + 0x50), asserts it is in [0, E_MGS_COUNT), then
// switches: states 0-5 fall through unchanged; state 6 (IN_GAME) INLINES OnLeave(IN_GAME) +
// meCurrentState = START_SCREEN(3) + OnEnter(START_SCREEN) -- NOT a SetState call (SetState would
// additionally guard != E_MGS_INVALID); any other value hits the assert.
void MainGameFlowStateInGame::Update()
{
    BrnGame::GetMainGameModule()->DoUpdate();

    if (!BrnGameMainFlowController::gBrnReturnToFrontEndRequested)
        return;
    BrnGameMainFlowController::gBrnReturnToFrontEndRequested = false;

    BrnGameMainFlowController::GameMainFlowController* lpController =
        BrnGameMainFlowController::gpMainGameFlowController;
    if (lpController == 0)   // Option B null guard (X360 reaches the live controller off the module global)
        return;

    const BrnGameMainFlowController::EMainGameFlowState leState = lpController->GetCurrentState();
    CGS_ASSERT(leState >= 0 && leState < BrnGameMainFlowController::E_MGS_COUNT,
               "meCurrentState >= 0 && meCurrentState < E_MGS_COUNT");

    switch (leState)
    {
    case BrnGameMainFlowController::E_MGS_INITIAL_LOADING_SCREEN:
    case BrnGameMainFlowController::E_MGS_CHECK_DISK_SPACE:
    case BrnGameMainFlowController::E_MGS_MARKETING_SCREENS:
    case BrnGameMainFlowController::E_MGS_START_SCREEN:
    case BrnGameMainFlowController::E_MGS_MEMORY_CARD:
    case BrnGameMainFlowController::E_MGS_COMPLETE_LOADING:
        return;
    case BrnGameMainFlowController::E_MGS_IN_GAME:
        // Inline IN_GAME -> START_SCREEN transition (X360 unrolls OnLeave + stamp + OnEnter here).
        lpController->GetState(BrnGameMainFlowController::E_MGS_IN_GAME)->OnLeave();
        lpController->SetCurrentStateRaw(BrnGameMainFlowController::E_MGS_START_SCREEN);
        lpController->GetState(BrnGameMainFlowController::E_MGS_START_SCREEN)->OnEnter();
        break;
    default:
        CGS_ASSERT(false, "Unknown main game state.\n");
        break;
    }
}
