#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// BrnAI::ResetOnTrackManager out-of-line members. This batch bodies GetAICar
// (@0x82765878); the file-scope static perf-mon handles are defined here so the class is
// complete. Construct (@0x82791A48) is declared in the header but is NOT bodied in this slice
// (its X360 body inlines the embedded ResetOnTrackDebugComponent's own Construct(owner) and a
// perf-mon-registration path whose foreign type interiors are owned by other, not-yet-
// reconstructed TUs; bodying it here would require fabricating those layouts).

namespace BrnAI
{
    // File-scope static perf-mon handles (DWARF BrnResetOnTrackManager.h:349-351;
    // X360 dword_82F3026C / dword_82F30274 / dword_82F30270).
    s32 ResetOnTrackManager::miInitialCoordinatesPM;
    s32 ResetOnTrackManager::muAvoidHNGPM;
    s32 ResetOnTrackManager::muTestLineHNGPM;

    // Never called; pins the request-queue offset from inside the class (offsetof on a private
    // member is only legal within class scope).
    void ResetOnTrackManager::_AssertLayout()
    {
        static_assert(offsetof(ResetOnTrackManager, mResetOnTrackRequestQueue) == 0x000,
                      "ROT queue offset drift");
    }

    // X360 0x82765878. Private helper; called by 17 sites (PlayerIsLookingBackwards,
    // ComputeInitialCoordinatesStandard, ResetAwayFromPlayer, ...).
    //
    // Two range asserts (E_GLOBAL_RACE_CAR_INDEX_0 <= index < E_GLOBAL_RACE_CAR_INDEX_COUNT ==
    // 35), then return &mpaAICars[index]: the X360 forms 0x1560*index + mpaAICars
    // (sizeof(AICar) == 0x1560 == 5472). AICar is opaque here (its full layout is another TU's),
    // so the element address is computed by the X360-attested byte stride rather than by
    // pointer subscript on the incomplete type -- the result is the same &mpaAICars[index].
    AICar* ResetOnTrackManager::GetAICar(EGlobalRaceCarIndex leGlobalRaceCarIndex)
    {
        CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                   "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
        CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

        static const u32 KU_AI_CAR_STRIDE = 0x1560;  // sizeof(AICar) == 5472 (X360-attested)
        return reinterpret_cast<AICar*>(
            reinterpret_cast<u8*>(mpaAICars) + KU_AI_CAR_STRIDE * static_cast<u32>(leGlobalRaceCarIndex));
    }
}
