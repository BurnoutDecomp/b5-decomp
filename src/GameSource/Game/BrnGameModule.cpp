#include "GameSource/Game/BrnGameModule.hpp"

namespace BrnGame
{
    BrnGameModule::BrnGameModule()
    {
    }

    BrnGameModule::~BrnGameModule()
    {
    }

    // @ 0x823C9EA8 - construct the game's modules. Loading-screen path: the renderer
    // module (which in turn constructs the loading-screen renderer). The other modules
    // (physics / AI / world / GUI / replay) are reconstructed incrementally; none of them
    // runs while the loading screen is shown.
    void BrnGameModule::Construct()
    {
        mRendererModule.Construct();

        // Enter the initial game-flow state, which flags the loading screen active; the
        // renderer module reads that flag each frame and shows the loading-screen renderer.
        // (The full GameMainFlowController drives state entry/transitions in the real game.)
        mInitialLoadingScreenState.OnEnter();
    }

    // @ 0x823A8B40 - the render/dispatch thread body: render one frame through the renderer
    // module. The threaded D3D thread-ownership handshake and the replay-module dispatch
    // are reconstructed with the threading core; for the single-threaded boot this simply
    // drives one render.
    void BrnGameModule::DispatchThread()
    {
        mRendererModule.Render();
    }
}
