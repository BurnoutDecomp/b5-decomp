#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

// BrnGameMainFlowController::GameMainFlowController - the top-level game-flow state machine.
// It owns the seven flow states by value, keeps a parallel pointer table (maStates) indexed
// by EMainGameFlowState, and tracks the active state in meCurrentState. SendEvent/SetState
// drive transitions; Update/Render forward to the active state. BrnGameModule::GameMain
// drives the active state directly each frame (maStates.GetItem(meCurrentState)->Update() per
// simulation sub-step, then ->Render()). Layout + API recovered from the DecFIGS DWARF
// (GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h).
namespace BrnGameMainFlowController
{
    enum EMainGameFlowState
    {
        E_MGS_INVALID = -1,
        E_MGS_INITIAL_LOADING_SCREEN = 0,
        E_MGS_CHECK_DISK_SPACE = 1,
        E_MGS_MARKETING_SCREENS = 2,
        E_MGS_START_SCREEN = 3,
        E_MGS_MEMORY_CARD = 4,
        E_MGS_COMPLETE_LOADING = 5,
        E_MGS_IN_GAME = 6,
        E_MGS_COUNT = 7,
    };

    enum EMainGameFlowEvent
    {
        E_MGE_GAMEOVER = 0,
        E_MGE_SKIP = 1,
        E_MGE_STATEEND = 2,
        E_MGE_COUNT = 3,
    };

    struct GameMainFlowController
    {
        void Construct();
        void Update();
        void Render();

        bool IsSaveLoadState() const;
        bool IsVideoState() const;
        bool IsInGameState() const;
        void SetVideoState(bool lbVideoState);
        void SetSaveLoadState(bool lbSaveLoadState);
        void SetInGameState(bool lbInGameState);
        void SendEvent(EMainGameFlowEvent leEvent);
        void SetState(EMainGameFlowState leState);

        // Active-state accessors used by the game module's per-frame update spine.
        EMainGameFlowState GetCurrentState() const { return meCurrentState; }
        MainGameFlowState* GetState(EMainGameFlowState leState) { return maStates.GetItem((u32)leState); }

    private:
        MainGameFlowStateInitialLoadingScreen mInitialLoadingScreen;
        MainGameFlowStateMarketingScreens     mMarketingScreens;
        MainGameFlowStateCheckDiskSpace       mCheckDiskSpace;
        MainGameFlowStateStartScreen          mStartScreen;
        MainGameFlowStateMemoryCard           mMemoryCard;
        MainGameFlowStateCompleteLoading      mCompleteLoading;
        MainGameFlowStateInGame               mInGame;

        Array<MainGameFlowState*, 7u>         maStates;
        EMainGameFlowState                    meCurrentState;
        bool                                  mbSaveLoadState;
        bool                                  mbVideoState;
        bool                                  mbInGameState;
    };

    // Option B bridge: the active controller, set in Construct(). The X360 reaches the controller
    // through the game-module global (off_830102D0 + 0x9A0664); the flow states use this to fire
    // SendEvent (e.g. the loading screen's FinishLoading -> SendEvent(E_MGE_STATEEND)).
    extern GameMainFlowController* gpMainGameFlowController;
}
