// BrnTrafficCheckManager_wS34_00.cpp -- TrafficCheckManager::Release / ::Destruct. Neither has
// an out-of-line console symbol: both are inlined into the race-car module's Release / Destruct.

#include "GameSource/World/EntityModules/RaceCarEntityModule/TrafficCheck/BrnTrafficCheckManager.h"

namespace BrnWorld
{
    // The two stores RaceCarEntityModule::Release makes on the manager's seat (module +0x180E8):
    // the chain to 0 and the dry-spell timer to the 6.0f cap (the value BrnTrafficCheckManager.cpp
    // names KF_TIME_MAX_TIME_BETWEEN_CHECKS), so a fresh session starts with no chain running.
    bool TrafficCheckManager::Release()
    {
        miCurrentCheckChain  = 0;
        mfTimeSinceLastCheck = 6.0f;
        return true;
    }

    // RaceCarEntityModule::Destruct makes the same two stores on the same seat.
    void TrafficCheckManager::Destruct()
    {
        miCurrentCheckChain  = 0;
        mfTimeSinceLastCheck = 6.0f;
    }
}
