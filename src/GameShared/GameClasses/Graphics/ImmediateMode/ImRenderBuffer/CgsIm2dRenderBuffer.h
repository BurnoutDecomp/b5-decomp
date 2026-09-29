#pragma once

#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"

namespace CgsGraphics
{
    struct Im2d;

    // ARTIST 0x827F9BA0 / DecFIGS CgsIm2dRenderBuffer.h:102: a polymorphic
    // ImRenderBuffer<Basic2dColouredTexturedVertex>, separate from Im2d.
    struct Im2dRenderBuffer : public Im2dColouredTexturedRenderBuffer
    {
        virtual void Dispatch(Im2d* lpRenderer) const;
        using Im2dColouredTexturedRenderBuffer::Dispatch;

        void SetTransform(const Im2dTransform& lrTransform);
        void BatchTransformTextureBlendRenderStatic(const Im2dTransform& lrTransform,
            renderengine::Texture* lpTexture, const renderengine::BlendState* lpBlendState,
            renderengine::PrimitiveType lePrimitiveType,
            const Basic2dColouredTexturedVertex* lpVertices, u32 luNumVertices, u8 lu8Flags);
        using Im2dColouredTexturedRenderBuffer::PushMask;
        void PushMask(renderengine::Texture* lpTexture,
                      const Basic2dColouredTexturedVertex* lpaLogicalCorners);
    };

    // FLAG PC-platform leaf: the existing APT fixed-function command consumer
    // uses logical 1280x720 positions and byte-range colour constants. Translate
    // the native Im2d interface's NDC / unit colour constants at this boundary;
    // both producers then share the same ordered stream without a format tag.
    Im2dTransform Im2dTransformToLogicalPC(const Im2dTransform& lrTransform);
}
