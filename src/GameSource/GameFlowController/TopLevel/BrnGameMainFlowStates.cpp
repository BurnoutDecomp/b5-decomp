#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

// Option B stand-in for the game-module global the original sets (off_830102D0 + 0x99FE38).
bool gBrnLoadingScreenShouldShow = false;

// @ 0x823AA950 - MainGameFlowStateInitialLoadingScreen::OnEnter. Reset the state to its
// start stage and flag the loading screen as active so the renderer shows it. (The X360
// also writes an assert-guard byte and a second game-module flag; those are reconstructed
// with the game-module global.)
void MainGameFlowStateInitialLoadingScreen::OnEnter()
{
    meLoadingStateStage = E_LOADINGSTATESTAGE_START;
    mbFinishedLoading = false;
    gBrnLoadingScreenShouldShow = true;
}

// @ 0x823AA9E8 - clear the loading-screen-active flag when leaving the state.
void MainGameFlowStateInitialLoadingScreen::OnLeave()
{
    gBrnLoadingScreenShouldShow = false;
}

// @ 0x823C6AC8 - once the world has finished loading, advance the flow. The transition
// (set the next stage + GameMainFlowController::SendEvent) is reconstructed with the
// controller; here we record the structure.
void MainGameFlowStateInitialLoadingScreen::FinishLoading()
{
    if (mbFinishedLoading)
    {
        // BrnGameMainFlowController::GameMainFlowController::SendEvent(..., 2) - deferred.
    }
}
