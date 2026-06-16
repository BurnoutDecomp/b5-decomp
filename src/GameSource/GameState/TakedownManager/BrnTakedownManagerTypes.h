#pragma once

#include "BrnCommonTypes.h"
#include "GameSource/GameState/BrnTakedownType.h"   // BrnGameState::ETakedownType (canonical)

namespace BrnGameState
{
    // Recovered from BrnTakedownManagerTypes.h (DecFIGS DWARF). CgsID is a 64-bit
    // hash, so the struct is 8-byte aligned and the owning queue's inline buffer
    // lands at +16.
    typedef u64 CgsID;

    // Slot index of an active race car (BrnGameState scope, per the DWARF). ETakedownType is
    // now the canonical one from BrnTakedownType.h (deduped - both used to define it minimally).
    // (NOTE: a same-valued EActiveRaceCarIndex also lives at global scope in BurnoutConstants.h,
    // used unqualified by the vehicle/traffic IO headers; unifying those scopes is a separate
    // cleanup, so this BrnGameState one is kept and completed to the full value set here.)
    enum EActiveRaceCarIndex : s32
    {
        E_ACTIVE_RACE_CAR_INDEX_INVALID = -1,
        E_ACTIVE_RACE_CAR_INDEX_0       = 0,
        E_ACTIVE_RACE_CAR_INDEX_1       = 1,
        E_ACTIVE_RACE_CAR_INDEX_2       = 2,
        E_ACTIVE_RACE_CAR_INDEX_3       = 3,
        E_ACTIVE_RACE_CAR_INDEX_4       = 4,
        E_ACTIVE_RACE_CAR_INDEX_5       = 5,
        E_ACTIVE_RACE_CAR_INDEX_6       = 6,
        E_ACTIVE_RACE_CAR_INDEX_7       = 7,
        E_ACTIVE_RACE_CAR_INDEX_COUNT   = 8,
    };

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
