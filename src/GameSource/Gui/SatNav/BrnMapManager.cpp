#include "GameSource/Gui/SatNav/BrnMapManager.h"

#include "SharedClasses/Gui/SatNav/BrnSatNavTile.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache (RecvEvent resource load)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple / ResourceRequestTypes
#include "pc/gcm/renderengine/renderstates.h"        // renderengine::TextureState (Initialize + GetResourceDescriptor)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface::GetAllocator (Construct)
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"    // BrnGui::MapTransform (Construct's world box)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG-TEMP] the map-lowres probe

#include <cstdlib>   // qsort (RefreshActiveTextureArray)
#include <cmath>     // fabsf (SetZoomLevel)

// BrnGui::MapManager - the sat-nav map tile manager bodies. The ctor value-initialises the
// leading sub-objects; RefreshActiveTextureArray rebuilds the flattened active-texture set;
// GetTileState / RemoveTileFromSet / SetZoomLevel manage the requested-tile cache. See the
// header for the (X360-pinned) member layout.
//
// ⭐ MOUNTABILITY (2026-08-29, main-menu wave D1). This TU used to hand-declare two MSVC CRT
// artifacts -- `_vector_constructor_iterator_` and rw::Resource::`default constructor closure'
// -- as extern "C" externals with no definition anywhere in the tree, which is why it could
// never be added to the build list. Both are gone: the ctor now spells the SAME construction
// as explicit C++ loops over the NAMED members (the committed-tree pattern for a PPC
// vector-ctor idiom). The rw::Resource closure @0x823FD950 is, verbatim,
//     li r11,0 ; stw r11,0(r3) ; stw r11,4(r3) ; stw r11,8(r3) ; stw r11,0xC(r3)
//     stw r11,0x10(r3) ; stw r11,0(r3) ; blr
// i.e. it ZEROES the whole 20-byte X360 rw::Resource -- exactly what value-initialising
// `rw::Resource()` does on the host (BaseResources<5>: five null base-resource pointers), so
// the semantics are preserved, not approximated.
//
// ⚠️ The old ctor also walked the object through RAW X360 BYTE OFFSETS (`new (lp + 0x2C8)`
// ...). Those offsets are 4-byte-pointer console offsets; on the LLP64 host sTileCache and
// sTexture are wider and every one of them pointed into the wrong member. Reaching the
// members BY NAME is both the campaign rule and the bug fix.

namespace BrnGui
{
    // ---------------------------------------------------------------------------
    // BrnGui::MapManager::MapManager
    //
    // X360 ARTIST @0x82508550. Default-constructs the sat-nav map manager. The body
    // only value-initialises the leading sub-objects; the remaining scalar members
    // (mbEnabled, muTilesRequestedCount, mpStateInterface, meZoomLevel, mpAllocator)
    // are set later by Construct().
    //
    //   +0x000  mWorldRect          sRect::sRect()          -> (0,0,1,1)
    //   +0x010  mScreenRect         sRect::sRect()
    //   +0x024  mLowResTexture.mBB      sRect::sRect()       (mLowResTexture @ +0x020)
    //   +0x034  mLowResTexture.mBBWorld sRect::sRect()
    //   +0x054  mLowResTextureCache.mTextureStateResources[3]  zeroed (3 * 20B)
    //   +0x0AC  maRequestedTiles[6].mTextureStateResources[3]  rw::Resource ctor
    //           (6 tiles, stride 88; 3 * 20B resources per tile)
    //   +0x2B4  mActiveTextures.maTextures[19].mBB / .mBBWorld  sRect::sRect()
    //
    // All byte offsets are X360 (4-byte-pointer ABI); members are reached by raw
    // offset here so the exact store order / partial zeroing matches the asm.
    // ---------------------------------------------------------------------------
    MapManager::MapManager()
    {
        // X360 0x82508550: sRect::sRect(this+0) / (this+16) -- mWorldRect / mScreenRect,
        // each the sRect default ctor @0x82447F50 -> (0, 0, 1, 1).
        mWorldRect  = SatNavTile::sRect();
        mScreenRect = SatNavTile::sRect();

        // X360: sRect::sRect(this+36) / (this+52) -- mLowResTexture's two bounding boxes
        // (mLowResTexture @+0x20, mBB @+0x04, mBBWorld @+0x14). mpTextureState is NOT
        // touched here; MainMapComponent::Construct's MapManager::Construct zeroes it.
        mLowResTexture.mBB      = SatNavTile::sRect();
        mLowResTexture.mBBWorld = SatNavTile::sRect();

        // X360: the `v3 = 2 downto 0` five-word store loop at this+84 -- zeroes
        // mLowResTextureCache.mTextureStateResources[3] (3 x 20 bytes on the console).
        for (uint32_t luIndex = 0; luIndex < KU_MAX_NUMBER_OF_TEXTURES_PER_TILE; ++luIndex)
        {
            mLowResTextureCache.mTextureStateResources[luIndex] = rw::Resource();
        }

        // X360: the `v4 = 5 downto 0` loop, stride 88, calling
        // `vector constructor iterator'(base, 20, 3, rw::Resource::`default ctor closure')
        // on each maRequestedTiles[i].mTextureStateResources -- i.e. zero all 6 x 3 entries
        // (the closure @0x823FD950 is a pure 20-byte zero-fill; see the banner).
        for (uint32_t luTile = 0; luTile < KU_TILE_ARRAY_SIZE; ++luTile)
        {
            for (uint32_t luIndex = 0; luIndex < KU_MAX_NUMBER_OF_TEXTURES_PER_TILE; ++luIndex)
            {
                maRequestedTiles[luTile].mTextureStateResources[luIndex] = rw::Resource();
            }
        }

        // X360: the `v6 = 18 downto 0` loop, stride 36, sRect::sRect on (base-16) and (base)
        // -- mActiveTextures.maTextures[19]'s mBB and mBBWorld. mpTextureState again untouched.
        for (uint32_t luIndex = 0; luIndex < KU_MAX_TEXTURES_IN_CACHE; ++luIndex)
        {
            mActiveTextures.maTextures[luIndex].mBB      = SatNavTile::sRect();
            mActiveTextures.maTextures[luIndex].mBBWorld = SatNavTile::sRect();
        }
    }

    // ---------------------------------------------------------------------------
    // BrnGui::MapManager::Construct
    //
    // X360 ARTIST @0x82458590 (json name field verified). Called by
    // MainMapComponent::Construct on the embedded manager (`bl` at 0x8245E3AC, r4 == the
    // state interface). Landed this wave so that the REAL MapManager::RecvEvent -- which
    // dereferences mpAllocator -- can replace its BrnMainMapLinkGates stand-in without
    // reading uninitialised state; the semantics were already transcribed into
    // BrnMainMap.cpp's FLAG boundary and are re-verified here against the export.
    //
    // ⭐ ONE CORRECTION TO THAT TRANSCRIPTION: it recorded `mWorldRect = {0,0,1,1}`. The
    // asm's four stores at this+36/40/44/48 are 0x24..0x30, which is
    // mLowResTexture.mBB, NOT mWorldRect (+0x00) -- mWorldRect is not touched by Construct
    // at all (the ctor's sRect() already left it {0,0,1,1}). The {0,0,1,1} unit box is the
    // backdrop texture's LOCAL box, and the world box below is that same unit square taken
    // into world space, which is exactly what a full-world backdrop should be.
    // ---------------------------------------------------------------------------
    void MapManager::Construct(CgsGui::StateInterface* lpStateInterface)
    {
        CGS_ASSERT(lpStateInterface != NULL, "lpStateInterface != NULL");   // cpp:62

        mpStateInterface = lpStateInterface;

        // The X360 inlines the accessor; the "mpAllocator != NULL" assert at
        // CgsGuiStateInterface.h:337 in the export set is that accessor's own, so the real
        // call carries it.
        mpAllocator = lpStateInterface->GetAllocator();

        mapDirectories[E_ZOOM_MEDIUM] = NULL;
        mapDirectories[E_ZOOM_HIGH]   = NULL;

        // X360 `memset(this + 156, 0, 528)` == maRequestedTiles (6 x 88 console bytes).
        // Spelled by name because 88 is the CONSOLE stride; the host record is wider.
        for (uint32_t luTile = 0; luTile < KU_TILE_ARRAY_SIZE; ++luTile)
        {
            SatNavTile::sTileCache& lTile = maRequestedTiles[luTile];
            lTile.meState        = SatNavTile::E_STATE_UNLOADED;
            lTile.muID           = 0u;
            lTile.mpTile         = NULL;
            lTile.muTextureCount = 0u;
            for (uint32_t luIndex = 0; luIndex < KU_MAX_NUMBER_OF_TEXTURES_PER_TILE; ++luIndex)
            {
                lTile.mTextureStateResources[luIndex] = rw::Resource();
                lTile.mapTextureStates[luIndex]       = NULL;
            }
        }

        muTilesRequestedCount = 0u;              // +0x568
        mbEnabled             = false;           // +0x564
        meZoomLevel           = E_ZOOM_MEDIUM;   // +0x570

        // The console zeroes exactly these two head words of the low-res pair -- the cache's
        // mpTile (+0x4C) and the backdrop's texture-state (+0x20). The latter is
        // load-bearing: it is RecvEvent's "already built" guard.
        mLowResTextureCache.mpTile    = NULL;
        mLowResTexture.mpTextureState = NULL;

        // this+36..48 -- the backdrop's LOCAL box is the unit square.
        mLowResTexture.mBB.mfLeft   = 0.0f;
        mLowResTexture.mBB.mfTop    = 0.0f;
        mLowResTexture.mBB.mfRight  = 1.0f;
        mLowResTexture.mBB.mfBottom = 1.0f;

        // this+52..64 -- the same unit square carried from NORMALISED space into WORLD
        // space. The asm stages the two corners through one scratch quadword: the first
        // MapTransform::Transform runs on (0,0) and its x/y land in words 0/1, then the
        // second runs on (1,1) into words 2/3 (0x82458748..0x82458770). The matrix pointers
        // are r3 = a copy of smm33NormalisedSpace @0x82FB2FA0 (the FROM space) and
        // r4 = a copy of smm33WorldSpace @0x82FB3610 (the TO space).
        const Vector2 lv2UnitMin = { 0.0f, 0.0f, 0.0f, 0.0f };
        const Vector2 lv2UnitMax = { 1.0f, 1.0f, 0.0f, 0.0f };
        const Vector2 lv2WorldMin = MapTransform::Transform(
            lv2UnitMin, MapTransform::GetNormalisedSpace(), MapTransform::GetWorldSpace());
        const Vector2 lv2WorldMax = MapTransform::Transform(
            lv2UnitMax, MapTransform::GetNormalisedSpace(), MapTransform::GetWorldSpace());

        mLowResTexture.mBBWorld.mfLeft   = lv2WorldMin.x;
        mLowResTexture.mBBWorld.mfTop    = lv2WorldMin.y;
        mLowResTexture.mBBWorld.mfRight  = lv2WorldMax.x;
        mLowResTexture.mBBWorld.mfBottom = lv2WorldMax.y;
    }

    // @0x82448760 -- the bundle-load state of the requested-tile slot owning luID.
    SatNavTile::EBundleState MapManager::GetTileState(uint32_t luID) const
    {
        CGS_ASSERT(luID != 0, "luID != 0");

        uint32_t luIndex = 0;
        const SatNavTile::sTileCache* lpRequestedTile = &maRequestedTiles[0];
        while (lpRequestedTile->muID != luID)
        {
            ++luIndex;
            ++lpRequestedTile;
            if (luIndex >= KU_TILE_ARRAY_SIZE)
            {
                uint32_t luFreeIndex = 0;
                for (lpRequestedTile = &maRequestedTiles[0];
                     lpRequestedTile->muID != 0 || lpRequestedTile->meState != SatNavTile::E_STATE_UNLOADED;
                     ++lpRequestedTile)
                {
                    if (++luFreeIndex >= KU_TILE_ARRAY_SIZE)
                        return SatNavTile::E_STATE_FULL;
                }
                return lpRequestedTile->meState;
            }
        }
        return lpRequestedTile->meState;
    }

    // @0x82448540 -- rebuild mActiveTextures from the low-res texture plus every LOADED
    // requested tile whose world bounding box overlaps the current world rect, then sort the
    // dynamic entries (everything after index 0) largest-world-area first.
    void MapManager::RefreshActiveTextureArray()
    {
        mActiveTextures.muTextureCount = 0;

        // The low-res backdrop texture, when present, is always active entry 0.
        if (mLowResTexture.mpTextureState != NULL)
        {
            mActiveTextures.maTextures[0] = mLowResTexture;
            mActiveTextures.muTextureCount = 1;
        }

        for (uint32_t luTileIndex = 0; luTileIndex < KU_TILE_ARRAY_SIZE; ++luTileIndex)
        {
            const SatNavTile::sTileCache& lTile = maRequestedTiles[luTileIndex];

            CGS_ASSERT(mActiveTextures.muTextureCount < KU_MAX_TEXTURES_IN_CACHE,
                       "mActiveTextures.muTextureCount < KU_MAX_TEXTURES_IN_CACHE");

            if (lTile.meState != SatNavTile::E_STATE_LOADED)
            {
                continue;
            }

            if (!lTile.mpTile->IsWithinWorldRect(mWorldRect))
            {
                continue;
            }

            const SatNavTile* lpTile = lTile.mpTile;
            for (uint32_t luIndex = 0; luIndex < lpTile->muTextureCount; ++luIndex)
            {
                SatNavTile::sTexture& lTexture = mActiveTextures.maTextures[mActiveTextures.muTextureCount];

                lTexture.mpTextureState = lTile.mapTextureStates[luIndex];

                CGS_ASSERT(luIndex < KU_MAX_NUMBER_OF_TEXTURES_PER_TILE,
                           "luIndex < KU_MAX_NUMBER_OF_TEXTURES_PER_TILE");
                lTexture.mBB = lpTile->mBB[luIndex];

                CGS_ASSERT(luIndex < KU_MAX_NUMBER_OF_TEXTURES_PER_TILE,
                           "luIndex < KU_MAX_NUMBER_OF_TEXTURES_PER_TILE");
                lTexture.mBBWorld = lpTile->mBBWorld[luIndex];

                ++mActiveTextures.muTextureCount;
            }
        }

        // Entry 0 (the low-res backdrop) stays pinned; sort the rest largest-area first.
        if (mActiveTextures.muTextureCount > 1)
        {
            qsort(&mActiveTextures.maTextures[1],
                  mActiveTextures.muTextureCount - 1,
                  sizeof(SatNavTile::sTexture),
                  SatNavTile::sTexture::QSortCallback);
        }
    }

    // @0x82448820 -- verify a tile is present and loaded before it is dropped from the set.
    // In this build the removal itself compiles away; only the two pre-conditions survive.
    // (GetTileState is evaluated as the asserted expression, matching the X360 unconditional call.)
    void MapManager::RemoveTileFromSet(SatNavTile* lpTile)
    {
        CGS_ASSERT(lpTile != NULL, "lpTile != NULL");

        CGS_ASSERT(GetTileState(lpTile->muID) == SatNavTile::E_STATE_LOADED,
                   "GetTileState( lpTile->muID) == SatNavTile::E_STATE_LOADED");
    }

    namespace
    {
        // PPC fabs stand-in used by SetZoomLevel's area comparison.
        inline float MyAbs(float lfValue)
        {
            return ::fabsf(lfValue);
        }
    }

    // @0x8244F768 -- change the map zoom. When zooming out from HIGH to MEDIUM, any loaded
    // tile whose world area differs from the target zoom's reference tile area by more than
    // 1.5x the smaller of the two is pruned (it no longer matches the medium-zoom footprint).
    void MapManager::SetZoomLevel(EZoomLevel leZoomLevel)
    {
        const EZoomLevel leCurrentZoom = meZoomLevel;
        if (leCurrentZoom == leZoomLevel)
        {
            return;
        }

        if (leCurrentZoom == E_ZOOM_HIGH && leZoomLevel == E_ZOOM_MEDIUM)
        {
            const SatNavTileDirectory* lpDirectory = mapDirectories[E_ZOOM_HIGH];
            if (lpDirectory != NULL)
            {
                const SatNavTileDirectory::sTileItem& lReferenceItem = lpDirectory->mpaItems[0];
                const float lfReferenceArea =
                    MyAbs(lReferenceItem.mBB[0].mfBottom - lReferenceItem.mBB[0].mfTop) *
                    MyAbs(lReferenceItem.mBB[0].mfRight  - lReferenceItem.mBB[0].mfLeft);

                for (uint32_t luIndex = 0; luIndex < KU_TILE_ARRAY_SIZE; ++luIndex)
                {
                    const SatNavTile::sTileCache& lTile = maRequestedTiles[luIndex];
                    if (lTile.meState != SatNavTile::E_STATE_LOADED)
                    {
                        continue;
                    }

                    CGS_ASSERT(lTile.mpTile != NULL, "lTile.mpTile != NULL");

                    const SatNavTile::sRect& lWorldBB = lTile.mpTile->mBBWorld[0];
                    const float lfTileArea = MyAbs(
                        MyAbs(lWorldBB.mfBottom - lWorldBB.mfTop) *
                        MyAbs(lWorldBB.mfRight  - lWorldBB.mfLeft));

                    const float lfDifference = lfTileArea - lfReferenceArea;
                    const float lfSmallerArea = (lfDifference >= 0.0f) ? lfReferenceArea : lfTileArea;
                    if (MyAbs(lfDifference) > (lfSmallerArea * 1.5f))
                    {
                        RemoveTileFromSet(lTile.mpTile);
                    }
                }
            }
        }

        meZoomLevel = leZoomLevel;
    }

    // @0x8244F898 -- receive a GUI event. Only the cache-ready event (type 64) is handled: it
    // carries the resolved GuiCache pointer in its first word. The first time it arrives (before
    // the low-res backdrop texture exists) the manager loads the map's low-res texture resource,
    // builds a sampler texture-state for it through the resource allocator, and latches it as the
    // backdrop texture.
    void MapManager::RecvEvent(const CgsModule::Event* lpEvent, int32_t liParam)
    {
        CGS_ASSERT(lpEvent != NULL, " invalid event passed ");

        const int32_t KI_GUI_CACHE_EVENT = 64;
        if (liParam != KI_GUI_CACHE_EVENT)
        {
            return;
        }

        GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpEvent);

        // Build the low-res backdrop texture-state exactly once.
        if (mLowResTexture.mpTextureState != NULL)
        {
            return;
        }

        // [DIAG-TEMP] NOT IN THE X360 BINARY -- [map-lowres] the backdrop build attempt.
        static int32_t siLowResLog = 6;
        const bool lbLog = (siLowResLog > 0 && CgsDev::Log::gpDebugPrint != 0);
        if (lbLog)
        {
            --siLowResLog;
            *CgsDev::Log::gpDebugPrint
                << "[map-lowres] event 64 reached; cache=" << (lpCache != NULL ? 1 : 0)
                << " alloc=" << (mpAllocator != NULL ? 1 : 0) << "\n";
        }

        // The low-res sat-nav map texture resource (id 199, a localised-text-class resource).
        const uint32_t luMapTextureResourceId = 199u;

        CgsGui::sResourceTuple lResourceTuple;
        lResourceTuple.muId   = luMapTextureResourceId;
        lResourceTuple.meType = CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT;   // 11

        const bool lbLoaded = lpCache->EnsureResourceIsLoaded(lResourceTuple);
        if (lbLog)
        {
            *CgsDev::Log::gpDebugPrint
                << "[map-lowres] EnsureResourceIsLoaded(199,LOCALISED_TEXT)=" << (lbLoaded ? 1 : 0) << "\n";
        }
        if (!lbLoaded)
        {
            return;
        }

        const void* lpTexture2D = lpCache->GetLoadedResource(luMapTextureResourceId);
        if (lbLog)
        {
            *CgsDev::Log::gpDebugPrint
                << "[map-lowres] GetLoadedResource(199)=" << (lpTexture2D != NULL ? 1 : 0) << "\n";
        }
        CGS_ASSERT(lpTexture2D != NULL, "lpTexture2D != NULL");

        // Sampler parameters for the backdrop texture-state (clamp-ish addressing, point/linear
        // filter, no mip bias). Field values mirror the X360 stores store-for-store.
        renderengine::TextureState::Parameters lParams;
        lParams.muAddressU      = 2u;
        lParams.muAddressV      = 2u;
        lParams.muAddressW      = 0u;
        lParams.muMagFilter     = 1u;
        lParams.muMinFilter     = 1u;
        lParams.muMipFilter     = 2u;
        lParams.muField6        = 0u;
        lParams.muField7        = 0u;
        lParams.muMaxAnisotropy = 13u;
        lParams.muField9        = 0u;
        lParams.muField10       = 1u;
        lParams.mfMipLodBias    = 0.0f;   // flt_82001CC0 == 0.0f
        lParams.mfField12       = 0.0f;   // flt_82001CC0 == 0.0f
        lParams.muField13       = 0u;
        lParams.muField14       = 0u;
        lParams.muField15       = 0u;
        lParams.mu8Field40      = 0u;
        lParams.mu8Field41      = 0u;
        lParams.mu8Field42      = 0u;
        lParams.mu8Field43      = 1u;
        lParams.mu8Field44      = 1u;
        lParams.mpTexture       = reinterpret_cast<renderengine::Texture*>(
            const_cast<void*>(lpTexture2D));

        // Size the texture-state resource, carve it from the manager's allocator, initialise the
        // sampler+raster state in it, and latch the result into the low-res cache slot + backdrop.
        //
        // ⚠️ X360-vs-host descriptor WIDTH. The console's GetResourceDescriptor writes a
        // rw::BaseResourceDescriptors<5> (10 words) and hands that same pointer straight to
        // the allocator vtable slot; the host rwcore's rw::ResourceDescriptor is <4> (8 words).
        // Reinterpreting the 10-word buffer as a ResourceDescriptor would be an X360 width on
        // the host, so the slot-0 {size, alignment} is transcribed across explicitly and the
        // remaining entries take rw's identity descriptor -- the committed pattern at
        // CgsAptRenderHandler.cpp:325-341.
        uint32_t laDescriptor[10];
        renderengine::TextureState::GetResourceDescriptor(laDescriptor);

        rw::ResourceDescriptor lAllocDescriptor;
        lAllocDescriptor.m_baseResourceDescriptors[0].m_size      = laDescriptor[0];
        lAllocDescriptor.m_baseResourceDescriptors[0].m_alignment = laDescriptor[1];
        lAllocDescriptor.m_baseResourceDescriptors[1].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[1].m_alignment = 1u;
        lAllocDescriptor.m_baseResourceDescriptors[2].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[2].m_alignment = 1u;
        lAllocDescriptor.m_baseResourceDescriptors[3].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[3].m_alignment = 1u;

        // X360 `(*(**(this+1396) + 16))(&result, mpAllocator, descriptor, 0)` -- the
        // IResourceAllocator DoAllocate slot, name argument NULL.
        rw::Resource lAllocatedResource = mpAllocator->DoAllocate(lAllocDescriptor, NULL);

        // X360: the five-word `*v12++ = *v11++` copy of the returned Resource into this+84.
        mLowResTextureCache.mTextureStateResources[0] = lAllocatedResource;

        renderengine::TextureState* lpState = renderengine::TextureState::Initialize(
            &mLowResTextureCache.mTextureStateResources[0], &lParams);

        mLowResTextureCache.mapTextureStates[0] =
            reinterpret_cast<CgsGraphics::TextureState*>(lpState);
        mLowResTexture.mpTextureState =
            reinterpret_cast<CgsGraphics::TextureState*>(lpState);

        // [DIAG-TEMP] NOT IN THE X360 BINARY.
        if (lbLog)
        {
            *CgsDev::Log::gpDebugPrint
                << "[map-lowres] BACKDROP BUILT state=" << (lpState != NULL ? 1 : 0) << "\n";
        }
    }

    // @0x8244FA80 -- rebuild the working tile set for the current world rect and zoom level. At
    // high zoom the sampled world rect is pulled inward by a quarter on every edge. Directory
    // tiles overlapping the (possibly inflated) rect would be added to the set; loaded tiles that
    // fell outside it would be dropped. In this build AddTileToSet / RemoveTileFromSet fold away,
    // leaving only their pre-condition asserts -- reproduced store-for-store against the asm.
    // Gated on the low-res backdrop texture being present (the map is only worked once loaded).
    void MapManager::CalculateCurrentTileSet()
    {
        const uint32_t luDirectoryIndex = static_cast<uint32_t>(meZoomLevel);

        if (mLowResTexture.mpTextureState == NULL)
        {
            return;
        }

        SatNavTile::sRect lWorldRect = mWorldRect;
        if (meZoomLevel == E_ZOOM_HIGH)
        {
            const float lfFactor = 0.25f;
            lWorldRect.mfLeft   = MyAbs(lWorldRect.mfLeft   * lfFactor) + lWorldRect.mfLeft;
            lWorldRect.mfTop    = MyAbs(lWorldRect.mfTop    * lfFactor) + lWorldRect.mfTop;
            lWorldRect.mfRight  = lWorldRect.mfRight  - (lWorldRect.mfRight  * lfFactor);
            lWorldRect.mfBottom = lWorldRect.mfBottom - (lWorldRect.mfBottom * lfFactor);
        }

        const SatNavTileDirectory* lpDirectory = mapDirectories[luDirectoryIndex];
        if (lpDirectory == NULL)
        {
            return;
        }

        // Directory tiles overlapping the world rect would be added (AddTileToSet inlined away).
        for (uint32_t luIndex = 0; luIndex < lpDirectory->muItemCount; ++luIndex)
        {
            const SatNavTileDirectory::sTileItem& lTileItem = lpDirectory->mpaItems[luIndex];
            if (lTileItem.IsWithinWorldRect(lWorldRect))
            {
                const char* lpcName = lTileItem.macBundleName;
                CGS_ASSERT(lpcName != NULL, "lpcName != NULL");
            }
        }

        // Loaded tiles that fell outside the world rect would be removed (RemoveTileFromSet inlined).
        for (uint32_t luIndex = 0; luIndex < KU_TILE_ARRAY_SIZE; ++luIndex)
        {
            const SatNavTile::sTileCache& lTile = maRequestedTiles[luIndex];
            if (lTile.meState != SatNavTile::E_STATE_LOADED)
            {
                continue;
            }
            if (lTile.mpTile->IsWithinWorldRect(lWorldRect))
            {
                continue;
            }

            CGS_ASSERT(lTile.mpTile != NULL, "lpTile != NULL");
            CGS_ASSERT(GetTileState(lTile.mpTile->muID) == SatNavTile::E_STATE_LOADED,
                       "GetTileState( lpTile->muID) == SatNavTile::E_STATE_LOADED");
        }
    }
}
