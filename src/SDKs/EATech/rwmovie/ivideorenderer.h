#pragma once
#include "SDKs/EATech/rwmovie/videorenderable.h"
namespace EA { namespace Allocator { class ICoreAllocator; } }
namespace rw { namespace movie
{
    class MovieDecoder;
    using ReleaseVideoRenderableFunc = void(VideoRenderable*, MovieDecoder*);

    // ARTIST 820D5798/820D5A48 and DecFIGS irenderer.h: sixteen slots.
    class IVideoRenderer
    {
    public:
        virtual ~IVideoRenderer() = default;
        virtual void Init(EA::Allocator::ICoreAllocator*, unsigned int, unsigned int) = 0;
        virtual unsigned int GetNumberOfVideoBuffers(VideoRenderable::VideoFormat) = 0;
        virtual float GetRenderingDelay() = 0;
        virtual void SetReleaseRenderableCallback(ReleaseVideoRenderableFunc*, MovieDecoder*) = 0;
        virtual void SetPositionX(float) = 0;
        virtual void SetPositionY(float) = 0;
        virtual void SetPositionZ(float) = 0;
        virtual float GetPositionX() = 0;
        virtual float GetPositionY() = 0;
        virtual float GetPositionZ() = 0;
        virtual void SetScaleX(float) = 0;
        virtual void SetScaleY(float) = 0;
        virtual float GetScaleX() = 0;
        virtual float GetScaleY() = 0;
        virtual void Render(VideoRenderable*) = 0;
    };
} }
