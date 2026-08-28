#include "GameSource/Gui/SatNav/BrnMapManager.h"

#include "SharedClasses/Gui/SatNav/BrnSatNavTile.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache (RecvEvent resource load)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple / ResourceRequestTypes
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // [map arm] StateInterface::GetAllocator (Construct)
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"    // [map arm] MapTransform (Construct's world-corner pair)
#include "pc/gcm/renderengine/renderstates.h"        // renderengine::TextureState (Initialize + GetResourceDescriptor)

#include <new>       // placement new (ctor)
#include <cstdlib>   // qsort (RefreshActiveTextureArray)
#include <cmath>     // fabsf (SetZoomLevel)

// BrnGui::MapManager - the sat-nav map tile manager bodies. The ctor value-initialises the
// leading sub-objects; RefreshActiveTextureArray rebuilds the flattened active-texture set;
// GetTileState / RemoveTileFromSet / SetZoomLevel manage the requested-tile cache. See the
// header for the (X360-pinned) member layout.

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
    // ⭐ [map arm 2026-08-27] REWRITTEN MEMBER-BY-NAME (was raw `this`+X360-offset
    // placement news through two hand-declared, never-defined CRT closure externs --
    // both un-mountable AND layout-corrupting on the LLP64 host, where every offset
    // past the first pointer differs from the console's). The console's placement-new
    // runs ARE the compiler-emitted member construction + the two explicit init loops;
    // the by-name folds below are those exact semantics on the host layout. The
    // `vector constructor iterator` over rw::Resource::`default constructor closure'
    // is a per-element rw::Resource default construction == value-init of the POD.
    // ---------------------------------------------------------------------------
    MapManager::MapManager()
    {
        // mWorldRect / mScreenRect / mLowResTexture's two boxes / every active-texture
        // entry's two boxes: sRect::sRect() -> (0,0,1,1). The console emits these as the
        // inline placement-new runs at +0x00/+0x10/+0x24/+0x34/+0x2B4.. .
        mWorldRect            = SatNavTile::sRect();
        mScreenRect           = SatNavTile::sRect();
        mLowResTexture.mBB      = SatNavTile::sRect();
        mLowResTexture.mBBWorld = SatNavTile::sRect();
        for (uint32_t luTexture = 0; luTexture < KU_MAX_TEXTURES_IN_CACHE; ++luTexture)
        {
            mActiveTextures.maTextures[luTexture].mBB      = SatNavTile::sRect();
            mActiveTextures.maTextures[luTexture].mBBWorld = SatNavTile::sRect();
        }

        // Zero mLowResTextureCache.mTextureStateResources[3] (the console's 3 x 5-word
        // zero loop at +0x54).
        for (int liResource = 0; liResource < 3; ++liResource)
        {
            mLowResTextureCache.mTextureStateResources[liResource] = rw::Resource();
        }

        // Default-construct the 3 rw::Resource entries of each maRequestedTiles[i]
        // cache (the console's vector-constructor-iterator loop at +0xAC, stride 0x58).
        for (uint32_t luTile = 0; luTile < KU_TILE_ARRAY_SIZE; ++luTile)
        {
            for (int liResource = 0; liResource < 3; ++liResource)
            {
                maRequestedTiles[luTile].mTextureStateResources[liResource] = rw::Resource();
            }
        }
    }

    // ---------------------------------------------------------------------------
    // BrnGui::MapManager::Construct
    //
    // X360 ARTIST @0x82458590 ([map arm 2026-08-27]). Adopt the state interface and
    // its allocator, reset the whole working set, and seed the low-res backdrop slot:
    // local BB = the unit rect, world BB = the WHOLE world rect (the two
    // MapTransform::Transform calls take the unit corners (0,0)/(1,1) from the
    // NORMALISED space (unk_82FB2FA0) into the WORLD space (unk_82FB3610) -- which is
    // by definition {smv4WorldRect.min, smv4WorldRect.max}; the backdrop texture
    // covers all of Paradise City).
    //   *(+0x56C) = lpStateInterface        *(+0x574) = iface->mpAllocator (h:337 assert)
    //   mapDirectories[2] = 0               memset(maRequestedTiles, 0, 0x210)
    //   muTilesRequestedCount = 0           mbEnabled = 0
    //   meZoomLevel = E_ZOOM_MEDIUM (0)     mLowResTextureCache.mpTile = 0
    //   mLowResTexture.mpTextureState = 0   mLowResTexture.mBB = (0,0,1,1)
    //   mLowResTexture.mBBWorld = {world(0,0), world(1,1)}
    // ---------------------------------------------------------------------------
    void MapManager::Construct(CgsGui::StateInterface* lpStateInterface)
    {
        CGS_ASSERT(lpStateInterface != NULL, "lpStateInterface != NULL");   // cpp:62
        mpStateInterface = lpStateInterface;
        // GetAllocator carries the console's own inlined mpAllocator assert
        // (CgsGuiStateInterface.h:337).
        mpAllocator = lpStateInterface->GetAllocator();

        mapDirectories[E_ZOOM_MEDIUM] = NULL;
        mapDirectories[E_ZOOM_HIGH]   = NULL;

        // The console memsets the whole 6-slot requested-tile array (0x210 bytes at
        // +0x9C); member-wise zero on the host layout.
        for (uint32_t luTile = 0; luTile < KU_TILE_ARRAY_SIZE; ++luTile)
        {
            SatNavTile::sTileCache& lrTile = maRequestedTiles[luTile];
            lrTile.meState        = SatNavTile::E_STATE_UNLOADED;
            lrTile.muID           = 0;
            lrTile.mpTile         = NULL;
            lrTile.muTextureCount = 0;
            for (int liResource = 0; liResource < 3; ++liResource)
            {
                lrTile.mTextureStateResources[liResource] = rw::Resource();
                lrTile.mapTextureStates[liResource]       = NULL;
            }
        }

        muTilesRequestedCount = 0;
        mbEnabled             = false;
        meZoomLevel           = E_ZOOM_MEDIUM;

        mLowResTextureCache.mpTile    = NULL;
        mLowResTexture.mpTextureState = NULL;

        mLowResTexture.mBB.mfLeft   = 0.0f;
        mLowResTexture.mBB.mfTop    = 0.0f;
        mLowResTexture.mBB.mfRight  = 1.0f;
        mLowResTexture.mBB.mfBottom = 1.0f;

        // The two normalised->world corner transforms (the console's stacked
        // smm33NormalisedSpace/smm33WorldSpace copies + the Transform pair).
        {
            Vector2 lv2Zero;
            lv2Zero.x = 0.0f; lv2Zero.y = 0.0f; lv2Zero.z = 0.0f; lv2Zero.w = 0.0f;
            Vector2 lv2One;
            lv2One.x = 1.0f; lv2One.y = 1.0f; lv2One.z = 0.0f; lv2One.w = 0.0f;

            const Vector2 lv2WorldMin = MapTransform::Transform(
                lv2Zero, MapTransform::GetNormalisedSpace(), MapTransform::GetWorldSpace());
            const Vector2 lv2WorldMax = MapTransform::Transform(
                lv2One, MapTransform::GetNormalisedSpace(), MapTransform::GetWorldSpace());

            mLowResTexture.mBBWorld.mfLeft   = lv2WorldMin.x;
            mLowResTexture.mBBWorld.mfTop    = lv2WorldMin.y;
            mLowResTexture.mBBWorld.mfRight  = lv2WorldMax.x;
            mLowResTexture.mBBWorld.mfBottom = lv2WorldMax.y;
        }
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

        // The low-res sat-nav map texture resource (id 199, a localised-text-class resource).
        const uint32_t luMapTextureResourceId = 199u;

        CgsGui::sResourceTuple lResourceTuple;
        lResourceTuple.muId   = luMapTextureResourceId;
        lResourceTuple.meType = CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT;   // 11

        if (!lpCache->EnsureResourceIsLoaded(lResourceTuple))
        {
            return;
        }

        const void* lpTexture2D = lpCache->GetLoadedResource(luMapTextureResourceId);
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
        uint32_t laDescriptor[10];
        renderengine::TextureState::GetResourceDescriptor(laDescriptor);

        rw::Resource lAllocatedResource = mpAllocator->DoAllocate(
            *reinterpret_cast<const rw::ResourceDescriptor*>(laDescriptor), NULL);

        mLowResTextureCache.mTextureStateResources[0] = lAllocatedResource;

        renderengine::TextureState* lpState = renderengine::TextureState::Initialize(
            &mLowResTextureCache.mTextureStateResources[0], &lParams);

        mLowResTextureCache.mapTextureStates[0] =
            reinterpret_cast<CgsGraphics::TextureState*>(lpState);
        mLowResTexture.mpTextureState =
            reinterpret_cast<CgsGraphics::TextureState*>(lpState);
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
