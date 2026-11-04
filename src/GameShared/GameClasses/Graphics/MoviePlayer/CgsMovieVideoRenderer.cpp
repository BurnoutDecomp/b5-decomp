#include "SDKs/RenderEngineClub/MAIN/components/src/texture2d.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
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
        : mpTexture(0), mpTextureState(0), mpPixelData(0) {}

    MovieVideoRenderer::MovieVideoRenderer()
        : mpParentMoviePlayer(0), mpAllocator(0), mpReleaseFunc(0),
          mpReleaseFuncContext(0), muWidth(0), muHeight(0), muTextureIndex(0),
          mJob(0), miMoviePlayerPM1(-1), miMoviePlayerPM2(-1) {}

    MovieVideoRenderer::~MovieVideoRenderer()
    {
        for (u32 i = 0; i < KU_NUM_TEXTURES; ++i)
        {
            TextureInfoType& lrTexture = maTextureInfoTypes[i];
            if (lrTexture.mpTexture != 0)
            {
                lrTexture.mpTexture->Release();
                CGS_ASSERT(lrTexture.mpTextureState != 0,
                           "maTextureInfoTypes[i].mpTextureState");
            }
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
        CGS_ASSERT(lrResource.m_baseResources[0] != 0, "lResource.GetMemoryResource()");
        return new (lrResource.m_baseResources[0]) MovieVideoRenderer();
    }

    void MovieVideoRenderer::Init(EA::Allocator::ICoreAllocator* lpAllocator, unsigned int luWidth,
                                  unsigned int luHeight)
    {
        mpAllocator = lpAllocator;
        muWidth = luWidth;
        muHeight = luHeight;
        mJob.Clear();
        for (u32 i = 0; i < KU_NUM_TEXTURES; ++i)
            CreateOutputTexture(&maTextureInfoTypes[i], luWidth, luHeight);
    }

    // ARTIST 0x827F8120
    void MovieVideoRenderer::CreateOutputTexture(TextureInfoType* lpTextureTypeInfo,
                                                 u32 luWidth, u32 luHeight)
    {
        const char* const lpcFile =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\graphics\\MoviePlayer/CgsMovieVideoRenderer.cpp";
        renderengine::Texture2D::Parameters lTextureParameters;
        lTextureParameters.muFlags = 0;
        lTextureParameters.muWidth = luWidth;
        lTextureParameters.muHeight = luHeight;
        lTextureParameters.muNumLevels = 1;
        lTextureParameters.meFormat = renderengine::PIXELFORMAT_A8R8G8B8;

        const rw::ResourceDescriptor lTextureDescriptor =
            renderengine::Texture2D::GetResourceDescriptor(lTextureParameters);
        lpTextureTypeInfo->mTextureResource =
            mpParentMoviePlayer->mpRwAllocator->DoAllocate(lTextureDescriptor, 0);
        if (!lpTextureTypeInfo->mTextureResource.m_baseResources[2])
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpTextureTypeInfo->mTextureResource.GetPhysicalMemoryResource()",
                                       lpcFile, 566);
            CgsDev::Assert::EndAssert();
        }

        lpTextureTypeInfo->mpTexture =
            renderengine::Texture2D::Initialize(lpTextureTypeInfo->mTextureResource, lTextureParameters);
        renderengine::Texture::Locked lLockedTexture;
        if (!lpTextureTypeInfo->mpTexture->Lock(renderengine::Texture::LOCK_WRITE, 0, 0, lLockedTexture))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpTextureTypeInfo->mpTexture->Lock(rw::graphics::core::Texture2D::LOCK_WRITE, 0, lLockedTexture)",
                lpcFile, 579);
            CgsDev::Assert::EndAssert();
        }
        lpTextureTypeInfo->mpPixelData = static_cast<u32*>(lLockedTexture.mpPixelData);

        renderengine::TextureState::Parameters lStateParameters;
        lStateParameters.muAddressU = D3DTADDRESS_CLAMP;
        lStateParameters.muAddressV = D3DTADDRESS_CLAMP;
        lStateParameters.muMagFilter = D3DTEXF_LINEAR;
        lStateParameters.muMinFilter = D3DTEXF_LINEAR;
        lStateParameters.mpTexture = lpTextureTypeInfo->mpTexture;
        const rw::ResourceDescriptor lStateDescriptor =
            renderengine::TextureState::GetResourceDescriptor(lStateParameters);
        lpTextureTypeInfo->mTextureStateResource =
            mpParentMoviePlayer->mpRwAllocator->DoAllocate(lStateDescriptor, 0);
        lpTextureTypeInfo->mpTextureState =
            renderengine::TextureState::Initialize(lpTextureTypeInfo->mTextureStateResource, lStateParameters);
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

    // ARTIST 0x827EAE48. The movie shader unpacks the three planes from the
    // texture channels, so the CPU writes the planes without converting to RGB.
    void MovieVideoRenderer::EncodeYuvOntoRgbaTexture(rw::movie::VideoRenderable* lpVideoRenderable,
                                                     u32* lpPixels)
    {
        typedef rw::movie::VideoRenderable VideoRenderable;
        CGS_ASSERT(lpVideoRenderable, "lpVideoRenderable");
        const VideoRenderable::VideoFormat leFormat = lpVideoRenderable->GetFormat();
        CGS_ASSERT(leFormat == VideoRenderable::VIDEOFORMAT_YV12 ||
                   leFormat == VideoRenderable::VIDEOFORMAT_YV12_BORDER48,
                   "lpVideoRenderable->GetFormat()");
        CGS_ASSERT(lpVideoRenderable->GetNumBuffersUsed() == 3,
                   "lpVideoRenderable->GetNumBuffersUsed() == 3");

        u32 laBufSizes[3];
        VideoRenderable::GetDataBufSizes(lpVideoRenderable->GetWidth(), lpVideoRenderable->GetHeight(),
                                        leFormat, lpVideoRenderable->GetNumBuffersUsed(), laBufSizes);
        CGS_ASSERT(laBufSizes[0] == 4 * laBufSizes[1] && laBufSizes[0] == 4 * laBufSizes[2],
                   "laBufSizes[0] == 4 * laBufSizes[1] && laBufSizes[0] == 4 * laBufSizes[2]");

        const u32 KU_PIXELS_PER_LOOP = 8;
        CGS_ASSERT(lpVideoRenderable->GetWidth() % KU_PIXELS_PER_LOOP == 0,
                   "lpVideoRenderable->GetWidth() % KU_PIXELS_PER_LOOP == 0");

        const u8* lpLuma = lpVideoRenderable->GetData(0);
        const u8* lpChroma0 = lpVideoRenderable->GetData(1);
        const u8* lpChroma1 = lpVideoRenderable->GetData(2);
        for (u32 luRow = 0; luRow < lpVideoRenderable->GetHeight(); ++luRow)
        {
            for (u32 luPixel = 0; luPixel < lpVideoRenderable->GetWidth(); luPixel += KU_PIXELS_PER_LOOP)
            {
                for (u32 i = 0; i < KU_PIXELS_PER_LOOP; ++i)
                {
                    const u32 luX = luPixel + i;
                    *lpPixels++ = (static_cast<u32>(lpLuma[luX]) << 16)
                                | (static_cast<u32>(lpChroma0[luX / 2]) << 8)
                                | lpChroma1[luX / 2];
                }
            }
            lpLuma += static_cast<s32>(lpVideoRenderable->GetStride(0));
            if (luRow & 1)
            {
                lpChroma0 += static_cast<s32>(lpVideoRenderable->GetStride(1));
                lpChroma1 += static_cast<s32>(lpVideoRenderable->GetStride(2));
            }
        }
    }

    // ARTIST 0x827FB060
    void MovieVideoRenderer::Render(s32 liStartRow, rw::movie::VideoRenderable* lpRenderable, bool lbStep)
    {
        CGS_ASSERT(lpRenderable, "lpNewVideoRenderable");

        TextureInfoType& lrTexture = maTextureInfoTypes[muTextureIndex];
        if (lbStep)
            muTextureIndex = (muTextureIndex + 1) & (KU_NUM_TEXTURES - 1);

        const u32 luWidth = lrTexture.mpTexture->GetWidth();
        EncodeYuvOntoRgbaTexture(lpRenderable, lrTexture.mpPixelData + luWidth * liStartRow);

        CGS_ASSERT(mpParentMoviePlayer->mpIm2dRenderBuffer, "mpParentMoviePlayer->mpIm2dRenderBuffer");
        Im2dRenderBuffer* lpBuffer = mpParentMoviePlayer->mpIm2dRenderBuffer;
        const ImRendererBase::StateLibrary& lrStateLibrary = ImRendererBase::GetStateLibrary();
        lpBuffer->BeginRendering();
        lpBuffer->SetState(lrStateLibrary.mpBlendState_Standard);
        lpBuffer->SetState(lrStateLibrary.mpRasterizerState_CullNone);

        const s32 liFrame = lpRenderable->GetFrameNumber();
        f32 lfFadeIn = 1.0f;
        if (liFrame < mpParentMoviePlayer->miCrossfadeInFrames)
            lfFadeIn = static_cast<f32>(liFrame) / static_cast<f32>(mpParentMoviePlayer->miCrossfadeInFrames);
        f32 lfFadeOut = 1.0f;
        if (-liFrame < mpParentMoviePlayer->miCrossfadeOutFrames)
            lfFadeOut = static_cast<f32>(-liFrame) / static_cast<f32>(mpParentMoviePlayer->miCrossfadeOutFrames);
        const f32 lfFade = lfFadeIn < lfFadeOut ? lfFadeIn : lfFadeOut;
        const u8 luAlpha = static_cast<u8>(static_cast<s64>(255.0f * lfFade));

        const f32 lfLeft = mpParentMoviePlayer->mfRectLeft;
        const f32 lfTop = mpParentMoviePlayer->mfRectTop;
        const f32 lfRight = mpParentMoviePlayer->mfRectRight;
        const f32 lfBottom = mpParentMoviePlayer->mfRectBottom;
        const Basic2dColouredTexturedVertex laVertices[4] = {
            { { lfLeft, lfTop },       { 255, 255, 255, luAlpha }, { 0.0f, 1.0f } },
            { { lfLeft, lfBottom },    { 255, 255, 255, luAlpha }, { 0.0f, 0.0f } },
            { { lfRight, lfTop },      { 255, 255, 255, luAlpha }, { 1.0f, 1.0f } },
            { { lfRight, lfBottom },   { 255, 255, 255, luAlpha }, { 1.0f, 0.0f } }
        };

        lpBuffer->SetState(lrTexture.mpTextureState);
        lpBuffer->SetTransform(Im2dTransform::GetAspectCorrected());
        lpBuffer->SetProgram(2);
        lpBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);
        lpBuffer->SetProgram(0);
        lpBuffer->EndRendering();

        if (mpReleaseFunc)
            mpReleaseFunc(lpRenderable, mpReleaseFuncContext);
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
