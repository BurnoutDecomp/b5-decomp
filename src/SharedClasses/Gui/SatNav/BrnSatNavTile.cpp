#include "SharedClasses/Gui/SatNav/BrnSatNavTile.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::SatNavTile::IsWithinWorldRect                    @0x82448068
//   BrnGui::SatNavTile::sRect::sRect                         @0x82447F50
//   BrnGui::SatNavTile::sTexture::QSortCallback              @0x82447F78
//   BrnGui::SatNavTileDirectory::sTileItem::IsWithinWorldRect@0x824480E0

namespace BrnGui
{
    // @0x82447F50 -- default rect is (left=0, top=0, right=1, bottom=1). Member init order
    // (left, top, right, bottom) matches the asm store order (flt 0.0 to +0/+4, 1.0 to +8/+0xC).
    SatNavTile::sRect::sRect()
        : mfLeft(0.0f)
        , mfTop(0.0f)
        , mfRight(1.0f)
        , mfBottom(1.0f)
    {
    }

    // @0x82447F78 -- qsort comparator: largest world bounding-box area first (descending).
    int32_t SatNavTile::sTexture::QSortCallback(const void* lpA, const void* lpB)
    {
        CGS_ASSERT((lpA != NULL) && (lpB != NULL), "(lpA != NULL) && (lpB != NULL)");

        const sTexture* lpTextureA = static_cast<const sTexture*>(lpA);
        const sTexture* lpTextureB = static_cast<const sTexture*>(lpB);

        CGS_ASSERT(lpTextureA != NULL, "lpTextureA != NULL");
        CGS_ASSERT(lpTextureB != NULL, "lpTextureB != NULL");

        const float lfAreaA = lpTextureA->mBBWorld.GetArea();
        const float lfAreaB = lpTextureB->mBBWorld.GetArea();

        int32_t liResult = -1;
        if (lfAreaA >= lfAreaB)
        {
            liResult = 0;
        }

        return liResult;
    }

    // @0x82448068 -- true if any of this tile's world bounding boxes overlaps lRect.
    bool SatNavTile::IsWithinWorldRect(const sRect& lRect) const
    {
        if (muTextureCount == 0)
        {
            return false;
        }

        for (uint32_t luIndex = 0; luIndex < muTextureCount; ++luIndex)
        {
            const sRect& lBB = mBBWorld[luIndex];
            if (lBB.mfLeft   <= lRect.mfRight  &&
                lBB.mfRight  >= lRect.mfLeft   &&
                lBB.mfTop    <= lRect.mfBottom &&
                lBB.mfBottom >= lRect.mfTop)
            {
                return true;
            }
        }

        return false;
    }

    // @0x824480E0 -- true if any of this directory item's bounding boxes overlaps lRect.
    bool SatNavTileDirectory::sTileItem::IsWithinWorldRect(const SatNavTile::sRect& lRect) const
    {
        if (muTextureCount == 0)
        {
            return false;
        }

        for (uint32_t luIndex = 0; luIndex < muTextureCount; ++luIndex)
        {
            const SatNavTile::sRect& lBB = mBB[luIndex];
            if (lBB.mfLeft   <= lRect.mfRight  &&
                lBB.mfRight  >= lRect.mfLeft   &&
                lBB.mfTop    <= lRect.mfBottom &&
                lBB.mfBottom >= lRect.mfTop)
            {
                return true;
            }
        }

        return false;
    }
}
