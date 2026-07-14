#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. GuiCache accessors/setters; each reads or
// writes one named member at its asm-proven offset, guarded by the game's debug assert
// (CGS_ASSERT is a no-op in this build, matching the X360 release assert machinery).

namespace BrnGui
{
    // @ 0x824B2FE8 -- index the preset-race table. The X360 bounds-checks against
    // miNumPresetRaces (@0x5280), then returns 120*(idx+170)+this, i.e. the stride-120
    // element idx of maPresetRaces (base 120*170 == 0x4FB0 == maPresetRacesStorage).
    const PresetRace* GuiCache::GetPresetRace(s32 liPresetRaceIndex) const
    {
        CGS_ASSERT(liPresetRaceIndex >= 0 && liPresetRaceIndex < miNumPresetRaces,
                   "liPresetRaceIndex >= 0 && liPresetRaceIndex < miNumPresetRaces");
        return reinterpret_cast<const PresetRace*>(maPresetRacesStorage + 120 * liPresetRaceIndex);
    }

    // @ 0x824EC3C8 -- latch the map-icon manager pointer (v3[4120] = a2, i.e.
    // mpMapIconManager @0x4060). Asserts the incoming pointer is non-null.
    void GuiCache::SetMapIconManager(MapIconManager* lpMapIconManager)
    {
        CGS_ASSERT(lpMapIconManager != nullptr, "Invalid map icon manager");
        mpMapIconManager = lpMapIconManager;
    }

    // @ 0x824EC4C8 -- has the given race car crossed the finish line. Bounds-checks the
    // ARCI (>= 0, < 8), then returns maRaceCarFinished[idx] (@0xA138) only when the
    // per-car position gate maEventPositionValid[idx] (@0xA140) is set, else false.
    bool GuiCache::HasRaceCarFinished(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= 0, "Invalid EActiveRaceCarIndex");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "Invalid EActiveRaceCarIndex");
        if (maEventPositionValid[leActiveRaceCarIndex])
        {
            return maRaceCarFinished[leActiveRaceCarIndex];
        }
        return false;
    }
}
