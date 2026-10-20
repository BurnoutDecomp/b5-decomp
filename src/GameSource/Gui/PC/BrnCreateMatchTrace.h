#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdarg>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// FLAG PC diagnostic: optional native Create Match receipts, absent from ARTIST.
// No game state/layout is changed. Requires its own flag AND an isolated numeric slot.
namespace BrnGui
{
namespace CreateMatchTracePC
{
    inline bool Enabled()
    {
        static const bool sbEnabled = []() {
#if defined(_WIN32)
            const char* lpcFlag = std::getenv("BRN_CREATE_MATCH_TRACE");
            const char* lpcSlot = std::getenv("BRN_HARNESS_SLOT");
            if (lpcFlag == 0 || std::strcmp(lpcFlag, "1") != 0 ||
                lpcSlot == 0 || lpcSlot[0] < '1' || lpcSlot[0] > '9')
                return false;
            u32 luLength = 0;
            for (const char* lpc = lpcSlot; *lpc != '\0'; ++lpc)
            {
                if (*lpc < '0' || *lpc > '9' || ++luLength > 6)
                    return false;
            }
            return luLength != 0;
#else
            return false;
#endif
        }();
        return sbEnabled;
    }

    struct TraceState
    {
        const void* mpObservedHelpBar;
        s32 miOptionsSubstate;
        u32 muResources;
        u32 muComponents;
        u32 muReady;
        u32 muRounds;
        u32 muHelpTicks;
        u32 muInitialHelpTicks;
        u32 muPendingRound;
        bool mbHaveHelpTime;
        f32 mfLastHelpTime;
    };

    inline TraceState& State()
    {
        // One diagnostic state per GUI thread, shared across these TUs by C++ inline
        // local-static rules. Other threads cannot inherit this stack-restored scope.
        static thread_local TraceState sState = {};
        return sState;
    }

    inline void Emit(const char* lpcFormat, ...)
    {
        if (!Enabled() || CgsDev::Log::gpDebugPrint == 0)
            return;
        char lacLine[384];
        va_list lArgs;
        va_start(lArgs, lpcFormat);
        std::vsnprintf(lacLine, sizeof(lacLine), lpcFormat, lArgs);
        va_end(lArgs);
        lacLine[sizeof(lacLine) - 1] = '\0';
        *CgsDev::Log::gpDebugPrint << lacLine;
    }

    inline void ResourcesReady(s32 liResourceCount)
    {
        if (!Enabled() || State().muResources >= 4)
            return;
        ++State().muResources;
        Emit("[create-match-trace] options resources-ready count=%d\n", liResourceCount);
    }

    inline void ComponentsReady()
    {
        if (!Enabled() || State().muComponents >= 4)
            return;
        ++State().muComponents;
        Emit("[create-match-trace] options components-ready\n");
    }

    inline void OptionsReady(s32 liSubstate, s32 liWindow, s32 liRows, s32 liHighlighted)
    {
        if (!Enabled())
            return;
        State().mbHaveHelpTime = false;
        State().muInitialHelpTicks = 0;
        State().muPendingRound = 0;
        if (State().muReady >= 4)
            return;
        ++State().muReady;
        Emit("[create-match-trace] options ready substate=%d window=%d rows=%d highlighted=%d\n",
            liSubstate, liWindow, liRows, liHighlighted);
    }

    struct RoundValue
    {
        bool mbValid;
        s32 miRow;
        u64 muRowId;
        s32 miChoice;
        u64 muChoiceId;
    };

    inline void RoundsAccepted(s32 liAction, const RoundValue& lBefore, const RoundValue& lAfter)
    {
        if (!Enabled() || (!lBefore.mbValid && !lAfter.mbValid) || State().muRounds >= 16)
            return;
        ++State().muRounds;
        State().muPendingRound = State().muRounds;
        Emit("[create-match-trace] rounds seq=%u accepted=1 action=%d before-valid=%d after-valid=%d row=%d row-id=%llu before-index=%d before-id=%llu after-index=%d after-id=%llu\n",
            State().muRounds, liAction, lBefore.mbValid ? 1 : 0, lAfter.mbValid ? 1 : 0,
            lAfter.miRow, static_cast<unsigned long long>(lAfter.muRowId),
            lBefore.miChoice, static_cast<unsigned long long>(lBefore.muChoiceId),
            lAfter.miChoice, static_cast<unsigned long long>(lAfter.muChoiceId));
    }

    class HelpBarScope
    {
    public:
        HelpBarScope(const void* lpBar, s32 liSubstate)
            : mpPrevious(0), miPreviousSubstate(0), mbEnabled(Enabled())
        {
            if (mbEnabled)
            {
                mpPrevious = State().mpObservedHelpBar;
                miPreviousSubstate = State().miOptionsSubstate;
                State().mpObservedHelpBar = lpBar;
                State().miOptionsSubstate = liSubstate;
            }
        }
        ~HelpBarScope()
        {
            if (mbEnabled)
            {
                State().mpObservedHelpBar = mpPrevious;
                State().miOptionsSubstate = miPreviousSubstate;
            }
        }
        HelpBarScope(const HelpBarScope&) = delete;
        HelpBarScope& operator=(const HelpBarScope&) = delete;
    private:
        const void* mpPrevious;
        s32 miPreviousSubstate;
        bool mbEnabled;
    };

    inline bool ObservingHelpBar(const void* lpBar)
    {
        return Enabled() && State().mpObservedHelpBar == lpBar;
    }

    inline void HelpBarCompleted(f32 lTime, f32 lStoredTime, s32 liUsed, s32 liCompleted,
                                 bool lbInfoValid, bool lbWidthsReady)
    {
        if (!Enabled() || State().muHelpTicks >= 8)
            return;
        const u32 luRound = State().muPendingRound;
        if (luRound == 0 && State().muInitialHelpTicks >= 3)
            return;
        // Show the first pending layout once; retain the remaining initial receipts
        // for actual completed layout, even if Apt needs several seconds to provide widths.
        if (luRound == 0 && State().muInitialHelpTicks != 0 && (!lbInfoValid || !lbWidthsReady))
            return;
        if (luRound == 0 && State().mbHaveHelpTime && lTime < State().mfLastHelpTime + 1.0f)
            return;
        State().muPendingRound = 0;
        if (luRound == 0)
            ++State().muInitialHelpTicks;
        State().mbHaveHelpTime = true;
        State().mfLastHelpTime = lTime;
        const u32 luTick = State().muHelpTicks++;
        Emit("[create-match-trace] helpbar completed tick=%u substate=%d round-seq=%u time=%.6f stored=%.6f used=%d completed=%d info=%d widths=%d\n",
            luTick, State().miOptionsSubstate, luRound, static_cast<double>(lTime), static_cast<double>(lStoredTime),
            liUsed, liCompleted, lbInfoValid ? 1 : 0, lbWidthsReady ? 1 : 0);
    }
    // FLAG PC diagnostic: one process-wide receipt per original Continue context.
    // These functions only observe values passed after the existing positive branch.
    inline void VehicleReady(s32 liState, s32 liType, bool lbChange, f32 lfCarousel,
                              bool lbVoiceOver, u64 luCurrent, u64 luDropIn)
    {
        if (!Enabled())
            return;
        static std::atomic_flag sOnce = ATOMIC_FLAG_INIT;
        if (sOnce.test_and_set(std::memory_order_relaxed))
            return;
        Emit("[create-match-trace] carselect vehicle-ready state=%d type=%d change=%d carousel=%.6f vo=%d current=%llu dropin=%llu\n",
            liState, liType, lbChange ? 1 : 0, static_cast<double>(lfCarousel), lbVoiceOver ? 1 : 0,
            static_cast<unsigned long long>(luCurrent), static_cast<unsigned long long>(luDropIn));
    }

    inline void LiveryReady(s32 liState, s32 liType, bool lbExiting, bool lbChange,
                             u64 luCurrent, u64 luDropIn)
    {
        if (!Enabled())
            return;
        static std::atomic_flag sOnce = ATOMIC_FLAG_INIT;
        if (sOnce.test_and_set(std::memory_order_relaxed))
            return;
        Emit("[create-match-trace] carselect livery-ready state=%d type=%d exiting=%d change=%d current=%llu dropin=%llu\n",
            liState, liType, lbExiting ? 1 : 0, lbChange ? 1 : 0,
            static_cast<unsigned long long>(luCurrent), static_cast<unsigned long long>(luDropIn));
    }

}
}
