#ifndef BRN_FLAPT_RENDERER_H
#define BRN_FLAPT_RENDERER_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsStack.h"               // CgsContainers::Stack<Im2dTransform,33> (the transform stack at +0x30) + Stack<u16,2> (mask-mesh counts)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h" // CgsGraphics::Im2dTransform (stack element)
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"                     // BrnFlapt::Mesh / FlaptFile / FlaptFile::GuiTexture (RenderMesh/RenderMask params)

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptRenderer.h
//
// BrnFlapt::FlaptRenderer - draws a Flapt (Flash-derived) GUI movie's meshes and
// text fields through the immediate-mode 2D render buffer. Reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Attested layout (from FlaptRenderer::Construct @0x82472270, RenderMesh
// @0x82470770, RenderMask @0x8246F900 and RenderTextField @0x82470A10; member
// access is BY NAME on both the X360 32-bit and the PC x64 targets - pointers
// widen, so byte offsets shift, but every store/load below is a named member,
// never a raw offset):
//
//   +0x00  mpImRenderSet     (Construct: *this = lpImRenderSet)
//   +0x04  mpTextRenderer    (Construct: *(this+4)  = lpTextRenderer)
//   +0x08  mpLanguageManager (Construct: *(this+8)  = lpLanguageManager)
//   +0x0C  mpFonts           (Construct: *(this+12) = lpFonts)
//   +0x10  mpCurrentTexture  (Construct: 0; RenderMesh v4[4]: current bound texture)
//   +0x14  mpCurrentBlendState (Construct: 0; RenderMesh v4[5]: current blend state)
//   +0x18  miShaderProgram   (Construct: -1; SetShader subject; RenderMesh v4[6])
//   +0x1C  miSpecialTextureShaderProgram (RenderMesh v4[7]; SetSpecialTextureShaderProgram writes it)
//   +0x20  mxFlags           (Construct: 0; bit0 = "drawing mask"; RenderMesh tests it)
//   +0x30  maTransformStack  (Construct: Push(this+48); Stack<Im2dTransform,33>, miLength at +0x870)
//   +0x880 mMaskMeshCounts   (Construct: 0; Stack<u16,2>, miLength at +0x884; per-active-mask mesh tally)
//
// Grow this header additively as the sibling FlaptRenderer TUs (Construct /
// RenderMask / StartRenderingFrame / StartDrawingMask / PopMask) land their
// bodies.
// ============================================================================

namespace CgsGraphics
{
    struct Im2d;          // the immediate-mode render buffer the render set feeds (== Im2dRenderBuffer on the PC fold)
    struct TextObject;    // one piece of bitmap-font text (RenderTextField param; defined in CgsFontRenderer.h)
}

namespace BrnFlapt
{
    // The render set handed to FlaptRenderer::Construct (X360 asserted name
    // "lpImRenderSet"). The only member the X360 ledger attests is its leading
    // immediate-mode render buffer, named by the RenderMesh assert string
    // "mpImRenderSet->mpIm2dRenderBuffer" (the `!**this` check). Modeled minimally
    // here (no separate home header exists for the type); grow additively if a
    // FlaptRenderSet TU is later recovered.
    struct FlaptRenderSet
    {
        CgsGraphics::Im2d* mpIm2dRenderBuffer;   // +0x00 (the 2D render buffer the commands drive)
    };

    struct FlaptRenderer
    {
        // Construct @ 0x82472270 : store the four collaborators (render set, text
        // renderer, language manager, font collection), clear the cached
        // texture/blend/shader state, build the default screen-space Im2dTransform
        // (origin + right/up basis + identity colour transform), fold the display
        // aspect ratio into it, and seed the transform stack with it. DWARF signature
        // Construct(CgsGui::ImRendererSet*, CgsGraphics::TextRenderer*,
        // CgsLanguage::LanguageManager*, const CgsGui::FontCollection*); the latter
        // three are held opaquely here, so they are taken as the matching member types.
        void Construct(FlaptRenderSet* lpImRenderSet, void* lpTextRenderer,
                       void* lpLanguageManager, const void* lpFonts);

        // SetShader @ 0x82470718 : if liProgramId differs from the cached
        // miShaderProgram, push a SetProgram command onto the render buffer and
        // cache the new id. Returns the render buffer (the X360 returns r3, the
        // SetProgram result / the renderer pointer); modeled as void since no
        // caller consumes the value.
        void SetShader(s32 liProgramId);

        // RenderMesh @ 0x82470770 : resolve the mesh's texture + shader, then either
        // route it to the mask path (mxFlags bit0) or batch a textured/blended static
        // vertex run for it through the render buffer.
        void RenderMesh(const Mesh* lpMesh, const FlaptFile* lpFile);

        // RenderTextField @ 0x82470A10 : set the current transform on the render
        // buffer, reset the texture/shader state, and hand the text object to the
        // bitmap-font TextRenderer for glyph submission.
        void RenderTextField(const CgsGraphics::TextObject* lpTextObject);

        // SetSpecialTextureShaderProgram @ 0x8246D748 : record the shader program id
        // RenderMesh installs when it hits a "special" (video / runtime) texture.
        void SetSpecialTextureShaderProgram(s32 liShaderProgram);

        // RenderMask @ 0x8246F900 : find the mesh quad's min/max screen-space corners,
        // bias them by the current Im2dTransform's origin contribution, and push a
        // two-vertex stencil mask region (with lpTexture) for it; then bump the
        // active mask's mesh tally.
        void RenderMask(const Mesh* lpMesh, const FlaptFile* lpFile,
                        const FlaptFile::GuiTexture* lpTexture);

        // StartRenderingFrame : begin a frame on the immediate-mode render buffer (called
        // by FlaptManager::Render before the movie draws). Declared here so the manager's
        // Render compiles; the body lands with this class's own TU.
        void StartRenderingFrame();

        // StartDrawingMask / PopMask : open and close one stencil-mask region. The Flapt
        // render walk (MovieClipInstance::Render) brackets a mask render layer with these:
        // StartDrawingMask sets the mask bit (bit0 of mxFlags) so RenderMesh routes the
        // mask-shape meshes to RenderMask and pushes a fresh mesh tally; the walk clears
        // the bit once the shape is drawn, then PopMask undoes the region and drops the
        // tally.
        void StartDrawingMask();
        void PopMask();

        FlaptRenderSet* mpImRenderSet;                // +0x00 (X360 asserted name "mpImRenderSet")
        void*           mpTextRenderer;               // +0x04 (CgsGraphics::TextRenderer*; opaque here)
        void*           mpLanguageManager;            // +0x08
        void*           mpFonts;                      // +0x0C
        void*           mpCurrentTexture;             // +0x10 (renderengine::Texture*; last bound texture)
        void*           mpCurrentBlendState;          // +0x14 (const BlendState*; last bound blend state)
        s32             miShaderProgram;              // +0x18  (Construct: -1)
        s32             miSpecialTextureShaderProgram;// +0x1C
        u8              mxFlags;                       // +0x20  (flags byte; bit0 = mask path)
        u8              mau8Opaque21[0x0F];           // +0x21..0x2F  pad to the +0x30 transform stack
        CgsContainers::Stack<CgsGraphics::Im2dTransform, 33> maTransformStack; // +0x30 (miLength at +0x870)
        u8              mau8OpaqueXform[0x0C];        // pad the transform-stack tail to the mask-count stack
        CgsContainers::Stack<u16, 2> mMaskMeshCounts; // +0x880 (miLength at +0x884; meshes drawn into each live mask)
    };
}

#endif // BRN_FLAPT_RENDERER_H
