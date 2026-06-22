#pragma once

#include "types.hpp"

// BrnPhysics::Props::eRespawnType -- a prop's respawn classification.
// Reconstructed from BURNOUT_X360_ARTIST.XEX / DecFIGS DWARF (PropRespawnTypesEnum.h:23).
// Returned by PropZoneData::GetRespawnTypeForProp; the X360 asm returns the raw values 0/1/2
// for the in-range cases and E_RESPAWN_TYPE_COUNT (3) for the "Shouldnt get here" fallback.
namespace BrnPhysics
{
namespace Props
{
    enum eRespawnType : s32
    {
        E_RESPAWN              = 0,
        E_DONT_RESPAWN         = 1,
        E_RESPAWN_CHANGED      = 2,
        E_RESPAWN_TYPE_COUNT   = 3,
    };
}
}
