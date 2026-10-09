#include <bitset>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBasePool.h"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropZoneData.h"
#include "SharedClasses/Physics/Props/BrnPropGraphicsList.h"

static int siAssertions, siChecks, siFailures;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) {
    ++siAssertions;
    // Turn the old body's null-resource assertion into an observable failure
    // before its following 0x24 read would terminate the regression process.
    if (std::strstr(lpcMessage, "main memory resource")) throw 1;
    return 0;
}
void* EndAssert() { return nullptr; }
}
namespace Log {
DebugPrint* gpDebugPrint = nullptr;
StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace PerfMonCpu {
void StartMonitor(s32) {}
void StopMonitor(s32) {}
}
}

namespace BrnWorld {
using BrnPhysics::Props::PropZoneData;
using BrnPhysics::Props::PropGraphicsList;
constexpr u32 KU_MAX_ZONES = 500, KU_MAX_LOADED_ZONES = 9;
constexpr s32 KI_EVENT_QUEUE_EMPTY = -1, KI_EVENT_PROP_INSTANCES_LOADED = 62;
constexpr s32 KI_EVENT_PROP_GRAPHICS_LIST_LOADED = 63;
constexpr s32 KI_PRP_INST_PREFIX_LEN = 9, KI_PRP_GL_PREFIX_LEN = 8;
constexpr s32 KI_PROP_INSTANCES_EVENT_ID = 0, KI_PROP_INSTANCES_POOL_ID = 3;

struct FixtureBits {
    std::bitset<500> mBits;
    bool IsBitSet(u32 luZone) const { return mBits.test(luZone); }
    void SetBit(u32 luZone) { mBits.set(luZone); }
    void UnSetBit(u32 luZone) { mBits.reset(luZone); }
};
struct FixtureReceiver {
    BrnResource::GameDataIO::GameDataAssetEvent maEvents[8] = {};
    s32 maiTypes[8] = {}, miCount = 0;
    void Add(u32 luZone, CgsResource::ResourceHandle lHandle) {
        auto& lrEvent = maEvents[miCount];
        char lacId[13];
        std::snprintf(lacId, sizeof(lacId), "PRP_INST_%u", luZone);
        lrEvent.mId = CgsIDCompress(lacId);
        lrEvent.mHandle = lHandle;
        maiTypes[miCount++] = KI_EVENT_PROP_INSTANCES_LOADED;
    }
    s32 At(s32 liIndex, const CgsModule::Event** lppEvent, s32* lpiSize) const {
        *lppEvent = liIndex < miCount ? &maEvents[liIndex] : nullptr;
        *lpiSize = sizeof(maEvents[0]);
        return liIndex < miCount ? maiTypes[liIndex] : KI_EVENT_QUEUE_EMPTY;
    }
    s32 GetFirstEvent(const CgsModule::Event** lppEvent, s32* lpiSize) const {
        return At(0, lppEvent, lpiSize);
    }
    s32 GetNextEvent(const CgsModule::Event* lpEvent,
                     const CgsModule::Event** lppEvent, s32* lpiSize) const {
        const auto* lpAsset = static_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEvent);
        return At(static_cast<s32>(lpAsset - maEvents) + 1, lppEvent, lpiSize);
    }
    void Clear() { miCount = 0; }
};
struct FixtureZoneManager {
    std::bitset<500> mLoaded;
    bool IsZoneLoaded(u16 luZone) const { return mLoaded.test(luZone); }
};
namespace PropEntityIO {
struct OutputBuffer_PreScene {
    s32 miRequests = 0, miLastZone = -1;
    template<class Queue> void GetPropInstances(Queue*, s32 liEvent, s32 liZone, s32 liPool) {
        if (liEvent != 0 || liPool != 3) throw 2;
        ++miRequests; miLastZone = liZone;
    }
};
}
struct PropEntityModule {
    FixtureReceiver mReceiverQueue;
    FixtureBits mabWaitingForInstances, mabWaitingForGraphics, mabLoadedWorldGraphics;
    FixtureZoneManager mZoneManager;
    CgsResource::ResourcePtr<PropGraphicsList> mapGraphicsLists[500];
    bool mbResourceSystemStalled = false;
    u32 muNumberOfLoadedZones = 0;
    s32 miLoadingPM = -1, miLoads = 0;
    void LoadZone(const PropZoneData* lpZone, PropEntityIO::OutputBuffer_PreScene*) {
        ++miLoads; ++muNumberOfLoadedZones;
        mZoneManager.mLoaded.set(lpZone->GetZoneId());
    }
    void Drain(PropEntityIO::OutputBuffer_PreScene* lpOutput);
};
PropEntityIO::OutputBuffer_PreScene* GetGameDataRequestInterface(PropEntityIO::OutputBuffer_PreScene* lpOutput) {
    return lpOutput;
}
#include "pc_prop_streaming_reply.inc"
}

static void Check(bool lbOK, const char* lpcName) {
    ++siChecks;
    if (!lbOK) { ++siFailures; std::printf("FAIL %s\n", lpcName); }
}
struct ZoneResource {
    BrnPhysics::Props::PropZoneData mZone = {};
    CgsResource::Entry mEntry = {};
    explicit ZoneResource(u16 luZone) {
        mZone.muZoneId = luZone; // test compiler exposes private fixture members
        mEntry.mResource.m_baseResources[0] = &mZone;
        auto* lpHead = reinterpret_cast<CgsResource::BaseResourcePtr*>(&mEntry.mResource);
        mEntry.mpResourceNext = mEntry.mpResourcePrev = mEntry.mpResourceThis = lpHead;
        mEntry.muResourceThreadId = reinterpret_cast<uintptr_t>(&mEntry);
    }
    CgsResource::ResourceHandle Handle() { return {&mEntry.mResource, &mEntry}; }
};
static bool Drain(BrnWorld::PropEntityModule& lrModule, BrnWorld::PropEntityIO::OutputBuffer_PreScene& lrOutput) {
    try { lrModule.Drain(&lrOutput); return true; }
    catch (int) { return false; }
}
int main() {
    using namespace BrnWorld;
    PropEntityIO::OutputBuffer_PreScene lOutput;
    {
        PropEntityModule lModule;
        lModule.mabLoadedWorldGraphics.SetBit(235);
        lModule.mabWaitingForInstances.SetBit(235);
        lModule.mReceiverQueue.Add(235, CgsResource::NULLResourceHandle);
        Check(Drain(lModule, lOutput), "empty acquire does not dereference a null prop resource");
        Check(lModule.miLoads == 0 && lModule.muNumberOfLoadedZones == 0,
              "empty acquire publishes no props and consumes no zone slot");
        Check(!lModule.mabWaitingForInstances.IsBitSet(235), "empty acquire releases the pending bit");
        Check(lModule.mReceiverQueue.miCount == 0, "empty completion is drained");
        Check(RequestPropInstancesForZone(lModule, 235, &lOutput)
              && lOutput.miRequests == 1 && lOutput.miLastZone == 235,
              "existing streaming producer retries the missing zone");
        ZoneResource lResource(235);
        lModule.mReceiverQueue.Clear();
        lModule.mReceiverQueue.Add(235, lResource.Handle());
        Check(Drain(lModule, lOutput) && lModule.miLoads == 1
              && lModule.mZoneManager.IsZoneLoaded(235), "a subsequent resident reply loads the zone");
        Check(!lModule.mabWaitingForInstances.IsBitSet(235), "successful reply clears its pending bit");
    }
    {
        PropEntityModule lModule;
        lModule.mabWaitingForInstances.SetBit(235);
        lModule.mReceiverQueue.Add(235, CgsResource::NULLResourceHandle);
        Drain(lModule, lOutput);
        Check(!RequestPropInstancesForZone(lModule, 235, &lOutput),
              "a zone whose world graphics unloaded is not requested again");
    }
    {
        PropEntityModule lModule;
        ZoneResource lEmpty(235), lReady(236);
        lEmpty.mEntry.mResource.m_baseResources[0] = nullptr;
        for (u32 luZone : {235u, 236u}) lModule.mabWaitingForInstances.SetBit(luZone);
        lModule.mReceiverQueue.Add(235, lEmpty.Handle());
        lModule.mReceiverQueue.Add(236, lReady.Handle());
        Check(Drain(lModule, lOutput), "an owner whose main memory was released is also safe");
        Check(lModule.miLoads == 1 && lModule.mZoneManager.IsZoneLoaded(236),
              "empty reply does not discard the following valid completion");
        Check(!lModule.mabWaitingForInstances.IsBitSet(235)
              && !lModule.mabWaitingForInstances.IsBitSet(236), "mixed completions clear both pending bits");
    }
    for (bool lbStalled : {false, true}) {
        PropEntityModule lModule;
        lModule.mbResourceSystemStalled = lbStalled;
        lModule.muNumberOfLoadedZones = lbStalled ? 0 : 9;
        lModule.mabWaitingForInstances.SetBit(235);
        lModule.mReceiverQueue.Add(235, CgsResource::NULLResourceHandle);
        Check(Drain(lModule, lOutput) && lModule.miLoads == 0
              && !lModule.mabWaitingForInstances.IsBitSet(235),
              lbStalled ? "stalled-resource gate remains intact" : "full-zone-pool gate remains intact");
    }
    Check(siAssertions == 0, "all reply scenarios complete without assertions");
    std::printf("PCPropStreamingReply: %d checks, %d failures\n", siChecks, siFailures);
    return siFailures ? 1 : 0;
}
