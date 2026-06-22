#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGameMainFlowController
{
    // Option B bridge to the active controller (X360 reaches it via the game-module global).
    GameMainFlowController* gpMainGameFlowController = nullptr;

    // @ 0x823C6440 - wire the per-state pointer table (indexed by EMainGameFlowState) to the
    // owned state objects, then start with no active state (SetState enters the first one).
    void GameMainFlowController::Construct()
    {
        gpMainGameFlowController = this;   // register the active controller (X360 game-module global)
        // X360 0x823C6440: store N(7) into maStates' count word (@ +0x4C) BEFORE the per-state
        // assignments -- maStates is a fixed index->state lookup table that is fully populated by
        // index, so its live count is its capacity (the checked GetItem then accepts indices 0..6).
        maStates.SetFullCount();
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

    // @ 0x823C6530 - the event-driven transition: switch on the active state, advance on the event.
    // Faithful to the X360 switch (it inlines OnLeave(current)+OnEnter(next); SetState does exactly
    // that, so each arm calls SetState). The boot path rides STATEEND: LOADING -> MARKETING -> START
    // -> MEMORY_CARD -> COMPLETE_LOADING -> IN_GAME. (CHECK_DISK_SPACE also -> MARKETING but isn't on
    // the LOADING path.) IN_GAME additionally takes GAMEOVER(0) -> START_SCREEN.
    void GameMainFlowController::SendEvent(EMainGameFlowEvent leEvent)
    {
        CGS_ASSERT(meCurrentState >= 0 && meCurrentState < E_MGS_COUNT,
                   "meCurrentState >= 0 && meCurrentState < E_MGS_COUNT");
        switch (meCurrentState)
        {
            case E_MGS_INITIAL_LOADING_SCREEN:
                if (leEvent == E_MGE_STATEEND) SetState(E_MGS_MARKETING_SCREENS);
                break;
            case E_MGS_CHECK_DISK_SPACE:
                if (leEvent == E_MGE_STATEEND) SetState(E_MGS_MARKETING_SCREENS);
                break;
            case E_MGS_MARKETING_SCREENS:
                if (leEvent == E_MGE_STATEEND) SetState(E_MGS_START_SCREEN);
                break;
            case E_MGS_START_SCREEN:
                if (leEvent == E_MGE_STATEEND) SetState(E_MGS_MEMORY_CARD);
                break;
            case E_MGS_MEMORY_CARD:
                if (leEvent == E_MGE_STATEEND) SetState(E_MGS_COMPLETE_LOADING);
                break;
            case E_MGS_COMPLETE_LOADING:
                if (leEvent == E_MGE_STATEEND) SetState(E_MGS_IN_GAME);
                break;
            case E_MGS_IN_GAME:
                if (leEvent == E_MGE_GAMEOVER)       SetState(E_MGS_START_SCREEN);
                else if (leEvent == E_MGE_STATEEND)  SetState(E_MGS_MEMORY_CARD);
                break;
            default:
                CGS_ASSERT(false, "Unknown main game state.\n");
                break;
        }
    }

    bool GameMainFlowController::IsSaveLoadState() const { return mbSaveLoadState; }
    bool GameMainFlowController::IsVideoState() const    { return mbVideoState; }
    bool GameMainFlowController::IsInGameState() const   { return mbInGameState; }
    void GameMainFlowController::SetVideoState(bool lbVideoState)       { mbVideoState = lbVideoState; }
    void GameMainFlowController::SetSaveLoadState(bool lbSaveLoadState) { mbSaveLoadState = lbSaveLoadState; }
    void GameMainFlowController::SetInGameState(bool lbInGameState)     { mbInGameState = lbInGameState; }
}
