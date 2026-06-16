#include "GameShared/GameClasses/World/CgsWorldMap2D.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsWorld::WorldMap2D::Construct @ 0x82907FD0
//   CgsWorld::WorldMap2D::GetValue(Vector2) const @ 0x82907FF8

namespace CgsWorld
{

void WorldMap2D::Construct(const void* lpData, Vector2 lWorldOrigin, Vector2 lWorldSize)
{
    // The blob begins with a 2-byte width then a 2-byte height, immediately
    // followed by width*height grid bytes. (X360: *(this+32)=*(_WORD*)lpData,
    // *(this+34)=((_WORD*)lpData)[1], *(this+36)=(_WORD*)lpData + 2.)
    const uint8_t* lpByteData = static_cast<const uint8_t*>(lpData);

    muWidth      = static_cast<const uint16_t*>(lpData)[0];
    muHeight     = static_cast<const uint16_t*>(lpData)[1];
    mpValues     = lpByteData + 4;   // skip the 2x uint16 header
    mWorldOrigin = lWorldOrigin;     // stvx128 v1 -> +0
    mWorldSize   = lWorldSize;       // stvx128 v2 -> +16
}

uint8_t WorldMap2D::GetValue(Vector2 lPosition) const
{
    // Normalised position within the world rect (0..1 across the grid).
    // X360 computes 1/size via vrefp + two Newton-Raphson refinement steps
    // (vnmsubfp/vmaddfp); a plain divide is the equivalent reconstruction.
    const float lfRatioX = (lPosition.x - mWorldOrigin.x) / mWorldSize.x;
    const float lfRatioY = (lPosition.y - mWorldOrigin.y) / mWorldSize.y;

    const int32_t liX = static_cast<int32_t>(lfRatioX * muWidth);
    const int32_t liY = static_cast<int32_t>(lfRatioY * muHeight);

    if (liX < 0 || liX >= muWidth || liY < 0 || liY >= muHeight)
    {
        return KU_INVALID_WORLD_MAP_VALUE;
    }

    return mpValues[liY * muWidth + liX];
}

}
