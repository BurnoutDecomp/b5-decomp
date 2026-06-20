#ifndef BRN_GUI_MODULE_H
#define BRN_GUI_MODULE_H

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"      // CgsModule::ModuleSingleBuffered base
#include "GameSource/Gui/BrnGuiMovieManager.h"                          // BrnGui::MovieManager (embedded)
#include "GameSource/Gui/Flow/HUD/States/BrnBootVideos.h"               // BrnGui::BootVideos (the boot-logo state)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"// CgsGui::StateInterface (BootVideos' channel)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // CgsModule::VariableEventQueue (BootVideos' in-queue)

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
    };
}

#endif
