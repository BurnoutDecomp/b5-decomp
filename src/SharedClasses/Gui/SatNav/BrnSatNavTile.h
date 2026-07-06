#ifndef BRN_SAT_NAV_TILE_H
#define BRN_SAT_NAV_TILE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu::Vector2)

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Layout + member names/order/enums from the
// DecFIGS dwarfdump (SharedClasses/Gui/SatNav/BrnSatNavTile.h); offsets are X360 references,
// pinned store-for-store against the pseudocode/asm. Only the members and methods reached by
// the currently-reconstructed bodies are given full definitions; the remaining declared methods
// (Construct/Destruct/SetTexture/... and the sRect helpers) land with the rest of the TU.

namespace rw { class Resource; }

namespace CgsGraphics
{
    class TextureState;
}

namespace CgsGraphics { class Texture2D; }

namespace CgsMemory { class LinearMalloc; }

namespace BrnGui
{
    // BrnSatNavTile.h:32-33
    const uint32_t KU_MAX_NUMBER_OF_TEXTURES_PER_TILE = 3;
    const uint32_t KU_TILE_BUNDLE_NAME_SIZE           = 32;

    // BrnSatNavTile.h:47 (X360: 0x9C bytes)
    struct SatNavTile
    {
        // BrnSatNavTile.h:51
        enum EBundleState
        {
            E_STATE_UNLOADED       = 0,
            E_STATE_REQUEST_LOAD   = 1,
            E_STATE_LOADED         = 2,
            E_STATE_REQUEST_UNLOAD = 3,
            E_STATE_FULL           = 4,
        };

        // BrnSatNavTile.h:63 - 16-byte axis-aligned rect (min/max), little helper struct.
        struct sRect
        {
            float mfLeft;    // +0x00
            float mfTop;     // +0x04
            float mfRight;   // +0x08
            float mfBottom;  // +0x0C

            void  EndianSwap();
            sRect();                                             // @0x82447F50 -> (0,0,1,1)
            sRect(float lfLeft, float lfTop, float lfRight, float lfBottom);
            float GetArea() const;                               // Abs(mfRight-mfLeft)*Abs(mfBottom-mfTop)
        };

        // BrnSatNavTile.h:94 - one loaded texture + its local/world bounding box.
        struct sTexture
        {
            CgsGraphics::TextureState* mpTextureState;  // +0x00
            sRect                      mBB;             // +0x04 (local)
            sRect                      mBBWorld;        // +0x14 (world)

            // qsort comparator, largest world area first. @0x82447F78
            static int32_t QSortCallback(const void* lpA, const void* lpB);
        };

        sRect    mBB[KU_MAX_NUMBER_OF_TEXTURES_PER_TILE];       // +0x00
        sRect    mBBWorld[KU_MAX_NUMBER_OF_TEXTURES_PER_TILE];  // +0x30
        int32_t  miXOffset;                                     // +0x60
        int32_t  miYOffset;                                     // +0x64
        uint32_t muTextureCount;                                // +0x68
        CgsGraphics::Texture2D* mpapTextures[KU_MAX_NUMBER_OF_TEXTURES_PER_TILE]; // +0x6C
        char     macBundleName[KU_TILE_BUNDLE_NAME_SIZE];       // +0x78
        uint32_t muID;                                          // +0x98

        void            Construct(const char* lacTileName, int32_t liXOffset, int32_t liYOffset,
                                  sRect* lBoundingBoxLocal, sRect* lBoundingBoxWorld,
                                  CgsGraphics::Texture2D** lapTexture, uint32_t luTextureCount,
                                  CgsMemory::LinearMalloc& lMalloc);
        void            Destruct();
        uint32_t        GetTextureCount() const;
        void            SetTexture(CgsGraphics::Texture2D* lpTexture, uint32_t luIndex);
        CgsGraphics::Texture2D* GetTexture(uint32_t luIndex) const;
        const sRect&    GetBoundingBox(uint32_t luIndex) const;
        const sRect&    GetWorldBoundingBox(uint32_t luIndex) const;
        bool            IsWithinScreenRect(const sRect& lRect) const;
        bool            IsWithinWorldRect(const sRect& lRect) const;   // @0x82448068
    };

    // BrnSatNavTile.h:166
    struct SatNavTileDirectory
    {
        // BrnSatNavTile.h:171 - one serialised tile record inside the directory resource.
        // X360 layout pinned by IsWithinWorldRect @0x824480E0: loop base r3+4 == &mBB[0].mfTop,
        // so mBB starts at +0x00; muTextureCount at +0x54. Total size 0x58.
        struct sTileItem
        {
            SatNavTile::sRect mBB[KU_MAX_NUMBER_OF_TEXTURES_PER_TILE];  // +0x00
            char              macBundleName[KU_TILE_BUNDLE_NAME_SIZE];  // +0x30
            uint32_t          muID;                                     // +0x50
            uint32_t          muTextureCount;                           // +0x54

            bool IsWorldPointInTile(Vector2 lPoint) const;
            bool IsWithinWorldRect(const SatNavTile::sRect& lRect) const;  // @0x824480E0
            void FixUp(const rw::Resource& lBaseResource);
            void FixDown(const rw::Resource& lBaseResource);
        };

        uint32_t   muItemCount;   // +0x00
        uint32_t   muWidth;       // +0x04
        uint32_t   muHeight;      // +0x08
        uint32_t   muTileWidth;   // +0x0C
        uint32_t   muTileHeight;  // +0x10
        sTileItem* mpaItems;      // +0x14
        char       macName[KU_TILE_BUNDLE_NAME_SIZE]; // +0x18
    };
}

#endif // BRN_SAT_NAV_TILE_H
