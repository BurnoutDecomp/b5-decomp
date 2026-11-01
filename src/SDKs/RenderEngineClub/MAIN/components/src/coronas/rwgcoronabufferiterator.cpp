#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h"

namespace renderengine
{
// ARTIST823F3350. Input rw::RGBA is ABGR; the record stores native RGBA8.
void CoronaBuffer::Iterator::Write(Vector3 lvPosition, Vector3 lvDirection, Vector2 lvSize,
                                  f32 lfDistance, rw::RGBA lColour, s32 liTextureID)
{
    Corona& lrRecord = mpData[muIndex];
    lrRecord.mvPosition = lvPosition;
    lrRecord.mvDirection = lvDirection;
    lrRecord.mvSize = lvSize;
    lrRecord.mfDistance = lfDistance;
    lrRecord.muColour = RGBA8(static_cast<u8>(lColour.m_rgba),
        static_cast<u8>(lColour.m_rgba >> 8), static_cast<u8>(lColour.m_rgba >> 16),
        static_cast<u8>(lColour.m_rgba >> 24));
    lrRecord.miTextureID = liTextureID;
    ++muIndex;
}
}
