#include "GameSource/Replays/Serialisers/BrnReplayRaceCarSerialiserFrame.h"
#include <cstring>

namespace BrnReplays
{
    void RaceCarSerialiserFrame::Clear()
    {
        mActiveRaceCars.Clear();
        mGlobalRaceCars.Clear();
        for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
        {
            std::memset(&maRenderParams[liCar], 0, sizeof(maRenderParams[liCar]));
            maRenderParams[liCar].Reset();
            maRaceCarModelIds[liCar] = 0;
            maWheelModelIds[liCar] = 0;
        }
    }
}
