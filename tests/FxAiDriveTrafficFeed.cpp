// L6 AIDRIVE (owner list 2026-09-27): THE AI TRAFFIC FEED -- rivals never saw a traffic car.
//
// The PRODUCTION body of
//   TrafficEntityModule::StoreAISceneResultsForNextFrame @0x82728400
// (and the two id constants it tests against) is extracted from the b5 sources by
// run_fxaidrive_traffic_feed.py into fxaidrive_traffic_feed.inc and hosted on a fixture that holds the
// module's REAL maStoredAITrafficData (decltype of the member). A revision without the body does not
// build, and every numeric check then counts as failed.
//
// The input is a REAL BrnTrafficIO::InputBuffer_PrePhysics whose scene-result queue is filled exactly as
// the scene pass fills it (SceneManagerModule::ProcessCoarseFrustumTestVp @0x828C6518 -> AllocateEvent(0,
// 4 * (written + 3)); +0 the query id, +4 the WRITTEN count, +8 the ATTEMPTED count (0x828C67EC..F8), the
// entity ids from +0xC). Its read seat (InputBuffer_PrePhysics::GetSceneResultQueue() const, the console's
// sub_82711310: read-lock tripwire then this + 0x28C30) is a counting stand-in here.
//
// Every expected value comes from the ARTIST asm of 0x82728400 (the banner of the production body):
//   the query id in [dword_82F2FE8C = 0x7ACE, dword_8300CB54 = 0x7ACE + 0x23] inclusive (cmplw blt / bgt);
//   n = the ATTEMPTED count (+8, lwz 0x827284B8), `cmpwi n, 0x20 ; ble` -> min(n, 32);
//   record = maStoredAITrafficData[id - base]: meRaceCarIndex = id - base, the n ids memcpy'd in order,
//   miNumTrafficIDs = n; any other id is passed over; an empty queue touches nothing.
#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gReadSeatCalls = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// The read seat the console calls (sub_82711310): here it only counts, and hands back the queue.
namespace BrnTraffic
{
namespace BrnTrafficIO
{
    const InputBuffer_PrePhysics::SceneResultQueue* InputBuffer_PrePhysics::GetSceneResultQueue() const
    {
        ++gReadSeatCalls;
        return &mSceneResultQueue;
    }
}
}

namespace BrnTraffic
{
    struct AITrafficFeedFixture
    {
        typedef TrafficEntityModule M;
        decltype(M::maStoredAITrafficData) maStoredAITrafficData;

        void StoreAISceneResultsForNextFrame(const BrnTrafficIO::InputBuffer_PrePhysics* lpInput);
    };
}

// The production body under test (+ the two id constants, in their anonymous namespace).
#include "fxaidrive_traffic_feed.inc"

using namespace BrnTraffic;
using namespace BrnTraffic::BrnTrafficIO;
typedef AITrafficFeedFixture Fixture;
typedef CgsSceneManager::SceneManagerIO::OutCoarseQueryResult Result;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static Fixture gFixture;
static InputBuffer_PrePhysics gInput;

static const u32 KU_BASE = 0x7ACEu;   // dword_82F2FE8C
static const u32 KU_SENTINEL = 0xDEADBEEFu;

// Every record starts poisoned, so an untouched record is visible.
static void Fresh()
{
    for (s32 liRecord = 0; liRecord < 8; ++liRecord)
    {
        StoredAITrafficData& lr = gFixture.maStoredAITrafficData[liRecord];
        lr.meRaceCarIndex  = static_cast<EActiveRaceCarIndex>(0x55);
        lr.miNumTrafficIDs = -7;
        for (s32 li = 0; li < BrnTrafficIO::KI_MAX_TRAFFIC_NEAR_A_RACECAR; ++li)
            lr.maTrafficEntityIDs[li].muValue = KU_SENTINEL;
    }
    gInput.mSceneResultQueue.Construct();
    gReadSeatCalls = 0;
    gAsserts = 0;
}

// A traffic-vehicle entity id as the scene pass resolves one: owner 2 in the top byte, index << 10.
static u32 TrafficId(u32 luIndex) { return 0x02000000u | (luIndex << 10); }

// One result record, laid out as ProcessCoarseFrustumTestVp writes it: the event holds `liWritten`
// ids; +8 carries `liAttempted`. Ids are TrafficId(luFirstIndex + i).
static void AddResult(u32 luQueryId, s32 liWritten, s32 liAttempted, u32 luFirstIndex)
{
    Result* lp = static_cast<Result*>(gInput.mSceneResultQueue.AllocateEvent(
        Result::KI_EVENT_TYPE, static_cast<s32>(4 * (liWritten + 3))));
    lp->mQueryId.mId          = luQueryId;
    lp->miNumResults          = liWritten;
    lp->miNumResultsAttempted = liAttempted;
    for (s32 li = 0; li < liWritten; ++li)
        lp->GetEntityIds()[li] = CgsSceneManager::EntityId(TrafficId(luFirstIndex + static_cast<u32>(li)));
}

static bool Untouched(s32 liRecord)
{
    const StoredAITrafficData& lr = gFixture.maStoredAITrafficData[liRecord];
    if (static_cast<s32>(lr.meRaceCarIndex) != 0x55 || lr.miNumTrafficIDs != -7) return false;
    for (s32 li = 0; li < BrnTrafficIO::KI_MAX_TRAFFIC_NEAR_A_RACECAR; ++li)
        if (lr.maTrafficEntityIDs[li].muValue != KU_SENTINEL) return false;
    return true;
}

// Record liRecord holds n ids TrafficId(first + i), and every slot past n is still the sentinel.
static bool Holds(s32 liRecord, s32 liCount, u32 luFirstIndex)
{
    const StoredAITrafficData& lr = gFixture.maStoredAITrafficData[liRecord];
    if (static_cast<s32>(lr.meRaceCarIndex) != liRecord || lr.miNumTrafficIDs != liCount) return false;
    for (s32 li = 0; li < BrnTrafficIO::KI_MAX_TRAFFIC_NEAR_A_RACECAR; ++li)
    {
        const u32 luWant = (li < liCount) ? TrafficId(luFirstIndex + static_cast<u32>(li)) : KU_SENTINEL;
        if (lr.maTrafficEntityIDs[li].muValue != luWant) return false;
    }
    return true;
}

static void Run() { gFixture.StoreAISceneResultsForNextFrame(&gInput); }

int main()
{
    // ---- the two id constants are the image's ----------------------------------------------------
    Check(KU_AI_FRUSTUM_QUERY_ID_BASE == KU_BASE, "id base == dword_82F2FE8C (0x7ACE)");
    Check(KU_AI_FRUSTUM_QUERY_ID_LAST == KU_BASE + 0x23u,
          "last accepted id == dword_8300CB54 == base + 0x23 (CRT thunk 0x82C66740..0x82C66750)");

    // ---- an empty queue: nothing is written ---------------------------------------------------------
    Fresh();
    Run();
    bool lbAll = true;
    for (s32 li = 0; li < 8; ++li) lbAll = lbAll && Untouched(li);
    Check(lbAll, "empty queue: every record untouched (GetLength() > 0 gate, 0x82728448)");
    Check(gReadSeatCalls == 1, "the queue is read through the const READ seat (sub_82711310)");

    // ---- one AI answer: car 2 sees five traffic cars -------------------------------------------------
    Fresh();
    AddResult(KU_BASE + 2, 5, 5, 100);
    Run();
    Check(Holds(2, 5, 100), "id base+2, 5 ids -> record 2: race car 2, the five ids in order, count 5");
    lbAll = true;
    for (s32 li = 0; li < 8; ++li) if (li != 2) lbAll = lbAll && Untouched(li);
    Check(lbAll, "id base+2: no other record touched");

    // ---- the nearby-traffic sphere (99) and ids outside the range are passed over -------------------
    Fresh();
    AddResult(99u, 4, 4, 7);                // PostNearbyTrafficSceneQueryRequest's sphere
    AddResult(KU_BASE - 1u, 3, 3, 9);       // below the base (blt 0x827284A8)
    AddResult(KU_BASE + 0x24u, 3, 3, 9);    // above base + 0x23 (bgt 0x827284B4)
    AddResult(KU_BASE + 0, 2, 2, 300);      // car 0 (the player's own query)
    AddResult(KU_BASE + 7, 1, 1, 599);      // car 7
    Run();
    Check(Holds(0, 2, 300), "a mixed queue: record 0 from id base+0");
    Check(Holds(7, 1, 599), "a mixed queue: record 7 from id base+7");
    lbAll = true;
    for (s32 li = 1; li < 7; ++li) lbAll = lbAll && Untouched(li);
    Check(lbAll, "a mixed queue: the sphere (99), base-1 and base+0x24 results write nothing");

    // ---- the count is the ATTEMPTED one (+8), not the written one (+4) -------------------------------
    Fresh();
    AddResult(KU_BASE + 4, 6, 3, 40);
    Run();
    Check(Holds(4, 3, 40), "written 6 / attempted 3: 3 ids copied (lwz n, 8(event) @0x827284B8)");

    // ---- more than 32: min(n, 32) (cmpwi 0x20 ; ble) -------------------------------------------------
    Fresh();
    AddResult(KU_BASE + 5, 40, 40, 200);
    Run();
    Check(Holds(5, 32, 200), "40 results: the first 32 ids kept, count 32");

    // ---- a later answer for the same car replaces the earlier one ------------------------------------
    Fresh();
    AddResult(KU_BASE + 1, 6, 6, 10);
    AddResult(KU_BASE + 1, 2, 2, 50);
    Run();
    {
        const StoredAITrafficData& lr = gFixture.maStoredAITrafficData[1];
        Check(lr.miNumTrafficIDs == 2 && lr.maTrafficEntityIDs[0].muValue == TrafficId(50)
              && lr.maTrafficEntityIDs[1].muValue == TrafficId(51)
              && lr.maTrafficEntityIDs[2].muValue == TrafficId(12),
              "two answers for car 1: the second's count and ids win; slots past its count keep the first's (memcpy of 4n only)");
    }

    // ---- zero results clear the count (the record still becomes the car's) ---------------------------
    Fresh();
    AddResult(KU_BASE + 3, 0, 0, 0);
    Run();
    Check(static_cast<s32>(gFixture.maStoredAITrafficData[3].meRaceCarIndex) == 3
          && gFixture.maStoredAITrafficData[3].miNumTrafficIDs == 0
          && gFixture.maStoredAITrafficData[3].maTrafficEntityIDs[0].muValue == KU_SENTINEL,
          "an empty answer for car 3: race car 3, count 0, nothing copied");

    Check(gAsserts == 0, "no assert on any of the above (lpInput is set)");

    std::printf("FxAiDriveTrafficFeed: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
