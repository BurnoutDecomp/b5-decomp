#pragma once

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

namespace CgsSceneManager::SceneManagerIO { struct OutCoarseQueryResult; }

namespace CgsPC::Scene
{
    // FLAG PC-platform leaf: extended cube queries can exceed the console's
    // 32 KB event queue after u16 coarse indices become u32 entity ids. Keep
    // every batch from the 16,384-index coarse buffer without changing IO layouts.
    struct FrustumResultsFrame
    {
        CgsModule::VariableEventQueue<131072, 16> mResults;
        const void* mpSource = nullptr;
        FrustumResultsFrame() { mResults.Construct(); }
    };

    inline FrustumResultsFrame& GetFrustumResultsFrame()
    {
        static thread_local FrustumResultsFrame sFrame;
        return sFrame;
    }

    template<class Queue>
    auto& BeginFrustumResults(Queue* lpSource)
    {
        auto& lrFrame = GetFrustumResultsFrame();
        lrFrame.mpSource = lpSource;
        lrFrame.mResults.Clear();
        return lrFrame.mResults;
    }

    template<class Queue>
    s32 FirstFrustumResult(const Queue* lpSource, const CgsModule::Event** lppEvent, s32* lpiSize)
    {
        const auto& lrFrame = GetFrustumResultsFrame();
        return lrFrame.mpSource == lpSource ? lrFrame.mResults.GetFirstEvent(lppEvent, lpiSize)
            : lpSource->GetFirstEvent(lppEvent, lpiSize);
    }

    template<class Queue>
    s32 NextFrustumResult(const Queue* lpSource, const CgsModule::Event* lpEvent,
        const CgsModule::Event** lppEvent, s32* lpiSize)
    {
        const auto& lrFrame = GetFrustumResultsFrame();
        return lrFrame.mpSource == lpSource ? lrFrame.mResults.GetNextEvent(lpEvent, lppEvent, lpiSize)
            : lpSource->GetNextEvent(lpEvent, lppEvent, lpiSize);
    }

    template<class Result = CgsSceneManager::SceneManagerIO::OutCoarseQueryResult, class Queue, class Ids>
    bool ForwardFrustumResult(Queue* lpDestination, const CgsModule::Event* lpSource,
        s32 liType, s32 liSize, const Ids& lrModuleIds)
    {
        if (!lpSource || liSize < static_cast<s32>(sizeof(Result))) return false;
        // Preserve ordinary records byte-for-byte. A large combined result can
        // exceed 32 KB even though each owner's bounded list fits its own queue.
        if (lpDestination->AddEventSafe(lpSource, liType, liSize)) return true;
        const s32 liCount = static_cast<s32>(lrModuleIds.GetLength());
        if (liCount < 0) return false;
        const auto* lpSourceResult = static_cast<const Result*>(lpSource);
        if (liCount > lpSourceResult->miNumResults) return false;
        const s32 liFilteredSize = static_cast<s32>(sizeof(Result)
            + static_cast<size_t>(liCount) * sizeof(*lpSourceResult->GetEntityIds()));
        auto* lpResult = static_cast<Result*>(lpDestination->AllocateEventSafe(liType, liFilteredSize));
        if (!lpResult) return false;
        lpResult->mQueryId = lpSourceResult->mQueryId;
        lpResult->miNumResults = lpResult->miNumResultsAttempted = liCount;
        auto* lpIds = lpResult->GetEntityIds();
        for (s32 li = 0; li < liCount; ++li) lpIds[li] = lrModuleIds.GetItem(li);
        return true;
    }
}
