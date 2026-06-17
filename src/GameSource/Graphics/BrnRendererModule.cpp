#include "GameSource/Graphics/BrnRendererModule.h"
#include "pc/gcm/renderengine/device.h"   // renderengine::Device frame bracket
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"  // CgsDev::DebugManager (debug HUD overlay)

// Minimal constructors for the off-path placeholder types embedded in BrnRendererModule
// (Option B). The job system and the buffered dispatch frame are reconstructed with the
// threading / dispatch core; on the single-threaded loading-screen boot they carry no
// behaviour, so these definitions keep the link closed without faking functionality. They
// move to their real homes when those subsystems come online. (The EAThread RWMutex is now
// the real type, embedded in the module base's DataBuffers - no stub ctor needed.)
EA::Jobs::Job::Job(s32 /*liPriority*/) {}
CgsGraphics::BufferedDispatchFrame::BufferedDispatchFrame() {}

// Flow-state signal (MainGameFlowStateInitialLoadingScreen sets this; stands in for the
// game-module global the X360 reads). True while the loading screen should be shown.
extern bool gBrnLoadingScreenShouldShow;

// High-res frame timer (CgsTimeUtils.cpp), forward-declared - drives the thread-monitor health.
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); }

namespace
{
    u32  gu32LastMonitorTick = 0;
    bool gbMonitorTickValid  = false;

    // Submit one solid-coloured quad (4-vertex triangle strip) through the Im2d, in 1280x720 logical px.
    void EmitColouredQuad(CgsGraphics::Im2d* lpIm2d, f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, CgsGraphics::RGBA8 lColour)
    {
        CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
        const f32 laPos[4][2] = { {lfX0, lfY0}, {lfX1, lfY0}, {lfX0, lfY1}, {lfX1, lfY1} };   // TL,TR,BL,BR
        for (s32 liVertex = 0; liVertex < 4; ++liVertex)
        {
            laVerts[liVertex].mv2Pos    = { laPos[liVertex][0], laPos[liVertex][1] };
            laVerts[liVertex].mv2Tex0UV = { 0.0f, 0.0f };
            laVerts[liVertex].mv4Colour = lColour;
        }
        lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);
    }
}

// @ 0x82405A30 - BrnRendererModule::RenderThreeThreadMonitors. Three squares bottom-centre, one per
// worker thread: green when the thread is running in real time, red when it has fallen behind. The X360
// draws them via the untextured Basic2dColouredVertex renderer at normalised coords (x 0.55/0.57/0.59,
// y 0.91-0.94); reconstructed through mIm2dRenderer untextured (SetTexture(null) -> solid colour), with
// the normalised coords scaled to the 1280x720 logical space.
void BrnRendererModule::RenderThreeThreadMonitors(bool lbThread0, bool lbThread1, bool lbThread2)
{
    const f32 KF_W = 1280.0f;
    const f32 KF_H = 720.0f;
    const CgsGraphics::RGBA8 KC_GREEN = { 0, 255, 0, 255 };
    const CgsGraphics::RGBA8 KC_RED   = { 255, 0, 0, 255 };

    const f32  laLeftX[3]      = { 0.55f, 0.57f, 0.59f };   // normalised left edge; width 0.015
    const bool labThreadOk[3]  = { lbThread0, lbThread1, lbThread2 };

    mIm2dRenderer.BeginRendering();
    mIm2dRenderer.SetState(static_cast<const CgsGraphics::BlendState*>(nullptr));
    mIm2dRenderer.SetTexture(nullptr);   // untextured -> solid vertex colour
    for (s32 liThread = 0; liThread < 3; ++liThread)
    {
        const CgsGraphics::RGBA8 lColour = labThreadOk[liThread] ? KC_GREEN : KC_RED;
        EmitColouredQuad(&mIm2dRenderer,
                         laLeftX[liThread] * KF_W,           0.91f * KF_H,
                         (laLeftX[liThread] + 0.015f) * KF_W, 0.94f * KF_H, lColour);
    }
    mIm2dRenderer.EndRendering();
}

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

    // Debug HUD overlay (the on-screen perf squares) - drawn on top of the loading screen, before the
    // present. The debug manager is the BrnGameModule-owned singleton (constructed at boot); the X360
    // Render path issues this each frame between the foreground overlay and ShowPixelBuffer. RenderWorld
    // (3D) is deferred, so the view/camera args are unused; the 2D buffer is the real Im2d the loading
    // screen renders through (mIm2dRenderer).
    if (CgsDev::DebugManager* lpDebugManager = CgsDev::DebugManager::ThreadSafeAquire())
    {
        Matrix44 lViewProjection;
        lViewProjection.SetIdentity();
        Vector3 lCameraPosition;
        lCameraPosition.SetZero();
        lpDebugManager->Render(lViewProjection, lCameraPosition, nullptr, &mIm2dRenderer);
        CgsDev::DebugManager::ThreadSafeRelease(lpDebugManager);
    }

    // The three per-thread monitor squares (X360 RenderThreeThreadMonitors). The real per-thread
    // "running in real time" flags need the threading system (deferred), so they are derived here from
    // the present-to-present frame time - matching the observed behaviour (green at framerate, reddening
    // as the game/CPU slows). The X360 gates this on a debug-display flag.
    {
        const u32 lu32Now  = CgsSystem::GetSystemTimerBaseTime();
        const u32 lu32Freq = CgsSystem::GetSystemTimerFrequency();
        f32 lfFrameMs = 0.0f;
        if (gbMonitorTickValid && lu32Freq != 0u)
            lfFrameMs = static_cast<f32>(static_cast<double>(lu32Now - gu32LastMonitorTick) * 1000.0 / static_cast<double>(lu32Freq));
        gu32LastMonitorTick = lu32Now;
        gbMonitorTickValid  = true;

        const f32 lfBudgetMs = 1000.0f / 60.0f;
        s32 liBehind = 0;
        if (lfFrameMs > lfBudgetMs * 1.10f) liBehind = 1;
        if (lfFrameMs > lfBudgetMs * 1.50f) liBehind = 2;
        if (lfFrameMs > lfBudgetMs * 2.00f) liBehind = 3;
        RenderThreeThreadMonitors(liBehind < 3, liBehind < 2, liBehind < 1);
    }

    renderengine::Device::ShowPixelBuffer();
}

// Renders the on-screen assert overlay (forwarded from BrnGameModule::RenderAssert). The real
// body draws the assert text via the immediate-mode renderer; minimal until the assert overlay
// path is reconstructed (asserts are inert on the boot/loading path).
void BrnRendererModule::RenderAssert(const AssertData* /*lpAssertData*/)
{
}
