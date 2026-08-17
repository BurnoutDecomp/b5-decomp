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

        // FLAG: ARTIST 0x823C6500-0x6508 stores 0 (E_MGS_INITIAL_LOADING_SCREEN) into +0x50, NOT -1
        // (DecFIGS 0x2E2408 confirms *(this+80)=0). Construct leaves the controller already in state 0,
        // so the first SendEvent's "meCurrentState in [0,7)" assert holds before any explicit SetState.
        meCurrentState  = E_MGS_INITIAL_LOADING_SCREEN;
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

    // ---- the three FSM state bytes: gm+0x9A06B8 / +0x9A06B9 / +0x9A06BA ----------------
    //
    // CONSUMER MAP (completed 2026-08-17, boot audit F-P4-12). The F-P3-6 map that the F4
    // byte-wiring was built against listed only the update-set consumer. There are two more
    // readers in the image, and both were missing from it:
    //
    //   0x9A06B8 (saveload)  ConstructUpdateSetFromFsm @0x823BD420  -> update-set bit 0x40
    //   0x9A06B9 (video)     ConstructUpdateSetFromFsm             -> update-set bit 0x20
    //                        DoUpdate_GameStatePreWorld @0x823EE0E8 -> read @0x823EE3D8  [+]
    //   0x9A06BA (ingame)    ConstructUpdateSetFromFsm             -> update-set bit 0x88
    //                        AutoTestManager::DumpGameState @0x823C04B8 -> read @0x823C04D0 [+]
    //
    // [+] Both extra readers sit in surface that is dead on this build -- the DoUpdate ladder
    // (F-P3-1) and the autotest lane -- so wiring the bytes did not need them and nothing
    // changed by their absence. They are recorded HERE, beside the setters, because the next
    // person to change what these bytes mean will look at the writers and needs to know the
    // full set of things that will start reading them when those two surfaces come back.
    bool GameMainFlowController::IsSaveLoadState() const { return mbSaveLoadState; }
    bool GameMainFlowController::IsVideoState() const    { return mbVideoState; }
    bool GameMainFlowController::IsInGameState() const   { return mbInGameState; }
    void GameMainFlowController::SetVideoState(bool lbVideoState)       { mbVideoState = lbVideoState; }
    void GameMainFlowController::SetSaveLoadState(bool lbSaveLoadState) { mbSaveLoadState = lbSaveLoadState; }
    void GameMainFlowController::SetInGameState(bool lbInGameState)     { mbInGameState = lbInGameState; }
}
