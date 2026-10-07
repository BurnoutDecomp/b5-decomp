#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h"
#include <cstring>
extern "C" {
#include <libswscale/swscale.h>
}

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
        lDescriptor.m_baseResourceDescriptors[0].m_size = (sizeof(MovieVideoRenderer) + 127u) & ~127u;
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

    void MovieVideoRenderer::CreateOutputTexture(TextureInfoType* lpInfo, u32 luWidth, u32 luHeight)
    {
        renderengine::Texture2D::Parameters lParameters = {};
        lParameters.muWidth = luWidth;
        lParameters.muHeight = luHeight;
        lParameters.muDepth = 1;
        lParameters.muNumLevels = 1;
        // FLAG PC-platform leaf: FFmpeg supplies BGRA, rather than Xenos YUV
        // planes packed into the original GPU movie texture.
        lParameters.muFormat = 21; // D3DFMT_A8R8G8B8.
        rw::ResourceDescriptor lDescriptor;
        renderengine::Texture2D::GetResourceDescriptor(&lDescriptor, &lParameters);
        lpInfo->mTextureResource = mpParentMoviePlayer->mpAllocator->DoAllocate(lDescriptor, nullptr);
        // Native allocation failure cannot transfer ownership to a fresh heap
        // wrapper; the original arena must own every successfully created slot.
        if (lpInfo->mTextureResource.m_baseResources[0] == nullptr)
            return;
        lpInfo->mpTexture = renderengine::Texture2D::Initialize(&lpInfo->mTextureResource, &lParameters);

        renderengine::TextureState::Parameters lState = {};
        lState.muAddressU = lState.muAddressV = 2;
        lState.muMagFilter = lState.muMinFilter = 1;
        lState.muMipFilter = 1;
        lState.muMaxAnisotropy = 13;
        lState.muField10 = 1;
        lState.mu8Field43 = lState.mu8Field44 = 1;
        lState.mpTexture = lpInfo->mpTexture;
        renderengine::TextureState::GetResourceDescriptor(&lDescriptor);
        lpInfo->mTextureStateResource = mpParentMoviePlayer->mpAllocator->DoAllocate(lDescriptor, nullptr);
        if (lpInfo->mTextureStateResource.m_baseResources[0] == nullptr)
            return;
        lpInfo->mpTextureState = renderengine::TextureState::Initialize(&lpInfo->mTextureStateResource, &lState);
    }

    // FLAG PC-platform leaf: native codec output and D3D upload replace the
    // console's MovieTexture encoding job. The caller still selects the original
    // texture slot, emits the original draw sequence, and owns the same lifetime.
    void MovieVideoRenderer::EncodeYuvOntoRgbaTexture(rw::movie::VideoRenderable* lpRenderable, u32* lpPixels)
    {
        const u32 luWidth = lpRenderable->GetWidth();
        const u32 luHeight = lpRenderable->GetHeight();
        if (lpRenderable->GetFormat() == rw::movie::VideoRenderable::VIDEOFORMAT_ARGB32)
        {
            for (u32 luRow = 0; luRow < luHeight; ++luRow)
                std::memcpy(lpPixels + luRow * muWidth,
                    lpRenderable->GetData(0) + luRow * lpRenderable->GetStride(0), luWidth * 4u);
            return;
        }
        const AVPixelFormat leFormat = lpRenderable->GetFormat() == rw::movie::VideoRenderable::VIDEOFORMAT_RGB24
            ? AV_PIX_FMT_RGB24 : AV_PIX_FMT_YUV420P;
        SwsContext* lpContext = sws_getContext(luWidth, luHeight, leFormat, luWidth, luHeight,
            AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        CGS_ASSERT(lpContext != nullptr, "movie colour conversion");
        const u8* lapData[4] = {lpRenderable->GetData(0), lpRenderable->GetData(1), lpRenderable->GetData(2), nullptr};
        int laiStrides[4] = {static_cast<int>(lpRenderable->GetStride(0)),
            static_cast<int>(lpRenderable->GetStride(1)), static_cast<int>(lpRenderable->GetStride(2)), 0};
        u8* lapOut[4] = {reinterpret_cast<u8*>(lpPixels), nullptr, nullptr, nullptr};
        int laiOutStrides[4] = {static_cast<int>(muWidth * 4u), 0, 0, 0};
        sws_scale(lpContext, lapData, laiStrides, 0, luHeight, lapOut, laiOutStrides);
        sws_freeContext(lpContext);
    }

    void MovieVideoRenderer::Render(rw::movie::VideoRenderable* lpRenderable)
    {
        Render(0, lpRenderable, true); // ARTIST 827FF2B0.
    }

    void MovieVideoRenderer::Render(s32 liStartRow, rw::movie::VideoRenderable* lpRenderable, bool lbStep)
    {
        CGS_ASSERT(lpRenderable != nullptr, "lpNewVideoRenderable");
        TextureInfoType& lrTexture = maTextureInfoTypes[muTextureIndex];
        if (lbStep)
            muTextureIndex = (muTextureIndex + 1u) & 3u;

        // Native image memory can only be addressed while the D3D lock is held.
        renderengine::Texture::LockInfo lLock;
        renderengine::Texture::Lock(lrTexture.mpTexture, 0, 0, 0, &lLock);
        CGS_ASSERT(lLock.mpBits != nullptr, "lpTextureTypeInfo->mpPixelData");
        lrTexture.mpPixelData = static_cast<u32*>(lLock.mpBits);
        // Preserve the row pitch supplied by the native device. Encode accepts
        // a packed destination, so conversion first uses a tight CPU image.
        u32* lpPacked = new u32[muWidth * lpRenderable->GetHeight()];
        EncodeYuvOntoRgbaTexture(lpRenderable, lpPacked);
        for (u32 luRow = 0; luRow < lpRenderable->GetHeight(); ++luRow)
            std::memcpy(static_cast<u8*>(lLock.mpBits) + (liStartRow + luRow) * lLock.muPitch,
                        lpPacked + luRow * muWidth, muWidth * 4u);
        delete[] lpPacked;
        renderengine::Texture::Unlock(lrTexture.mpTexture, &lLock);
        lrTexture.mpPixelData = nullptr;

        Im2dRenderBuffer* lpBuffer = mpParentMoviePlayer->mpIm2dRenderBuffer;
        CGS_ASSERT(lpBuffer != nullptr, "mpParentMoviePlayer->mpIm2dRenderBuffer");
        lpBuffer->BeginRendering();
        lpBuffer->SetState(CgsGui::gpGuiBlendStateStandard);
        lpBuffer->SetState(CgsGui::gpGuiRasterizerStateCullNone);
        const f32 lfAlpha = mpParentMoviePlayer->ComputeCrossfadeAlpha(mpParentMoviePlayer->mfLastElapsedSec);
        const RGBA8 lColour = {255, 255, 255, static_cast<u8>(lfAlpha * 255.0f)};
        const f32 lfLeft = mpParentMoviePlayer->mfRectLeft;
        const f32 lfTop = mpParentMoviePlayer->mfRectTop;
        const f32 lfRight = mpParentMoviePlayer->mfRectRight;
        const f32 lfBottom = mpParentMoviePlayer->mfRectBottom;
        Basic2dColouredTexturedVertex laVertices[4] = {
            {{lfLeft, lfTop}, lColour, {0.0f, 1.0f}},
            {{lfLeft, lfBottom}, lColour, {0.0f, 0.0f}},
            {{lfRight, lfTop}, lColour, {1.0f, 1.0f}},
            {{lfRight, lfBottom}, lColour, {1.0f, 0.0f}}};
        // FFmpeg's BGRA image is top-down; the original GPU texture is flipped.
        for (auto& lrVertex : laVertices)
            lrVertex.mv2Tex0UV.y = 1.0f - lrVertex.mv2Tex0UV.y;
        lpBuffer->SetState(lrTexture.mpTextureState);
        // This existing shared constant already carries the native logical units.
        lpBuffer->Im2dColouredTexturedRenderBuffer::SetTransform(CgsGui::gBillboardScreenTransform);
        lpBuffer->SetProgram(2);
        lpBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);
        lpBuffer->SetProgram(0);
        lpBuffer->EndRendering();
        if (mpReleaseFunc != nullptr)
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
