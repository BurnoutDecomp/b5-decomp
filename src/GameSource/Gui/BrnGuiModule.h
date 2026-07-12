#ifndef BRN_GUI_MODULE_H
#define BRN_GUI_MODULE_H

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"      // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"  // CgsGui::ViewIO Input/OutputBuffer (the per-frame bridge pair)
#include "GameSource/Gui/BrnGuiMovieManager.h"                          // BrnGui::MovieManager (embedded)
#include "GameSource/Gui/BrnGuiAptRuntime.h"                             // BrnGui::AptRuntimeHost (embedded)
#include "GameSource/Gui/BrnGuiViewModule.h"                             // BrnGui::ViewModule (embedded)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                  // CgsGui::CgsGuiModuleIO::InputBuffer (the inbound GUI event buffer)
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"          // CgsGui::ModelIO Input/OutputBuffer (the FSM controller's IO pair)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"     // CgsResource::Pool (holds the loaded FSM bundle)
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"               // CgsMemory::HeapMalloc (the FSM Lua VM heap)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"             // CgsMemory::LinearMalloc (the HUD state pool)
#include "GameSource/Gui/BrnGuiFsmController.h"                         // BrnGui::GuiFsmController (the flow FSM controller)
#include "GameSource/Gui/Flow/HUD/BrnHudFlow.h"                         // BrnGui::BrnHudFlow (the 14-state HUD flow)
#include "GameSource/Gui/Flow/Overlay/BrnOverlayFlow.h"                 // BrnGui::BrnOverlayFlow (the 15-popup-state overlay flow)
#include "GameSource/Gui/Flow/Screen/BrnScreenFlow.h"                   // BrnGui::BrnScreenFlow (the 61-state front-end SCREEN flow)
#include "GameSource/Gui/BrnGuiProfile.h"                               // BrnGui::ProfileManager (module-owned; REAL)
#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"             // CgsGui::SystemUserProfile (module-owned; X360 +949152)
#include "GameSource/Gui/BrnGuiCache.h"                                 // BrnGui::GuiCache (the flow states' cache)

// BrnGui::GuiModule -- the GUI module (a dispatched CgsModule, like BrnRendererModule). The X360 module
// (Construct 0x82518028 / Prepare 0x82518D68 / Update 0x82527A58 / Render 0x825146B8) builds the entire
// GUI subsystem (model + view + the HUD/Screen flows) and EMBEDS the MovieManager (X360 +301600), driving
// it each frame via UpdateAndRenderMovieManager (0x82511240).
//
// THE BOOT/MENU FLOW IS THE REAL CONTROLLER CHAIN (2026-07-10): the module owns the real
// BrnGui::GuiFsmController + BrnHudFlow (the 14-state pool BF_PRELOAD..PRE_FLY_BY in ONE
// CgsGui::StateMachine), sequenced exactly as the X360 GuiModule::Update does --
//   * GuiEventRunFsm posts (event 144, from the game module's BridgeGameToGui) ->
//     GuiFsmController::RunFsm;
//   * the controller's load machine posts GuiEventLoadRequest records into the ModelIO
//     input buffer; the FSM LuaCode bundle loads land as GuiEventLoadNotification (14)
//     records on the ModelIO output buffer; PrepareLua enters the script's state;
//   * the flow's states register their observed events (records 34/35 on the state
//     interface output queue); the module fans matching inbound GUI events into the
//     flow's in-queue (the EventInterpreterModule observer-subscription dispatch);
//   * each boot state posts command 70 (channel 40) at phase end; the game module's
//     BridgeGuiToGame consumes it and the game main flow requests the next FSM stage.
// The [PC IO] leaf that remains module-side is the synchronous FSM-bundle load standing
// in for the GuiResourceModule module dispatch (ServiceFsmBundleRequests).
namespace BrnGui
{
    class GuiModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        void Construct() override;
        bool Prepare() override;
        bool Release() override;
        void Destruct() override;
        void Update() override;

        // The per-frame GUI render drive (X360 BrnGui::GuiModule::Render @0x825146B8 ->
        // CgsGui::GuiModule::Render @0x8285AF38's core): publish the active renderer set
        // into the view input buffer (SetImRenderers), run the view module's render entry
        // (ViewModule::Render @0x82858810 -> the RenderInternal virtual -> AptAux::Render
        // -> the engine render walk), then flush the filled Apt command buffer to D3D9
        // (the host's PC dispatch leaf). FLAG PC-ABI adapter: the console signature takes
        // the scheduler's view/GUI IO buffers + the render output buffer and gates on the
        // module-prepared byte (+949208); this PC drive owns its IO pair and gates on the
        // Apt bring-up. Called from BrnRendererModule::Render (the PC render thread).
        void Render();

        MovieManager* GetMovieManager() { return &mMovieManager; }
        ViewModule* GetViewModule() { return &mViewModule; }
        AptRuntimeHost* GetAptRuntimeHost() { return &mAptRuntimeHost; }

        // Hand this sub-step's GUI module INPUT buffer (filled by BrnGameModule's
        // BridgeControllerToGui + BridgeGameToGui) to the update drive; Update dispatches
        // its inbound events (144 -> RunFsm, 14/16/481 -> the model notifications,
        // 504/508/513 -> the MovieManager) and fans the rest into the HUD flow's in-queue
        // per the observer subscriptions. FLAG (bridge stand-in): on the console the
        // buffer arrives through the module scheduler's IO set.
        void SetGuiEventInputBuffer(CgsGui::CgsGuiModuleIO::InputBuffer* lpBuffer)
        {
            mpGuiEventInputBuffer = lpBuffer;
        }

        // The module's GUI OUT event queue for this frame (the flow states' channel-40
        // command records and the module's own out events). BrnGameModule::BridgeGuiToGame
        // drains it after the module update -- the PC stand-in for the console GUI module
        // OUTPUT buffer's out-event queue the bridge reads. Cleared by the bridge.
        CgsModule::VariableEventQueue<18432, 16>* GetGuiOutQueue() { return &mGuiOutQueue; }

        // The always-available GUI components manager the module owns lives at a far
        // offset in a part of the GuiModule layout this slice does not yet model; it is
        // reached BY NAME through the free accessor GetAlwaysAvailableComponentsManager()
        // (bodied in BrnGuiModule.cpp). See KU_OFF_AAC_MANAGER.

    public:
        // X360 byte offset of the always-available components manager within GuiModule
        // (mpGuiModule + 0x17D670; used by the free GetAlwaysAvailableComponentsManager()
        // accessor in BrnGuiModule.cpp). [FLAG -- uncommitted-layout offset; becomes a real
        // named member once the full GuiModule view/components block is reconstructed.]
        static const u32 KU_OFF_AAC_MANAGER = 0x17D670;

    private:
        // Dispatch this sub-step's inbound GUI events (the real GuiModule::Update event
        // switch @0x82527A58: 144 -> RunFsm, 481 -> HandleHudStateLoadComplete + forward,
        // 14/16 -> the model-out notification queue, 504/508/513 -> MovieManager), and fan
        // every event the flow subscribed to (records 34/35) into the HUD flow's in-queue
        // -- the EventInterpreterModule observer dispatch.
        void DispatchInboundGuiEvents();

        // Drain one flow's StateInterface output queue -- the per-frame dispatch point
        // for everything its states post: subscription records (34/35, keyed into that
        // flow's observed-id table), movie play/stop (508/509), the channel-40 command
        // records (-> mGuiOutQueue for the game bridge), the channel-41 view-state
        // records (-> the view input queue), and the menu-music / audio-trigger events
        // (155/201, the PC sound leaves).
        void DrainFlowOutputQueue(s32 liFlow);

        // [PC IO] the GuiResourceModule module-dispatch stand-in: drain the ModelIO input
        // buffer's FSM-bundle load requests (GuiEventLoadRequest, queue type 39), load
        // "FSM/<NAME>.BUNDLE" synchronously, and post the GuiEventLoadNotification (14)
        // into the ModelIO output buffer's notification queue the controller reads.
        void ServiceFsmBundleRequests();

        // Post one event into each live flow's in-queue if that flow subscribed to it
        // (the observer-subscription filter, per flow).
        void RouteEventToFlow(const CgsModule::Event* lpEvent, s32 liId, s32 liSize);

        // This sub-step's GUI module INPUT buffer (set by BrnGameModule each sub-step; the
        // buffer itself lives on the update IO stack and is re-created per sub-step).
        CgsGui::CgsGuiModuleIO::InputBuffer* mpGuiEventInputBuffer;

        ViewModule mViewModule;       // DecFIGS BrnGuiModule.h:441 (owns Apt/text/render state)

        // The view-module IO pair the per-frame bridge fills (the input buffer carries the
        // view-state events -- frame time step 26, the play-movie events 18, the load
        // notifications 14 -- into CgsGui::ViewModule::Update). FLAG (bridge stand-in):
        // the console fills these through the module scheduler's IO stacks.
        CgsGui::ViewIO::InputBuffer  mViewInputBuffer;
        CgsGui::ViewIO::OutputBuffer mViewOutputBuffer;
        s64 miLastViewFrameMs;        // PC frame clock for the time-step event (FLAG: wall clock)
        MovieManager mMovieManager;   // X360 +301600 (drives the boot/attract videos)
        AptRuntimeHost mAptRuntimeHost; // GUI-owned Apt host published while the module is prepared

        // ---- the real flow-controller chain (X360 GuiModule members) --------------------
        GuiCache          mGuiCache;        // X360 +1005376 (the flow states' cache; event-64 payload)
        BrnScreenFlow     mScreenFlow;      // X360 mScreenFlow (SCREEN = E_GUIFLOW_SCREEN, the front-end)
        BrnHudFlow        mHudFlow;         // X360 +638904-adjacent flow set (HUD = E_GUIFLOW_HUD)
        BrnOverlayFlow    mOverlayFlow;     // X360 mOverlayFlow (OVERLAY = E_GUIFLOW_OVERLAY)
        GuiFsmController  mFsmController;   // X360 +638904 (the flow FSM controller)
        ProfileManager    mProfileManager;  // X360 +681696 (the REAL save/load manager)
        CgsGui::SystemUserProfile mSystemUserProfile; // X360 +949152 (sign-in watcher the manager listens to)
        CgsMemory::HeapMalloc  mFsmLuaHeap;    // the FSM Lua VM heap (the controller's allocator)
        CgsMemory::HeapMalloc   mProfileHeap;   // the manager's heap (mugshot buffer + SLS callbacks)
        CgsMemory::LinearMalloc mProfileLinear; // the manager's linear (SLS Prepare)
        CgsMemory::LinearMalloc mHudStatePool; // the 14-state pool allocator (HudFlow::Prepare)
        CgsMemory::LinearMalloc mOverlayStatePool; // the 15-popup-state pool allocator (OverlayFlow::Prepare)
        CgsMemory::LinearMalloc mScreenStatePool;  // the 61-state pool allocator (ScreenFlow::Prepare)

        // The ModelIO pair the controller exchanges with the (module-dispatch) resource
        // loader: requests out through the input buffer, notifications back on the output
        // buffer. On the console these are per-frame stack buffers ("GUIModel"); the PC
        // module owns a persistent pair and clears the queues per frame as the real
        // Update does (Clear @ the update tail).
        CgsGui::ModelIO::InputBuffer  mModelInputBuffer;
        CgsGui::ModelIO::OutputBuffer mModelOutputBuffer;

        // The per-flow in-queues (every observed event is fanned into the subscribing
        // flow's queue) + the module GUI-OUT queue the game bridge drains.
        CgsModule::VariableEventQueue<18432, 16> mScreenInQueue;
        CgsModule::VariableEventQueue<18432, 16> mHudInQueue;
        CgsModule::VariableEventQueue<18432, 16> mOverlayInQueue;
        CgsModule::VariableEventQueue<18432, 16> mGuiOutQueue;

        // The observer-subscription tables (records 34/35 from each flow's state
        // interface output queue). One flag set per flow slot.
        static const s32 KI_MAX_OBSERVED_EVENT_ID = 1024;
        bool mabObservedEventIds[E_GUIFLOW_COUNT][KI_MAX_OBSERVED_EVENT_ID];

        // One resident pool per flow slot for the loaded FSM LuaCode bundle (request ids
        // 13/14/15 = SCREEN/HUD/OVERLAY; each flow's ScriptedFsm holds its LuaCode
        // resource while live, so the pools must not alias across flows).
        CgsResource::Pool mFsmBundlePool[E_GUIFLOW_COUNT];
        bool              mbResourcesReadyFed;   // fed BF_LEGAL its resources-ready (567) yet
    };

    // Renderer bridge, matching gpActiveMovieManager: GuiModule publishes itself while
    // prepared; BrnRendererModule drives GuiModule::Render (the GUI render chain) through it.
    extern GuiModule* gpActiveGuiModule;

    // The current menu-music stream hash (X360 dword_830082A8): the last hash posted on
    // the menu-music channel; 0 == silence. Read by the post-title intro handoff.
    extern s32 gCurrentMenuMusicHash;
}

#endif
