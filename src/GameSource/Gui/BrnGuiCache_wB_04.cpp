#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h" // SpecificGameModeEventInterface::Event (GetOnlineLandmarkIndex) + BrnGameState::LandmarkIndex

// Reconstructed from BURNOUT_X360_ARTIST.XEX. GuiCache online-mode accessors that index
// the cache's far member tables at their asm-proven offsets. CGS_ASSERT is a no-op in this
// build (CgsAssert.h), matching the project convention for the X360 assert machinery -- each
// getter fires its bounds/validity assert, then returns the raw table element the release
// build would have.

namespace BrnGui
{
    // @ 0x8241E778 -- live count of preset (online) events. mEvents is a CgsArray whose
    // count/ctor sentinel lives at +0x9E54 (mEventsCtorSentinel); -1 means the array was
    // used before Construct/Clear. The X360 reads the same field for the assert and the
    // return (base maEventsStorage @0x8040 + 7700).
    s32 GuiCache::GetNumPresetEvents() const
    {
        CGS_ASSERT(mEventsCtorSentinel != -1, "Array used before Construct/Clear was called");
        return mEventsCtorSentinel;
    }

    // @ 0x8240FB50 -- landmark index for the given online checkpoint slot in the current
    // online round. Bounds-checks the checkpoint against KI_MAX_LANDMARKS_IN_MODE, asserts
    // the online game-mode options table (@0xA800) is present, then bounds-checks the live
    // round index (@0xA7FC) against KU_MAX_ONLINE_ROUNDS_IN_MODE and forwards to the round's
    // Event::GetLandmark (base + 44*miOnlineRoundIndex; sizeof(Event) == 44).
    BrnGameState::LandmarkIndex
        GuiCache::GetOnlineLandmarkIndex(u32 luCheckpointIndex) const
    {
        typedef BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event OnlineModeEvent;

        CGS_ASSERT(luCheckpointIndex < static_cast<u32>(BrnGameState::GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE),
                   "( liCheckpointIndex >= 0 ) && ( liCheckpointIndex < KI_MAX_LANDMARKS_IN_MODE )");

        const OnlineModeEvent* lpOnlineGameModeOptions =
            reinterpret_cast<const OnlineModeEvent*>(maOnlineGameModeOptionsStorage);
        CGS_ASSERT(lpOnlineGameModeOptions != nullptr, "GetOnlineGameModeOptions()");

        CGS_ASSERT(static_cast<u32>(miOnlineRoundIndex) < 10u,
                   "( GetOnlineRoundIndex() >= 0 ) && ( static_cast<uint32_t>( GetOnlineRoundIndex() ) < "
                   "BrnGameState::GameStateModuleIO::KU_MAX_ONLINE_ROUNDS_IN_MODE )");

        return lpOnlineGameModeOptions[miOnlineRoundIndex].GetLandmark(static_cast<s32>(luCheckpointIndex));
    }

    // @ 0x824436D0 -- is the given race car still in car-select. Bounds-checks the ARCI
    // (>= 0, < 8), then returns maOnlinePlayerInCarSelect[idx] (@0xB854, stride 1).
    bool GuiCache::GetOnlinePlayerInCarSelect(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        return maOnlinePlayerInCarSelect[leActiveRaceCarIndex];
    }
}
