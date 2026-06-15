#pragma once

#include "types.hpp"

// Top-level game-flow states. Reconstructed from the X360 ARTIST build
// (GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h). The initial
// loading-screen state drives the boot loading screen: on entry it flags the loading
// screen as active; BrnRendererModule::Render reads that flag and issues SHOW/HIDE to the
// loading-screen renderer (matching the X360 Render's state-driven AddCommand).
//
// Option B: gBrnLoadingScreenShouldShow stands in for the game-module global field the
// original writes (off_830102D0 + 0x99FE38). The full MainGameFlowState base hierarchy
// and BrnGameMainFlowController are reconstructed incrementally.

// Flow-state -> renderer signal: true while the initial loading screen should be shown.
extern bool gBrnLoadingScreenShouldShow;

enum ELoadingStateStage
{
    E_LOADINGSTATESTAGE_START = 0,
};

class MainGameFlowStateInitialLoadingScreen
{
public:
    void OnEnter();        // @ 0x823AA950
    void OnLeave();        // @ 0x823AA9E8
    void FinishLoading();  // @ 0x823C6AC8

private:
    s32  meLoadingStateStage;
    bool mbFinishedLoading;
};
