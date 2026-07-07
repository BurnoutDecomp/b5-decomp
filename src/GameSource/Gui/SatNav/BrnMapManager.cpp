#include "GameSource/Gui/SatNav/BrnMapManager.h"

#include "SharedClasses/Gui/SatNav/BrnSatNavTile.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache (RecvEvent resource load)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple / ResourceRequestTypes
#include "pc/gcm/renderengine/renderstates.h"        // renderengine::TextureState (Initialize + GetResourceDescriptor)

#include <new>       // placement new (ctor)
#include <cstdlib>   // qsort (RefreshActiveTextureArray)
#include <cmath>     // fabsf (SetZoomLevel)

// BrnGui::MapManager - the sat-nav map tile manager bodies. The ctor value-initialises the
// leading sub-objects; RefreshActiveTextureArray rebuilds the flattened active-texture set;
// GetTileState / RemoveTileFromSet / SetZoomLevel manage the requested-tile cache. See the
// header for the (X360-pinned) member layout.

// MSVC CRT: construct an array of objects with a per-element ctor function.
// Matches the X360 PPC bl to `vector constructor iterator' in the ctor.
extern "C" void __cdecl _vector_constructor_iterator_(
    void* pBase,
    unsigned int uElementSize,
    int nCount,
    void* (__cdecl* pCtor)(void*));

// rw::Resource default-construct closure passed to the vector iterator
// (X360 symbol rw::Resource::`default constructor closure'). Constructs one
// rw::Resource in place and returns it.
namespace rw { struct Resource; }
extern "C" void* __cdecl rw__Resource___default_constructor_closure_(void* lpSelf);

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
        char* const lp = reinterpret_cast<char*>(this);

        // mWorldRect / mScreenRect (each SatNavTile::sRect -> default (0,0,1,1)).
        new (lp + 0x00) SatNavTile::sRect();
        new (lp + 0x10) SatNavTile::sRect();

        // mLowResTexture bounding boxes (sTexture @ +0x20: mBB @ +4, mBBWorld @ +0x14).
        new (lp + 0x24) SatNavTile::sRect();
        new (lp + 0x34) SatNavTile::sRect();

        // Zero mLowResTextureCache.mTextureStateResources[3] at +0x54 (3 * 20 bytes).
        // X360: v3 = 2 downto 0, five words zeroed per 20-byte stride.
        {
            uint32_t* lpZero = reinterpret_cast<uint32_t*>(lp + 0x54);
            for (int liI = 2; liI >= 0; --liI)
            {
                lpZero[0] = 0u;
                lpZero[1] = 0u;
                lpZero[2] = 0u;
                lpZero[3] = 0u;
                lpZero[4] = 0u;
                lpZero += 5;
            }
        }

        // Construct the 3 rw::Resource entries of each maRequestedTiles[i] cache.
        // Resource array sits at (tile base +0x10); first tile base = +0x9C,
        // so first resource array = +0xAC. Tile stride 88 (0x58), 6 tiles.
        // On X360 rw::Resource is 20 bytes; element size 0x14.
        {
            char* lpResources = lp + 0xAC;
            for (int liI = 5; liI >= 0; --liI, lpResources += 0x58)
            {
                _vector_constructor_iterator_(lpResources, 0x14u, 3,
                    rw__Resource___default_constructor_closure_);
            }
        }

        // mActiveTextures.maTextures[19]: default-construct each texture's two
        // bounding boxes (mBB @ +4, mBBWorld @ +0x14; sTexture stride 36).
        // First texture base = +0x2B4, so mBBWorld[0] = +0x2C8, mBB[0] = +0x2B8.
        {
            char* lpBBWorld = lp + 0x2C8;
            for (int liI = 18; liI >= 0; --liI, lpBBWorld += 0x24)
            {
                new (lpBBWorld - 0x10) SatNavTile::sRect();  // mBB
                new (lpBBWorld)        SatNavTile::sRect();  // mBBWorld
            }
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
