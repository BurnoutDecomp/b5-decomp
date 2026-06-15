#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"

namespace BrnGameMainFlowController
{
    // @ 0x823C6440 - wire the per-state pointer table (indexed by EMainGameFlowState) to the
    // owned state objects, then start with no active state (SetState enters the first one).
    void GameMainFlowController::Construct()
    {
        maStates.GetItem(E_MGS_INITIAL_LOADING_SCREEN) = &mInitialLoadingScreen;
        maStates.GetItem(E_MGS_CHECK_DISK_SPACE)       = &mCheckDiskSpace;
        maStates.GetItem(E_MGS_MARKETING_SCREENS)      = &mMarketingScreens;
        maStates.GetItem(E_MGS_START_SCREEN)           = &mStartScreen;
        maStates.GetItem(E_MGS_MEMORY_CARD)            = &mMemoryCard;
        maStates.GetItem(E_MGS_COMPLETE_LOADING)       = &mCompleteLoading;
        maStates.GetItem(E_MGS_IN_GAME)                = &mInGame;

        meCurrentState  = E_MGS_INVALID;
        mbSaveLoadState = false;
        mbVideoState    = false;
        mbInGameState   = false;
    }

    // Transition: leave the current state, enter the new one. The X360 SendEvent/SetState pair
    // drives transitions via the per-state event tables; this is the core enter/leave plumbing.
    void GameMainFlowController::SetState(EMainGameFlowState leState)
    {
        if (meCurrentState != E_MGS_INVALID)
            maStates.GetItem((u32)meCurrentState)->OnLeave();
        meCurrentState = leState;
        if (leState != E_MGS_INVALID)
            maStates.GetItem((u32)leState)->OnEnter();
    }

    void GameMainFlowController::Update()
    {
        if (meCurrentState != E_MGS_INVALID)
            maStates.GetItem((u32)meCurrentState)->Update();
    }

    void GameMainFlowController::Render()
    {
        if (meCurrentState != E_MGS_INVALID)
            maStates.GetItem((u32)meCurrentState)->Render();
    }

    // Event-driven transitions (E_MGE_STATEEND/SKIP/GAMEOVER -> next state) are reconstructed
    // with the per-state event tables; the active state advances itself via SetState for now.
    void GameMainFlowController::SendEvent(EMainGameFlowEvent) {}

    bool GameMainFlowController::IsSaveLoadState() const { return mbSaveLoadState; }
    bool GameMainFlowController::IsVideoState() const    { return mbVideoState; }
    bool GameMainFlowController::IsInGameState() const   { return mbInGameState; }
    void GameMainFlowController::SetVideoState(bool lbVideoState)       { mbVideoState = lbVideoState; }
    void GameMainFlowController::SetSaveLoadState(bool lbSaveLoadState) { mbSaveLoadState = lbSaveLoadState; }
    void GameMainFlowController::SetInGameState(bool lbInGameState)     { mbInGameState = lbInGameState; }
}
