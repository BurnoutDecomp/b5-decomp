// FX-TRAFFIC2 (crash parity 2026-09-24, G59-D3): the PRODUCTION
//   TrafficEntityModule::CreateBodiesForCrashingNetworkTraffic            @0x8274B4B0
//   TrafficEntityModule::IsPlayingOnlineGameMode (DWARF h:2221, inlined at 0x8274B528)
// extracted from the b5 sources by run_fxtraffic2_network_crash_bodies.py and hosted on a fixture
// that has the module's real member types; SafeRequestMakeVehiclePhysical (the callee the body
// hands each entry to) is a recorder here. Every expected value below is derived from the ARTIST
// asm (the decode is in the body's banner):
//   offline with entries -> the "IsPlayingOnlineGameMode() || ...GetLength() == 0" assert (5796),
//   and NO early return; causer = (meLocalPlayerIndex << 10) | 0x01000000 (0x8274B5C8/0x8274B5D0);
//   per u16 entry SafeRequestMakeVehiclePhysical(entry, 0 CRASHED, causer, 1 CRASHING, 0 Standard,
//   lpOutput, lpCreatedBodies); then the list is cleared (stwx 0 -> +0x57E34).
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static const char* gpcLastAssert = "";

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        gpcLastAssert = lpcMessage;
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

struct RequestCall
{
    u32 muVehicle; s32 miReason; u32 muTarget; s32 miTrafficType, miCrashType; const void* mpOutput; const void* mpBodies;
};
static std::vector<RequestCall> gRequests;

namespace BrnTraffic
{
    struct NetCrashFixture
    {
        typedef TrafficEntityModule M;
        typedef M::TotalTrafficBitArray TotalTrafficBitArray;

        decltype(M::maNewCrashedNetworkVehicles) maNewCrashedNetworkVehicles;
        decltype(M::meLocalPlayerIndex)          meLocalPlayerIndex;
        decltype(M::mbIsOnlineGameMode)          mbIsOnlineGameMode;

        bool IsPlayingOnlineGameMode() const;
        void SafeRequestMakeVehiclePhysical(u32 luVehicle, PhysicalReason leReason, EntityId lTargetEntityId,
                                            BrnPhysics::Vehicle::ETrafficType leTrafficType,
                                            BrnPhysics::Vehicle::eCrashTrafficType leCrashType,
                                            BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                            TotalTrafficBitArray* lpMadePhysical)
        {
            gRequests.push_back(RequestCall{ luVehicle, static_cast<s32>(leReason), lTargetEntityId.muValue,
                                             static_cast<s32>(leTrafficType), static_cast<s32>(leCrashType),
                                             lpOutput, lpMadePhysical });
        }

        void CreateBodiesForCrashingNetworkTraffic(BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                                   TotalTrafficBitArray* lpCreatedBodies);
    };
}

// The production bodies under test.
#include "network_crash_bodies.inc"

using namespace BrnTraffic;
using namespace BrnTraffic::BrnTrafficIO;
typedef NetCrashFixture Fixture;

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
alignas(64) static unsigned char gaOutput[sizeof(OutputBuffer_PrePhysics)];
static Fixture::TotalTrafficBitArray gBodies;

static OutputBuffer_PrePhysics* Out() { return reinterpret_cast<OutputBuffer_PrePhysics*>(gaOutput); }

static void Fresh(bool lbOnline, u32 luLocalPlayer)
{
    gFixture.maNewCrashedNetworkVehicles.Clear();
    gFixture.meLocalPlayerIndex = static_cast<decltype(gFixture.meLocalPlayerIndex)>(luLocalPlayer);
    gFixture.mbIsOnlineGameMode = lbOnline;
    gBodies.UnSetAll();
    gRequests.clear();
    gAsserts = 0;
    gpcLastAssert = "";
}

static void Run()
{
    gFixture.CreateBodiesForCrashingNetworkTraffic(Out(), &gBodies);
}

static bool Request(size_t luCall, u32 luVehicle, u32 luCauser)
{
    if (luCall >= gRequests.size())
    {
        return false;
    }
    const RequestCall& lr = gRequests[luCall];
    return lr.muVehicle == luVehicle && lr.miReason == 0 && lr.muTarget == luCauser && lr.miTrafficType == 1
        && lr.miCrashType == 0 && lr.mpOutput == Out() && lr.mpBodies == &gBodies;
}

int main()
{
    // ---- online, two remote crashes, local player 1 -----------------------------------------
    Fresh(true, 1);
    gFixture.maNewCrashedNetworkVehicles.Append(12);
    gFixture.maNewCrashedNetworkVehicles.Append(40);
    Run();
    Check(gRequests.size() == 2, "online: one SafeRequestMakeVehiclePhysical per queued car");
    Check(Request(0, 12, 0x01000400u),
          "first: (12, CRASHED, causer = local race car 1 == 0x01000400, CRASHING, Standard, out, bodies)");
    Check(Request(1, 40, 0x01000400u), "second: (40, same arguments), in queue order");
    Check(gFixture.maNewCrashedNetworkVehicles.GetLength() == 0, "the list is cleared (stwx 0 -> +0x57E34)");
    Check(gAsserts == 0, "online with entries: no assert");

    // ---- the causer is the LOCAL player's race-car id --------------------------------------
    Fresh(true, 3);
    gFixture.maNewCrashedNetworkVehicles.Append(599);
    Run();
    Check(gRequests.size() == 1 && Request(0, 599, 0x01000C00u),
          "local player 3 -> causer 0x01000C00 ((3 << 10) | owner 1 << 24)");

    // ---- offline and empty: nothing ----------------------------------------------------------
    Fresh(false, 0);
    Run();
    Check(gRequests.empty() && gAsserts == 0 && gFixture.maNewCrashedNetworkVehicles.GetLength() == 0,
          "offline and empty: no call, no assert");

    // ---- offline with an entry: the assert fires, but the body still runs (no early return) ---
    Fresh(false, 0);
    gFixture.maNewCrashedNetworkVehicles.Append(5);
    Run();
    Check(gAsserts == 1
          && std::strcmp(gpcLastAssert, "IsPlayingOnlineGameMode() || maNewCrashedNetworkVehicles.GetLength() == 0") == 0,
          "offline with an entry: exactly the .cpp:5796 assert");
    Check(gRequests.size() == 1 && Request(0, 5, 0x01000000u) && gFixture.maNewCrashedNetworkVehicles.GetLength() == 0,
          "offline with an entry: still promoted and cleared -- the assert is not a return");

    std::printf("FxTraffic2NetworkCrashBodies: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
