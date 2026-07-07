#ifndef BRN_MAP_MANAGER_H
#define BRN_MAP_MANAGER_H

#include "types.hpp"
#include "SharedClasses/Gui/SatNav/BrnSatNavTile.h"   // SatNavTile, sTexture/sTileCache/sRect, SatNavTileDirectory

// BrnMapManager.h
// BrnGui::MapManager - owns the sat-nav map's tile working set: the current world/screen
// rects, the low-res backdrop texture + its cache, the six requested-tile cache slots, the
// two zoom-level tile directories, and the flattened "active textures" array the map UI
// draws each frame.
//
// X360 authority (BURNOUT_X360_ARTIST.XEX):
//   MapManager (ctor)         @ 0x82508550
//   RefreshActiveTextureArray @ 0x82448540
//   GetTileState (const)      @ 0x82448760
//   RemoveTileFromSet         @ 0x82448820
//   SetZoomLevel              @ 0x8244F768
// DWARF: references/DecFIGS/dwarfdump/GameSource/Gui/SatNav/BrnMapManager.h
//
// LAYOUT: byte offsets are X360 references (4-byte-pointer ABI), pinned store-for-store
// against the reconstructed bodies. The aggregate spans 0x578 so later methods land cleanly.
// Uses SatNavTile::sTileCache (added to BrnSatNavTile.h alongside this TU).

namespace rw { class IResourceAllocator; }
namespace CgsGui { struct StateInterface; }   // held by pointer only (out-of-batch Construct)
namespace CgsModule { struct Event; }          // RecvEvent payload base (empty marker; carries the GuiCache* in its first word)

namespace BrnGui
{
    // BrnMapManager.h:29
    const uint32_t KU_TILE_ARRAY_SIZE       = 6;
    // BrnMapManager.h:52 -- active-texture array capacity (1 low-res + up to 6 tiles x 3).
    const uint32_t KU_MAX_TEXTURES_IN_CACHE = 19;

    class MainMapComponent;   // owning component; sets mbEnabled directly on the map-active event

    // BrnMapManager.h:48 (X360 `this` spans 0x578 bytes)
    class MapManager
    {
        // MainMapComponent::RecvEvent (BrnMainMap.cpp @0x82458370) stores the map-active byte
        // straight into mbEnabled (X360 `stb r11, 0x5F0(this)` == MapManager +0x564), so the
        // owning component is a friend rather than routing through an invented setter.
        friend class MainMapComponent;

    public:
        // BrnMapManager.h:61
        enum EZoomLevel
        {
            E_ZOOM_MEDIUM = 0,
            E_ZOOM_HIGH   = 1,
            E_ZOOM_COUNT  = 2,
        };

        // BrnMapManager.h:55 -- flattened per-frame active texture set the map UI draws.
        struct sActiveTextures
        {
            SatNavTile::sTexture maTextures[KU_MAX_TEXTURES_IN_CACHE]; // +0x000 (rel), sTexture stride 0x24
            uint32_t             muTextureCount;                       // +0x2AC (rel) == abs +0x560
        };

        MapManager();                                                 // @0x82508550

        void            SetZoomLevel(EZoomLevel leZoomLevel);         // @0x8244F768

        // @0x8244F898 -- on the cache-ready event (type 64), lazily build the low-res backdrop
        // texture state from the loaded map texture resource (once).
        void            RecvEvent(const CgsModule::Event* lpEvent, int32_t liParam);

    private:
        SatNavTile::EBundleState GetTileState(uint32_t luID) const;   // @0x82448760
        void            RemoveTileFromSet(SatNavTile* lpTile);        // @0x82448820
        void            RefreshActiveTextureArray();                  // @0x82448540

        // @0x8244FA80 -- rebuild the working tile set for the current world rect + zoom: add
        // directory tiles overlapping the (zoom-inflated) rect, drop loaded tiles that fell out.
        void            CalculateCurrentTileSet();

    private:
        SatNavTile::sRect       mWorldRect;               // +0x000
        SatNavTile::sRect       mScreenRect;              // +0x010
        SatNavTile::sTexture    mLowResTexture;           // +0x020 (stride 0x24)
        SatNavTile::sTileCache  mLowResTextureCache;      // +0x044 (stride 0x58)
        SatNavTile::sTileCache  maRequestedTiles[KU_TILE_ARRAY_SIZE]; // +0x09C (6 x 0x58 = 0x210)
        SatNavTileDirectory*    mapDirectories[E_ZOOM_COUNT];         // +0x2AC ([E_ZOOM_HIGH]=+0x2B0)
        sActiveTextures         mActiveTextures;          // +0x2B4 (maTextures +0x2B4, muTextureCount +0x560)
        bool                    mbEnabled;                // +0x564
        uint32_t                muTilesRequestedCount;    // +0x568
        CgsGui::StateInterface* mpStateInterface;         // +0x56C
        EZoomLevel              meZoomLevel;              // +0x570
        rw::IResourceAllocator* mpAllocator;              // +0x574
    };
}

#endif // BRN_MAP_MANAGER_H
