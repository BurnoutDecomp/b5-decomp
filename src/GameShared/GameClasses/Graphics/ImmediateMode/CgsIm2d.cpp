// <d3d9.h> needs winuser (LPMSG), but a transitive include below (rw/core/debug/DebugCriticalSection.h
// via the renderengine/Im2d chain) defines NOUSER/NOGDI ahead of its own <windows.h>, which strips
// winuser. Bring the full <Windows.h> in FIRST so LPMSG is defined before any NOUSER guard runs --
// the same ordering guard CgsImRenderBufferTemplate.cpp uses for the shared 2D draw path.
#include <Windows.h>

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (batch-capacity guards)
#include "pc/gcm/renderengine/device.h"        // gDevice, gDisplayWidth/Height
#include "pc/gcm/renderengine/texture.h"       // Texture::mpD3DTexture
#include "pc/gcm/renderengine/renderstates.h"  // TextureState::mpRaster (SetState(TextureState*))

#include <d3d9.h>
#include <cstdio>   // [diag] BRN_IM2D_TRACE line formatting

// PC / D3D9 implementation of the CgsGraphics immediate-mode 2D renderer. The X360/PS3
// Im2d batches into platform command buffers via vertex descriptors + program buffers;
// the PC backend draws each batch directly with DrawPrimitiveUP. Screen-space vertices
// (the engine works in a 1280x720 logical space) are scaled to the actual back buffer
// and submitted pre-transformed (D3DFVF_XYZRHW).

// [diag] BRN_IM2D_TRACE: per-draw attribution trace (throttled to every 60th present).
// The present counter lives in device.cpp; the last-bound texture is tracked below.
namespace renderengine { extern u32 guPresentCount; }
namespace CgsDev { namespace Log { void WriteToLog(const char*); } }

namespace
{
    const void* gpIm2dTraceLastTexture = nullptr;

    bool Im2dTraceEnabled()
    {
        static int siState = -1;
        if (siState < 0)
        {
            char lacBuf[8];
            siState = (GetEnvironmentVariableA("BRN_IM2D_TRACE", lacBuf, sizeof(lacBuf)) > 0) ? 1 : 0;
        }
        return siState == 1;
    }

    struct D3DScreenVertex
    {
        float x, y, z, rhw;
        DWORD color;
        float u, v;
    };
    const DWORD KU_SCREEN_FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

    const f32 KF_LOGICAL_WIDTH  = 1280.0f;
    const f32 KF_LOGICAL_HEIGHT = 720.0f;

    // The text path submits whole-line triangle strips (6 verts/glyph); size the reserve/submit
    // scratch well above the font's KU_MAX_VERTICES (1536) so a long line never overflows.
    const u32 KU_RENDER_BUFFER_MAX = 2048;
}

namespace CgsGraphics
{
    // ---- ImRendererBase (non-template) -------------------------------------------
    void ImRendererBase::SetTexture(renderengine::Texture* lpTexture)
    {
        gpIm2dTraceLastTexture = lpTexture;   // [diag] BRN_IM2D_TRACE attribution
        if (renderengine::gDevice != nullptr)
        {
            renderengine::gDevice->SetTexture(0, lpTexture != nullptr ? lpTexture->mpD3DTexture : nullptr);
            // With a texture, modulate it by the vertex colour (the loading-screen path); with NO
            // texture, drive the stage from the vertex colour alone (SELECTARG2 = DIFFUSE) so untextured
            // prims - the debug overlay's solid squares - draw their colour regardless of the driver's
            // unbound-texture default.
            const DWORD luOp = (lpTexture != nullptr) ? D3DTOP_MODULATE : D3DTOP_SELECTARG2;
            renderengine::gDevice->SetTextureStageState(0, D3DTSS_COLOROP, luOp);
            renderengine::gDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, luOp);
        }
    }

    // The CgsGraphics state objects are opaque here; for the 2D loading-screen path
    // SetState installs the standard alpha-blend-over-framebuffer state.
    void ImRendererBase::SetState(const BlendState* /*lpState*/)
    {
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == nullptr)
        {
            return;
        }
        lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        lpDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        lpDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }

    // Bind the bitmap and modulate it by the vertex colour
    void ImRendererBase::SetState(const renderengine::TextureState* lpTextureState)
    {
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == nullptr)
        {
            return;
        }
        renderengine::Texture* lpTexture = (lpTextureState != nullptr) ? lpTextureState->mpRaster : nullptr;
        lpDevice->SetTexture(0, lpTexture != nullptr ? lpTexture->mpD3DTexture : nullptr);
        // Use bilinear filtering
        lpDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        lpDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        lpDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        lpDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    }

    // ---- ImRenderer<V> (template) ------------------------------------------------
    template <typename V>
    void ImRenderer<V>::BeginRendering()
    {
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == nullptr)
        {
            return;
        }
        lpDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        lpDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        lpDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        lpDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        lpDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        lpDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        // Frame-start mask reset: any scissor mask left by an unbalanced PushMask on the
        // previous frame is cleared (the console's mask state is per-frame too).
        lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        // Bilinear (D3D9 defaults to POINT -> blocky); no mip filtering for the 1:1 2D content.
        lpDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        lpDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        lpDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        lpDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        lpDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    }

    template <typename V>
    void ImRenderer<V>::EndRendering()
    {
    }

    template <typename V>
    bool ImRenderer<V>::SetProgram(s8 /*li8Program*/)
    {
        // FLAG PC-platform leaf: the PC 2D path uses the D3D9 fixed-function
        // vertex/texture pipeline, so the console shader slot has no host object.
        return false;
    }

    template <typename V>
    void ImRenderer<V>::Render(renderengine::PrimitiveType /*lePrimitiveType*/, const V* lpVertices, u32 luCount)
    {
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == nullptr || lpVertices == nullptr || luCount < 3u)
        {
            return;
        }

        const f32 lfScaleX = static_cast<f32>(renderengine::gDisplayWidth) / KF_LOGICAL_WIDTH;
        const f32 lfScaleY = static_cast<f32>(renderengine::gDisplayHeight) / KF_LOGICAL_HEIGHT;

        // [diag] BRN_IM2D_TRACE: log this draw's logical bounds + colour + texture every
        // 60th present so each on-screen quad can be attributed to its emitter.
        if (Im2dTraceEnabled() && (renderengine::guPresentCount % 60u) == 0u)
        {
            f32 lfMinX = lpVertices[0].mv2Pos.x, lfMaxX = lfMinX;
            f32 lfMinY = lpVertices[0].mv2Pos.y, lfMaxY = lfMinY;
            for (u32 i = 1; i < luCount; ++i)
            {
                const f32 x = lpVertices[i].mv2Pos.x, y = lpVertices[i].mv2Pos.y;
                if (x < lfMinX) lfMinX = x; if (x > lfMaxX) lfMaxX = x;
                if (y < lfMinY) lfMinY = y; if (y > lfMaxY) lfMaxY = y;
            }
            char lacMsg[192];
            std::snprintf(lacMsg, sizeof(lacMsg),
                        "[Im2dTrace] f=%u n=%u xy=(%.0f,%.0f)-(%.0f,%.0f) rgba=%02X%02X%02X%02X tex=%p\n",
                        renderengine::guPresentCount, luCount, lfMinX, lfMinY, lfMaxX, lfMaxY,
                        lpVertices[0].mv4Colour.r, lpVertices[0].mv4Colour.g,
                        lpVertices[0].mv4Colour.b, lpVertices[0].mv4Colour.a,
                        gpIm2dTraceLastTexture);
            CgsDev::Log::WriteToLog(lacMsg);
        }

        // Same capacity as the reserve/submit scratch run; an over-capacity run fires
        // the assert (nothing on the 2D path submits runs this large).
        static D3DScreenVertex saBatch[KU_RENDER_BUFFER_MAX];
        CGS_ASSERT(luCount <= KU_RENDER_BUFFER_MAX, "Im2d Render run exceeds the batch buffer");
        if (luCount > KU_RENDER_BUFFER_MAX)
        {
            luCount = KU_RENDER_BUFFER_MAX;
        }
        for (u32 i = 0; i < luCount; ++i)
        {
            saBatch[i].x = lpVertices[i].mv2Pos.x * lfScaleX;
            saBatch[i].y = lpVertices[i].mv2Pos.y * lfScaleY;
            saBatch[i].z = 0.0f;
            saBatch[i].rhw = 1.0f;
            saBatch[i].color = D3DCOLOR_ARGB(lpVertices[i].mv4Colour.a, lpVertices[i].mv4Colour.r,
                                             lpVertices[i].mv4Colour.g, lpVertices[i].mv4Colour.b);
            saBatch[i].u = lpVertices[i].mv2Tex0UV.x;
            saBatch[i].v = lpVertices[i].mv2Tex0UV.y;
        }

        lpDevice->SetFVF(KU_SCREEN_FVF);
        // The loading screen submits 4-vertex quads as triangle strips.
        lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, luCount - 2u, saBatch, sizeof(D3DScreenVertex));
    }

    // The X360 reserve/submit buffer API, folded onto the PC immediate renderer (see CgsImRenderBuffer.h).
    // RenderStart hands back a CPU scratch run the caller fills; RenderEnd submits it as one strip. The
    // text path's RenderStart/RenderEnd never nest, so a single static run per vertex type is safe here.
    template <typename V>
    V* ImRenderer<V>::RenderStart(u32 luVertexCount)
    {
        static V saScratch[KU_RENDER_BUFFER_MAX];
        (void)luVertexCount;   // (X360 asserts luVertexCount < KU_MAX_VERTICES; the run is pre-sized)
        return saScratch;
    }

    template <typename V>
    void ImRenderer<V>::RenderEnd(renderengine::PrimitiveType /*lePrimitiveType*/, const V* lpVertices, u32 luVertexCount)
    {
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == nullptr || lpVertices == nullptr || luVertexCount < 3u)
        {
            return;
        }

        const f32 lfScaleX = static_cast<f32>(renderengine::gDisplayWidth) / KF_LOGICAL_WIDTH;
        const f32 lfScaleY = static_cast<f32>(renderengine::gDisplayHeight) / KF_LOGICAL_HEIGHT;

        static D3DScreenVertex saBatch[KU_RENDER_BUFFER_MAX];
        if (luVertexCount > KU_RENDER_BUFFER_MAX)
        {
            luVertexCount = KU_RENDER_BUFFER_MAX;
        }
        for (u32 i = 0; i < luVertexCount; ++i)
        {
            saBatch[i].x = lpVertices[i].mv2Pos.x * lfScaleX;
            saBatch[i].y = lpVertices[i].mv2Pos.y * lfScaleY;
            saBatch[i].z = 0.0f;
            saBatch[i].rhw = 1.0f;
            saBatch[i].color = D3DCOLOR_ARGB(lpVertices[i].mv4Colour.a, lpVertices[i].mv4Colour.r,
                                             lpVertices[i].mv4Colour.g, lpVertices[i].mv4Colour.b);
            saBatch[i].u = lpVertices[i].mv2Tex0UV.x;
            saBatch[i].v = lpVertices[i].mv2Tex0UV.y;
        }

        lpDevice->SetFVF(KU_SCREEN_FVF);
        // One triangle strip per line: the font's glyph quads are joined by degenerate connectors.
        lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, luVertexCount - 2u, saBatch, sizeof(D3DScreenVertex));
    }

    // ---- Im2dTransform -----------------------------------------------------------
    // X360 @0x823DB448 is NOT a no-op: it folds the display aspect ratio into the transform
    // basis -- builds a Matrix33, runs rw::math Mult with aspect-correction constants, and
    // writes the corrected mOriginXYZ (+0x00) and mRightUp (+0x10) back via VMX stvx128. A
    // faithful reconstruction of that VMX matrix fold is a deferred keystone (the ledger TU
    // class:CgsGraphics::Im2dTransform stays BLOCKED). The PC Im2d backend below maps logical
    // 1280x720 coordinates straight onto the back buffer, so on THIS PC path the fold is a
    // deliberate no-op divergence -- it is NOT a reconstruction of the X360 body.
    void Im2dTransform::TransformByAspectRatio()
    {
        // PC backend: coordinates are already in back-buffer space; the X360 VMX aspect fold
        // (see note above) is intentionally not applied on this path.
    }

    // ---- Im2dBase<V> (template) --------------------------------------------------
    template <typename V>
    void Im2dBase<V>::SetTransform(const Im2dTransform& lTransform)
    {
        mCurrentTransform = lTransform;
    }

    template <typename V>
    void Im2dBase<V>::BatchTransformTextureBlendRenderStatic(
        const Im2dTransform& lrTransform,
        renderengine::Texture* lpTexture,
        const BlendState* lpBlendState,
        renderengine::PrimitiveType lePrimitiveType,
        const V* lpVertices,
        u32 luNumVertices,
        u8 /*luFlags*/)
    {
        if (lpVertices == nullptr || luNumVertices == 0)
        {
            return;
        }

        SetTransform(lrTransform);
        this->SetTexture(lpTexture);
        this->SetState(lpBlendState);

        // A Flapt mesh's vertex count is serialised as a u8 (BrnFlapt::Mesh::muNumVerts),
        // so a single batch can never exceed 255 vertices -- the fixed transform buffer
        // covers the whole range; an over-capacity run fires the assert.
        enum { KI_MAX_FLAPT_BATCH = 256 };
        V laTransformed[KI_MAX_FLAPT_BATCH];
        CGS_ASSERT(luNumVertices <= KI_MAX_FLAPT_BATCH,
                   "Flapt batch exceeds the transform buffer");
        if (luNumVertices > KI_MAX_FLAPT_BATCH)
        {
            luNumVertices = KI_MAX_FLAPT_BATCH;
        }

        for (u32 luVertex = 0; luVertex < luNumVertices; ++luVertex)
        {
            laTransformed[luVertex] = lpVertices[luVertex];

            // mRightUp lanes are {m00, m10, m01, m11} (right = (.x,.y), up = (.z,.w)) -- the
            // serialised flapt/apt raw {a,b,c,d} order, matching the Apt dispatch fold and
            // ComposeDrawTransform (see BrnFlaptMovieClipInstance.cpp: ground-truthed against
            // the FLAPTHUD save-icon spin keyframes; the transposed reading reversed every
            // flapt rotation).
            const f32 lfNdcX =
                lrTransform.mOriginXYZ.x +
                lrTransform.mRightUp.x * lpVertices[luVertex].mv2Pos.x +
                lrTransform.mRightUp.z * lpVertices[luVertex].mv2Pos.y;
            const f32 lfNdcY =
                lrTransform.mOriginXYZ.y +
                lrTransform.mRightUp.y * lpVertices[luVertex].mv2Pos.x +
                lrTransform.mRightUp.w * lpVertices[luVertex].mv2Pos.y;

            // FLAG PC-platform leaf: ImRenderer::Render consumes the engine's
            // 1280x720 logical coordinates, while the console command carries NDC.
            laTransformed[luVertex].mv2Pos.x = (lfNdcX + 1.0f) * 640.0f;
            laTransformed[luVertex].mv2Pos.y = (1.0f - lfNdcY) * 360.0f;
        }

        this->Render(lePrimitiveType, laTransformed, luNumVertices);
    }

    template <typename V>
    void Im2dBase<V>::PushMask(renderengine::Texture* lpTexture, V* lpaMaskVertices)
    {
        if (lpaMaskVertices == nullptr)
        {
            return;
        }

        // FLAG PC-platform leaf: materialise the console's two-corner stencil mask as a
        // D3D9 scissor rectangle (an axis-aligned approximation -- the console masks by
        // stencil texture; Flapt masks are axis-aligned quads, so the rect is exact for
        // them). The corners arrive in the 1280x720 logical space, so scale to the back
        // buffer like every draw on this path. PopMask (below) disables the scissor;
        // BeginRendering also clears it at frame start.
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == nullptr)
        {
            return;
        }

        const f32 lfScaleX = static_cast<f32>(renderengine::gDisplayWidth) / KF_LOGICAL_WIDTH;
        const f32 lfScaleY = static_cast<f32>(renderengine::gDisplayHeight) / KF_LOGICAL_HEIGHT;

        RECT lRect;
        lRect.left = static_cast<LONG>(lpaMaskVertices[0].mv2Pos.x * lfScaleX);
        lRect.top = static_cast<LONG>(lpaMaskVertices[0].mv2Pos.y * lfScaleY);
        lRect.right = static_cast<LONG>(lpaMaskVertices[1].mv2Pos.x * lfScaleX);
        lRect.bottom = static_cast<LONG>(lpaMaskVertices[1].mv2Pos.y * lfScaleY);
        lpDevice->SetScissorRect(&lRect);
        lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        this->SetTexture(lpTexture);
    }

    // FLAG PC-platform leaf: the pop side of the scissor mask (the console's
    // ImCommandPopMask). Driven by the Flapt mask path once FlaptRenderer::PopMask is
    // homed; BeginRendering's frame-start reset bounds any unbalanced push meanwhile.
    template <typename V>
    void Im2dBase<V>::PopMask()
    {
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == nullptr)
        {
            return;
        }
        lpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    }

    // Instantiate the coloured+textured 2D renderer the loading screen uses.
    template struct ImRenderer<Basic2dColouredTexturedVertex>;
    template struct Im2dBase<Basic2dColouredTexturedVertex>;
}
