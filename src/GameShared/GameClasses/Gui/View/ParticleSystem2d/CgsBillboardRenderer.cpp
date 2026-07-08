#include "CgsBillboardRenderer.h"

#include <cmath>

#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsGuiBillboardInfo.h"                 // CgsGui::BillboardInfo
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // CgsGraphics::ImRenderBuffer<V> / Im2dTransform
#include "GameShared/GameClasses/Core/CgsAssert.h"                                                // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The three in-scope bodies of the billboard batcher:
//   Construct  @ 0x82852C38
//   Render     @ 0x828577E0  (const)
//   SetUVTable @ 0x8284F478  (private)
//
// The static CGS_ASSERT supplies __FILE__/__LINE__; the X360-baked line numbers are noted next to
// each assert for the record (they are dropped from the reproduced message strings, which are the
// verbatim X360 rodata expressions).

namespace CgsGui
{
namespace
{
    // ---------------------------------------------------------------------------------------------
    // X360 module data-segment globals the draw binds, owned by the immediate-mode / GUI render
    // setup (NOT this TU): dword_83010F54 is the shared depth-stencil state pointer the billboard
    // strip is drawn with; unk_83011090 is the default screen-space Im2dTransform stamped ahead of
    // the draw (it sits alongside CgsIm2dTransform's statics in the data segment). They are
    // referenced read-only here; their definitions live in the (un-recovered) owning module.
    // FLAG: un-homed data-segment collaborators -- declared extern so no value is fabricated.
    // ---------------------------------------------------------------------------------------------
    // (declared at namespace scope below)

    // Repack a billboard's stored RGBA colour word into the vertex colour word. The X360 swaps the
    // outermost bytes and keeps the middle two -- verbatim the pseudocode expression
    //   ((((c << 8) | BYTE2(c)) << 8 | BYTE1(c)) << 8) | HIBYTE(c)   ==  new = [b0 b2 b1 b3].
    inline u32 PackVertexColour(u32 luColour)
    {
        return (((((luColour << 8) | ((luColour >> 16) & 0xFFu)) << 8)
                 | ((luColour >> 8) & 0xFFu)) << 8) | ((luColour >> 24) & 0xFFu);
    }
}

// The two X360 data-segment globals (see the FLAG above), reached read-only by Render.
extern const renderengine::DepthStencilState* gpBillboardDepthStencilState;  // dword_83010F54
extern const CgsGraphics::Im2dTransform       gBillboardScreenTransform;     // unk_83011090

// The shared full-texture default UV set (guest &unk_820E08B8): the four corners of the whole
// texture, in the same [u,v, u,v2, u2,v, u2,v2] corner order SetUVTable's grid generator emits.
// For a single 1x1 frame the generator yields exactly {0,0, 0,1, 1,0, 1,1}, so this is derived
// from that body (not a guessed rodata blob).
const f32 BillboardRenderer::maUVDefault[8] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f };

// -------------------------------------------------------------------------------------------------
// SetUVTable @ 0x8284F478
// -------------------------------------------------------------------------------------------------
void BillboardRenderer::SetUVTable(s32 liFramesX, s32 liFramesY, s32 liNumFrames)
{
    miNumFrames = 0;
    mpUVTable   = nullptr;

    // A non-positive explicit count means "one frame per grid cell".
    if (liNumFrames <= 0)
        liNumFrames = liFramesX * liFramesY;

    CGS_ASSERT(liNumFrames <= KI_MAX_ANIMFRAMES, "liNumFrames<=KI_MAX_ANIMFRAMES"); // :83

    if (liNumFrames < 0)
        liNumFrames = 0;

    miNumFrames = liNumFrames;

    // One-frame (or degenerate) table: point at the shared full-texture default and stop.
    if (liNumFrames <= 1)
    {
        miNumFrames = 1;
        mpUVTable   = maUVDefault;
        return;
    }

    const f32 lfInvFramesX = 1.0f / static_cast<f32>(liFramesX);
    const f32 lfInvFramesY = 1.0f / static_cast<f32>(liFramesY);

    CGS_ASSERT(miNumFrames < KI_MAX_ANIMFRAMES, "miNumFrames < KI_MAX_ANIMFRAMES"); // :92

    // Emit four corner UVs (8 floats) per frame, walking the framesX x framesY grid.
    s32  liUVIndex = 0;
    f32* lpUV      = mafUVTab;
    for (s32 liFrame = 0; liFrame < miNumFrames; ++liFrame)
    {
        const s32 liCol = liFrame % liFramesX;
        const s32 liRow = liFrame / liFramesX;
        const f32 lfU   = static_cast<f32>(liCol) * lfInvFramesX;
        const f32 lfV   = static_cast<f32>(liRow) * lfInvFramesY;
        const f32 lfU2  = lfU + lfInvFramesX;
        const f32 lfV2  = lfV + lfInvFramesY;

        lpUV[0] = lfU;   lpUV[1] = lfV;    // top-left
        lpUV[2] = lfU;   lpUV[3] = lfV2;   // bottom-left
        lpUV[4] = lfU2;  lpUV[5] = lfV;    // top-right
        lpUV[6] = lfU2;  lpUV[7] = lfV2;   // bottom-right

        lpUV      += 8;
        liUVIndex += 8;
    }

    CGS_ASSERT(liUVIndex == miNumFrames * 8, "liUVIndex == miNumFrames*8"); // :107

    mpUVTable = mafUVTab;
}

// -------------------------------------------------------------------------------------------------
// Construct @ 0x82852C38
// -------------------------------------------------------------------------------------------------
void BillboardRenderer::Construct(rw::IResourceAllocator* lpAllocator, s32 liMaxBillboards,
                                  const renderengine::TextureState* lpTextureState,
                                  const renderengine::BlendState* lpBlendState,
                                  s32 liFramesX, s32 liFramesY, s32 liNumFrames)
{
    mpUVTable       = nullptr;          // +8
    miVertexCount   = 0;               // +0x2014
    miMaxBillboards = liMaxBillboards; // +0x200C
    mpTextureState  = lpTextureState;  // +0x2030
    mpAllocator     = lpAllocator;     // +0x2018
    mpBlendState    = lpBlendState;    // +0x2048
    miNumFrames     = 1;               // +0     (SetUVTable resets it below)
    meAnimMode      = E_REPEAT;        // +4     default playback mode

    // Allocate the owned strip vertex buffer: 120 bytes (six 20-byte vertices) per billboard,
    // 32-byte aligned, named "BillboardRenderer". The X360 builds a serialised BaseResourceDescriptors<5>
    // whose leading pool is {120*max, 32} and whose remaining pools are the identity {0, 1}; on the
    // PC rwcore <4> form the leading pool is set and the rest keep the identity default ctor. The
    // resource's first base pointer is the vertex buffer.
    rw::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(120 * liMaxBillboards);
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 32;

    rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, "BillboardRenderer");
    maVertexList = reinterpret_cast<CgsGraphics::Basic2dColouredTexturedVertex*>(lResource.m_baseResources[0]);

    SetUVTable(liFramesX, liFramesY, liNumFrames);
}

// -------------------------------------------------------------------------------------------------
// Render @ 0x828577E0  (const)
//
// DWARF signature: Render(CgsGraphics::Im2dRenderBuffer*, const BillboardInfo*, int) const. The X360
// passes an Im2dRenderBuffer base and reaches its command buffer at `base + 4` (an
// ImRenderBuffer<Basic2dColouredTexturedVertex>) for every command, stamping the transform on the
// base (Im2dRenderBuffer::SetTransform, which itself does +4). On the x64 gate that +4 offset is not
// load-bearing, so -- like AptRenderHandler::Render -- the parameter is reduced to the command buffer
// directly and every op (including the transform) goes through it (the committed ImRenderBuffer<V>).
// -------------------------------------------------------------------------------------------------
void BillboardRenderer::Render(
    CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer,
    const BillboardInfo* laBillboards, s32 liNumBillboards) const
{
    // Nothing to draw, or no bound texture state -> skip entirely.
    if (liNumBillboards <= 0 || mpTextureState == nullptr)
        return;

    CGS_ASSERT(liNumBillboards <= miMaxBillboards, "liNumBillboards<=miMaxBillboards"); // :173

    s32 liRenderedCount = 0;
    for (s32 liIndex = 0; liIndex < liNumBillboards; ++liIndex)
    {
        const BillboardInfo& lrBillboard = laBillboards[liIndex];
        s32 liTexFrame = lrBillboard.miTextureFrame;

        // Draw this billboard only when the frame index is valid: >= 0, and either the mode is not
        // E_BLANK (an animated mode remaps out-of-range frames) or the frame is already in range.
        if (liTexFrame < 0 || (meAnimMode == E_BLANK && liTexFrame >= miNumFrames))
            continue;

        // Remap an out-of-range frame per the playback mode.
        if (liTexFrame >= miNumFrames)
        {
            switch (meAnimMode)
            {
            case E_BLANK:
                CGS_ASSERT(false, "false"); // :197 (a blank billboard should never reach here)
                break;
            case E_REPEAT:
                liTexFrame %= miNumFrames;
                break;
            case E_CLAMP:
                liTexFrame = miNumFrames - 1;
                break;
            case E_SHUTTLE:
            {
                const s32 liPeriod = 2 * miNumFrames - 2;
                liTexFrame %= liPeriod;
                if (liTexFrame >= miNumFrames)
                    liTexFrame = liPeriod - liTexFrame;
                break;
            }
            default:
                break;
            }
        }

        CGS_ASSERT(liTexFrame >= 0 && liTexFrame < miNumFrames,
                   "TexFrame >= 0 && TexFrame < miNumFrames"); // :209

        const f32* lpUV = &mpUVTable[8 * liTexFrame];

        // The four real corners of this quad occupy the leading four vertices of this billboard's
        // six-vertex block (the trailing two are filled as degenerate strip bridges below / by the
        // next drawn billboard). Stride is six vertices per drawn billboard.
        CgsGraphics::Basic2dColouredTexturedVertex* lpQuad = &maVertexList[6 * liRenderedCount];

        const f32 lfPosX     = lrBillboard.mfPosX;
        const f32 lfPosY     = lrBillboard.mfPosY;
        const f32 lfSizeX    = lrBillboard.mfSizeX;
        const f32 lfSizeY    = lrBillboard.mfSizeY;
        const f32 lfRotation = lrBillboard.mfRotation;
        const u32 luColour   = PackVertexColour(lrBillboard.muDiffuse);

        // Half-extents rotated into screen space. The unrotated case skips the trig (and uses exact
        // zeros for the cross terms). lfDx/lfDy are the along-axis half-extents, lfSx/lfSy the
        // cross-axis ones; the corner formulas below combine them (matching the X360 add/sub tree).
        f32 lfDx, lfDy, lfSx, lfSy;
        if (lfRotation == 0.0f)
        {
            lfDx = lfSizeX *  0.5f;
            lfDy = lfSizeY * -0.5f;
            lfSx = 0.0f;
            lfSy = 0.0f;
        }
        else
        {
            const f32 lfHalfCos = static_cast<f32>(std::cos(-static_cast<f64>(lfRotation))) * 0.5f;
            const f32 lfHalfSin = static_cast<f32>(std::sin(-static_cast<f64>(lfRotation))) * 0.5f;
            lfDx = lfSizeX * lfHalfCos;
            lfDy = -(lfSizeY * lfHalfCos);
            lfSx = -(lfSizeX * lfHalfSin);
            lfSy = -(lfSizeY * lfHalfSin);
        }

        // Corner 0 (top-left) .. corner 3 (bottom-right).
        lpQuad[0].mv2Pos.x = (lfPosX - lfDx) - lfSy;
        lpQuad[0].mv2Pos.y = (lfPosY + lfDy) + lfSx;
        lpQuad[1].mv2Pos.x = (lfPosX + lfSy) - lfDx;
        lpQuad[1].mv2Pos.y = (lfPosY - lfDy) + lfSx;
        lpQuad[2].mv2Pos.x = (lfPosX + lfDx) - lfSy;
        lpQuad[2].mv2Pos.y = (lfPosY - lfSx) + lfDy;
        lpQuad[3].mv2Pos.x = (lfPosX + lfSy) + lfDx;
        lpQuad[3].mv2Pos.y = (lfPosY - lfSx) - lfDy;

        // Colour (the packed word is stored straight into the vertex's RGBA8 slot) + corner UVs.
        for (s32 liCorner = 0; liCorner < 4; ++liCorner)
            reinterpret_cast<u32&>(lpQuad[liCorner].mv4Colour) = luColour;

        lpQuad[0].mv2Tex0UV.x = lpUV[0]; lpQuad[0].mv2Tex0UV.y = lpUV[1];
        lpQuad[1].mv2Tex0UV.x = lpUV[2]; lpQuad[1].mv2Tex0UV.y = lpUV[3];
        lpQuad[2].mv2Tex0UV.x = lpUV[4]; lpQuad[2].mv2Tex0UV.y = lpUV[5];
        lpQuad[3].mv2Tex0UV.x = lpUV[6]; lpQuad[3].mv2Tex0UV.y = lpUV[7];

        // Stitch this quad into the strip: fill the previous billboard's two trailing vertices with
        // degenerate bridge vertices (dup of the previous quad's last corner, then dup of this quad's
        // first corner). Only after the first drawn billboard.
        if (liRenderedCount > 0)
        {
            lpQuad[-2] = lpQuad[-3];  // prev block slot 4 = prev quad's last corner (corner 3)
            lpQuad[-1] = lpQuad[0];   // prev block slot 5 = this quad's first corner (corner 0)
        }

        ++liRenderedCount;
    }

    if (liRenderedCount <= 0)
        return;

    // Submit the assembled triangle strip. Open a rendering block only if the buffer was not already
    // inside one (and close it symmetrically). The strip has (6 * drawn - 2) vertices: six per drawn
    // billboard minus the last block's two unused bridge slots.
    const bool lbWasInRenderingBlock = lpRenderBuffer->IsInARenderingBlock();
    if (!lbWasInRenderingBlock)
        lpRenderBuffer->BeginRendering();

    lpRenderBuffer->SetState(mpTextureState);
    lpRenderBuffer->SetState(mpBlendState);
    lpRenderBuffer->SetState(gpBillboardDepthStencilState);
    lpRenderBuffer->SetTransform(gBillboardScreenTransform);

    lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), // PRIMITIVETYPE_TRIANGLESTRIPS
                           maVertexList, static_cast<u32>(6 * liRenderedCount - 2));

    if (!lbWasInRenderingBlock)
        lpRenderBuffer->EndRendering();
}
}
