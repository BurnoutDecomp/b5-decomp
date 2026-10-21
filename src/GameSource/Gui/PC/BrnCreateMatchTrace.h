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
        u32 muRouteEpoch;
        u32 muRouteReceipts;
        bool mbHaveRoute;
        s32 miRouteRound;
        s32 miRouteRounds;
        u64 muRouteHighlight;
        bool mbRouteInfo;
        f64 mdRouteDistance;
        u32 muSummaryReadyReceipts;
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

    // FLAG PC diagnostic: bounded submission receipts. Only explicit initialized
    // scalar fields and ACTIVE event/landmark elements enter the fingerprint.
    enum ParamsSite
    {
        E_PARAMS_SUMMARY_READY, E_PARAMS_SUMMARY_SUBMIT, E_PARAMS_CONSUMED,
        E_PARAMS_SERVER_READBACK, E_PARAMS_GUI_OUTPUT, E_PARAMS_MODIFY_RESULT,
        E_PARAMS_SITE_COUNT
    };

    struct SubmissionState
    {
        std::atomic<u32> muEpoch;
        std::atomic<u32> muSequence;
        std::atomic<u32> maCounts[E_PARAMS_SITE_COUNT];
        SubmissionState() : muEpoch(0), muSequence(0)
        {
            for (s32 li = 0; li < E_PARAMS_SITE_COUNT; ++li)
                maCounts[li].store(0, std::memory_order_relaxed);
        }
    };

    inline SubmissionState& Submission()
    {
        static SubmissionState sSubmission;
        return sSubmission;
    }

    inline u32 SubmissionReceipt(ParamsSite leSite)
    {
        if (!Enabled() || Submission().maCounts[leSite].fetch_add(1, std::memory_order_relaxed) >= 16)
            return 0;
        return Submission().muSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    inline void RouteEntered()
    {
        if (!Enabled()) return;
        ++State().muRouteEpoch;
        State().muRouteReceipts = 0;
        State().mbHaveRoute = false;
    }

    inline void RouteReady(s32 liRound, s32 liRounds, u64 luHighlighted,
                           bool lbRouteInfo, f64 ldDistance)
    {
        if (!Enabled() || State().muRouteReceipts >= 16) return;
        if (State().mbHaveRoute && State().miRouteRound == liRound &&
            State().miRouteRounds == liRounds && State().muRouteHighlight == luHighlighted &&
            State().mbRouteInfo == lbRouteInfo && State().mdRouteDistance == ldDistance) return;
        State().mbHaveRoute = true;
        State().miRouteRound = liRound;
        State().miRouteRounds = liRounds;
        State().muRouteHighlight = luHighlighted;
        State().mbRouteInfo = lbRouteInfo;
        State().mdRouteDistance = ldDistance;
        ++State().muRouteReceipts;
        Emit("[create-match-trace] route ready epoch=%u substate=2 round=%d rounds=%d highlighted-id=%llu route-info=%d distance=%.17g\n",
            State().muRouteEpoch, liRound, liRounds, static_cast<unsigned long long>(luHighlighted),
            lbRouteInfo ? 1 : 0, static_cast<double>(ldDistance));
    }

    inline void SummaryEntered()
    {
        if (!Enabled()) return;
        Submission().muEpoch.fetch_add(1, std::memory_order_relaxed);
        State().muSummaryReadyReceipts = 0;
        for (s32 li = 0; li < E_PARAMS_SITE_COUNT; ++li)
            Submission().maCounts[li].store(0, std::memory_order_relaxed);
    }

    inline void MixWord(u64& lrHash, u32 luValue)
    {
        lrHash ^= luValue;
        lrHash *= 1099511628211ull;
    }

    template<class Params> inline u64 AppliedFingerprint(const Params& lParams, bool& lbValid)
    {
        u64 luHash = 14695981039346656037ull;
        // ModifyGame does not apply previous-mode/ranked; print those as metadata.
        const s32 laiWords[] = {lParams.meGameMode, lParams.meSecurity, lParams.meBoostType,
            lParams.meVehicleChoice, lParams.miTimeLimit, lParams.miNumRounds,
            lParams.miVehicleClass, lParams.miNumRunnerCrashes,
            lParams.mbInfiniteBoost ? 1 : 0, lParams.mbTrafficOn ? 1 : 0,
            lParams.mbTrafficCheckingOn ? 1 : 0};
        for (s32 liWord : laiWords) MixWord(luHash, static_cast<u32>(liWord));
        lbValid = lParams.miNumRounds >= 0 && lParams.miNumRounds <= Params::KI_NUM_EVENTS;
        if (!lbValid) return luHash;
        for (s32 liRound = 0; liRound < lParams.miNumRounds; ++liRound)
        {
            const auto& lEvent = lParams.maEvents[liRound];
            const s32 liCount = lEvent.GetNumLandmarks();
            MixWord(luHash, static_cast<u32>(lEvent.GetEventID()));
            MixWord(luHash, lEvent.GetTrafficLightTriggerId());
            MixWord(luHash, static_cast<u32>(liCount));
            // Event owns sixteen LandmarkIndex elements; inactive slots are not read.
            if (liCount < 0 || liCount > 16) { lbValid = false; continue; }
            for (s32 li = 0; li < liCount; ++li)
                MixWord(luHash, static_cast<u32>(static_cast<s32>(lEvent.GetLandmark(li))));
        }
        return luHash;
    }

    template<class Params> inline void Parameters(ParamsSite leSite, const Params& lParams)
    {
        const u32 luSequence = SubmissionReceipt(leSite);
        if (luSequence == 0) return;
        static const char* const lapcSites[] = {"summary-ready", "summary-submit257", "consume257",
            "server-readback15", "gui-output257", "modify-result"};
        bool lbValid = false;
        const u64 luFingerprint = AppliedFingerprint(lParams, lbValid);
        Emit("[create-match-trace] params stage=%s seq=%u epoch=%u bounds-valid=%d mode=%d previous=%d security=%d boost=%d vehicle-choice=%d time=%d rounds=%d class=%d crashes=%d ranked=%d infinite=%d traffic=%d checking=%d applied-fp=%016llx\n",
            lapcSites[leSite], luSequence, Submission().muEpoch.load(std::memory_order_relaxed), lbValid ? 1 : 0,
            lParams.meGameMode, lParams.mePreviousGameMode, lParams.meSecurity, lParams.meBoostType,
            lParams.meVehicleChoice, lParams.miTimeLimit, lParams.miNumRounds, lParams.miVehicleClass,
            lParams.miNumRunnerCrashes, lParams.mbRanked ? 1 : 0, lParams.mbInfiniteBoost ? 1 : 0,
            lParams.mbTrafficOn ? 1 : 0, lParams.mbTrafficCheckingOn ? 1 : 0,
            static_cast<unsigned long long>(luFingerprint));
        if (!lbValid) return;
        for (s32 liRound = 0; liRound < lParams.miNumRounds; ++liRound)
        {
            const auto& lEvent = lParams.maEvents[liRound];
            char lacLandmarks[128] = "-";
            s32 liUsed = 0;
            for (s32 li = 0; li < lEvent.GetNumLandmarks(); ++li)
            {
                const s32 liWritten = std::snprintf(lacLandmarks + liUsed, sizeof(lacLandmarks) - liUsed,
                    "%s%d", li == 0 ? "" : ",", static_cast<s32>(lEvent.GetLandmark(li)));
                if (liWritten < 0 || liWritten >= static_cast<s32>(sizeof(lacLandmarks)) - liUsed) break;
                liUsed += liWritten;
            }
            Emit("[create-match-trace] params-round stage=%s seq=%u round=%d event=%d light=%u count=%d landmarks=%s\n",
                lapcSites[leSite], luSequence, liRound, lEvent.GetEventID(), lEvent.GetTrafficLightTriggerId(),
                lEvent.GetNumLandmarks(), lacLandmarks);
        }
    }

    template<class Params> inline void SummaryReady(s32 liRound, s32 liHighlighted, const Params& lParams)
    {
        if (!Enabled() || State().muSummaryReadyReceipts >= 16) return;
        ++State().muSummaryReadyReceipts;
        Emit("[create-match-trace] summary ready epoch=%u substate=2 round=%d highlighted=%d selected-done=%d\n",
            Submission().muEpoch.load(std::memory_order_relaxed), liRound, liHighlighted,
            liHighlighted == 0 ? 1 : 0);
        Parameters(E_PARAMS_SUMMARY_READY, lParams);
    }

    inline void ModifyResult(s32 liStatus)
    {
        const u32 luSequence = SubmissionReceipt(E_PARAMS_MODIFY_RESULT);
        if (luSequence == 0) return;
        Emit("[create-match-trace] modify-result seq=%u epoch=%u status=%d success=%d\n", luSequence,
            Submission().muEpoch.load(std::memory_order_relaxed), liStatus, liStatus == 2 ? 1 : 0);
    }

}
}
