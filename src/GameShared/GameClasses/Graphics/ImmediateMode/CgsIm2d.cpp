#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "pc/gcm/renderengine/device.h"    // gDevice, gDisplayWidth/Height
#include "pc/gcm/renderengine/texture.h"   // Texture::mpD3DTexture

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
}

namespace CgsGraphics
{
    // ---- ImRendererBase (non-template) -------------------------------------------
    void ImRendererBase::SetTexture(renderengine::Texture* lpTexture)
    {
        if (renderengine::gDevice != nullptr)
        {
            renderengine::gDevice->SetTexture(0, lpTexture != nullptr ? lpTexture->mpD3DTexture : nullptr);
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

    // ---- Im2dTransform -----------------------------------------------------------
    void Im2dTransform::TransformByAspectRatio()
    {
        // The PC Im2d::Render maps logical 1280x720 coordinates straight onto the back
        // buffer, so the aspect-ratio fold is a no-op here (kept for API parity).
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
