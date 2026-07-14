#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/BrnGameStateTypes.h"   // BrnGameState::LandmarkIndex (returned BY VALUE)

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The last residual GuiCache accessors: two
// array-indexed forwarders (scoring-traffic / preset-event), the two landmark getters, and
// the online-finish-point popcount. Each accesses named members at its asm-proven offset,
// guarded by the game's debug assert (CGS_ASSERT is a no-op in this build, matching the X360
// release assert machinery).

// ------------------------------------------------------------------------------------------
// Un-homed element accessors the two indexed getters tail-forward into. The GuiCache TU only
// forms the array object base + index and hands them to these callees (the element sizeof /
// stride is not attested here, so the index arithmetic necessarily lives in the callee). A
// not-yet-homed callee is satisfied by its declaration under cl /c; no body is needed.
// ------------------------------------------------------------------------------------------
namespace BrnTraffic
{
    // X360 @0x82450718 tail-calls this scoring-traffic CgsArray element accessor. The ledger
    // truncates its symbol to "BrnTraffic::BrnTraf"; it is homed in the class:BrnTraffic
    // scoring TU. It takes the array object base -- GuiCache::maScoringTrafficDataStorage
    // (the mTrafficCarInfo.mScoreTargets CgsArray @0xA150) -- and the element index, and
    // returns the addressed ScoringTrafficData. FLAG: exact symbol name unrecovered (truncated
    // in the ledger).
    const ScoringTrafficData* GetScoringTrafficDataElement(const u8* lpScoreTargetsArray, u32 luIndex);
}

namespace BrnGui
{
    // X360 sub_8241E430 -- the mEvents CgsArray<Event> element accessor GetPresetEvent
    // tail-calls (ledger: [external/unknown], un-named). It takes the events-array object base
    // -- GuiCache::maEventsStorage (the mEvents CgsArray @0x8040) -- and the element index, and
    // returns the addressed preset Event, exposed here through the minimal-slice PresetEvent.
    // FLAG: symbol un-named in the ledger (sub_8241E430).
    const PresetEvent* GetPresetEventAtIndex(const u8* lpEventsArray, u32 luIndex);
}

namespace BrnGameState
{
    // FLAG: sentinel value inferred from the committed LandmarkIndex convention. LandmarkIndex
    // is a signed-16-bit wrapper (BrnGameStateTypes.h); that header documents K_INVALID_LANDMARK
    // as a sentinel but does not yet define it, and no other TU homes it. The X360 compares
    // mEventDestinationLandmarkIndex against the data word word_82F25440, whose stored value is
    // not dumped, so 0xFFFF (-1) is used per the s16 invalid-index convention -- not a novel
    // magic number. Internal linkage (const at namespace scope); no ODR reach.
    static const u16 K_INVALID_LANDMARK = 0xFFFF;
}

namespace BrnGui
{
    // @ 0x82450718 -- index the scoring-traffic array (mTrafficCarInfo.mScoreTargets @0xA150).
    // The X360 forms &maScoringTrafficDataStorage, front-guards the CgsArray "used before
    // Construct/Clear" sentinel (miScoringTrafficCount @0xA3D0 == -1) and the upper bound
    // (luIndex < count), then tail-forwards (base, index) to the array's element accessor and
    // returns its ScoringTrafficData*.
    const BrnTraffic::ScoringTrafficData* GuiCache::GetScoringTrafficData(u32 luIndex) const
    {
        CGS_ASSERT(miScoringTrafficCount != -1, "Array used before Construct/Clear was called");
        CGS_ASSERT(luIndex < static_cast<u32>(miScoringTrafficCount),
                   "liIndex >= 0 && liIndex < mTrafficCarInfo.mScoreTargets.GetLength()");
        return BrnTraffic::GetScoringTrafficDataElement(maScoringTrafficDataStorage, luIndex);
    }

    // @ 0x8240FA88 -- the event's destination landmark index. Valid only in the race-style game
    // modes (same mode gate as GetEventDestinationDistrict; the X360 skips the assert for
    // meGameModeType in {0,1,10,6,8,5}); then asserts the stored index is not the invalid-
    // landmark sentinel. Returns mEventDestinationLandmarkIndex (@0x9F4C) wrapped as a
    // LandmarkIndex (the X360 copies the raw u16 into the caller's out slot).
    BrnGameState::LandmarkIndex GuiCache::GetEventDestinationLandmarkIndex() const
    {
        CGS_ASSERT(
            (meGameModeType == 0) || (meGameModeType == 1) || (meGameModeType == 10)
                || (meGameModeType == 6) || (meGameModeType == 8) || (meGameModeType == 5),
            "race-style game mode required for GetEventDestinationLandmarkIndex");
        CGS_ASSERT(mEventDestinationLandmarkIndex != BrnGameState::K_INVALID_LANDMARK,
                   "mEventDestinationLandmarkIndex != BrnGameState::K_INVALID_LANDMARK");
        return BrnGameState::LandmarkIndex(mEventDestinationLandmarkIndex);
    }

    // @ 0x8241E7D8 -- number of online finish points = total set bits across the 256-bit
    // finish-point bitmask (maOnlineFinishPointsMask @0x7770, 4 doublewords). The X360 loads
    // each 64-bit word and computes its population count with the classic 5-step SWAR sequence,
    // then sums the four per-word counts. Reconstructed store-for-store from the asm masks: the
    // step-1 / step-2 masks the X360 materialises carry redundant high bits set (0xD555.../
    // 0xF333...), but those bits only ever meet the zero bits shifted in, so each step is the
    // canonical popcount mask exactly.
    u32 GuiCache::GetNumOnlineFinishPoints() const
    {
        u32 luFinishPointCount = 0;
        for (s32 liWord = 0; liWord < 4; ++liWord)
        {
            u64 luBits = maOnlineFinishPointsMask[liWord];
            luBits = luBits - ((luBits >> 1) & 0xD555555555555555ULL);
            luBits = ((luBits >> 2) & 0xF333333333333333ULL) + (luBits & 0x3333333333333333ULL);
            luBits = (luBits + (luBits >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
            luFinishPointCount += static_cast<u32>((luBits * 0x0101010101010101ULL) >> 56);
        }
        return luFinishPointCount;
    }

    // @ 0x824EC610 -- the event's finish landmark. Fetches the checkpoint count
    // (GetCheckpointsInEvent), asserts it is non-zero, then returns the per-checkpoint finish-
    // landmark entry indexed by that count: the u16 at maCheckpointLandmarks[checkpoints]
    // (@+0x9F52 + 2*checkpoints), wrapped as a LandmarkIndex.
    BrnGameState::LandmarkIndex GuiCache::GetEventFinishLandmark() const
    {
        const u8 lu8NumCheckpointsInEvent = GetCheckpointsInEvent();
        CGS_ASSERT(lu8NumCheckpointsInEvent > 0, "lu8NumCheckpointsInEvent > 0");
        return BrnGameState::LandmarkIndex(maCheckpointLandmarks[lu8NumCheckpointsInEvent]);
    }

    // @ 0x8241E520 -- index the preset (online) event list (mEvents @0x8040). The X360 front-
    // guards liIndex >= 0, the CgsArray "used before Construct/Clear" sentinel
    // (mEventsCtorSentinel @0x9E54 == -1) and the upper bound (liIndex < GetNumEvents()), then
    // tail-forwards (&maEventsStorage, liIndex) to the array element accessor and returns its
    // Event*.
    const PresetEvent* GuiCache::GetPresetEvent(s32 liIndex) const
    {
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        CGS_ASSERT(mEventsCtorSentinel != -1, "Array used before Construct/Clear was called");
        CGS_ASSERT(static_cast<u32>(liIndex) < static_cast<u32>(mEventsCtorSentinel),
                   "((uint32_t)liIndex) < mEvents.mEvents.GetNumEvents()");
        return GetPresetEventAtIndex(maEventsStorage, static_cast<u32>(liIndex));
    }
}
