#pragma once

#include "GameSource/Graphics/BrnRendererModule.h"
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h"

// BrnGame::BrnGameModule - the top-level game module that owns and drives every engine
// module (renderer, physics, AI, world, GUI, replay, ...). Reconstructed from the X360
// ARTIST build. Option B (loading-screen path): the renderer module - which owns and
// drives the loading-screen renderer - is real here; the remaining modules are
// reconstructed incrementally and are not exercised while the loading screen is up.
namespace BrnGame
{
    class BrnGameModule
    {
    public:
        BrnGameModule();
        ~BrnGameModule();

        void Construct();       // @ 0x823C9EA8 - construct the owned modules
        void DispatchThread();  // @ 0x823A8B40 - render-thread body: render one frame

    private:
        BrnRendererModule mRendererModule;
        // The top-level game-flow state. In the full engine the GameMainFlowController owns
        // and transitions the flow states; here the game module enters the initial
        // loading-screen state directly (the controller is reconstructed incrementally).
        MainGameFlowStateInitialLoadingScreen mInitialLoadingScreenState;
    };
}
