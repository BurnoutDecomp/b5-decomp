#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // SetUpAllEventStartsInterface (complete type for GetNumEventStarts)

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

    // The GuiCache layout pin (GuiCache::_AssertLayout) now lives ONCE, inline in
    // BrnGuiCache.h, as the comprehensive GC_FAR block that pins every asm-attested far member
    // relative to mv4WorldCameraPosition (plus the pointer-invariant prefix). The former,
    // narrower copy that lived here was a redefinition of that inline (C2084) and pinned a
    // strict subset of the same offsets, so it has been removed -- the header version is
    // canonical and supersedes it.

    // @0x824F8830 -- GuiCache::GetNumEventStarts (DWARF BrnGuiCache.h:801). Pure tail-forwarder:
    // X360 `addi r3, r3, 0x5690` (this + 0x5690) then `b sub_824F7688`, i.e. hands
    // &mSetUpAllEventStartsInterface (the SetUpAllEventStartsInterface embedded at GuiCache+0x5690)
    // to that interface's GetNumEventStarts() @0x824F7688 and tail-returns its result. No local assert.
    u32 GuiCache::GetNumEventStarts() const
    {
        const u8* lpBase = reinterpret_cast<const u8*>(this);
        const BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface* lpEventStarts =
            reinterpret_cast<const BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface*>(
                lpBase + 0x5690);
        return lpEventStarts->GetNumEventStarts();
    }
}
