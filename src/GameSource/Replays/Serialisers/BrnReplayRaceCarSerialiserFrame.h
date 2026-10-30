#pragma once

#include "types.hpp"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include <cstddef>

namespace BrnReplays
{
    // ARTIST RaceCarSerialiserFrame::Clear 0x82657748 clears the two output
    // interfaces, eight 0x14A0 render records and two eight-entry ID arrays.
    // Read/Write transfer the complete 0xD7E0-byte frame as one block.
    struct RaceCarSerialiserFrame
    {
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface mActiveRaceCars;
        BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface mGlobalRaceCars;
        BrnWorld::ActiveRaceCar::RenderParams maRenderParams[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        CgsID maRaceCarModelIds[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        CgsID maWheelModelIds[E_ACTIVE_RACE_CAR_INDEX_COUNT];

        RaceCarSerialiserFrame* Clear();
    };

    // The serialiser allocates 0x1B000 bytes; the two frames occupy 0x1AFC0.
    struct RaceCarEntitySerialiserStaticLayout
    {
        RaceCarSerialiserFrame mPreviousFrame;
        RaceCarSerialiserFrame mLiveFrame;
    };

    CGS_STATIC_ASSERT((offsetof(RaceCarSerialiserFrame, mGlobalRaceCars) == 0x28F0), "ARTIST global replay output");
    CGS_STATIC_ASSERT((offsetof(RaceCarSerialiserFrame, maRenderParams) == 0x3260), "ARTIST replay render records");
    CGS_STATIC_ASSERT((offsetof(RaceCarSerialiserFrame, maRaceCarModelIds) == 0xD760), "ARTIST replay car IDs");
    CGS_STATIC_ASSERT((offsetof(RaceCarSerialiserFrame, maWheelModelIds) == 0xD7A0), "ARTIST replay wheel IDs");
    CGS_STATIC_ASSERT((sizeof(RaceCarSerialiserFrame) == 0xD7E0), "ARTIST replay frame size");
    CGS_STATIC_ASSERT((offsetof(RaceCarEntitySerialiserStaticLayout, mLiveFrame) == 0xD7E0), "ARTIST live replay frame");
    CGS_STATIC_ASSERT((sizeof(RaceCarEntitySerialiserStaticLayout) == 0x1AFC0), "two native replay frames");
}
