#include "GameSource/Gui/BrnGuiModule.h"

#include <cstdio>                                                         // std::snprintf (probe logging)

#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"           // CgsGui::State
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"// CgsResource::BundleLoader
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"// CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"    // E_RESOURCETYPE_LUACODE / E_MEMTYPE_*
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"      // CgsResource::LuaCodeResource

// The initial loading-screen signal (MainGameFlowStateInitialLoadingScreen raises it on entry and drops it
// at FinishLoading). The boot videos gate on its completion so the loading screen shows before the logos.
extern bool gBrnLoadingScreenShouldShow;

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
        // Track the loading-screen lifecycle (the boot's gate). The game-flow loading screen sets
        // gBrnLoadingScreenShouldShow during the InitialLoadingScreen stages and drops it at FinishLoading.
        if (gBrnLoadingScreenShouldShow)
            mbLoadingHasShown = true;
        const bool lbLoadingComplete = mbLoadingHasShown && !gBrnLoadingScreenShouldShow;

        // ---- PHASE 0: BF_LOADING -----------------------------------------------------------------------
        // Run the BootLoading state through the BRNFLOADFSM Lua FSM while the loading screen is up. The boot
        // resources are already resident (loaded synchronously in Prepare), so feed cache-ready immediately;
        // BootLoading shows its loading screen (event emitted, not yet routed to the visual -- the game-flow
        // loading screen drives that) and waits. When the loading completes, feed loading-complete(137):
        // BootLoading sends "BF_PROCEED", and the sequencer advances to BF_VIDEOS (the PC stand-in for the
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
                mBootLoadingStateMachine.CgsFsm::Fsm::Update();   // runs BootLoading through the Lua FSM

                if (lbLoadingComplete)
                {
                    CgsModule::Event lDone;
                    mBootLoadingInQueue.AddEvent(&lDone, 137, static_cast<s32>(sizeof(lDone)));
                    mBootLoadingStateMachine.CgsFsm::Fsm::Update();   // process 137 -> SendStateEvent("BF_PROCEED")
                    CgsDev::Log::WriteToLog("[GuiModule] BF_LOADING done -> advancing to BF_VIDEOS.\n");
                    miBootPhase  = 1;
                    mbBootStarted = false;   // re-arm cache-ready for BF_VIDEOS
                }
            }
            else if (lbLoadingComplete)
            {
                miBootPhase  = 1;           // no BF_LOADING FSM -> go straight to the videos
                mbBootStarted = false;
            }
            return;   // the MovieManager bridge below belongs to BF_VIDEOS (phase 1)
        }

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
    }
}
