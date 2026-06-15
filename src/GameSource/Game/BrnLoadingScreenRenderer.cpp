#include "GameSource/Game/BrnLoadingScreenRenderer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The loading-screen renderer animates the
// loading visuals on its own render path. AddCommand/RenderBackground/RenderForeground
// are faithful to the pseudocode; RenderBlackOverlay's quad/transform math was emitted
// as hand-written PPC VMX (Hex-Rays failed to lift it) and is reconstructed here as the
// equivalent clean logical C++ (a fade-in/out full-screen black overlay quad);
// SetupLoadingScreenTexture is the platform create-from-pixels path expressed through
// the renderengine texture API. Member offsets were reconciled against the pseudocode
// (DWARF order is not offset-authoritative): muLastTime@24, mbRenderInBackground@48,
// mfBlackOverlayFade@52, mbKillBlackOverlayWhenDone@56, mfTimeStep@88, etc.

// Platform high-res timer (CgsTimeUtils.cpp) and Xbox memcpy.
namespace CgsSystem
{
    u32 GetSystemTimerBaseTime();
    u32 GetSystemTimerFrequency();
}
extern "C" void* XMemCpy(void* lpDst, const void* lpSrc, u32 luSize);

namespace CgsGraphics { class BlendState; }
// Render states the overlay installs before drawing (X360 .rdata globals @0x83010F20
// etc.; resolved at link time).
extern CgsGraphics::BlendState* const gpOverlayBlendState;

// .rdata animation/layout constants (values live in .rdata on the X360, resolved at
// link time; addresses noted). KF_OVERLAY_FADE_* are per-step fade rates. Declared with
// external linkage so the per-TU compile gate is satisfied without inventing values.
extern const f32 KF_OVERLAY_FADE_OUT_RATE;  // flt_82F2411C
extern const f32 KF_OVERLAY_FADE_IN_RATE;   // flt_82F24120
extern const f32 KF_OVERLAY_HEIGHT_SCALE;   // flt_82F24080
extern const f32 KF_OVERLAY_UV_U;           // flt_82FAE59C
extern const f32 KF_OVERLAY_UV_V;           // flt_82FAE5AC

namespace BrnGame
{
    // @ 0x823AAD48 - external command interface: map a command id onto the renderer's
    // visibility / black-overlay state.
    void LoadingScreenRenderer::AddCommand(s32 liCommand)
    {
        switch (liCommand)
        {
        case 1:   // show (foreground)
            mbVisible = true;
            mbHiding = false;
            mbRenderInBackground = false;
            mbKillBlackOverlayWhenDone = true;
            break;
        case 2:   // begin hiding
            mbHiding = true;
            break;
        case 3:   // show in the background
            mbRenderInBackground = true;
            mbVisible = true;
            mbHiding = false;
            mbKillBlackOverlayWhenDone = true;
            break;
        case 4:   // black overlay: start fading out
            mfBlackOverlayFade = 1.2f;
            mbBlackOverlayVisible = true;
            mbBlackOverlayHiding = true;
            mbKillBlackOverlayWhenDone = false;
            break;
        case 5:   // black overlay: start fading in
            mfBlackOverlayFade = 0.0f;
            mbBlackOverlayHiding = false;
            mbBlackOverlayVisible = true;
            mbKillBlackOverlayWhenDone = false;
            break;
        }
    }

    // @ 0x823EDEB8 - draw the loading visuals when running as a background layer.
    void LoadingScreenRenderer::RenderBackground()
    {
        if (mbRenderInBackground && !mbBlackOverlayVisible)
        {
            Render();
        }
    }

    // @ 0x823EDE18 - foreground path: advance the frame timestep, draw the visuals (if
    // not backgrounded and no overlay), then update the black overlay.
    void LoadingScreenRenderer::RenderForeground(CgsGraphics::Im2d* lpIm2d)
    {
        const u32 luNow = CgsSystem::GetSystemTimerBaseTime();
        const u32 luDelta = luNow - static_cast<u32>(muLastTime);
        muLastTime = luNow;

        const u32 luFrequency = CgsSystem::GetSystemTimerFrequency();
        f32 lfTimeStep = static_cast<f32>((10000u * luDelta) / luFrequency) * 0.0001f;
        if (lfTimeStep > 0.1f)
        {
            lfTimeStep = 0.1f;
        }
        mfTimeStep = lfTimeStep;

        if (!mbRenderInBackground && !mbBlackOverlayVisible)
        {
            Render();
        }
        RenderBlackOverlay(lpIm2d);
    }

    // @ 0x823E8750 - the fading full-screen black overlay. The X360 built the quad
    // transform/vertices with inline VMX; this is the equivalent logical reconstruction.
    void LoadingScreenRenderer::RenderBlackOverlay(CgsGraphics::Im2d* lpIm2d)
    {
        using namespace CgsGraphics;

        if (!mbBlackOverlayVisible)
        {
            return;
        }

        lpIm2d->BeginRendering();

        Im2dTransform lTransform;
        lTransform.TransformByAspectRatio();
        lpIm2d->SetTransform(lTransform);

        // Install the overlay's render state (alpha-blend over the framebuffer).
        lpIm2d->SetState(gpOverlayBlendState);

        // Advance the overlay fade and decide whether this is its final frame.
        bool lbHideAfterDraw = false;
        if (mbBlackOverlayHiding)
        {
            mfBlackOverlayFade -= mfTimeStep * KF_OVERLAY_FADE_OUT_RATE;
            if (mfBlackOverlayFade < 0.0f)
            {
                mfBlackOverlayFade = 0.0f;
                mbBlackOverlayHiding = false;
                lbHideAfterDraw = true;
            }
        }
        else
        {
            mfBlackOverlayFade += mfTimeStep * KF_OVERLAY_FADE_IN_RATE;
            if (mfBlackOverlayFade >= 3.0f && mbKillBlackOverlayWhenDone)
            {
                lbHideAfterDraw = true;
            }
        }
        if (lbHideAfterDraw)
        {
            mbBlackOverlayVisible = false;
        }

        // Alpha ramps with the fade, clamped to [0, 1] -> [0, 255].
        f32 lfAlpha = mfBlackOverlayFade;
        if (lfAlpha > 1.0f) { lfAlpha = 1.0f; }
        if (lfAlpha < 0.0f) { lfAlpha = 0.0f; }
        const u8 luAlpha = static_cast<u8>(lfAlpha * 255.0f + 0.5f);

        lpIm2d->SetTexture(mpCarTexture);

        // Full-screen quad: x in [0, 1280], y centred over the 720 area (aspect-scaled).
        const f32 lfQuadHeight = KF_OVERLAY_HEIGHT_SCALE * 720.0f * 0.00078125f;  // 1/1280
        const f32 lfTop = (720.0f - lfQuadHeight) * 0.5f;
        const f32 lfBottom = lfTop + lfQuadHeight;
        const RGBA8 lColour = { 0, 0, 0, luAlpha };

        Basic2dColouredTexturedVertex laVertices[4];
        laVertices[0].mv2Pos = { 0.0f, lfTop };
        laVertices[0].mv4Colour = lColour;
        laVertices[0].mv2Tex0UV = { 0.0f, KF_OVERLAY_UV_V };
        laVertices[1].mv2Pos = { 1280.0f, lfTop };
        laVertices[1].mv4Colour = lColour;
        laVertices[1].mv2Tex0UV = { KF_OVERLAY_UV_U, KF_OVERLAY_UV_V };
        laVertices[2].mv2Pos = { 0.0f, lfBottom };
        laVertices[2].mv4Colour = lColour;
        laVertices[2].mv2Tex0UV = { 0.0f, 0.0f };
        laVertices[3].mv2Pos = { 1280.0f, lfBottom };
        laVertices[3].mv4Colour = lColour;
        laVertices[3].mv2Tex0UV = { KF_OVERLAY_UV_U, 0.0f };

        lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);
        lpIm2d->EndRendering();
    }

    // @ 0x823C6B08 - create a 2D texture sized to lfWidth x lfHeight and upload the
    // supplied pixel data into it.
    renderengine::Texture* LoadingScreenRenderer::SetupLoadingScreenTexture(
        f32 lfWidth, f32 lfHeight, s32 /*liUnused0*/, s32 /*liUnused1*/, s32 /*liUnused2*/,
        const void* lpPixelData, u32 luDataSize)
    {
        renderengine::Texture2D::Parameters lParams = {};
        lParams.muWidth = static_cast<u32>(lfWidth + 0.5f);
        lParams.muHeight = static_cast<u32>(lfHeight + 0.5f);
        lParams.muDepth = 1;
        lParams.muNumLevels = 1;
        lParams.muFormat = 340;   // X360 surface format

        renderengine::Texture2D::ResourceDescriptor lDescriptor;
        renderengine::Texture2D::GetResourceDescriptor(&lDescriptor, &lParams);
        renderengine::Texture* lpTexture = renderengine::Texture2D::Initialize(&lDescriptor, &lParams);

        renderengine::Texture::LockInfo lLockInfo;
        renderengine::Texture::Lock(lpTexture, 0, 0, 0, &lLockInfo);
        XMemCpy(lLockInfo.mpBits, lpPixelData, luDataSize);
        renderengine::Texture::Unlock(lpTexture, &lLockInfo);

        return lpTexture;
    }
}
