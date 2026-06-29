#pragma once

// ===================================================================================
// BrnProgression::RivalData  -- one persisted "rival" record in the player's progression
//   profile (b5-decomp/.../Progression/BrnProgressionRivalData.h).
//
// Embedded by value as Profile::maRivals[64] (Profile+0x6280, 0x38 stride, count word
// Profile+0x274). Layout PROVEN from BURNOUT_X360_ARTIST.XEX (Profile::AddRival
// @0x82359F40 zeroes the whole 0x38 record: std r4(rivalId) @+0x00, std r5(carId) @+0x08,
// then stw 0 across +0x10..+0x33 and stb 0 @+0x34). Member names/types/order are the
// DecFIGS DWARF (BrnProfile.h:188). All access is by name.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (u64)

namespace BrnProgression
{
    struct RivalData
    {
        // DWARF BrnProfile.h:195 (EState enum).
        enum EState
        {
            E_STATE_LOCKED   = 0,
            E_STATE_UNLOCKED = 1,
            E_STATE_FLEEING  = 2,
            E_STATE_BEATEN   = 3,
            E_STATE_COUNT    = 4
        };

        // Construct(rivalId, carId): the X360 AddRival inlines this (zero everything except
        // the two ids). Declared so Profile::AddRival can name it; body lives with this TU.
        void Construct(CgsID lRivalId, CgsID lCarId);

        CgsID  mRivalId;                      // +0x00
        CgsID  mCarId;                        // +0x08
        EState meState;                       // +0x10
        s32    miEventCount;                  // +0x14
        s32    miTakedownFromCount;           // +0x18
        s32    miVerticalTakedownFromCount;   // +0x1C
        s32    miTakedownToCount;             // +0x20
        s32    miVerticalTakedownToCount;     // +0x24
        s32    miTakedownToInEventCount;      // +0x28
        s32    miTakedownToInLastEventCount;  // +0x2C
        s32    miEventMissingCount;           // +0x30
        bool   mbHasBeenHit;                  // +0x34
    };

    static_assert(sizeof(RivalData) == 0x38, "RivalData must be the 0x38 maRivals stride");
}
