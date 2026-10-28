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

        // ARTIST 0x824502A8 / DecFIGS CgsIm2dRenderBuffer.h:244: the derived
        // command API accepts two VPU Vector3 values. Keep the base Vector4-ref
        // helper visible for existing native callers.
        using Im2dColouredTexturedRenderBuffer::PushBoostBarColours;
        void PushBoostBarColours(rw::math::vpu::Vector3 lrColour0,
                                 rw::math::vpu::Vector3 lrColour1);
    };

}
