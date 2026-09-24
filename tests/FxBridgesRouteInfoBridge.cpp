// FX-BRIDGES (crash parity 2026-09-24) CC-11, the BRIDGE half of the checkpoint-distance route pair: the PRODUCTION
// body of BrnGame::BridgeWorldToGameState_RouteInfo (leg 10 of BridgeWorldToGameState @0x823E5368, de-inlined in
// GameBridgeWorldToX.cpp) extracted verbatim by run_fxbridges_route_info.py and compiled against the REAL
// RouteResponse / RouteResponseQueue (BrnRouteMapModuleIO.h), the REAL ModeManagerRouteInfoEvent (BrnGameEvents.h)
// and the REAL VariableEventQueue<1536,16>, with a fixture for the world output's route-response accessor.
//   0x823E54FC  only muOwnerId (+0x140C) == 2 (E_OWNER_MODE_MANAGER) is forwarded
//   0x823E5508  distance = node count (+0x1400) > 0 ? the route's +0x8 (Route::GetDistance) : 0.0f
//   0x823E5530  { (s32) muEventId (+0x140E), distance } -> AddEvent(record, 174, 8)
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }   // VariableEventQueue::OutputQueueContents' filter word
}

using BrnAI::RouteMapModuleIO::RouteResponse;
typedef BrnAI::RouteMapModuleIO::RouteResponseQueue RouteResponseQueue;

struct WorldOutputFixture
{
    RouteResponseQueue mQueue;
    const RouteResponseQueue* GetRouteResponseQueue() const { return &mQueue; }
};

namespace BrnGame
{
    static void RouteInfo(CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue, const WorldOutputFixture* lpWorldOutput)
#include "fxbridges_route_info_leg10.inc"
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

static RouteResponse MakeResponse(u16 luOwner, u16 luEvent, s32 liNodes, f32 lfDistance)
{
    RouteResponse lResponse;
    std::memset(&lResponse, 0, sizeof(lResponse));
    lResponse.Construct(luOwner, luEvent);
    lResponse.GetRoute()->miNodeCount = liNodes;
    lResponse.GetRoute()->maNodes[0].z = lfDistance;   // Route +0x8 == node 0's distance to the checkpoint
    return lResponse;
}

static WorldOutputFixture gWorld;

int main()
{
    Check(sizeof(BrnGameState::GameStateModuleIO::ModeManagerRouteInfoEvent) == 8,
          "the record is 8 bytes (li r6, 8 @0x823E5534)");

    gWorld.mQueue.Construct();
    gWorld.mQueue.AddEvent(MakeResponse(0, 5, 10, 11.0f));          // an AI answer
    gWorld.mQueue.AddEvent(MakeResponse(2, 0, 12, 480.25f));        // the mode manager's leg 0
    gWorld.mQueue.AddEvent(MakeResponse(1, 9, 3, 7.0f));            // a GUI answer
    gWorld.mQueue.AddEvent(MakeResponse(2, 1, 0, 999.0f));          // the mode manager's leg 1, NO route

    CgsModule::VariableEventQueue<1536, 16> lEvents;
    lEvents.Construct();
    BrnGame::RouteInfo(&lEvents, &gWorld);

    s32 liCount = 0, liType174 = 0;
    s32 laEventIds[4] = { -1, -1, -1, -1 };
    f32 lafDistances[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    s32 liSizesOk = 1;
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lEvents.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != 0)
    {
        if (liType == 174 && liCount < 4)
        {
            const BrnGameState::GameStateModuleIO::ModeManagerRouteInfoEvent* lpInfo =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::ModeManagerRouteInfoEvent*>(lpEvent);
            laEventIds[liCount]   = lpInfo->miEventId;
            lafDistances[liCount] = lpInfo->mfRouteDistance;
            if (liSize != 8) liSizesOk = 0;
            ++liType174;
        }
        ++liCount;
        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lEvents.GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }

    Check(liCount == 2 && liType174 == 2, "only the two E_OWNER_MODE_MANAGER answers become events, both type 174");
    Check(laEventIds[0] == 0 && lafDistances[0] == 480.25f,
          "a routed answer carries its event id and Route::GetDistance() (+0x8)");
    Check(laEventIds[1] == 1 && lafDistances[1] == 0.0f,
          "an answer with no nodes carries 0.0f (flt_82001CC0), not the stale +0x8");
    Check(liSizesOk == 1 && gAsserts == 0, "each record is posted with size 8; no asserts");

    std::printf("FxBridgesRouteInfoBridge: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
