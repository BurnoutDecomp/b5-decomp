#include "GameSource/Graphics/BrnRendererModule.h"
#include "pc/gcm/renderengine/device.h"   // renderengine::Device frame bracket

// Minimal constructors for the off-path placeholder types embedded in BrnRendererModule
// (Option B). The real EAThread RWMutex, the job system, and the buffered dispatch frame
// are reconstructed with the threading / dispatch core; on the single-threaded loading-
// screen boot they carry no behaviour, so these definitions keep the link closed without
// faking functionality. They move to their real homes when those subsystems come online.
EA::Thread::RWMutex::RWMutex(const char* /*lpcName*/, bool /*lbIntraProcess*/) {}
EA::Jobs::Job::Job(s32 /*liPriority*/) {}
CgsGraphics::BufferedDispatchFrame::BufferedDispatchFrame() {}

// Flow-state signal (MainGameFlowStateInitialLoadingScreen sets this; stands in for the
// game-module global the X360 reads). True while the loading screen should be shown.
extern bool gBrnLoadingScreenShouldShow;

// @ 0x8240A778 - BrnRendererModule::Construct. Reconstructed from the X360 ARTIST build.
//
// Option B (layout-faithful incremental): the loading-screen render path is reconstructed
// for real here - the double-buffered shader-constant frames and the loading-screen
// renderer, which is what actually draws during boot. The remaining subsystems the full
// Construct builds (effects arbitrator, dispatch frames, the Im2d/Im3d family, render-
// target memory, corona/postfx/occlusion/shadow/sun managers) are held as opaque storage
// and their construction is reconstructed incrementally; none of them draws during the
// loading screen, so the screen boots through the real module without them.
void BrnRendererModule::Construct()
{
    // Double-buffered per-frame shader constants (maShaderConstantsFrames[2]).
    maShaderConstantsFrames[0].Construct();
    maShaderConstantsFrames[1].Construct();

    // The loading-screen renderer (creates its textures + scratch buffer, picks language).
    mLoadingScreenRenderer.Construct();
}

// @ 0x8240BFA8 - BrnRendererModule::Render. Reconstructed from the X360 ARTIST build.
//
// The full Render walks the whole frame (shadow maps, env map, world/car opaque +
// transparent, sky, coronas, particles, post-fx, MSAA resolve) and finishes with the
// loading-screen overlay and the present. During boot none of the world systems have
// data, so those passes are data-gated off; Option B reconstructs the part that actually
// runs - frame begin, the loading-screen foreground overlay, and the present. The gameplay
// passes are reconstructed incrementally as their subsystems come online.
void BrnRendererModule::Render()
{
    if (!renderengine::Device::FrameBegin())
    {
        return;
    }

    // (gameplay-render passes here when reconstructed; gated off during the loading screen)

    // Mirror the flow state's loading-screen signal into the renderer's command queue -
    // the X360 Render issues this state-driven AddCommand each frame (Render line ~344).
    if (gBrnLoadingScreenShouldShow && !mLoadingScreenRenderer.IsRenderingLoadingScreen())
    {
        mLoadingScreenRenderer.AddCommand(BrnGame::E_LSC_SHOW);
    }
    else if (!gBrnLoadingScreenShouldShow && mLoadingScreenRenderer.IsRenderingLoadingScreen())
    {
        mLoadingScreenRenderer.AddCommand(BrnGame::E_LSC_HIDE);
    }

    mLoadingScreenRenderer.RenderForeground(&mIm2dRenderer);

    renderengine::Device::ShowPixelBuffer();
}

// Renders the on-screen assert overlay (forwarded from BrnGameModule::RenderAssert). The real
// body draws the assert text via the immediate-mode renderer; minimal until the assert overlay
// path is reconstructed (asserts are inert on the boot/loading path).
void BrnRendererModule::RenderAssert(const AssertData* /*lpAssertData*/)
{
}
