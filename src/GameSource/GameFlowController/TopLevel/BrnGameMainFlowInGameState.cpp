#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ARTIST823AACF8: use the controller embedded in the live main-game object.
void MainGameFlowStateInGame::OnEnter()
{
    BrnGame::BrnGameModule* const lpGameModule = gpBurnoutGame;
    lpGameModule->GetMainFlowStateMachine().SetInGameState(true);
    lpGameModule->RequestGuiFsmStage(5);
}

// ARTIST823AAD28.
void MainGameFlowStateInGame::OnLeave()
{
    gpBurnoutGame->GetMainFlowStateMachine().SetInGameState(false);
}

// ARTIST823E79B8.
void MainGameFlowStateInGame::Render()
{
    gpBurnoutGame->DoDispatch();
}

// ARTIST823F3048: DoUpdate owns the complete frame; then consume game-over.
void MainGameFlowStateInGame::Update()
{
    gpBurnoutGame->DoUpdate();
    BrnGame::BrnGameModule* const lpGameModule = gpBurnoutGame;
    if (!lpGameModule->IsGameOverRequested())
        return;
    lpGameModule->ClearGameOver();
    BrnGameMainFlowController::GameMainFlowController& lrController =
        lpGameModule->GetMainFlowStateMachine();
    const BrnGameMainFlowController::EMainGameFlowState leState = lrController.GetCurrentState();
    if (leState < 0 || leState >= BrnGameMainFlowController::E_MGS_COUNT)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("meCurrentState >= 0 && meCurrentState < E_MGS_COUNT",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameFlowController/TopLevel/BrnGameMainFlowController.cpp", 206);
        CgsDev::Assert::EndAssert();
    }
    switch (lrController.GetCurrentState())
    {
    case BrnGameMainFlowController::E_MGS_INITIAL_LOADING_SCREEN:
    case BrnGameMainFlowController::E_MGS_CHECK_DISK_SPACE:
    case BrnGameMainFlowController::E_MGS_MARKETING_SCREENS:
    case BrnGameMainFlowController::E_MGS_START_SCREEN:
    case BrnGameMainFlowController::E_MGS_MEMORY_CARD:
    case BrnGameMainFlowController::E_MGS_COMPLETE_LOADING:
        return;
    case BrnGameMainFlowController::E_MGS_IN_GAME:
        lrController.GetState(BrnGameMainFlowController::E_MGS_IN_GAME)->OnLeave();
        lrController.SetCurrentStateRaw(BrnGameMainFlowController::E_MGS_START_SCREEN);
        lrController.GetState(BrnGameMainFlowController::E_MGS_START_SCREEN)->OnEnter();
        break;
    default:
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Unknown main game state.\n",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameFlowController/TopLevel/BrnGameMainFlowController.cpp", 297);
        CgsDev::Assert::EndAssert();
        break;
    }
}
