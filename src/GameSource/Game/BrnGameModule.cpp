#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert
#include "GameShared/GameClasses/Development/DebugSystem/CgsDebugFontBringUp.h"   // LoadAndSetDebugFont
#include "SDKs/EA/GameTalk/GameTalk.h"               // EA::GameTalk::GameTalkMessage (RenderMetricsMessageHandler)

#include <cstring>   // memset
#include <string.h>  // _stricmp (RenderMetricsMessageHandler; MSVC canonical, not declared by <cstring>)
#include <chrono>    // the ToGui repeat-clock feed (FLAG PC time source: wall clock)

#include "GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.h" // CgsInput::InputPadsPC (the PC pad-fill leaf)

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
        // Sized to the DOCUMENTED X360 values (main() @0x827E60D8 carves 0x780000 each;
        // see the comment above) -- the 64KB scratch stand-ins overflowed once the sound
        // module's ~66KB Root IO buffers started allocating here (the stage-4 assert).
        static u8 saUpdateInputMem[0x780000];
        static u8 saUpdateOutputMem[0x780000];
        sUpdateInputStack.Construct("UpdateInput");
        sUpdateInputStack.Prepare(saUpdateInputMem, sizeof(saUpdateInputMem), 16);
        sUpdateOutputStack.Construct("UpdateOutput");
        sUpdateOutputStack.Prepare(saUpdateOutputMem, sizeof(saUpdateOutputMem), 16);
        mpUpdateInputBufferStack = &sUpdateInputStack;
        mpUpdateOutputBufferStack = &sUpdateOutputStack;

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
        mWorldModule.Construct();        // +0x010E80  [gated] X360 passes &mCpuMonitors (slot +64);
                                         //            the WorldModule placeholder takes none yet.
        mInputModule.Construct();        // +0x6E9430  (slot 0; placeholder -> base)
        mGuiModule.Construct();          // +0x6EA820  [gated] X360 slot +84 with two sub-objects
                                         //            (+0x65A1D0/+0x65A1F4, inside the GameData
                                         //            module); the movie-hosting slice takes none.
        mGameStateModule.Construct();    // +0x669380  (slot 0; placeholder -> base)
        mEffectsModule.Construct();      // +0x878A00  (slot 0; placeholder -> base)
        mDirectorModule.Construct();     // +0x6B0C90  [gated] X360 slot +64; placeholder -> base.
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
                    case 19:   // PlayAptLoadingMovie -> the loading screen shows
                        gBrnLoadingScreenShouldShow = true;
                        break;
                    case 20:   // StopAptLoadingMovie -> the loading screen drops
                        gBrnLoadingScreenShouldShow = false;
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
                    case 86: case 87: case 89:
                        // Quit-to-dash (X360: XGetLaunchData + XLaunchNewImage). [FLAG PC
                        // platform: no dash relaunch; logged so the request is visible.]
                        CgsDev::Log::WriteToLog("[GameModule] GUI quit-to-dash command (86/87/89) -- "
                                                "PC platform no-op (FLAG).\n");
                        break;
                    case 138:  // dispatch render state 3
                    case 589:  // dispatch render state 4
                    case 590:  // dispatch render state 5 (the accept fade)
                        // The console writes the dispatch-thread input buffer's render-state
                        // byte (+39312). [FLAG: the PC renderer's mode consumer lands with
                        // the loading-screen render slice.]
                        break;
                    default:
                        break;
                }
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

    // FLAG PC-platform leaf: see DoUpdate above (the render dispatch runs from the PC
    // render thread's BrnRendererModule::Render drive).
    int BrnGameModule::DoDispatch()
    {
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
    // the GameData input's RequestInterface<32768>, (2) waits for the 3 loads + acquires the
    // "gamedb://...blobbyshadow.TextureConfig2d" resource, then (3) runs the per-module game
    // prepares. [gated] the whole body rides the GameData request/bundle-streaming path (the
    // module's file IO + bank/pool population, still the allocator gate) and the game-module IO
    // buffer members -- none in this layout yet. Returns done so UpdateThread advances; the
    // loading-screen renderer loads its own assets meanwhile.
    bool BrnGameModule::GamePrepare()
    {
        return true;
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

    // @ BrnGameModule.cpp:1253 - start-of-update-frame hook (resets the frame's lookback timer
    // and signals the renderer module's start-of-frame). Minimal until the renderer's
    // StartOfFrame + the lookback timer are reconstructed.
    void BrnGameModule::OnStartOfUpdateFrame()
    {
    }

    // @ BrnGameModule.cpp:1275 - end-of-update-frame hook (metrics + buffer-swap bookkeeping).
    // Minimal until those subsystems are reconstructed.
    void BrnGameModule::OnEndOfUpdateFrame()
    {
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
        mRenderModule.Render();

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
