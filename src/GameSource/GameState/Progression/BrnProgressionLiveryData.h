#pragma once

// ===================================================================================
// BrnProgression::LiveryData  -- one persisted "chosen livery" record in the player's
//   progression profile (b5-decomp/.../Progression/BrnProgressionLiveryData.h).
//
// Maps a base car id -> the livery-variant car id the player has chosen for it, plus the
// distance driven in that livery. Embedded by value as Profile::maLiveryChoices[512]
// (Profile+0x3280, 0x18 stride, count word Profile+0x270).
//
// Layout PROVEN from BURNOUT_X360_ARTIST.XEX (Profile::SetChosenLiveryIdForBaseCar
// @0x82359C90 / GetChosenLiveryDataForBaseCar @0x82359DC8): 0x18 stride, mBaseCarId @+0x00
// (the 8-byte id `ld 0(r11)` compared to the lookup key), mChosenLiveryCarId @+0x08 (the
// `stdx r27` write), mfDistanceDriven @+0x10 (the `stfs 0.0, 0x10(r11)` reset). Member
// names/types/order are the DecFIGS DWARF (BrnProfile.h:170). All access is by name.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (u64)

namespace BrnProgression
{
    struct LiveryData
    {
        CgsID mBaseCarId;          // +0x00  base car this livery choice is keyed on
        CgsID mChosenLiveryCarId;  // +0x08  the chosen livery-variant car id
        f32   mfDistanceDriven;    // +0x10  distance driven in this livery
    };

    static_assert(sizeof(LiveryData) == 0x18, "LiveryData must be the 0x18 maLiveryChoices stride");
}
