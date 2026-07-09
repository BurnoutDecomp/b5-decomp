#ifndef BRN_GUI_MODULE_H
#define BRN_GUI_MODULE_H

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"      // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"  // CgsGui::ViewIO Input/OutputBuffer (the per-frame bridge pair)
#include "GameSource/Gui/BrnGuiMovieManager.h"                          // BrnGui::MovieManager (embedded)
#include "GameSource/Gui/BrnGuiAptRuntime.h"                             // BrnGui::AptRuntimeHost (embedded)
#include "GameSource/Gui/BrnGuiViewModule.h"                             // BrnGui::ViewModule (embedded)
#include "GameSource/Gui/Flow/HUD/States/BrnBootVideos.h"               // BrnGui::BootVideos (the boot-logo state)
#include "GameSource/Gui/Flow/HUD/States/BrnBootLoading.h"              // BrnGui::BootLoading (the boot loading state)
#include "GameSource/Gui/Flow/HUD/States/BrnBootLegal.h"                // BrnGui::BootLegal (the boot legal/title-screen state)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"// CgsGui::StateInterface (BootVideos' channel)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // CgsModule::VariableEventQueue (BootVideos' in-queue)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateMachine.h"  // CgsGui::StateMachine (runs the boot FSM Lua script)
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"     // CgsResource::Pool (holds the loaded FSM bundle)
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"               // CgsMemory::HeapMalloc (the boot FSM Lua VM heap)

// BrnGui::GuiModule -- the GUI module (a dispatched CgsModule, like BrnRendererModule). The X360 module
// (Construct 0x82518028 / Prepare 0x82518D68 / Update 0x82527A58 / Render 0x825146B8) builds the entire
// GUI subsystem (model + view + the HUD/Screen flows) and EMBEDS the MovieManager (X360 +301600), driving
// it each frame via UpdateAndRenderMovieManager (0x82511240: MovieManager::Update + MoviePlayer::Render
// through the ViewIO ImRenderers).
//
// [MINIMAL MOVIE-HOSTING SLICE] This reconstructs only the slice that hosts + drives the MovieManager (the
// movie-relevant part of GuiModule); the in-game GUI model/view/APT + the full HUD-flow sequencing are
// data-gated follow-ons (the established pattern for big modules). The boot HUD flow (BrnHudFlow ->
// BootVideos) that fires the play-video events is reconstructed in the next phases and driven from Update;
// the movie frame is presented through the renderer's existing gpActiveMovieManager draw hook (the X360
// renders it through the GUI's own ViewIO ImRenderers -- a follow-on once the GUI view path is reconstructed).
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

        // The always-available GUI components manager the module owns (the in-game EATrax
        // banner / achievement pop-up / save-icon / etc.) lives at a far offset in a part of
        // the GuiModule layout this MINIMAL movie-hosting slice does not yet model. It is
        // reached BY NAME through the free accessor GetAlwaysAvailableComponentsManager()
        // declared in BrnGuiAlwaysAvailableComponentsManager.h (bodied in BrnGuiModule.cpp),
        // which encapsulates the X360-attested byte offset there. See KU_OFF_AAC_MANAGER.

    public:
        // X360 byte offset of the always-available components manager within GuiModule
        // (mpGuiModule + 0x17D670; used by the free GetAlwaysAvailableComponentsManager()
        // accessor in BrnGuiModule.cpp). [FLAG -- uncommitted-layout offset; becomes a real
        // named member once the full GuiModule view/components block is reconstructed.]
        static const u32 KU_OFF_AAC_MANAGER = 0x17D670;

    private:
        // Route the boot HUD flow <-> the MovieManager for one frame: feed BootVideos its input events,
        // tick it, deliver its play/stop output to the manager, tick the manager, and feed video-finished
        // back. [MINIMAL boot driver -- the X360 runs BootVideos inside BrnHudFlow + routes via EventObserver;
        // here the GuiModule drives the single boot state + bridges the queues, marked.]
        void UpdateBootVideoFlow();

        ViewModule mViewModule;       // DecFIGS BrnGuiModule.h:441 (owns Apt/text/render state)

        // The view-module IO pair the per-frame bridge fills (the GuiFsmController /
        // BridgeFromInputToView stand-in): the input buffer carries the view-state
        // events (frame time step 26 + the play-movie events 18) into
        // CgsGui::ViewModule::Update; the output buffer receives the view's outbound
        // events. FLAG (bridge stand-in): the console fills these through the module
        // scheduler's IO stacks.
        CgsGui::ViewIO::InputBuffer  mViewInputBuffer;
        CgsGui::ViewIO::OutputBuffer mViewOutputBuffer;
        s64 miLastViewFrameMs;        // PC frame clock for the time-step event (FLAG: wall clock)
        MovieManager mMovieManager;   // X360 +301600 (drives the boot/attract videos)
        AptRuntimeHost mAptRuntimeHost; // GUI-owned Apt host published while the module is prepared

        // The boot-logo flow this module drives (the in-game GUI model/view/full HudFlow is data-gated).
        BootVideos             mBootVideos;
        CgsGui::StateInterface mBootStateInterface;                  // BootVideos' output channel
        CgsModule::VariableEventQueue<18432, 16> mBootInQueue;       // BootVideos' input queue (cache-ready/video-finished)
        bool                   mbBootStarted;                        // fed the initial cache-ready event yet?
        bool                   mbLoadingHasShown;                    // the initial loading screen has been displayed

        // Boot through the REAL Lua FSM: a CgsGui::StateMachine runs the BRNVIDEOFSM Lua script (loaded
        // from FSM/BRNVIDEOFSM.BUNDLE) which SetStates() the BF_VIDEOS state -> mBootVideos. This is the
        // first slice of the faithful flow (the X360 runs this inside BrnHudFlow + the GuiFsmController,
        // sequenced via ModelIO; see [[lua-system]]). If the bundle/script fails to come up, the module
        // falls back to driving mBootVideos directly (mbBootFsmReady=false) so the boot videos still play.
        CgsGui::StateMachine   mBootStateMachine;                    // runs the BRNVIDEOFSM Lua FSM
        CgsResource::Pool      mBootFsmPool;                         // holds the loaded FSM LuaCode bundle
        CgsMemory::HeapMalloc  mBootLuaHeap;                         // backing heap for the boot FSM Lua VMs (shared)
        bool                   mbBootFsmReady;                       // the Lua FSM loaded + entered BF_VIDEOS

        // Boot-phase sequencer: the boot runs BF_LOADING (BRNFLOADFSM) then BF_VIDEOS (BRNVIDEOFSM), each a
        // single-state FSM in its own StateMachine, advancing at loading-complete (the PC stand-in for the
        // X360 GuiFsmController, which sequences these via ModelIO; see [[lua-system]]). BF_LOADING runs the
        // real BootLoading state through its Lua script; its show/stop-loading-screen events are emitted but
        // not yet routed to the visual (the game-flow loading screen drives that) -- FLAG, routing is a
        // follow-on. Phase 0 = BF_LOADING, phase 1 = BF_VIDEOS.
        BootLoading            mBootLoading;                         // BF_LOADING state
        CgsGui::StateMachine   mBootLoadingStateMachine;             // runs the BRNFLOADFSM Lua FSM
        CgsGui::StateInterface mBootLoadingStateInterface;           // BootLoading's output channel
        CgsModule::VariableEventQueue<18432, 16> mBootLoadingInQueue;// BootLoading's input queue (cache-ready/loading-complete)
        CgsResource::Pool      mBootLoadingPool;                     // holds the BRNFLOADFSM LuaCode bundle
        bool                   mbBootLoadingFsmReady;                // the BRNFLOADFSM Lua FSM loaded + entered BF_LOADING
        bool                   mbBootLoadingCompleteFed;             // fed BootLoading the loading-complete (137) input yet

        // Phase 2 = BF_LEGAL: BootLegal (the legal / title screen that shows title_screen02) run through
        // the BRNLEGALFSM Lua FSM, loaded when BF_VIDEOS signals "done" (the PC stand-in for the X360
        // GuiFsmController loading the next single-state FSM on the boot-videos-done state event).
        BootLegal              mBootLegal;                           // BF_LEGAL state
        CgsGui::StateMachine   mBootLegalStateMachine;               // runs the BRNLEGALFSM Lua FSM
        CgsGui::StateInterface mBootLegalStateInterface;             // BootLegal's output channel
        CgsModule::VariableEventQueue<18432, 16> mBootLegalInQueue;  // BootLegal's input queue
        CgsResource::Pool      mBootLegalPool;                       // holds the BRNLEGALFSM LuaCode bundle
        bool                   mbBootLegalFsmReady;                  // the BRNLEGALFSM Lua FSM loaded + entered BF_LEGAL
        s32                    miBootPhase;                          // 0 = BF_LOADING, 1 = BF_VIDEOS, 2 = BF_LEGAL
    };

    // Renderer bridge, matching gpActiveMovieManager: GuiModule publishes itself while
    // prepared; BrnRendererModule drives GuiModule::Render (the GUI render chain) through it.
    extern GuiModule* gpActiveGuiModule;
}

#endif
