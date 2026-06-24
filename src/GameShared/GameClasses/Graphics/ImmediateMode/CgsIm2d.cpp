#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "pc/gcm/renderengine/device.h"        // gDevice, gDisplayWidth/Height
#include "pc/gcm/renderengine/texture.h"       // Texture::mpD3DTexture
#include "pc/gcm/renderengine/renderstates.h"  // TextureState::mpRaster (SetState(TextureState*))

#include <d3d9.h>

// PC / D3D9 implementation of the CgsGraphics immediate-mode 2D renderer. The X360/PS3
// Im2d batches into platform command buffers via vertex descriptors + program buffers;
// the PC backend draws each batch directly with DrawPrimitiveUP. Screen-space vertices
// (the engine works in a 1280x720 logical space) are scaled to the actual back buffer
// and submitted pre-transformed (D3DFVF_XYZRHW).

namespace
{
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
    void ImRenderer<V>::Render(renderengine::PrimitiveType /*lePrimitiveType*/, const V* lpVertices, u32 luCount)
    {
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (lpDevice == nullptr || lpVertices == nullptr || luCount < 3u)
        {
            return;
        }

        const f32 lfScaleX = static_cast<f32>(renderengine::gDisplayWidth) / KF_LOGICAL_WIDTH;
        const f32 lfScaleY = static_cast<f32>(renderengine::gDisplayHeight) / KF_LOGICAL_HEIGHT;

        enum { KI_MAX_BATCH = 64 };
        D3DScreenVertex laBatch[KI_MAX_BATCH];
        if (luCount > KI_MAX_BATCH)
        {
            luCount = KI_MAX_BATCH;
        }
        for (u32 i = 0; i < luCount; ++i)
        {
            laBatch[i].x = lpVertices[i].mv2Pos.x * lfScaleX;
            laBatch[i].y = lpVertices[i].mv2Pos.y * lfScaleY;
            laBatch[i].z = 0.0f;
            laBatch[i].rhw = 1.0f;
            laBatch[i].color = D3DCOLOR_ARGB(lpVertices[i].mv4Colour.a, lpVertices[i].mv4Colour.r,
                                             lpVertices[i].mv4Colour.g, lpVertices[i].mv4Colour.b);
            laBatch[i].u = lpVertices[i].mv2Tex0UV.x;
            laBatch[i].v = lpVertices[i].mv2Tex0UV.y;
        }

        lpDevice->SetFVF(KU_SCREEN_FVF);
        // The loading screen submits 4-vertex quads as triangle strips.
        lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, luCount - 2u, laBatch, sizeof(D3DScreenVertex));
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

    // Instantiate the coloured+textured 2D renderer the loading screen uses.
    template struct ImRenderer<Basic2dColouredTexturedVertex>;
    template struct Im2dBase<Basic2dColouredTexturedVertex>;
}
