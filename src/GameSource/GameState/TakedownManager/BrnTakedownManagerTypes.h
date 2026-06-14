#pragma once

#include "BrnCommonTypes.h"

namespace BrnGameState
{
    // Recovered from BrnTakedownManagerTypes.h (DecFIGS DWARF). CgsID is a 64-bit
    // hash, so the struct is 8-byte aligned and the owning queue's inline buffer
    // lands at +16.
    typedef u64 CgsID;
    enum EActiveRaceCarIndex : s32 { E_TD_ACTIVE_RACE_CAR_NONE = -1 };
    enum ETakedownType       : s32 { E_TAKEDOWN_NONE = 0 };

    struct TakedownEvent
    {
        EActiveRaceCarIndex meAggressorIndex;
        EActiveRaceCarIndex meVictimIndex;
        CgsID               mAggressorCarID;
        CgsID               mVictimCarID;
        ETakedownType       meType;
        s32                 miMultipleTakedownCount;
        s32                 miTakedownChainCount;
        bool                mbMarkedManTakeDown;
        bool                mbRemote;
        bool                mbSettledScore;
    };
}
