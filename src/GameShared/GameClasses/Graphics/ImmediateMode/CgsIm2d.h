#pragma once

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"   // renderengine::ProgramVariableHandle
#include "rw/math/vpu/types.h"                                                 // rw::math::vpu::Vector3 / Vector4

// CgsGraphics::Im2d - the concrete screen-space immediate-mode renderer. Im2dBase<V>
// adds the transform stack on top of ImRenderer<V>; Im2d specialises it for the
// coloured+textured vertex and adds the (out-of-scope) mask state. Hierarchy from the
// DecFIGS DWARF (CgsIm2d.h). The mask/device internals are out of scope; in-scope
// callers only hold an Im2d* and drive it through the inherited render API.
namespace CgsGraphics
{
    // [DIAG] NOT IN THE X360 BINARY. The back-buffer scissor rect the last PushMask
    // installed {left, top, right, bottom}, published so the FLAPT mask path can log it
    // under BRN_PROP_DIAG without duplicating the fold. Storage in CgsIm2d.cpp.
    extern s32 gLastIm2dMaskRect[4];

    template <typename V>
    struct Im2dBase : public ImRenderer<V>
    {
        // [gateui r6] SetTransform publishes the batch transform as buffer STATE, exactly
        // as the console does: Im2dRenderBuffer::Dispatch @0x827F9BA0 case 0x16 hands the
        // four rows to ImRenderer<Basic2dColouredTexturedVertex>::SetTransform @0x823AC048,
        // which uploads them as GPU shader constants -- so EVERY run dispatched afterwards
        // (mesh batch, mask quad, glyph strip) is transformed by them until the next
        // SetTransform. The PC fold has no GPU constant to upload, so the state has to be
        // folded on the CPU at each submission point; before this round only the mesh path
        // (BatchTransformTextureBlendRenderStatic, from its own argument) did so, and the
        // mask + glyph paths silently dropped the transform. See CgsIm2d.cpp.
        void SetTransform(const Im2dTransform& lTransform);

        // [gateui r6] Open a rendering block. Resets the published transform to the
        // canonical screen->NDC transform (the same one BrnFlapt::FlaptRenderer::Construct
        // and BrnGame::LoadingScreenRenderer build) so a pass that never publishes one --
        // the debug 2D overlay, the movie player, the loading screen, all of which submit
        // ready-made 1280x720 logical coordinates -- folds through an exact identity, and
        // no transform survives from the previous pass. Then chains to ImRenderer<V>.
        void BeginRendering();

        // [gateui r6] Submit a reserved run (the RenderStart/RenderEnd text path) through
        // the published transform. This is the hop the FLAPT HUD text was missing: the
        // TextRenderer lays each glyph out in the text field's LOCAL box space and
        // BrnFlapt::FlaptRenderer::RenderTextField publishes the field's composed transform
        // immediately before submitting, so without the fold every FLAPT text field drew at
        // its authored box origin interpreted as raw screen pixels.
        void RenderEnd(renderengine::PrimitiveType lePrimitiveType,
                       const V* lpVertices, u32 luVertexCount);

        // Append a "set-transform + bound texture/blend + draw a static vertex run"
        // command to the open batch (X360 Im2dRenderBuffer::BatchTransformTextureBlendRenderStatic
        // @0x824590xx, the <Basic2dColouredTexturedVertex> instance, called by
        // BrnFlapt::FlaptRenderer::RenderMesh). luFlags is the ImCommandBatchTransformTextureBlendRender
        // KU_FLAG_SETTEXTURE(1)/KU_FLAG_SETBLEND(2) bitmask telling the command which of the
        // bound states actually changed this submission. The X360 ImRenderBuffer carries this
        // command API on the polymorphic buffer; on the PC fold (Im2dRenderBuffer == Im2d) it is
        // reached by name.
        void BatchTransformTextureBlendRenderStatic(const Im2dTransform& lrTransform,
                                                    renderengine::Texture* lpTexture,
                                                    const BlendState* lpBlendState,
                                                    renderengine::PrimitiveType lePrimitiveType,
                                                    const V* lpVertices,
                                                    u32 luNumVertices,
                                                    u8 luFlags);

        // Append a "push stencil mask" command to the open batch: the mask region is a
        // two-vertex (min-corner, max-corner) screen-space rectangle, drawn with lpTexture
        // (X360 Im2dRenderBuffer::PushMask(renderengine::Texture*, GuiGeometryMesh::Im2dVertex*)
        // @0x8246F638, the <Basic2dColouredTexturedVertex> instance, called by
        // BrnFlapt::FlaptRenderer::RenderMask). DWARF CgsIm2dRenderBuffer.h:193. On the PC fold
        // (Im2dRenderBuffer == Im2d) it is reached by name; GuiGeometryMesh::Im2dVertex is the
        // 20-byte screen-space coloured+textured vertex (== V here).
        // [gateui r6] The two corners arrive in the mask mesh's LOCAL space (RenderMask
        // reads them straight out of the file's vertex table) and are folded through the
        // published transform here -- see the SetTransform note above.
        void PushMask(renderengine::Texture* lpTexture, V* lpaMaskVertices);

        // Append a "pop stencil mask" command: undo the innermost PushMask (X360
        // ImCommandPopMask, DWARF CgsImRenderBuffer.h:208; the pop side of the Flapt
        // mask path, driven by BrnFlapt::FlaptRenderer::PopMask). On the PC fold the
        // mask is a scissor rect, so the pop disables the scissor test.
        void PopMask();

        Im2dTransform mCurrentTransform;
    };

    // The console Im2d adds the shader-driven pixel MASK (up to V_IM2D_MAX_MASK_COUNT nested
    // layers, applied by the masked program variants) and the boost-bar colour constants. The PC
    // GUI path masks with the scissor rect of the Im2dBase / render-buffer PushMask instead, so
    // nothing on the PC draw path drives these members yet; they are the console's own API.
    struct Im2d : public Im2dBase<Basic2dColouredTexturedVertex>
    {
        // The console's V_IM2D_MAX_MASK_COUNT (the PushMask / SaveMaskShaderConstants bound).
        static const u32 KU_IM2D_MAX_MASK_COUNT = 2;
        // One Im2dMask per program pair: the masked program of pair k reads maMask[k].
        static const u32 KU_IM2D_NUM_MASK_PROGRAMS = 4;

        struct Im2dMask
        {
            struct Im2dMaskLayer
            {
                renderengine::ProgramVariableHandle mPixelMaskPositionMinMax;
                renderengine::ProgramVariableHandle mPixelMaskUVStartEnd;
                renderengine::ProgramVariableHandle mPixelMaskUVDifference;
                rw::math::vpu::Vector4                             mvPixelMaskPositionMinMax;
                rw::math::vpu::Vector4                             mvPixelMaskUVStartEnd;
                rw::math::vpu::Vector4                             mvPixelMaskUVDifference;
                renderengine::TextureState*         mpMaskTextureState;
                renderengine::Texture*              mpMaskTexture;
            };

            Im2dMaskLayer                       maPixelMaskHandles[KU_IM2D_MAX_MASK_COUNT];
            rw::math::vpu::Vector4                             mvPixelMask;
            renderengine::ProgramVariableHandle mPixelMaskUseHandle;
        };

        // Push a mask layer: switch to the masked program variant on the first layer, record
        // the layer's rectangle / UVs / texture into every mask block, then upload the top layer.
        void PushMask(renderengine::TextureState* lpMaskTextureState,
                      renderengine::Texture* lpMaskTexture,
                      const Basic2dColouredTexturedVertex* lpaMaskVertices);
        // Pop the top mask layer; the last pop returns to the unmasked program variant.
        void PopMask();
        // Upload the boost bar's outer / inner colours to both program slots' constants.
        void PushBoostBarColours(rw::math::vpu::Vector3 lvOuterColour, rw::math::vpu::Vector3 lvInnerColour);

    protected:
        void SaveMaskShaderConstants(const Basic2dColouredTexturedVertex* lpaMaskVertices,
                                     renderengine::TextureState* lpMaskTextureState,
                                     renderengine::Texture* lpMaskTexture);
        void SetMaskPixelShaderState();

        Im2dMask                            maMask[KU_IM2D_NUM_MASK_PROGRAMS];
        u32                                 mu32NumMasks;
        renderengine::ProgramVariableHandle maBoostBarOuterColours[2];
        renderengine::ProgramVariableHandle maBoostBarInnerColours[2];
    };
}
