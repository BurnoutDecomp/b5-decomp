#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert
#include "GameShared/GameClasses/Development/DebugSystem/CgsDebugFontBringUp.h"   // LoadAndSetDebugFont
#include "SDKs/EA/GameTalk/GameTalk.h"               // EA::GameTalk::GameTalkMessage (RenderMetricsMessageHandler)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"      // BrnGui::GuiAudioTriggerEvent (GUI-out event 201)
#include "GameShared/GameClasses/System/PC/CgsGuiSoundPC.h" // GUI presentation-sound PC consumer

#include <cstring>   // memset
#include <string.h>  // _stricmp (RenderMetricsMessageHandler; MSVC canonical, not declared by <cstring>)
#include <chrono>    // the ToGui repeat-clock feed (FLAG PC time source: wall clock)

#include "GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h" // CgsInput::InputPadsPC (the PC pad-fill leaf)
#include "GameShared/GameClasses/Gui/CgsGuiModule.h" // CgsGui::GuiModule::AddGuiEvent (the world-load report below)
#include "GameSource/Game/BrnLoadingScreenRenderer.h" // BrnGame::ELoadingScreenCommand (BridgeGuiToGame's command slot)
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h" // ValidityAccount::SetupFailFlagMask (interim bridge in Construct)
#include "GameSource/Resource/BrnGameDataModuleIO.h" // GameDataIO::InputBuffer/OutputBuffer (GamePrepare's request bracket)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"          // DirectorIO::InputBuffer (DoUpdate_Director)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOSceneQuery.h" // DirectorIO::SceneQuery{Input,Output}Buffer

// The in-game flow-state latch (BrnGameMainFlowInGameState.cpp) -- the world-load
// stand-in below keys its loading-complete report on it.
namespace BrnGameMainFlowController { extern bool gBrnInGameStateActive; }

// ---- Xbox 360 XDK entry point (real prototype lives in the XDK). Displays the system
// "dirty disc" error UI for the given signed-in user. ------------------------------------
extern "C" unsigned long XShowDirtyDiscErrorUI(unsigned long dwUserIndex);

namespace BrnGame
{
    // File-scope globals the X360 GameMain references:
    //   byte_82FAEB90  - the debug force-assert toggle (_lbForceAssert); fires a guarded
    //                    assert when set so a build can be forced to break into the assert path.
    //   dword_83084890 - a gate cleared while the simulation sub-steps run and set once they
    //                    complete; the dispatch/render side reads it. File-scope here, promoted
    //                    to a shared global when its other readers are reconstructed.
    static bool         _lbForceAssert = false;
    static volatile s32 sbSimUpdateComplete = 0;

    BrnGameModule::BrnGameModule()
        : mpUpdateInputBufferStack(0)
        , mpUpdateOutputBufferStack(0)
        , mbSimPaused(false)
        , mbDiskError(false)
        , mbStalled(false)
        , mbRequestDoStepFrame(false)
        , mbRequestDoPlayFrame(false)
        , meGamePrepareStage(E_GAMEPREPARESTAGE_START)
        , meGameUpdateStage(E_GAMEUPDATESTAGE_PREPARE)
        , miNumSimFramesRequired(0)
        , mfDebugUpdateDeltaSeconds(0.0f)
        , mfDebugUpdateTimeScale(0.0f)
        , meFrameRateManagerType(CgsSystem::E_FRAMERATEMANAGER_SINGLE)
        , mi8FrameRateMinSteps(1)
        , mi8FrameRateMaxSteps(1)
        , mi8ActualFrameRateMinStepsThisFrame(1)
        , mi8ActualFrameRateMaxStepsThisFrame(1)
        , mbSteppingFrames(false)
        , mbDoStep(false)
        , mbStopStepping(false)
        , mpGuiInputBuffer(0)
        , mpGuiViewInputBuffer(0)
        , mpGuiModelOutputBuffer(0)
        , mpGuiOutputBuffer(0)
        , mpDirectorOutputBuffer(0)
        , mbGamePrepareReceiverQueueConstructed(false)
        , miInputModuleState(0)
        , miPlayer0ControllerPort(0)
        , miSecondaryControllerPort(0)
        , mbGuiAcceptsControllerInput(false)
        , mbGuiSuppressMenuAccept(false)
        , miLanguageCycleTimerLo(0)
        , mfLanguageCycleTimerFrac(0.0f)
        , miRenderMetricsRequested(0)
    {
        // (The X360 ctor does not touch mCpuMonitors -- Construct() sentinel-fills and
        // registers the monitors, at its X360 position.)
    }

    BrnGameModule::~BrnGameModule()
    {
    }

    // @ BrnGameModule.cpp:155 (X360 0x823C9EA8) - construct the game module: base, debug
    // systems, the CPU monitor registrations, then EVERY engine module (X360 order), then the
    // flow state machine. Sites the X360 body has that are still gated on unreconstructed
    // members/subsystems are flagged inline.
    void BrnGameModule::Construct()
    {
        // X360 step 1: the module base (resets the prepare/release stages + constructs the two
        // DataBuffers -- flag/pointer init only).
        CgsModule::ModuleSingleBuffered::Construct();

        // [gated] X360 steps 2-3: CheckClassSizes, then the game-state field seeds at
        // +10094156..+10097260 (release stage = 7, module counts 11/4, the engine-modules-loaded
        // flag = 0) -- those words fall in this layout's omitted member ranges.

        // Per-frame IO buffer stacks the update spine + the scripted module loads allocate their
        // per-frame IO buffers on. [PC stand-in] On the X360 these five stacks are OWNED BY main()
        // (0x827E60D8) -- UpdateInput/UpdateOutput 0x780000, ResourceInput 0x40000, ResourceOutput
        // 0x20000, Dispatch 0x18000 bytes, align 128, carved from the boot allocator -- and handed
        // to BrnGameModule::Prepare (vtable +64, 0x823DB848), which stores them at +10055424..
        // Until that Prepare + the allocator layer land, two fixed scratch blocks stand in.
        static CgsModule::IOBufferStack sUpdateInputStack;
        static CgsModule::IOBufferStack sUpdateOutputStack;
        // FLAG PC-platform leaf: sizing. The DOCUMENTED X360 value is 0x780000 each (main()
        // @0x827E60D8), but that is a 32-bit-pointer budget: on this LLP64 host every IO
        // buffer carrying pointers/handles is wider than its console twin, and
        // WorldModule::Update pushes ~30 of them (several ~800 KB) in one frame. With
        // 0x780000 the world drive overflowed the stacks mid-frame -- CreateIOBuffer then
        // returns NULL and the failure only surfaces later as a "not Constructed" assert on
        // the buffer that was never allocated (IOBufferStack::Alloc now names the overflow
        // when it happens). Doubled to 0x1000000 each; the console ratio is documented above.
        static u8 saUpdateInputMem[0x1000000];
        static u8 saUpdateOutputMem[0x1000000];
        sUpdateInputStack.Construct("UpdateInput");
        sUpdateInputStack.Prepare(saUpdateInputMem, sizeof(saUpdateInputMem), 16);
        sUpdateOutputStack.Construct("UpdateOutput");
        sUpdateOutputStack.Prepare(saUpdateOutputMem, sizeof(saUpdateOutputMem), 16);
        mpUpdateInputBufferStack = &sUpdateInputStack;
        mpUpdateOutputBufferStack = &sUpdateOutputStack;

        // The dispatch-thread input pair (X360: DispatchThreadInputBufferManager::Construct
        // @0x823CBCE0 from Prepare @0x823DB848, on the dedicated Dispatch stack main() carves
        // -- 0x18000 bytes, see the stack note above). [PC stand-in placement: constructed
        // here with the other stack scaffolding until the real Prepare stage-machine lands;
        // the stack size is the documented console value.]
        static CgsModule::IOBufferStack sDispatchStack;
        static u8 saDispatchMem[0x18000];
        sDispatchStack.Construct("Dispatch");
        sDispatchStack.Prepare(saDispatchMem, sizeof(saDispatchMem), 128);
        mDispatchThreadInputBufferManager.Construct(&sDispatchStack);

        // X360 step 4: the debug manager (params block seeded from unk_820DC120 + overrides +
        // Allocators::mpInternalDebugAllocator as the RW allocator; the PC slice passes DEFAULT).
        // ConstructRenderer is the PC bring-up of the 2D debug renderer so the overlay draws over
        // the loading screen. [gated] the "Force Quit to Start Menu" debug toggle + the FOPEN
        // "%sMAP_ARTIST.BIN" memory-map name globals that follow on the X360.
        mDebugManager.Construct(&CgsDev::DebugManagerConstructParameters::DEFAULT);
        mDebugManager.ConstructRenderer();

        // X360 step 5: sentinel-fill the CPU monitor handle block (BrnCpuMonitors::Construct
        // 0x823A90A8), then register all 40 monitors. Rows are (name, page, minimum, budget-ms,
        // libperf-tagged), transcribed store-for-store from 0x823C9EA8 (the 5-arg AddMonitor:
        // on PPC the float budget occupies the r6 slot, so r7 is the libperf flag).
        mCpuMonitors.Construct();
        {
            using CgsDev::PerfMonCpu::AddMonitor;
            typedef CgsDev::PerfMonCpuPage EPage;
            mCpuMonitors.miUT_TotalUpdate      = AddMonitor("UT: Total simulation",          (EPage)0,  false, 210.0f, false);
            mCpuMonitors.miUT_EachUpdate       = AddMonitor("UT: Each sim step",             (EPage)0,  false,  70.0f, true);
            mCpuMonitors.miUT_NetworkAIRaceCar = AddMonitor("      Network + AI + Racecar",  (EPage)0,  false,  10.0f, true);
            mCpuMonitors.miUT_Network          = AddMonitor("         Network",              (EPage)0,  false,   3.0f, true);
            mCpuMonitors.miUT_AI               = AddMonitor("         AI",                   (EPage)0,  false,   5.0f, true);
            mCpuMonitors.miUT_RaceCar          = AddMonitor("         RaceCar",              (EPage)0,  false,   3.0f, true);
            mCpuMonitors.miUT_GameState        = AddMonitor("      GameState",               (EPage)0,  false,   3.0f, true);
            mCpuMonitors.miUT_Replay           = AddMonitor("      Replay",                  (EPage)0,  false,   3.0f, true);
            mCpuMonitors.miUT_GUI              = AddMonitor("      GUI",                     (EPage)0,  false,   2.0f, true);
            mCpuMonitors.miUT_Director         = AddMonitor("      Director",                (EPage)0,  false,   5.0f, true);
            mCpuMonitors.miUT_Sound            = AddMonitor("      Sound",                   (EPage)0,  false,   5.0f, true);
            mCpuMonitors.miUT_Effects          = AddMonitor("      Effects",                 (EPage)0,  false,   0.5f, true);
            mCpuMonitors.miUT_Traffic          = AddMonitor("      Traffic",                 (EPage)0,  false,   8.0f, true);
            mCpuMonitors.miUT_Triggers         = AddMonitor("      Triggers",                (EPage)0,  false,   0.5f, true);
            mCpuMonitors.miUT_CrashManager     = AddMonitor("      CrashManager",            (EPage)0,  false,   1.0f, true);
            mCpuMonitors.miUT_Physics          = AddMonitor("      Physics",                 (EPage)0,  false,  28.0f, true);
            mCpuMonitors.miUT_World            = AddMonitor("      World",                   (EPage)0,  false,   4.0f, true);
            mCpuMonitors.miUT_Resource         = AddMonitor("UT: ResourceSystem",            (EPage)0,  false,   5.0f, false);
            mCpuMonitors.miUT_RenderAll        = AddMonitor("UT: Render",                    (EPage)0,  false,  15.0f, false);
            mCpuMonitors.miUT_FrustumTesting   = AddMonitor("      FrustumTests",            (EPage)0,  false,   2.0f, false);
            mCpuMonitors.miUT_RenderMainScreen = AddMonitor("      RenderMainScreen",        (EPage)0,  false,   7.0f, false);
            mCpuMonitors.miUT_RenderShadowMap  = AddMonitor("      RenderShadows",           (EPage)0,  false,   1.5f, false);
            mCpuMonitors.miUT_RenderEnvMap     = AddMonitor("      RenderEnvMap",            (EPage)0,  false,   1.5f, false);
            mCpuMonitors.miUT_RenderFX         = AddMonitor("      RenderEffects",           (EPage)0,  false,   1.0f, false);
            mCpuMonitors.miUT_RenderGUI        = AddMonitor("      RenderGUI",               (EPage)0,  false,   2.0f, false);
            mCpuMonitors.miDT_DispatchToGpu    = AddMonitor("DT: DispatchToGPU",             (EPage)0,  false,  95.0f, false);
            mCpuMonitors.miUT_ThreadSync       = AddMonitor("UT: Thread sync",               (EPage)0,  false,  10.0f, false);
            mCpuMonitors.miUT_WaitOnDispatch   = AddMonitor("      WaitOnDispatch",          (EPage)0,  false,  10.0f, false);
            mCpuMonitors.miUT_DebugManager     = AddMonitor("UT: Debug manager",             (EPage)0,  false,   0.0f, false);
            mCpuMonitors.miUT_RaceCar_SQ       = AddMonitor("RC SceneQueries",               (EPage)12, false,   0.5f, true);
            mCpuMonitors.miUT_Traffic_SQ       = AddMonitor("Traf SceneQueries",             (EPage)2,  false,   0.5f, true);
            mCpuMonitors.miUT_Triggers_SQ      = AddMonitor("Trigger SceneQueries",          (EPage)5,  false,   0.5f, true);
            mCpuMonitors.miUT_GameState_Bridge = AddMonitor("GameState Bridges",             (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_GUI_Bridge       = AddMonitor("GUI Bridges",                   (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_AI_Bridge        = AddMonitor("AI Bridges",                    (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_RaceCar_Bridge   = AddMonitor("RaceCar Bridges",               (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_Traffic_Bridge   = AddMonitor("Traffic Bridges",               (EPage)11, false,   0.1f, true);
            mCpuMonitors.miUT_Director_Bridge  = AddMonitor("Bridges",                       (EPage)13, false,   0.1f, true);
            mCpuMonitors.miUT_Director_SQ      = AddMonitor("Scene Queries",                 (EPage)13, false,   1.0f, true);
            mCpuMonitors.miUT_SoundUpdate      = AddMonitor("Sound",                         (EPage)14, false,  10.0f, true);
        }

        // X360 step 6: construct EVERY engine module, in the X360 call order (member offsets
        // confirmed by GameRelease 0x823F03C8's typed per-module IO buffers). Modules whose real
        // Construct exists run it; the placeholder modules run the module base's Construct (which
        // is also what makes their Release/Destruct paths well-defined).
        mSoundModule.Construct();        // +0x8A7F00  RootSoundModule::Construct 0x826AF350 (slot 0)
        mRenderModule.Construct();       // +0x004400  BrnRendererModule::Construct (direct call)
        mGameDataModule.Construct();     // +0x5F4A00  GameDataModule::Construct 0x82671B90 (slot 0, NO args)
        mWorldModule.Construct(mCpuMonitors); // +0x010E80  WorldModule::Construct 0x827CF540
                                         //            (slot +64, const BrnCpuMonitors& -- the
                                         //            handle block filled above). REAL module
                                         //            mounted 2026-07-26 (world-render campaign).
        mInputModule.Construct();        // +0x6E9430  (slot 0; placeholder -> base)
        mGuiModule.Construct();          // +0x6EA820  [gated] X360 slot +84 with two sub-objects
                                         //            (+0x65A1D0/+0x65A1F4, inside the GameData
                                         //            module); the movie-hosting slice takes none.
        mGameStateModule.Construct();    // +0x669380  (slot 0; placeholder -> base)
        mEffectsModule.Construct();      // +0x878A00  (slot 0; placeholder -> base)
        // [FLAG interim bridge] ValidityAccount's static fail-flag mask must be built BEFORE the
        // director module constructs its cameras: CameraState::Construct/Clear (@0x82220950) and
        // BehaviourHelper::Update both assert `sbFailFlagMaskSet`. The console builds it inside
        // CameraState::Construct; until that runs it, the one-time setup happens here.
        // DELETE-WHEN: CameraState::Construct performs the setup itself.
        BrnDirector::Camera::ValidityAccount::SetupFailFlagMask();
        mDirectorModule.Construct(0.0f); // +0x6B0C90  X360 slot +64 @0x8225C590 (REAL module,
                                         //            mounted 2026-07-29 -- DJ fly-by campaign).
        mReplayModule.Construct();       // +0x8BD680  (slot 0; ReplayModule -> base)
        mNetworkModule.Construct();      // +0x8C39C0  [gated] X360 slot +64 with arg 0; placeholder -> base.

        // [gated] X360 steps 7-9: the DebugComponentPerfMonCpu::SetPageName table (pages 0-23:
        // "General".."Flapt"), the vsync-rate 50/60 M_CGS_PERFMON_CPU_SETGAMEFREQUENCY check, the
        // event-receiver queue @+10094268 (capacity 1024, align 16), the update/lookback timers +
        // CgsSystem::FrameRateManager::Construct, and the Debug/Framerate + Debug/Sim (Step/Play
        // via StepFrameCB/PlayFrameCB) + screenshot debug-interface registrations, and the two
        // replay serialisers (DirectorBridgeSerialiser/GameModuleSerialiser::Construct) -- their
        // members/subsystems are not in this layout yet.

        // NOTE: the bitmap debug font is NOT brought up here -- the D3D device does not exist yet at
        // Construct time (Device::Start's create lands later), so the atlas raster's FixUp would get a
        // null device and produce a textureless font. It is brought up from DispatchThread instead,
        // once the render path has the device live (see below).

        // X360 step 10: the main game flow controller (GameMainFlowController::Construct
        // @+10094180), then the PC boot enters the initial loading screen (OnEnter raises the
        // renderer's loading-screen signal).
        mMainFlowStateMachine.Construct();
        mMainFlowStateMachine.SetState(BrnGameMainFlowController::E_MGS_INITIAL_LOADING_SCREEN);

        // GUI module Prepare (movie-hosting slice): load VIDEOS\VIDEOLIST.BUNDLE (metadata only, no
        // device needed) + publish gpActiveMovieManager so the renderer draws the active movie. The
        // X360 prepares the GuiModule via the loading flow's LoadGUIModule stage + the module
        // dispatch; this is the minimal PC hookup (Update runs in GameMain). It stays idle until a
        // 508 GuiEventPlayVideo is queued (BrnBootVideos in Phase 3).
        mGuiModule.Prepare();

        // ---- input bring-up (PC stand-in for the unreconstructed input setup pass) --------
        // The console input module's Prepare constructs its output buffer, scans the pads and
        // flips the module state to "ready / player-0 assigned"; none of that pass is
        // reconstructed, so seed the observable outcome here: the real OutputBuffer::Construct
        // (@0x828F85E0), port 0 assigned, module ready (==4), and the GUI accepting controller
        // input (the console sets it from the GUI flow state -- FLAG follow-on).
        mPcInputOutputBuffer.Construct();
        miInputModuleState        = 4;
        miPlayer0ControllerPort   = 0;
        mbGuiAcceptsControllerInput = true;

        // ---- the GUI flow-FSM bridge state (X360 Construct zeroes the slots; the bridge
        //      parks the stage at 6 after the first pass) --------------------------------
        miGuiFsmStage      = 0;
        mbGuiPhaseComplete = false;
        mbGuiPreAccept     = false;
    }

    // @ 0x823DCA10 -- the game->GUI flow-FSM bridge (see BrnGameModule.hpp). Posts each
    // pending stage's GuiEventRunFsm record(s) as raw 24-byte event-144 AddEvents (the
    // X360 posts these raw, not through the AddGuiEvent<T> template), then parks the
    // stage and clears the phase-complete flag.
    void BrnGameModule::BridgeGameToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer)
    {
        if (lpGuiInputBuffer == 0)
            return;

        struct RunFsmPost
        {
            static void Post(CgsGui::CgsGuiModuleIO::InputBuffer* lpBuffer, const char* lpacFsmName,
                             const char* lpacInitialState, s32 liFsmToRun, s32 liFlowToUse)
            {
                BrnGui::GuiEventRunFsm lEvent;
                lEvent.mFsmId          = CgsIDCompress(lpacFsmName);
                lEvent.mInitialStateId = (lpacInitialState != 0) ? CgsIDCompress(lpacInitialState)
                                                                 : static_cast<CgsID>(0);
                lEvent.meFsmToRun      = static_cast<BrnGui::EHUDFSMs>(liFsmToRun);
                lEvent.meFlowToUse     = static_cast<BrnGui::GuiFlow>(liFlowToUse);
                lpBuffer->GetGuiEvents()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lEvent), 144,
                    static_cast<s32>(sizeof(lEvent)));
            }
        };

        switch (miGuiFsmStage)
        {
            case 1:
                RunFsmPost::Post(lpGuiInputBuffer, "BrnVideoFsm", 0,
                                 BrnGui::E_GUI_HUD_BOOT, BrnGui::E_GUIFLOW_HUD);
                break;
            case 2:
                RunFsmPost::Post(lpGuiInputBuffer, "BrnLegalFsm", 0,
                                 BrnGui::E_GUI_HUD_BOOT, BrnGui::E_GUIFLOW_HUD);
                break;
            case 3:
                RunFsmPost::Post(lpGuiInputBuffer, "BrnCmpLdFsm", 0,
                                 BrnGui::E_GUI_HUD_BOOT, BrnGui::E_GUIFLOW_HUD);
                break;
            case 4:
                RunFsmPost::Post(lpGuiInputBuffer, "BrnBFProFsm", 0,
                                 BrnGui::E_GUI_HUD_BOOT, BrnGui::E_GUIFLOW_HUD);
                break;
            case 5:
                // The in-game handoff: the front-end SCREEN flow at its LOADING state plus
                // the freeburn HUD FSM.
                RunFsmPost::Post(lpGuiInputBuffer, "BrnScreenFsm", "LOADING",
                                 BrnGui::E_GUI_HUD_BOOT, BrnGui::E_GUIFLOW_SCREEN);
                RunFsmPost::Post(lpGuiInputBuffer, "BrnFBFsm", 0,
                                 BrnGui::E_GUI_HUD_FREEBURN, BrnGui::E_GUIFLOW_HUD);
                break;
            default:
                break;
        }

        if (miGuiFsmStage != 6)
        {
            miGuiFsmStage      = 6;
            mbGuiPhaseComplete = false;
        }
    }

    // @ 0x823CB758 -- the GUI->game out-event consumer (see BrnGameModule.hpp). The PC
    // consumers map the console's dispatch-buffer render states onto the renderer's
    // loading-screen signal; the quit-to-dash (86/87/89 -> XLaunchNewImage) and the
    // brightness/contrast forwards (545/546) are platform follow-ons.
    void BrnGameModule::BridgeGuiToGame(CgsModule::VariableEventQueue<18432, 16>* lpGuiOutQueue)
    {
        if (lpGuiOutQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpGuiOutQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liId >= 0 && lpEvent != 0)
        {
            if (liId == 40)   // channel 40: GuiEventOut command records (muEventType @+4)
            {
                const u32 luCommand = reinterpret_cast<const u32*>(lpEvent)[1];
                switch (luCommand)
                {
                    case 19:   // PlayAptLoadingMovie -> ShowLoadingScreen (X360 write buffer [9828] = 1)
                        mDispatchThreadInputBufferManager.GetWriteBuffer()->ShowLoadingScreen();
                        break;
                    case 20:   // StopAptLoadingMovie -> HideLoadingScreen ([9828] = 2)
                        mDispatchThreadInputBufferManager.GetWriteBuffer()->HideLoadingScreen();
                        break;
                    case 70:   // GUI phase complete -- the main flow advances on it
                        mbGuiPhaseComplete = true;
                        break;
                    case 71:   // pre-accept -- resume the world load during the accept dwell
                        mbGuiPreAccept = true;
                        break;
                    case 90:   // profile-first-boot flag (X360 +10094136: 0 -> 1)
                        if (miInputModuleState == 0)
                            miInputModuleState = 1;
                        break;
                    case 65:
                        // The SCREEN flow's in-game entry notice (InGame::OnEnter posts
                        // {1,65,12,flag=1}). X360: BridgeGuiToGameState @0x823DDB78
                        // forwards it as GameState action 106; the GameState/world side
                        // then closes the boot loading-screen lifecycle as its streaming
                        // settles. [FLAG world-load stand-in: with no world modules on PC
                        // the "world" is ready the moment gameplay owns the screen, so the
                        // in-game notice retires the boot loading screen here -- the same
                        // observable the console produces at this point. The real
                        // GameState consumer replaces this when the world side lands.]
                        mDispatchThreadInputBufferManager.GetWriteBuffer()->HideLoadingScreen();
                        CgsDev::Log::WriteToLog("[GameModule] in-game screen entered (65) -> "
                                                "loading screen retired (world-load stand-in).\n");
                        break;
                    case 86: case 87: case 89:
                        // Quit-to-dash (X360: XGetLaunchData + XLaunchNewImage). [FLAG PC
                        // platform: no dash relaunch; logged so the request is visible.]
                        CgsDev::Log::WriteToLog("[GameModule] GUI quit-to-dash command (86/87/89) -- "
                                                "PC platform no-op (FLAG).\n");
                        break;
                    // The "render states" ARE the loading-screen commands: the console
                    // BridgeGuiToGame @0x823CB758 writes the dispatch write buffer's
                    // meLoadingScreenCommand (+39312 == [9828]) that Render forwards into
                    // LoadingScreenRenderer::AddCommand. 138 = the save/load prompt
                    // background (BootProfile posts it right before playing
                    // SaveLoadComponent: the loading screen renders BEHIND the GUI,
                    // dimmed); 589/590 = the black-overlay fades BootLegal posts around
                    // the title screen and the menu accept.
                    case 138:  // -> ShowLoadingScreenSaveLoadBG ([9828] = 3)
                        mDispatchThreadInputBufferManager.GetWriteBuffer()->ShowLoadingScreenSaveLoadBG();
                        break;
                    case 589:  // -> BlackOverlayFadeIn ([9828] = 4: reveal from black)
                        mDispatchThreadInputBufferManager.GetWriteBuffer()->BlackOverlayFadeIn();
                        break;
                    case 590:  // -> BlackOverlayFadeOut ([9828] = 5: the accept fade to black)
                        mDispatchThreadInputBufferManager.GetWriteBuffer()->BlackOverlayFadeOut();
                        break;
                    default:
                        break;
                }
            }
            else if (liId == 201)
            {
                const BrnGui::GuiAudioTriggerEvent* lpAudio =
                    reinterpret_cast<const BrnGui::GuiAudioTriggerEvent*>(lpEvent);
                CgsSystem::GuiSoundPC::OnTrigger(
                    lpAudio->macLabel, 0, lpAudio->macComponent, lpAudio->meAction);
            }
            const CgsModule::Event* lpNext = 0;
            liId = lpGuiOutQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
        lpGuiOutQueue->Clear();
    }

    // The per-frame spines the in-game flow state drives (MainGameFlowStateInGame::
    // Update -> DoUpdate, ::Render -> DoDispatch). On the X360 these run the game
    // module's owned-module update/dispatch walk; the PC host loop (EngineUpdate ->
    // BrnGameModule::DispatchThread) already drives that walk once per frame, so a
    // second walk here would double-update every module.
    // FLAG PC-platform leaf: no-op returns until the module scheduler ownership moves
    // under the game module's own spines (the host loop is the current driver).
    int BrnGameModule::DoUpdate()
    {
        return 0;
    }

    // @ 0x823E8BD0 -- DoUpdate's WORLD leg. Reconstructed against the X360 body; the
    // per-frame world drive itself (the vtable +76 dispatch and the boot-video variant) is
    // REAL here. NOTE the caller DoUpdate is still the PC-platform leaf above (the PC host
    // loop owns the module drive), so today the world is driven per frame by the scripted
    // loading spine's LoadingScriptedState::UpdateWorldModule instead; this entry is the
    // in-game cascade's hook and goes live with the DoUpdate reconstruction.
    //
    // X360 order:
    //   StartMonitor(gm+10055508);
    //   CreateIOBuffer<BrnWorldIO::UpdateInputBuffer>(inStack, &worldIn, "World")
    //     (asserting "mpStack->CreateIOBuffer( &mpBuffer, lpcName )", CgsModuleIOHelper.h:52);
    //   sub_823B7620(worldIn, controllerOut, gameStateOut, guiOut, replayOut, soundOut)
    //     == LockBuffersForIO with FIVE sources;
    //   UpdateInputBuffer::SetTimerStatusInterface(worldIn, gm+10095372);
    //   LockForRead(networkOut);
    //   BridgeControllerToWorld / BridgeNetworkToWorld / BridgeGameStateToWorld /
    //   BridgeGuiToWorld; SetReplayStatusInterface(ReplayIO::OutputBuffer_PreSim::
    //   GetStatusInterface(replayOut)); BridgeSoundToWorld; UnlockForRead(networkOut);
    //   sub_823B7760(...) == the matching UnlockBuffersForIO;
    //   StopMonitor;
    //   (updateSet & 0x20) ? WorldModule::UpdateForBootUpVideo(updateSet, inStack, outStack,
    //                            worldIn, worldOut)
    //                      : world->vtbl+76(updateSet, inStack, outStack, worldIn, worldOut,
    //                            gm->mpWorldUpdateFrameAllocator);
    //   LockForRead(worldOut);
    //   gm+10094123 = (worldOut->GetWorldEntityStatusInterface()->GetImmediateStreamed() == 0);
    //   UnlockForRead(worldOut);
    //   DestroyIOBuffer(inStack, &worldIn) (asserting CgsModuleIOHelper.h:57).
    void BrnGameModule::DoUpdate_World(CgsModule::IOBufferStack* lpUpdateInputBufferStack,
                                       CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                                       const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                                       BrnWorldIO::UpdateOutputBuffer* lpWorldUpdateOutputBuffer,
                                       CgsMemory::LinearMalloc* lpWorldFrameAllocator,
                                       BrnUpdateSet lUpdateSet)
    {
        BrnWorldIO::UpdateInputBuffer* lpWorldInput = 0;
        const bool lbCreated = lpUpdateInputBufferStack->CreateIOBuffer(&lpWorldInput, "World");
        CGS_ASSERT(lbCreated, "mpStack->CreateIOBuffer( &mpBuffer, lpcName )");  // CgsModuleIOHelper.h:52
        (void)lbCreated;
        // The X360 CreateIOBuffer<T> instantiation runs T::Construct after the stack alloc.
        lpWorldInput->Construct();

        // The controller leg of the five-source input staging (the only one of the six
        // X360 bridges whose body is committed).
        // [FLAG PC boot gate] BridgeNetworkToWorld @0x823DF8B0, BridgeGameStateToWorld
        // @0x823E1890, BridgeGuiToWorld @0x823CBE90, BridgeSoundToWorld @0x823CDC98 and the
        // replay-status latch (ReplayIO::OutputBuffer_PreSim::GetStatusInterface) are not
        // reconstructed; their source modules' output buffers are not threaded into this
        // leg on the PC yet, so the staging is omitted rather than faked. The world drive
        // below reads NONE of those fields on the streaming path (the streamer runs off the
        // world-entity module's own PVS state + the game-action queue), so the observable
        // streaming behaviour is unchanged. Restore them with the DoUpdate cascade.
        // The X360's SetTimerStatusInterface(gm+10095372) is part of that same staging.
        lpWorldInput->LockForWrite();
        BridgeControllerToWorld(lpWorldInput, lpInputOutputBuffer);
        lpWorldInput->UnlockForWrite();

        if ((lUpdateSet & 0x20) != 0)
        {
            mWorldModule.UpdateForBootUpVideo(lUpdateSet, lpUpdateInputBufferStack,
                                              lpUpdateOutputBufferStack,
                                              lpWorldInput, lpWorldUpdateOutputBuffer);
        }
        else
        {
            mWorldModule.Update(lUpdateSet, lpUpdateInputBufferStack, lpUpdateOutputBufferStack,
                                lpWorldInput, lpWorldUpdateOutputBuffer, lpWorldFrameAllocator);
        }

        // [FLAG] the X360 latches !GetImmediateStreamed() into gm+10094123 here (the
        // "world still streaming" flag the in-game flow polls); that member sits in this
        // incremental layout's omitted range and has no committed consumer -- it lands
        // with the DoUpdate cascade that reads it.

        const bool lbDestroyed = lpUpdateInputBufferStack->DestroyIOBuffer(&lpWorldInput);
        CGS_ASSERT(lbDestroyed, "mpStack->DestroyIOBuffer( &mpBuffer )");        // CgsModuleIOHelper.h:57
        (void)lbDestroyed;
    }

    // ------------------------------------------------------------------------------------
    // The per-frame DIRECTOR leg (2026-07-29, DJ fly-by campaign).
    //
    // The console drives the director module through the module scheduler in three passes per
    // sub-step, bracketing the GUI update:
    //     DirectorModule::PreSceneQueryUpdate @0x8225C768   (before the scene query)
    //     <the scene manager services the queries>
    //     DirectorModule::Update              @0x82275300   (consumes the answers, runs
    //                                                        MainDirector -> the arbitrator ->
    //                                                        CameraFinaliser, PUBLISHES)
    //     <the GUI update>
    //     DirectorModule::PostGuiUpdate       @0x82250DD0
    //
    // [FLAG PC placement] the PC has no module scheduler yet (BrnGameModule::DoUpdate is a
    // documented no-op -- the host loop owns the module walk), so the three passes are driven
    // straight from the sim loop in the same ORDER, with the scene-query step between passes 1
    // and 2 being whatever the scene manager already did this frame. The two scene-query IO
    // buffers are created per sub-step on the update stacks (the console's own lifetime); the
    // OUTPUT buffer is the frame's mpDirectorOutputBuffer so the published camera is still
    // alive when DoDispatch runs.
    //
    // [FLAG PC input staging] the director INPUT buffer is created and Constructed here but
    // NOT staged: the X360 fills it through BridgeControllerToDirector / the race-car entity
    // module's global output interface / the world's contact + timer publishes, none of which
    // is threaded on the PC. A zero-staged input reports NO live player car
    // (GetPlayerCarIndex + GetUsedRaceCars), which MainDirector::Update reads as its
    // "no player" branch -- so the director carries its last finalised camera forward instead
    // of driving a fabricated one. That is the console's own no-player behaviour, and it is
    // why the published camera is currently STATIC.
    // ------------------------------------------------------------------------------------
    void BrnGameModule::DoUpdate_Director(bool lbPostGui)
    {
        if (mpDirectorOutputBuffer == 0)
            return;

        // The console's module scheduler only dispatches a module's per-frame entry points once
        // its staged Prepare has reported done; the PC drives them by hand, so it asks here.
        // Skipping this is not cosmetic: DirectorModule::Prepare stage 7 is what carves the
        // BehaviourManager's helper/behaviour pools, and an arbitrator running before that
        // allocates out of an un-carved pool (BehaviourManager.h:782 `lHelperID >= 0` fires).
        if (!mDirectorModule.IsPrepared())
            return;

        BrnDirector::DirectorIO::InputBuffer* lpDirectorInput = 0;
        mpUpdateInputBufferStack->CreateIOBuffer(&lpDirectorInput, "Director");
        if (lpDirectorInput == 0)
            return;
        // [FLAG PC bring-up] Zero the staged region before Construct. The IO stack hands back
        // RE-USED memory and the generic PC CreateIOBuffer<T> only placement-news, so every
        // published member (mePlayerCarIndex, mUsedRaceCars, the per-car VehicleInfo array, the
        // control block) would otherwise hold the PREVIOUS tenant's bytes. That is not a
        // cosmetic risk here: MainDirector::GetLivePlayerCarIndex reads mePlayerCarIndex and
        // mUsedRaceCars to decide whether a player car exists, so stale bytes could hand the
        // arbitrator a car index whose VehicleInfo is garbage. Zeroing gives the console's own
        // "no live player car" answer deterministically (index 0 with its used-race-car bit
        // clear -> GetLivePlayerCarIndex returns -1).
        // DELETE-WHEN: the real per-frame staging lands (BridgeControllerToDirector + the
        // race-car entity module's global output interface + the world contact/timer publishes),
        // at which point every one of these fields is written before it is read.
        memset(lpDirectorInput, 0, sizeof(*lpDirectorInput));
        // The X360 CreateIOBuffer<T> instantiation runs T::Construct after the stack alloc;
        // the generic PC template placement-news only (same restoration as LoadWorldModule).
        lpDirectorInput->CgsModule::IOBuffer::Construct();

        // [FLAG PC bring-up, env-gated BRN_DIRECTOR_ATTRACT] Stage a live player car and raise
        // the attract-mode request, so the director's OWN state machine can be exercised before
        // the real staging exists.
        //
        // WHY THIS EXISTS. MainDirector::Update @0x82274070 runs its entire gameplay middle --
        // including UpdateArbitrator, the only path to ArbStateAttractMode and therefore to the
        // DJ fly-by's BehaviourRoadRunner -- ONLY when GetLivePlayerCarIndex() != -1. On the
        // console that is always true in attract mode (the demo drives a real car); on this
        // build no race car exists, so the director takes its no-player branch every frame and
        // re-publishes its last finalised camera. Measured with BRN_DIRECTOR_TRACE: the
        // published camera sits at the origin, identity basis, FOV 90 (Camera::Construct's
        // defaults) and never changes.
        //
        // ⚠️ THIS IS A DIAGNOSTIC, NOT A FEATURE, AND IT IS OFF BY DEFAULT. With it on, the
        // arbitrator runs against a ZEROED VehicleInfo -- so anything downstream that reads the
        // player car reads zeros. It exists to find the NEXT blocker, not to produce a camera.
        // DELETE-WHEN: the race-car entity module publishes into the director input for real.
        static const bool sbForceAttract = (getenv("BRN_DIRECTOR_ATTRACT") != 0);
        if (sbForceAttract && !lbPostGui)
        {
            lpDirectorInput->LockForWrite();
            lpDirectorInput->SetPlayerCarIndex(E_ACTIVE_RACE_CAR_INDEX_0);
            lpDirectorInput->SetRaceCarInUse(0u, true);
            lpDirectorInput->UnlockForWrite();
            mDirectorModule.GetMainDirector().GetArbitrator().SetDoAttractMode(true);
        }

        if (!lbPostGui)
        {
            BrnDirector::DirectorIO::SceneQueryOutputBuffer* lpSceneQueryOutput = 0;
            BrnDirector::DirectorIO::SceneQueryInputBuffer*  lpSceneQueryInput  = 0;
            mpUpdateOutputBufferStack->CreateIOBuffer(&lpSceneQueryOutput, "DirectorSceneQuery");
            mpUpdateInputBufferStack->CreateIOBuffer(&lpSceneQueryInput, "DirectorSceneQuery");
            if (lpSceneQueryOutput != 0 && lpSceneQueryInput != 0)
            {
                lpSceneQueryOutput->CgsModule::IOBuffer::Construct();
                lpSceneQueryInput->CgsModule::IOBuffer::Construct();

                // lbIsReplaying == false: the PC has no replay playback path (the module's own
                // replay legs are documented gates).
                mDirectorModule.PreSceneQueryUpdate(0, 0, lpDirectorInput,
                                                    mpDirectorOutputBuffer,
                                                    lpSceneQueryOutput, false);
                mDirectorModule.Update(0, 0, lpDirectorInput, mpDirectorOutputBuffer,
                                       lpSceneQueryInput, lpSceneQueryOutput);
            }
            if (lpSceneQueryInput != 0)
                mpUpdateInputBufferStack->DestroyIOBuffer(&lpSceneQueryInput);
            if (lpSceneQueryOutput != 0)
                mpUpdateOutputBufferStack->DestroyIOBuffer(&lpSceneQueryOutput);
        }
        else
        {
            mDirectorModule.PostGuiUpdate(0, 0, lpDirectorInput, mpDirectorOutputBuffer);

            // [DIAG BRN_DIRECTOR_TRACE] Report what the director actually PUBLISHED this
            // frame. This is the measurement the fly-by campaign needs: whether the director
            // camera exists, where it is, and whether it MOVES. Off unless the env var is set;
            // remove with the bring-up path.
            static const bool sbTrace = (getenv("BRN_DIRECTOR_TRACE") != 0);
            if (sbTrace)
            {
                static s32 siTraceFrame = 0;
                if ((siTraceFrame % 30) == 0 && CgsDev::Log::gpDebugPrint != 0)
                {
                    mpDirectorOutputBuffer->LockForRead();
                    const BrnDirector::Camera::Camera* lpCamera =
                        mpDirectorOutputBuffer->GetCameraOutput();
                    const rw::math::vpu::Matrix44Affine& lXform = lpCamera->GetTransform();
                    // Also report WHERE THE ARBITRATOR IS. Without this the trace cannot tell
                    // "the camera is static because the arbitrator never ran" from "... because
                    // the behaviour it picked produced nothing" -- which is exactly the
                    // distinction the fly-by campaign keeps having to make. EState:
                    // 0 PREPARE / 1 PRE_NORMAL / 2 NORMAL / 3 CRASH_NAV / 4 CRASH_NAV_ICE /
                    // 5 CHANGING_TO_ATTRACT / 6 ATTRACT_MODE / 7 FINAL_ELITE / 8 RENDER_METRICS /
                    // 9 RELEASE.
                    const BrnDirector::Arbitrator& lrArbitrator =
                        mDirectorModule.GetMainDirector().GetArbitrator();
                    *CgsDev::Log::gpDebugPrint
                        << "[director] f" << siTraceFrame
                        << " arb " << static_cast<s32>(lrArbitrator.GetState())
                        << (lrArbitrator.GetDoAttractMode() ? " attract" : " -")
                        << " eye (" << lXform.wAxis.x << ", " << lXform.wAxis.y
                        << ", " << lXform.wAxis.z << ")"
                        << " at (" << lXform.zAxis.x << ", " << lXform.zAxis.y
                        << ", " << lXform.zAxis.z << ")"
                        << " fov " << lpCamera->GetFOV() << "\n";
                    mpDirectorOutputBuffer->UnlockForRead();
                }
                ++siTraceFrame;
            }
        }

        mpUpdateInputBufferStack->DestroyIOBuffer(&lpDirectorInput);
    }

    // @ 0x823DC458 -- the dispatch (render-feed) spine. The X360 body creates the
    // world-dispatch + renderer IO buffer pairs, stages the director's camera into the
    // renderer input, runs BrnRendererModule::Update (which publishes the game-side
    // dispatch frame through RendererIO::OutputBuffer::SetDispatchFrame),
    // BridgeRendererToWorld/ToGui/ToEffects, then WorldModule::GenerateFrustumQueries
    // and WorldModule::GenerateDispatchLists, the effects dispatch and the debug render.
    //
    // [FLAG PC bring-up] Only the WORLD leg is driven here, and through the world
    // module's bring-up producer rather than the X360 call: the director module
    // publishes no camera, none of the four IO buffers is created on PC, and
    // WorldModule::GenerateDispatchLists' frustum-test result comes from the scene
    // manager's job path, which is still an inert gate. The GDL frame is taken
    // straight from the renderer's write slot -- the exact expression
    // BrnRendererModule::Update @0x82405E28 publishes, and the same one
    // BrnRendererModule::Render walks after OnEndOfUpdateFrame's swap.
    // Restore the real body when DoDispatch's IO set + the frustum query are live.
    int BrnGameModule::DoDispatch()
    {
        CgsGraphics::DispatchFrame* lpDispatchFrame = mRenderModule.GetDispatchFrameForWrite();
        if (lpDispatchFrame != 0)
        {
            mWorldModule.GenerateDispatchListsBringUp(lpDispatchFrame);
        }
        return 0;
    }

    // @ BrnGameModule.cpp:1047 (X360 0x823BC868) - tear down the owned modules + the module base.
    // The X360 first clears a terminated flag (off +10094134) and destructs an off-path collision
    // generator (off +10095440) - both fall in the omitted member ranges of this incremental
    // layout - then calls Destruct() on all 11 engine modules in the order below, the module base,
    // and finally the network module. Reconstructed faithfully (the two off-path subsystem touches
    // are noted, not declared here). Not on the boot/loading path.
    void BrnGameModule::Destruct()
    {
        // [+10094134 terminated flag = 0; +10095440 CgsCollision::BaseCollisionGenerator::Destruct
        //  - both off-path in this incremental layout, omitted]
        mEffectsModule.Destruct();
        mSoundModule.Destruct();
        mDirectorModule.Destruct();
        mRenderModule.Destruct();
        mGameStateModule.Destruct();
        mGuiModule.Destruct();
        mInputModule.Destruct();
        mWorldModule.Destruct();
        mReplayModule.Destruct();
        mGameDataModule.Destruct();
        CgsModule::ModuleSingleBuffered::Destruct();
        mNetworkModule.Destruct();
    }

    // @ BrnGameModule.cpp:925 (X360 0x823CB2A8) - resumable staged release (counterpart of
    // Prepare). meReleaseStage is the resume point; a failed sub-release returns false so the
    // caller re-drives from the same stage. The release SEQUENCE is GUI -> sound -> hardware
    // (+input) -> game-data -> module base -> network -> done; the cases are written in that
    // execution order with fallthrough (the same resumable-switch shape as the module base's
    // ModuleSingleBuffered::Release), so entering at any stage runs the rest. Note the enum
    // values along that path are not monotonic (HARDWARE = 5 sits between SOUND = 2 and
    // GAMEDATAMODULE = 3); C++ fallthrough follows source order, not case value, so this is
    // exact - the Hex-Rays output only used gotos because its jump table sorts by case value.
    // Off-path touches (the System360HW release + two terminated-flag fields) are in this
    // layout's omitted ranges and noted. Not on the boot/loading path.
    bool BrnGameModule::Release()
    {
        switch (meReleaseStage)
        {
            case E_RELEASESTAGE_START:
            case E_RELEASESTAGE_GUI:
                meReleaseStage = E_RELEASESTAGE_GUI;
                if (!mGuiModule.Release())
                    return false;
                // fall through
            case E_RELEASESTAGE_SOUND:
                meReleaseStage = E_RELEASESTAGE_SOUND;
                if (!mSoundModule.Release())
                    return false;
                // fall through
            case E_RELEASESTAGE_HARDWARE:
                meReleaseStage = E_RELEASESTAGE_HARDWARE;
                // [+10097088 BrnHW::System360HW::Release(&mHardware): off-path hardware, omitted;
                //  the X360 also returns false here if the hardware release fails]
                if (!mInputModule.Release())
                    return false;
                // fall through
            case E_RELEASESTAGE_GAMEDATAMODULE:
                meReleaseStage = E_RELEASESTAGE_GAMEDATAMODULE;
                if (!mGameDataModule.Release())
                    return false;
                // fall through
            case E_RELEASESTAGE_MANAGER:
                meReleaseStage = E_RELEASESTAGE_MANAGER;
                if (!CgsModule::ModuleSingleBuffered::Release())
                    return false;
                // fall through
            case E_RELEASESTAGE_NETWORK:
                meReleaseStage = E_RELEASESTAGE_NETWORK;
                if (!mNetworkModule.Release())
                    return false;
                // fall through
            case E_RELEASESTAGE_DONE:
                // [+10094134 terminated flag = 0: off-path, omitted]
                meReleaseStage = E_RELEASESTAGE_DONE;
                // [+10094156 = 0: off-path, omitted]
                return true;
            default:
                CGS_ASSERT(false, "0");
                return false;
        }
    }

    // operator++(EReleaseStage&, int) @ 0x823A8AD8 - POST-increment for the EReleaseStage stage
    // machine, used by Release() to advance meReleaseStage. X360 body: v1 = *a1; *a1 = *a1 + 1;
    // if (*a1 > 7) assert; return v1 -- save the old value, advance the stage, assert the advanced
    // value is still <= E_RELEASESTAGE_DONE (=7), and return the OLD value.
    //
    // FLAG (static-message assert): the X360 fires the assert with the literal expression string
    // "leEnumIndex <= BrnGameModule::E_RELEASESTAGE_DONE" at BrnGameModule.h:1443. We use the
    // committed CGS_ASSERT macro with that same expression text (the macro supplies __FILE__ /
    // __LINE__); the original's BrnGameModule.h:1443 site is NOT baked in.
    BrnGameModule::EReleaseStage operator++(BrnGameModule::EReleaseStage& leStage, int)
    {
        BrnGameModule::EReleaseStage leOld = leStage;
        leStage = static_cast<BrnGameModule::EReleaseStage>(leStage + 1);
        CGS_ASSERT(leStage <= BrnGameModule::E_RELEASESTAGE_DONE,
                   "leEnumIndex <= BrnGameModule::E_RELEASESTAGE_DONE");
        return leOld;
    }

    // @ BrnGameModule.cpp:1580 (X360 0x823EFBD0) - one-time per-game-instance prepare: under the
    // GameData IO buffer locks, a resumable stage machine that (1) queues LoadBundle requests for
    // "shaders.bndl", "GlobalTextureDictionary.bin" and "Language\\Fonts\\Default.font" through
    // the GameData input's RequestInterface<32768>, (2) waits for the 3 loads then posts the six
    // global-texture acquires, (3) waits for those and hands them to
    // BrnRendererModule::PrepareAgain, then (4) runs the GameState module's game-prepare.
    //
    // X360 stage-machine shape (switch on gm+10094164, reproduced exactly):
    //   case 0/1: stage=1; LoadBundle(&q, 2, 10, "shaders.bndl", true)
    //                      LoadBundle(&q, 1, 10, "GlobalTextureDictionary.bin", false)
    //                      LoadBundle(&q, 0,  0, "Language\\Fonts\\Default.font", false)   ->fall
    //   case 2:   stage=2; if (q.GetLength() < 3) return not-done;
    //                      q.Clear(); post the 6 acquire events (ids 0..5)                 ->fall
    //   case 3:   stage=3; if (q.GetLength() < 5) return not-done;
    //                      walk the queue (event type 4) -> the 5 texture pointers + the
    //                      font handle (id 5: Font::CreateTextureState + SetDebugFont);
    //                      assert each; BrnRendererModule::PrepareAgain(...); q.Clear()    ->fall
    //   case 4:   stage=4; GameStateModule game-prepare (vtable +64) on a scratch
    //                      GameStateModuleIO::OutputBuffer; done -> return true
    //   not-done tail (LABEL_36): create the RendererIO pair, BrnRendererModule::Update,
    //                      latch the reusable loading-screen allocator, publish
    //                      IsStalled/IsDiskError onto the dispatch input, CheckDiskError.
    //
    // RECONSTRUCTED HERE: stages 0/1/2 -- the bundle loads and their completion wait. This is
    // the stage that brings SHADERS.BNDL online, which is what lets every streamed world
    // Material resolve its ShaderTechnique / ShaderProgramBuffer imports instead of falling
    // back to the bring-up shader.
    //
    // [gated] stages 3 and 4:
    //   * stage 3's five "gamedb://burnout5/Playground/GlobalTextures/..." acquires feed
    //     BrnRendererModule::PrepareAgain (blobby shadow, the two cloud textures, the corona
    //     atlas, the glass fracture) -- PrepareAgain is not reconstructed and none of its four
    //     consumers (blobby-shadow manager, sky dome, corona manager, damage FX) is live.
    //   * the id-5 "default" font acquire lands the debug font; the PC already brings that up
    //     from DispatchThread via CgsDev::LoadAndSetDebugFont (which owns the same
    //     Font::CreateTextureState + DebugManager::SetDebugFont pair).
    //   * stage 4's GameStateModule game-prepare rides the GameState module placeholder.
    // DELETE the gate when PrepareAgain + the GameState game-prepare land.
    //
    // FLAG PC-platform leaf: the X360 services the GameData module on its own resource-update
    // thread (IThreadClass::ResourceUpdateThread) while this stage machine blocks the update
    // thread, and BrnGameModule::Prepare has already prepared the GameDataModule by the time
    // UpdateThread first runs. The single-threaded PC host has neither: GameDataModule::Prepare
    // is driven from the loading flow (which only runs once GamePrepare has returned done) and
    // nothing else pumps GameDataModule::Update. So this body drives BOTH inline -- the module
    // prepare first, then one Update pump per wait frame. Both are the module's own resumable
    // machines, so the flow's later GameDataModule::Prepare call still finds it done.
    bool BrnGameModule::GamePrepare()
    {
        // The X360 pair is gm+10055440 / gm+10055444 (see the accessors' note).
        BrnResource::GameDataIO::InputBuffer*  lpGameDataInput =
            BrnGameMainFlowController::GetScriptedLoadGameDataInput();
        BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput =
            BrnGameMainFlowController::GetScriptedLoadGameDataOutput();

        if (!mbGamePrepareReceiverQueueConstructed)
        {
            mbGamePrepareReceiverQueueConstructed = true;
            mGamePrepareReceiverQueue.Construct();
        }

        // FLAG PC-platform leaf (see above): stand in for the X360's already-completed
        // module prepare + its concurrent resource thread.
        if (!mGameDataModule.Prepare(0, 0))
            return false;

        lpGameDataInput->LockForWrite();
        lpGameDataOutput->LockForRead();

        bool lbDone = false;
        switch (meGamePrepareStage)
        {
        case E_GAMEPREPARESTAGE_START:
        case E_GAMEPREPARESTAGE_LOADBUNDLES:
        {
            meGamePrepareStage = E_GAMEPREPARESTAGE_LOADBUNDLES;

            // The three one-time bundle loads, in the X360's order/arguments. Pool 10 is
            // the game-wide "permanent" pool (GameDataModule's pool table); the font goes
            // to pool 0. Only shaders.bndl sets mbUseHDCache.
            lpGameDataInput->GetRequestInterface()->LoadBundle(
                &mGamePrepareReceiverQueue, 2, 10, "shaders.bndl", true);
            lpGameDataInput->GetRequestInterface()->LoadBundle(
                &mGamePrepareReceiverQueue, 1, 10, "GlobalTextureDictionary.bin", false);
            lpGameDataInput->GetRequestInterface()->LoadBundle(
                &mGamePrepareReceiverQueue, 0, 0, "Language\\Fonts\\Default.font", false);
        }
        // fall through

        case E_GAMEPREPARESTAGE_WAITBUNDLES:
        {
            meGamePrepareStage = E_GAMEPREPARESTAGE_WAITBUNDLES;

            if (mGamePrepareReceiverQueue.GetLength() < 3)
                break;

            mGamePrepareReceiverQueue.Clear();
        }
        // fall through

        case E_GAMEPREPARESTAGE_WAITACQUIRES:
        case E_GAMEPREPARESTAGE_GAMESTATE:
        {
            // [gated] stages 3 + 4 -- see the note above.
            meGamePrepareStage = E_GAMEPREPARESTAGE_GAMESTATE;
            lbDone = true;
            break;
        }

        default:
        {
            CGS_ASSERT(false, "Got into an unknown state");   // X360 BrnGameModule.cpp:1807
            break;
        }
        }

        lpGameDataOutput->UnlockForRead();
        lpGameDataInput->UnlockForWrite();

        if (!lbDone)
        {
            // FLAG PC-platform leaf (see above): the X360's not-done tail runs the renderer
            // update + disk-error publication; on PC the piece this stage machine cannot do
            // without is the GameData pump that services the requests it just queued.
            mGameDataModule.Update(lpGameDataInput, lpGameDataOutput);
        }

        return lbDone;
    }

    // @ BrnGameModule.cpp:2650 (X360 0x823F03C8) - teardown counterpart of GamePrepare: under the
    // GameData IO locks, an 11-stage resumable release that drives the per-module game releases
    // (incl. the World release with a scratch BrnWorldIO::UpdateOutputBuffer and the GameState
    // release with a scratch GameStateModuleIO::OutputBuffer, both on the update output stack).
    // [gated] same dependencies as GamePrepare. Returns done.
    bool BrnGameModule::GameRelease()
    {
        return true;
    }

    // @ BrnGameModule.cpp:1253 (X360 0x823A8BB0) - start-of-update-frame hook. The X360 body is
    // exactly two statements: `*(this + 8919372) = 0.0` (the frame lookback timer) then
    // `BrnRendererModule::StartOfFrame(this + 15808)`. The renderer call is live now (it rewinds
    // the game-side dispatch-list ring the world modules fill this frame and opens the shader-
    // constant table's frame on its bin); the lookback timer member is not in this layout yet.
    void BrnGameModule::OnStartOfUpdateFrame()
    {
        mRenderModule.StartOfFrame();
    }

    // @ BrnGameModule.cpp:1275 - end-of-update-frame hook. The X360 body @0x823DBBA0 runs
    // the render-metrics GameTalk report, the particle end-of-frame, the dispatch-buffer
    // swap (inlined DispatchThreadInputBufferManager::Swap -- the written buffer becomes
    // the read buffer and the new write buffer is re-Constructed), then the GUI/renderer
    // end-of-frame + perfmon swap. The swap is the piece the boot path needs: it publishes
    // this frame's loading-screen command to the dispatch side and clears the next write
    // buffer's one-shot slot. [gated] the metrics/particle/GUI end-of-frame notifies land with
    // their subsystems.
    //
    // BrnRendererModule::EndOfFrame @0x823FFE28 is live now: its SwapBuffers @0x823FC678 advances
    // the game-side dispatch-list ring so the frame the world modules filled this update becomes
    // the frame BrnRendererModule::Render walks next (without it the read cursor never reaches the
    // written slot and every world dispatch list reads empty).
    void BrnGameModule::OnEndOfUpdateFrame()
    {
        mDispatchThreadInputBufferManager.Swap();
        mRenderModule.EndOfFrame();
    }

    // @ BrnGameModule.cpp:1533 - resource-update worker-thread body (streams resources while the
    // sim runs). Not driven on the single-threaded boot path; minimal until the threading core.
    void BrnGameModule::ResourceUpdateThread(Mutex* /*lpMutex*/)
    {
    }

    // @ BrnGameModule.cpp:1221 - the render/dispatch thread body. The threaded D3D
    // thread-ownership handshake and the per-module dispatch are reconstructed with the
    // threading core; for the boot loading screen this drives one render through the renderer
    // module.
    void BrnGameModule::DispatchThread()
    {
        // The console dispatch thread hands the renderer the manager's READ buffer (the
        // frame the update side just published via OnEndOfUpdateFrame's swap).
        mRenderModule.Render(mDispatchThreadInputBufferManager.GetReadBuffer());

        // Bring up the resource (bitmap) debug font from the render path, where the D3D device is live
        // (the atlas raster's FixUp creates a D3D texture). Retried every frame; LoadAndSetDebugFont
        // bails without latching while gDevice is null and latches once it has loaded + created the
        // atlas, so this is a single real load and a cheap guard check thereafter.
        CgsDev::LoadAndSetDebugFont("Language/Fonts/Default.font", mDebugManager);
    }

    // @ BrnGameModule.cpp:3916 - clear the whole game-module object, then stamp each owned
    // module's memory region with 0x7FFFFFFF so any read of un-prepared module memory is caught.
    // NOTE: the memset uses the FULL X360 object size (0x9A1300); it overflows this
    // incrementally-populated layout, so it's only safe once the layout is complete - debug-only,
    // not called on the boot/loading path.
    void BrnGameModule::DebugMemoryInit(BrnGameModule* lpData)
    {
        std::memset(lpData, 0, 0x9A1300);
        DebugSetMemoryToInt(&lpData->mGameStateModule, 292368, 0x7FFFFFFF);
        DebugSetMemoryToInt(&lpData->mGuiModule, 1629392, 0x7FFFFFFF);
        DebugSetMemoryToInt(&lpData->mNetworkModule, 866560, 0x7FFFFFFF);
        DebugSetMemoryToInt(&lpData->mInputModule, 5096, 0x7FFFFFFF);
    }

    // @ BrnGameModule.cpp:1845 - the per-frame update spine.
    //
    // Latches this frame's simulation-step bounds (forcing a single step when single-stepping
    // frames), begins the frame-rate manager's frame, releases the input read-lock, ticks the
    // debug manager and polls the step-frame request. Then it runs the active flow state's
    // Update() once per required simulation sub-step (recreating the static IO buffers between
    // steps), and finally the active state's Render() once for the frame, before tearing down
    // this frame's GUI/director IO buffers. Returns false (the X360 returns 0).
    //
    // The mCpuMonitors.* handles are the CPU perfmon slots the original brackets each region
    // with; the field names follow the BrnCpuMonitors declaration order at the X360 offsets.
    bool BrnGameModule::GameMain()
    {
        using namespace CgsDev;

        PerfMonCpu::StartMonitor(mCpuMonitors.miUT_TotalUpdate);

        mi8ActualFrameRateMinStepsThisFrame = mi8FrameRateMinSteps;
        mi8ActualFrameRateMaxStepsThisFrame = mi8FrameRateMaxSteps;
        CgsSystem::EFrameRateManagerType leFrameRateType = meFrameRateManagerType;
        if (mbSteppingFrames)
        {
            leFrameRateType = CgsSystem::E_FRAMERATEMANAGER_SINGLE;
            mi8ActualFrameRateMinStepsThisFrame = 1;
            mi8ActualFrameRateMaxStepsThisFrame = 1;
        }

        CGS_ASSERT(!_lbForceAssert, "!_lbForceAssert");

        mFrameRateManager.StartUpdateFrame(leFrameRateType, false);
        UnlockInputForRead();

        PerfMonCpu::StopMonitor(mCpuMonitors.miUT_TotalUpdate);

        // FLAG: ARTIST 0x823CB498 brackets DebugManager::Update with the monitor at
        // mCpuMonitors+0x48 (=miUT_DebugManager), not +0x4C (miUT_RenderAll). Fixed.
        PerfMonCpu::StartMonitor(mCpuMonitors.miUT_DebugManager);
        mDebugManager.Update(mfDebugUpdateDeltaSeconds * mfDebugUpdateTimeScale);
        UpdateRequestDoStepFrame();
        PerfMonCpu::StopMonitor(mCpuMonitors.miUT_DebugManager);

        PerfMonCpu::StartMonitor(mCpuMonitors.miUT_TotalUpdate);
        PerfMonCpu::SetNumIterationsTaken(miNumSimFramesRequired);
        sbSimUpdateComplete = 0;
        if (miNumSimFramesRequired > 0)
        {
            s32 liStep = 0;
            do
            {
                // ARTIST 0x823CB498 brackets Create/DestroyStaticIOBuffers with the monitor at
                // mCpuMonitors+0x10 -- under the corrected 40-field layout (no +0x08 hole,
                // miUT_Replay @+0x40) that handle is miUT_GameState.
                PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState);
                CreateStaticIOBuffers();
                mFrameRateManager.miPrevNumSimulationStepsRequired = liStep + 1;
                PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState);

                PerfMonCpu::StartMonitor(mCpuMonitors.miUT_EachUpdate);
                BrnGameMainFlowController::EMainGameFlowState leState = mMainFlowStateMachine.GetCurrentState();
                if (leState != BrnGameMainFlowController::E_MGS_INVALID)
                {
                    MainGameFlowState* lpState = mMainFlowStateMachine.GetState(leState);
                    lpState->Update();
                }
                // ---- the DIRECTOR's pre-GUI passes (X360 module-scheduler order) ----------
                // PreSceneQueryUpdate + Update. The module publishes its finalised camera into
                // mpDirectorOutputBuffer, which stays alive through this frame's Render.
                DoUpdate_Director(false);
                // ---- controller -> GUI input pass (the console per-substep bridge) ---------
                // Fill the player-0 pad record (InputPadsPC, the PC stand-in for the input
                // module's own fill), then run the REAL BridgeControllerToGui: it synthesises
                // the GUI controller events from the pad record and pushes them through
                // CgsGui::GuiModule::AddGuiEvent into this sub-step's GUI input buffer. The
                // GUI module drains that buffer during its Update below.
                {
                    // Advance the ToGui repeat/language-cycle clock (FLAG PC time source: the
                    // console words ride the game module's timer pass, unreconstructed).
                    const s64 liNowMs = static_cast<s64>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count());
                    miLanguageCycleTimerLo   = static_cast<s32>(liNowMs / 1000);
                    mfLanguageCycleTimerFrac = static_cast<f32>(liNowMs % 1000) * 0.001f;

                    mPcInputOutputBuffer.LockForWrite();
                    CgsInput::InputPadsPC::UpdatePlayer0(&mPcInputOutputBuffer);
                    mPcInputOutputBuffer.UnlockForWrite();

                    mPcInputOutputBuffer.LockForRead();
                    mpGuiInputBuffer->LockForWrite();
                    BridgeControllerToGui(mpGuiInputBuffer, &mPcInputOutputBuffer);
                    // The game->GUI flow-FSM bridge (X360 0x823DCA10, run under the same
                    // write bracket the console's LoadingScriptedState::Update uses): post
                    // any pending GuiEventRunFsm stage the main flow requested.
                    BridgeGameToGui(mpGuiInputBuffer);
                    // FLAG world-load stand-in: the console's GameState side reports the
                    // world-load status as game action 191, which the game module's
                    // translator (GameBridgeGameStateToX @0x823E9CE0, case 191) forwards to
                    // the GUI as GuiEvent<136> (load started) / GuiEvent<137> (load
                    // complete). The SCREEN flow's LOADING state blocks on 137 before it
                    // hands the flow to the in-game state. The PC has no world streaming
                    // yet, so its "world load" completes trivially: report loading-complete
                    // each sub-step while the in-game flow state is live (delivery is
                    // registration-filtered, so the event only reaches a state that is
                    // actually waiting on it). The real action-191 producer replaces this
                    // when the world-streaming pipeline lands.
                    if (BrnGameMainFlowController::gBrnInGameStateActive)
                    {
                        CgsGui::GuiEvent<137> lWorldLoadComplete;
                        CgsGui::GuiModule::AddGuiEvent(lWorldLoadComplete, mpGuiInputBuffer);
                    }
                    mpGuiInputBuffer->UnlockForWrite();
                    mPcInputOutputBuffer.UnlockForRead();

                    // Hand this sub-step's filled buffer to the GUI drive (FLAG bridge
                    // stand-in: the console passes it through the module scheduler's IO set).
                    mGuiModule.SetGuiEventInputBuffer(mpGuiInputBuffer);
                }
                // GUI module per-frame tick (drives the FSM controller + the HUD flow + the
                // MovieManager). The X360 ticks this through the module dispatch.
                mGuiModule.Update();
                // The GUI->game out-event consumer (X360 0x823CB758): latch the flow
                // commands (70/71, the loading screen 19/20, ...) the states posted.
                BridgeGuiToGame(mGuiModule.GetGuiOutQueue());
                // ---- the DIRECTOR's post-GUI pass (X360 module-scheduler order) -----------
                DoUpdate_Director(true);
                PerfMonCpu::StopMonitor(mCpuMonitors.miUT_EachUpdate);

                if (liStep != miNumSimFramesRequired - 1)
                {
                    PerfMonCpu::StartMonitor(mCpuMonitors.miUT_GameState);
                    DestroyStaticIOBuffers();
                    PerfMonCpu::StopMonitor(mCpuMonitors.miUT_GameState);
                }

                ++liStep;
            }
            while (liStep < miNumSimFramesRequired);
        }
        PerfMonCpu::StopMonitor(mCpuMonitors.miUT_TotalUpdate);
        sbSimUpdateComplete = 1;

        // FLAG: ARTIST 0x823CB498 brackets the per-frame Render with the monitor at
        // mCpuMonitors+0x4C (=miUT_RenderAll), not +0x50 (miUT_FrustumTesting). Fixed.
        PerfMonCpu::StartMonitor(mCpuMonitors.miUT_RenderAll);
        {
            BrnGameMainFlowController::EMainGameFlowState leState = mMainFlowStateMachine.GetCurrentState();
            if (leState != BrnGameMainFlowController::E_MGS_INVALID)
            {
                MainGameFlowState* lpState = mMainFlowStateMachine.GetState(leState);
                lpState->Render();
            }
        }

        mpUpdateOutputBufferStack->DestroyIOBuffer<BrnDirector::DirectorIO::OutputBuffer>(&mpDirectorOutputBuffer);
        mpUpdateOutputBufferStack->DestroyIOBuffer<CgsGui::ModelIO::OutputBuffer>(&mpGuiModelOutputBuffer);
        mpUpdateOutputBufferStack->DestroyIOBuffer<CgsGui::CgsGuiModuleIO::OutputBuffer>(&mpGuiOutputBuffer);
        mpUpdateInputBufferStack->DestroyIOBuffer<CgsGui::ViewIO::InputBuffer>(&mpGuiViewInputBuffer);
        mpUpdateInputBufferStack->DestroyIOBuffer<CgsGui::CgsGuiModuleIO::InputBuffer>(&mpGuiInputBuffer);

        PerfMonCpu::StopMonitor(mCpuMonitors.miUT_RenderAll);
        return false;
    }

    // @ BrnGameModule.cpp:2497 - allocate this sub-step's static GUI/director IO buffers from
    // the update input/output buffer stacks.
    void BrnGameModule::CreateStaticIOBuffers()
    {
        mpUpdateInputBufferStack->CreateIOBuffer<CgsGui::CgsGuiModuleIO::InputBuffer>(&mpGuiInputBuffer, "Gui");
        // The X360 CreateIOBuffer<InputBuffer> instantiation (@0x823AC898) runs the buffer's
        // Construct after the stack alloc; the generic PC template placement-news only, so
        // run the real Construct (@0x82857378) here.
        mpGuiInputBuffer->Construct();
        mpUpdateInputBufferStack->CreateIOBuffer<CgsGui::ViewIO::InputBuffer>(&mpGuiViewInputBuffer, "GuiView");
        mpUpdateOutputBufferStack->CreateIOBuffer<CgsGui::CgsGuiModuleIO::OutputBuffer>(&mpGuiOutputBuffer, "Gui");
        mpUpdateOutputBufferStack->CreateIOBuffer<CgsGui::ModelIO::OutputBuffer>(&mpGuiModelOutputBuffer, "GuiModel");
        mpUpdateOutputBufferStack->CreateIOBuffer<BrnDirector::DirectorIO::OutputBuffer>(&mpDirectorOutputBuffer, "Director");
        // The X360 CreateIOBuffer<T> instantiation runs the buffer's Construct after the stack
        // alloc; the generic PC template placement-news only. This was harmless while
        // DirectorIO::OutputBuffer was an EMPTY ODR stub with no lock state; now that the real
        // 1828-byte buffer is here, every DirectorModule pass locks it and the lock asserts
        // eStatusConstructed (CgsIOBuffer.cpp:27/36). Raise the base status here, exactly as
        // the GUI input buffer above does.
        if (mpDirectorOutputBuffer != 0)
            mpDirectorOutputBuffer->CgsModule::IOBuffer::Construct();
    }

    // @ BrnGameModule.cpp:2515 - free this sub-step's static GUI/director IO buffers (reverse
    // order of CreateStaticIOBuffers).
    void BrnGameModule::DestroyStaticIOBuffers()
    {
        mpUpdateOutputBufferStack->DestroyIOBuffer<BrnDirector::DirectorIO::OutputBuffer>(&mpDirectorOutputBuffer);
        mpUpdateOutputBufferStack->DestroyIOBuffer<CgsGui::ModelIO::OutputBuffer>(&mpGuiModelOutputBuffer);
        mpUpdateOutputBufferStack->DestroyIOBuffer<CgsGui::CgsGuiModuleIO::OutputBuffer>(&mpGuiOutputBuffer);
        mpUpdateInputBufferStack->DestroyIOBuffer<CgsGui::ViewIO::InputBuffer>(&mpGuiViewInputBuffer);
        mpUpdateInputBufferStack->DestroyIOBuffer<CgsGui::CgsGuiModuleIO::InputBuffer>(&mpGuiInputBuffer);
    }

    // @ BrnGameModule.cpp:4048 - apply a pending step/play-frame request: a step request
    // starts single-stepping and runs one step; a play request resumes from stepping.
    void BrnGameModule::UpdateRequestDoStepFrame()
    {
        if (mbRequestDoStepFrame)
        {
            mbSteppingFrames = true;
            mbDoStep = true;
            mbRequestDoStepFrame = false;
        }
        else if (mbRequestDoPlayFrame)
        {
            if (mbSteppingFrames)
                mbStopStepping = true;
            mbRequestDoPlayFrame = false;
        }
    }

    // @ BrnGameModule.cpp:3542 - build this frame's simulation update-set bitmask from the
    // active flow-state machine + game flags.
    BrnUpdateSet BrnGameModule::ConstructUpdateSetFromFsm()
    {
        u32 luUpdateSet = 0x80;
        if (mMainFlowStateMachine.IsInGameState())
            luUpdateSet = 0x88;
        if (mMainFlowStateMachine.IsSaveLoadState())
            luUpdateSet = (luUpdateSet & 0xFF3F) | 0x40;
        if (mMainFlowStateMachine.IsVideoState())
            luUpdateSet |= 0x20;
        if (mbSimPaused || mMainFlowStateMachine.IsSaveLoadState())
            luUpdateSet |= 0x1;
        if (mbStalled)
            luUpdateSet |= 0x400;
        if (mbDiskError)
            luUpdateSet |= 0x200;
        return (BrnUpdateSet)luUpdateSet;
    }

    // @ BrnGameModule.cpp:1146 - the update-thread stage machine: drives GamePrepare (one-time
    // setup) -> GameMain (per-frame update, repeating) -> GameRelease (teardown). meGameUpdateStage
    // is PREPARE(0)/MAIN(1)/RELEASE(2). Returns true to keep the thread alive.
    bool BrnGameModule::UpdateThread()
    {
        switch (meGameUpdateStage)
        {
        case E_GAMEUPDATESTAGE_PREPARE:
            if (!GamePrepare())
                return true;
            meGameUpdateStage = E_GAMEUPDATESTAGE_MAIN;
            GameMain();
            return true;

        case E_GAMEUPDATESTAGE_MAIN:
            meGameUpdateStage = E_GAMEUPDATESTAGE_MAIN;
            GameMain();
            return true;

        case E_GAMEUPDATESTAGE_RELEASE:
            meGameUpdateStage = E_GAMEUPDATESTAGE_RELEASE;
            if (GameRelease())
                meGameUpdateStage = E_GAMEUPDATESTAGE_PREPARE;
            return true;

        default:
            CGS_ASSERT(false, "Invalid update stage\n");
            return true;
        }
    }

    // @ BrnGameModule.cpp:3872 - debug helper: fill liNumBytes at lpDest with the repeating
    // 32-bit pattern luValue (head/tail handled byte-wise). Used by DebugMemoryInit to stamp
    // freed module memory with a marker value.
    void BrnGameModule::DebugSetMemoryToInt(void* lpDest, s32 liNumBytes, u32 luValue)
    {
        u8*       lpByte = static_cast<u8*>(lpDest);
        const u8* lpPattern = reinterpret_cast<const u8*>(&luValue);
        for (s32 i = 0; i < liNumBytes; ++i)
            lpByte[i] = lpPattern[i & 3];
    }

    // @ BrnGameModule.cpp:1515 - after the GPU vsync wait completes, ask the frame-rate
    // manager how many simulation steps this render frame must run (driving GameMain's
    // catch-up loop count).
    void BrnGameModule::OnCompletionOfVsyncWait()
    {
        miNumSimFramesRequired = mFrameRateManager.UpdatePostRenderWait(
            mi8ActualFrameRateMinStepsThisFrame, mi8ActualFrameRateMaxStepsThisFrame);
    }

    // @ BrnGameModule.cpp:1565 - render the on-screen assert overlay via the renderer module.
    void BrnGameModule::RenderAssert(const AssertData* lpAssertData)
    {
        mRenderModule.RenderAssert(lpAssertData);
    }

    // @ BrnGameModule.cpp:3981 - debug-menu "step frame" callback: begin single-stepping and
    // request one step. The void* context is the game-module instance.
    void BrnGameModule::StepFrameCB(void* lpContext)
    {
        BrnGameModule* lpThis = static_cast<BrnGameModule*>(lpContext);
        lpThis->mbSteppingFrames = true;
        lpThis->mbDoStep = true;
    }

    // @ BrnGameModule.cpp:3996 - debug-menu "play frame" callback: while single-stepping,
    // request a resume.
    void BrnGameModule::PlayFrameCB(void* lpContext)
    {
        BrnGameModule* lpThis = static_cast<BrnGameModule*>(lpContext);
        if (lpThis->mbSteppingFrames)
            lpThis->mbStopStepping = true;
    }

    // @ X360 0x823A8B38 - the disk-error worker thread body. Raises the system dirty-disc
    // error UI for the given user and never returns (the console holds on the error UI).
    // Tail-calls straight into the XDK entry point.
    void BrnGameModule::DiskErrorThreadProc(unsigned long dwUserIndex)
    {
        XShowDirtyDiscErrorUI(dwUserIndex);
    }

    // @ X360 0x823C0268 - return the embedded hardware object (the launch/soft-reboot data
    // block the platform fills at boot). The X360 forms this + 0x9A11C0, which is &mHardware.
    BrnHW::System360HW* BrnGameModule::GetSoftRebootData()
    {
        return &mHardware;
    }

    // @ X360 0x823C0288 - forward to the hardware object's invite-reboot query (the X360
    // hands it &mHardware as the System360HW `this` and tail-calls).
    int BrnGameModule::HasGameBeenRebootedDueToInvite()
    {
        return mHardware.HasGameBeenRebootedDueToInvite();
    }

    // @ X360 0x823C0278 - the soft-reboot flag. Reads a single byte at game-module + 0x9A1254,
    // which is mHardware + 0x94 (the launch-data flag byte BrnHW::System360HW::mPad94).
    int BrnGameModule::HasGameBeenSoftRebooted()
    {
        return mHardware.mPad94;
    }

    // @ X360 0x823A9030 - the game module's GameTalk "StopRenderMetrics" receiver. Walks
    // every key of the incoming message and, on the "StopRenderMetrics" key, clears the
    // render-metrics-requested flag. Same receiver shape as
    // BrnDirector::Camera::BehaviourRenderMetrics::GameTalkMessageReceiver: the context
    // module is delivered by pointer-to-pointer (lppModule).
    void BrnGameModule::RenderMetricsMessageHandler(EA::GameTalk::GameTalkMessage* lpMessage,
                                                    BrnGameModule** lppModule)
    {
        BrnGameModule* lpModule = *lppModule;
        for (s32 liIndex = 0; liIndex < lpMessage->GetNumKeys(); ++liIndex)
        {
            const char* lpcKey = lpMessage->GetKey(liIndex);
            if (_stricmp(lpcKey, "StopRenderMetrics") == 0)
                lpModule->miRenderMetricsRequested = 0;
        }
    }
}
