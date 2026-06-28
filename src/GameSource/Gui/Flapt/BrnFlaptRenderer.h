#ifndef BRN_FLAPT_RENDERER_H
#define BRN_FLAPT_RENDERER_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsStack.h"               // CgsContainers::Stack<Im2dTransform,33> (the transform stack at +0x30)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h" // CgsGraphics::Im2dTransform (stack element)

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptRenderer.h
//
// BrnFlapt::FlaptRenderer - draws a Flapt (Flash-derived) GUI movie's meshes and
// text fields through the immediate-mode 2D render buffer. Reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Attested layout (from FlaptRenderer::Construct @0x82472270 and RenderMesh
// @0x82470770; member access is BY NAME on both the X360 32-bit and the PC x64
// targets - pointers widen, so byte offsets shift, but every store/load below is
// a named member, never a raw offset):
//
//   +0x00  mpImRenderSet     (Construct: *this = lpImRenderSet)
//   +0x04  mpTextRenderer    (Construct: *(this+4)  = lpTextRenderer)
//   +0x08  mpLanguageManager (Construct: *(this+8)  = lpLanguageManager)
//   +0x0C  mpFonts           (Construct: *(this+12) = lpFonts)
//   +0x10  mpCurrentTexture  (Construct: 0; RenderMesh v4[4]: current bound texture)
//   +0x14  mpCurrentColourFormat (Construct: 0; RenderMesh v4[5]: current colour-format state)
//   +0x18  miShaderProgram   (Construct: -1; SetShader subject; RenderMesh v4[6])
//   +0x1C  miSpecialTextureShaderProgram (RenderMesh v4[7]; SetSpecialTextureShaderProgram writes it)
//   +0x20  mbDrawingMask     (Construct: 0; RenderMesh tests bit0 to pick the mask path)
//   +0x30  maTransformStack  (Construct: Push(this+48); Stack<Im2dTransform,33>, miLength at +0x870)
//
// Grow this header additively as the sibling FlaptRenderer TUs (Construct /
// RenderMesh / RenderMask / RenderTextField / SetSpecialTextureShaderProgram /
// StartRenderingFrame / StartDrawingMask / PopMask) land their bodies.
// ============================================================================

namespace CgsGraphics
{
    struct Im2d;   // the immediate-mode render buffer the render set feeds (== Im2dRenderBuffer on the PC fold)
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
        CgsGraphics::Im2d* mpIm2dRenderBuffer;   // +0x00 (the 2D render buffer SetShader drives)
    };

    struct FlaptRenderer
    {
        // SetShader @ 0x82470718 : if liProgramId differs from the cached
        // miShaderProgram, push a SetProgram command onto the render buffer and
        // cache the new id. Returns the render buffer (the X360 returns r3, the
        // SetProgram result / the renderer pointer); modeled as void since no
        // caller consumes the value.
        void SetShader(s32 liProgramId);

        FlaptRenderSet* mpImRenderSet;                // +0x00 (X360 asserted name "mpImRenderSet")
        void*           mpTextRenderer;               // +0x04
        void*           mpLanguageManager;            // +0x08
        void*           mpFonts;                      // +0x0C
        void*           mpCurrentTexture;             // +0x10
        void*           mpCurrentColourFormat;        // +0x14
        s32             miShaderProgram;              // +0x18  (Construct: -1)
        s32             miSpecialTextureShaderProgram;// +0x1C
        u8              mbDrawingMask;                // +0x20  (flags byte; bit0 = mask path)
        u8              mau8Opaque21[0x0F];           // +0x21..0x2F  pad to the +0x30 transform stack
        CgsContainers::Stack<CgsGraphics::Im2dTransform, 33> maTransformStack; // +0x30 (miLength at +0x870)
    };
}

#endif // BRN_FLAPT_RENDERER_H
