#pragma once

// BrnReplays::RaceCarSerialiserFrame, recovered from ARTIST's X360 consumers.
//
// The console record is a typed snapshot, even though RaceCarEntitySerialiser::Read/Write
// pass the whole live frame to BaseSerialiser.  ARTIST 0x822F5CF8 and 0x82306120 identify
// these fields directly: active interface at +0, global interface at +10480, eight
// ActiveRaceCar::RenderParams at +12896 (stride 5280), then the model and wheel CgsID arrays
// at +55136 and +55200.  The two output-interface copies and the eight RenderParams copies
// are member-wise on the host so ResourceHandle and EventQueue pointers keep native width.
//
// X360 frame size is 0xD7E0 (55264); the native PC frame is 55328 because the actual active
// output interface widens from 10480 to 10544 and each CgsID is the native 8-byte type.
// This preserves native pointer identity and does not claim cross-bitness replay-file
// compatibility.  The outer owner keeps its two typed frames plus the original 64-byte
// reserve/alignment tail, so its native allocation is 110720 (0x1B080).

#include "types.hpp"
#include "BrnCommonTypes.h" // CgsID
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include <cstddef>

namespace BrnReplays
{
    static const u32 KU_RACECAR_FRAME_SIZE_X360 = 0xD7E0u;
    static const u32 KU_RACECAR_FRAME_SLOT_COUNT = 8u;
    static const u32 KU_RACECAR_STATIC_LAYOUT_TAIL_SIZE = 64u;

    struct RaceCarSerialiserFrame
    {
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
            mActiveRaceCarOutputInterface;
        BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface
            mGlobalRaceCarOutputInterface;
        BrnWorld::ActiveRaceCar::RenderParams maRenderParams[KU_RACECAR_FRAME_SLOT_COUNT];
        CgsID maRaceCarIds[KU_RACECAR_FRAME_SLOT_COUNT];
        CgsID maWheelIds[KU_RACECAR_FRAME_SLOT_COUNT];

        // X360 0x82657748: clear the two interfaces, reset every render-parameter record,
        // clear both model-id arrays, and return this frame.
        RaceCarSerialiserFrame* Clear();

        // X360 0x822E7820: member-wise frame copy; each RenderParams copy retains the
        // destination queue storage while copying the source queue's live events.
        RaceCarSerialiserFrame& operator=(const RaceCarSerialiserFrame& lrOther);
    };

    struct RaceCarEntitySerialiserStaticLayout
    {
        RaceCarSerialiserFrame mPreviousFrame;
        RaceCarSerialiserFrame mLiveFrame;
        u8 maTail[KU_RACECAR_STATIC_LAYOUT_TAIL_SIZE];
    };

    static_assert(sizeof(BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface) == 10544,
                  "Native RaceCar active interface width changed");
    static_assert(sizeof(BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface) == 2416,
                  "RaceCar global interface width changed");
    static_assert(sizeof(BrnWorld::ActiveRaceCar::RenderParams) == 5280,
                  "RaceCar RenderParams width changed");
    static_assert(sizeof(RaceCarSerialiserFrame) == 55328,
                  "Native RaceCar replay frame must use the typed 64-bit record");
    static_assert(sizeof(RaceCarEntitySerialiserStaticLayout) == 110720,
                  "Native RaceCar static owner must preserve two frames and the 64-byte tail");
    static_assert(offsetof(RaceCarSerialiserFrame, mGlobalRaceCarOutputInterface) == 10544,
                  "Native active/global replay interface order changed");
    static_assert(offsetof(RaceCarSerialiserFrame, maRenderParams) == 12960,
                  "Native replay RenderParams array offset changed");
    static_assert(offsetof(RaceCarSerialiserFrame, maRaceCarIds) == 55200,
                  "Native replay car-id array offset changed");
    static_assert(offsetof(RaceCarSerialiserFrame, maWheelIds) == 55264,
                  "Native replay wheel-id array offset changed");
}
