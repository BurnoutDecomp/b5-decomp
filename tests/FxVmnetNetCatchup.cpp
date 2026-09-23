// FX-VMNET (crash parity 2026-09-23, G44-D2 part 2): the PRODUCTION body of
// VehicleManager::UpdateNetworkCatchup @0x82618E30 (BrnVehicleManagerPlayerStats.cpp), extracted
// by run_fxvmnet_net_catchup.py and re-homed onto VmFixture. The driver queue is the REAL
// VehicleDriverInputInterface (its VariableEventQueue<5040,16> templates are header-inline) filled
// with REAL BrnNetworkDriverControls records; the log sink runs the production StrStreamBase
// formatting (CgsStrStream.cpp compiled in). VehicleDriver / RaceCarPhysics are fixture types that
// record the StartCatchupInterpolation call (its own body is FxVmnetCatchup's subject).
//
// Console facts checked (ARTIST asm 0x82618E30):
//   only type-2 (NETWORK) records are processed (0x82618F1C `cmpwi r3,2`)
//   maeRaceCarTypes[idx] == INACTIVE -> skipped; any other non-NETWORK -> assert :4098, NOT skipped
//   lbSnap = mbSnap (lbz 0xB8) || !mNetworkCarsRecievedFirstUpdate.IsBitSet(idx) (this+0xAEB8)
//   StartCatchupInterpolation(&maRaceCarDrivers[idx], &maRaceCarVehicles[idx], +0x50, +0x90, +0xA0, lbSnap)
//   on a snap: the HIDE_ONLINE line (0x820941E8 / 0x820941FC / 0x82099F80 / "(%f, %f, %f)"
//   off_82F31964 / "\n"), SetBit(idx), SetNetworkRaceCarHidden(idx, 1)
#define _ALLOW_KEYWORD_MACROS 1
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static unsigned giAsserts = 0;
static std::string gLog;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int) { ++giAsserts; std::printf("ASSERT (expected in one case): %s\n", lpcMessage); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Message { u64 gxMessageFilterFlags = 1; }
namespace Log
{
    StrStreamBase& DebugPrint::operator<<(const char* lpcText) { gLog += lpcText; return *this; }
    void WriteToLog(const char*) {}
    static DebugPrint sCapture;
    DebugPrint* gpDebugPrint = &sCapture;
}
}

// The production scalar / AppendFormat formatting.
#include "GameShared/GameClasses/Development/CgsStrStream.cpp"

namespace BrnPhysics
{
namespace Vehicle
{
    struct FixtureCar { s32 miTag; };

    struct CatchupCall
    {
        const void*    mpDriver;
        const void*    mpCar;
        Matrix44Affine mTransform;
        Vector3        mLinear;
        Vector3        mAngular;
        bool           mbSnap;
    };
    static std::vector<CatchupCall> gCalls;

    struct FixtureDriver
    {
        s32 miTag;
        void StartCatchupInterpolation(FixtureCar* lpVehicle, const Matrix44Affine& lrTransform,
                                       const Vector3 lLinear, const Vector3 lAngular, bool lbSnap)
        {
            gCalls.push_back(CatchupCall{ this, lpVehicle, lrTransform, lLinear, lAngular, lbSnap });
        }
    };

    struct VmFixture
    {
        typedef FixtureDriver VehicleDriver;
        typedef FixtureCar    RaceCarPhysics;

        FixtureDriver              maRaceCarDrivers[8];
        FixtureCar                 maRaceCarVehicles[8];
        BrnWorld::ERaceCarType     maeRaceCarTypes[8];
        CgsContainers::BitArray<8> mNetworkCarsRecievedFirstUpdate;
        std::vector<std::pair<s32, s32>> mHides;

        void SetNetworkRaceCarHidden(EActiveRaceCarIndex leIndex, s32 liFrames)
        {
            mHides.push_back(std::make_pair(static_cast<s32>(leIndex), liFrames));
        }
        void UpdateNetworkCatchup(const VehicleDriverInputInterface* lpInputInterface);
    };

    // The production UpdateNetworkCatchup, or for the pre-fix revision (no body) one that does nothing.
#include "net_catchup.inc"
}
}

using namespace BrnPhysics::Vehicle;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

static VmFixture gVm;
static VehicleDriverInputInterface gInterface;   // static storage: 16-byte aligned queue data

static void NewFrame()
{
    gInterface.GetUpdateDriverQueue()->Clear();
    gCalls.clear();
    gVm.mHides.clear();
    gLog.clear();
}

static void AddNetwork(s32 liCar, bool lbSnap, f32 lfX)
{
    BrnNetworkDriverControls lControls;
    std::memset(&lControls, 0, sizeof(lControls));
    lControls.miVehicleID = liCar;
    lControls.mTransform.xAxis = Vector3{ 1, 0, 0, 0 };
    lControls.mTransform.yAxis = Vector3{ 0, 1, 0, 0 };
    lControls.mTransform.zAxis = Vector3{ 0, 0, 1, 0 };
    lControls.mTransform.wAxis = Vector3{ lfX, 2.5f, -3.25f, 0 };
    lControls.mLinearVelocity  = Vector3{ 11, 12, 13, 0 };
    lControls.mAngularVelocity = Vector3{ 0.5f, 0.25f, 0.125f, 0 };
    lControls.mbSnap = lbSnap;
    gInterface.GetUpdateDriverQueue()->AddEvent(&lControls, E_DRIVER_TYPE_NETWORK, sizeof(lControls));
}

static void AddOther(s32 liType)
{
    BrnPlayerDriverControls lControls;
    std::memset(&lControls, 0, sizeof(lControls));
    lControls.miVehicleID = 0;
    gInterface.GetUpdateDriverQueue()->AddEvent(&lControls, liType, sizeof(lControls));
}

int main()
{
    gInterface.GetUpdateDriverQueue()->Construct();
    for (s32 i = 0; i < 8; ++i)
    {
        gVm.maeRaceCarTypes[i] = BrnWorld::E_RACE_CAR_TYPE_INACTIVE;
        gVm.mNetworkCarsRecievedFirstUpdate.UnSetBit(static_cast<u32>(i));
    }
    gVm.maeRaceCarTypes[0] = BrnWorld::E_RACE_CAR_TYPE_PLAYER;
    gVm.maeRaceCarTypes[3] = BrnWorld::E_RACE_CAR_TYPE_NETWORK;
    gVm.maeRaceCarTypes[6] = BrnWorld::E_RACE_CAR_TYPE_AI;

    // ---- frame 1: the first update of network car 3 snaps ------------------------------------
    NewFrame();
    AddOther(E_DRIVER_TYPE_PLAYER);
    AddNetwork(3, false, 1.5f);
    AddOther(E_DRIVER_TYPE_TRAFFIC);
    gVm.UpdateNetworkCatchup(&gInterface);
    Check(gCalls.size() == 1, "frame 1: exactly one StartCatchupInterpolation (only the NETWORK record)");
    if (gCalls.size() == 1)
    {
        const CatchupCall& c = gCalls[0];
        Check(c.mpDriver == &gVm.maRaceCarDrivers[3] && c.mpCar == &gVm.maRaceCarVehicles[3],
              "frame 1: driver/vehicle are maRaceCarDrivers[3] / maRaceCarVehicles[3]");
        Check(c.mbSnap, "frame 1: no first update yet -> lbSnap even though mbSnap == 0");
        Check(c.mTransform.wAxis.x == 1.5f && c.mLinear.y == 12.0f && c.mAngular.z == 0.125f,
              "frame 1: transform (+0x50) and velocities (+0x90 / +0xA0) passed through");
    }
    Check(gVm.mNetworkCarsRecievedFirstUpdate.IsBitSet(3u), "frame 1: the snap sets mNetworkCarsRecievedFirstUpdate bit 3");
    Check(gVm.mHides.size() == 1 && gVm.mHides[0].first == 3 && gVm.mHides[0].second == 1,
          "frame 1: SetNetworkRaceCarHidden(3, 1)");
    Check(gLog == "HIDE_ONLINE: Network race car 3, type 2 was made hidden because it snapped to "
                  "(1.500000, 2.500000, -3.250000)\n",
          "frame 1: the console's HIDE_ONLINE snap line, position through \"(%f, %f, %f)\"");

    // ---- frame 2: same car, first update already received, no snap flag ---------------------
    NewFrame();
    AddNetwork(3, false, 1.75f);
    gVm.UpdateNetworkCatchup(&gInterface);
    Check(gCalls.size() == 1 && !gCalls[0].mbSnap, "frame 2: first update seen -> interpolate (lbSnap 0)");
    Check(gVm.mHides.empty() && gLog.empty(), "frame 2: no hide, no log without a snap");

    // ---- frame 3: an explicit snap from the network --------------------------------------------
    NewFrame();
    AddNetwork(3, true, 2.0f);
    gVm.UpdateNetworkCatchup(&gInterface);
    Check(gCalls.size() == 1 && gCalls[0].mbSnap && gVm.mHides.size() == 1,
          "frame 3: mbSnap (lbz 0xB8) forces the snap arm again");

    // ---- frame 4: an INACTIVE slot is skipped silently -----------------------------------------
    NewFrame();
    AddNetwork(5, true, 3.0f);
    const unsigned luAssertsBefore = giAsserts;
    gVm.UpdateNetworkCatchup(&gInterface);
    Check(gCalls.empty() && gVm.mHides.empty() && giAsserts == luAssertsBefore,
          "frame 4: maeRaceCarTypes == INACTIVE (3) -> record skipped, no assert");

    // ---- frame 5: a non-network, non-inactive slot asserts and is STILL processed --------------
    NewFrame();
    AddNetwork(6, false, 4.0f);
    const unsigned luAssertsBefore5 = giAsserts;
    gVm.UpdateNetworkCatchup(&gInterface);
    Check(giAsserts == luAssertsBefore5 + 1, "frame 5: an AI-typed slot fires the :4098 assert");
    Check(gCalls.size() == 1 && gCalls[0].mpDriver == &gVm.maRaceCarDrivers[6],
          "frame 5: ... and the console carries on into StartCatchupInterpolation (no skip)");

    // ---- the log gate is the log only ----------------------------------------------------------
    NewFrame();
    gVm.mNetworkCarsRecievedFirstUpdate.UnSetBit(3u);
    CgsDev::Message::gxMessageFilterFlags = 0;
    AddNetwork(3, false, 5.0f);
    gVm.UpdateNetworkCatchup(&gInterface);
    CgsDev::Message::gxMessageFilterFlags = 1;
    Check(gLog.empty() && gVm.mHides.size() == 1 && gVm.mNetworkCarsRecievedFirstUpdate.IsBitSet(3u),
          "filter bit clear: the snap still sets the bit and hides, silently");

    std::printf("FxVmnetNetCatchup: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
