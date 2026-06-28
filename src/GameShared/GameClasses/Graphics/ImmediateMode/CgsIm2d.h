#pragma once

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"

// CgsGraphics::Im2d - the concrete screen-space immediate-mode renderer. Im2dBase<V>
// adds the transform stack on top of ImRenderer<V>; Im2d specialises it for the
// coloured+textured vertex and adds the (out-of-scope) mask state. Hierarchy from the
// DecFIGS DWARF (CgsIm2d.h). The mask/device internals are out of scope; in-scope
// callers only hold an Im2d* and drive it through the inherited render API.
namespace CgsGraphics
{
    template <typename V>
    struct Im2dBase : public ImRenderer<V>
    {
        void SetTransform(const Im2dTransform& lTransform);

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
        void PushMask(renderengine::Texture* lpTexture, V* lpaMaskVertices);

        Im2dTransform mCurrentTransform;
    };

    struct Im2d : public Im2dBase<Basic2dColouredTexturedVertex>
    {
    };
}
