#ifndef BRN_GUI_MODULE_H
#define BRN_GUI_MODULE_H

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"      // CgsModule::ModuleSingleBuffered base
#include "GameSource/Gui/BrnGuiMovieManager.h"                          // BrnGui::MovieManager (embedded)
#include "GameSource/Gui/Flow/HUD/States/BrnBootVideos.h"               // BrnGui::BootVideos (the boot-logo state)
#include "GameSource/Gui/Flow/HUD/States/BrnBootLoading.h"              // BrnGui::BootLoading (the boot loading state)
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

        MovieManager* GetMovieManager() { return &mMovieManager; }

    private:
        // Route the boot HUD flow <-> the MovieManager for one frame: feed BootVideos its input events,
        // tick it, deliver its play/stop output to the manager, tick the manager, and feed video-finished
        // back. [MINIMAL boot driver -- the X360 runs BootVideos inside BrnHudFlow + routes via EventObserver;
        // here the GuiModule drives the single boot state + bridges the queues, marked.]
        void UpdateBootVideoFlow();

        MovieManager mMovieManager;   // X360 +301600 (drives the boot/attract videos)

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
        s32                    miBootPhase;                          // 0 = BF_LOADING, 1 = BF_VIDEOS
    };
}

#endif
