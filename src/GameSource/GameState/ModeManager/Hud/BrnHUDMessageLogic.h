#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"               // CgsID (== u64)
#include "GameSource/BurnoutConstants.h"  // EActiveRaceCarIndex

// Minimal owning home for the ObjectPool<BufferedCrashingCar,8,s32> element so the
// instantiation in ObjectPool_BufferedCrashingCar_8.cpp has a complete type. Only the
// nested BufferedCrashingCar record is declared here; the remaining BrnGameState::
// HUDMessageLogic members (mActionQueue, the pool member itself, etc.) are owned by their
// respective ledger TUs and land with those slices. Field layout + names are authoritative
// from the DecFIGS DWARF (BrnHUDMessageLogic.h:244-248). The 16-byte element stride this
// produces matches the X360 pool layout proven by the pseudocode offsets (free-queue at byte
// 128, miNumObjectsFree at 160, BitArray at 168 => 8 * 16-byte elements occupy bytes 0..127).
namespace BrnGameState
{
class HUDMessageLogic
{
public:
    // BrnHUDMessageLogic.h:244 -- one car queued for an online-crash HUD message.
    struct BufferedCrashingCar
    {
        CgsID               mRivalID;               // BrnHUDMessageLogic.h:246 (u64, +0)
        f32                 mfTimeUntilUnbuffered;  // BrnHUDMessageLogic.h:247 (+8)
        EActiveRaceCarIndex meActiveRaceCarIndex;   // BrnHUDMessageLogic.h:248 (+12)
    };
};
}
