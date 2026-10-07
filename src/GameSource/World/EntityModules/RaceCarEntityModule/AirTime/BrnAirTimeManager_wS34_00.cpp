// BrnAirTimeManager_wS34_00.cpp -- AirTimeManager::Release / ::Destruct. Neither has an
// out-of-line console symbol: both are inlined into the race-car module's Release / Destruct.

#include "GameSource/World/EntityModules/RaceCarEntityModule/AirTime/BrnAirTimeManager.h"

namespace BrnWorld
{
    // The four stores RaceCarEntityModule::Release makes on the manager's seat (module +0x180D8):
    // meState = NONE (3), then the three timers to 0.0f.
    bool AirTimeManager::Release()
    {
        meState            = E_AIR_STATE_NONE;
        mfTimeSinceLastAir = 0.0f;
        mfPreviousAirTime  = 0.0f;
        mfTotalAirTime     = 0.0f;
        return true;
    }

    // RaceCarEntityModule::Destruct makes the same four stores on the same seat.
    void AirTimeManager::Destruct()
    {
        meState            = E_AIR_STATE_NONE;
        mfTimeSinceLastAir = 0.0f;
        mfPreviousAirTime  = 0.0f;
        mfTotalAirTime     = 0.0f;
    }
}
