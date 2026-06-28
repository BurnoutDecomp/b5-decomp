#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"   // CgsGraphics::Im2d (full def) + ImRenderer<V>::SetProgram + the batch/transform command API
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"     // CgsGraphics::TextRenderer / TextObject (RenderTextField submission)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"                     // BrnFlapt::Mesh / FlaptFile / FlaptFile::GuiTexture / GuiVertex

// BrnFlapt::FlaptRenderer member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU bodies the shader-state accessor and the per-object render entry points:
//
//   SetShader                     @ 0x82470718
//   SetSpecialTextureShaderProgram@ 0x8246D748
//   RenderMesh                    @ 0x82470770
//   RenderTextField               @ 0x82470A10
//
// (The remaining FlaptRenderer methods - Construct / RenderMask / StartRenderingFrame
// / StartDrawingMask / PopMask - land in their own TUs in this same file. Construct
// and RenderMask are blocked on the rw::math::vpu vector-math operators used to build
// and apply the screen transform, which have no reconstructed home.)

namespace CgsGraphics
{
    // renderengine::PrimitiveType for a triangle strip (X360 RenderMesh passes the
    // literal 6 as the primitive-type argument; same value the font/movie paths use).
    static const u32 KU_PRIMITIVE_TRIANGLE_STRIP = 6u;
}

namespace BrnFlapt
{

// The batch command's "what changed" bitmask, from the DecFIGS DWARF
// (CgsGraphics::ImCommandBatchTransformTextureBlendRender::KU_FLAG_SETTEXTURE = 1,
// KU_FLAG_SETBLEND = 2). The command type itself has no reconstructed home, so the
// two flag values are pinned here against the DWARF rather than reaching into it.
const u8 KU_BATCH_FLAG_SETTEXTURE = 1;
const u8 KU_BATCH_FLAG_SETBLEND   = 2;


// The immediate renderer's two default render-state singletons that RenderMesh binds.
// These are global state objects with no reconstructed C++ home (the immediate-mode
// renderer's default-state pool); the X360 reads each as a pointer-valued global:
//   gpFlaptNoTexture          = *(0x83010F58)  - the "no texture" texture bound for an
//                               untextured mesh (miTextureId < 0).
//   gpFlaptDefaultBlendState  = *(0x83010F20)  - the default blend state every mesh
//                               submission binds.
// Declared extern (data references the render path reads), to be resolved by the
// renderer's own state-initialisation TU once it is recovered.
extern renderengine::Texture* const         gpFlaptNoTexture;
extern const CgsGraphics::BlendState* const gpFlaptDefaultBlendState;

// ---- SetShader @ 0x82470718 ----------------------------------------------
// Cache-then-set the immediate-mode render buffer's vertex/pixel program. The X360
// compares the requested program id against the cached miShaderProgram (a signed
// word, initialised to -1 by Construct); only on a change does it push the
// SetProgram command and update the cache - so a redundant SetShader is a no-op.
//
// X360 call chain for the SetProgram receiver: r11 = this->mpImRenderSet (lwz 0(this)),
// r11 = mpImRenderSet->mpIm2dRenderBuffer (lwz 0(r11)), r3 = r11 + 4. The "+4" is the
// X360 polymorphic ImRenderBuffer<V>'s command-API sub-object offset; on the PC fold
// (Im2dRenderBuffer == Im2d, which derives ImRenderer<Basic2dColouredTexturedVertex>)
// SetProgram is reached as an ordinary base-class method by name, so the offset folds
// away. SetProgram takes the program id as a byte (the X360 sign-extends it with
// extsb before the call); the cache is the full word the caller passed.
void FlaptRenderer::SetShader(s32 liProgramId)
{
    if (liProgramId != miShaderProgram)
    {
        mpImRenderSet->mpIm2dRenderBuffer->SetProgram(static_cast<s8>(liProgramId));
        miShaderProgram = liProgramId;
    }
}

// ---- SetSpecialTextureShaderProgram @ 0x8246D748 -------------------------
// Record the shader program id RenderMesh installs whenever it draws the file's
// "special" (video / runtime-substituted) texture. A one-line field store
// (X360: `stw r4, 0x1C(r3); blr`).
void FlaptRenderer::SetSpecialTextureShaderProgram(s32 liShaderProgram)
{
    miSpecialTextureShaderProgram = liShaderProgram;
}

// ---- RenderMesh @ 0x82470770 ---------------------------------------------
// Draw one mesh of the movie. Resolve its texture (a negative texture id means
// "untextured", and the program is forced back to 0; the texture at index
// muNumTextures-muNumSpecialTextures is the "special" texture, which selects the
// special-texture shader). With the texture+shader chosen, either route the mesh to
// the mask path (mxFlags bit0 set) or batch a static, textured/blended vertex run for
// it through the render buffer - flagging which of the texture/blend states actually
// changed since the last submission so the command only re-binds what it must.
void FlaptRenderer::RenderMesh(const Mesh* lpMesh, const FlaptFile* lpFile)
{
    CGS_ASSERT(lpMesh != 0, "lpMesh");
    CGS_ASSERT(lpFile != 0, "lpFile");
    CGS_ASSERT(mpImRenderSet->mpIm2dRenderBuffer != 0, "mpImRenderSet->mpIm2dRenderBuffer");
    CGS_ASSERT(lpFile->mpaVerts != 0, "lpFile->mpaVerts");

    bool lbIsSpecialTexture = false;
    renderengine::Texture* lpTexture;

    const s32 liTextureId = lpMesh->miTextureId;   // signed: <0 means "no texture"
    if (liTextureId < 0)
    {
        lpTexture = gpFlaptNoTexture;
        if (miShaderProgram != 0)
        {
            // Force the program back to 0 directly (bypassing SetShader's cache, then
            // re-syncing the cache), matching the X360.
            mpImRenderSet->mpIm2dRenderBuffer->SetProgram(static_cast<s8>(0));
            miShaderProgram = 0;
        }
    }
    else
    {
        CGS_ASSERT(static_cast<u32>(liTextureId) < lpFile->muNumTextures,
                   "(uint32_t)lpMesh->miTextureId < lpFile->muNumTextures");

        lpTexture = lpFile->mpapTextures[liTextureId];

        // The special textures occupy the trailing muNumSpecialTextures slots; the X360
        // tests equality against the first of those slots.
        if (liTextureId == static_cast<s32>(lpFile->muNumTextures - lpFile->muNumSpecialTextures))
        {
            lbIsSpecialTexture = true;
            SetShader(miSpecialTextureShaderProgram);
        }
        else
        {
            SetShader(0);
        }
    }

    // A non-special mesh must resolve to a real texture.
    if (!lbIsSpecialTexture)
    {
        CGS_ASSERT(lpTexture != 0, "lbIsSpecialTexture || lpTexture");
    }
    if (lpTexture == 0)
    {
        return;
    }

    if ((mxFlags & 1u) != 0)
    {
        RenderMask(lpMesh, lpFile, lpTexture);
        return;
    }

    CGS_ASSERT(static_cast<u32>(lpMesh->muVertOffset + lpMesh->muNumVerts) <= lpFile->muNumVerts,
               "(uint32_t)( lpMesh->muVertOffset + lpMesh->muNumVerts ) <= lpFile->muNumVerts");

    const CgsGraphics::BlendState* lpBlendState = gpFlaptDefaultBlendState;

    // Only re-bind the texture/blend state the batch command actually changes.
    u8 luFlags = 0;
    if (mpCurrentBlendState != lpBlendState)
    {
        luFlags = KU_BATCH_FLAG_SETBLEND;
        mpCurrentBlendState = const_cast<CgsGraphics::BlendState*>(lpBlendState);
    }
    if (mpCurrentTexture != lpTexture)
    {
        mpCurrentTexture = lpTexture;
        luFlags |= KU_BATCH_FLAG_SETTEXTURE;
    }

    const FlaptFile::GuiVertex* lpVerts = &lpFile->mpaVerts[lpMesh->muVertOffset];
    const CgsGraphics::Im2dTransform& lrTransform = maTransformStack.Peek();

    mpImRenderSet->mpIm2dRenderBuffer->BatchTransformTextureBlendRenderStatic(
        lrTransform,
        lpTexture,
        lpBlendState,
        static_cast<renderengine::PrimitiveType>(CgsGraphics::KU_PRIMITIVE_TRIANGLE_STRIP),
        lpVerts,
        lpMesh->muNumVerts,
        luFlags);
}

// ---- RenderTextField @ 0x82470A10 ----------------------------------------
// Submit one text field. Set the current transform on the render buffer, reset the
// in-flight shader program (back to 0) and the cached texture/blend state, then hand
// the text object to the bitmap-font TextRenderer, which batches its glyph quads.
void FlaptRenderer::RenderTextField(const CgsGraphics::TextObject* lpTextObject)
{
    CGS_ASSERT(lpTextObject != 0, "lpTextObject");
    CGS_ASSERT(mpImRenderSet->mpIm2dRenderBuffer != 0, "mpImRenderSet->mpIm2dRenderBuffer");
    CGS_ASSERT(mpTextRenderer != 0, "mpTextRenderer");

    const CgsGraphics::Im2dTransform& lrTransform = maTransformStack.Peek();
    mpImRenderSet->mpIm2dRenderBuffer->SetTransform(lrTransform);

    if (miShaderProgram != 0)
    {
        mpImRenderSet->mpIm2dRenderBuffer->SetProgram(static_cast<s8>(0));
        miShaderProgram = 0;
    }

    // The text path leaves the buffer's bound texture/blend state cleared so the next
    // mesh submission re-binds them.
    mpCurrentBlendState = 0;
    mpCurrentTexture = 0;

    static_cast<CgsGraphics::TextRenderer*>(mpTextRenderer)->RenderString(
        mpImRenderSet->mpIm2dRenderBuffer, *lpTextObject);
}

}
