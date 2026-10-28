#pragma once
#include "SDKs/EATech/rwmovie/ivideorenderer.h"
#include "SDKs/EATech/eajobs/job.h"
#include "GameShared/Jobs/MovieTexture/MovieTexture.h"
#include "rw/rwcore_structs.h"
namespace BrnGui { class MovieManager; }
namespace CgsGraphics
{
    class MoviePlayer;

    // Original owner: four separately allocated texture/state resources.
    // ARTIST 827EF2E0/827FB000/827FB060, DecFIGS CgsMovieVideoRenderer.h.
    class MovieVideoRenderer : public rw::movie::IVideoRenderer
    {
    public:
        static const u32 KU_NUM_TEXTURES = 4;
        struct TextureInfoType
        {
            rw::Resource mTextureResource;
            renderengine::Texture2D* mpTexture;
            rw::Resource mTextureStateResource;
            renderengine::TextureState* mpTextureState;
            u32* mpPixelData;
            TextureInfoType();
        };

        MovieVideoRenderer();
        ~MovieVideoRenderer() ;
        static rw::ResourceDescriptor GetResourceDescriptor();
        static MovieVideoRenderer* Initialize(rw::Resource& lrResource);
        void SetParentMoviePlayer(MoviePlayer* lpParent) { mpParentMoviePlayer = lpParent; }
        void Init(EA::Allocator::ICoreAllocator*, unsigned int, unsigned int) ;
        unsigned int GetNumberOfVideoBuffers(rw::movie::VideoRenderable::VideoFormat) ;
        float GetRenderingDelay() ;
        void SetReleaseRenderableCallback(rw::movie::ReleaseVideoRenderableFunc*, rw::movie::MovieDecoder*) ;
        void Render(rw::movie::VideoRenderable*) ;
        void Render(s32 liStartRow, rw::movie::VideoRenderable* lpRenderable, bool lbStep);
        void SetPositionX(float) ;
        void SetPositionY(float) ;
        void SetPositionZ(float) ;
        float GetPositionX() ;
        float GetPositionY() ;
        float GetPositionZ() ;
        void SetScaleX(float) ;
        void SetScaleY(float) ;
        float GetScaleX() ;
        float GetScaleY() ;

    private:
        friend class MoviePlayer;
        friend class BrnGui::MovieManager;
        void CreateOutputTexture(TextureInfoType*, u32, u32);
        void EncodeYuvOntoRgbaTexture(rw::movie::VideoRenderable*, u32*);
        MoviePlayer* mpParentMoviePlayer;
        EA::Allocator::ICoreAllocator* mpAllocator;
        rw::movie::ReleaseVideoRenderableFunc* mpReleaseFunc;
        rw::movie::MovieDecoder* mpReleaseFuncContext;
        u32 muWidth, muHeight;
        TextureInfoType maTextureInfoTypes[KU_NUM_TEXTURES];
        u32 muTextureIndex;
        MovieTextureData mJobData;
        EA::Jobs::Job mJob;
        s32 miMoviePlayerPM1, miMoviePlayerPM2;
    };
    MovieVideoRenderer* MovieVideoRendererVectorDeletingDtor(MovieVideoRenderer*, char);
}
