#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

namespace BrnAI {
#include "ai_route_storage_source.inc"
}

static int siChecks, siFailures;
static void Check(bool lbPass, const char* lpcName)
{
    ++siChecks;
    if (!lbPass) {
        ++siFailures;
        std::printf("FAIL %s\n", lpcName);
    }
}

int main()
{
    using namespace BrnAI;
    Check(sizeof(RouteNode) == 16 && alignof(RouteNode) == 4, "original scalar node extent and alignment");
    Check(sizeof(Route) == 0x140C && alignof(Route) == 4, "original route extent before owner tail");
    Check(std::is_same<decltype(AICar::mRoute), Route>::value, "AICar owns a real Route");
    Check(std::is_same<decltype(RouteMapModuleIO::RouteResponse::mRoute), Route>::value,
          "response owns a real Route");
    Check(sizeof(RouteMapModuleIO::RouteResponse) == 5136
          && alignof(RouteMapModuleIO::RouteResponse) == 4, "original response stride and alignment");

    Route lSource;
    std::memset(&lSource, 0xA5, sizeof(lSource));
    lSource.miNodeCount = 2;
    lSource.miDefaultStartNode = 1;
    lSource.meStatus = Route::E_STATUS_COMPLETE;
    lSource.maNodes[0].Construct(10.0f, 20.0f, 0x1234, 0x56);
    lSource.maNodes[1].Construct(30.0f, 40.0f, 0x2345, 0x67);
    lSource.maNodes[0].SetDistanceLeft(42.5f);

    Route lCopy;
    std::memset(&lCopy, 0x5A, sizeof(lCopy));
    lCopy.Construct(&lSource);
    Check(lCopy.GetNodeCount() == 2 && lCopy.GetDefaultStartNode() == 1
          && lCopy.GetStatus() == Route::E_STATUS_COMPLETE, "copy carries the original three control words");
    Check(std::memcmp(lCopy.maNodes, lSource.maNodes, 2 * sizeof(RouteNode)) == 0,
          "copy preserves every populated record byte including padding");
    unsigned char laUntouched[sizeof(RouteNode)];
    std::memset(laUntouched, 0x5A, sizeof(laUntouched));
    Check(std::memcmp(&lCopy.maNodes[2], laUntouched, sizeof(laUntouched)) == 0,
          "copy leaves the unused node suffix alone");
    Check(lCopy.GetNode(0)->GetSectionIndex() == 0x1234
          && lCopy.GetNode(0)->GetPortalIndex() == 0x56
          && lCopy.GetNode(0)->GetDistanceLeft() == 42.5f, "typed section portal and distance survive copy");

    Route lBeforeSelf = lCopy;
    lCopy.Construct(&lCopy);
    Check(std::memcmp(&lCopy, &lBeforeSelf, sizeof(lCopy)) == 0, "self copy retains all route bytes");

    AICar lCar;
    std::memset(&lCar, 0xCD, sizeof(lCar));
    unsigned char laCarTail[sizeof(AICar) - 0x140C];
    std::memcpy(laCarTail, reinterpret_cast<const unsigned char*>(&lCar) + 0x140C, sizeof(laCarTail));
    Check(lCar.GetRoute() == &lCar.mRoute && static_cast<const AICar&>(lCar).GetRoute() == &lCar.mRoute,
          "both car accessors return the embedded owner");
    Check(reinterpret_cast<const unsigned char*>(&lCar.mAggressiveness)
          - reinterpret_cast<const unsigned char*>(&lCar) == 0x140C, "aggressiveness immediately follows scalar route");
    lSource.miNodeCount = Route::KI_MAX_NODES;
    lCar.GetRoute()->Construct(&lSource);
    Check(std::memcmp(laCarTail, reinterpret_cast<const unsigned char*>(&lCar) + 0x140C, sizeof(laCarTail)) == 0,
          "full 320-node copy cannot overwrite the car's following state");

    RouteMapModuleIO::RouteResponse lResponse;
    std::memset(&lResponse, 0xA7, sizeof(lResponse));
    unsigned char laBeforeIds[sizeof(Route)];
    std::memcpy(laBeforeIds, &lResponse.mRoute, sizeof(Route));
    lResponse.Construct(0xABCD, 0x1234);
    Check(lResponse.GetOwnerId() == 0xABCD && lResponse.GetEventId() == 0x1234,
          "response constructor sets both ids");
    Check(std::memcmp(laBeforeIds, &lResponse.mRoute, sizeof(Route)) == 0,
          "response id construction does not initialize the route");
    lResponse.GetRoute()->Construct(&lSource);
    Check(lResponse.GetOwnerId() == 0xABCD && lResponse.GetEventId() == 0x1234,
          "full route copy preserves response ids at 140C and 140E");

    Route lEmpty{};
    lEmpty.miDefaultStartNode = -1;
    lEmpty.meStatus = Route::E_STATUS_BLOCKED;
    lCopy.Construct(&lEmpty);
    Check(lCopy.GetNodeCount() == 0 && lCopy.GetDefaultStartNode() == -1
          && lCopy.GetStatus() == Route::E_STATUS_BLOCKED, "empty copy still carries control words");

    Route lAppend{};
    Check(lAppend.AddNode(lSource.maNodes[0]) && lAppend.GetNodeCount() == 1,
          "first node appends");
    Check(std::memcmp(lAppend.maNodes, lSource.maNodes, sizeof(RouteNode)) == 0,
          "append preserves the complete original record");
    RouteNode lDuplicate;
    lDuplicate.Construct(10.0f, 20.0f, 99, 77);
    Check(lAppend.AddNode(lDuplicate) && lAppend.GetNodeCount() == 1
          && lAppend.GetNode(0)->GetSectionIndex() == 0x1234, "position duplicate succeeds without replacing metadata");
    lDuplicate.Construct(std::numeric_limits<f32>::quiet_NaN(), 20.0f, 99, 77);
    Check(lAppend.AddNode(lDuplicate) && lAppend.GetNodeCount() == 2,
          "unordered position is not treated as a duplicate");

    lCar.mRoute.miNodeCount = Route::KI_MAX_NODES - 1;
    lCar.mRoute.maNodes[Route::KI_MAX_NODES - 2].Construct(-1.0f, -1.0f, 0, 0);
    Check(lCar.mRoute.AddNode(lSource.maNodes[0]) && lCar.mRoute.GetNodeCount() == Route::KI_MAX_NODES,
          "final capacity slot appends");
    Check(!lCar.mRoute.AddNode(lDuplicate) && lCar.mRoute.GetNodeCount() == Route::KI_MAX_NODES,
          "full capacity rejects before reading or storing another slot");
    Check(std::memcmp(laCarTail, reinterpret_cast<const unsigned char*>(&lCar) + 0x140C, sizeof(laCarTail)) == 0,
          "capacity boundary retains adjacent car state");

    std::printf("AiRouteStorage: %d checks, %d failures\n", siChecks, siFailures);
    return siFailures ? 1 : 0;
}
