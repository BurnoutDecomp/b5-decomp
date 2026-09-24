// FX-TRAFFIC2 (crash parity 2026-09-24, reviewer C on 0e0a5781 item 3): the PRODUCTION
//   TrafficEntityModule::ManageTriggers                  @0x82747518
//   TrafficEntityModule::GetHull                         @0x8271D8B0
// extracted from the b5 sources by run_fxtraffic2_manage_triggers.py and hosted on a fixture that
// has the module's real member types, writing into a real OutputBuffer_PreScene's
// TriggerManagementInputInterface (the add VariableEventQueue<131072,16> and the remove
// EventQueue<InRemoveTriggerEvent,256>). LightTrigger::GetTransform is a stub here (it is
// ExpandPosPlusYRotToTransform, not under test). Checked against the ARTIST asm:
//   REMOVE list first: one InRemoveTriggerEvent per light trigger of each hull,
//     id = (hull << 8) | 0x39000000 | index (0x827476AC..0x827476B8);
//   ADD list: one type-2 InAddBoxTriggerEvent per trigger: GetTransform(), the same id at +0x40,
//     0xFF at +0x44/+0x45, |dimensions| at +0x50 (vandc128 sign clear), 0x60 bytes;
//   nothing is posted while the +0x729F0 debug byte is set; both lists are always cleared.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

namespace BrnTraffic
{
    // Not under test: a deterministic stand-in for ExpandPosPlusYRotToTransform(mPosPlusYRot).
    Matrix44Affine LightTrigger::GetTransform() const
    {
        Matrix44Affine lTransform;
        lTransform.xAxis = { 1.0f, 2.0f, 3.0f, 0.0f };
        lTransform.yAxis = { 4.0f, 5.0f, 6.0f, 0.0f };
        lTransform.zAxis = { 7.0f, 8.0f, 9.0f, 0.0f };
        lTransform.wAxis = { mPosPlusYRot.x, mPosPlusYRot.y, mPosPlusYRot.z, 1.0f };
        return lTransform;
    }

namespace BrnTrafficIO
{
    // The lock-checked accessor (0x82710E78) without the lock bookkeeping.
    OutputBuffer_PreScene::TriggerManagementInputInterface*
    OutputBuffer_PreScene::GetTriggerManagementInputInterface()
    {
        return &mTriggerManagementInputInterface;
    }
}

    struct TriggerFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::mHullsToAddTriggersFor)    mHullsToAddTriggersFor;
        decltype(M::mHullsToRemoveTriggersFor) mHullsToRemoveTriggersFor;
        decltype(M::mpData)                    mpData;
        decltype(M::mbDEBUGRunningWorstCase)   mbDEBUGRunningWorstCase;

        const Hull* GetHull(u32 luIndex) const;
        void ManageTriggers(BrnTrafficIO::OutputBuffer_PreScene* lpOutput);
    };
}

// The production bodies under test.
#include "manage_triggers.inc"

using namespace BrnTraffic;
using namespace BrnTraffic::BrnTrafficIO;
using namespace BrnWorld::TriggerEntityModuleIO;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaOutput[sizeof(OutputBuffer_PreScene)];
alignas(64) static unsigned char gaTrafficData[sizeof(TrafficData)];
alignas(64) static unsigned char gaHulls[8][sizeof(Hull)];
alignas(64) static LightTrigger gaTriggers[8][4];
static Hull* gapHulls[8];
alignas(64) static unsigned char gaFixture[sizeof(TriggerFixture)];   // raw: no ResourcePtr ctor/dtor

static OutputBuffer_PreScene& Out() { return *reinterpret_cast<OutputBuffer_PreScene*>(gaOutput); }
static TriggerManagementInputInterface& Iface() { return Out().mTriggerManagementInputInterface; }
static Hull& HullAt(u32 luHull) { return *reinterpret_cast<Hull*>(gaHulls[luHull]); }
static TriggerFixture& Fx() { return *reinterpret_cast<TriggerFixture*>(gaFixture); }

static void Fresh()
{
    std::memset(gaOutput, 0, sizeof(gaOutput));
    std::memset(gaFixture, 0, sizeof(gaFixture));
    Iface().mAddTriggerEventQueue.Construct();
    Iface().mRemoveTriggerEventQueue.Construct();
    std::memset(gaTrafficData, 0, sizeof(gaTrafficData));
    std::memset(gaHulls, 0, sizeof(gaHulls));
    std::memset(gaTriggers, 0, sizeof(gaTriggers));
    TrafficData& lrData = *reinterpret_cast<TrafficData*>(gaTrafficData);
    lrData.muNumHulls = 8;
    for (u32 luHull = 0; luHull < 8; ++luHull)
    {
        gapHulls[luHull] = &HullAt(luHull);
        HullAt(luHull).mpaLightTriggers = gaTriggers[luHull];
    }
    lrData.mpapHulls = gapHulls;
    Fx().mpData.mpResourceMemory = gaTrafficData;
    Fx().mHullsToAddTriggersFor.Clear();
    Fx().mHullsToRemoveTriggersFor.Clear();
    Fx().mbDEBUGRunningWorstCase = false;
    gAsserts = 0;
}

static void Trigger(u32 luHull, u32 luIndex, Vector3 lDimensions, f32 lfX, f32 lfY, f32 lfZ)
{
    gaTriggers[luHull][luIndex].mDimensions  = lDimensions;
    gaTriggers[luHull][luIndex].mPosPlusYRot = { lfX, lfY, lfZ, 0.25f };
    if (HullAt(luHull).muNumLightTriggers < luIndex + 1)
    {
        HullAt(luHull).muNumLightTriggers = static_cast<u8>(luIndex + 1);
    }
}

static u32 Id(u32 luHull, u32 luIndex) { return (luHull << 8) | 0x39000000u | luIndex; }

// The add queue's events, in order.
static s32 AddEvents(const InAddBoxTriggerEvent** lpaEvents, s32* lpaTypes, s32* lpaSizes, s32 liMax)
{
    s32 liCount = 0;
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = Iface().mAddTriggerEventQueue.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent && liCount < liMax)
    {
        lpaEvents[liCount] = reinterpret_cast<const InAddBoxTriggerEvent*>(lpEvent);
        lpaTypes[liCount]  = liType;
        lpaSizes[liCount]  = liSize;
        ++liCount;
        if (liCount >= Iface().mAddTriggerEventQueue.GetLength())
        {
            break;
        }
        const CgsModule::Event* lpNext = 0;
        liType  = Iface().mAddTriggerEventQueue.GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    return liCount;
}

int main()
{
    // ---- hull 3 leaves, hull 5 arrives -------------------------------------------------------
    Fresh();
    Trigger(3, 0, { 10.0f, 12.0f, 48.0f, 0.0f }, 1.0f, 0.0f, 1.0f);
    Trigger(3, 1, { 20.0f, 12.0f, 56.0f, 0.0f }, 2.0f, 0.0f, 2.0f);
    Trigger(5, 0, { -6.0f, 12.0f, -104.0f, 0.0f }, 100.0f, 5.0f, -40.0f);
    Trigger(5, 1, { 56.0f, -12.0f, 48.0f, 0.0f }, 200.0f, 6.0f, -50.0f);
    Fx().mHullsToRemoveTriggersFor.Append(3);
    Fx().mHullsToAddTriggersFor.Append(5);
    Fx().ManageTriggers(&Out());

    Check(Iface().mRemoveTriggerEventQueue.GetLength() == 2
          && Iface().mRemoveTriggerEventQueue.GetEvent(0).mTriggerID == Id(3, 0)
          && Iface().mRemoveTriggerEventQueue.GetEvent(1).mTriggerID == Id(3, 1),
          "hull 3's two light triggers are removed as (3 << 8) | 0x39000000 | i (0x827476AC..0x827476CC)");

    const InAddBoxTriggerEvent* lapEvents[8] = {};
    s32 laiTypes[8] = {};
    s32 laiSizes[8] = {};
    const s32 liAdded = AddEvents(lapEvents, laiTypes, laiSizes, 8);
    Check(liAdded == 2 && laiTypes[0] == 2 && laiTypes[1] == 2
          && laiSizes[0] == static_cast<s32>(sizeof(InAddBoxTriggerEvent)) && sizeof(InAddBoxTriggerEvent) == 0x60,
          "hull 5's two triggers are added as type-2, 0x60-byte InAddBoxTriggerEvents (0x82747818 li r5, 2)");
    if (liAdded == 2)
    {
        const InAddBoxTriggerEvent& lr0 = *lapEvents[0];
        const InAddBoxTriggerEvent& lr1 = *lapEvents[1];
        Check(lr0.mPackedQueryFlagsAndIndex == Id(5, 0) && lr1.mPackedQueryFlagsAndIndex == Id(5, 1)
              && lr0.muTriggerType == 0xFFu && lr0.muSubType == 0xFFu && lr1.muTriggerType == 0xFFu && lr1.muSubType == 0xFFu,
              "the add event carries the id at +0x40 and 0xFF at +0x44/+0x45 (0x82747848 / `stb r24(-1)`)");
        Check(lr0.mDimensions.x == 6.0f && lr0.mDimensions.y == 12.0f && lr0.mDimensions.z == 104.0f
              && lr1.mDimensions.x == 56.0f && lr1.mDimensions.y == 12.0f && lr1.mDimensions.z == 48.0f,
              "the add event carries |mDimensions| (0x82747834 vandc128 with the 0x80000000 splat), full extents");
        Check(lr0.mTransform.wAxis.x == 100.0f && lr0.mTransform.wAxis.z == -40.0f && lr0.mTransform.yAxis.y == 5.0f
              && lr1.mTransform.wAxis.x == 200.0f,
              "the add event carries the trigger's GetTransform() (0x82747800 ExpandPosPlusYRotToTransform)");
    }
    else
    {
        Check(false, "add events: fields");
        Check(false, "add events: dimensions");
        Check(false, "add events: transform");
    }
    Check(Fx().mHullsToAddTriggersFor.GetLength() == 0 && Fx().mHullsToRemoveTriggersFor.GetLength() == 0,
          "both hull lists are cleared (0x827478AC / 0x827478B0)");
    Check(gAsserts == 0, "no assert");

    // ---- the "Running Worst Case" debug byte suppresses every post, the lists still clear ----
    Fresh();
    Trigger(3, 0, { 10.0f, 12.0f, 48.0f, 0.0f }, 1.0f, 0.0f, 1.0f);
    Trigger(5, 0, { 10.0f, 12.0f, 48.0f, 0.0f }, 1.0f, 0.0f, 1.0f);
    Fx().mHullsToRemoveTriggersFor.Append(3);
    Fx().mHullsToAddTriggersFor.Append(5);
    Fx().mbDEBUGRunningWorstCase = true;
    Fx().ManageTriggers(&Out());
    Check(Iface().mRemoveTriggerEventQueue.GetLength() == 0 && Iface().mAddTriggerEventQueue.GetLength() == 0
          && Fx().mHullsToAddTriggersFor.GetLength() == 0 && Fx().mHullsToRemoveTriggersFor.GetLength() == 0,
          "+0x729F0 set: nothing is posted (0x827476A0 / 0x827477DC `bne`), both lists still cleared");

    // ---- empty lists (every frame on this build): nothing posted, nothing asserted ------------
    Fresh();
    Fx().ManageTriggers(&Out());
    Check(Iface().mRemoveTriggerEventQueue.GetLength() == 0 && Iface().mAddTriggerEventQueue.GetLength() == 0
          && gAsserts == 0,
          "empty lists: no event, no assert");

    std::printf("FxTraffic2ManageTriggers: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
