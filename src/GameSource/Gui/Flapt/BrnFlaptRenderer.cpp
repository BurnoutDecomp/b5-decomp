#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"   // CgsGraphics::Im2d (full def) + ImRenderer<V>::SetProgram + the batch/transform command API
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"     // CgsGraphics::TextRenderer / TextObject (RenderTextField submission)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"                     // BrnFlapt::Mesh / FlaptFile / FlaptFile::GuiTexture / GuiVertex

// BrnFlapt::FlaptRenderer member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU bodies the constructor, the shader-state accessor, the per-object render entry
// points, and the mask submission path:
//
//   Construct                     @ 0x82472270
//   SetShader                     @ 0x82470718
//   SetSpecialTextureShaderProgram@ 0x8246D748
//   RenderMesh                    @ 0x82470770
//   RenderMask                    @ 0x8246F900
//   RenderTextField               @ 0x82470A10
//
// (The remaining FlaptRenderer methods - StartRenderingFrame / StartDrawingMask /
// PopMask - land in their own TUs in this same file.)
//
// Construct and RenderMask build/apply a CgsGraphics::Im2dTransform through its four
// named Vector4 rows (mOriginXYZ / mRightUp / mColourShift / mColourScale). The X360
// emitted the row construction and the corner transform as hand-VMX (vperm against the
// KV_IM2DTRANSFORMPERMUTECONST_* tables, vmulfp/vmaddfp lane work); this reconstruction
// de-optimises that back into ordinary named-component math, which is what the inlined
// Im2dTransform::Construct / the corner-transform loop were in the original source.

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
// These are REAL console globals (X360 reads each as a pointer-valued global):
//   gpFlaptNoTexture          = *(0x83010F58)  - the "no texture" texture bound for an
//                               untextured mesh (miTextureId < 0).
//   gpFlaptDefaultBlendState  = *(0x83010F20)  - the default blend state every mesh
//                               submission binds.
// FLAG (un-homed console globals): the renderer state-initialisation TU that
// populates them is not yet recovered, so they are defined null here as link
// placeholders. RenderMesh's faithful guard below then asserts-and-skips an
// untextured mesh until that TU lands (a real gpFlaptNoTexture makes the guard
// pass, exactly as on the console). Replace these definitions with externs when
// the owning TU is homed.
renderengine::Texture* const         gpFlaptNoTexture = 0;
const CgsGraphics::BlendState* const gpFlaptDefaultBlendState = 0;

// ---- StartRenderingFrame @ 0x82470698 --------------------------------------
void FlaptRenderer::StartRenderingFrame()
{
    mpCurrentTexture = 0;
    mpCurrentBlendState = 0;

    // FLAG PC-platform leaf: the console appends explicit CullNone and ZBufferOff
    // state commands after BeginRendering. The PC Im2d backend applies those same
    // D3D9 states inside BeginRendering and has no console StateLibrary object.
    mpImRenderSet->mpIm2dRenderBuffer->BeginRendering();
}

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

    // A non-special mesh must resolve to a real texture; any mesh without one is
    // skipped (the special texture may legitimately be unbound while its provider
    // component has not produced a frame yet).
    CGS_ASSERT(lbIsSpecialTexture || lpTexture != 0, "lbIsSpecialTexture || lpTexture");
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

// ---- Construct @ 0x82472270 ----------------------------------------------
// Build the renderer: cache its four collaborators, clear the cached texture/blend/
// shader state, then construct the default screen-space Im2dTransform and seed the
// transform stack with it (after folding in the display aspect ratio). The screen
// transform maps a 1280x720 logical screen onto normalised device coordinates:
//   origin   = (-1, +1)            (screen pixel (0,0) -> NDC top-left)
//   right    = ( 1/640, 0 )        (screen-X span 1280 -> NDC width 2)
//   up       = ( 0, -1/360 )       (screen-Y span 720  -> NDC height 2, Y flipped)
//   colour   : shift 0, scale 1    (identity colour transform)
//
// The X360 packs this with VMX: it scales the rwmath unit basis vectors by the two
// reciprocal half-extents (1/640 = 0.0015625, -1/360 = -0.0027777778) and permutes the
// lanes (vperm against the KV_IM2DTRANSFORMPERMUTECONST_* tables) into the transform's
// mRightUp row, which interleaves the right/up basis as {right.x, up.x, right.y, up.y}.
// TransformByAspectRatio reads the basis back exactly that way (Right=(mRightUp.x,
// mRightUp.z), Up=(mRightUp.y, mRightUp.w)). De-optimised here to the equivalent
// named-component writes (this is the inlined CgsGraphics::Im2dTransform::Construct).
void FlaptRenderer::Construct(FlaptRenderSet* lpImRenderSet, void* lpTextRenderer,
                              void* lpLanguageManager, const void* lpFonts)
{
    CGS_ASSERT(lpImRenderSet != 0, "lpImRenderSet");
    CGS_ASSERT(lpTextRenderer != 0, "lpTextRenderer");
    CGS_ASSERT(lpLanguageManager != 0, "lpLanguageManager");
    CGS_ASSERT(lpFonts != 0, "lpFonts");

    mpImRenderSet     = lpImRenderSet;
    mpTextRenderer    = lpTextRenderer;
    mpLanguageManager = lpLanguageManager;
    mpFonts           = const_cast<void*>(lpFonts);

    mpCurrentTexture    = 0;
    mpCurrentBlendState = 0;
    mxFlags             = 0;
    miShaderProgram     = -1;

    // The fixed-capacity stacks both start empty-but-constructed.
    maTransformStack.Construct();
    mMaskMeshCounts.Construct();

    // Build the default screen->NDC transform by named components, then fold the
    // display aspect ratio into it in place.
    const f32 KF_NDC_PER_HALF_WIDTH  =  1.0f / 640.0f;   // 0.0015625
    const f32 KF_NDC_PER_HALF_HEIGHT = -1.0f / 360.0f;   // -0.0027777778 (Y flipped)

    CgsGraphics::Im2dTransform lScreenXForm;

    // Origin: NDC top-left corner.
    lScreenXForm.mOriginXYZ.x = -1.0f;
    lScreenXForm.mOriginXYZ.y =  1.0f;
    lScreenXForm.mOriginXYZ.z =  0.0f;
    lScreenXForm.mOriginXYZ.w =  0.0f;

    // Right/Up basis interleaved as {right.x, up.x, right.y, up.y}: a pure scale, so the
    // off-diagonal lanes (up.x, right.y) are zero.
    lScreenXForm.mRightUp.x = KF_NDC_PER_HALF_WIDTH;    // right.x
    lScreenXForm.mRightUp.y = 0.0f;                     // up.x
    lScreenXForm.mRightUp.z = 0.0f;                     // right.y
    lScreenXForm.mRightUp.w = KF_NDC_PER_HALF_HEIGHT;   // up.y

    // Identity colour transform.
    lScreenXForm.mColourShift.x = 0.0f;
    lScreenXForm.mColourShift.y = 0.0f;
    lScreenXForm.mColourShift.z = 0.0f;
    lScreenXForm.mColourShift.w = 0.0f;
    lScreenXForm.mColourScale.x = 1.0f;
    lScreenXForm.mColourScale.y = 1.0f;
    lScreenXForm.mColourScale.z = 1.0f;
    lScreenXForm.mColourScale.w = 1.0f;

    lScreenXForm.TransformByAspectRatio();

    maTransformStack.Push(lScreenXForm);
}

// ---- RenderMask @ 0x8246F900 ---------------------------------------------
// Push a stencil mask region for one mesh quad. Find the quad's min-corner (the vertex
// with both the smallest X and smallest Y) and max-corner (largest X and Y); bias each
// corner's position by the current transform's origin contribution (mRightUp scale x
// mOriginXYZ origin), carry through the per-corner colour and UV, and submit the two
// corner vertices as the mask rectangle through the render buffer's PushMask. Finally
// bump the mesh tally for the active mask.
//
// The X360 transforms the two corners with VMX vmaddfp lane work; the operation it emits
// for each corner is pos.x = mRightUp.x * mOriginXYZ.x + cornerX and pos.y = mRightUp.w *
// mOriginXYZ.y + cornerY (the origin-bias product is shared by both corners). Reproduced
// here verbatim as named-component math.
void FlaptRenderer::RenderMask(const Mesh* lpMesh, const FlaptFile* lpFile,
                               const FlaptFile::GuiTexture* lpTexture)
{
    CGS_ASSERT(lpMesh != 0, "lpMesh");
    CGS_ASSERT(lpFile != 0, "lpFile");
    CGS_ASSERT(lpTexture != 0, "lpTexture");
    CGS_ASSERT((mxFlags & 1u) != 0, "IsRenderingMask()");
    CGS_ASSERT(!mMaskMeshCounts.IsEmpty(), "!mMaskMeshCounts.IsEmpty()");

    // Mask meshes must be quads (the strip is a single 4-vertex rectangle).
    CGS_ASSERT(lpMesh->muNumVerts == 4, "Mask meshes need to be quads");

    const FlaptFile::GuiVertex* lpVerts = &lpFile->mpaVerts[lpMesh->muVertOffset];

    // Min-corner (smallest x AND y) and max-corner (largest x AND y), seeded from vertex 0.
    FlaptFile::GuiVertex lMinVert = lpVerts[0];
    FlaptFile::GuiVertex lMaxVert = lpVerts[0];
    for (u32 luVert = 1; luVert < lpMesh->muNumVerts; ++luVert)
    {
        const FlaptFile::GuiVertex& lrVert = lpVerts[luVert];
        if (lrVert.mv2Pos.x <= lMinVert.mv2Pos.x && lrVert.mv2Pos.y <= lMinVert.mv2Pos.y)
        {
            lMinVert = lrVert;
        }
        else if (lrVert.mv2Pos.x >= lMaxVert.mv2Pos.x && lrVert.mv2Pos.y >= lMaxVert.mv2Pos.y)
        {
            lMaxVert = lrVert;
        }
    }

    const CgsGraphics::Im2dTransform& lrTransform = maTransformStack.Peek();

    // The origin-bias the X360 adds to each corner (a per-submission constant).
    const f32 lfOriginBiasX = lrTransform.mRightUp.x * lrTransform.mOriginXYZ.x;
    const f32 lfOriginBiasY = lrTransform.mRightUp.w * lrTransform.mOriginXYZ.y;

    CgsGraphics::Basic2dColouredTexturedVertex laMaskVerts[2];

    laMaskVerts[0].mv2Pos.x   = lfOriginBiasX + lMinVert.mv2Pos.x;
    laMaskVerts[0].mv2Pos.y   = lfOriginBiasY + lMinVert.mv2Pos.y;
    laMaskVerts[0].mv4Colour  = lMinVert.mv4Colour;
    laMaskVerts[0].mv2Tex0UV  = lMinVert.mv2Tex0UV;

    laMaskVerts[1].mv2Pos.x   = lfOriginBiasX + lMaxVert.mv2Pos.x;
    laMaskVerts[1].mv2Pos.y   = lfOriginBiasY + lMaxVert.mv2Pos.y;
    laMaskVerts[1].mv4Colour  = lMaxVert.mv4Colour;
    laMaskVerts[1].mv2Tex0UV  = lMaxVert.mv2Tex0UV;

    mpImRenderSet->mpIm2dRenderBuffer->PushMask(
        const_cast<renderengine::Texture*>(lpTexture), laMaskVerts);

    // Count this mesh against the currently-open mask.
    ++mMaskMeshCounts[mMaskMeshCounts.GetLength() - 1];
}

}
