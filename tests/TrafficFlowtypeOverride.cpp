// Proposed body-level regression for TrafficEntityModule::AddVehiclesToTargetList.
// The runner extracts both production methods into this fixture. Only the sink's test-only
// boundary capture/range guard is inserted; for catalogue-valid bytes its production loop runs.

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficFlowType.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

static u32 gFlowtypeChecks = 0;
static u32 gFlowtypeFailures = 0;
static u32 gFlowtypeAssertCount = 0;
static std::string gFlowtypeLastAssert;

struct FlowtypeAssertTrap : public std::runtime_error
{
    explicit FlowtypeAssertTrap(const char* lpcExpression)
        : std::runtime_error(lpcExpression ? lpcExpression : "")
    {
    }
};

namespace CgsDev
{
namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char* lpcExpression, const char*, int)
    {
        ++gFlowtypeAssertCount;
        gFlowtypeLastAssert = lpcExpression ? lpcExpression : "";
        throw FlowtypeAssertTrap(gFlowtypeLastAssert.c_str());
    }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnTraffic
{
    static u32 gFlowtypeSinkCalls = 0;
    static std::vector<u8> gFlowtypeCapturedAssetIds;
    static bool gbFlowtypeCaptureOnly = false;

    static void Check(bool lbCondition, const char* lpcName)
    {
        ++gFlowtypeChecks;
        if (!lbCondition)
        {
            ++gFlowtypeFailures;
            std::printf("FAIL %s\n", lpcName);
        }
    }

    static bool HasAssetIds(const std::vector<u8>& lrActual,
                            std::initializer_list<u8> lrExpected)
    {
        return lrActual == std::vector<u8>(lrExpected);
    }

    // The real TrafficCarStreamer declares six BaseStreamer callbacks. This derived fixture
    // supplies inert callbacks for vtable completeness; AddVehiclesToTargetList is inherited
    // from the real class and its production body is the method extracted by the runner.
    struct RecordingTrafficCarStreamer : public TrafficCarStreamer
    {
        s32 QueryLoad(const BrnWorld::StreamerTargetEntry*, s32) override { return 0; }
        s32 QueryUnload(const BrnWorld::StreamerTargetEntry*, s32) override { return 0; }
        void OnLoadBegin(s32) override {}
        void OnUnloadBegin(s32) override {}
        void OnLoadComplete(const BrnResource::GameDataIO::GameDataAssetEvent*, s32) override {}
        void OnUnloadComplete(const BrnResource::GameDataIO::UnloadGameDataResponse*, s32) override {}
    };

    static void ClearSink(RecordingTrafficCarStreamer& lrStreamer)
    {
        lrStreamer.muNumAssets = KU_MAX_VEHICLE_ASSETS;
        std::memset(lrStreamer.maxLoadFlags, 0, sizeof(lrStreamer.maxLoadFlags));
        gFlowtypeSinkCalls = 0;
        gFlowtypeCapturedAssetIds.clear();
        gbFlowtypeCaptureOnly = false;
        gFlowtypeAssertCount = 0;
        gFlowtypeLastAssert.clear();
    }

    // Exact source-member types for the fields the production body reads. The data pointer and
    // all flow/hull records below use the production declarations; only GetHull and the missing
    // diagnostic logger are fixture adapters for the existing hull fallback.
    struct FlowtypeFixture
    {
        typedef TrafficEntityModule Module;

        decltype(Module::mpData) mpData{};
        decltype(Module::meLocalPlayerIndex) meLocalPlayerIndex =
            static_cast<decltype(Module::meLocalPlayerIndex)>(-1);
        decltype(Module::miDEBUGFlowtypeOverride) miDEBUGFlowtypeOverride = -1;
        decltype(Module::maaRaceCarHulls) maaRaceCarHulls{};
        RecordingTrafficCarStreamer mStreamer{};
        Hull mHull{};
        u32 muExpectedHullId = 2;
        mutable u32 muHullLookups = 0;

        FlowtypeFixture() { ClearSink(mStreamer); }

        const Hull* GetHull(u32 luIndex) const
        {
            ++muHullLookups;
            return luIndex == muExpectedHullId ? &mHull : nullptr;
        }

        void AddVehiclesToTargetList();
    };

    struct FlowInput
    {
        TrafficData mData{};
        FlowType mFlow{};
        FlowType* mFlowTable[1]{};
        u16 mauTypeIds[32]{};
        VehicleTypeData maVehicleTypes[32]{};

        FlowInput()
        {
            mData.muDataVersion = TrafficData::KU_DATA_VERSION;
            mData.muNumFlowTypes = 1;
            mData.muNumVehicleAssets = static_cast<u8>(KU_MAX_VEHICLE_ASSETS);
            mData.mpapFlowTypes = mFlowTable;
            mData.mpaVehicleTypes = maVehicleTypes;
            mFlowTable[0] = &mFlow;
            mFlow.mpauVehicleTypeIds = mauTypeIds;
            mFlow.mpauCumulativeProb = nullptr;
        }

        void SetAssetIds(const u8* lpAssetIds, u32 luCount)
        {
            mFlow.muNumVehicleTypes = static_cast<u8>(luCount);
            mData.muNumVehicleTypes = static_cast<u16>(luCount);
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                mauTypeIds[luIndex] = static_cast<u16>(luIndex);
                maVehicleTypes[luIndex].muAssetId = lpAssetIds[luIndex];
            }
        }

        void Bind(FlowtypeFixture& lrModule)
        {
            // The harness uses the real ResourcePtr and real TrafficData object. The existing
            // fixture compile flags expose this protected pointer field for direct test setup.
            lrModule.mpData.mpResourceMemory = &mData;
        }
    };

    // This logger is reached only in the existing hull fallback and carries no decision logic.
    void LogMissingLeg_T1(bool&, const char*) {}
}

#include "traffic_flowtype_override.inc"

using namespace BrnTraffic;

static bool IsRequested(const RecordingTrafficCarStreamer& lrStreamer, u8 luAsset)
{
    return (lrStreamer.maxLoadFlags[luAsset]
            & static_cast<u8>(TrafficCarStreamer::E_LOADFLAG_REQUESTED)) != 0;
}

static void ConfigureHullFallback(FlowtypeFixture& lrModule)
{
    lrModule.meLocalPlayerIndex = static_cast<decltype(lrModule.meLocalPlayerIndex)>(0);
    lrModule.maaRaceCarHulls[0].Construct();
    lrModule.maaRaceCarHulls[0].Append(lrModule.muExpectedHullId);
    lrModule.mHull.muNumVehicleAssets = 2;
    lrModule.mHull.mauVehicleAssets[0] = 4;
    lrModule.mHull.mauVehicleAssets[1] = 9;
}

static void TestNoPlayerWinsBeforeDataOrHull()
{
    FlowtypeFixture lModule;
    lModule.meLocalPlayerIndex = static_cast<decltype(lModule.meLocalPlayerIndex)>(-1);
    lModule.miDEBUGFlowtypeOverride = 0;
    bool lbThrew = false;
    try { lModule.AddVehiclesToTargetList(); }
    catch (const FlowtypeAssertTrap&) { lbThrew = true; }

    Check(!lbThrew, "no-player guard returns before the null ResourcePtr is dereferenced");
    Check(gFlowtypeAssertCount == 0, "no-player guard emits no assertion");
    Check(gFlowtypeSinkCalls == 0, "no-player guard makes no streamer call");
    Check(lModule.muHullLookups == 0, "no-player guard does not consult the hull list");
}

static void TestFallback(s32 liOverride, bool lbBindData, const char* lpcLabel)
{
    FlowtypeFixture lModule;
    FlowInput lInput;
    if (lbBindData)
    {
        lInput.Bind(lModule);
    }
    ConfigureHullFallback(lModule);
    lModule.miDEBUGFlowtypeOverride = liOverride;

    bool lbThrew = false;
    try { lModule.AddVehiclesToTargetList(); }
    catch (const FlowtypeAssertTrap&) { lbThrew = true; }

    const std::string lPrefix(lpcLabel);
    Check(!lbThrew, (lPrefix + " has no assertion").c_str());
    Check(gFlowtypeSinkCalls == 1, (lPrefix + " calls the real streamer sink once").c_str());
    Check(HasAssetIds(gFlowtypeCapturedAssetIds, {4, 9}),
          (lPrefix + " preserves the current-hull asset order").c_str());
    Check(IsRequested(lModule.mStreamer, 4), (lPrefix + " requests hull asset 4").c_str());
    Check(IsRequested(lModule.mStreamer, 9), (lPrefix + " requests hull asset 9").c_str());
    Check(!IsRequested(lModule.mStreamer, 3), (lPrefix + " does not request a flow asset").c_str());
    Check(lModule.muHullLookups == 1, (lPrefix + " uses one hull lookup").c_str());
    Check(gFlowtypeAssertCount == 0, (lPrefix + " leaves assertion count at zero").c_str());
}

static void TestEligibleFlowAndDuplicateOrder()
{
    FlowtypeFixture lModule;
    FlowInput lInput;
    const u8 lauAssets[] = {3, 7, 3, 63, 7};
    lInput.SetAssetIds(lauAssets, 5);
    lInput.Bind(lModule);
    lModule.meLocalPlayerIndex = static_cast<decltype(lModule.meLocalPlayerIndex)>(0);
    lModule.miDEBUGFlowtypeOverride = 0;

    bool lbThrew = false;
    try { lModule.AddVehiclesToTargetList(); }
    catch (const FlowtypeAssertTrap&) { lbThrew = true; }

    Check(!lbThrew, "eligible flow does not assert");
    Check(HasAssetIds(gFlowtypeCapturedAssetIds, {3, 7, 63}),
          "eligible flow preserves first-occurrence order and removes duplicates");
    Check(gFlowtypeSinkCalls == 1, "eligible flow reaches the production sink once");
    Check(IsRequested(lModule.mStreamer, 3), "eligible flow requests asset 3");
    Check(IsRequested(lModule.mStreamer, 7), "eligible flow requests asset 7");
    Check(IsRequested(lModule.mStreamer, 63), "eligible flow requests the last valid asset slot");
    Check(!IsRequested(lModule.mStreamer, 4), "eligible flow does not request an absent asset");
    Check(!gbFlowtypeCaptureOnly, "valid asset indices execute the original sink loop");
    Check(gFlowtypeAssertCount == 0, "eligible flow leaves assertion count at zero");
    Check(lModule.muHullLookups == 0, "eligible flow returns before the hull path");
}

static void TestHighBytesAtContainerBoundaryOnly()
{
    FlowtypeFixture lModule;
    FlowInput lInput;
    const u8 lauAssets[] = {0x80, 0xFF, 0x80, 0x01, 0xFF};
    lInput.SetAssetIds(lauAssets, 5);
    lInput.Bind(lModule);
    lModule.meLocalPlayerIndex = static_cast<decltype(lModule.meLocalPlayerIndex)>(0);
    lModule.miDEBUGFlowtypeOverride = 0;

    bool lbThrew = false;
    try { lModule.AddVehiclesToTargetList(); }
    catch (const FlowtypeAssertTrap&) { lbThrew = true; }

    Check(!lbThrew, "container-byte ABI check only: high bytes reach the capture boundary");
    Check(HasAssetIds(gFlowtypeCapturedAssetIds, {0x80, 0xFF, 0x01}),
          "container-byte ABI check only: high bytes retain first-occurrence order");
    Check(gbFlowtypeCaptureOnly,
          "container-byte ABI check only: out-of-catalogue bytes are stopped before sink indexing");
    Check(gFlowtypeSinkCalls == 1, "container-byte ABI check only: the sink boundary is reached");
    Check(std::all_of(std::begin(lModule.mStreamer.maxLoadFlags),
                      std::end(lModule.mStreamer.maxLoadFlags),
                      [](u8 luFlags) { return luFlags == 0; }),
          "container-byte ABI check only: the guarded 64-slot sink loop is not executed");
    Check(gFlowtypeAssertCount == 0,
          "container-byte ABI check only: no out-of-range sink assertion is fabricated");
}

static void TestEmptyFlowKeepsArrayGetItemAssertion()
{
    FlowtypeFixture lModule;
    FlowInput lInput;
    lInput.SetAssetIds(nullptr, 0);
    lInput.Bind(lModule);
    lModule.meLocalPlayerIndex = static_cast<decltype(lModule.meLocalPlayerIndex)>(0);
    lModule.miDEBUGFlowtypeOverride = 0;

    bool lbCaught = false;
    try { lModule.AddVehiclesToTargetList(); }
    catch (const FlowtypeAssertTrap&) { lbCaught = true; }

    Check(lbCaught, "empty flow triggers the original checked GetItem assertion");
    Check(gFlowtypeLastAssert == "Array index out of bounds",
          "empty flow reports the original Array index expression");
    Check(gFlowtypeAssertCount == 1, "empty flow trips exactly one Array assertion");
    Check(gFlowtypeSinkCalls == 0, "empty flow assertion occurs before the streamer sink");
}

static void TestCapacityKeepsArrayAppendAssertion()
{
    FlowtypeFixture lModule;
    FlowInput lInput;
    u8 lauAssets[17]{};
    for (u32 luIndex = 0; luIndex < 17; ++luIndex)
    {
        lauAssets[luIndex] = static_cast<u8>(luIndex);
    }
    lInput.SetAssetIds(lauAssets, 17);
    lInput.Bind(lModule);
    lModule.meLocalPlayerIndex = static_cast<decltype(lModule.meLocalPlayerIndex)>(0);
    lModule.miDEBUGFlowtypeOverride = 0;

    bool lbCaught = false;
    try { lModule.AddVehiclesToTargetList(); }
    catch (const FlowtypeAssertTrap&) { lbCaught = true; }

    Check(lbCaught, "17 distinct assets trigger the original 16-slot Append assertion");
    Check(gFlowtypeLastAssert == "Array container out of space",
          "capacity overflow reports the original Array capacity expression");
    Check(gFlowtypeAssertCount == 1, "capacity overflow trips exactly one Array assertion");
    Check(gFlowtypeSinkCalls == 0, "capacity assertion occurs before the streamer sink");
}

int main()
{
    TestNoPlayerWinsBeforeDataOrHull();
    TestFallback(-1, false, "default -1 override fallback");
    TestFallback(1, true, "override equal to flow count fallback");
    TestEligibleFlowAndDuplicateOrder();
    TestHighBytesAtContainerBoundaryOnly();
    TestEmptyFlowKeepsArrayGetItemAssertion();
    TestCapacityKeepsArrayAppendAssertion();
    std::printf("TrafficFlowtypeOverride: %u checks, %u failures\n",
                gFlowtypeChecks, gFlowtypeFailures);
    return gFlowtypeFailures ? 1 : 0;
}
