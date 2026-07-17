#include "GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h"

#include "GameSource/Gui/BrnGuiCache.h"                                       // BrnGui::GuiCache
#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"                            // BrnFlapt::FlaptRenderer
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"                             // BrnFlapt::FlaptFile::SetSpecialTexture
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"         // CgsNetwork::NetworkTexture::GetFormat
#include "GameShared/GameClasses/Network/Utilities/CgsNetworkImageConverter.h" // CgsNetwork::NetworkImageConverter
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "pc/gcm/renderengine/pixelformat.h"                                  // renderengine::PixelFormat

#include <cstring>   // memset

// ============================================================================
// BrnGui::NetworkPlayerImageRenderer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// Behaviour + calling convention are authoritative from the X360 ARTIST asm; the
// DecFIGS DWARF supplies declaration shape (member names/types, enum values). See
// the header for the full attested member layout. Triple-buffered network mugshot
// images are unpacked (per pixel format: A1R5G5B5 -> normal, G8B8 -> YUY2, DXT1 ->
// compressed) and surfaced to the Flapt GUI as a named special texture.
// ============================================================================

namespace BrnGui
{
    namespace
    {
        // The resource the GuiCache must have loaded before the renderer reports ready
        // (DWARF: maResourcesToLoad / muNumResourcesToLoad, file-static). The X360 build
        // watches a single resource tuple; its exact id is the .rdata value the cache key
        // resolves to (left as the recovered id). FLAG: the watched resource id/type are
        // the recovered .rdata values (unk_82F2571C / dword_82F25724).
        const CgsGui::sResourceTuple maResourcesToLoad[1] =
        {
            { 0u, CgsGui::E_GUI_RESOURCETYPE_FSM },
        };
        const u32 muNumResourcesToLoad = 1u;

        // The id the default texture is fetched from the GuiCache under (X360 GetLoadedResource
        // arg 0xEC = 236). FLAG: recovered constant.
        const u32 KU_DEFAULT_TEXTURE_RESOURCE_ID = 236u;

        // The Flapt special-texture name the GUI mesh samples for this component.
        const char KAC_SPECIAL_TEXTURE_NAME[] = "CustomComponentTexture.tif";

        // The renderer's component CgsID (GetID @0x82445CA8 returns this 64-bit constant).
        const CgsID KID_NETWORK_PLAYER_IMAGE = 0xA6864CCE2CE23B68ULL;
    }

    // ---- Construct @ 0x82445A50 --------------------------------------------------
    // Chain the base construct, then clear the whole buffer/locked/flag set and seed the
    // staging + buffer-index + frame-count state.
    void NetworkPlayerImageRenderer::Construct()
    {
        CustomRenderComponentInterface::Construct();

        mpGuiCache       = 0;
        mpFlaptRenderer  = 0;
        mpHeapAllocator  = 0;
        mpTextureAllocator = 0;
        mePrepareStage   = E_PREPARESTAGE_START;
        meReleaseStage   = E_RELEASESTAGE_START;
        mpDefaultTexture = 0;

        for (s32 liDisplay = 0; liDisplay < KI_MAX_NUM_TEXTURES_TO_DISPLAY; ++liDisplay)
        {
            for (s32 liBuffer = 0; liBuffer < KI_NUM_TEXTURES_TO_BUFFER; ++liBuffer)
            {
                maapTextureBuffer[liDisplay][liBuffer]     = 0;
                maapYUY2TextureBuffer[liDisplay][liBuffer] = 0;
                maabRenderTexture[liDisplay][liBuffer]           = false;
                maabRenderCompressedTexture[liDisplay][liBuffer] = false;
                maabRenderYUY2Texture[liDisplay][liBuffer]       = false;
            }
            mapCompressedTextureBuffer[liDisplay] = 0;
        }

        miCurrentRenderTexture    = KI_INITIAL_RENDER_FROM_TEXTURE;   // 1
        miCurrentCopyToTexture    = KI_INITIAL_COPY_TO_TEXTURE;       // 2
        miClearTexturesFrameCount = KI_CLEAR_TEXTURES_NOT_SET;        // -1
        mbRenderTexture           = false;
        mbUseDefaultTexture       = false;
    }

    // ---- SetFlaptRenderer (DWARF cpp:121) ---------------------------------------
    void NetworkPlayerImageRenderer::SetFlaptRenderer(BrnFlapt::FlaptRenderer* lpFlaptRenderer)
    {
        mpFlaptRenderer = lpFlaptRenderer;
    }

    // ---- Prepare @ 0x82451560 ----------------------------------------------------
    // The staged async prepare. Each call advances the state machine by one step and
    // returns true only once E_PREPARESTAGE_DONE is reached.
    bool NetworkPlayerImageRenderer::Prepare(CgsGui::GuiEventQueueSmall* /*lpEventQueue*/,
                                             rw::IResourceAllocator* lpHeapAllocator,
                                             rw::IResourceAllocator* lpTextureAllocator)
    {
        switch (mePrepareStage)
        {
        case E_PREPARESTAGE_START:
            mePrepareStage     = E_PREPARESTAGE_START;
            mpHeapAllocator    = lpHeapAllocator;
            mpTextureAllocator = lpTextureAllocator;
            // fall through into E_PREPARESTAGE_TEXTURES

        case E_PREPARESTAGE_TEXTURES:
        {
            mePrepareStage = E_PREPARESTAGE_TEXTURES;

            // Build the three resource descriptors: normal (A1R5G5B5), YUY2 (G8B8) and
            // compressed (DXT1), all 160x120, one mip. The X360 fills a Texture2D::Parameters
            // for each then snapshots the resulting descriptor (10 u32s) onto the stack.
            renderengine::Texture2D::Parameters lNormalParams;
            lNormalParams.muWidth     = KU_TEXTURE_WIDTH;
            lNormalParams.muHeight    = KU_TEXTURE_HEIGHT;
            lNormalParams.muDepth     = 1u;
            lNormalParams.muNumLevels = 1u;
            lNormalParams.muFormat    = renderengine::PIXELFORMAT_A1R5G5B5;
            lNormalParams.muUsage     = 0u;
            lNormalParams.mauReserved[0] = 0u;
            lNormalParams.mauReserved[1] = 0u;

            renderengine::Texture2D::Parameters lYUY2Params = lNormalParams;
            lYUY2Params.muFormat = renderengine::PIXELFORMAT_G8B8;

            renderengine::Texture2D::Parameters lCompressedParams = lNormalParams;
            lCompressedParams.muFormat = renderengine::PIXELFORMAT_DXT1;

            renderengine::Texture2D::ResourceDescriptor lNormalDesc;
            renderengine::Texture2D::ResourceDescriptor lYUY2Desc;
            renderengine::Texture2D::ResourceDescriptor lCompressedDesc;
            renderengine::Texture2D::GetResourceDescriptor(&lNormalDesc, &lNormalParams);
            renderengine::Texture2D::GetResourceDescriptor(&lYUY2Desc, &lYUY2Params);
            renderengine::Texture2D::GetResourceDescriptor(&lCompressedDesc, &lCompressedParams);

            // Allocate + lock + clear every buffered texture from the texture allocator.
            for (s32 liDisplay = 0; liDisplay < KI_MAX_NUM_TEXTURES_TO_DISPLAY; ++liDisplay)
            {
                for (s32 liBuffer = 0; liBuffer < KI_NUM_TEXTURES_TO_BUFFER; ++liBuffer)
                {
                    // Normal (A1R5G5B5) texture into the resource memory the allocator hands out.
                    renderengine::Texture2D* lpNormal = renderengine::Texture2D::Initialize(
                        &lNormalDesc, &lNormalParams);
                    maapTextureBuffer[liDisplay][liBuffer] = lpNormal;
                    CGS_ASSERT(maapTextureBuffer[liDisplay][liBuffer],
                               "maapTextureBuffer[ liTextureIndex ][ liIndex ]");
                    renderengine::Texture::Lock(maapTextureBuffer[liDisplay][liBuffer], 0, 0, 0,
                                                &maaLockedTextures[liDisplay][liBuffer]);
                    memset(maaLockedTextures[liDisplay][liBuffer].mpPixelData, 0,
                           maaLockedTextures[liDisplay][liBuffer].muStride *
                               maaLockedTextures[liDisplay][liBuffer].muHeight);

                    // YUY2 (G8B8) texture.
                    renderengine::Texture2D* lpYUY2 = renderengine::Texture2D::Initialize(
                        &lYUY2Desc, &lYUY2Params);
                    maapYUY2TextureBuffer[liDisplay][liBuffer] = lpYUY2;
                    CGS_ASSERT(maapYUY2TextureBuffer[liDisplay][liBuffer],
                               "maapYUY2TextureBuffer[ liTextureIndex ][ liIndex ]");
                    renderengine::Texture::Lock(maapYUY2TextureBuffer[liDisplay][liBuffer], 0, 0, 0,
                                                &maaLockedYUY2Textures[liDisplay][liBuffer]);
                    memset(maaLockedYUY2Textures[liDisplay][liBuffer].mpPixelData, 0,
                           maaLockedYUY2Textures[liDisplay][liBuffer].muStride *
                               maaLockedYUY2Textures[liDisplay][liBuffer].muHeight);

                    maabRenderTexture[liDisplay][liBuffer]           = false;
                    maabRenderCompressedTexture[liDisplay][liBuffer] = false;
                    maabRenderYUY2Texture[liDisplay][liBuffer]       = false;
                }

                // Compressed (DXT1) texture -- single-buffered per display index.
                renderengine::Texture2D* lpCompressed = renderengine::Texture2D::Initialize(
                    &lCompressedDesc, &lCompressedParams);
                mapCompressedTextureBuffer[liDisplay] = lpCompressed;
                CGS_ASSERT(mapCompressedTextureBuffer[liDisplay],
                           "mapCompressedTextureBuffer[ liTextureIndex ]");
                renderengine::Texture::Lock(mapCompressedTextureBuffer[liDisplay], 0, 0, 0,
                                            &maLockedCompressedTextures[liDisplay]);
                // DXT1: clear (height rounded up to 4-row blocks) * stride bytes.
                memset(maLockedCompressedTextures[liDisplay].mpPixelData, 0,
                       ((maLockedCompressedTextures[liDisplay].muHeight + 3) / 4) *
                           maLockedCompressedTextures[liDisplay].muStride);
            }

            // Seed the buffer indices and bind the first frame as the Flapt special texture.
            miCurrentRenderTexture = KI_INITIAL_RENDER_FROM_TEXTURE;   // 1
            miCurrentCopyToTexture = KI_INITIAL_COPY_TO_TEXTURE;       // 2
            mbRenderTexture        = false;
            {
                s32 liShaderProgram = 0;
                renderengine::Texture* lpOutput =
                    GetRenderOutput(0, &liShaderProgram, 0);
                // The render-output slot returns the component's Flapt movie; bind the live
                // frame into its special-texture slot (X360 passes r3 straight to SetSpecialTexture).
                BrnFlapt::FlaptFile::SetSpecialTexture(lpOutput,
                                                       KAC_SPECIAL_TEXTURE_NAME);
            }
            miClearTexturesFrameCount = KI_CLEAR_TEXTURES_NOT_SET;     // -1
            mePrepareStage = E_PREPARESTAGE_LOAD_DEFAULT_TEXTURE;
            // fall through into E_PREPARESTAGE_LOAD_DEFAULT_TEXTURE
        }

        case E_PREPARESTAGE_LOAD_DEFAULT_TEXTURE:
            mePrepareStage = E_PREPARESTAGE_LOAD_DEFAULT_TEXTURE;
            if (mpGuiCache != 0 &&
                mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            {
                mePrepareStage = E_PREPARESTAGE_INIT_DEFAULT_TEXTURE;
            }
            return false;

        case E_PREPARESTAGE_INIT_DEFAULT_TEXTURE:
            PrepareDefaultTexture();
            mePrepareStage = E_PREPARESTAGE_DONE;
            return false;

        case E_PREPARESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_DONE;
            return true;

        default:
            CGS_ASSERT(false, " unknown prepare stage in NetworkPlayerImageRenderer component ");
            return false;
        }

        return false;
    }

    // ---- Release @ 0x82445B28 ----------------------------------------------------
    // Staged teardown: destruct every buffered texture, reset buffer/frame state.
    bool NetworkPlayerImageRenderer::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASESTAGE_START:
            meReleaseStage = E_RELEASESTAGE_START;
            // fall through into E_RELEASESTAGE_TEXTURES

        case E_RELEASESTAGE_TEXTURES:
            meReleaseStage = E_RELEASESTAGE_TEXTURES;

            for (s32 liDisplay = 0; liDisplay < KI_MAX_NUM_TEXTURES_TO_DISPLAY; ++liDisplay)
            {
                for (s32 liBuffer = 0; liBuffer < KI_NUM_TEXTURES_TO_BUFFER; ++liBuffer)
                {
                    if (maapTextureBuffer[liDisplay][liBuffer])
                        renderengine::Texture::Destruct(maapTextureBuffer[liDisplay][liBuffer]);
                    renderengine::Texture2D* lpYUY2 = maapYUY2TextureBuffer[liDisplay][liBuffer];
                    maapTextureBuffer[liDisplay][liBuffer] = 0;
                    if (lpYUY2)
                        renderengine::Texture::Destruct(lpYUY2);
                    maapYUY2TextureBuffer[liDisplay][liBuffer] = 0;

                    maabRenderTexture[liDisplay][liBuffer]           = false;
                    maabRenderCompressedTexture[liDisplay][liBuffer] = false;
                    maabRenderYUY2Texture[liDisplay][liBuffer]       = false;
                }

                if (mapCompressedTextureBuffer[liDisplay])
                    renderengine::Texture::Destruct(mapCompressedTextureBuffer[liDisplay]);
                mapCompressedTextureBuffer[liDisplay] = 0;
            }

            miCurrentRenderTexture    = KI_INITIAL_RENDER_FROM_TEXTURE;   // 1
            miCurrentCopyToTexture    = KI_INITIAL_COPY_TO_TEXTURE;       // 2
            mbRenderTexture           = false;
            miClearTexturesFrameCount = KI_CLEAR_TEXTURES_NOT_SET;        // -1
            meReleaseStage = E_RELEASESTAGE_DONE;
            return true;

        case E_RELEASESTAGE_DONE:
            meReleaseStage = E_RELEASESTAGE_DONE;
            return true;

        default:
            CGS_ASSERT(false, " unknown release stage in SatNavRender component ");
            return false;
        }
    }

    // ---- Destruct @ 0x82445C58 ---------------------------------------------------
    void NetworkPlayerImageRenderer::Destruct()
    {
        CustomRenderComponentInterface::Destruct();

        miCurrentRenderTexture    = KI_INITIAL_RENDER_FROM_TEXTURE;   // 1
        miCurrentCopyToTexture    = KI_INITIAL_COPY_TO_TEXTURE;       // 2
        mbRenderTexture           = false;
        miClearTexturesFrameCount = KI_CLEAR_TEXTURES_NOT_SET;        // -1
    }

    // ---- RecvEvent @ 0x82449CA0 --------------------------------------------------
    // Dispatch a GUI module event.
    void NetworkPlayerImageRenderer::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
    {
        CGS_ASSERT(lpEvent, " null event passed ");

        switch (liEventType)
        {
        case 64:   // E_GUI_CACHE: latch the GuiCache pointer (event's leading field).
        {
            CGS_ASSERT(lpEvent, "NULL != lpGuiCacheEvent");
            BrnGui::GuiCache* const* lppCache =
                reinterpret_cast<BrnGui::GuiCache* const*>(lpEvent);
            mpGuiCache = *lppCache;
            break;
        }

        case 258:  // E_NETWORK_PLAYER_IMAGE: copy a new image, or arm a clear.
        {
            const GuiEventNetworkPlayerImage* lpImageEvent =
                reinterpret_cast<const GuiEventNetworkPlayerImage*>(lpEvent);

            // Render is on while either a texture is supplied or the index is a real slot.
            mbRenderTexture = (lpImageEvent->mpTexture != 0) || (lpImageEvent->miTextureIndex != -1);

            if (mbRenderTexture)
            {
                CGS_ASSERT((lpImageEvent->miTextureIndex >= 0) &&
                               (lpImageEvent->miTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY),
                           "(lpNetworkPlayerImageEvent->miTextureIndex >= 0) && "
                           "(lpNetworkPlayerImageEvent->miTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY)");
                miClearTexturesFrameCount = KI_CLEAR_TEXTURES_NOT_SET;   // -1

                if (lpImageEvent->mpTexture)
                {
                    CopyTexture(lpImageEvent, miCurrentCopyToTexture);
                    mbUseDefaultTexture = false;
                }
                else
                {
                    // No image for this index: disarm all three render flags for this slot.
                    const s32 liDisplay = lpImageEvent->miTextureIndex;
                    const s32 liBuffer  = miCurrentCopyToTexture;
                    maabRenderTexture[liDisplay][liBuffer]           = false;
                    maabRenderCompressedTexture[liDisplay][liBuffer] = false;
                    maabRenderYUY2Texture[liDisplay][liBuffer]       = false;
                }
            }
            else
            {
                miClearTexturesFrameCount = KI_NUM_TEXTURES_TO_BUFFER;   // 3: arm the clear countdown
            }
            break;
        }

        case 571:  // E_USE_DEFAULT: force the baked default texture.
            mbUseDefaultTexture = true;
            break;

        default:
            break;
        }
    }

    // ---- Update @ 0x82449C80 -----------------------------------------------------
    // Count the arm-to-clear timer down; clear the surfaces when it underflows.
    void NetworkPlayerImageRenderer::Update()
    {
        if (miClearTexturesFrameCount >= 0)
        {
            --miClearTexturesFrameCount;
            if (miClearTexturesFrameCount < 0)
                ClearTextures();
        }
    }

    // ---- GetID @ 0x82445CA8 ------------------------------------------------------
    CgsID NetworkPlayerImageRenderer::GetID() const
    {
        return KID_NETWORK_PLAYER_IMAGE;
    }

    // ---- GetRenderOutput @ 0x82445CC0 -------------------------------------------
    // Choose which texture to display for liTextureIndex; *lpiShaderProgram selects the
    // sampling path (1 => YUY2). Returns the default texture when forced, else the live
    // normal/compressed/YUY2 buffer entry for the current render slot, else NULL.
    renderengine::Texture* NetworkPlayerImageRenderer::GetRenderOutput(
        s32 liTextureIndex, s32* lpiShaderProgram, CgsGui::ImRendererSet* /*lpRendererSet*/)
    {
        if (mbUseDefaultTexture)
        {
            CGS_ASSERT(mpDefaultTexture != 0, "NULL != mpDefaultTexture");
            *lpiShaderProgram = 0;
            return mpDefaultTexture;
        }

        CGS_ASSERT((liTextureIndex >= 0) && (liTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY),
                   "(liTextureIndex >= 0) && (liTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY)");
        CGS_ASSERT(lpiShaderProgram != 0, "lpiShaderProgram != NULL");

        *lpiShaderProgram = 0;

        if (mbRenderTexture)
        {
            const s32 liRender = miCurrentRenderTexture;
            if (maabRenderTexture[liTextureIndex][liRender])
                return maapTextureBuffer[liTextureIndex][liRender];
            if (maabRenderCompressedTexture[liTextureIndex][liRender])
                return mapCompressedTextureBuffer[liTextureIndex];
            if (maabRenderYUY2Texture[liTextureIndex][liRender])
            {
                *lpiShaderProgram = 1;
                return maapYUY2TextureBuffer[liTextureIndex][miCurrentRenderTexture];
            }
        }
        return 0;
    }

    // ---- SwapBuffers @ 0x82445E58 ------------------------------------------------
    // Advance render / copy-to slots (mod 3), re-bind the now-current frame as the Flapt
    // special texture and push its shader program to the Flapt renderer.
    void NetworkPlayerImageRenderer::SwapBuffers()
    {
        miCurrentRenderTexture = (miCurrentRenderTexture + 1) % 3;
        miCurrentCopyToTexture = (miCurrentCopyToTexture + 1) % 3;

        s32 liShaderProgram = 0;
        renderengine::Texture* lpOutput = GetRenderOutput(0, &liShaderProgram, 0);
        BrnFlapt::FlaptFile::SetSpecialTexture(lpOutput,
                                               KAC_SPECIAL_TEXTURE_NAME);

        CGS_ASSERT(mpFlaptRenderer, "mpFlaptRenderer");
        mpFlaptRenderer->SetSpecialTextureShaderProgram(liShaderProgram);
    }

    // ---- GetNumTextures (DWARF cpp:725) -----------------------------------------
    s32 NetworkPlayerImageRenderer::GetNumTextures() const
    {
        return KI_MAX_NUM_TEXTURES_TO_DISPLAY;
    }

    // ---- RenderComponent (DWARF cpp:507) ----------------------------------------
    void NetworkPlayerImageRenderer::RenderComponent(CgsGui::ImRendererSet* /*lpRendererSet*/)
    {
        // The Flapt movie consumes this component through its special texture; the renderer
        // has no direct per-component draw work.
    }

    // ---- CopyTexture @ 0x82445F30 ------------------------------------------------
    // Unpack lpEvent's NetworkTexture into the liCopyToTexture buffer slot, routing by the
    // source pixel format and arming the matching render flag.
    void NetworkPlayerImageRenderer::CopyTexture(const GuiEventNetworkPlayerImage* lpEvent,
                                                 s32 liCopyToTexture)
    {
        CGS_ASSERT((lpEvent->miTextureIndex >= 0) &&
                       (lpEvent->miTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY),
                   "(lpEvent->miTextureIndex >= 0) && (lpEvent->miTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY)");
        CGS_ASSERT(lpEvent->mpTexture, "lpEvent->mpTexture");

        const s32 liDisplay = lpEvent->miTextureIndex;

        // Start with all three flags for this slot disarmed.
        maabRenderTexture[liDisplay][liCopyToTexture]           = false;
        maabRenderCompressedTexture[liDisplay][liCopyToTexture] = false;
        maabRenderYUY2Texture[liDisplay][liCopyToTexture]       = false;

        const renderengine::PixelFormat leFormat = lpEvent->mpTexture->GetFormat();
        CgsNetwork::NetworkImageConverter lConverter;

        if (leFormat == renderengine::PIXELFORMAT_DXT1)
        {
            // Block-compressed: unpack into the single-buffered compressed surface.
            maabRenderCompressedTexture[liDisplay][liCopyToTexture] = true;
            lConverter.UnpackFromNetworkTexture(lpEvent->mpTexture,
                                                &maLockedCompressedTextures[liDisplay]);
        }
        else if (leFormat == renderengine::PIXELFORMAT_G8B8)
        {
            // YUY2 path.
            maabRenderYUY2Texture[liDisplay][liCopyToTexture] = true;
            lConverter.UnpackFromNetworkTexture(lpEvent->mpTexture,
                                                &maaLockedYUY2Textures[liDisplay][liCopyToTexture]);
        }
        else
        {
            // Normal (A1R5G5B5) path.
            maabRenderTexture[liDisplay][liCopyToTexture] = true;
            lConverter.UnpackFromNetworkTexture(lpEvent->mpTexture,
                                                &maaLockedTextures[liDisplay][liCopyToTexture]);
        }
    }

    // ---- ClearTextures @ 0x824460E0 ----------------------------------------------
    // Zero every locked surface (normal + YUY2 + compressed) of the whole buffer set.
    void NetworkPlayerImageRenderer::ClearTextures()
    {
        for (s32 liDisplay = 0; liDisplay < KI_MAX_NUM_TEXTURES_TO_DISPLAY; ++liDisplay)
        {
            for (s32 liBuffer = 0; liBuffer < KI_NUM_TEXTURES_TO_BUFFER; ++liBuffer)
            {
                if (maaLockedTextures[liDisplay][liBuffer].mpPixelData)
                    memset(maaLockedTextures[liDisplay][liBuffer].mpPixelData, 0,
                           maaLockedTextures[liDisplay][liBuffer].muStride *
                               maaLockedTextures[liDisplay][liBuffer].muHeight);

                void* lpYUY2Data = maaLockedYUY2Textures[liDisplay][liBuffer].mpPixelData;
                if (lpYUY2Data)
                    memset(lpYUY2Data, 0,
                           maaLockedYUY2Textures[liDisplay][liBuffer].muStride *
                               maaLockedYUY2Textures[liDisplay][liBuffer].muHeight);
            }

            // The compressed surface (DXT1): clear (height -> 4-row blocks) * stride.
            if (maLockedCompressedTextures[liDisplay].mpPixelData)
                memset(maLockedCompressedTextures[liDisplay].mpPixelData, 0,
                       ((maLockedCompressedTextures[liDisplay].muHeight + 3) / 4) *
                           maLockedCompressedTextures[liDisplay].muStride);
        }
    }

    // ---- SetClearTextures (DWARF cpp:738) ---------------------------------------
    // Arm the clear countdown (cleared after KI_NUM_TEXTURES_TO_BUFFER frames).
    void NetworkPlayerImageRenderer::SetClearTextures()
    {
        miClearTexturesFrameCount = KI_NUM_TEXTURES_TO_BUFFER;
    }

    // ---- PrepareDefaultTexture @ 0x824461A0 -------------------------------------
    // Fetch the baked default texture from the GuiCache into mpDefaultTexture.
    void NetworkPlayerImageRenderer::PrepareDefaultTexture()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");
        mpDefaultTexture = const_cast<renderengine::Texture2D*>(
            static_cast<const renderengine::Texture2D*>(
                mpGuiCache->GetLoadedResource(KU_DEFAULT_TEXTURE_RESOURCE_ID)));
        CGS_ASSERT(mpDefaultTexture != 0, "NULL != mpDefaultTexture");
    }
}
