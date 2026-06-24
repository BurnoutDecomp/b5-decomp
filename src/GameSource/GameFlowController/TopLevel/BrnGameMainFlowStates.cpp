#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"   // gpMainGameFlowController, SendEvent
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"   // DebugManager::Update during load
#include "GameSource/Resource/BrnGameDataModule.h"   // GameDataModule + BrnGame::GetMainGameDataModule()
#include "GameSource/Sound/Module/BrnRootSoundModule.h"   // RootSoundModule + BrnGame::GetMainSoundModule() (stage 4)
#include "GameSource/Resource/BrnResourceAllocator.h"     // BrnResource::GetGameDataGeneralAllocator() (stage 4)

// Engine clock (same source the loading-screen renderer animates from). Defined in
// CgsTimeUtils.cpp; used here to pace the (currently stubbed) load so it is visible.
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); }

// The game-data streaming module the loading screen brings up (X360 Update 0x823EF688 case 8:
// GameDataModule::Prepare via vtable+64). On X360 it lives in the game module; here a file-static
// instance is driven by the load stage until it reports ready. Its Prepare spine runs the REAL
// resource module-tree bring-up (base + ResourceModule::Prepare chaining Memory/Pool/Bundle/
// FileSystem). The rw-allocator-gated CreateBanks/CreatePools/CreateAllocators inside it are still
// stubs that report success, so this exercises the real module lifecycle without yet allocating
// banks/pools (those fills are step 5a/5b). [NEVER run headless -- the user runs the exe.]
// The game's one GameDataModule lives in BrnGameModule (gGameModule.mGameDataModule), constructed by
// BrnGameModule::Construct; the loading flow (case 8) drives THAT instance via GetMainGameDataModule()
// -- no parallel copy. (gGameModule is a file-scope static -> zero-init BSS + ctor-run, so its stage
// fields are valid and its vtables are set, no calloc/placement-new needed.)
static bool g_bLoggedGameDataPrepare = false;

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
namespace
{
    // The real DWARF/PS3 stage identities (E_LOADINGSTAGE_*). On the X360 ARTIST spine the late
    // stages (Juice/Massive/Replays) DON'T load those subsystems -- see the per-case notes below.
    const char* const kapcStageNames[] = {
        "START", "ControllerModule", "GUIModule", "DirectorModule", "SoundModule",
        "Network", "Juice", "Massive", "Replays", "DONE",
    };

    // [INTERIM PACING] The per-stage LoadXxxModule loads are still stubs, so gate each stage on a
    // short engine-clock dwell (~0.4s) to keep the loading screen visible. The real X360 advances a
    // stage when its async module load reports complete; this dwell is removed per stage as each real
    // LoadXxxModule is reconstructed (the real load then provides the natural timing). Resets itself
    // each time it elapses so the next stage starts a fresh dwell.
    u32 g_uStageStartTick = 0;
    bool StageDwellElapsed()
    {
        const u32 luNow  = CgsSystem::GetSystemTimerBaseTime();
        const u32 luFreq = CgsSystem::GetSystemTimerFrequency();
        if (g_uStageStartTick == 0)
            g_uStageStartTick = luNow;
        if (luFreq != 0 && (luNow - g_uStageStartTick) < (luFreq * 4u / 10u))
            return false;
        g_uStageStartTick = luNow;
        return true;
    }
}

// @ 0x823EF688 - the real boot-load stage machine: load one engine module per stage
// (Controller -> GUI -> Director -> Sound -> Network), then the GameDataModule prepare, then
// FinishLoading. The X360 also creates the GUI IO buffers and renders + bridges the GUI each frame
// while loading (RendererIO + BrnRendererModule::Update + BridgeGameToGui/BridgeGuiToResource/
// BridgeGuiToGame + RenderGUI + GuiEventTimeInfo). That render/bridge loop needs the per-module IO
// types (CgsGui::CgsGuiModuleIO / ViewIO / ModelIO / RendererIO) which aren't reconstructed yet, so
// it is a [follow-on]; for now the GUI + MovieManager keep running through BrnGameModule's inline
// hookup. The per-stage LoadXxxModule bodies are also [stubs] (interim dwell) until reconstructed.
void MainGameFlowStateInitialLoadingScreen::Update()
{
    // The debug manager updates every frame while loading (X360 gates this on stage > Controller).
    if (meLoadingScreenStage > E_LOADINGSTAGE_CONTROLLERMODULE)
    {
        if (CgsDev::DebugManager* lpDebug = CgsDev::DebugManager::ThreadSafeAquire())
        {
            lpDebug->Update(1.0f / 60.0f);
            CgsDev::DebugManager::ThreadSafeRelease(lpDebug);
        }
    }

    switch (meLoadingScreenStage)
    {
    case E_LOADINGSTAGE_START:
    case E_LOADINGSTAGE_CONTROLLERMODULE:
        // X360: LoadControllerModule (0x823C68C0) -- allocates the Controller module's input
        // rw::core::GeneralResourceAllocator from the GameData allocator. [stub: interim dwell;
        // real load = Phase 3]
        meLoadingScreenStage = E_LOADINGSTAGE_CONTROLLERMODULE;
        if (StageDwellElapsed())
            AdvanceLoadingStage(E_LOADINGSTAGE_GUIMODULE);
        break;
    case E_LOADINGSTAGE_GUIMODULE:
        // X360: LoadGUIModule (0x823EF310). The GUI module + MovieManager currently run via
        // BrnGameModule's inline hookup, so this stage only paces here. [real = GUI-IO/bridge phase]
        if (StageDwellElapsed())
            AdvanceLoadingStage(E_LOADINGSTAGE_DIRECTORMODULE);
        break;
    case E_LOADINGSTAGE_DIRECTORMODULE:
        // X360: LoadDirectorModule. [stub: interim dwell; real load = roadmap]
        if (StageDwellElapsed())
            AdvanceLoadingStage(E_LOADINGSTAGE_SOUND_MODULE);
        break;
    case E_LOADINGSTAGE_SOUND_MODULE:
        // X360: LoadSoundModule (0x823E75A8) -> RootSoundModule::Prepare (vtable+64) with the GameData
        // general allocator + the Root IO buffers. Now a REAL load: drive RootSoundModule::Prepare each
        // frame until it reports prepared, then advance. [minimal] the X360 LoadSoundModule also creates
        // RootInput/RootOutput IO buffers via the IOBufferStack + forwards the module's resource requests
        // into the GameData input on the still-preparing path; those are grown when Prepare consumes them
        // (Prepare currently reports prepared immediately).
        {
            static bool s_bLoggedSoundLoad = false;
            if (!s_bLoggedSoundLoad)
            {
                s_bLoggedSoundLoad = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading stage 4 (SoundModule) -- real load\n";
            }
            if (BrnGame::GetMainSoundModule()->Prepare(BrnResource::GetGameDataGeneralAllocator(), 0, 0, 0, 0))
                AdvanceLoadingStage(E_LOADINGSTAGE_NETWORK);
        }
        break;
    case E_LOADINGSTAGE_NETWORK:
        // X360: LoadNetworkModule. [stub]
        if (StageDwellElapsed())
            AdvanceLoadingStage(E_LOADINGSTAGE_JUICE);
        break;
    case E_LOADINGSTAGE_JUICE:
        // [X360 ARTIST divergence] The PS3 DecFIGS build loads Juice here; the X360 ARTIST Update
        // does NOT -- stage 6 is a passthrough. (The enum name is the DWARF/PS3 identity.)
        AdvanceLoadingStage(E_LOADINGSTAGE_MASSIVE);
        break;
    case E_LOADINGSTAGE_MASSIVE:
        // [X360 ARTIST divergence] No Massive load here; the X360 sets the "engine modules loaded"
        // flag (off_830102D0 + 10097260 = 1) at this stage. [follow-on: that game-module flag isn't
        // mapped in this incremental layout]
        AdvanceLoadingStage(E_LOADINGSTAGE_REPLAYS);
        break;
    case E_LOADINGSTAGE_REPLAYS:
        // [X360 ARTIST divergence] Not a Replays load -- this is where the X360 prepares the
        // GameDataModule (case 8: GameDataModule::Prepare via vtable+64). Now WIRED: drive the real
        // GameDataModule::Prepare each frame until it reports ready (its resumable stage machine
        // brings up the resource module tree -- base + ResourceModule::Prepare chaining Memory/Pool/
        // Bundle/FileSystem). The bank/pool/allocator creation inside Prepare is still stubbed
        // (reports success) until step 5a/5b; bundle streaming + file I/O remain a later phase.
        if (!g_bLoggedGameDataPrepare)
        {
            g_bLoggedGameDataPrepare = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: preparing GameDataModule (case 8)\n";
        }
        // Prepare the game's one GameDataModule (already Construct'd by BrnGameModule::Construct).
        if (BrnGame::GetMainGameDataModule()->Prepare(0, 0))
            AdvanceLoadingStage(E_LOADINGSTAGE_DONE);
        break;
    case E_LOADINGSTAGE_DONE:
    default:
        // Load complete (once): drop the renderer's loading-screen signal and advance the flow
        // (FinishLoading -> SendEvent(STATEEND) -> MARKETING_SCREENS). Guarded so the held DONE
        // stage doesn't re-fire every frame.
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

// Advance to the next load stage + log it (the X360 stages are visible in BrnGame.log so the real
// progression can be followed). Kept separate so each stage's real LoadXxxModule (Phase 3+) just
// calls this on its own completion instead of the interim dwell.
void MainGameFlowStateInitialLoadingScreen::AdvanceLoadingStage(ELoadingScreenStage leNextStage)
{
    meLoadingScreenStage = leNextStage;
    if ((CgsDev::Message::gxMessageFilterFlags & 1) &&
        leNextStage >= E_LOADINGSTAGE_START && leNextStage <= E_LOADINGSTAGE_DONE)
    {
        *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading stage "
                                   << (s32)leNextStage << " ("
                                   << kapcStageNames[leNextStage] << ")\n";
    }
}

// @ 0x823C6AC8 - the load is complete: advance the flow. The X360 gates on a completion flag
// (this+0xC) and stamps a game-module load-state field to 5 before firing; here the Update stage
// machine gates the call (it fires FinishLoading once, at DONE) and that game-module field isn't
// mapped in this incremental layout. SendEvent(STATEEND) takes LOADING -> MARKETING_SCREENS.
void MainGameFlowStateInitialLoadingScreen::FinishLoading()
{
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
}

// --- remaining leaves (minimal until each state's TU is reconstructed) ------------------
MainGameFlowStateStartScreen::MainGameFlowStateStartScreen() : meLoadingStage(E_STARTSCREEN_START) {}
void MainGameFlowStateStartScreen::OnEnter() {}
void MainGameFlowStateStartScreen::OnLeave() {}
void MainGameFlowStateStartScreen::Update() {}
void MainGameFlowStateStartScreen::Render() {}

// MARKETING_SCREENS is a LoadingScriptedState (module loading) -- NOT a movie player. The boot/attract
// videos are owned by the GUI module's HUD flow (BrnGui::BootVideos plays the HD/EA/Criterion logos;
// BrnGui::BootLegal/BootAttract play the post-title attract), driven by GUI events with data from
// VIDEOS\VIDEOLIST.BUNDLE (see the movie postmortem). Until the real module-loading body is reconstructed,
// this passes straight through to the next flow state.
MainGameFlowStateMarketingScreens::MainGameFlowStateMarketingScreens() {}

void MainGameFlowStateMarketingScreens::OnEnter()
{
    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "MarketingScreens: OnEnter\n";
}

void MainGameFlowStateMarketingScreens::OnLeave() {}

void MainGameFlowStateMarketingScreens::Update()
{
    if (BrnGameMainFlowController::gpMainGameFlowController != 0)
        BrnGameMainFlowController::gpMainGameFlowController->SendEvent(BrnGameMainFlowController::E_MGE_STATEEND);
}

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
