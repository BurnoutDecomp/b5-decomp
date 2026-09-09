#pragma once

// Traffic -> GUI shared-IO element payload: the per-vehicle score record the traffic module
// stages for the HUD's score-target markers, and the fixed array of them that travels as the
// GUI traffic-car-info event payload.
//
// Element size is console-authoritative: the GUI cache seats twenty of these in a 640-byte span
// followed by the array's count word, i.e. a 32-byte stride (Vector3 16 + f32 4 + three 16-bit
// fields 6 -> 26, padded to 32 by the Vector3 16-align). The array itself is 640 + 4, padded to
// 656 by the same alignment, which is the byte count the world bridge posts.
#include "types.hpp"                                          // f32/s16/u16/s32
#include "BrnCommonTypes.h"                                   // Vector3
#include "GameShared/GameClasses/Containers/CgsArray.h"      // Array<T,N>

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // Capacity of the score-target array (the GUI shows at most this many scored cars).
    const s32 KI_MAX_CAR_SCORES_TO_SHOW = 20;

    // One scorable traffic vehicle as the HUD sees it.
    struct alignas(16) VehicleScoreData
    {
        Vector3 mPosition;          // +0x00
        f32     mfDistanceSquared;  // +0x10
        s16     miScore;            // +0x14
        s16     miMultiplier;       // +0x16
        u16     muVehicleIndex;     // +0x18
    };

    static_assert(sizeof(VehicleScoreData) == 0x20, "VehicleScoreData stride 0x20");

    // The score-target list: the GUI traffic-car-info event IS this array (no event header).
    typedef Array<VehicleScoreData, KI_MAX_CAR_SCORES_TO_SHOW> ScoringVehicleArray;

    static_assert(sizeof(ScoringVehicleArray) == 656, "ScoringVehicleArray is the 656-byte GUI payload");
}
}
