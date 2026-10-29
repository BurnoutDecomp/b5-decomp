#pragma once

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplayRaceCarSerialiserFrame.h"

namespace BrnReplays
{
    // X360 RaceCarEntitySerialiser is embedded in RaceCarEntityModule at +0x18600.
    // Its X360 base Construct receives the original record/static capacity and skip flag;
    // the native frame's wider typed size is used for both capacities on PC.
    class RaceCarEntitySerialiser : public BaseSerialiser
    {
    public:
        s32 Construct();
        RaceCarEntitySerialiserStaticLayout* GetStaticLayout();
        RaceCarSerialiserFrame* CheckStaticLayoutCleared();
        s32 Read();
        s32 Write();

    private:
        bool mbStaticLayoutCleared;
    };
}
