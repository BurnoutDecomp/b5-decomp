#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"   // CgsGraphics::Im2d (full def) + ImRenderer<V>::SetProgram + the batch/transform command API
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"     // CgsGraphics::TextRenderer / TextObject (RenderTextField submission)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"                     // BrnFlapt::Mesh / FlaptFile / FlaptFile::GuiTexture / GuiVertex
#include "pc/gcm/renderengine/texture.h"                              // renderengine::Texture2D create path (interim white "no texture")
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // [gateui r6] gpDebugPrint (the mask/text placement probes)
#include <stdlib.h>                                                   // [gateui r6] getenv (BRN_PROP_DIAG)

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


// The immediate renderer's two default render-state singletons RenderMesh binds. On the
// console these are members of the shared CgsGraphics::ImRendererBase::StateLibrary
// (mgStateLibrary), built once by ImRendererBase::ConstructOnceOnly:
//   gpFlaptNoTexture          = mgStateLibrary.mpTexture_White      (X360 *(0x83010F58)) -
//                               the white "no texture" bound for an untextured mesh
//                               (miTextureId < 0); it samples white so the mesh shows its
//                               vertex colours (this is how solid-colour UI -- backgrounds,
//                               borders -- draws).
//   gpFlaptDefaultBlendState  = mgStateLibrary.mpBlendState_Standard (X360 *(0x83010F20)).
//
// FLAG (interim homing): the ImRendererBase state library (CgsImRenderer.cpp) is not yet
// compiled into the build, so its ConstructOnceOnly never runs and these were null link
// placeholders -- which made RenderMesh's faithful `lbIsSpecialTexture || lpTexture` guard
// assert on the first untextured mesh once MovieClipInstance::Render was bodied. Until the
// state library is homed and linked (a separate, scoped milestone -- see the flapt-render
// bring-up notes), build the SAME white fallback ConstructWhiteTexture produces (4x4 A8R8G8B8,
// every texel 0xFFFFFFFF) lazily here, through the PC render-engine Texture2D create path
// (identical to CgsMoviePlayer / LoadingScreenRenderer). The default blend state stays null:
// RenderMesh only binds a blend state when it differs from the cached one, and both start
// null, so it is never bound (the batch inherits the current device blend) -- tolerable and
// non-asserting. Replace both with `extern` refs into mgStateLibrary once ConstructOnceOnly
// is brought into the build.
const CgsGraphics::BlendState* const gpFlaptDefaultBlendState = 0;

// Interim ConstructWhiteTexture (see FLAG above): the faithful 4x4 all-white A8R8G8B8 fallback,
// built once via the render-engine Texture2D create/lock/fill/unlock path the PC texture
// creators use. Returned to RenderMesh in place of the null gpFlaptNoTexture placeholder.
renderengine::Texture* GetFlaptNoTexture()
{
    static renderengine::Texture* spWhiteTexture = 0;
    if (spWhiteTexture == 0)
    {
        renderengine::Texture2D::Parameters lParams = {};
        lParams.muWidth     = 4;
        lParams.muHeight    = 4;
        lParams.muDepth     = 1;
        lParams.muNumLevels = 1;
        lParams.muFormat    = 340;   // A8R8G8B8 (the PC render-engine's 32-bit RGBA format)

        renderengine::Texture2D::ResourceDescriptor lDesc;
        renderengine::Texture2D::GetResourceDescriptor(&lDesc, &lParams);
        renderengine::Texture2D* lpTexture = renderengine::Texture2D::Initialize(&lDesc, &lParams);

        renderengine::Texture::LockInfo lLocked;
        renderengine::Texture::Lock(lpTexture, 0, 0, 0, &lLocked);
        if (lLocked.mpBits != 0)
        {
            // Fill white row-by-row at the locked pitch (the surface row stride may exceed
            // the 16-byte texel run for a 4-wide A8R8G8B8 row).
            u8* lpRow = static_cast<u8*>(lLocked.mpBits);
            for (u32 luY = 0; luY < 4; ++luY)
            {
                u32* lpTexels = reinterpret_cast<u32*>(lpRow);
                for (u32 luX = 0; luX < 4; ++luX)
                {
                    lpTexels[luX] = 0xFFFFFFFFu;
                }
                lpRow += lLocked.muPitch;
            }
        }
        renderengine::Texture::Unlock(lpTexture, &lLocked);
        spWhiteTexture = lpTexture;
    }
    return spWhiteTexture;
}

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
        lpTexture = GetFlaptNoTexture();   // interim mgStateLibrary.mpTexture_White (see FLAG above)
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

    // [DIAG] NOT IN THE X360 BINARY. [gateui r6] The text-placement probe. The glyph run
    // the TextRenderer builds is in the field's LOCAL box space; this transform is what
    // carries it onto the banner. Until this round the submit path (RenderStart/RenderEnd)
    // dropped it, so every FLAPT text field landed at its authored box origin read as raw
    // screen pixels -- which is why the HUD-message banner's two strings both drew on top
    // of each other in the screen's top-left corner (both boxes author top-left (-2,-2)).
    // Printed in NDC, so a non-degenerate basis and an origin away from (-1,+1) is proof.
    {
        static const bool sbUiGateDiag  = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static s32        siDiagPrinted = 0;
        if ( sbUiGateDiag && siDiagPrinted < 12 && CgsDev::Log::gpDebugPrint != 0 )
        {
            ++siDiagPrinted;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] flapt text ndcOrigin=(" << lrTransform.mOriginXYZ.x
                << "," << lrTransform.mOriginXYZ.y
                << ") basis=(" << lrTransform.mRightUp.x << "," << lrTransform.mRightUp.y
                << "," << lrTransform.mRightUp.z << "," << lrTransform.mRightUp.w
                << ") alpha=" << (lrTransform.mColourScale.w + lrTransform.mColourShift.w)
                << "\n";
        }
    }

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
// mRightUp row, which interleaves the right/up basis as {right.x, right.y, up.x, up.y}
// == the serialised flapt/apt {a,b,c,d} raw order. (The screen transform is DIAGONAL, so
// the vperm lanes cannot distinguish the interleave; the rotation-bearing FLAPTHUD
// keyframe data is the ground truth -- see ComposeDrawTransform's note.) De-optimised
// here to the equivalent named-component writes (the inlined Im2dTransform::Construct).
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

    // Right/Up basis interleaved as {right.x, right.y, up.x, up.y}: a pure scale, so the
    // off-diagonal lanes (right.y, up.x) are zero.
    lScreenXForm.mRightUp.x = KF_NDC_PER_HALF_WIDTH;    // right.x (m00)
    lScreenXForm.mRightUp.y = 0.0f;                     // right.y (m10)
    lScreenXForm.mRightUp.z = 0.0f;                     // up.x    (m01)
    lScreenXForm.mRightUp.w = KF_NDC_PER_HALF_HEIGHT;   // up.y    (m11)

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

    // [gateui r6] FLAG PC-platform leaf: publish this mask mesh's own composed transform
    // before the push. On the console the PushMask command is dispatched into a stream in
    // which the transform is persistent GPU state, so the mask quad is transformed by the
    // shader constants already resident; the PC fold has no resident GPU transform, so the
    // state the mask corners must be folded through -- the transform the enclosing
    // MovieClipInstance::Render walk pushed for this very mesh, i.e. the stack top read
    // above -- is (re)published explicitly here. Without it Im2dBase::PushMask has no
    // transform to fold and the mask degenerates to the mesh's raw local quad.
    mpImRenderSet->mpIm2dRenderBuffer->SetTransform(lrTransform);

    mpImRenderSet->mpIm2dRenderBuffer->PushMask(
        const_cast<renderengine::Texture*>(lpTexture), laMaskVerts);

    // [DIAG] NOT IN THE X360 BINARY. [gateui r6] The mask-rect probe: prints the
    // back-buffer scissor the push installed. Before this round every FLAPT mask folded to
    // a ~31x31 px rect in the screen's top-left corner (the corners were read as 1280x720
    // logical coordinates), which clipped the whole masked layer -- the HUD-message
    // banner's ribbon body -- away. A rect that now tracks the wipe is the proof.
    {
        static const bool sbUiGateDiag  = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static s32        siDiagPrinted = 0;
        if ( sbUiGateDiag && siDiagPrinted < 12 && CgsDev::Log::gpDebugPrint != 0 )
        {
            ++siDiagPrinted;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] flapt mask rect=(" << CgsGraphics::gLastIm2dMaskRect[0]
                << "," << CgsGraphics::gLastIm2dMaskRect[1]
                << ")-(" << CgsGraphics::gLastIm2dMaskRect[2]
                << "," << CgsGraphics::gLastIm2dMaskRect[3]
                << ") localMin=(" << lMinVert.mv2Pos.x << "," << lMinVert.mv2Pos.y
                << ") localMax=(" << lMaxVert.mv2Pos.x << "," << lMaxVert.mv2Pos.y
                << ")\n";
        }
    }

    // Count this mesh against the currently-open mask.
    ++mMaskMeshCounts[mMaskMeshCounts.GetLength() - 1];
}

// ---- StartDrawingMask / PopMask ------------------------------------------
// The open/close pair for the Flapt stencil-mask path (MovieClipInstance::Render brackets
// a mask render layer with them; RenderMesh routes meshes to RenderMask while the mask bit
// is set, and RenderMask tallies them). Reconstructed from the mask protocol rather than
// each method's own disassembly: RenderMask's invariants ((mxFlags & 1) set, an open
// mMaskMeshCounts entry it post-increments), the attested Stack<u16,2> call sites
// (StartDrawingMask -> Push @0x8246E738; PopMask -> Peek @0x8246E8A8 then Pop @0x8246E7F8,
// per CgsStackUnsignedShort2.cpp), and the render buffer's PushMask/PopMask command pair.

// StartDrawingMask : begin defining a mask shape. Flag the renderer into the mask path and
// open a fresh mesh tally for this mask (RenderMask bumps it per mask mesh).
void FlaptRenderer::StartDrawingMask()
{
    mxFlags |= 1u;
    mMaskMeshCounts.Push(static_cast<u16>(0));
}

// PopMask : close the innermost mask. Emit the render buffer's pop-mask command (on the PC
// fold this disables the scissor region) and drop this mask's mesh tally. The mask bit has
// already been cleared by the render walk once the shape's meshes were drawn.
void FlaptRenderer::PopMask()
{
    CGS_ASSERT(!mMaskMeshCounts.IsEmpty(), "!mMaskMeshCounts.IsEmpty()");
    mpImRenderSet->mpIm2dRenderBuffer->PopMask();
    mMaskMeshCounts.Pop();
}

}
