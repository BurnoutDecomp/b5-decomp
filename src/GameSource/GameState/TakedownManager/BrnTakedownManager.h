#pragma once

#include "types.hpp"
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"  // TakedownEvent + enums

// BrnGameState::TakedownManager - tracks per-active-car takedown state. MINIMAL / INCREMENTAL
// reconstruction (DecFIGS DWARF, BrnTakedownManager.h): only the nested RaceCarData type + the
// maRaceCarData[8] array (the manager's first member, at offset 0 - which is why the X360 treats
// a TakedownManager* directly as a RaceCarData* of the player's car) are reconstructed here. The
// rest of the manager (camera timers, the mode/progression managers, the owned debug component,
// ~20 update methods) is reconstructed by the TakedownManager TU; this header is extended toward
// that shape. Named members + real types (semantic parity), so consumers access by name.

namespace BrnGameState
{
    struct TakedownManager
    {
        struct RaceCarData
        {
            bool          mabRevengeRelationships[8];
            f32           mfTimeSinceLastTakenDown;
            f32           mfTimeSinceVictimCrashed;
            bool          mbWaitingOnTakedown;
            TakedownEvent mPendingTakedownEvent;
            f32           mfTimeSinceLastTakedown;
            s32           miTakedownChainLength;
            s32           miMultipleTakedownLength;

            void Clear();   // reconstructed by the TakedownManager TU
        };

        RaceCarData maRaceCarData[8];   // offset 0 (per active race car)
        // [remaining TakedownManager members - reconstructed by the TakedownManager TU]
    };
}
