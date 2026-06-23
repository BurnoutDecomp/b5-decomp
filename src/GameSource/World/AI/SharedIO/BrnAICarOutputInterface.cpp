#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"

// .cpp definition home for the two X360 out-of-line members of
// BrnAI::AIModuleIO::AICarOutputInterface (the other accessors stay inline in the header).
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). Both bodies
// gate the AI-car index against [0, KI_MAX_OUT_OF_RANGE_RACE_CARS) with a non-gating assert
// tripwire (the X360 `< 0` / `>= 0x23 (35)` branch-to-assert path falls through and still
// indexes), exactly as the asm shows.
//
//   GetAISectionIndex            @0x8230F888
//     - assert idx >= 0 && idx < 35  (BrnAICarOutputInterface.h:229 == 0xE5)
//     - `addi r11, idx, 0xA4C; slwi r11, 1; lhzx r3, r11, this` ->
//       u16 read at byte +(2*(idx+2636)) == mauAISections[idx] (mauAISections @ +5272, stride 2).
//     Caller (X360): BrnGameState::ModeManager::CheckForOutOfRangeCarsReachingFinish.
//
//   SetAICarDistanceToCheckpoint @0x827644F0
//     - assert idx >= 0 && idx < 35  (BrnAICarOutputInterface.h:212 == 0xD4)
//     - assert value >= 0.0f ("The lfDistToRouteEnd  is less than 0! Mental!",
//       BrnAICarOutputInterface.h:213 == 0xD5; `lfs f0, flt(=0.0); fcmpu; bge` skips the assert
//       when value >= 0 -- still a non-gating tripwire, the store happens regardless).
//     - `addi r11, idx, 0x503; slwi r11, 2; stfsx f31, r11, this` ->
//       f32 store at byte +(4*(idx+1283)) == mafDistanceToCheckpoint[idx]
//       (mafDistanceToCheckpoint @ +5132, stride 4).
//     Caller (X360): BrnAI::AIModule::ExportCarData.

namespace BrnAI
{
namespace AIModuleIO
{
    u16 AICarOutputInterface::GetAISectionIndex(s32 liAICarIndex) const
    {
        CGS_ASSERT(liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                   "liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");
        return mauAISections[liAICarIndex];
    }

    void AICarOutputInterface::SetAICarDistanceToCheckpoint(s32 liAICarIndex, f32 lfDistanceToCheckpoint)
    {
        CGS_ASSERT(liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                   "liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");
        CGS_ASSERT(lfDistanceToCheckpoint >= 0.0f,
                   "The lfDistToRouteEnd  is less than 0! Mental!");
        mafDistanceToCheckpoint[liAICarIndex] = lfDistanceToCheckpoint;
    }
}
}
