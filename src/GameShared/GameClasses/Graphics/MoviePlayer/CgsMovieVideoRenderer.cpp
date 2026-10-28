#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h"
#include <cstring>

#include <new>   // ::operator delete

// CgsGraphics::MovieVideoRenderer position/scale/delay control surface, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The renderer always presents the movie frame full-screen at the default
// transform: position queries assert-false and return 0, scale queries assert-false and return the
// identity 1, the position setters accept only 0 (flt_82001CC0), the scale setters accept only 1
// (flt_82001C98), and the renderer adds no rendering delay.

namespace CgsGraphics
{
    MovieVideoRenderer::TextureInfoType::TextureInfoType()
        : mpTexture(nullptr), mpTextureState(nullptr), mpPixelData(nullptr) {}

    MovieVideoRenderer::MovieVideoRenderer()
        : mpParentMoviePlayer(nullptr), mpAllocator(nullptr), mpReleaseFunc(nullptr),
          mpReleaseFuncContext(nullptr), muWidth(0), muHeight(0), muTextureIndex(0),
          mJob(nullptr), miMoviePlayerPM1(-1), miMoviePlayerPM2(-1) {}

    MovieVideoRenderer::~MovieVideoRenderer()
    {
        for (TextureInfoType& lrTexture : maTextureInfoTypes)
        {
            if (lrTexture.mpTexture != nullptr)
                renderengine::Texture::Destroy(lrTexture.mpTexture);
            CGS_ASSERT(lrTexture.mpTexture == nullptr || lrTexture.mpTextureState != nullptr,
                       "maTextureInfoTypes[i].mpTextureState");
        }
    }

    rw::ResourceDescriptor MovieVideoRenderer::GetResourceDescriptor()
    {
        rw::ResourceDescriptor lDescriptor;
        // ARTIST 827FAE60..8C requests 0x500 bytes, aligned to 128, for the
        // 0x4D8-byte console object. Apply that same rounding to native sizeof.
        lDescriptor.m_baseResourceDescriptors[0].m_size = 0x500u;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 128;
        return lDescriptor;
    }

    MovieVideoRenderer* MovieVideoRenderer::Initialize(rw::Resource& lrResource)
    {
        CGS_ASSERT(lrResource.m_baseResources[0] != nullptr, "lResource.GetMemoryResource()");
        return new (lrResource.m_baseResources[0]) MovieVideoRenderer();
    }

    void MovieVideoRenderer::Init(EA::Allocator::ICoreAllocator* lpAllocator, unsigned int luWidth,
                                  unsigned int luHeight)
    {
        mpAllocator = lpAllocator;
        muWidth = luWidth;
        muHeight = luHeight;
        mJob.Clear();
        for (TextureInfoType& lrTexture : maTextureInfoTypes)
            CreateOutputTexture(&lrTexture, luWidth, luHeight);
    }

    unsigned int MovieVideoRenderer::GetNumberOfVideoBuffers(rw::movie::VideoRenderable::VideoFormat)
    {
        return 3; // ARTIST vtable slot 2 folds to 826D7F68.
    }

    void MovieVideoRenderer::SetReleaseRenderableCallback(rw::movie::ReleaseVideoRenderableFunc* lpFunc,
                                                          rw::movie::MovieDecoder* lpContext)
    {
        mpReleaseFunc = lpFunc;
        mpReleaseFuncContext = lpContext;
    }

    void MovieVideoRenderer::Render(rw::movie::VideoRenderable* lpRenderable)
    {
        Render(0, lpRenderable, true); // ARTIST 827FF2B0.
    }

    float MovieVideoRenderer::GetPositionY()
    {
        CGS_ASSERT(false, "false"); // ARTIST 827EAC78.
        return 0.0f;
    }
    // @ 0x827EAC30. Position-X query is unsupported: asserts-false and returns 0.
    float MovieVideoRenderer::GetPositionX()
    {
        CGS_ASSERT(false, "false");
        return 0.0f;
    }

    // @ 0x827EACC0. Position-Z query is unsupported: asserts-false and returns 0.
    float MovieVideoRenderer::GetPositionZ()
    {
        CGS_ASSERT(false, "false");
        return 0.0f;
    }

    // @ 0x827EAB30. This renderer adds no rendering delay -- returns 0.
    float MovieVideoRenderer::GetRenderingDelay()
    {
        return 0.0f;
    }

    // @ 0x827EADA8. Scale-X query is unsupported: asserts-false and returns identity 1.
    float MovieVideoRenderer::GetScaleX()
    {
        CGS_ASSERT(false, "false");
        return 1.0f;
    }

    // @ 0x827EADF0. Scale-Y query is unsupported: asserts-false and returns identity 1.
    float MovieVideoRenderer::GetScaleY()
    {
        CGS_ASSERT(false, "false");
        return 1.0f;
    }

    // @ 0x827EAB40. Repositioning in X is unsupported: only the default value (0) is accepted;
    // the X360 compares the argument against 0.0 (flt_82001CC0) and asserts only when they differ.
    void MovieVideoRenderer::SetPositionX(float lfPositionX)
    {
        if (lfPositionX != 0.0f)
        {
            CGS_ASSERT(false, "false");
        }
    }

    // @ 0x827EAB90. Default position Y is 0.0f (flt_82001CC0); any other value asserts.
    void MovieVideoRenderer::SetPositionY(float lfPositionY)
    {
        CGS_ASSERT(lfPositionY == 0.0f, "false");
    }

    // @ 0x827EABE0. Default position Z is 0.0f (flt_82001CC0); any other value asserts.
    void MovieVideoRenderer::SetPositionZ(float lfPositionZ)
    {
        CGS_ASSERT(lfPositionZ == 0.0f, "false");
    }

    // @ 0x827EAD08. Default X scale is 1.0f (flt_82001C98); any other value asserts.
    void MovieVideoRenderer::SetScaleX(float lfScaleX)
    {
        CGS_ASSERT(lfScaleX == 1.0f, "false");
    }

    // @ 0x827EAD58. Default Y scale is 1.0f (flt_82001C98); any other value asserts.
    void MovieVideoRenderer::SetScaleY(float lfScaleY)
    {
        CGS_ASSERT(lfScaleY == 1.0f, "false");
    }

    // MovieVideoRenderer::'vector deleting destructor' @ 0x827EF3A0. The X360 emits the standard
    // MSVC deleting-destructor thunk:
    //   ~MovieVideoRenderer(this);           // bl CgsGraphics__MovieVideoRenderer___MovieVideoRenderer
    //   if (flags & 1)                       // clrlwi r11,r30,31; cmplwi; beq
    //       operator delete(this);           // bl operator_delete (global ::operator delete)
    //   return this;                         // r3 = this on both paths
    // The destructor it calls is the original four-texture teardown above (0x827EAA98); this thunk
    // only wires up the destroy-then-conditionally-free sequence the binary performs.
    MovieVideoRenderer* MovieVideoRendererVectorDeletingDtor(MovieVideoRenderer* lpRenderer, char lcFlags)
    {
        lpRenderer->~MovieVideoRenderer();
        if (lcFlags & 1)
        {
            ::operator delete(lpRenderer);
        }
        return lpRenderer;
    }
}
