#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Engine clock (same source the loading-screen renderer animates from). Defined in
// CgsTimeUtils.cpp; used here to pace the (currently stubbed) load so it is visible.
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); }

// Option B stand-in for the game-module global the original loading-screen state writes
// (off_830102D0 + 0x99FE38). BrnRendererModule::Render reads it to show the loading screen.
bool gBrnLoadingScreenShouldShow = false;

// --- MainGameFlowState (abstract base) --------------------------------------------------
MainGameFlowState::MainGameFlowState() {}
void MainGameFlowState::OnEnter() {}
void MainGameFlowState::OnLeave() {}
void MainGameFlowState::Update() {}
void MainGameFlowState::Render() {}

// --- LoadingScriptedState ---------------------------------------------------------------
LoadingScriptedState::LoadingScriptedState()
    : meLoadingStateStage(E_LOADINGSTATESTAGE_START)
    , mbLoadingPaused(false)
{
}
void LoadingScriptedState::OnEnter() {}
void LoadingScriptedState::OnLeave() {}
void LoadingScriptedState::Update() {}
void LoadingScriptedState::Render() {}
void LoadingScriptedState::FinishLoading() {}

// --- MainGameFlowStateInitialLoadingScreen (the boot loading screen) --------------------
MainGameFlowStateInitialLoadingScreen::MainGameFlowStateInitialLoadingScreen()
    : meLoadingScreenStage(E_LOADINGSTAGE_START)
    , mpInputModuleAllocator(0)
    , mbGuiPreloadDone(false)
{
}

// @ 0x823AA950 - reset the load stages to START and raise the renderer's loading-screen
// signal (Option B bridge for the game-module global the X360 writes).
void MainGameFlowStateInitialLoadingScreen::OnEnter()
{
    meLoadingStateStage  = E_LOADINGSTATESTAGE_START;
    meLoadingScreenStage = E_LOADINGSTAGE_START;
    mbGuiPreloadDone     = false;
    gBrnLoadingScreenShouldShow = true;

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: OnEnter - loading screen shown\n";
}

// @ 0x823AA9E8 - clear the loading-screen signal on exit.
void MainGameFlowStateInitialLoadingScreen::OnLeave()
{
    gBrnLoadingScreenShouldShow = false;
}

// @ 0x823EF688 - the scripted module-by-module load. The X360 body loads one module per stage
// (controller -> GUI -> director -> sound -> network -> juice -> massive -> replays) via the
// LoadXxxModule helpers, advancing meLoadingScreenStage as each completes. Reconstructed here
// as the real stage-machine control flow; the per-stage module loads are filled in as each
// module TU is reconstructed. Advances one stage per update and holds at DONE (the screen
// stays up until the next flow state is reconstructed).
void MainGameFlowStateInitialLoadingScreen::Update()
{
    // DEMO PACING (temporary): the per-stage LoadXxxModule calls are still stubs (instant), so
    // each stage would complete in one frame and the screen would flash by. Pace the stage
    // machine by the engine clock (~0.4s/stage) so the load is visible. When the real
    // per-module loads are reconstructed, their actual load time provides the pacing and this
    // gate is removed (the stage advances when the current module finishes loading).
    static u32 s_uStageStartTick = 0;
    const u32 luNow  = CgsSystem::GetSystemTimerBaseTime();
    const u32 luFreq = CgsSystem::GetSystemTimerFrequency();
    if (s_uStageStartTick == 0)
        s_uStageStartTick = luNow;
    if (luFreq != 0 && (luNow - s_uStageStartTick) < (luFreq * 4u / 10u))
        return;
    s_uStageStartTick = luNow;

    static const char* const kapcStageNames[] = {
        "START", "ControllerModule", "GUIModule", "DirectorModule", "SoundModule",
        "Network", "Juice", "Massive", "Replays", "DONE",
    };
    if ((CgsDev::Message::gxMessageFilterFlags & 1) &&
        meLoadingScreenStage >= E_LOADINGSTAGE_START && meLoadingScreenStage < E_LOADINGSTAGE_DONE)
    {
        *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading stage "
                                   << (s32)meLoadingScreenStage << " ("
                                   << kapcStageNames[meLoadingScreenStage] << ")\n";
    }

    switch (meLoadingScreenStage)
    {
    case E_LOADINGSTAGE_START:
        meLoadingScreenStage = E_LOADINGSTAGE_CONTROLLERMODULE;
        break;
    case E_LOADINGSTAGE_CONTROLLERMODULE:   // LoadControllerModule
        meLoadingScreenStage = E_LOADINGSTAGE_GUIMODULE;
        break;
    case E_LOADINGSTAGE_GUIMODULE:          // LoadGUIModule
        meLoadingScreenStage = E_LOADINGSTAGE_DIRECTORMODULE;
        break;
    case E_LOADINGSTAGE_DIRECTORMODULE:     // LoadDirectorModule
        meLoadingScreenStage = E_LOADINGSTAGE_SOUND_MODULE;
        break;
    case E_LOADINGSTAGE_SOUND_MODULE:       // LoadSoundModule
        meLoadingScreenStage = E_LOADINGSTAGE_NETWORK;
        break;
    case E_LOADINGSTAGE_NETWORK:            // LoadNetworkModule
        meLoadingScreenStage = E_LOADINGSTAGE_JUICE;
        break;
    case E_LOADINGSTAGE_JUICE:
        meLoadingScreenStage = E_LOADINGSTAGE_MASSIVE;
        break;
    case E_LOADINGSTAGE_MASSIVE:
        meLoadingScreenStage = E_LOADINGSTAGE_REPLAYS;
        break;
    case E_LOADINGSTAGE_REPLAYS:            // LoadReplayModule
        meLoadingScreenStage = E_LOADINGSTAGE_DONE;
        CGS_ASSERT(false, "Asserts are working!");
        break;
    case E_LOADINGSTAGE_DONE:
    default:
        // Load complete (once): drop the renderer's loading-screen signal so it fades out
        // (E_LSC_HIDE). The full game here issues SendEvent(E_MGE_STATEEND) -> the next flow
        // state (start screen) takes over; that transition lands when those states are
        // reconstructed. Guarded so the held DONE stage doesn't re-log every frame.
        if (gBrnLoadingScreenShouldShow)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading complete\n";
            FinishLoading();
            gBrnLoadingScreenShouldShow = false;
        }
        break;
    }
}

// @ 0x823C6AC8 - once the world has finished loading, advance the flow (SendEvent STATEEND);
// reconstructed with the controller's event tables.
void MainGameFlowStateInitialLoadingScreen::FinishLoading() {}

// --- remaining leaves (minimal until each state's TU is reconstructed) ------------------
MainGameFlowStateStartScreen::MainGameFlowStateStartScreen() : meLoadingStage(E_STARTSCREEN_START) {}
void MainGameFlowStateStartScreen::OnEnter() {}
void MainGameFlowStateStartScreen::OnLeave() {}
void MainGameFlowStateStartScreen::Update() {}
void MainGameFlowStateStartScreen::Render() {}

MainGameFlowStateMarketingScreens::MainGameFlowStateMarketingScreens() {}
void MainGameFlowStateMarketingScreens::OnEnter() {}
void MainGameFlowStateMarketingScreens::OnLeave() {}
void MainGameFlowStateMarketingScreens::Update() {}
void MainGameFlowStateMarketingScreens::Render() {}

MainGameFlowStateCheckDiskSpace::MainGameFlowStateCheckDiskSpace() {}
void MainGameFlowStateCheckDiskSpace::OnEnter() {}
void MainGameFlowStateCheckDiskSpace::OnLeave() {}
void MainGameFlowStateCheckDiskSpace::Update() {}
void MainGameFlowStateCheckDiskSpace::Render() {}

MainGameFlowStateMemoryCard::MainGameFlowStateMemoryCard() {}
void MainGameFlowStateMemoryCard::OnEnter() {}
void MainGameFlowStateMemoryCard::OnLeave() {}
void MainGameFlowStateMemoryCard::Update() {}
void MainGameFlowStateMemoryCard::Render() {}

MainGameFlowStateCompleteLoading::MainGameFlowStateCompleteLoading() : mbIsCollisionWorldPrepared(false) {}
void MainGameFlowStateCompleteLoading::OnEnter() {}
void MainGameFlowStateCompleteLoading::OnLeave() {}
void MainGameFlowStateCompleteLoading::Update() {}
void MainGameFlowStateCompleteLoading::Render() {}

MainGameFlowStateInGame::MainGameFlowStateInGame() {}
void MainGameFlowStateInGame::OnEnter() {}
void MainGameFlowStateInGame::OnLeave() {}
void MainGameFlowStateInGame::Update() {}
void MainGameFlowStateInGame::Render() {}
