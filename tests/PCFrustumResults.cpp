#include <cstdio>
#include <thread>
#include <vector>
#include "pc/scene/FrustumResults.h"
static int giChecks = 0, giFailures = 0, giAsserts = 0;
static void Check(bool lbValue, const char* lpcName)
{ ++giChecks; if (!lbValue) { ++giFailures; std::printf("FAIL %s\n", lpcName); } }
namespace CgsDev::Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++giAsserts; return 0; }
void* EndAssert() { return nullptr; }
}
struct Result : CgsModule::Event {
    u32 mQueryId; s32 miNumResults, miNumResultsAttempted;
    u32* GetEntityIds() { return reinterpret_cast<u32*>(this+1); }
    const u32* GetEntityIds() const { return reinterpret_cast<const u32*>(this+1); }
};
struct Ids {
    std::vector<u32> maIds;
    size_t GetLength()const{return maIds.size();}
    u32 GetItem(s32 li)const{return maIds[li];}
};
using LegacyQueue = CgsModule::VariableEventQueue<32768,16>;
static void Fill(void* lpEvent, u32 luId, u32 luCount)
{
    auto* lpResult = static_cast<Result*>(lpEvent);
    lpResult->mQueryId = luId; lpResult->miNumResults = lpResult->miNumResultsAttempted = luCount;
    auto* lpIds = reinterpret_cast<u32*>(lpResult + 1);
    for (u32 lu = 0; lu < luCount; ++lu) lpIds[lu] = luId ^ lu;
}
static s32 First(const LegacyQueue* lpQueue,const CgsModule::Event** lppEvent,s32* lpiBytes)
{
#ifdef PC_FRUSTUM_LEGACY
    return lpQueue->GetFirstEvent(lppEvent,lpiBytes);
#else
    return CgsPC::Scene::FirstFrustumResult(lpQueue,lppEvent,lpiBytes);
#endif
}
static s32 Next(const LegacyQueue* lpQueue,const CgsModule::Event* lpEvent,const CgsModule::Event** lppNext,s32* lpiBytes)
{
#ifdef PC_FRUSTUM_LEGACY
    return lpQueue->GetNextEvent(lpEvent,lppNext,lpiBytes);
#else
    return CgsPC::Scene::NextFrustumResult(lpQueue,lpEvent,lppNext,lpiBytes);
#endif
}
int main()
{
    LegacyQueue lLegacy; lLegacy.Construct();
    auto& lrNative = CgsPC::Scene::BeginFrustumResults(&lLegacy);
    const u32 lauCounts[] = {1600,1600,1000,400,400,1000,1000,400,500,600};
    const u32 lauIds[] = {0xff000000,0xff000002,0xff000003,0xff000004,0xff000005,
        0xff000006,0xff000007,0xff000008,0xff000009,0xff00000a};
    bool lbLegacySpace = true;
    for (u32 lu = 0; lu < 10; ++lu)
    {
        const s32 liBytes = 12 + 4*lauCounts[lu];
        auto* lpEvent = static_cast<Result*>(lrNative.AllocateEventSafe(0,liBytes));
        Check(lpEvent != nullptr,"full native batch allocated");
        if (!lpEvent) return 2;
        Fill(lpEvent,lauIds[lu],lauCounts[lu]);
        if (lbLegacySpace) lbLegacySpace = lLegacy.AddEventSafe(lpEvent,0,liBytes);
    }
    Check(lLegacy.GetLength()==9,"32 KB console queue reproduces the missing far cascade");
    const CgsModule::Event* lpEvent = nullptr; s32 liBytes = 0;
    First(&lLegacy,&lpEvent,&liBytes);
    for (u32 lu = 0; lu < 10; ++lu)
    {
        Check(lpEvent != nullptr,"native cursor retains every query including the far cascade");
        if (!lpEvent) break;
        const auto* lpResult=static_cast<const Result*>(lpEvent);
        Check(lpResult->mQueryId==lauIds[lu] && lpResult->miNumResults==static_cast<s32>(lauCounts[lu])
            && liBytes==12+4*lauCounts[lu],"query id, count and payload size survive extended capture");
        const auto* lpIds=reinterpret_cast<const u32*>(lpResult+1);
        Check(lpIds[lauCounts[lu]-1]==(lauIds[lu]^(lauCounts[lu]-1)),"last entity id survives queue expansion");
        Next(&lLegacy,lpEvent,&lpEvent,&liBytes);
    }
    Check(lpEvent==nullptr,"native cursor terminates after the final cascade");
    bool lbOtherThread=false;
    std::thread lOther([&] {
        LegacyQueue lOtherQueue;lOtherQueue.Construct();
        auto& lrOther=CgsPC::Scene::BeginFrustumResults(&lOtherQueue);
        lbOtherThread=lrOther.GetLength()==0;
    }); lOther.join();
    Check(lbOtherThread && lrNative.GetLength()==10,"another thread cannot clear this frame's results");
    lLegacy.Clear();
    auto& lrEmpty=CgsPC::Scene::BeginFrustumResults(&lLegacy);
    First(&lLegacy,&lpEvent,&liBytes);
    Check(lrEmpty.GetLength()==0 && lpEvent==nullptr,"new frames cannot reuse stale query results");
    for (u32 lu=0;lu<16;++lu)
    {
        auto* lpFull=lrEmpty.AllocateEventSafe(0,12+4*1024);
        Check(lpFull!=nullptr,"maximum coarse-buffer payload fits after widening to entity ids");
        if(lpFull) Fill(lpFull,lu,1024);
    }
    for (const u32 luCount : {8192u,9000u})
    {
        auto& lrLarge=CgsPC::Scene::BeginFrustumResults(&lLegacy);
        const s32 liLargeBytes=12+4*luCount;
        auto* lpLarge=static_cast<Result*>(lrLarge.AllocateEventSafe(0,liLargeBytes));
        Check(lpLarge!=nullptr,"one large job-boundary batch fits the native frame");
        if(!lpLarge)return 2;
        Fill(lpLarge,0xff00000a,luCount);
        Ids lWorldIds,lPropIds,lAllIds;
        const u32 luWorldCount=luCount==8192?4500:3600;
        for(u32 lu=0;lu<luCount;++lu)
        {
            const u32 luId=lpLarge->GetEntityIds()[lu];lAllIds.maIds.push_back(luId);
            (lu<luWorldCount?lWorldIds:lPropIds).maIds.push_back(luId);
        }
        LegacyQueue lWorld,lProps,lOversized;lWorld.Construct();lProps.Construct();lOversized.Construct();
        Check(CgsPC::Scene::ForwardFrustumResult<Result>(&lWorld,lpLarge,0,liLargeBytes,lWorldIds)
            && CgsPC::Scene::ForwardFrustumResult<Result>(&lProps,lpLarge,0,liLargeBytes,lPropIds),
            "oversized combined results route safely to bounded world and prop queues");
        const CgsModule::Event *lpWorld=nullptr,*lpProps=nullptr;
        lWorld.GetFirstEvent(&lpWorld,&liBytes);lProps.GetFirstEvent(&lpProps,&liBytes);
        const auto* lpWorldResult=static_cast<const Result*>(lpWorld);
        const auto* lpPropResult=static_cast<const Result*>(lpProps);
        Check(lpWorldResult && lpWorldResult->mQueryId==0xff00000a
            && lpWorldResult->miNumResults==static_cast<s32>(luWorldCount)
            && lpWorldResult->GetEntityIds()[luWorldCount-1]==lpLarge->GetEntityIds()[luWorldCount-1],
            "world forwarding preserves every selected entity and query identity");
        Check(lpPropResult && lpPropResult->miNumResults==static_cast<s32>(luCount-luWorldCount)
            && lpPropResult->GetEntityIds()[luCount-luWorldCount-1]==lpLarge->GetEntityIds()[luCount-1],
            "prop forwarding preserves every selected entity through the 5400-id owner limit");
        Check(!CgsPC::Scene::ForwardFrustumResult<Result>(&lOversized,lpLarge,0,liLargeBytes,lAllIds)
            && lOversized.GetLength()==0,"an oversized owner list is rejected without an unchecked write");
        auto* lpSmall=static_cast<Result*>(lrLarge.AllocateEventSafe(0,12+4*100));
        Fill(lpSmall,42,100);lWorld.Clear();
        Check(CgsPC::Scene::ForwardFrustumResult<Result>(&lWorld,lpSmall,0,12+4*100,lWorldIds),
            "ordinary results keep the original forwarding path");
        lWorld.GetFirstEvent(&lpWorld,&liBytes);
        Check(liBytes==12+4*100 && !std::memcmp(lpSmall,lpWorld,liBytes),
            "ordinary result records remain byte-for-byte identical");
    }
    LegacyQueue lFallback;lFallback.Construct();Result lOne={};lOne.mQueryId=123;
    lFallback.AddEventSafe(&lOne,0,sizeof(lOne));
    CgsPC::Scene::FirstFrustumResult(&lFallback,&lpEvent,&liBytes);
    Check(lpEvent && static_cast<const Result*>(lpEvent)->mQueryId==123,"unrelated output queues keep their original reader");
    Check(giAsserts==0,"expanded frame requires no asserts or overflow suppression");
    std::printf("PCFrustumResults: %d checks, %d failures\n",giChecks,giFailures);
    return giFailures?1:0;
}
