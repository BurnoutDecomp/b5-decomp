#pragma once

#include "types.hpp"

// Top-level game-flow states. Hierarchy reconstructed from the X360 ARTIST build
// (GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h):
//
//   MainGameFlowState (abstract base: OnEnter/OnLeave/Update/Render)
//     +-- LoadingScriptedState (scripted module-by-module load; adds FinishLoading)
//     |     +-- MainGameFlowStateInitialLoadingScreen   (the boot loading screen)
//     |     +-- MainGameFlowStateStartScreen
//     |     +-- MainGameFlowStateMarketingScreens
//     |     +-- MainGameFlowStateCheckDiskSpace
//     |     +-- MainGameFlowStateMemoryCard
//     |     +-- MainGameFlowStateCompleteLoading
//     +-- MainGameFlowStateInGame
//
// The per-frame update spine (BrnGameModule::GameMain) drives the active state's Update()
// (vtable +8) each simulation sub-step and its Render() (vtable +12) once per frame. The
// scripted-load helper methods (LoadXxxModule / RenderGUI / Suspend/ResumeLoadingWorld) are
// non-virtual and omitted here (they don't affect layout/vtable); they are reconstructed
// with each state's OnEnter/Update body.
//
// Option B bridge: gBrnLoadingScreenShouldShow stands in for the game-module global the
// original loading-screen state writes; BrnRendererModule::Render reads it to show the
// loading screen.

extern bool gBrnLoadingScreenShouldShow;

namespace rw { namespace core { struct GeneralResourceAllocator; } }

// --- base -------------------------------------------------------------------------------
struct MainGameFlowState
{
    MainGameFlowState();

    virtual void OnEnter();   // vtable +0
    virtual void OnLeave();   // vtable +4
    virtual void Update();    // vtable +8
    virtual void Render();    // vtable +12
};

// --- scripted-load intermediate ---------------------------------------------------------
struct LoadingScriptedState : public MainGameFlowState
{
    enum ELoadingStateStage
    {
        E_LOADINGSTATESTAGE_START = 0,
        E_LOADINGSTATESTAGE_SOUND_AGAIN = 1,
        E_LOADINGSTATESTAGE_EFFECTS = 2,
        E_LOADINGSTATESTAGE_WORLD = 3,
        E_LOADINGSTATESTAGE_SOUND_BINDPROPS = 4,
        E_LOADINGSTATESTAGE_WORLD_COLLISION = 5,
        E_LOADINGSTATESTAGE_DONE = 6,
        E_LOADINGSTATE_NUM = 7,
    };

    LoadingScriptedState();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
    virtual void FinishLoading();   // vtable +16

protected:
    ELoadingStateStage meLoadingStateStage;
    bool               mbLoadingPaused;
};

// --- leaves -----------------------------------------------------------------------------
struct MainGameFlowStateInitialLoadingScreen : public LoadingScriptedState
{
    enum ELoadingScreenStage
    {
        E_LOADINGSTAGE_START = 0,
        E_LOADINGSTAGE_CONTROLLERMODULE = 1,
        E_LOADINGSTAGE_GUIMODULE = 2,
        E_LOADINGSTAGE_DIRECTORMODULE = 3,
        E_LOADINGSTAGE_SOUND_MODULE = 4,
        E_LOADINGSTAGE_NETWORK = 5,
        E_LOADINGSTAGE_JUICE = 6,
        E_LOADINGSTAGE_MASSIVE = 7,
        E_LOADINGSTAGE_REPLAYS = 8,
        E_LOADINGSTAGE_DONE = 9,
    };

    MainGameFlowStateInitialLoadingScreen();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void FinishLoading();

private:
    // Advance to + log the next load stage (interim helper; each real LoadXxxModule will call this
    // on its own completion). Non-virtual; does not affect layout/vtable.
    void AdvanceLoadingStage(ELoadingScreenStage leNextStage);

protected:
    ELoadingScreenStage                 meLoadingScreenStage;
    rw::core::GeneralResourceAllocator* mpInputModuleAllocator;
    bool                                mbGuiPreloadDone;
};

struct MainGameFlowStateStartScreen : public LoadingScriptedState
{
    enum EStartScreenLoadingStage
    {
        E_STARTSCREEN_START = 0,
        E_STARTSCREEN_WORLD = 1,
        E_STARTSCREEN_SOUND_BINDPROPS = 2,
        E_STARTSCREEN_EFFECTS = 3,
        E_STARTSCREEN_DONE = 4,
        E_STARTSCREEN_NUM = 5,
    };

    MainGameFlowStateStartScreen();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();

private:
    EStartScreenLoadingStage meLoadingStage;
};

struct MainGameFlowStateMarketingScreens : public LoadingScriptedState
{
    MainGameFlowStateMarketingScreens();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
};

struct MainGameFlowStateCheckDiskSpace : public LoadingScriptedState
{
    MainGameFlowStateCheckDiskSpace();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
};

struct MainGameFlowStateMemoryCard : public LoadingScriptedState
{
    MainGameFlowStateMemoryCard();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
};

struct MainGameFlowStateCompleteLoading : public LoadingScriptedState
{
    MainGameFlowStateCompleteLoading();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();

private:
    bool mbIsCollisionWorldPrepared;
};

struct MainGameFlowStateInGame : public MainGameFlowState
{
    MainGameFlowStateInGame();

    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();
    virtual void Render();
};
