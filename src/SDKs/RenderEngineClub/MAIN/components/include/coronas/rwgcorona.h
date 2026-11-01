#pragma once

#include "BrnCommonTypes.h"
#include "SDKs/EATech/include/cmn/renderengine/types.h"

namespace renderengine
{
// ARTIST823F3350 writes; ARTIST82404F30 reads; DecFIGS rwgcorona.h:12.
struct Corona
{
    Vector3 mvPosition;
    Vector3 mvDirection;
    Vector2 mvSize;
    f32 mfDistance;
    RGBA8 muColour;
    s32 miTextureID;
};
CGS_STATIC_ASSERT((sizeof(Corona) == 0x40), "ARTIST corona record stride");
CGS_STATIC_ASSERT((__alignof(Corona) == 16), "ARTIST corona vectors");
}
