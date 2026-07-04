#include "GameSource/Gui/BrnGuiModule.h"

#include <cstdio>                                                         // std::snprintf (probe logging)

#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"           // CgsGui::State
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"// CgsResource::BundleLoader
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"// CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"    // E_RESOURCETYPE_LUACODE / E_MEMTYPE_*
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"      // CgsResource::LuaCodeResource
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> (read the loading-event subtype)
#include "GameShared/GameClasses/Fsm/CgsEvent.h"                          // CgsFsm::Event (drive BF_PROCEED through the FSM)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::GuiEventPlayAptMovie (channel-41 payload)
#include "GameSource/Gui/BrnAptRuntimeBringUp.h"                          // BrnGui::AptRuntime* (the Apt runtime bring-up + driver)

// PC KEYBOARD BRING-UP (FLAG): poll GetAsyncKeyState without dragging <Windows.h> into this
// game-source TU (its NOUSER/NOGDI lean-defines conflict). Signature per WinUser.h.
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
static short BrnGuiPcGetAsyncKeyState(int liVKey) { return GetAsyncKeyState(liVKey); }

// The loading-screen visual signal (BrnRendererModule::Render shows the loading screen while it's set).
// The GUI BF_LOADING state now OWNS this when its FSM is live: BootLoading::Update PlayLoadingScreen
// (channel 40, GuiEventPlayAptLoadingMovie/19) raises it; BootLoading::OnLeave StopLoadingScreen
// (channel 40, GuiEventStopAptLoadingMovie/20) drops it. The game-flow loading state defers to us via
// gBrnGuiDrivesLoadingScreen and signals loading-done via gBrnInitialLoadingComplete.
extern bool gBrnLoadingScreenShouldShow;   // defined in BrnGameMainFlowStates.cpp
extern bool gBrnInitialLoadingComplete;    // set by the game-flow when the load stages finish
extern bool gBrnGuiDrivesLoadingScreen;    // we set this when the BF_LOADING FSM is live

namespace
{
    // Backing for the boot FSM resource pool (3 mem types, like the MovieManager pool) + the Lua VM heap.
    // The FSM scripts are tiny single-state bundles (~1.5 KB each); the pool reserves 64 KB for its own
    // node/management structures (matching the MovieManager pool), so each backing buffer must comfortably
    // exceed that. 256 KB/type is plenty and not large.
    const u32 KU_FSM_POOL_BYTES = 256u * 1024u;
    // One backing set per concurrently-live boot FSM pool (BF_LOADING + BF_VIDEOS).
    u8 s_fsmPoolBacking[2][CgsResource::E_MEMTYPE_NUMTYPES][KU_FSM_POOL_BYTES];
    u8 s_bootLuaHeapBuffer[512u * 1024u];
}

// BrnGui::GuiModule -- the GUI module (minimal movie-hosting slice; see BrnGuiModule.h). X360
// GuiModule::Construct (0x82518028) builds the whole GUI subsystem + the embedded MovieManager; this
// reconstructs only the MovieManager host + lifecycle. The base ModuleSingleBuffered stages (IO buffers /
// data structures) run as for any dispatched module (the BrnRendererModule pattern). (The isolated
// ProfileHost::HandleProfileTaskResult decomp lives in BrnGuiProfileHost.cpp.)

namespace BrnGui
{
    // NOTE: this minimal GuiModule is driven DIRECTLY by BrnGameModule (Construct/Prepare/Update called
    // inline), not through the module dispatch, so it does NOT run the base ModuleSingleBuffered lifecycle.
    // The base Prepare() builds the module's input/output DataStructures via CreateInputDataStructure, which
    // asserts ("new module type - can't lock/unlock") for a module that hasn't declared them -- and the movie
    // slice needs none of that IO. (The real X360 GuiModule is double-buffered with full IO; that's the
    // follow-on when the GUI runs under the real dispatch.)
    // Load a single-state FSM bundle (one LuaCode resource, type 0x22) into lPool and return the
    // compiled-or-source Lua chunk. [PC IO] the X360 streams these through the GuiFsmController's
    // ModelIO resource requests; the PC port loads them synchronously via CgsResource::BundleLoader,
    // the same leaf the MovieManager uses for VIDEOLIST.BUNDLE (see [[lua-system]]). Returns null if
    // the bundle is missing/unreadable or carries no LuaCode resource.
    static CgsResource::LuaCodeResource* LoadFsmLuaCode(CgsResource::Pool& lPool, const char* lpcBundlePath,
                                                        s32 liBackingSet)
    {
        CgsResource::Pool::InitOptions lOptions;
        lOptions.miId   = 2;
        lOptions.mpcName = "BootFsm";
        for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
        {
            lOptions.maHeapInfo[lt].muMaxNodes       = 64u;
            lOptions.maHeapInfo[lt].muHeapMemorySize = KU_FSM_POOL_BYTES - 64u * 1024u;
            lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
            lOptions.mResource.m_baseResources[lt]   = s_fsmPoolBacking[liBackingSet][lt];
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_FSM_POOL_BYTES;
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
        }
        lOptions.muMaxResources         = 64u;
        lOptions.muMaxImports           = 64u;
        lOptions.miRefCountThreshold    = 0;
        lOptions.miNumDependencies      = 0;
        lOptions.miBankId               = 0;
        lOptions.mbAllowDefragmentation = false;
        lPool.InitPool(&lOptions);

        CgsResource::BundleLoader lLoader;
        const s32 liLoaded = lLoader.LoadBundle(lpcBundlePath, &lPool, CgsResource::ResolveResourceType);
        if (liLoaded <= 0)
        {
            CgsDev::Log::WriteToLog("[GuiModule] boot FSM bundle missing/unreadable.\n");
            return 0;
        }

        s32 liIndex = -1;
        CgsResource::Entry* lpEntry =
            lPool.FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_LUACODE, &liIndex);
        if (lpEntry == 0)
        {
            CgsDev::Log::WriteToLog("[GuiModule] boot FSM bundle has no LuaCode resource.\n");
            return 0;
        }
        CgsResource::LuaCodeResource* lpLua = reinterpret_cast<CgsResource::LuaCodeResource*>(
            lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY]);
        if (lpLua != 0)
        {
            char lac[96];
            std::snprintf(lac, sizeof(lac), "[GuiModule] FSM LuaCode resource loaded (%u bytes).\n",
                          lpLua->GetSourceSize());
            CgsDev::Log::WriteToLog(lac);
        }
        return lpLua;
    }

    // Route the loading interface's emitted loading-screen events to the renderer's loading-screen signal.
    // BootLoading emits them on channel 40 (GuiEventPlayAptLoadingMovie/19 = show; GuiEventStopAptLoadingMovie
    // /20 = stop); the GuiEvent muEventType subtype tells them apart. This is what makes the GUI BF_LOADING
    // state OWN the loading-screen visual (the faithful path); the game-flow defers to it.
    static void RouteLoadingScreenEvents(CgsGui::StateInterface& lStateInterface)
    {
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue = lStateInterface.GetOutputEventQueue();
        if (lpOutQueue == 0)
            return;
        CgsModule::VariableEventQueue<65536, 16>* lpOutBase = lpOutQueue;
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpOutBase->GetFirstEvent(&lpEvent, &liSize);
        while (liId >= 0 && lpEvent != 0)
        {
            if (liId == 40)   // channel 40 = apt-movie events; BootLoading uses the loading-movie subtypes
            {
                const s32 liSubtype = reinterpret_cast<const CgsGui::GuiEvent<0>*>(lpEvent)->muEventType;
                if (liSubtype == 19)        // GuiEventPlayAptLoadingMovie -> show the loading screen
                    gBrnLoadingScreenShouldShow = true;
                else if (liSubtype == 20)   // GuiEventStopAptLoadingMovie -> stop the loading screen
                    gBrnLoadingScreenShouldShow = false;
            }
            const CgsModule::Event* lpNext = 0;
            liId = lpOutBase->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
        lpOutBase->Clear();
    }

    // Route the BF_LEGAL state's emitted Apt-movie events to the Apt runtime. BootLegal emits
    // GuiEventPlayAptMovie on channel 41 (type 18) carrying the movie name ("Title_Screen02") +
    // level num; StateInterface::PlayAptMovie posted it. This is the channel-41 consumer the Apt
    // runtime needed -- the parallel of RouteLoadingScreenEvents (which reads channel 40). It
    // hands the movie name to the Apt runtime (load + tick + render). Defensive: every step the
    // runtime takes is itself null-checked + logged + bails cleanly (see BrnAptRuntimeBringUp.cpp).
    // NOTE: this does NOT clear the output queue -- BootLegal's other channel-41/40 outputs
    // (apt-view-state, music, etc.) are consumed elsewhere; here we only OBSERVE channel 41.
    static void RouteAptMovieEvents(CgsGui::StateInterface& lStateInterface)
    {
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue = lStateInterface.GetOutputEventQueue();
        if (lpOutQueue == 0)
            return;
        CgsModule::VariableEventQueue<65536, 16>* lpOutBase = lpOutQueue;
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpOutBase->GetFirstEvent(&lpEvent, &liSize);
        while (liId >= 0 && lpEvent != 0)
        {
            if (liId == 41)   // channel 41 = apt-movie events; BootLegal posts GuiEventPlayAptMovie (type 18)
            {
                const CgsGui::GuiEventPlayAptMovie* lpPlay =
                    reinterpret_cast<const CgsGui::GuiEventPlayAptMovie*>(lpEvent);
                if (lpPlay->muEventType == 18)   // GuiEventPlayAptMovie -> consume + (attempt to) load
                {
                    BrnGui::AptRuntimeBringUp();   // ensure the runtime is up (idempotent)
                    BrnGui::AptRuntimePlayMovie(lpPlay->mpacMovieName, lpPlay->miLevelNum);
                }
            }
            const CgsModule::Event* lpNext = 0;
            liId = lpOutBase->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
        // Do NOT Clear() here: BootLegal's output queue carries other events consumed elsewhere.
    }

    // Bring one boot FSM phase up: construct + wire the state into its StateMachine, then compile + enter
    // the Lua script (ScriptedFsm::Prepare -> SetState(id) -> state OnEnter). Returns true if the FSM came up.
    static bool SetupBootPhase(CgsGui::StateMachine& lStateMachine, CgsGui::State& lState,
                               CgsGui::StateInterface& lStateInterface,
                               CgsModule::VariableEventQueue<18432, 16>& lInQueue,
                               CgsResource::LuaCodeResource* lpLuaCode, CgsMemory::HeapMalloc& lHeap, CgsID lId)
    {
        if (lpLuaCode == 0)
            return false;
        lState.Construct(lId, &lStateMachine);
        lStateMachine.Construct();
        lStateMachine.SetStateInterface(&lStateInterface);
        CgsGui::State* lapStates[1] = { &lState };
        lStateMachine.SetStates(lapStates, 1);
        lStateMachine.SetInEventQueue(reinterpret_cast<InputBuffer::GuiEventQueue*>(&lInQueue));
        return lStateMachine.Prepare(lpLuaCode, &lHeap, lId);
    }

    void GuiModule::Construct()
    {
        mMovieManager.Construct();
        mbBootStarted = false;
        mbLoadingHasShown = false;
        mbBootFsmReady = false;
        mbBootLoadingFsmReady = false;
        mbBootLoadingCompleteFed = false;
        miBootPhase = 0;
    }

    bool GuiModule::Prepare()
    {
        // Load VIDEOS\VIDEOLIST.BUNDLE synchronously (English; see MovieManager::Prepare) and publish the
        // manager so the renderer draws the active movie each frame (interim render bridge; the X360 renders
        // it through the GUI's own ViewIO ImRenderers via UpdateAndRenderMovieManager 0x82511240).
        mMovieManager.Prepare(0);
        gpActiveMovieManager = &mMovieManager;

        mbBootStarted = false;
        mbLoadingHasShown = false;
        mbBootFsmReady = false;
        mbBootLoadingFsmReady = false;
        mbBootLoadingCompleteFed = false;
        miBootPhase = 0;

        // Shared Lua VM heap for both boot-phase FSMs (each gets its own lua_State from it).
        mBootLuaHeap.Construct(s_bootLuaHeapBuffer, static_cast<s32>(sizeof(s_bootLuaHeapBuffer)));

        // FAITHFUL BOOT FLOW: sequence the boot through the real Lua FSMs -- BF_LOADING (BRNFLOADFSM) then
        // BF_VIDEOS (BRNVIDEOFSM), each a single-state FSM the X360 GuiFsmController loads in turn. [PC IO]
        // the bundles are loaded synchronously (the X360 streams them via ModelIO; see [[lua-system]]); the
        // boot advances BF_LOADING->BF_VIDEOS at loading-complete (the PC stand-in for the controller's
        // load-complete sequencing). If a phase's FSM fails to come up, the boot still reaches the videos
        // (BF_VIDEOS falls back to driving BootVideos directly), so the logos always play.

        // PHASE 0: BF_LOADING (the boot loading state, run through its Lua FSM).
        mBootLoadingInQueue.Construct();
        mBootLoadingStateInterface.Construct();
        CgsResource::LuaCodeResource* lpLoadingLua = LoadFsmLuaCode(mBootLoadingPool, "FSM/BRNFLOADFSM.BUNDLE", 0);
        mbBootLoadingFsmReady = SetupBootPhase(mBootLoadingStateMachine, mBootLoading, mBootLoadingStateInterface,
                                               mBootLoadingInQueue, lpLoadingLua, mBootLuaHeap,
                                               CgsIDCompress("BF_LOADING"));
        // When BF_LOADING is live it owns the loading-screen visual; tell the game-flow to defer.
        gBrnGuiDrivesLoadingScreen = mbBootLoadingFsmReady;

        // PHASE 1: BF_VIDEOS (the boot-logo state, run through its Lua FSM).
        mBootInQueue.Construct();
        mBootStateInterface.Construct();
        CgsResource::LuaCodeResource* lpVideosLua = LoadFsmLuaCode(mBootFsmPool, "FSM/BRNVIDEOFSM.BUNDLE", 1);
        mbBootFsmReady = SetupBootPhase(mBootStateMachine, mBootVideos, mBootStateInterface,
                                        mBootInQueue, lpVideosLua, mBootLuaHeap, CgsIDCompress("BF_VIDEOS"));

        CgsDev::Log::WriteToLog(mbBootLoadingFsmReady
            ? "[GuiModule] BF_LOADING running through the real BRNFLOADFSM Lua FSM.\n"
            : "[GuiModule] BF_LOADING FSM unavailable -> skipping straight to BF_VIDEOS.\n");
        CgsDev::Log::WriteToLog(mbBootFsmReady
            ? "[GuiModule] BF_VIDEOS running through the real BRNVIDEOFSM Lua FSM.\n"
            : "[GuiModule] BF_VIDEOS FSM unavailable -> driving BootVideos directly (fallback).\n");

        if (!mbBootFsmReady)
        {
            // Fallback: drive BootVideos directly (the pre-FSM path) so the logos still play.
            mBootVideos.SetStateInterface(&mBootStateInterface);
            mBootVideos.SetInEventQueue(reinterpret_cast<InputBuffer::GuiEventQueue*>(&mBootInQueue));
            mBootVideos.OnEnter();
        }

        // PHASE 2: BF_LEGAL queues. The FSM itself (BRNLEGALFSM) is loaded + entered only when BF_VIDEOS
        // signals "done" (in UpdateBootVideoFlow) -- the PC stand-in for the X360 GuiFsmController loading
        // the next single-state FSM on the boot-videos-done state event, so BootLegal::OnEnter does not fire
        // until the logos finish.
        mBootLegalInQueue.Construct();
        mBootLegalStateInterface.Construct();
        mbBootLegalFsmReady = false;

        // Stand up the Apt runtime host (allocator + interpreter + AptAux host callback table +
        // the render buffer) so it is live before BF_LEGAL posts PlayAptMovie("Title_Screen02").
        // Idempotent + defensive (logs [AptRT] probes; bails cleanly at the first un-homed piece).
        BrnGui::AptRuntimeBringUp();

        return true;
    }

    bool GuiModule::Release()
    {
        if (mbBootLoadingFsmReady)
        {
            mBootLoadingStateMachine.Release();   // ScriptedFsm::Release -> lua_close + clear
            mbBootLoadingFsmReady = false;
        }
        if (mbBootFsmReady)
        {
            mBootStateMachine.Release();
            mbBootFsmReady = false;
        }
        mBootLuaHeap.Destruct();
        gpActiveMovieManager = 0;
        mMovieManager.Release();
        return true;
    }

    void GuiModule::Destruct()
    {
        mMovieManager.Destruct();
    }

    // X360 GuiModule::Update (0x82527A58) ticks the GUI model + the HUD/Screen flows + the MovieManager.
    // Here it runs the boot-logo flow (BootVideos) bridged to the MovieManager.
    void GuiModule::Update()
    {
        UpdateBootVideoFlow();
    }

    // Drive the boot-logo flow for one frame: feed BootVideos its events, tick it, deliver its output to
    // the MovieManager, tick the manager, and feed video-finished back. [MINIMAL boot driver -- the X360
    // runs BootVideos inside BrnHudFlow and routes via the EventObserver; this bridges the queues directly.]
    void GuiModule::UpdateBootVideoFlow()
    {
        // ---- PHASE 0: BF_LOADING -----------------------------------------------------------------------
        // Run the BootLoading state through the BRNFLOADFSM Lua FSM while the loading screen is up. The boot
        // resources are already resident (loaded synchronously in Prepare), so feed cache-ready immediately;
        // BootLoading shows the loading screen (PlayLoadingScreen -> channel-40/19), which RouteLoadingScreen
        // Events turns into gBrnLoadingScreenShouldShow=true so the renderer draws it. When the game-flow's
        // load stages finish (gBrnInitialLoadingComplete), feed loading-complete(137): BootLoading sends
        // "BF_PROCEED"; we then Release the loading FSM so BootLoading::OnLeave -> StopLoadingScreen
        // (channel-40/20) drops the loading screen, and advance to BF_VIDEOS (the PC stand-in for the
        // GuiFsmController loading the next single-state FSM).
        if (miBootPhase == 0)
        {
            if (mbBootLoadingFsmReady)
            {
                if (!mbBootStarted)
                {
                    CgsModule::Event lReady;
                    mBootLoadingInQueue.AddEvent(&lReady, 64, static_cast<s32>(sizeof(lReady)));
                    mbBootStarted = true;
                }
                // Deliver the loading-complete INPUT (137) once the game-flow's load stages finish; this is
                // what the X360 loading system raises at BootLoading. BootLoading::Update then DECIDES to
                // proceed (SendStateEvent "BF_PROCEED") -- the advance below is driven by THAT decision, not
                // by gBrnInitialLoadingComplete directly.
                if (gBrnInitialLoadingComplete && !mbBootLoadingCompleteFed)
                {
                    CgsModule::Event lDone;
                    mBootLoadingInQueue.AddEvent(&lDone, 137, static_cast<s32>(sizeof(lDone)));
                    mbBootLoadingCompleteFed = true;
                }

                mBootLoadingStateMachine.CgsFsm::Fsm::Update();   // runs BootLoading through the Lua FSM
                RouteLoadingScreenEvents(mBootLoadingStateInterface);  // BootLoading show/stop -> loading screen

                // The state signalled a transition: run it through the FSM (the flow's mechanism --
                // ScriptedFsm::SendEvent -> mLuaState.NextState -> SetState), then sequence to the next phase.
                // For the single-state boot FSMs NextState is a no-op, so (faithful to the X360
                // GuiFsmController) the controller advances by loading the next phase's FSM on the state event.
                if (mBootLoading.IsStateChangePending())
                {
                    CgsFsm::Event lEvent;
                    lEvent.Construct(CgsIDCompress(mBootLoading.GetPendingEventName()));
                    mBootLoadingStateMachine.SendEvent(&lEvent);     // BF_PROCEED -> NextState (no-op for BF_LOADING)
                    mBootLoading.ClearStateChange();

                    mBootLoadingStateMachine.Release();              // OnLeave -> StopLoadingScreen (40/20)
                    RouteLoadingScreenEvents(mBootLoadingStateInterface);  // ... -> loading screen off
                    mbBootLoadingFsmReady = false;                   // FSM released; don't tick/release again
                    CgsDev::Log::WriteToLog("[GuiModule] BF_LOADING signalled BF_PROCEED -> advancing to BF_VIDEOS.\n");
                    miBootPhase  = 1;
                    mbBootStarted = false;   // re-arm cache-ready for BF_VIDEOS
                }
            }
            else if (gBrnInitialLoadingComplete)
            {
                miBootPhase  = 1;           // no BF_LOADING FSM -> go straight to the videos
                mbBootStarted = false;
            }
            return;   // the MovieManager bridge below belongs to BF_VIDEOS (phase 1)
        }

        // ---- PHASE 2: BF_LEGAL ------------------------------------------------------------------------
        // BootLegal (the legal / title screen) runs through the BRNLEGALFSM Lua FSM, driving its
        // 0..10-stage machine (wait-cache -> play the title_screen02 title movie -> fade-in -> wait-start
        // -> attract loop -> menu). The title/attract movie + the menu's Apt view-state output are the
        // deferred render boundaries (the movie-definition prepare + the Apt engine, FLAG'd in
        // BrnBootLegalBoundary.cpp); the flow reaching this phase + the title_screen02 request IS the
        // BF_LEGAL milestone. [follow-on: bridge the title movie to the MovieManager + the menu Apt-view
        // to the Apt engine so the legal/title screen renders.]
        if (miBootPhase == 2)
        {
            if (!mbBootStarted)
            {
                CgsModule::Event lReadyEvent;
                mBootLegalInQueue.AddEvent(&lReadyEvent, 64, static_cast<s32>(sizeof(lReadyEvent)));  // cache-ready
                mbBootStarted = true;
            }

            // ---- boot-resources-ready feedback (event 567; bring-up FLAG) ----------------------
            // The console GUI cache posts 567 when the title's expected apt components have
            // initialised, which arms BootLegal's press-start path (mbWaitForStartPressed).
            // The cache watcher isn't reconstructed; post it once when the apt movie is live.
            {
                static bool sbResourceReadyFed = false;
                if (!sbResourceReadyFed && BrnGui::AptRuntimeIsMovieLive())
                {
                    CgsModule::Event lReady;
                    mBootLegalInQueue.AddEvent(&lReady, 567, static_cast<s32>(sizeof(lReady)));
                    sbResourceReadyFed = true;
                    CgsDev::Log::WriteToLog("[GuiModule] apt movie live -> fed resources-ready (567) to BF_LEGAL.\n");
                }
            }

            // ---- PC KEYBOARD -> boot-flow input events (bring-up FLAG) -------------------------
            // The console feeds BootLegal pad input through the event-interpreter pipeline
            // (event 143 = "press start" feedback; events 6/21 = controller actions with the
            // sub-id at payload+4: 41 menu-next, 42 menu-prev, 45 back). That pipeline is not
            // up on PC yet, so poll the keyboard here and post the SAME event records BootLegal
            // consumes: Enter/Space -> 143 (start), Down/Right -> 6/41, Up/Left -> 6/42,
            // Escape -> 6/45. Edge-triggered so a held key posts once.
            {
                struct PcActionEvent : public CgsModule::Event
                {
                    s32 miPad0;    // +0x00
                    s32 miSubId;   // +0x04 (BootLegal reads the action sub-id here)
                    s32 miPad2;
                    s32 miPad3;
                    explicit PcActionEvent(s32 liSubId)
                        : miPad0(0), miSubId(liSubId), miPad2(0), miPad3(0) {}
                };
                struct PcKeyMap { int iVKey; s32 iEventId; s32 iSubId; };
                static const PcKeyMap KA_KEYS[] =
                {
                    { 0x0D /*VK_RETURN*/, 143, 0  },
                    { 0x20 /*VK_SPACE*/,  143, 0  },
                    { 0x28 /*VK_DOWN*/,   6,   41 },
                    { 0x27 /*VK_RIGHT*/,  6,   41 },
                    { 0x26 /*VK_UP*/,     6,   42 },
                    { 0x25 /*VK_LEFT*/,   6,   42 },
                    { 0x1B /*VK_ESCAPE*/, 6,   45 },
                };
                static bool sabKeyWasDown[sizeof(KA_KEYS) / sizeof(KA_KEYS[0])] = {};
                for (u32 luKey = 0; luKey < sizeof(KA_KEYS) / sizeof(KA_KEYS[0]); ++luKey)
                {
                    const bool lbDown = (BrnGuiPcGetAsyncKeyState(KA_KEYS[luKey].iVKey) & 0x8000) != 0;
                    if (lbDown && !sabKeyWasDown[luKey])
                    {
                        PcActionEvent lAction(KA_KEYS[luKey].iSubId);
                        const bool lbAdded = mBootLegalInQueue.AddEvent(&lAction, KA_KEYS[luKey].iEventId,
                                                   static_cast<s32>(sizeof(lAction)));
                        char lacKey[96];
                        std::snprintf(lacKey, sizeof(lacKey),
                            "[GuiModule] key vk=0x%02X -> event %d/%d (AddEvent=%d)\n",
                            KA_KEYS[luKey].iVKey, KA_KEYS[luKey].iEventId,
                            KA_KEYS[luKey].iSubId, lbAdded ? 1 : 0);
                        CgsDev::Log::WriteToLog(lacKey);
                    }
                    sabKeyWasDown[luKey] = lbDown;
                }
            }
            if (mbBootLegalFsmReady)
                mBootLegalStateMachine.CgsFsm::Fsm::Update();   // runs BootLegal through the Lua FSM
            else
                mBootLegal.Update();                            // fallback: drive BootLegal directly

            // Consume BootLegal's channel-41 PlayAptMovie("Title_Screen02") output -> the Apt runtime
            // (load the title movie), then tick the Apt runtime for this frame (advance + render).
            RouteAptMovieEvents(mBootLegalStateInterface);

            // ---- BF_LEGAL attract-video pump (the BF_VIDEOS steps 3/3b/4/5 for this phase) ----
            // BootLegal's attract loop posts play(508)/stop(509) onto its state interface; deliver
            // them to the MovieManager, tick it, and feed video-finished (510) back into the
            // BF_LEGAL in-queue -- exactly what the X360 module dispatch does for every phase.
            // The legal out-queue is append-only (RouteAptMovieEvents deliberately does not
            // Clear() it -- the channel-41 re-fire is load-bearing), so a high-water cursor keeps
            // the pump from re-delivering the same records each frame.
            {
                static const CgsModule::Event* s_pVideoPumpCursor = 0;
                CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpLegalOut =
                    mBootLegalStateInterface.GetOutputEventQueue();
                if (lpLegalOut != 0)
                {
                    CgsModule::VariableEventQueue<65536, 16>* lpOutBase = lpLegalOut;
                    const CgsModule::Event* lpEvent = 0;
                    s32 liSize = 0;
                    s32 liId = lpOutBase->GetFirstEvent(&lpEvent, &liSize);
                    bool lbPastCursor = (s_pVideoPumpCursor == 0);
                    const CgsModule::Event* lpLast = s_pVideoPumpCursor;
                    while (liId >= 0 && lpEvent != 0)
                    {
                        if (lbPastCursor && (liId == 508 || liId == 509))
                        {
                            mMovieManager.GetReceiverQueue()->AddEvent(lpEvent, liId, liSize);
                            char lacV[80];
                            std::snprintf(lacV, sizeof(lacV),
                                          "[GuiModule] BF_LEGAL video event %d -> MovieManager.\n", liId);
                            CgsDev::Log::WriteToLog(lacV);
                        }
                        if (lpEvent == s_pVideoPumpCursor)
                            lbPastCursor = true;
                        lpLast = lpEvent;
                        const CgsModule::Event* lpNext = 0;
                        liId = lpOutBase->GetNextEvent(lpEvent, &lpNext, &liSize);
                        lpEvent = lpNext;
                    }
                    s_pVideoPumpCursor = lpLast;
                }
                // Drain the receiver queue into RecvEvent + tick the manager (BF_VIDEOS 3b/4).
                {
                    CgsModule::VariableEventQueue<1024, 16>* lpRecv = mMovieManager.GetReceiverQueue();
                    const CgsModule::Event* lpEvent = 0;
                    s32 liSize = 0;
                    s32 liId = lpRecv->GetFirstEvent(&lpEvent, &liSize);
                    while (liId >= 0 && lpEvent != 0)
                    {
                        mMovieManager.RecvEvent(lpEvent, liId);
                        const CgsModule::Event* lpNext = 0;
                        liId = lpRecv->GetNextEvent(lpEvent, &lpNext, &liSize);
                        lpEvent = lpNext;
                    }
                    lpRecv->Clear();
                }
                mMovieManager.Update();
                // Video finished (incl. the missing-attract-file skip) -> 510 to BF_LEGAL (step 5).
                if (mMovieManager.HasFinishedReporting())
                {
                    CgsModule::Event lFinishedEvent;
                    mBootLegalInQueue.AddEvent(&lFinishedEvent, 510, static_cast<s32>(sizeof(lFinishedEvent)));
                    mMovieManager.AcknowledgeFinishedAndReturnToIdle();
                    CgsDev::Log::WriteToLog("[GuiModule] BF_LEGAL video finished -> fed 510.\n");
                }
            }

            BrnGui::AptRuntimeUpdate();
            return;
        }

        // Entering BF_VIDEOS: the loading screen must be down (BootLoading::OnLeave dropped it; this is a
        // belt-and-suspenders in case the stop event didn't route, so no loading screen lingers over the logos).
        if (gBrnLoadingScreenShouldShow)
            gBrnLoadingScreenShouldShow = false;

        // ---- PHASE 1: BF_VIDEOS ------------------------------------------------------------------------
        // 1. Feed cache-ready once so BootVideos starts playing the logos.
        if (!mbBootStarted)
        {
            CgsModule::Event lReadyEvent;
            mBootInQueue.AddEvent(&lReadyEvent, 64, static_cast<s32>(sizeof(lReadyEvent)));
            mbBootStarted = true;
        }

        // 2. Tick the boot state. Through the real Lua FSM (StateMachine drives the current state's
        //    PreUpdate/Update/PostUpdate) when it came up, else directly (fallback). Either way it reads
        //    mBootInQueue and emits play/stop onto mBootStateInterface's output.
        if (mbBootFsmReady)
            mBootStateMachine.CgsFsm::Fsm::Update();
        else
            mBootVideos.Update();

        // 3. Deliver the state's output play(508)/stop(509) to the MovieManager's receiver queue.
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue = mBootStateInterface.GetOutputEventQueue();
        if (lpOutQueue != 0)
        {
            // Drive the queue through its VariableEventQueue base (inline) -- GuiEventQueueBase's own
            // Clear/GetFirstEvent/GetNextEvent are declared-only.
            CgsModule::VariableEventQueue<65536, 16>* lpOutBase = lpOutQueue;
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lpOutBase->GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                if (liId == KI_GUIEVENT_PLAY_VIDEO || liId == KI_GUIEVENT_STOP_VIDEO)
                    mMovieManager.GetReceiverQueue()->AddEvent(lpEvent, liId, liSize);
                const CgsModule::Event* lpNext = 0;
                liId = lpOutBase->GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            lpOutBase->Clear();
        }

        // 3b. Dispatch the MovieManager's receiver queue to RecvEvent. In the X360 the module event
        //     dispatcher drains the EventReceiverQueue<1024,16> into MovieManager::RecvEvent each frame;
        //     this synchronous bridge does the same (otherwise the 508/509 we just queued never reach
        //     HandlePlayVideoEvent, so nothing is ever queued and the manager sits in IDLE forever).
        {
            CgsModule::VariableEventQueue<1024, 16>* lpRecv = mMovieManager.GetReceiverQueue();
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lpRecv->GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                mMovieManager.RecvEvent(lpEvent, liId);
                const CgsModule::Event* lpNext = 0;
                liId = lpRecv->GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            lpRecv->Clear();
        }

        // 4. Tick the MovieManager.
        mMovieManager.Update();

        // 5. When the manager parks in REPORTING_FINISHED (a video ended), feed video-finished (510) back to
        //    BootVideos and return the manager to IDLE so the next queued video plays. (The X360 GUI flow
        //    owns this acknowledge+reset; Update itself keeps the manager in REPORTING_FINISHED. Resetting
        //    here is what lets the EA logo be followed by Criterion -- without it the manager sticks at
        //    REPORTING_FINISHED and the queued Criterion is never picked up.)
        if (mMovieManager.HasFinishedReporting())
        {
            CgsModule::Event lFinishedEvent;
            mBootInQueue.AddEvent(&lFinishedEvent, 510, static_cast<s32>(sizeof(lFinishedEvent)));
            mMovieManager.AcknowledgeFinishedAndReturnToIdle();
        }

        // 6. BF_VIDEOS -> BF_LEGAL: when BootVideos signals "done" (Criterion finished), advance to the
        //    legal/title screen. Faithful to the X360 GuiFsmController, the controller loads the NEXT phase
        //    FSM (BRNLEGALFSM) on the boot-videos-done state event -- so run the videos FSM's transition,
        //    release it, then load + enter BF_LEGAL. Mirrors the BF_LOADING -> BF_VIDEOS advance above.
        if (mBootVideos.IsStateChangePending())
        {
            CgsFsm::Event lEvent;
            lEvent.Construct(CgsIDCompress(mBootVideos.GetPendingEventName()));
            mBootStateMachine.SendEvent(&lEvent);   // "done" -> NextState (no-op for the single-state BF_VIDEOS)
            mBootVideos.ClearStateChange();
            mBootStateMachine.Release();
            mbBootFsmReady = false;

            CgsResource::LuaCodeResource* lpLegalLua = LoadFsmLuaCode(mBootLegalPool, "FSM/BRNLEGALFSM.BUNDLE", 2);
            mbBootLegalFsmReady = SetupBootPhase(mBootLegalStateMachine, mBootLegal, mBootLegalStateInterface,
                                                 mBootLegalInQueue, lpLegalLua, mBootLuaHeap, CgsIDCompress("BF_LEGAL"));
            if (!mbBootLegalFsmReady)
            {
                // Fallback: drive BootLegal directly (no BRNLEGALFSM) so the legal/title screen still runs.
                mBootLegal.SetStateInterface(&mBootLegalStateInterface);
                mBootLegal.SetInEventQueue(reinterpret_cast<InputBuffer::GuiEventQueue*>(&mBootLegalInQueue));
                mBootLegal.OnEnter();
            }
            CgsDev::Log::WriteToLog(mbBootLegalFsmReady
                ? "[GuiModule] BF_VIDEOS signalled done -> advancing to BF_LEGAL (BRNLEGALFSM Lua FSM).\n"
                : "[GuiModule] BF_VIDEOS signalled done -> advancing to BF_LEGAL (direct fallback).\n");
            miBootPhase   = 2;
            mbBootStarted = false;   // re-arm cache-ready for BF_LEGAL
        }
    }
}
