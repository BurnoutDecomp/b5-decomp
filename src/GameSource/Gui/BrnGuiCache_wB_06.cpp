#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Three GuiCache accessors over the
// per-active-race-car / scoring-traffic tables. Each reads ONE named member at its
// asm-proven offset, guarded by the game's debug assert (CGS_ASSERT is a no-op in this
// build, matching the X360 release assert machinery). The two ARCI-indexed accessors
// front-guard the index with the [0, E_ACTIVE_RACE_CAR_INDEX_COUNT) range check the
// X360 emits (it builds an "Invalid EActiveRaceCarIndex : <n>" message); GetScoring
// TrafficCount front-guards the CgsArray "used before Construct/Clear" sentinel.

namespace BrnGui
{
    // @ 0x82443B28 -- maRaceCarConnecting[index] @0xA0EC. asm range-guards a2<0 (h:3886)
    // and a2>=8 (h:3887), then returns the byte with no validity gate.
    bool GuiCache::IsActiveRaceCarConnecting(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(0 <= leActiveRaceCarIndex, "Invalid EActiveRaceCarIndex");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "Invalid EActiveRaceCarIndex");
        return maRaceCarConnecting[leActiveRaceCarIndex];
    }

    // @ 0x82443D78 -- maEventPositionOfRaceCar[index] @0xA130 (s8 place), gated on
    // maEventPositionValid[index] @0xA140: returns the place only when the slot is valid,
    // else 0. asm range-guards a2<0 (h:3960) and a2>=8 (h:3961).
    s32 GuiCache::GetEventPositionOfRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(0 <= leActiveRaceCarIndex, "Invalid EActiveRaceCarIndex");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "Invalid EActiveRaceCarIndex");
        if (maEventPositionValid[leActiveRaceCarIndex])
            return maEventPositionOfRaceCar[leActiveRaceCarIndex];
        return 0;
    }

    // @ 0x824497C0 -- the scoring-traffic CgsArray length (miScoringTrafficCount @0xA3D0).
    // asm forms &maScoringTrafficDataStorage (@0xA150), then reads the count member 640
    // bytes past it; the -1 sentinel means the array was used before Construct/Clear.
    s32 GuiCache::GetScoringTrafficCount() const
    {
        CGS_ASSERT(miScoringTrafficCount != -1,
                   "Array used before Construct/Clear was called");
        return miScoringTrafficCount;
    }
}
