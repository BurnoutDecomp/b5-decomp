#include "GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h"

#include "GameSource/Gui/BrnGuiCache.h"                                       // BrnGui::GuiCache
#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"                            // BrnFlapt::FlaptRenderer
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"                             // BrnFlapt::FlaptFile::SetSpecialTexture
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"         // CgsNetwork::NetworkTexture::GetFormat
#include "GameShared/GameClasses/Network/Utilities/CgsNetworkImageConverter.h" // CgsNetwork::NetworkImageConverter
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "pc/gcm/renderengine/pixelformat.h"                                  // renderengine::PixelFormat
#include "pc/gcm/renderengine/device.h"                                       // renderengine::gDevice (the PC device gate)

#include <cstring>   // memset
#include <cstdio>    // [diag] snprintf ([netimg] slot-ring probe)
#include <cstdlib>   // [diag] getenv   (BRN_NETIMG gates it)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                     // [diag] CgsDev::Log::WriteToLog

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
        // (DWARF: maResourcesToLoad / muNumResourcesToLoad, file-static).
        //
        // ⚠️ CORRECTED 2026-08-16 -- both fields were WRONG. They were `{ 0u,
        // E_GUI_RESOURCETYPE_FSM }`, FLAG'd "the recovered .rdata values", and that left the
        // renderer WAITING ON resource 0 while FETCHING resource 236 two stages later, i.e.
        // a tuple that could never make PrepareDefaultTexture correct. The real values were
        // read out of the unpacked image at the exact addresses the asm loads
        // (Prepare @0x82451560: `addi r4, r11, unk_82F2571C` / `lwz r5, dword_82F25724`):
        //     0x82F2571C = 0x000000EC = 236     <- muId   (== KU_DEFAULT_TEXTURE_RESOURCE_ID)
        //     0x82F25720 = 0x0000000B = 11      <- meType (E_GUI_RESOURCETYPE_TEXTURE)
        //     0x82F25724 = 0x00000001 = 1       <- muNumResourcesToLoad
        // Type 11 is the GUITEXTURES.BIN family ("%s" verbatim), which is exactly where the
        // asset lives: gGuiResourceIdentifier[236] == "Headtif", and
        // CgsResource::ID::HashString("headtif") == 0x2E0CA9B3 is resource [04] of the
        // shipped, already-ported (bnd2 platform 4) GUITEXTURES.BIN.
        // ⚠️⚠️ [licence-icon] TYPE VALUE CORRECTED AGAIN 2026-08-24 -- the 2026-08-16 pass READ
        // 11 from the image (three lines up!) and then spelled it with the committed enumerator
        // E_GUI_RESOURCETYPE_TEXTURE, whose value in CgsGuiResourceModuleIO.h is 10 -- the PS3
        // enum, one BELOW the ARTIST value (the +1 apt-family drift the KAAC_RESOURCE_TEMPLATES
        // banner documents). Value 10 is ARTIST's FLAPT-PERSISTENT: the request routed to the
        // flapt template ("%s.BUNDLE", apt persistent bank) and the boot died trying to load a
        // bundle named 'Headtif.BUNDLE' and registering an empty-named FLApt file (measured:
        // three-assert cascade @BrnFlaptManager.cpp:176). The ARTIST literal-with-cast is the
        // same idiom the working font tuples use (BrnGuiModule.cpp kaFontResources, type 16).
        const CgsGui::sResourceTuple maResourcesToLoad[1] =
        {
            { 236u, static_cast<CgsGui::ResourceRequestTypes>(11) },   // ARTIST type 11 == texture
        };
        const u32 muNumResourcesToLoad = 1u;

        // ================= [netimg] THE SLOT-RING PROBE =================================
        // DELETE-WHEN-STABLE. Inert unless BRN_NETIMG names a non-zero value.
        //
        // WHAT IT HAS TO SEPARATE, and why nothing already in the tree could. The defect this
        // was written for is not "the picture is wrong", it is "the picture is written to a
        // buffer nothing ever reads". A probe that only counted arriving events, or only
        // counted GetRenderOutput calls, would look IDENTICAL before and after the fix: the
        // events arrive either way and the reads happen either way. The separating quantity is
        // the pair of RING CURSORS -- miCurrentCopyToTexture (where RecvEvent writes) and
        // miCurrentRenderTexture (where GetRenderOutput reads) -- and specifically the SET OF
        // DISTINCT VALUES each takes over a run. SwapBuffers is their only writer in the whole
        // image, so without it each set has exactly ONE element and the two elements differ;
        // with it each has three and they meet. Hence bitmasks + distinct-value counts below,
        // not maxima. [[diagnostics-that-lie]]
        bool NetImgProbeEnabled()
        {
            static int siEnabled = -1;
            if (siEnabled < 0)
            {
                const char* lpcValue = std::getenv("BRN_NETIMG");
                siEnabled = (lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0') ? 1 : 0;
            }
            return siEnabled != 0;
        }

        u32 guNetImgRenderSlotMask = 0;   // bit n set == miCurrentRenderTexture was n at a READ
        u32 guNetImgCopySlotMask   = 0;   // bit n set == miCurrentCopyToTexture was n at a WRITE
        u32 guNetImgSwaps          = 0;
        u32 guNetImgRecv258        = 0;   // type-258 events seen
        u32 guNetImgCopies         = 0;   // of those, ones carrying a real texture (-> CopyTexture)
        u32 guNetImgReads          = 0;   // GetRenderOutput calls
        u32 guNetImgReadsDefault   = 0;   //   ... answered with the baked default texture
        u32 guNetImgReadsLive      = 0;   //   ... answered with a live buffer  (THE ONE THAT MATTERS)
        u32 guNetImgReadsNull      = 0;   //   ... answered NULL
        u32 guNetImgUpdates        = 0;   // per-frame Update() calls -- the census cadence

        void NetImgDumpCensus(const char* lpcWhere)
        {
            char lacMsg[320];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[netimg] census@%s upd=%u swaps=%u recv258=%u copies=%u reads=%u "
                "live=%u default=%u null=%u renderSlots=0x%X(%u) copySlots=0x%X(%u)\n",
                lpcWhere, guNetImgUpdates, guNetImgSwaps, guNetImgRecv258, guNetImgCopies,
                guNetImgReads, guNetImgReadsLive, guNetImgReadsDefault, guNetImgReadsNull,
                guNetImgRenderSlotMask,
                static_cast<unsigned>(((guNetImgRenderSlotMask >> 0) & 1u) +
                                      ((guNetImgRenderSlotMask >> 1) & 1u) +
                                      ((guNetImgRenderSlotMask >> 2) & 1u)),
                guNetImgCopySlotMask,
                static_cast<unsigned>(((guNetImgCopySlotMask >> 0) & 1u) +
                                      ((guNetImgCopySlotMask >> 1) & 1u) +
                                      ((guNetImgCopySlotMask >> 2) & 1u)));
            CgsDev::Log::WriteToLog(lacMsg);
        }

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

            // [FLAG PC-platform device gate 2026-08-27] every Texture2D::Initialize below
            // needs the D3D device. On a healthy boot the device exists before any prepare
            // runs (renderengine::Device::Start is called from EnginePrepare, before
            // gGameModule.Construct -- BrnMain.cpp). But Device::Start can FAIL wholesale
            // and unrecoverably (measured: GetDeviceCaps hr=0x8876086C D3DERR_NOTAVAILABLE
            // when the user's session is DISCONNECTED -- an RDP drop leaves the whole run
            // headless; Device::Start's [device] diag line names the step). Without this
            // gate such a run dies on 21 "maapTextureBuffer[...]" asserts (null Initialize
            // returns) instead of limping on like every other device consumer does. Defer
            // exactly like the AptRT text bring-up ("device not up yet -- deferred"):
            // report not-ready and let the GuiModule prepare PUMP retry next frame.
            if (renderengine::gDevice == 0)
                return false;

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

            // [netimg] DELETE-WHEN-STABLE. One line per arriving picture, carrying BOTH
            // cursors, so the write slot and the read slot are on the same line.
            ++guNetImgRecv258;
            if (lpImageEvent->mpTexture != 0) ++guNetImgCopies;
            if (miCurrentCopyToTexture >= 0 && miCurrentCopyToTexture < KI_NUM_TEXTURES_TO_BUFFER)
                guNetImgCopySlotMask |= (1u << miCurrentCopyToTexture);
            if (NetImgProbeEnabled())
            {
                char lacMsg[220];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[netimg] RECV258 n=%u tex=%p idx=%d copyTo=%d render=%d "
                    "renderTexture=%d useDefault=%d\n",
                    guNetImgRecv258, static_cast<void*>(lpImageEvent->mpTexture),
                    static_cast<int>(lpImageEvent->miTextureIndex),
                    static_cast<int>(miCurrentCopyToTexture),
                    static_cast<int>(miCurrentRenderTexture),
                    mbRenderTexture ? 1 : 0, mbUseDefaultTexture ? 1 : 0);
                CgsDev::Log::WriteToLog(lacMsg);
            }

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
        // [netimg] DELETE-WHEN-STABLE. THE CENSUS CADENCE IS HERE, not on the read path, and
        // that is deliberate: the first version of this probe printed every 600th
        // GetRenderOutput call and printed NOTHING AT ALL over a 150 s run, because the
        // default-texture short-circuit means the component is barely read. A probe whose
        // sampling clock is the very quantity under test reports silence as "no data" and
        // silence as "no defect" identically. Update() is the per-frame drive, so this ticks
        // whatever the renderer is doing. [[diagnostics-that-lie]]
        ++guNetImgUpdates;
        if (NetImgProbeEnabled() && (guNetImgUpdates % 300u) == 0u) NetImgDumpCensus("upd");

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
        // [netimg] DELETE-WHEN-STABLE. Census only -- one line per read would be one line per
        // component per frame. The census is dumped from SwapBuffers (and, when the ring is
        // frozen, every 600th read, so a run in which SwapBuffers never fires still reports).
        ++guNetImgReads;
        if (miCurrentRenderTexture >= 0 && miCurrentRenderTexture < KI_NUM_TEXTURES_TO_BUFFER)
            guNetImgRenderSlotMask |= (1u << miCurrentRenderTexture);
        if (NetImgProbeEnabled() && guNetImgReads <= 3u)
        {
            char lacFirst[200];
            std::snprintf(lacFirst, sizeof(lacFirst),
                "[netimg] READ n=%u idx=%d render=%d useDefault=%d renderTexture=%d "
                "flags=%d%d%d default=%p\n",
                guNetImgReads, static_cast<int>(liTextureIndex),
                static_cast<int>(miCurrentRenderTexture), mbUseDefaultTexture ? 1 : 0,
                mbRenderTexture ? 1 : 0,
                (liTextureIndex >= 0 && liTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY &&
                 maabRenderTexture[liTextureIndex][miCurrentRenderTexture]) ? 1 : 0,
                (liTextureIndex >= 0 && liTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY &&
                 maabRenderCompressedTexture[liTextureIndex][miCurrentRenderTexture]) ? 1 : 0,
                (liTextureIndex >= 0 && liTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY &&
                 maabRenderYUY2Texture[liTextureIndex][miCurrentRenderTexture]) ? 1 : 0,
                static_cast<void*>(mpDefaultTexture));
            CgsDev::Log::WriteToLog(lacFirst);
        }

        if (mbUseDefaultTexture)
        {
            ++guNetImgReadsDefault;
            if (NetImgProbeEnabled() && (guNetImgReads % 600u) == 0u) NetImgDumpCensus("read");
            CGS_ASSERT(mpDefaultTexture != 0, "NULL != mpDefaultTexture");
            *lpiShaderProgram = 0;
            return mpDefaultTexture;
        }
        if (NetImgProbeEnabled() && (guNetImgReads % 600u) == 0u) NetImgDumpCensus("read");

        CGS_ASSERT((liTextureIndex >= 0) && (liTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY),
                   "(liTextureIndex >= 0) && (liTextureIndex < KI_MAX_NUM_TEXTURES_TO_DISPLAY)");
        CGS_ASSERT(lpiShaderProgram != 0, "lpiShaderProgram != NULL");

        *lpiShaderProgram = 0;

        if (mbRenderTexture)
        {
            const s32 liRender = miCurrentRenderTexture;
            if (maabRenderTexture[liTextureIndex][liRender])
            {
                ++guNetImgReadsLive;   // [netimg]
                return maapTextureBuffer[liTextureIndex][liRender];
            }
            if (maabRenderCompressedTexture[liTextureIndex][liRender])
            {
                ++guNetImgReadsLive;   // [netimg]
                return mapCompressedTextureBuffer[liTextureIndex];
            }
            if (maabRenderYUY2Texture[liTextureIndex][liRender])
            {
                ++guNetImgReadsLive;   // [netimg]
                *lpiShaderProgram = 1;
                return maapYUY2TextureBuffer[liTextureIndex][miCurrentRenderTexture];
            }
        }
        ++guNetImgReadsNull;   // [netimg]
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

        // [netimg] DELETE-WHEN-STABLE. Not one line per swap (that is one per frame); the
        // first 5 swaps prove the ring moves at all, then a census every 600.
        ++guNetImgSwaps;
        if (miCurrentCopyToTexture >= 0 && miCurrentCopyToTexture < KI_NUM_TEXTURES_TO_BUFFER)
            guNetImgCopySlotMask |= (1u << miCurrentCopyToTexture);   // the CURSOR, not just writes
        if (NetImgProbeEnabled() && (guNetImgSwaps <= 5u || (guNetImgSwaps % 600u) == 0u))
        {
            char lacMsg[200];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[netimg] SWAP n=%u render=%d copyTo=%d out=%p shader=%d flapt=%p\n",
                guNetImgSwaps, static_cast<int>(miCurrentRenderTexture),
                static_cast<int>(miCurrentCopyToTexture), static_cast<void*>(lpOutput),
                static_cast<int>(liShaderProgram), static_cast<void*>(mpFlaptRenderer));
            CgsDev::Log::WriteToLog(lacMsg);
            if ((guNetImgSwaps % 600u) == 0u) NetImgDumpCensus("swap");
        }

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
