#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. StateLoadingHelper tracks how many of
// its watched resources are pending an unload. Increment/Decrement adjust the count
// and then run a debug consistency check: the count must equal the number of resources
// actually in a pending-unload state (LOAD_CANCELLED / UNLOAD_REQUESTED / UNLOADING).
// The consistency loop feeds CGS_ASSERT, which is a no-op in this build (CgsAssert.h),
// matching the project convention for the X360 assert machinery.

namespace BrnGui
{
    namespace
    {
        u32 CountRealPendingUnloads(const StateLoadingHelper::ResourceInfo* lpResources, u32 luCount)
        {
            u32 luRealPending = 0;
            for (u32 i = 0; i < luCount; ++i)
            {
                StateLoadingHelper::EResourceState leState = lpResources[i].meState;
                if (leState == StateLoadingHelper::E_STATE_LOAD_CANCELLED
                    || leState == StateLoadingHelper::E_STATE_UNLOAD_REQUESTED
                    || leState == StateLoadingHelper::E_STATE_UNLOADING)
                {
                    ++luRealPending;
                }
            }
            return luRealPending;
        }
    }

    // @ 0x824EC008
    void StateLoadingHelper::IncrementUnloadPending()
    {
        ++muPendingUnloadCount;
        CGS_ASSERT(muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH,
                   "muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH");

        const u32 luRealPending = CountRealPendingUnloads(maResources, KU_MAX_RESOURCES_TO_WATCH);
        CGS_ASSERT(muPendingUnloadCount == luRealPending,
                   "Pending Unload count does not equal real pending unload count. "
                   "This is skipable but might crash with out of memory");
        (void)luRealPending;
    }

    // @ 0x824EC1E8
    void StateLoadingHelper::DecrementUnloadPending()
    {
        --muPendingUnloadCount;
        CGS_ASSERT(muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH,
                   "muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH");

        const u32 luRealPending = CountRealPendingUnloads(maResources, KU_MAX_RESOURCES_TO_WATCH);
        CGS_ASSERT(muPendingUnloadCount == luRealPending,
                   "Pending Unload count does not equal real pending unload count. "
                   "This is skipable but might crash with out of memory");
        (void)luRealPending;
    }

    // =====================================================================
    //  GuiCache scalar / pointer snapshot accessors.
    //
    //  Each is a thin read of one cached member, guarded by the game's debug
    //  assert (CGS_ASSERT is a no-op in this build, matching the X360 release
    //  assert machinery). Offsets / branch senses are taken straight from the
    //  X360 ARTIST asm; members are accessed BY NAME against the recovered
    //  GuiCache layout in BrnGuiCache.h.
    // =====================================================================

    // @ 0x8240F2C8
    f32 GuiCache::GetCurrentTimeInEvent() const
    {
        CGS_ASSERT(0.0f <= mfEventTime, "0.0f <= mfEventTime");
        return mfEventTime;
    }

    // @ 0x8240F330
    f32 GuiCache::GetTargetTimeInEvent() const
    {
        CGS_ASSERT(0.0f <= mfTargetTime, "0.0f <= mfTargetTime");
        return mfTargetTime;
    }

    // @ 0x8240F450
    s32 GuiCache::GetOpponentsInEvent() const
    {
        CGS_ASSERT(-1 < miOpponentsInEvent, "-1 < miOpponentsInEvent");
        return miOpponentsInEvent;
    }

    // @ 0x82472DB0 -- valid only in the race-style game modes; then asserts the
    // district was actually resolved (!= E_DISTRICT_INVALID, value 18).
    s32 GuiCache::GetEventDestinationDistrict() const
    {
        // asm @0x82472DD8..E08 skips the assert (beq) for meGameModeType in {0,1,10,6,8,5}.
        CGS_ASSERT(
            (meGameModeType == 0) || (meGameModeType == 1) || (meGameModeType == 10)
                || (meGameModeType == 6) || (meGameModeType == 8) || (meGameModeType == 5),
            "race-style game mode required for GetEventDestinationDistrict");
        // BrnWorld::E_DISTRICT_INVALID == 18 (asm immediate).
        CGS_ASSERT(mEventDestinationDistrict != 18,
                   "mEventDestinationDistrict != BrnWorld::E_DISTRICT_INVALID");
        return mEventDestinationDistrict;
    }

    // @ 0x824EC468
    s32 GuiCache::GetCheckpointReached() const
    {
        CGS_ASSERT(0 <= miCheckpointReached, "0 <= miCheckpointReached");
        return miCheckpointReached;
    }

    // @ 0x8240F4B0 -- road-rage only.
    s32 GuiCache::GetCurrentTakedownsInEvent() const
    {
        CGS_ASSERT(meGameModeType == 3, "BrnGameState::GsmIO::E_MODE_ROAD_RAGE == meGameModeType");
        CGS_ASSERT(-1 < miTakedownsCurrent, "-1 < miTakedownsCurrent");
        return miTakedownsCurrent;
    }

    // @ 0x8240F550 -- road-rage only.
    s32 GuiCache::GetTargetTakedownsInEvent() const
    {
        CGS_ASSERT(meGameModeType == 3, "BrnGameState::GsmIO::E_MODE_ROAD_RAGE == meGameModeType");
        CGS_ASSERT(-1 < miTakedownTarget, "-1 < miTakedownTarget");
        return miTakedownTarget;
    }

    // @ 0x8240F5F0
    s32 GuiCache::GetCurrentScoreInEvent() const
    {
        CGS_ASSERT(-1 < miScoreCurrent, "-1 < miScoreCurrent");
        return miScoreCurrent;
    }

    // @ 0x8240F650
    s32 GuiCache::GetTargetScoreInEvent() const
    {
        CGS_ASSERT(-1 < miScoreTarget, "-1 < miScoreTarget");
        return miScoreTarget;
    }

    // @ 0x8240F6B0
    s32 GuiCache::GetCurrentComboInEvent() const
    {
        CGS_ASSERT(-1 < miScoreCombo, "-1 < miScoreCombo");
        return miScoreCombo;
    }

    // @ 0x8240F710
    s32 GuiCache::GetMultiplierInEvent() const
    {
        CGS_ASSERT(-1 < miComboMultiplier, "-1 < miComboMultiplier");
        return miComboMultiplier;
    }

    // @ 0x8240F7F0 -- pursuit mode; the X360 tests the high word of the 8-byte id
    // for kCGSID_NULL (id != 0).
    CgsID GuiCache::GetPursuitCarID() const
    {
        CGS_ASSERT(meGameModeType == 4, "BrnGameState::GsmIO::E_MODE_PURSUIT == meGameModeType");
        CGS_ASSERT(mPursuedCarID != static_cast<CgsID>(0), "kCGSID_NULL != mPursuedCarID");
        return mPursuedCarID;
    }

    // @ 0x824B3060
    CgsID GuiCache::GetShutdownCarID() const
    {
        CGS_ASSERT(mShutdownCarID != static_cast<CgsID>(0), "kCGSID_NULL != mShutdownCarID");
        return mShutdownCarID;
    }

    // @ 0x824B30C0 -- E_UNLOCKTYPE_NONE is 0.
    s32 GuiCache::GetTrophyCarUnlockType() const
    {
        CGS_ASSERT(meTrophyCarUnlockType != 0,
                   "BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_NONE != meTrophyCarUnlockType");
        return meTrophyCarUnlockType;
    }

    // @ 0x8240FC28 -- E_ROAD_PANEL_MODE_COUNT is 2.
    s32 GuiCache::GetActiveRoadRuleScoringMode() const
    {
        CGS_ASSERT(2 != meRoadRuleScoreMode,
                   "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_COUNT != meRoadRuleScoreMode");
        return meRoadRuleScoreMode;
    }

    // @ 0x8240F168
    const FreeburnChallengeManager* GuiCache::GetFreeburnChallengeManager() const
    {
        CGS_ASSERT(mpChallengeManager != nullptr, "mpChallengeManager");
        return mpChallengeManager;
    }

    // @ 0x82472D00
    const HudMessageController* GuiCache::GetHudMessageController() const
    {
        CGS_ASSERT(mpHudMessageController != nullptr, "mpHudMessageController");
        return mpHudMessageController;
    }

    // @ 0x82472D58
    const HudMessageDirector* GuiCache::GetHudMessageDirector() const
    {
        CGS_ASSERT(mpHudMessageDirector != nullptr, "mpHudMessageDirector");
        return mpHudMessageDirector;
    }

    // @ 0x8240F0B0 -- mTimeInfo.mfTimeNow (member at +4); guards the uninitialised sentinel.
    f32 GuiCache::GetTime() const
    {
        CGS_ASSERT(mfTimeNow != -3.4028235e38f, "mfTimeNow!=-FLT_MAX");
        return mfTimeNow;
    }

    // @ 0x8240F110
    WorldDataController* GuiCache::GetWorldDataController() const
    {
        CGS_ASSERT(mpWorldDataController != nullptr, "mpWorldDataController");
        // The X360 stores the controller as a non-const pointer the GUI mutates through;
        // the cache member is declared const, so cast away for the (non-const) accessor.
        return const_cast<WorldDataController*>(mpWorldDataController);
    }

    // @ (far member +40536) -- the active game-mode the GUI reads to pick mode-specific
    // apt key-frames. Plain read, no assert in the X360 path.
    s32 GuiCache::GetCurrentGameModeType() const
    {
        return meGameModeType;
    }

    // Uncalled layout pin. The class carries 8-byte pointers on the LLP64 gate host but
    // 4-byte pointers on the X360 target, so ABSOLUTE byte offsets past a pointer member
    // are not host-invariant (AGENTS.md: absolute offsets only for pointer-free regions).
    // We therefore pin the RELATIVE spacing of the asm-attested scalar block @+40748..+40960,
    // which is entirely pointer-free, so its inter-member deltas equal the X360 deltas on
    // any host. Never called.
    void GuiCache::_AssertLayout()
    {
        // mfTimeNow is the first scalar after the 4-byte header word: pointer-free prefix.
        static_assert(offsetof(GuiCache, mfTimeNow) == 4, "mfTimeNow @ +4 (mTimeInfo.mfTimeNow)");

        // Pointer-free per-event scalar block: deltas match the X360 offsets exactly.
        static_assert(offsetof(GuiCache, mfTargetTime) - offsetof(GuiCache, mfEventTime) == 40752 - 40748, "mfTargetTime - mfEventTime");
        static_assert(offsetof(GuiCache, miOpponentsInEvent) - offsetof(GuiCache, mfEventTime) == 40772 - 40748, "miOpponentsInEvent - mfEventTime");
        static_assert(offsetof(GuiCache, mEventDestinationLandmarkIndex) - offsetof(GuiCache, mfEventTime) == 40780 - 40748, "destLandmark - mfEventTime");
        static_assert(offsetof(GuiCache, mEventDestinationDistrict) - offsetof(GuiCache, mfEventTime) == 40784 - 40748, "destDistrict - mfEventTime");
        static_assert(offsetof(GuiCache, miCheckpointReached) - offsetof(GuiCache, mfEventTime) == 40884 - 40748, "miCheckpointReached - mfEventTime");
        static_assert(offsetof(GuiCache, muCheckpointsInEvent) - offsetof(GuiCache, miCheckpointReached) == 40888 - 40884, "muCheckpointsInEvent - miCheckpointReached");
        static_assert(offsetof(GuiCache, miTakedownsCurrent) - offsetof(GuiCache, miCheckpointReached) == 40892 - 40884, "miTakedownsCurrent - miCheckpointReached");
        static_assert(offsetof(GuiCache, miTakedownTarget) - offsetof(GuiCache, miCheckpointReached) == 40896 - 40884, "miTakedownTarget - miCheckpointReached");
        static_assert(offsetof(GuiCache, miScoreCurrent) - offsetof(GuiCache, miCheckpointReached) == 40900 - 40884, "miScoreCurrent - miCheckpointReached");
        static_assert(offsetof(GuiCache, miScoreTarget) - offsetof(GuiCache, miCheckpointReached) == 40904 - 40884, "miScoreTarget - miCheckpointReached");
        static_assert(offsetof(GuiCache, miScoreCombo) - offsetof(GuiCache, miCheckpointReached) == 40908 - 40884, "miScoreCombo - miCheckpointReached");
        static_assert(offsetof(GuiCache, miComboMultiplier) - offsetof(GuiCache, miCheckpointReached) == 40912 - 40884, "miComboMultiplier - miCheckpointReached");
        static_assert(offsetof(GuiCache, mShutdownCarID) - offsetof(GuiCache, mPursuedCarID) == 40944 - 40928, "mShutdownCarID - mPursuedCarID");
        static_assert(offsetof(GuiCache, meTrophyCarUnlockType) - offsetof(GuiCache, mShutdownCarID) == 40960 - 40944, "meTrophyCarUnlockType - mShutdownCarID");
    }
}
