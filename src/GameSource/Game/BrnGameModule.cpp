#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert
#include "GameShared/GameClasses/Development/DebugSystem/CgsDebugFontBringUp.h"   // LoadAndSetDebugFont
#include "SDKs/EA/GameTalk/GameTalk.h"               // EA::GameTalk::GameTalkMessage (RenderMetricsMessageHandler)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"      // BrnGui::GuiAudioTriggerEvent (GUI-out event 201) + GuiEventProgressionProfileData (350)
#include "GameSource/GameState/Progression/BrnProfile.h" // BrnProgression::Profile (the action-193 payload's first word)
#include "GameShared/GameClasses/System/PC/CgsGuiSoundPC.h" // GUI presentation-sound PC consumer
#include "GameShared/GameClasses/System/PC/CgsMovieAudioPC.h" // SpeechAudioPC -- the voice-over leaf

#include <cstring>   // memset
#include <string.h>  // _stricmp (RenderMetricsMessageHandler; MSVC canonical, not declared by <cstring>)
#include <chrono>    // the ToGui repeat-clock feed (FLAG PC time source: wall clock)

#include "GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h" // CgsInput::InputPadsPC (the PC pad-fill leaf)
#include "GameShared/GameClasses/Gui/CgsGuiModule.h" // CgsGui::GuiModule::AddGuiEvent (the world-load report below)
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h" // CgsGui::GuiEventTimeInfo (the per-frame GUI timestep)
#include "GameSource/Game/BrnLoadingScreenRenderer.h" // BrnGame::ELoadingScreenCommand (BridgeGuiToGame's command slot)
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h" // ValidityAccount::SetupFailFlagMask (interim bridge in Construct)
#include "GameSource/Resource/BrnGameDataModuleIO.h" // GameDataIO::InputBuffer/OutputBuffer (GamePrepare's request bracket)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"          // DirectorIO::InputBuffer (DoUpdate_Director)
#include "GameSource/GameState/BrnGameStateModuleIO.h" // GameStateModuleIO::OutputBuffer (BridgeGameStateToDirector)
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
        , mpWorldUpdateOutputBuffer(0)
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

        // The calibration settings BridgeGuiToGame publishes every frame -- Construct @0x823C9EA8
        // seeds them (`*(v9 + 10096740) = 50; *(v9 + 10096744) = 50; *(v9 + 10096748) = 1;`): the
        // game's default slider position, i.e. the post-fx composite's NEUTRAL brightness/contrast.
        miBrightness                        = 50;
        miContrast                          = 50;
        mbEnableCalibrationUnfriendlyPostFx = true;

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

        // ---- X360 step 9 (the part that matters to every timed subsystem) -----------------
        // Construct @0x823C9EA8, verbatim:
        //     CgsSystem::TimerStatusInterface::Clear(gm + 10095372);
        //     lfRate = 1.0 / <vsync refresh rate>;               // the 50/60 the step above checks
        //     CgsSystem::Timer::Prepare(gm + 10095316, lfRate);  // the GAME timer
        //     CgsSystem::Timer::Prepare(gm + 10095344, lfRate);  // the SIM timer
        //     *(gm + 10095340) = 1;                              // gameTimer.mbRunning (+24)
        //     *(gm + 10095368) = 1;                              // simTimer.mbRunning  (+24)
        // FLAG (PC-platform leaf): the console reads the refresh rate out of the display mode
        // it validated one step earlier (50 or 60, asserting anything else); this build has no
        // reconstructed mode query, so it takes the 60Hz arm -- the same value the rest of the
        // PC bring-up already assumes. Everything downstream reads the RATE, not the constant.
        {
            const f32 lfTimerRate = 1.0f / 60.0f;
            mTimerStatusInterface.Clear();
            mGameTimer.Prepare(lfTimerRate);
            mSimTimer.Prepare(lfTimerRate);
            mGameTimer.SetRunning(true);
            mSimTimer.SetRunning(true);
        }

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
        mbGuiVoiceOverPending  = false;
        muGuiVoiceOverHash     = 0;
        mbGuiVoiceOverSounding = false;
        mbDirectorCameraLive   = false;
        mbPlayerCarCrashing    = false;
        mbWorldDataPrepared    = false;
        mbCarSelectionPublished = false;
        mbCarSelectActivatePending  = false;
        maiPendingCarSelectActivate[0] = 0;
        maiPendingCarSelectActivate[1] = 0;
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
        // The event walk is skipped without a queue (a PC guard: the console always has the GUI
        // output buffer's queue); the publish tail below runs regardless, as on the console.
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = (lpGuiOutQueue != 0) ? lpGuiOutQueue->GetFirstEvent(&lpEvent, &liSize) : -1;
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
                    case 466:
                        // A GUI voice-over REQUEST (the {4, 466, 12, <name hash>} record
                        // BrnGui::Intro's SetupComponents / HandleTransitionFrom* /
                        // Update post through OutputGuiEvent<GuiEventAudioVoiceOver>).
                        // On the console this leaves through BridgeGuiToSound and the
                        // sound module answers it on the GUI input side; see the FLAG'd
                        // reply block in the GUI-input write bracket below.
                        // The payload word (record +12) is the line's
                        // CgsSound::Playback::Name::MakeHash id -- carry it through so the
                        // speech player can resolve and sound the actual stream.
                        muGuiVoiceOverHash    = reinterpret_cast<const u32*>(lpEvent)[3];
                        mbGuiVoiceOverPending = true;
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
                    case 405:
                        // ⭐ THE CAR-SELECT SCREEN'S OWN DATA REQUEST. Every car-select screen
                        // state posts command 405 from CarSelectMain::OnEnter @0x824C8920 --
                        // it is the console's "this screen is up, send me the car-select
                        // data" request, and the 406 / 412 / 413 records are its answers.
                        // Re-arming the publish latch here is what makes the answers arrive
                        // ONCE PER SCREEN instead of once per boot.
                        //
                        // ⚠️ THIS MATTERS AND IT WAS FOUND BY BOOT: every screen state is its
                        // OWN object with its OWN mCurrentSetupInfo / car list. Publishing once
                        // per boot fed CS_VEHICLE and left CS_LIVERY with mCarId == (CgsID)-1
                        // and an EMPTY livery selector -- five asserts a frame from
                        // CanCarBePainted and GetHighlighted(). The latch is armed by the
                        // request now, exactly as the console's producer is.
                        mbCarSelectionPublished = false;
                        break;
                    case 192:
                    {
                        // ⭐⭐ THE CAR-SELECT ACTIVATE RECORD -- the GUI half of the junkyard
                        // handover. { 8, 192, 12, action, carSelectType }, 20 bytes on channel
                        // 40, posted by CarSelectVehicle::Update (action 0 == start),
                        // CarSelectLivery::Update (action 1 == enter modification) and
                        // CarSelectMain::ExitCarSelection (action 4 == exit the junkyard).
                        //
                        // The console routes it BridgeGuiToGameState @0x823DDB78 case 192 ->
                        // game event 94 (8 bytes: the same two words, in the same order) ->
                        // ProcessGameEvents case 94. GameBridgeGUIToX.cpp already reconstructs
                        // that translation faithfully, but it has no caller and its sink
                        // (GameStateModuleIO::PostWorldInput) has no definition, so this walk --
                        // which is already over the very same out-queue -- performs the SAME
                        // decode and calls the extracted case-94 arm directly.
                        // [FLAG PC bring-up] DELETE-WHEN BridgeGuiToGameState has a caller and
                        // ProcessGameEvents drains a real post-world input buffer.
                        // ⚠️ LATCHED, NOT DISPATCHED HERE. This walk runs inside the GUI phase;
                        // the game-state module's output buffer belongs to the SIM spine and is
                        // only written under its own Lock/Unlock bracket there. The request is
                        // therefore handed to the same sub-step leg that already ticks the
                        // CarSelectManager (see the E_MGS_IN_GAME block in DoUpdate), which is
                        // also the console's ordering: ProcessGameEvents runs inside
                        // PreWorldUpdate, not inside the GUI update.
                        maiPendingCarSelectActivate[0] =
                            static_cast<s32>(reinterpret_cast<const u32*>(lpEvent)[3]);   // action
                        maiPendingCarSelectActivate[1] =
                            static_cast<s32>(reinterpret_cast<const u32*>(lpEvent)[4]);   // type
                        mbCarSelectActivatePending = true;
                        break;
                    }
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
        // ⚠️ The Clear moved OUT of this function (fly-by wave). The console does not clear
        // here either -- BridgeGuiToDirector @0x823CBF70 walks the SAME queue later in the same
        // frame, and clearing it in the first consumer would drop every director command on the
        // floor. DoUpdate now clears once, after both bridges have run.

        // ---- the console's UNCONDITIONAL publish tail (@0x823CB758, after the event loop) -----
        //     CgsModule::IOBuffer::LockForWrite(v4);                                  v4 = the WRITE buffer
        //     DispatchThreadInputBuffer::SetBrightness(v4, *(a1 + 10096740));         miBrightness
        //     DispatchThreadInputBuffer::SetContrast(v4, *(a1 + 10096744));           miContrast
        //     DispatchThreadInputBuffer::SetCalibrationUnfriendlyEnablePostFx(v4, *(a1 + 10096748));
        //     DispatchThreadInputBuffer::SetCalibrationTextureHandle(v4, v15);       v15 = qword_82FAE900
        //     CgsModule::IOBuffer::UnlockForWrite(v4);                                (the null handle;
        //                                                                              event 546 overrides)
        // This is how the options-menu brightness / contrast reach the renderer: BrnRendererModule::
        // Render reads them off the READ buffer for the post-fx composite (GetBrightness @0x8240DCC8 /
        // GetContrast @0x8240DCFC). Landed 2026-08-15 with the composite: without it the buffer read
        // 0 / 0 and the frame came out at contrast 0.5 / brightness -0.5 -- black. The texture handle
        // is left as the buffer's own Clear()'d value (== the console's qword_82FAE900 null handle,
        // the same word the SetDebugFont family compares against); GUI event 546 -- the calibration
        // screen's BrightnessContrastPostFxControl -- is a follow-on and not handled yet, so the
        // handle is never anything else here.
        {
            BrnGame::DispatchThreadInputBuffer* const lpWrite = mDispatchThreadInputBufferManager.GetWriteBuffer();
            lpWrite->LockForWrite();
            lpWrite->SetBrightness(miBrightness);
            lpWrite->SetContrast(miContrast);
            lpWrite->SetCalibrationUnfriendlyEnablePostFx(mbEnableCalibrationUnfriendlyPostFx);
            lpWrite->UnlockForWrite();
        }
    }

    // ------------------------------------------------------------------------------------
    // BridgeGuiToDirector  @ 0x823CBF70   -- ⭐ THE GUI -> DIRECTOR SEAM
    //
    // The console walks the GUI module's out-event queue and, for each recognised command,
    // raises the matching published flag on the DIRECTOR's input buffer. Every store is a
    // literal 1 (or a payload word) into the buffer's flag tail; MainDirector::PostGuiUpdate
    // @0x82236F88 reads them all back one pass later and folds them into the director's
    // GameState. Reconstructed arm-for-arm from the X360 switch; each `case` below carries the
    // console's own offset so the mapping is checkable.
    //
    // ⭐ 476 / 477 / 478 are the whole point of this function for the intro: BrnGui::Intro
    // posts 477 at START_FLYBY and 478 when its 7.67 s dwell expires, and until this bridge
    // existed NOTHING carried them anywhere -- the director never learned the front-end had
    // asked for a fly-by. Their meanings are NOT assumed: 477 -> +0x7ABE ->
    // GameState::mbGameIntroFlybyActive is pinned by ArbStateCarSelect::Update's own assert
    // string, and 476/478 fall either side of it in both the DWARF member order and the asm.
    //
    // FLAG PC-ABI adapter (identical to BridgeGuiToGame's): on the console the GUI out queue
    // re-keys each record by its GuiEventWrapper type, so GetFirstEvent returns 477 directly
    // and hands back the boxed payload. The PC GuiModule keeps the records on their CHANNEL
    // (40) and forwards them verbatim, so the type is read out of the record's own header word
    // and the payload is taken at the record's own miOutEventOffset -- which is exactly what
    // the console's GuiEventWrapper::GetRawEvent() does, so the payload words the arms read are
    // the same words.
    // ------------------------------------------------------------------------------------
    void BrnGameModule::BridgeGuiToDirector(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
                                            CgsModule::VariableEventQueue<18432, 16>* lpGuiOutQueue)
    {
        if (lpDirectorInput == 0 || lpGuiOutQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpGuiOutQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liId >= 0 && lpEvent != 0)
        {
            // ---- resolve (command, payload) -- see the PC-ABI FLAG above ------------------
            const u32* lpuRecord  = reinterpret_cast<const u32*>(lpEvent);
            s32        liCommand  = liId;
            const u32* lpuPayload = lpuRecord;
            if (liId == 40 && liSize >= 12)
            {
                liCommand = static_cast<s32>(lpuRecord[1]);
                const u32 luOffset = lpuRecord[2];
                if (luOffset >= 12u && static_cast<s32>(luOffset) < liSize)
                {
                    lpuPayload = reinterpret_cast<const u32*>(
                        reinterpret_cast<const u8*>(lpEvent) + luOffset);
                }
            }
            const s32 liPayload0 = static_cast<s32>(lpuPayload[0]);

            switch (liCommand)
            {
                case 77:    // +0x7ACC  the car-select ticker closed
                    lpDirectorInput->SetCarSelectTickerClosedThisFrame();
                    break;

                case 94:    // the shortcut menu opened/closed (payload word = the new state)
                    lpDirectorInput->SetShortcutMenuEvent(liPayload0 != 0);
                    break;

                case 191:   // crash-nav shown/hidden -- ignored entirely when payload[1] == 2
                    if (static_cast<s32>(lpuPayload[1]) != 2)
                    {
                        if (liPayload0 == 0)
                            lpDirectorInput->SetGotCrashNavShownEvent();
                        else if (liPayload0 == 1)
                            lpDirectorInput->SetGotCrashNavHiddenEvent();
                    }
                    break;

                case 192:   // +0x7AC2  end of car select (only for payload {4, 1})
                    if (liPayload0 == 4 && static_cast<s32>(lpuPayload[1]) == 1)
                        lpDirectorInput->SetEndOfCarSelect();
                    break;

                case 290:   // +0x7AD0  entered the online post-event flow
                    lpDirectorInput->SetEnteredOnlinePostEvent();
                    break;

                case 294:   // +0x7ACF  left the online post-event flow
                    lpDirectorInput->SetLeftOnlinePostEvent();
                    break;

                case 298:   // the console's own "over to you, Big E..." placeholder -- a debug
                            // print and nothing else. Kept because it is real console output.
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0
                        && CgsDev::Log::gpDebugPrint != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "Handle new rival camera presentation stuff here - over to you, Big E...\n";
                    }
                    break;

                case 303:   // +0x7ABC / +0x7AB4  rank up this frame + the new rank. The X360
                            // also runs the colour-calibration-shown store on this arm (the
                            // compiler merged the two tails); reproduced.
                    lpDirectorInput->SetRankUp(liPayload0);
                    lpDirectorInput->SetGotColourCalibrationShownEvent();
                    break;

                case 415:   // +0x7ACB  the car selection changed
                    lpDirectorInput->SetCarSelectionChangedThisFrame();
                    break;

                case 469:   // +0x7AD3 / +0x7AD4  the 100%-completion sequence start/finish
                case 470:   // (the two ids share one body on the console)
                    if (liPayload0 != 0)
                        lpDirectorInput->SetStarting100PercentSequence();
                    else
                        lpDirectorInput->SetFinished100PercentSequence();
                    break;

                case 475:   // +0x7AC9 / +0x7AA4  new director profile data + its payload word
                    lpDirectorInput->SetDirectorProfileData(liPayload0);
                    break;

                case 476:   // +0x7ABD  ⭐ start the NEW-PROFILE intro
                    lpDirectorInput->SetStartNewProfileIntro();
                    break;

                case 477:   // +0x7ABE  ⭐ START the GAME-INTRO FLY-BY
                    lpDirectorInput->SetStartGameIntroFlyby();
                    break;

                case 478:   // +0x7ABF  ⭐ STOP the game-intro fly-by (clears both latches)
                    lpDirectorInput->SetStopGameIntroFlyby();
                    break;

                case 479:   // +0x7AD2  online event loading started
                    lpDirectorInput->SetStartedOnlineEventLoading();
                    break;

                case 480:   // +0x7AD1  online event loading finished
                    lpDirectorInput->SetFinishedOnlineEventLoading();
                    break;

                case 501:   // the GUI PFX hook enumeration (a 404-byte publish). The console
                            // memcpys all 404 bytes off the payload unconditionally; the size
                            // guard is ours, because a short record here would read past the
                            // queue entry (the exact class of bug that cost this project the
                            // intro AV). No producer posts 501 on this build yet.
                    if (liSize >= static_cast<s32>(404 + (reinterpret_cast<const u8*>(lpuPayload)
                                                          - reinterpret_cast<const u8*>(lpEvent))))
                    {
                        lpDirectorInput->SetHookEnumeration(lpuPayload);
                    }
                    break;

                case 514:
                    lpDirectorInput->SetGotColourCalibrationShownEvent();
                    break;

                case 515:
                    lpDirectorInput->SetGotColourCalibrationHiddenEvent();
                    break;

                case 591:   // +0x7AB8  the requested camera type. The console stores 0 or 1 and
                            // fires "Unhandled camera type : <n>" for anything else WITHOUT
                            // storing (GameBridgeGUIToX.cpp:1657).
                    CGS_ASSERT(liPayload0 == 0 || liPayload0 == 1, "Unhandled camera type");
                    if (liPayload0 == 0 || liPayload0 == 1)
                        lpDirectorInput->SetCameraType(liPayload0);
                    break;

                default:
                    break;
            }

            const CgsModule::Event* lpNext = 0;
            liId = lpGuiOutQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
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

        // Two of the six X360 input-staging bridges are committed now: the CONTROLLER leg and
        // (2026-08-01) the GAME-STATE leg.
        // [FLAG PC boot gate] BridgeNetworkToWorld @0x823DF8B0, BridgeGuiToWorld @0x823CBE90,
        // BridgeSoundToWorld @0x823CDC98 and the replay-status latch
        // (ReplayIO::OutputBuffer_PreSim::GetStatusInterface) are still not reconstructed; their
        // source modules' output buffers are not threaded into this leg on the PC yet, so that
        // staging is omitted rather than faked. The X360's SetTimerStatusInterface(gm+10095372)
        // is part of the same staging. Restore them with the DoUpdate cascade.
        lpWorldInput->LockForWrite();
        BridgeControllerToWorld(lpWorldInput, lpInputOutputBuffer);
        lpWorldInput->UnlockForWrite();

        // ⭐⭐ THE GAME-STATE -> WORLD BRIDGE (X360 BridgeGameStateToWorld @0x823E1890), the
        // only producer of the world update input buffer's game-action queue. The console gets
        // the game-state output buffer from DoUpdate's own LockBuffersForIO five-source call
        // (sub_823B7620); this leg reaches the module it owns directly, exactly as the director
        // leg does for BridgeGameStateToDirector.
        // ⚠️ NOTE this entry is currently UNREACHED -- DoUpdate is a PC-platform leaf, so the
        // live world drive is DriveWorldUpdateFrame (BrnGameMainFlowStates.cpp), which runs the
        // same bridge. Both sites move together.
        {
            BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput =
                mGameStateModule.GetOutputBuffer();
            if (lpGameStateOutput != 0)
            {
                lpGameStateOutput->LockForRead();
                lpWorldInput->LockForWrite();
                BridgeGameStateToWorld(lpWorldInput, lpGameStateOutput);
                lpWorldInput->UnlockForWrite();
                lpGameStateOutput->UnlockForRead();
            }
        }

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
    // ------------------------------------------------------------------------------------
    // UpdateTimers  @ 0x823BCFD0
    //
    // The console body is:
    //     LockForRead(gameStateOut); LockForRead(directorOut);
    //     TimerRequests::Append(gm+10095420, GameStateModuleIO::OutputBuffer::GetTimerR(...));
    //     TimerRequests::Append(gm+10095428, ... + 8);
    //     if ( !((GetTimerR(...)[2] >> 2) & 1) )      // the "director owns the timers" bit
    //     {
    //         lpIn = DirectorIO::OutputBuffer::GetTimerRequestIn(directorOut);
    //         TimerRequests::Append(gm+10095420, lpIn);
    //         TimerRequests::Append(gm+10095428, lpIn + 8);
    //     }
    //     UnlockForRead(directorOut); UnlockForRead(gameStateOut);
    //     TimerRequestInterface::ApplyToTimers(gm+10095420, gm+10095316, gm+10095344);
    //     CgsSystem::Timer::Update(gm+10095316);
    //     CgsSystem::Timer::Update(gm+10095344);
    //
    // ⚠️ QUIET GATE (the request half): the two producers -- the GameState module's timer
    // request block and DirectorIO::OutputBuffer::GetTimerRequestIn -- are BOTH un-staged on
    // this build (the GameState module is a placeholder, and the director output's timer
    // request interface is never written). Appending from unwritten sources would apply
    // garbage pause/slomo scales to the timers, which is strictly worse than applying none:
    // with no requests, ApplyToTimers is the identity (scale target stays 1.0) and the two
    // Update calls below are the whole observable effect. So only the tail runs.
    // DELETE-WHEN: the GameState module is real and the director stages its timer requests.
    // ------------------------------------------------------------------------------------
    void BrnGameModule::UpdateTimers()
    {
        mGameTimer.Update();
        mSimTimer.Update();
    }

    // ------------------------------------------------------------------------------------
    // BridgeTimers  @ 0x823BD150   -- ⭐ THE FRAME TIMESTEP PUBLISH
    //
    //     *(gm+10095424) = 1.0f;  *(gm+10095420) = 0;      // the game TimerRequests reset
    //     *(gm+10095432) = 1.0f;  *(gm+10095428) = 0;      // the sim  TimerRequests reset
    //     TimerStatusInterface::StoreTimers(gm+10095372, gm+10095316, gm+10095344);
    //     LockForWrite(directorInput);
    //       <48-byte copy of gm+10095372 into DirectorIO::InputBuffer::GetTimerStatusInterface()>
    //     UnlockForWrite(directorInput);
    //
    // The two TimerRequests resets belong to the un-staged request half documented on
    // UpdateTimers above (they clear the accumulators the Appends would have filled); with no
    // producers there is nothing to clear, so they are gated with it. The StoreTimers snapshot
    // and the copy into the director input are the live, load-bearing half: without them the
    // director input's timer status stays at DoUpdate_Director's zero-fill, every
    // TimerStatus::GetCurrentTimeStep() reads 0, and the whole camera-behaviour middle
    // advances by `speed * 0` per frame.
    // ------------------------------------------------------------------------------------
    void BrnGameModule::BridgeTimers(BrnDirector::DirectorIO::InputBuffer* lpDirectorInput)
    {
        mTimerStatusInterface.StoreTimers(&mGameTimer, &mSimTimer);

        lpDirectorInput->LockForWrite();
        *lpDirectorInput->GetTimerStatusInterface() = mTimerStatusInterface;
        lpDirectorInput->UnlockForWrite();
    }

    // ------------------------------------------------------------------------------------
    // BrnGameModule::BridgeGameStateToDirector  @ X360 0x823CD170
    // (console home GameSource/Game/GameBridgeGameStateToX.cpp; body kept here with the other
    //  DoUpdate_Director bridges -- the BridgeGuiToDirector precedent.)
    //
    // ⭐ THE GAME-ACTION LEG (the whole reason this bridge exists, and the only leg whose two
    // ends are real today):
    //     lpDirectorInput->GetGameActionQueue()->Append( *lpGameStateOutput->GetGameActionQueue() )
    // The X360 renders it as
    //     v29 = sub_823B96F0(a3);                                  // gameStateOut->GetGameActionQueue() const
    //     v30 = BrnDirector::DirectorIO::InputBuffe(a2);           // directorIn->GetGameActionQueue()
    //     CgsModule::VariableEventQueue<13312,16>::Append<13312,16>(v30, v29);
    // -- the console itself proving both queues are the SAME instantiated type.
    //
    // ⚠️ THE BRIEF'S "TWO-LINER" IS WRONG, and it matters: 0x823CD170 is a ~150-line function
    // whose OTHER legs are, in console order --
    //   * a BrnReplays::DirectorBridgeSerialiser state machine wrapped round the whole body
    //     (Lock / GetStaticLayout / Unlock; on replay PLAYBACK the game actions are Appended
    //     from the SERIALISED layout at +10128 instead of from the live buffer, and on RECORD
    //     they are Appended into it as well);
    //   * a walk of the game-state output buffer's takedown-event queue (this+0x4040) matching
    //     each record's race-car index against the player's, publishing mbPlayerTakenDown /
    //     mePlayerKillerCarIndex into the director input (+31424 / +31404) with the verbatim
    //     assert "lePlayerKillerRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID"
    //     (GameBridgeGameStateToX.cpp:228);
    //   * the player's vehicle TEAM into the director input (+31408) and all 8 teams through
    //     DirectorIO::InputBuffer::SetVehicleTeam;
    //   * a CarScoreData::operator= of the player's score record (gameStateOut+173240 + 296*i)
    //     into the director input at +12560, plus +12888 and the two flags at +31445/+31446.
    // NONE of those legs is reconstructed here: every source member they read is still opaque
    // storage in this model's GameStateModuleIO::OutputBuffer and every one of their consumers
    // in the director input is likewise unmodelled. Writing them against opaque zeroes would be
    // the "data arrives wrong-but-plausible" failure mode, so they are documented, not faked.
    // DELETE-WHEN: the takedown queue / score / team spans of OutputBuffer are typed.
    // ------------------------------------------------------------------------------------
    void BrnGameModule::BridgeGameStateToDirector(
        BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        lpDirectorInput->GetGameActionQueue()->Append(*lpGameStateOutput->GetGameActionQueue());
    }

    // ------------------------------------------------------------------------------------
    // ⭐⭐ BridgeGameStateToWorld  @ 0x823E1890
    //
    // Twelve transfers, no branches -- the entire console body is `mr r3,r30 / bl <getter> /
    // mr r4,r3 / mr r3,r31 / bl <setter>` twelve times over. Called by DoUpdate_World
    // @0x823E8BD0 with this sub-step's world UPDATE INPUT buffer (r4) and the game-state
    // module's OUTPUT buffer (r5). The caller owns both locks (world input write-locked,
    // game-state output read-locked); every accessor below asserts one or the other, which is
    // how the console proves that bracket.
    //
    // WHY THE FIRST LINE IS THE POINT OF THE WHOLE FUNCTION. It is the ONLY producer of the
    // world update input buffer's game-action queue in the entire image.
    // WorldModule::HandleGameActions (0x827C44D8), WorldModule::BridgeInputToEntityModules
    // (0x827ADF88) and WorldModule::BridgeActionsTo{Physics,Traffic}Module all DRAIN
    // UpdateInputBuffer::GetGameActionQueue(); nothing else ever fills it. Until now every one
    // of them was draining a queue that could not be non-empty, so no game action reached the
    // world at all -- including ResetPlayerCarAction (type 0), the record CarSelectManager
    // builds to place the player's car at a junkyard spawn location.
    //
    // FIVE of the twelve getters are UNNAMED in the ARTIST exports (sub_823B96F0 /
    // sub_823B9840 / the truncated "BrnGameState::GameStateM" @0x823B9AE0 / sub_823B9B88 /
    // sub_823BA038) and TWO are INLINED (the bridge computes `+173240` and `+175976` itself,
    // asm 0x823E1938 and 0x823E1948). All seven are identified at their accessors in
    // BrnGameStateModuleIO.h by the member offset each returns plus its assert's own
    // __FILE__/__LINE__ -- the same recovery route GetGameActionQueue() const already used.
    //
    // FLAG (home): the DWARF home is GameSource/Game/GameBridgeGameStateToX.cpp, alongside its
    // five siblings. The body sits HERE for the same reason BridgeGameStateToDirector's does --
    // and, this time, for a measured one as well: that TU IS NOT MOUNTED and DOES NOT COMPILE
    // (`mpCgsGuiModule` is referenced but declared nowhere, and its BrnGui::GuiTakedownEvent /
    // GuiSoftTakedownEvent placeholders collide with the real definitions in
    // BrnGuiEventTypeDefs.h:982 / BrnGuiDemangledEventTypes.h:251). Re-home both bridges there
    // when that TU is repaired and mounted.
    //
    // ⚠️ ONE DELIBERATE DEVIATION FROM A LITERAL TRANSCRIPTION, and it is load-bearing. The
    // console passes the scoring snapshots as raw addresses because there the source span and
    // the destination slice are both 2736 bytes. MEASURED on this x64 build:
    // sizeof(GameStateModuleIO::ScoringOutputInterface) == 2672, while
    // BrnWorldIO::SetScoringInterface still memcpy's the console's 2736. So the source goes
    // through GetScoringOutputInterface(), whose storage is deliberately kept at the CONSOLE's
    // width -- see the ⚠️ at that accessor. Without that the bridge would over-read the source
    // by 64 bytes every frame.
    // ------------------------------------------------------------------------------------
    void BrnGameModule::BridgeGameStateToWorld(
        BrnWorldIO::UpdateInputBuffer* lpWorldInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        CGS_ASSERT(lpWorldInput != 0, "lpWorldInput");
        CGS_ASSERT(lpGameStateOutput != 0, "lpGameStateOutput");

        // 1. the GAME-ACTION queue (sub_823B96F0 -> AppendGameActionQueue 0x823C8B80). Both
        //    sides are the same CgsModule::VariableEventQueue<13312,16> instantiation.
        lpWorldInput->AppendGameActionQueue(lpGameStateOutput->GetGameActionQueue());

        // 2. the takedown-event queue (sub_823B9840 -> AppendTakedownEventQueue 0x823C8C38).
        //    FLAG cross-home cast: GameStateModuleIO::TakedownEventOutputQueueType is still a
        //    forward-declared incomplete class; BrnWorldIO names the same X360 payload as the
        //    committed EventQueue<BrnGameState::TakedownEvent,8>.
        lpWorldInput->AppendTakedownEventQueue(
            reinterpret_cast<const BrnWorldIO::TakedownEventQueue*>(
                lpGameStateOutput->GetTakedownEventOutputQueue()));

        // 3/4. the active-payback pair (0x823B9E28 / 0x823B9ED8 -> 0x823B4F50 / 0x823B5000)
        lpWorldInput->SetActivePaybackType(lpGameStateOutput->GetActivePaybackType());
        lpWorldInput->SetActivePaybackAggressor(lpGameStateOutput->GetActivePaybackAggressor());

        // 5. the trigger-management interface (0x823B9AE0 -> 0x823DB778). Both sides are the
        //    committed BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface now.
        lpWorldInput->AppendTriggerManagementInputInterface(
            lpGameStateOutput->GetTriggerManagementInputInterface());

        // 6. the trigger-query interface (sub_823B9B88 -> 0x823C8CF0). Both sides are the same
        //    VariableEventQueue<4096,16> instantiation.
        lpWorldInput->AppendTriggerQueryInputInterface(
            lpGameStateOutput->GetTriggerQueryInputInterface());

        // 7. the race-car race-distance interface (sub_823BA038 -> 0x823B4A58).
        //    FLAG cross-home cast: BrnWorldIO models this payload as its own 10-word slice.
        lpWorldInput->SetRaceCarRaceDistanceInterface(
            reinterpret_cast<const BrnWorldIO::RaceCarRaceDistanceInterface*>(
                lpGameStateOutput->GetRaceCarRaceDistanceInterface()));

        // 8/9. the two scoring snapshots (INLINED on X360 -> 0x823B4B28 / 0x823B4BE0).
        //      FLAG cross-home casts; see the ⚠️ in the banner for the 2736-vs-2672 span.
        lpWorldInput->SetScoringInterface(
            reinterpret_cast<const BrnWorldIO::ScoringInterface*>(
                lpGameStateOutput->GetScoringOutputInterface()));
        lpWorldInput->SetOnlineScoringInterface(
            reinterpret_cast<const BrnWorldIO::OnlineScoringInterface*>(
                lpGameStateOutput->GetOnlineScoringOutputInterface()));

        // 10. the controller-active flag (0x823B9F88 -> 0x823B4C98)
        lpWorldInput->SetControllerActive(lpGameStateOutput->GetControllerActive());
    }

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
        // [FLAG PC bring-up] Zero the staged region. The IO stack hands back RE-USED memory
        // and CreateIOBuffer<T> does not zero it (neither does the console), so every
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
        // RE-Construct after the bring-up memset; the template already Constructed once.
        // CreateIOBuffer<T> above runs T::Construct itself now (2026-08-15), so this is the
        // SECOND Construct of this buffer -- and it is deliberate, because the memset in between
        // wiped what the first one wrote. It re-seeds the -1 sentinels: the buffer's OWN
        // Construct @0x822393D0 (not just the IOBuffer base) sets mePlayerCarIndex and
        // miCameraType to -1, and without the re-run the zero-fill would publish camera-type 0
        // as a live request every single frame. Delete this line together with the memset above,
        // not before it.
        lpDirectorInput->Construct();

        // ⭐ THE FRAME TIMESTEP. The console's module scheduler runs BridgeTimers @0x823BD150
        // over this exact pair (game module -> director input) before the director's passes;
        // it is the ONLY producer of the director input's timer status, and every camera
        // behaviour's per-frame advance is `<speed> * GetCurrentTimeStep()`. Staged on the
        // pre-GUI call, matching the console's bridge order (the post-GUI pass consumes the
        // same buffer contents).
        if (!lbPostGui)
            BridgeTimers(lpDirectorInput);

        // ⭐ THE GUI -> DIRECTOR BRIDGE. X360 DoUpdate_DirectorPostGUI @0x823DCE38 runs it on
        // the POST-GUI pass, bracketed by the same write lock, immediately before
        // DirectorModule::PostGuiUpdate consumes what it published. This is the only path by
        // which BrnGui::Intro's fly-by START/END commands (477 / 478) reach the director.
        if (lbPostGui)
        {
            lpDirectorInput->LockForWrite();
            BridgeGuiToDirector(lpDirectorInput, mGuiModule.GetGuiOutQueue());
            lpDirectorInput->UnlockForWrite();
        }

        // ⭐⭐ THE WORLD -> DIRECTOR BRIDGE (X360 BridgeWorldToDirector @0x823E3AB0, run by
        // DoUpdate_Director on the PRE-GUI pass, right after BridgeControllerToDirector).
        // This is the ONLY caller of DirectorIO::InputBuffer::SetRaceCarInfo in the image --
        // it is what gives the director's cameras a REAL per-car VehicleInfo instead of the
        // zero-filled one every VehicleRef used to resolve to. The world output buffer is the
        // one the world leg (DriveWorldUpdateFrame, run earlier in this same sub-step by the
        // flow state) published into; see GetWorldUpdateOutputBuffer().
        //
        // ⛔ THE FAKE PLAYER CAR IS RETIRED. This block used to call SetPlayerCarIndex(0) +
        // SetRaceCarInUse(0, true) on a car whose VehicleInfo was ZEROED, purely so
        // MainDirector::GetLivePlayerCarIndex() != -1 and the attract-mode stand-in would
        // tick. VehicleRef::IsValid passed on it and any ICE-anim take framed against it was
        // an ORIGIN camera. Both fields now come from the real chain:
        //   RaceCarEntityModule::UpdateOutputInterfaces  (publishes the active-car interface)
        //     -> WorldModule::BridgeRaceCarEntityInfoToOutput_PostPhysics  (into the world out)
        //       -> BridgeWorldToDirector                                   (into the director)
        // If no player car is active the bridge publishes index -1, which is the console's own
        // "no live player car" answer -- an honest fail instead of a silently wrong camera.
        if (!lbPostGui && mpWorldUpdateOutputBuffer != 0)
        {
            mpWorldUpdateOutputBuffer->LockForRead();
            lpDirectorInput->LockForWrite();
            BridgeWorldToDirector(lpDirectorInput, mpWorldUpdateOutputBuffer);
            lpDirectorInput->UnlockForWrite();
            mpWorldUpdateOutputBuffer->UnlockForRead();
        }

        // ⭐⭐ THE GAME-STATE -> DIRECTOR BRIDGE (X360 BridgeGameStateToDirector @0x823CD170,
        // run by DoUpdate_Director on the PRE-GUI pass). Its Append is the ONLY producer of
        // the game actions MainDirector::ProcessInputQueue drains -- the only route by which
        // the junkyard / car-select ladder can ever start. All three links are real now:
        //   producer  GameStateModuleIO::OutputBuffer::GetGameActionQueue()  (X360 0x823B96F0)
        //   bridge    this call
        //   consumer  MainDirector::ProcessInputQueue                        (cases 62..85)
        // ⛔ WHAT IS STILL MISSING is a PRODUCER THAT POSTS: CarSelectManager (the TU that
        // posts entry action 73) is not mounted yet, so this queue is empty every frame today.
        // See the measured cost at the top of BrnCarSelectManager.cpp.
        if (!lbPostGui)
        {
            BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput =
                mGameStateModule.GetOutputBuffer();
            // ⭐ RUNTIME-PROVEN 2026-08-01 (PROD_RUN1, 140 s, 0 asserts): a temporary probe that
            // posted game action 73 (E_ACTION_CAR_SELECT_TRANSITION_IN, payload {1,0}) onto
            // lpGameStateOutput->GetGameActionQueue() at frame 120 -- i.e. onto the CONSOLE'S OWN
            // producer surface, not forged into the director -- drove the director's
            // meJunkyardState 0 -> 2 (E_JY_INTRO_NO_CARS) and held it for 388 trace samples.
            // The probe was removed; the path it exercised is the code below.
            if (lpGameStateOutput != 0)
            {
                lpGameStateOutput->LockForRead();
                lpDirectorInput->LockForWrite();
                BridgeGameStateToDirector(lpDirectorInput, lpGameStateOutput);
                lpDirectorInput->UnlockForWrite();
                lpGameStateOutput->UnlockForRead();
            }
        }

        // ------------------------------------------------------------------------------
        // ⛔ THE INTRO FLY-BY STAND-IN IS RETIRED (2026-08-01).
        //
        // What used to be here: the GUI's fly-by request (477) was intercepted and turned into
        // `Arbitrator::SetDoAttractMode(true)`, running ArbStateAttractMode's BehaviourRoadRunner
        // -- a real console camera, but the DEBUG one. The X360 image's only writer of
        // mbDoAttractMode besides Arbitrator::Construct is the debug-menu entry "Do attract mode"
        // (DebugComponent::OnActivate @0x82275F68), so that camera never ships.
        //
        // Its DELETE-WHEN ("ArbStateCarSelect + BehaviourIceAnim + the game-intro shot group are
        // real") is met, and the last link -- something that actually makes the director's
        // meJunkyardState non-zero so the roaming ladder hands over to ArbStateCarSelect -- landed
        // with GameStateModule::ProcessGameEventsReallyEnterJunkyardBringUp. The console's own
        // chain now runs end to end:
        //   BridgeGuiToDirector 477 -> InputBuffer::mbStartGameIntroFlyby
        //   MainDirector::PostGuiUpdate  -> GameState::mbGameIntroFlybyActive = true
        //   ArbStateCarSelect::Update    -> the authored ICE camera movies from the "game intro"
        //                                   shot group ("606002")
        //
        // ⚠️ WHAT SURVIVES, AND WHY. mbDirectorCameraLive is NOT part of the stand-in: it is the
        // ONLY gate on DoDispatch's director->world camera handover (see DoDispatch). Deleting it
        // with the rest would have taken the director's camera off the world entirely -- fly-by
        // included -- so it stays, gated onto the states in which the director is actually
        // placing a camera rather than re-publishing a stale one.
        // [FLAG PC bring-up] the GATE is the deviation, not the camera. DELETE-WHEN DoDispatch's
        // IO buffer set is real (the console routes the director camera unconditionally).
        //
        // ⭐⭐ WIDENED TO COVER DRIVING (2026-08-02, drive-handover wave) -- MEASURED, NOT
        // ASSUMED. The two original terms were the fly-by request and
        // `meJunkyardState != E_JY_INACTIVE`. The junkyard exit CLEARS meJunkyardState, so the
        // gate closed on the exact frame the car was handed to the player: the director camera
        // was live for the car-select screen and dead for driving, precisely inverted. Measured
        // on the committed build (`DH_TRACE1`, plain + BRN_DIRECTOR_TRACE, 200 s):
        //     f5160  arb 2  jy 0   eye (3010.878, -2.599, -1946.891)  fov 72.605   <- exiting
        //     f5340  arb 2  jy 0   eye (3007.983, -2.310, -1939.326)  fov 90.395   <- chase cam
        //     ... held to f11040, because nothing is driving the parked car ...
        //     line 1311 of the same run: "world camera -> bring-up tour camera (flyby=0 junkyard=0)"
        // fov 90.395088 is BehaviourGameplayExternal::Update's own signature value
        // (2*atan(tan(80/2 deg) * 1.2) in degrees, the authored 80 widened by the boost/speed
        // term), so the thing publishing post-handover IS the chase camera, at a real city
        // position, not a stale latch left by the car-select shot.
        //
        // The added term is the ARBITRATOR's own outer state. E_STATE_NORMAL is the state in
        // which Arbitrator::Update dispatches a live behaviour stack and copies its produced
        // camera into lrCameraInOut every frame; in E_STATE_PREPARE (the boot value, trace f0)
        // it publishes Camera::Construct's identity at the origin. That is exactly the
        // "is the director placing a camera?" question this gate is asking, and it is a
        // strictly WIDER condition than the two it joins (both measured with arb == 2), so it
        // cannot close a window that used to be open.
        // ⚠️ The origin guard in DoDispatch stays load-bearing regardless -- E_STATE_NORMAL is
        // reached before the first behaviour actually places a camera.
        // ------------------------------------------------------------------------------
        if (!lbPostGui)
        {
            const BrnDirector::MainDirector& lrMainDirector = mDirectorModule.GetMainDirector();
            const bool lbFlybyRequested = lrMainDirector.IsGameIntroFlybyActive();
            const bool lbJunkyardActive =
                (lrMainDirector.GetGameState().meJunkyardState
                    != BrnDirector::GameState::E_JY_INACTIVE);
            const bool lbArbitratorRunning =
                (lrMainDirector.GetArbitrator().GetState()
                    == BrnDirector::Arbitrator::E_STATE_NORMAL);
            const bool lbDriving = lbFlybyRequested || lbJunkyardActive || lbArbitratorRunning;

            if (mbDirectorCameraLive != lbDriving)
            {
                mbDirectorCameraLive = lbDriving;
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[FLAG PC bring-up] world camera "
                        << (lbDriving ? "-> DIRECTOR" : "-> bring-up tour camera")
                        << " (flyby=" << (lbFlybyRequested ? 1 : 0)
                        << " junkyard=" << (lbJunkyardActive ? 1 : 0)
                        << " arbNormal=" << (lbArbitratorRunning ? 1 : 0) << ")\n";
                }
            }
        }

        if (!lbPostGui)
        {
            BrnDirector::DirectorIO::SceneQueryOutputBuffer* lpSceneQueryOutput = 0;
            BrnDirector::DirectorIO::SceneQueryInputBuffer*  lpSceneQueryInput  = 0;
            mpUpdateOutputBufferStack->CreateIOBuffer(&lpSceneQueryOutput, "DirectorSceneQuery");
            mpUpdateInputBufferStack->CreateIOBuffer(&lpSceneQueryInput, "DirectorSceneQuery");
            if (lpSceneQueryOutput != 0 && lpSceneQueryInput != 0)
            {

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
                    // ⭐ ALSO report the junkyard / car-select sub-state. It is the gate on the
                    // whole car-select ladder (ArbStateRoaming -> E_STATE_CAR_SELECT), and
                    // until MainDirector::ProcessInputQueue landed it could never be anything
                    // but 0 -- so "did the game-action seam actually run?" is exactly this
                    // number. EJunkyardState: 0 INACTIVE / 1 INTRO_UNLOCKING_CARS /
                    // 2 INTRO_NO_CARS / 3 CAR_UNLOCK / 4 CAR_SELECT / 5 WAITING_FOR_AUDIO.
                    const BrnDirector::GameState& lrGameState =
                        mDirectorModule.GetMainDirector().GetGameState();
                    *CgsDev::Log::gpDebugPrint
                        << "[director] f" << siTraceFrame
                        << " dt " << (mSimTimer.GetRate() * mSimTimer.GetScaleCurrent())
                        << " arb " << static_cast<s32>(lrArbitrator.GetState())
                        << " jy " << static_cast<s32>(lrGameState.meJunkyardState)
                        // ⭐ ADDED 2026-08-01 (ArbStateRoaming::Update wave). The roaming
                        // ladder in ProcessPossibleStateChanges STOPS at the
                        // `GetCurrentEventState() == E_EVENT_STATE_PRE_INTRO(0)` arm and never
                        // reaches the junkyard test below it, so "jy" alone cannot say whether
                        // the car-select transition was even eligible. These two are the gate:
                        // meEventType (-1 == free roam) and the event-state journal head
                        // (0 PRE_INTRO / 1 INTRO / 2 COUNTDOWN / 3 ACTIVE / 4 POST_EVENT).
                        << " evtype " << static_cast<s32>(lrGameState.meEventType)
                        << " evst " << static_cast<s32>(lrGameState.GetCurrentEventState())
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
        // ---- stage the DIRECTOR's published camera for the world ------------------------
        // The console does this through the renderer/world dispatch IO buffer set:
        // DoDispatch fills RendererIO::InputBuffer's camera from the director output,
        // BridgeRendererToWorld @0x823CDD20 hands it on, and the real
        // WorldModule::GenerateDispatchLists latches it (`mLastCameraInput = *lpCameraInput`).
        // None of those four buffers is created on this build, so the camera is handed over
        // directly to the world module's bring-up producer instead.
        //
        // ⚠️ THE ORIGIN GUARD IS LOAD-BEARING, not defensive dressing. Until the director's
        // arbitrator reaches a behaviour that actually places a camera, the published camera
        // is Camera::Construct's default -- identity basis at (0,0,0). The world streamer
        // takes the published eye as its PVS query point, and (0,0,0) is off the ground-plane
        // zone map, so publishing it unloads every resident track unit and the city goes
        // black. This campaign has hit that four times. So: only route a camera that is
        // somewhere real.
        // DELETE-WHEN: DoDispatch's IO buffer set is real (then this whole staging goes with
        // GenerateDispatchListsBringUp).
        // ⭐ ...and only while the DIRECTOR is the thing driving it (mbDirectorCameraLive, set
        // in DoUpdate_Director for the frames in which the arbitrator is actually running a
        // behaviour stack -- see the widening banner there). Outside that window the director
        // publishes whatever its last state left -- a STATIC camera -- and routing that would
        // freeze the world and stop the streamer turning over. Handing the world back to the
        // tour camera keeps it alive over boot and loading.
        // ⚠️ "The published eye did not change this frame" is NOT the same as "the camera is
        // stale": with the chase camera live and the car parked, holding still is the correct
        // output. Distinguish the two by what the arbitrator is doing, never by the value.
        // ---- stage the DIRECTOR's published camera as the EFFECTS CAMERA INPUT ------------
        // The console: `SetCameraInput(effectsDispatchInput, GetCameraOutput(directorOutput))`
        // (DoDispatch @0x823DC458 line 103, the ONLY caller of SetCameraInput @0x823C9988).
        // The record is what BrnEffects::EffectsModule::GenerateRenderRequests @0x8227FF10
        // reads to decide, per frame, whether depth-of-field / B4 blur / motion blur are on
        // and with what amounts. The EffectsIO dispatch buffer does not exist on this build,
        // so the renderer's base-frame bring-up producer takes the copy directly.
        // UNCONDITIONAL, like the console's call: the mbDirectorCameraLive / origin guards on
        // the world hand-over below are world-STREAMER safety, and applying them here would
        // freeze the effects record instead of letting the director turn effects off.
        // DELETE-WHEN DoDispatch's IO buffer set is real (with the two hand-overs below).
        if (mpDirectorOutputBuffer != 0)
        {
            mpDirectorOutputBuffer->LockForRead();
            mRenderModule.PCBringUpSetCameraInput(mpDirectorOutputBuffer->GetCameraOutput());
            mpDirectorOutputBuffer->UnlockForRead();
        }

        if (mpDirectorOutputBuffer != 0 && mbDirectorCameraLive)
        {
            mpDirectorOutputBuffer->LockForRead();
            const BrnDirector::Camera::Camera* lpCamera = mpDirectorOutputBuffer->GetCameraOutput();
            if (lpCamera != 0)
            {
                const rw::math::vpu::Matrix44Affine& lrXform = lpCamera->GetTransform();
                const f32 lfDistSq = lrXform.wAxis.x * lrXform.wAxis.x
                                   + lrXform.wAxis.y * lrXform.wAxis.y
                                   + lrXform.wAxis.z * lrXform.wAxis.z;
                if (lfDistSq > 1.0f)
                {
                    mWorldModule.SetBringUpCameraOverride(lrXform, lpCamera->GetFOV());

                    static bool sbLoggedHandover = false;
                    if (!sbLoggedHandover && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbLoggedHandover = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[FLAG PC bring-up] world camera HANDED OVER to the director: eye ("
                            << lrXform.wAxis.x << ", " << lrXform.wAxis.y << ", "
                            << lrXform.wAxis.z << ") fov " << lpCamera->GetFOV() << "\n";
                    }
                }
            }
            mpDirectorOutputBuffer->UnlockForRead();
        }

        // ---- stage the RENDERER's four WORLD-layer effects frames for the world -----------
        // The console does this through the same dispatch IO buffer set as the camera above:
        // BrnRendererModule::Update @0x82405E28 line 110 publishes
        // mEffectsArbitrator.GetExternalEffectsFrame(KU_EFFECTS_LAYER_WORLD, luSlot) into
        // RendererIO::OutputBuffer, BridgeRendererToWorld @0x823CDD20 copies all four across
        // with BrnWorldIO::DispatchInputBuffer::SetEffectsFrame @0x823B6BD8
        // (GameBridgeRendererToX.cpp:50), and WorldModule::GenerateDispatchLists reads them
        // back as GetEffectsFrame(0..3) for EnvironmentManager::GenerateEffects @0x827BE698.
        // FOUR slots: kau8SlotsPerEffectsLayer[KU_EFFECTS_LAYER_WORLD] == 4 (byte_8203E110 =
        // 01 04 02). The accessor returns null until the renderer's arbitrator is
        // Constructed; the world producer checks all four before it calls GenerateEffects.
        // DELETE-WHEN: DoDispatch's IO buffer set is real (this goes with the camera staging
        // and GenerateDispatchListsBringUp).
        mWorldModule.SetBringUpEffectsFrames(mRenderModule.GetWorldEffectsFrameBringUp(0),
                                             mRenderModule.GetWorldEffectsFrameBringUp(1),
                                             mRenderModule.GetWorldEffectsFrameBringUp(2),
                                             mRenderModule.GetWorldEffectsFrameBringUp(3));

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
        {
            meGamePrepareStage = E_GAMEPREPARESTAGE_WAITACQUIRES;

            // Stage 3: resolve the five global textures and hand them to the renderer.
            //
            // The five gamedb paths below are the X360's own literals (GamePrepare
            // @0x823EFBD0, BrnGameModule.cpp:1751-1755) and every one of them hashes to an
            // entry that IS present in the converted platform-4 GlobalTextureDictionary.bin
            // -- the bundle stage 2 above just finished loading into pool 10.
            //
            // FLAG PC-platform leaf: the console posts five ASYNC AcquireResource events
            // (ids 0..4) into the GameData request queue and collects the replies here on a
            // later frame. Its responder, PoolModule::DoAcquireResourceRequest @0x828FCD48,
            // is exactly `GetPool(poolId)->FindResource(id, checkRefCount, statusMask 2)`
            // plus the entry's main-memory slot -- so the resolve below calls that same pair
            // directly. The bundle is already resident (stage 2 waited for it), so there is
            // nothing to wait for and the round trip would only add a frame. Replace this
            // with the real acquire events when the GameData RequestInterface grows its
            // AcquireResource builder.
            {
                static const char* const KAPC_GLOBAL_TEXTURE_PATHS[5] =
                {
                    "gamedb://burnout5/Playground/GlobalTextures/blobbyshadow.TextureConfig2d?ID=240931",
                    "gamedb://burnout5/Playground/GlobalTextures/cloud1density.TextureConfig2d?ID=381388",
                    "gamedb://burnout5/Playground/GlobalTextures/cloud1quadrant.TextureConfig2d?ID=381382",
                    "gamedb://burnout5/Playground/GlobalTextures/corona_atlas.TextureConfig2d?ID=297312",
                    "gamedb://burnout5/Playground/GlobalTextures/glass_fracture.TextureConfig2d?ID=440917"
                };
                const s32 KI_GLOBAL_TEXTURE_POOL = 10;   // the same pool stage 2 loaded into

                renderengine::Texture* lapTextures[5] = { 0, 0, 0, 0, 0 };
                CgsResource::Pool* lpPool =
                    mGameDataModule.GetResourceModule().GetPoolModule().GetPool(KI_GLOBAL_TEXTURE_POOL);
                if (lpPool != 0)
                {
                    for (s32 liTexture = 0; liTexture < 5; ++liTexture)
                    {
                        // The bundle keys every resource by CgsResource::ID::HashString of the
                        // full lowercased gamedb path (a reflected CRC32, @0x828D84A8), widened
                        // into the 64-bit id slot.
                        CgsResource::ID lId;
                        lId.SetHash(static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(
                            reinterpret_cast<const u8*>(KAPC_GLOBAL_TEXTURE_PATHS[liTexture])))));
                        s32 liIndex = -1;
                        CgsResource::Entry* lpEntry = lpPool->FindResource(lId, false, 2, &liIndex);
                        if (lpEntry != 0)
                        {
                            lapTextures[liTexture] = static_cast<renderengine::Texture*>(
                                lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY]);
                        }
                    }
                }

                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[GamePrepare] global textures: blobby=" << (lapTextures[0] != 0)
                        << " cloudDensity=" << (lapTextures[1] != 0)
                        << " cloudLighting=" << (lapTextures[2] != 0)
                        << " corona=" << (lapTextures[3] != 0)
                        << " glassFracture=" << (lapTextures[4] != 0) << "\n";
                }

                // X360 BrnGameModule.cpp:1751-1755 -- the argument ORDER is the console's.
                mRenderModule.PrepareAgain(lapTextures[0], lapTextures[1], lapTextures[2],
                                           lapTextures[3], lapTextures[4]);
            }
        }
        // fall through

        case E_GAMEPREPARESTAGE_GAMESTATE:
        {
            // ⭐ REAL since 2026-08-01 (was `[gated]`, which is why TRIGGERS.DAT never loaded).
            // X360 0x823EFBD0, LABEL_31:
            //     CreateIOBuffer<GameStateModuleIO::OutputBuffer>(mpUpdateOutputBufferStack,
            //                                                     &out, "GameState");
            //     prepared = (*(vtable+64))(&mGameStateModule, out, mpUpdateOutputBufferStack,
            //                               GameDataOutputBuffer::GetAllocatorList(gameDataOut));
            //     if (prepared) { DestroyIOBuffer(out); unlock both; return 1; }
            //     else { LockForRead(out);
            //            gameDataIn->AppendRequestInterface<3072>(*out->GetResourceRequestInterface());
            //            UnlockForRead(out); DestroyIOBuffer(out); }
            //
            // ⚠️ FLAG (PC deviation, ONE buffer instead of a per-pass scratch): the console
            // carves a FRESH GameStateModuleIO::OutputBuffer off the update-output IOBufferStack
            // on every pass, so its request queue starts empty each time and the append moves
            // exactly that pass's requests. The PC module owns ONE persistent buffer (see
            // GameStateModule::GetOutputBuffer's own FLAG -- it is newed in Construct because
            // nothing on PC ran the module's Prepare, which is the console's allocation point).
            // Re-appending a persistent queue every pass would re-issue every request already
            // sent, so the queue is CLEARED after the append -- which is exactly the state the
            // console's next pass starts in. DELETE-WHEN the module's real
            // CreateOutputDataStructure path lands and the buffer becomes per-pass again.
            meGamePrepareStage = E_GAMEPREPARESTAGE_GAMESTATE;

            BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput =
                mGameStateModule.GetOutputBuffer();

            const bool lbGameStatePrepared = mGameStateModule.Prepare(
                lpGameStateOutput, mpUpdateOutputBufferStack,
                lpGameDataOutput ? lpGameDataOutput->GetAllocatorList() : 0);

            if (!lbGameStatePrepared)
            {
                if (lpGameStateOutput != 0)
                {
                    // ⚠️ The CONST overload of GetResourceRequestInterface is the one the console
                    // calls here (X360 `OutputBu` @0x823B9798, "Not locked for reading" line 268);
                    // its non-const twin @0x8231D560 asserts the WRITE lock and would fire under
                    // this read lock. Same const-alias idiom as LoadDirectorModule's
                    // GetVaultRequestInterface read.
                    const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputRead =
                        lpGameStateOutput;
                    lpGameStateOutput->LockForRead();
                    lpGameDataInput->AppendRequestInterface<3072>(
                        *lpGameStateOutputRead->GetResourceRequestInterface());
                    lpGameStateOutput->UnlockForRead();

                    // [PC deviation, see the FLAG above] the console's per-pass buffer dies here.
                    lpGameStateOutput->LockForWrite();
                    lpGameStateOutput->GetResourceRequestInterface()->mRequestQueue.Clear();
                    lpGameStateOutput->UnlockForWrite();
                }
                break;   // still preparing -- the tail below pumps the GameData module
            }

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

    // @ 0x823BC9B8 (BrnGameModule.cpp:1533) - the resource-update worker-thread body. The X360
    // body is three statements:
    //
    //     mbResourceUpdateBusy = ( (*(*(this + 6243520) + 68))(   // GameDataModule::Update
    //                                  this + 6243520,
    //                                  *(this + 10053832),        // mpUpdateInputBufferStack
    //                                  *(this + 10053836),        // mpUpdateOutputBufferStack
    //                                  *(this + 10053840),        // the GameData INPUT buffer
    //                                  *(this + 10053844) )       // the GameData OUTPUT buffer
    //                            == 1 );
    //     CheckDiskError( this, *(this + 10053844) );
    //
    // (the thread's `this` is the game module + 1600 -- hence the `a1 - 1600` in the
    // CheckDiskError call and the 1600-byte-shifted member offsets; add it back and the four
    // arguments are exactly gm+10055432/36/40/44, i.e. the two update IO buffer stacks and THE
    // SAME GameData input/output pair BrnGameModule::Prepare stage 3 created and every
    // LoadXxxModule / GamePrepare / LoadingScriptedState brackets. There is no second pair and
    // no hand-off: the console synchronises the two threads purely with the IOBuffer
    // read/write locks.)
    //
    // ⭐ THIS IS THE PUMP. Nothing else services the GameData request queue: the requests every
    // module stages into the input buffer are drained, dispatched and answered here. The X360
    // runs it continuously on its own thread from BrnGameModule::Prepare onwards, which is why
    // a module's Prepare can stage a request at boot loading stage 3 and see the reply a frame
    // later.
    //
    // FLAG PC-platform leaf: the single-threaded PC host has no resource thread, so GameMain
    // calls this once per simulation sub-step, right after the flow state's Update (i.e. after
    // this sub-step's requests are staged and their IO locks released). Same work, same
    // buffers, same order -- serialised instead of concurrent.
    //
    // [deferred] CheckDiskError: the console's dirty-disc watch reads the GameData output's
    // filesystem-status interface, which is not reconstructed (the output buffer's status
    // member is a documented deferral in BrnGameDataModuleIO.h).
    // ============================================================================================
    // FLAG GameState stand-in -- THE CAR-SELECT CAR LIST (GUI events 406 and 412).
    //
    // CONSOLE CHAIN, recovered end to end from ARTIST:
    //   BrnGameState::GameStateModule::ProcessGameEvents @0x823A0A18, case 88, calls
    //   GetListOfPlayerSelectableVehicles @0x82376500 into a stack CgsContainers::Array<s64,128>,
    //   fills the two BitArray<128> state words and the cars-unlocked count beside it, and posts
    //   the whole 0x430-byte record as GAME ACTION 184; the player's own car goes out the same
    //   way as game action 182. BrnGameModule::BridgeGameStateToGui @0x823EE880 ->
    //   TranslateGameActionsToGuiEvents @0x823E9CE0 then converts them:
    //     case 182 @0x823EBC10 -> AddGuiEvent<GuiPlayerInfoResponse>  (GUI event 406, 0x40 bytes)
    //     case 184 @0x823EBCA4 -> AddGuiEvent<GuiCarSelectionEvent>   (GUI event 412, 0x430 bytes)
    //   Event 412 is the ONLY writer of CarSelectVehicle::maSelectedCars / gsiNumCarouselCars, and
    //   406 is the only writer of CarSelectMain::mCurrentSetupInfo.mCarId before the player picks.
    //
    // BOTH PAYLOAD LAYOUTS ARE X360-ATTESTED, not guessed:
    //   406 @0x823D4828 queues id 0x196 / size 0x40, and the consumer reads the car id at +0x20
    //       (CarSelectMain's own `ld r11, 0x20(r30)`).
    //   412 @0x823D4998 queues id 0x19C / size 0x430; HandleCarInfoResponseEvent @0x824BEDC0 reads
    //       the count at payload +0x400 (`lwz r11, 0x400(r29)`) -- i.e. the record leads with the
    //       Array<s64,128> at +0, NOT with a 12-byte GuiEvent header. (⚠️ the opaque placeholder
    //       `GuiCarSelectionEvent : GuiEvent<412>` in BrnGuiDemangledEventTypes.h has the right
    //       SIZE and the wrong interior, so this posts the record directly rather than through
    //       AddGuiEvent<T> -- the same reason the GuiEventTimeInfo publish above bypasses it.)
    //
    // WHY A STAND-IN: neither producer exists on PC. BridgeGameStateToGui is a documented
    // deferral in GameBridgeGameStateToX.cpp (a ~300-case switch over un-homed action payloads),
    // and its action-184 source lives in ProcessGameEvents' case 88, itself unreconstructed.
    //
    // ⚠️ AND THE CONSOLE FILTER WOULD RETURN THE SAME ONE CAR HERE. GetListOfPlayerSelectableVehicles
    // admits a vehicle only if the player OWNS it: the offline arm needs Profile::FindCar(id) to
    // hit, the other needs ProgressionManager::IsCarUnlocked(id), and both scan the player
    // profile's owned-car list. On this build that list is EMPTY (GameStateModule::Prepare's
    // progression stages are `[deferred]`, and no save exists), so a faithful transcription of the
    // filter would publish ZERO cars. The one car the player demonstrably has is the one the
    // junkyard entry put them in -- CarSelectManager::EnterJunkyardAtStartOfGame hands it to
    // OnSpecialEventPlayerCarChange, which is what GetActivePlayerCarId reads back -- so that is
    // what goes out, as a one-entry list with both state BitArrays clear (never driven, never
    // wrecked: the same answer an empty profile gives the console) and cars-unlocked == 1.
    // DELETE-WHEN: the progression profile is populated at game start and the real
    // ProcessGameEvents/BridgeGameStateToGui producers land -- then transcribe the filter.
    //
    // WHEN IT FIRES: once per car-select entry, on the edge where the SCREEN flow's live state
    // subscribes to event 412 (its own RegisterForEvents record, which the module tracks). Posting
    // before that would drop the record on the subscription filter; posting every frame would
    // re-commit the player's selection under them, because event 406's handler overwrites
    // mCurrentSetupInfo/mDesiredSetupInfo.
    // ============================================================================================
    void BrnGameModule::PublishCarSelectionToGui()
    {
        const bool lbListening = mGuiModule.IsScreenFlowObserving(412);
        if (!lbListening)
        {
            mbCarSelectionPublished = false;   // re-arm for the next entry
            return;
        }
        if (mbCarSelectionPublished || mpGuiInputBuffer == 0)
            return;

        const CgsID lPlayerCarId = mGameStateModule.GetActivePlayerCarId();
        if (lPlayerCarId == 0)
            return;   // the junkyard entry has not committed a car yet; try again next sub-step

        mbCarSelectionPublished = true;

        // ---- GUI event 406 (GuiPlayerInfoResponse, 0x40 bytes; car id at +0x20) --------------
        {
            u8 laRecord[0x40];
            std::memset(laRecord, 0, sizeof(laRecord));
            *reinterpret_cast<CgsID*>(&laRecord[0x20]) = lPlayerCarId;
            mpGuiInputBuffer->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(laRecord), 406,
                static_cast<s32>(sizeof(laRecord)));
        }

        // ---- GUI event 412 (GuiCarSelectionEvent, 0x430 bytes) ------------------------------
        // +0x000 CgsContainers::Array<s64,128> ids   +0x400 its count
        // +0x408 BitArray<128> driven                +0x418 BitArray<128> wrecked
        // +0x428 cars-unlocked total
        {
            u8 laRecord[0x430];
            std::memset(laRecord, 0, sizeof(laRecord));
            reinterpret_cast<CgsID*>(&laRecord[0])[0] = lPlayerCarId;
            *reinterpret_cast<s32*>(&laRecord[0x400]) = 1;   // one selectable car
            *reinterpret_cast<s32*>(&laRecord[0x428]) = 1;   // cars unlocked total
            mpGuiInputBuffer->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(laRecord), 412,
                static_cast<s32>(sizeof(laRecord)));
        }

        // ---- GUI event 413 (the UNLOCKED-LIVERY list, 0x48 bytes) ---------------------------
        // +0x00 CgsContainers::Array<s64,8> ids   +0x40 its count
        //
        // ⭐ WITHOUT THIS THE CS_LIVERY SCREEN'S CAROUSEL IS EMPTY, and empty is not benign:
        // BrnGui::CarSelectLivery::HandleUnlockedLiveryResponseEvent is the ONLY writer of
        // muNumUnlockedLiveryCars, so with no producer SetupComponents calls
        // SetupTextSelection(0, ...) and every later GetHighlighted() runs on an empty
        // SelectableGroup -- which has no lower bound on miHighlightedIndex and has already
        // been observed returning the NON-NULL garbage 0x0000FF00, defeating the console's own
        // `!= 0` assert and access-violating downstream.
        //
        // FLAG STAND-IN, same shape and same reason as the 406 / 412 records above: the
        // console's producer answers a GUI request out of the player's PROFILE livery
        // ownership, which this build's (empty) profile car list cannot serve. The one car the
        // game state actually has is published as its own single unlocked livery -- which is
        // also what the Junkyard starter car genuinely is. Replace with the real
        // GameStateModule producer when the profile livery path lands. ⛔ Do NOT widen this
        // into a "every livery is unlocked" list; that would invent a rule the console
        // does not have.
        {
            u8 laRecord[0x48];
            std::memset(laRecord, 0, sizeof(laRecord));
            reinterpret_cast<CgsID*>(&laRecord[0])[0] = lPlayerCarId;
            *reinterpret_cast<s32*>(&laRecord[0x40]) = 1;   // one unlocked livery
            mpGuiInputBuffer->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(laRecord), 413,
                static_cast<s32>(sizeof(laRecord)));
        }

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[CarSelectBridge] published GUI 406 + 412 + 413 for car id "
                << static_cast<u32>(lPlayerCarId) << " (1 selectable car, 1 unlocked livery)\n";
        }
    }

    void BrnGameModule::ResourceUpdateThread(Mutex* /*lpMutex*/)
    {
        BrnResource::GameDataIO::InputBuffer*  lpGameDataInput =
            BrnGameMainFlowController::GetScriptedLoadGameDataInput();
        BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput =
            BrnGameMainFlowController::GetScriptedLoadGameDataOutput();

        // ---- X360 GuiModule::Prepare stage 14, driven from here -------------------------
        // BrnGui::WorldDataController::Prepare @0x82516770 stages its resource requests into
        // the GameData request interface and reads the replies out of its own receiver queue.
        // The console runs it from BrnGui::GuiModule::Prepare, which the module scheduler
        // calls with THIS SAME buffer pair; the PC's GuiModule::Prepare runs at Construct
        // time, long before the GameData IO exists, so the stage is driven here -- the one
        // place the pair is live and pumped -- immediately BEFORE the pump, so a request
        // staged this sub-step is serviced in the same call and its reply is waiting on the
        // next. (Same "stage then pump" order as GamePrepare's own stages.)
        // Stops re-entering once the machine reports done; while a stage is parked on a
        // request no PC producer answers it only re-checks its queue (it does not re-issue),
        // which is the console's own arm -- see the banner in BrnGuiWorldDataController.cpp.
        if (!mbWorldDataPrepared && lpGameDataInput != 0)
        {
            lpGameDataInput->LockForWrite();
            mbWorldDataPrepared = mGuiModule.PrepareWorldData(lpGameDataInput);
            lpGameDataInput->UnlockForWrite();

            // [diagnostic, one-shot] the keystone this whole car-select campaign turned on:
            // print the vehicle list the GUI side ended up holding, from the GUI side.
            static bool s_bLoggedGuiVehicleList = false;
            if (!s_bLoggedGuiVehicleList
                && mGuiModule.GetWorldDataController().GetVehicleList() != 0)
            {
                s_bLoggedGuiVehicleList = true;
                *CgsDev::Log::gpDebugPrint
                    << "[GuiWorldData] mpVehicleList = "
                    << const_cast<void*>(static_cast<const void*>(
                           mGuiModule.GetWorldDataController().GetVehicleList()))
                    << " (" << mGuiModule.GetWorldDataController().GetVehicleList()->GetVehicleCount()
                    << " entries), state "
                    << static_cast<s32>(mGuiModule.GetWorldDataController().GetState()) << "\n";
            }
        }

        mGameDataModule.Update(lpGameDataInput, lpGameDataOutput);
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
        // X360 0x823CB498 reads *(gm+10095356) * *(gm+10095360) here -- i.e. the SIM timer's
        // mfRate * mfScaleCurrent (Timer +0xC and +0x10), which is exactly its current time
        // step. Now that the timer pair is real this reads the real members; the
        // mfDebugUpdate* stand-ins it used to read are retired.
        mDebugManager.Update(mSimTimer.GetRate() * mSimTimer.GetScaleCurrent());
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

                // ⭐ THE GAME-STATE PRE-WORLD LEG (X360 DoUpdate_GameStatePreWorld @0x823EE0E8 ->
                // GameStateModule::PreWorldUpdate @0x823A5328). Only its ONE-SHOT start-of-game
                // latch is reconstructed (see PreWorldUpdateSetupPlayerCarBringUp); it runs
                // BEFORE the flow state's Update, because that is what drives the world and
                // therefore BridgeGameStateToWorld -- the leg that carries this sub-step's game
                // actions (including the ResetPlayerCarAction it posts) into the world. The
                // console's ordering is the same: PreWorldUpdate, then DoUpdate_World.
                //
                // ⚠️ MEASURED (2026-08-01): the flow-state gate below is LOAD-BEARING, not
                // defensive. GameStateModule::Prepare's terminal stage -- where the latch is
                // armed -- completes during InitialLoadingScreen stage 1, ~150 log lines BEFORE
                // WorldModule::Prepare stage 6 has even created the race-car module's slots. Fire
                // it there and the action is posted into a queue whose consumer does not exist
                // yet, and this sub-step's retiring Clear() throws it away: the first run of this
                // wave posted a perfectly correct ResetPlayerCarAction and NOTHING EVER SAW IT.
                // On the console the arming EVENT cannot occur before the game is running, so
                // this gate stands in for that ordering, not for the call.
                if (leState == BrnGameMainFlowController::E_MGS_IN_GAME)
                {
                    // ⭐ THE SECOND ARGUMENT IS THE ORDERING STAND-IN FOR GAME EVENT 78.
                    // The console completes the start-of-game junkyard entry when the GUI tells
                    // it to (BrnGui::InGame::OnEnter's command 145 -> BridgeGuiToGameState ->
                    // game event 78 -> ProcessGameEvents case 78). That bridge is not plumbed
                    // here, and its event provably arrives BEFORE the latch it tests exists on
                    // this build (measured: InGame::OnEnter logs ~40 lines ahead of
                    // SendSetupPlayerCarEvent), so the entry is completed on the GUI's OTHER
                    // first-boot signal instead: mbNewProfileIntroActive, which BrnGui::InGame's
                    // own first-boot gate raises (command 476, the same screen state that posts
                    // the 145) and which BridgeGuiToDirector + MainDirector::PostGuiUpdate carry
                    // into the director's GameState.
                    // ⚠️ IT IS NOT A DELAY FOR SAFETY'S SAKE -- it is the CORRECT arm selector.
                    // ArbStateCarSelect::Prepare reads exactly this flag to choose between the
                    // authored GAME-INTRO movies and the junkyard E_STATE_INTRO, and completing
                    // the entry before it is up sends the state down the wrong arm and then trips
                    // that arm's own `!mbGameIntroFlybyActive` tripwire once per frame.
                    // [FLAG PC bring-up] DELETE-WHEN BridgeGuiToGameState has a caller and
                    // ProcessGameEvents drains a real post-world input buffer.
                    const bool lbGuiFirstBootIntroLive =
                        mDirectorModule.GetMainDirector().IsNewProfileIntroActive();
                    mGameStateModule.PreWorldUpdateSetupPlayerCarBringUp(lbGuiFirstBootIntroLive);

                    // ⭐⭐ THE CAR-SELECT LEG (X360 PreWorldUpdate @0x823A5904..0x823A5958), in
                    // the console's own body order: the one-shot entry leg above, then the
                    // per-sub-step CarSelectManager tick. It is the whole image's only caller of
                    // CarSelectManager::Update, and therefore the only thing that ever ENDS the
                    // junkyard transition-in and moves GameState::meJunkyardState off
                    // E_JY_INTRO_NO_CARS. The gate lives inside (mJunkyardId != null), exactly
                    // as the console's does.
                    // ⚠️ THE TIMESTEP IS THE GAME TIMER'S, NOT THE SIM TIMER'S: the console
                    // latches `TimerStatusInterface::maEntries[0].mfValue04 * .mfValue08`
                    // @0x823A54D8, and StoreTimers writes entry 0 from mGameTimer. Read off the
                    // LIVE timer for the same reason the DJ/road-time leg below does -- nothing
                    // on this build stages a GameStateModuleIO::PreWorldInputBuffer.
                    // ⭐ THE GUI -> GAME-STATE CAR-SELECT LEG. BridgeGuiToGame's channel-40 walk
                    // latched a GUI out-event 192 (GuiEventActivateCarSelect) during the GUI
                    // phase; hand it to the extracted ProcessGameEvents case-94 arm here, where
                    // the game-state module owns its output buffer -- which is also where the
                    // console runs ProcessGameEvents (inside PreWorldUpdate @0x823A58B8). Serviced
                    // BEFORE the CarSelectManager tick so an ExitJunkyard lands in the same
                    // sub-step's Update, exactly as the console's body order does.
                    // [FLAG PC bring-up] DELETE-WHEN BridgeGuiToGameState has a caller.
                    if (mbCarSelectActivatePending)
                    {
                        mbCarSelectActivatePending = false;
                        mGameStateModule.ProcessGameEventsActivateCarSelectBringUp(
                            maiPendingCarSelectActivate[0], maiPendingCarSelectActivate[1]);
                    }

                    mGameStateModule.PreWorldUpdateCarSelectBringUp(
                        mGameTimer.GetRate() * mGameTimer.GetScaleCurrent());

                    // ⭐⭐ THE CONTROLLER-ACTIVE PUBLISH (X360 PreWorldUpdate @0x823A5328, the
                    // store just before the case-193 AddEvent). It MUST run before
                    // lpState->Update() below, because that is the leg that drives the world and
                    // therefore BridgeGameStateToWorld, which reads the flag back out at :1022 --
                    // the same reason the two legs above run here.
                    // ⚠️ WITHOUT IT NOTHING CAN DRIVE: ProcessPlayerVehicleInput zero-fills the
                    // whole BrnPlayerDriverControls record while the flag is false. See the
                    // header banner for the measurement.
                    // ⓘ NARROWER THAN THE CONSOLE, DELIBERATELY: the console publishes from
                    // PreWorldUpdate on every frame; this runs only inside E_MGS_IN_GAME, which
                    // is a strict subset (the flag's only consumer is the race-car module, which
                    // needs an attached player car anyway). It inherits the ordering gate the
                    // two legs above already stand behind rather than adding a second one.
                    mGameStateModule.PreWorldUpdatePublishControllerActiveBringUp();
                }

                if (leState != BrnGameMainFlowController::E_MGS_INVALID)
                {
                    MainGameFlowState* lpState = mMainFlowStateMachine.GetState(leState);
                    lpState->Update();
                }
                // ---- the RESOURCE tick (FLAG PC placement: the console's own thread) -------
                // The X360 runs BrnGameModule::ResourceUpdateThread @0x823BC9B8 concurrently
                // with this loop; the single-threaded host serialises it here, immediately
                // after the flow state has staged this sub-step's requests and released the
                // GameData IO locks. Without it NOTHING drains the GameData request queue
                // outside GamePrepare, and any module Prepare that waits on a resource reply
                // (DirectorModule stage 3 = WorldMap::LoadData) wedges the loading flow.
                ResourceUpdateThread(0);
                // ---- the TIMER tick (X360 BrnGameModule::UpdateTimers @0x823BCFD0) --------
                // Once per sim sub-step, before anything that reads a timestep. The console
                // runs it from the same per-sub-step spine.
                UpdateTimers();
                // ---- the DIRECTOR's pre-GUI passes (X360 module-scheduler order) ----------
                // PreSceneQueryUpdate + Update. The module publishes its finalised camera into
                // mpDirectorOutputBuffer, which stays alive through this frame's Render.
                DoUpdate_Director(false);

                // ⭐ RETIRE THIS SUB-STEP'S GAME ACTIONS (FLAG PC lifecycle, 2026-08-01).
                // Both consumers of the game-state output buffer's game-action queue have now
                // run: the WORLD leg (DriveWorldUpdateFrame -> BridgeGameStateToWorld, inside
                // lpState->Update() above) and the DIRECTOR leg (DoUpdate_Director(false) ->
                // BridgeGameStateToDirector). The post-GUI director pass does not read it.
                //
                // WHY THIS IS NEEDED. On the console the game-state module is a
                // CgsModule::ModuleSingleBuffered whose OUTPUT DataStructure is re-Constructed
                // by the module scheduler every frame, so its game-action queue starts EMPTY --
                // a posted action is ONE-SHOT. On the PC the module owns ONE persistent
                // OutputBuffer (BrnGameStateModule.cpp:68, itself a flagged bring-up seam), so
                // without this the queue only ever grows: every action would be re-delivered to
                // the world and the director on every sub-step for the rest of the session, and
                // the 13312-byte queue would eventually overflow its own AddEvent. Latent until
                // now (nothing posts yet), LIVE the moment CarSelectManager is ticked.
                // This is the same lifecycle the GUI out-queue Clear() further down handles for
                // the same reason, and the same one GamePrepare stage 4 already applies to this
                // very buffer's resource-request queue.
                // DELETE-WHEN the module's real Prepare/Swap DataStructure path lands.
                {
                    BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput =
                        mGameStateModule.GetOutputBuffer();
                    if (lpGameStateOutput != 0)
                    {
                        lpGameStateOutput->LockForWrite();
                        lpGameStateOutput->GetGameActionQueue()->Clear();
                        lpGameStateOutput->UnlockForWrite();
                    }
                }
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

                    // ⭐ THE GUI FRAME TIMESTEP. X360 BridgeGameStateToGui @0x823EE880 closes
                    // with AddGuiEvent<CgsGui::GuiEventTimeInfo> @0x823EF300, whose 8-byte
                    // payload the asm builds as
                    //     payload[0] = timerStatus[+8] * timerStatus[+4]
                    //                  (mfTimeStepMultiplier * mfBaseTimeStep,
                    //                   i.e. TimerStatus::GetCurrentTimeStep())
                    //     payload[1] = time.miSeconds + time.mfFraction
                    // MainGameFlowStateInitialLoadingScreen::Update @0x823EF9B0 and
                    // LoadingScriptedState::Update @0x823F2950 post the identical record while
                    // the boot loading screen is up. GuiCache latches it at its +0x00/+0x04
                    // and EVERY GUI-side timer reads it from there.
                    //
                    // This is the GAME timer (not the sim timer): the console reads the
                    // game-timer half of the published TimerStatus pair. Read here off the
                    // LIVE mGameTimer rather than off mTimerStatusInterface, because on this
                    // build the status pair is only published by BridgeTimers, which runs
                    // inside DoUpdate_Director and returns early until the director module
                    // reports prepared -- so the published half reads 0 for the whole boot
                    // and every GUI dwell would stay frozen. The two are the SAME VALUE by
                    // construction: TimerStatusInterface::StoreTimers @0x828D7518 copies
                    // mfBaseTimeStep <- Timer::mfRate and mfTimeStepMultiplier <-
                    // Timer::mfScaleCurrent, and TimerStatus::GetCurrentTimeStep() is their
                    // product; mTime is likewise Timer::miAccumTicks + Timer::mfAccumulator.
                    // UpdateTimers() has already ticked both timers earlier in this sub-step.
                    //
                    // ⚠️ NOT through CgsGui::GuiModule::AddGuiEvent<T>: that template assumes
                    // T derives from CgsGui::GuiEvent<N> and pushes (&event + 12) with size
                    // sizeof(T) - 12. GuiEventTimeInfo is a PLAIN 8-byte payload with no
                    // GuiEvent<N> base (its own GetEventType() carries the id, and the header
                    // pins sizeof == 8), so that arithmetic goes negative and the template
                    // takes its payload-less arm -- a 1-byte marker. The console's
                    // AddGuiEvent<GuiEventTimeInfo> @0x823D1468 pushes the WHOLE object:
                    // AddEvent(&event, 26, 8). Reproduced against the queue directly.
                    {
                        CgsGui::GuiEventTimeInfo lTimeInfo;
                        lTimeInfo.Set(mGameTimer.GetRate() * mGameTimer.GetScaleCurrent(),
                                      static_cast<f32>(mGameTimer.GetAccumTicks())
                                          + mGameTimer.GetAccumulator());
                        mpGuiInputBuffer->GetGuiEvents()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lTimeInfo),
                            lTimeInfo.GetEventType(),
                            static_cast<s32>(sizeof(lTimeInfo)));
                    }
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
                    // FLAG GameState stand-in (the live-progression handoff, event 350).
                    // CONSOLE CHAIN, recovered end to end from ARTIST:
                    //   BrnGameState::GameStateModule::PreWorldUpdate @0x823A5EA4 posts, on
                    //   EVERY pre-world update and unconditionally, game action 193 =
                    //   { &this->mProfile, the ProgressionData resource (or NULL), a flag }
                    //   (12 bytes) onto the module's GameActionQueue; then
                    //   BrnGameModule::BridgeGameStateToGui @0x823EE880 ->
                    //   TranslateGameActionsToGuiEvents @0x823E9CE0 case 193 @0x823EBBA4
                    //   copies those 12 bytes into a stack record and publishes them with
                    //   AddGuiEvent<GuiEventProgressionProfileData> @0x823EBBC8. That single
                    //   `bl` is the ONLY producer of GUI event 350 in the whole image.
                    // On arrival the console does two things: CgsGui::GuiModule::Update's
                    // event switch (case 209 == id 350, the switch is rebased by 141)
                    // @0x82528AB4 hands payload[0]/payload[1] to
                    // ProfileManager::SetProgressionProfile, and the registered flow states
                    // latch the profile pointer -- which is how BrnGui::InGame gets the
                    // mpProfile whose mbIsNewProfile byte gates "TO_INTRO".
                    // WHY A STAND-IN: none of that chain exists on PC. mGameStateModule is a
                    // placeholder (BrnGameModule.cpp:204), BrnGameModule::DoUpdate is a
                    // documented no-op, and GameBridgeGameStateToX.cpp explicitly defers
                    // BridgeGameStateToGui. The GUI module is therefore the only owner of a
                    // BrnProgression::Profile in the process -- BrnGuiModule::Prepare installs
                    // it through the very accessor the console's event-350 handler calls -- so
                    // this posts the SAME pair the console's action 193 carries, from the same
                    // point in the sub-step (inside the GUI-input write bracket, before the
                    // GUI drive), through the same AddGuiEvent<T> boundary. Replace it with
                    // the real translator when the GameState module lands.
                    {
                        BrnGui::GuiEventProgressionProfileData lProgressionProfile;
                        lProgressionProfile.mpProfile =
                            mGuiModule.GetProfileManager().GetProgressionProfile();
                        lProgressionProfile.mpProgressionData =
                            mGuiModule.GetProfileManager().GetProgressionData();
                        // The console's flag byte ORs Profile::muMedalCountFromTheStart >= 4
                        // with two game-state-module words that have no PC equivalent; the
                        // medal-count half is the readable one (it is the same test
                        // ProgressionManager::AreRoadRulesAvailable makes) and no
                        // reconstructed consumer reads the byte yet.
                        lProgressionProfile.mbRoadRulesAvailable =
                            lProgressionProfile.mpProfile != 0
                            && lProgressionProfile.mpProfile->GetMedalCountFromTheStart() >= 4u;
                        if (lProgressionProfile.mpProfile != 0)
                        {
                            // The three steps of AddGuiEvent<GuiEventProgressionProfileData>
                            // @0x823D4548, inlined: the X360 emits it as a NON-static member of
                            // the CgsGui::GuiModule the game module holds at +7252512, but the
                            // body never reads `this` (see CgsGuiModule.h), and the PC game
                            // module has no such embed to call it through.
                            mpGuiInputBuffer->GetGuiEvents()->AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lProgressionProfile),
                                lProgressionProfile.GetEventType(),
                                static_cast<s32>(sizeof(lProgressionProfile)));
                        }
                    }
                    // FLAG GameState stand-in (THE CAR-SELECT CAR LIST -- GUI events 406 + 412).
                    PublishCarSelectionToGui();
                    // FLAG sound stand-in (the VOICE-OVER round trip, GUI events 466/467).
                    // CONSOLE CHAIN: the GUI posts a voice-over REQUEST as out-event 466
                    // carrying a CgsSound::Playback::Name::MakeHash id; BridgeGuiToSound
                    // @0x823C0A58 hands the GUI out-queue to the sound root input buffer,
                    // and the sound side reports back through the module's
                    // PreUpdateOutput GuiOutEventQueue -- 466 when the line STARTS, 467
                    // when it FINISHES. BrnGui::Intro subscribes to both and gates every
                    // one of its timed transitions on them: 466 sets mbVoiceOverPlaying
                    // (which freezes mfPauseTimer) and 467 clears it AND RESETS
                    // mfPauseTimer to 0 -- 467 is the ONLY writer of that reset outside
                    // Intro::OnEnter (image-wide scan of stores to Intro+0xDEC).
                    // WHY A STAND-IN: neither leg exists on PC. The sound module's GUI
                    // drain (BrnSound::Logic::SoundLogicModule::ProcessGuiEvents
                    // @0x826ED6C8) is unreconstructed and so is the return leg
                    // (BridgeSoundToGuiPreUpdate -- see the banner in
                    // GameBridgeSoundToX.cpp). With NOBODY answering, mfPauseTimer is
                    // never reset after the WELCOME-TEXT dwell has already run it up to
                    // KF_INTRO_TRANSITION_PAUSE, so the LICENCE state's own 2 s dwell is
                    // zero and the driver licence is on screen for a single frame.
                    // WHAT THIS POSTS: the console's own answer for a voice-over whose
                    // sample never plays -- started, then immediately finished, in the
                    // same sub-step. No duration is invented; the GUI simply gets its
                    // dwell floor back. Replace with the real bridge when the sound
                    // module's GUI legs land.
                    // WHAT THIS DOES NOW: the request's name hash is handed to the PC
                    // speech player (CgsSystem::SpeechAudioPC -- the SpeechEffect leaf),
                    // which resolves it to its stream and sounds it on the audio output's
                    // dedicated voice slot. 466 goes back as soon as the line starts and
                    // 467 only when it has PLAYED OUT, which is the console's own timing
                    // (SpeechEffect::UpdateParams @0x826F8074 posts 467 off the stream's
                    // end state) -- so BrnGui::Intro's mfPauseTimer now holds for the real
                    // length of the line instead of being released in the same sub-step.
                    // When the line cannot be sounded (no mapping, missing stream, no
                    // audio device) the old immediate 466-then-467 answer still goes out,
                    // so the flow can never stall on a silent line.
                    if (mbGuiVoiceOverPending)
                    {
                        mbGuiVoiceOverPending = false;
                        const bool lbSounding =
                            CgsSystem::SpeechAudioPC::PlayByNameHash(muGuiVoiceOverHash);

                        CgsGui::GuiEvent<466> lVoiceOverStarted;
                        CgsGui::GuiModule::AddGuiEvent(lVoiceOverStarted, mpGuiInputBuffer);

                        if (lbSounding)
                        {
                            mbGuiVoiceOverSounding = true;   // 467 follows when it ends
                        }
                        else
                        {
                            CgsGui::GuiEvent<467> lVoiceOverFinished;
                            CgsGui::GuiModule::AddGuiEvent(lVoiceOverFinished, mpGuiInputBuffer);
                        }
                    }
                    else if (mbGuiVoiceOverSounding && CgsSystem::SpeechAudioPC::ConsumeFinished())
                    {
                        mbGuiVoiceOverSounding = false;
                        CgsGui::GuiEvent<467> lVoiceOverFinished;
                        CgsGui::GuiModule::AddGuiEvent(lVoiceOverFinished, mpGuiInputBuffer);
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
                // It runs BridgeGuiToDirector over the SAME out-queue, so the queue must still
                // be intact here -- that is why BridgeGuiToGame no longer clears it.
                DoUpdate_Director(true);
                // Both GUI out-event consumers have now run; retire this frame's records.
                // (The console's queue lifecycle is the module scheduler's IO-buffer teardown;
                // on PC the queue is a module member, so it is cleared explicitly.)
                mGuiModule.GetGuiOutQueue()->Clear();
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

        mpUpdateOutputBufferStack->DestroyIOBuffer<BrnWorldIO::UpdateOutputBuffer>(&mpWorldUpdateOutputBuffer);
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
        // CreateIOBuffer<T> runs each buffer's own Construct right after the stack alloc, the
        // same as the X360 instantiations (e.g. CgsGuiModuleIO::InputBuffer @0x823AC898 ->
        // InputBuffer::Construct @0x82857378, DirectorIO::OutputBuffer @0x823ACBF8). The hand
        // Construct calls that used to follow each line here are gone -- they would now run
        // twice, and DirectorIO::OutputBuffer::Construct re-clears a freshly built buffer.
        mpUpdateInputBufferStack->CreateIOBuffer<CgsGui::CgsGuiModuleIO::InputBuffer>(&mpGuiInputBuffer, "Gui");
        mpUpdateInputBufferStack->CreateIOBuffer<CgsGui::ViewIO::InputBuffer>(&mpGuiViewInputBuffer, "GuiView");
        mpUpdateOutputBufferStack->CreateIOBuffer<CgsGui::CgsGuiModuleIO::OutputBuffer>(&mpGuiOutputBuffer, "Gui");
        mpUpdateOutputBufferStack->CreateIOBuffer<CgsGui::ModelIO::OutputBuffer>(&mpGuiModelOutputBuffer, "GuiModel");
        mpUpdateOutputBufferStack->CreateIOBuffer<BrnDirector::DirectorIO::OutputBuffer>(&mpDirectorOutputBuffer, "Director");

        // ⭐ THIS SUB-STEP'S WORLD UPDATE OUTPUT BUFFER (see the header note on
        // GetWorldUpdateOutputBuffer). The console's DoUpdate owns it for the whole sub-step
        // and threads it through every leg; DoUpdate is a PC leaf, so it lives here with the
        // other per-sub-step buffers. The world leg (DriveWorldUpdateFrame) drives the world
        // INTO it and DoUpdate_Director's BridgeWorldToDirector reads it back out.
        mpUpdateOutputBufferStack->CreateIOBuffer<BrnWorldIO::UpdateOutputBuffer>(
            &mpWorldUpdateOutputBuffer, "World");
    }

    // @ BrnGameModule.cpp:2515 - free this sub-step's static GUI/director IO buffers (reverse
    // order of CreateStaticIOBuffers).
    void BrnGameModule::DestroyStaticIOBuffers()
    {
        mpUpdateOutputBufferStack->DestroyIOBuffer<BrnWorldIO::UpdateOutputBuffer>(&mpWorldUpdateOutputBuffer);
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
