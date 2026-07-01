#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.h"   // gpMainGameFlowController, SendEvent
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"   // DebugManager::Update during load
#include "GameSource/Resource/BrnGameDataModule.h"   // GameDataModule + BrnGame::GetMainGameDataModule()
#include "GameSource/Resource/BrnGameDataModuleIO.h" // GameDataIO::InputBuffer/OutputBuffer (LoadSoundModule args)
#include "GameSource/Sound/Module/BrnRootSoundModule.h"   // RootSoundModule + BrnGame::GetMainSoundModule() (stage 4)
#include "GameSource/Game/BrnGameModule.hpp"         // BrnGame::GetMainGameModule() (the update IO stacks)

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

// The boot loading screen is owned by the GUI flow's BF_LOADING state (BrnGui::BootLoading), which
// shows it (PlayLoadingScreen) and dismisses it (StopLoadingScreen) through its StateInterface -- the
// faithful path (the X360 GUI flow drives the loading-screen APT movie). When BrnGui::GuiModule has
// that FSM live it sets gBrnGuiDrivesLoadingScreen, and this game-flow state stops touching
// gBrnLoadingScreenShouldShow (the GUI drives it); it only does the loading work and, once finished,
// raises gBrnInitialLoadingComplete so the GUI flow advances BF_LOADING -> BF_VIDEOS. If the GUI FSM
// is NOT live (gBrnGuiDrivesLoadingScreen false), this state keeps managing the screen itself (the
// prior behaviour) so the boot never loses its loading screen.
bool gBrnInitialLoadingComplete = false;
bool gBrnGuiDrivesLoadingScreen = false;   // set by BrnGui::GuiModule when the BF_LOADING Lua FSM is live

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

// @ 0x823E75A8 - one frame of the sound-module load. The X360 body:
//   * CreateIOBuffer<Io::RootInputBuffer>(gm->mpUpdateInputBufferStack,  &lpRootIn,  "Sound")
//     CreateIOBuffer<Io::RootOutputBuffer>(gm->mpUpdateOutputBufferStack, &lpRootOut, "Sound")
//   * prepared = gm->mSoundModule.Prepare(lpGameDataOutputBuffer->GetAllocatorList(),
//                    gm->mpUpdateInputBufferStack, gm->mpUpdateOutputBufferStack,
//                    lpRootIn, lpRootOut)                                   (vtable +64)
//   * still preparing: LockForRead(lpRootOut); append the RootOutputBuffer's AttribSys queue
//     (VariableEventQueue<32768,16>::Append<2048,16>) + its resource-request interface
//     (AppendRequestInterface<4096>) into lpGameDataInputBuffer; UnlockForRead.
//   * destroy the two buffers (output first) and return `prepared`.
bool LoadingScriptedState::LoadSoundModule(BrnResource::GameDataIO::InputBuffer* lpGameDataInputBuffer,
                                           const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutputBuffer)
{
    BrnGame::BrnGameModule* lpGameModule = BrnGame::GetMainGameModule();
    CgsModule::IOBufferStack* lpUpdateInputStack  = lpGameModule->GetUpdateInputBufferStack();
    CgsModule::IOBufferStack* lpUpdateOutputStack = lpGameModule->GetUpdateOutputBufferStack();

    BrnSound::Module::Io::RootInputBuffer*  lpRootInputBuffer  = 0;
    BrnSound::Module::Io::RootOutputBuffer* lpRootOutputBuffer = 0;
    lpUpdateInputStack->CreateIOBuffer<BrnSound::Module::Io::RootInputBuffer>(&lpRootInputBuffer, "Sound");
    lpUpdateOutputStack->CreateIOBuffer<BrnSound::Module::Io::RootOutputBuffer>(&lpRootOutputBuffer, "Sound");

    // [follow-on] The loading state's per-frame GameData IO bracket (the X360 Update creates the
    // GameData input/output buffers each frame and threads them through every LoadXxxModule) is
    // not reconstructed yet, so the buffers arrive null here; the allocator list is then null and
    // Prepare's carve stages stay allocator-gated (see BrnRootSoundModule.cpp).
    const BrnResource::GameDataIO::AllocatorList* lpAllocatorList =
        lpGameDataOutputBuffer ? &lpGameDataOutputBuffer->GetAllocatorList() : 0;

    bool lbPrepared = BrnGame::GetMainSoundModule()->Prepare(
        lpAllocatorList, lpUpdateInputStack, lpUpdateOutputStack,
        lpRootInputBuffer, lpRootOutputBuffer);

    if (!lbPrepared && lpGameDataInputBuffer)
    {
        // Still preparing: forward the module's resource requests into the GameData input.
        // [gated] the RootOutputBuffer request interfaces + getters (the X360's
        // GetAttribSysRequestInterface / GetResourceRequestInterface reads under LockForRead)
        // are still the minimal Io slice; the forwarding lands when those members do.
    }

    lpUpdateOutputStack->DestroyIOBuffer<BrnSound::Module::Io::RootOutputBuffer>(&lpRootOutputBuffer);
    lpUpdateInputStack->DestroyIOBuffer<BrnSound::Module::Io::RootInputBuffer>(&lpRootInputBuffer);
    return lbPrepared;
}

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
    gBrnInitialLoadingComplete = false;
    // Show the loading screen immediately (no first-frame gap). When the GUI BF_LOADING FSM is live it
    // also shows + later dismisses the screen; here we only ensure it's up from frame 0.
    gBrnLoadingScreenShouldShow = true;

    if (CgsDev::Message::gxMessageFilterFlags & 1)
        *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: OnEnter - loading screen shown\n";
}

// @ 0x823AA9E8 - clear the loading-screen signal on exit (unless the GUI BF_LOADING state owns it,
// in which case BootLoading::OnLeave -> StopLoadingScreen drops it).
void MainGameFlowStateInitialLoadingScreen::OnLeave()
{
    if (!gBrnGuiDrivesLoadingScreen)
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
        // X360: LoadSoundModule (0x823E75A8) -- the real per-frame load: create the Root sound IO
        // buffer pair on the update IO stacks, drive RootSoundModule::Prepare (vtable+64) until it
        // reports prepared, forwarding its resource requests meanwhile, then advance. [follow-on]
        // the X360 Update threads this frame's GameData input/output buffers into every
        // LoadXxxModule (the per-frame GameData IO bracket at the top of the real Update body);
        // that bracket isn't reconstructed yet, so the args are null (see LoadSoundModule).
        {
            static bool s_bLoggedSoundLoad = false;
            if (!s_bLoggedSoundLoad)
            {
                s_bLoggedSoundLoad = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading stage 4 (SoundModule) -- real load\n";
            }
            if (LoadSoundModule(0, 0))
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
        // Load complete (once): raise the completion signal + advance the flow (FinishLoading ->
        // SendEvent(STATEEND) -> MARKETING_SCREENS). The GUI BF_LOADING state watches
        // gBrnInitialLoadingComplete and dismisses the loading screen itself (StopLoadingScreen) as it
        // proceeds to BF_VIDEOS; only when the GUI isn't driving do we drop the screen here. Guarded so
        // the held DONE stage doesn't re-fire every frame.
        if (!gBrnInitialLoadingComplete)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "InitialLoadingScreen: loading complete\n";
            FinishLoading();
            gBrnInitialLoadingComplete = true;
            if (!gBrnGuiDrivesLoadingScreen)
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

// --- MainGameFlowStateCheckDiskSpace ----------------------------------------------------
// Standalone byte global the X360 OnEnter clears-then-sets (byte_82FAE28E). It is NOT a member
// (the X360 stores to an absolute .data address, not a `this`-relative slot): it is set to 0 on
// entry and 1 once OnEnter has finished its stage stamp -- a "disk-check OnEnter has run" flag.
bool gBrnCheckDiskSpaceEntered = false;

MainGameFlowStateCheckDiskSpace::MainGameFlowStateCheckDiskSpace() {}

// @ 0x823AAA98 - clear the entered-flag, assert we arrived at the START stage, then stamp the
// game-module load-state slot to 4 and raise the entered-flag. (No disk/storage query is issued
// here -- the X360 body is just these flag writes + the START-stage assert + the stage stamp.)
void MainGameFlowStateCheckDiskSpace::OnEnter()
{
    gBrnCheckDiskSpaceEntered = false;

    // X360 gated the assert on the global assert-enabled flag (dword_82FAE4B0); CGS_ASSERT subsumes
    // that gate. The X360 reports this from BrnGameMainFlowStates.h:195 (the OnEnter decl site).
    CGS_ASSERT(meLoadingStateStage == E_LOADINGSTATESTAGE_START,
               "meLoadingStateStage == E_LOADINGSTATESTAGE_START");

    gBrnCheckDiskSpaceEntered = true;

    // X360: *(off_830102D0 + 0x9A0644) = 4 -- stamp the value 4 into a game-module-aggregate slot
    // (the same off_830102D0 base that holds the flow controller at +0x9A0664 and the loading-screen
    // signal at +0x99FE38). The recovered immediate is 4. [follow-on] that game-module field isn't
    // mapped in this incremental layout (same status as the +10097260 modules-loaded flag above), so
    // the stamp has no modelled target yet; the flag writes + assert above are faithful.
}
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
