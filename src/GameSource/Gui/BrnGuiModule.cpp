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
    u8 s_fsmPoolBacking[CgsResource::E_MEMTYPE_NUMTYPES][KU_FSM_POOL_BYTES];
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
    static CgsResource::LuaCodeResource* LoadFsmLuaCode(CgsResource::Pool& lPool, const char* lpcBundlePath)
    {
        CgsResource::Pool::InitOptions lOptions;
        lOptions.miId   = 2;
        lOptions.mpcName = "BootFsm";
        for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
        {
            lOptions.maHeapInfo[lt].muMaxNodes       = 64u;
            lOptions.maHeapInfo[lt].muHeapMemorySize = KU_FSM_POOL_BYTES - 64u * 1024u;
            lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
            lOptions.mResource.m_baseResources[lt]   = s_fsmPoolBacking[lt];
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

    void GuiModule::Construct()
    {
        mMovieManager.Construct();
        mbBootStarted = false;
        mbLoadingHasShown = false;
        mbBootFsmReady = false;
    }

    bool GuiModule::Prepare()
    {
        // Load VIDEOS\VIDEOLIST.BUNDLE synchronously (English; see MovieManager::Prepare) and publish the
        // manager so the renderer draws the active movie each frame (interim render bridge; the X360 renders
        // it through the GUI's own ViewIO ImRenderers via UpdateAndRenderMovieManager 0x82511240).
        mMovieManager.Prepare(0);
        gpActiveMovieManager = &mMovieManager;

        // Stand up the boot-logo flow: BootVideos emits play/stop through mBootStateInterface and reads its
        // input events from mBootInQueue (which this module feeds cache-ready + video-finished).
        mBootInQueue.Construct();
        mBootStateInterface.Construct();
        mbBootStarted = false;
        mbLoadingHasShown = false;
        mbBootFsmReady = false;

        // FAITHFUL FLOW (first slice): drive BootVideos through a real CgsGui::StateMachine running the
        // BRNVIDEOFSM Lua script. The script's SetState("BF_VIDEOS") activates the C++ BootVideos state;
        // from then on the StateMachine ticks it (Fsm::Update) just as BrnHudFlow does. [PC IO] the FSM
        // bundle is loaded synchronously (the X360 streams it via the GuiFsmController's ModelIO requests;
        // see [[lua-system]]). If anything in this path fails, fall back to driving BootVideos directly so
        // the boot videos still play (and log which path is active).
        CgsResource::LuaCodeResource* lpLuaCode = LoadFsmLuaCode(mBootFsmPool, "FSM/BRNVIDEOFSM.BUNDLE");
        if (lpLuaCode != 0)
        {
            mBootLuaHeap.Construct(s_bootLuaHeapBuffer, static_cast<s32>(sizeof(s_bootLuaHeapBuffer)));

            mBootVideos.Construct(CgsIDCompress("BF_VIDEOS"), &mBootStateMachine);
            mBootStateMachine.Construct();
            mBootStateMachine.SetStateInterface(&mBootStateInterface);
            CgsGui::State* lapStates[1] = { &mBootVideos };
            mBootStateMachine.SetStates(lapStates, 1);
            mBootStateMachine.SetInEventQueue(reinterpret_cast<InputBuffer::GuiEventQueue*>(&mBootInQueue));

            // Compile + enter the script: ScriptedFsm::Prepare -> SetState("BF_VIDEOS") -> BootVideos::OnEnter.
            mbBootFsmReady = mBootStateMachine.Prepare(lpLuaCode, &mBootLuaHeap, CgsIDCompress("BF_VIDEOS"));
        }

        CgsDev::Log::WriteToLog(mbBootFsmReady
            ? "[GuiModule] boot videos driven through the real BRNVIDEOFSM Lua FSM.\n"
            : "[GuiModule] boot FSM unavailable -> driving BootVideos directly (fallback).\n");

        if (!mbBootFsmReady)
        {
            // Fallback: the prior direct-drive path (no Lua FSM).
            mBootVideos.SetStateInterface(&mBootStateInterface);
            mBootVideos.SetInEventQueue(reinterpret_cast<InputBuffer::GuiEventQueue*>(&mBootInQueue));
            mBootVideos.OnEnter();
        }
        return true;
    }

    bool GuiModule::Release()
    {
        if (mbBootFsmReady)
        {
            mBootStateMachine.Release();   // ScriptedFsm::Release -> lua_close + clear
            mBootLuaHeap.Destruct();
            mbBootFsmReady = false;
        }
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
        // 1. Feed cache-ready once -- but only after the initial loading screen has been shown AND completed.
        //    This stands in for the boot FSM's BootPreload phase: in the real flow BootPreload runs first
        //    (the loading screen is up while boot resources stream), and BootVideos only plays once the
        //    cache is ready. The bundle itself loaded synchronously in Prepare, so the gate here is the
        //    initial-loading-screen completion (MainGameFlowStateInitialLoadingScreen drops
        //    gBrnLoadingScreenShouldShow at FinishLoading) -- so the user sees the loading screen first,
        //    THEN the EA/Criterion logos. [follow-on: drive this off the real BootPreload state.]
        if (gBrnLoadingScreenShouldShow)
            mbLoadingHasShown = true;
        const bool lbLoadingComplete = mbLoadingHasShown && !gBrnLoadingScreenShouldShow;
        if (!mbBootStarted && lbLoadingComplete)
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
