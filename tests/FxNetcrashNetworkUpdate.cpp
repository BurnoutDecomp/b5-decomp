// crash parity FX-NETCRASH (2026-09-25), online hull set piece 1: the PRODUCTION body of
// TrafficEntityModule::GenerateNetworkUpdateEvents @0x827287A8, extracted by
// run_fxnetcrash_network_update.py and compiled as a member of NetFixture. NetFixture carries
// exactly the module members the body reaches, each declared with the real member's type. The input
// buffer is a fake that exposes the two race-car accessors the body calls. The OUTPUT is the real
// BrnTrafficIO::TrafficNetworkOutputInterface (BrnTrafficNetworkOutputInterface.cpp is linked), so
// the SetActiveHull / SetDetectedHullSyncDivergence / ActivateHull bodies are the production ones
// too, and so is Pvs::GetHullIndexForPoint (BrnTrafficPvs.cpp is linked).
//
// Expected values come from the runner (network_cases.inc), computed from the console's predicate
// (0x82728900..0x82728970) and the Pvs grid formula, independent of the reconstruction.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"
#include "SharedClasses/Traffic/BrnTrafficPvs.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0, gGateLogs = 0;

namespace CgsDev
{
namespace Assert
{
    char* gpcMessageBuffer = nullptr;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::printf("ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

namespace BrnTraffic
{
namespace
{
    // The module's named-gate logger; the double counts the calls so the test can see the hash leg
    // being reached (online, RUNNING, decision frame) and nowhere else.
    inline void LogMissingLeg_T1(bool& lrbAlreadyLogged, const char*)
    {
        lrbAlreadyLogged = true;
        ++gGateLogs;
    }

    // The BRN_NETCRASH_DIAG witness stream stays off here (its lines are PC-only prints).
    CgsDev::Log::DebugPrint* NetCrashDiagStream() { return nullptr; }
    const s32 KI_NETCRASH_HULL_DIAG_MAX_LINES = 48;
}

    struct FakeRaceCarState
    {
        struct { Vector3 xAxis, yAxis, zAxis, wAxis; } mTransform;   // only wAxis is read
    };

    struct FakeActiveRaceCars
    {
        bool             mabActive[E_ACTIVE_RACE_CAR_INDEX_COUNT];
        FakeRaceCarState maStates[E_ACTIVE_RACE_CAR_INDEX_COUNT];

        bool IsRaceCarActive(EActiveRaceCarIndex leIndex) const { return mabActive[leIndex]; }
        const FakeRaceCarState* GetRaceCarState(EActiveRaceCarIndex leIndex) const { return &maStates[leIndex]; }
    };

    struct FakeInput
    {
        FakeActiveRaceCars mCars;
        const FakeActiveRaceCars* GetActiveRaceCarOutputInterface() const { return &mCars; }
    };

    struct FakeOutput
    {
        BrnTrafficIO::TrafficNetworkOutputInterface mNetwork;
        BrnTrafficIO::TrafficNetworkOutputInterface* GetNetworkInterface() { return &mNetwork; }
    };

    struct FakeData { Pvs* mpPvs; };
    struct FakeDataPtr
    {
        FakeData* mp;
        FakeData* operator->() { return mp; }
    };

    struct NetFixture
    {
        typedef TrafficEntityModule M;
        static const M::EState E_STATE_RUNNING = M::E_STATE_RUNNING;

        FakeDataPtr                                mpData;
        decltype(M::mbHullSyncDivergence)          mbHullSyncDivergence;
        decltype(M::mbAllowDivergentBehaviour)     mbAllowDivergentBehaviour;
        decltype(M::meState)                       meState;
        decltype(M::mbNeedToBroadcastHullChange)   mbNeedToBroadcastHullChange;
        decltype(M::mHullChangeToBroadcast)        mHullChangeToBroadcast;
        bool                                       mbDecisionFrame;

        bool IsDecisionFrame() { return mbDecisionFrame; }

        void GenerateNetworkUpdateEvents(const FakeInput* lpInput, FakeOutput* lpOutput);
    };

#include "gnue_body.inc"
}

using namespace BrnTraffic;

#include "network_cases.inc"

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", lpcWhat);
    }
}

static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }

static Pvs      gPvs;
static FakeData gData;

static void Prepare(NetFixture& lrModule, FakeInput& lrInput, FakeOutput& lrOutput)
{
    // The grid: min (-1024, 0, -1024), 128 m cells, 16 x 16 -- every scale in the cases is exact.
    gPvs.mGridMin       = Vector3{ -1024.0f, 0.0f, -1024.0f, 0.0f };
    gPvs.mCellSize      = Vector3{ 128.0f, 1.0f, 128.0f, 0.0f };
    gPvs.mRecipCellSize = Vector3{ 1.0f / 128.0f, 1.0f, 1.0f / 128.0f, 0.0f };
    gPvs.muNumCells_X   = 16;
    gPvs.muNumCells_Z   = 16;
    gPvs.muNumCells     = 256;
    gPvs.mpaHullPvs     = nullptr;
    gData.mpPvs         = &gPvs;

    std::memset(&lrModule, 0, sizeof(lrModule));
    lrModule.mpData.mp = &gData;

    std::memset(&lrInput, 0, sizeof(lrInput));
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        lrInput.mCars.mabActive[liCar] = KA_CARS[liCar].active != 0;
        lrInput.mCars.maStates[liCar].mTransform.wAxis =
            Vector3{ FromBits(KA_CARS[liCar].pos[0]), FromBits(KA_CARS[liCar].pos[1]),
                     FromBits(KA_CARS[liCar].pos[2]), 1.0f };
    }

    lrOutput.mNetwork.Construct();
}

int main()
{
    // ---- 1. offline (divergent behaviour allowed), RUNNING, a broadcast pending -----------------
    {
        NetFixture lModule; FakeInput lInput; FakeOutput lOutput;
        Prepare(lModule, lInput, lOutput);
        lModule.mbAllowDivergentBehaviour   = true;
        lModule.meState                     = TrafficEntityModule::E_STATE_RUNNING;
        lModule.mbNeedToBroadcastHullChange = true;
        lModule.mbHullSyncDivergence        = true;
        lModule.mbDecisionFrame             = true;
        const unsigned luGates = gGateLogs;

        lModule.GenerateNetworkUpdateEvents(&lInput, &lOutput);

        bool lbTable = true;
        for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
        {
            if (lOutput.mNetwork.mau16ActiveHulls[liCar] != KA_EXPECTED_HULLS[liCar])
            {
                std::printf("  car %d: hull %u, want %u\n", liCar, lOutput.mNetwork.mau16ActiveHulls[liCar],
                            KA_EXPECTED_HULLS[liCar]);
                lbTable = false;
            }
        }
        Check(lbTable, "active-hull table: the console predicate (dd > 100 && (y > 0.5 || dd > 40000)) and "
                       "Pvs::GetHullIndexForPoint, 0xFFFF otherwise -- all eight slots");
        Check(lOutput.mNetwork.mbActiveHullsValid, "mbActiveHullsValid set (stb 1, 0x7D) -- GetActiveHulls stops asserting");
        Check(lOutput.mNetwork.mbHullSyncDivergence, "divergence byte copied from mbHullSyncDivergence (stb 0x7E)");
        Check(lOutput.mNetwork.GetActivateHullQueue().GetLength() == 0, "offline: no hull broadcast");
        Check(lModule.mbNeedToBroadcastHullChange, "offline: the pending broadcast is left alone");
        Check(gGateLogs == luGates, "offline: the hash leg is not reached");
    }

    // ---- 2. online, RUNNING, a broadcast pending, a decision frame -----------------------------
    {
        NetFixture lModule; FakeInput lInput; FakeOutput lOutput;
        Prepare(lModule, lInput, lOutput);
        lModule.mbAllowDivergentBehaviour   = false;
        lModule.meState                     = TrafficEntityModule::E_STATE_RUNNING;
        lModule.mbNeedToBroadcastHullChange = true;
        lModule.mHullChangeToBroadcast.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        lModule.mHullChangeToBroadcast.muNewActiveHull      = 77;
        lModule.mHullChangeToBroadcast.muUpdateFrame        = 1234;
        lModule.mbHullSyncDivergence        = false;
        lModule.mbDecisionFrame             = true;
        const unsigned luGates = gGateLogs;

        lModule.GenerateNetworkUpdateEvents(&lInput, &lOutput);

        const BrnTrafficIO::TrafficNetworkOutputInterface::ActivateHullQueue& lrQueue =
            lOutput.mNetwork.GetActivateHullQueue();
        Check(lrQueue.GetLength() == 1
              && lrQueue.GetEvent(0).meActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_3
              && lrQueue.GetEvent(0).muNewActiveHull == 77
              && lrQueue.GetEvent(0).muUpdateFrame == 1234,
              "online RUNNING: mHullChangeToBroadcast goes out as one ActivateHull(arc, hull, frame) (0x82728AA0)");
        Check(!lModule.mbNeedToBroadcastHullChange, "online RUNNING: mbNeedToBroadcastHullChange cleared (0x82728AA4)");
        Check(!lOutput.mNetwork.mbHullSyncDivergence, "divergence byte copied (false)");
        Check(gGateLogs == luGates + 1, "online RUNNING decision frame: the hash leg is reached (and gated)");
    }

    // ---- 3. online but still STARTING_UP: nothing goes out --------------------------------------
    {
        NetFixture lModule; FakeInput lInput; FakeOutput lOutput;
        Prepare(lModule, lInput, lOutput);
        lModule.mbAllowDivergentBehaviour   = false;
        lModule.meState                     = TrafficEntityModule::E_STATE_STARTING_UP;
        lModule.mbNeedToBroadcastHullChange = true;
        lModule.mbDecisionFrame             = true;
        const unsigned luGates = gGateLogs;

        lModule.GenerateNetworkUpdateEvents(&lInput, &lOutput);

        Check(lOutput.mNetwork.GetActivateHullQueue().GetLength() == 0 && lModule.mbNeedToBroadcastHullChange,
              "online STARTING_UP: no broadcast, the flag stays set (meState != E_STATE_RUNNING, 0x82728A4C)");
        Check(gGateLogs == luGates, "online STARTING_UP: the hash leg is not reached");
        Check(lOutput.mNetwork.mbActiveHullsValid, "the hull table is published in every state");
    }

    // ---- 4. online RUNNING, nothing pending, not a decision frame ------------------------------
    {
        NetFixture lModule; FakeInput lInput; FakeOutput lOutput;
        Prepare(lModule, lInput, lOutput);
        lModule.mbAllowDivergentBehaviour   = false;
        lModule.meState                     = TrafficEntityModule::E_STATE_RUNNING;
        lModule.mbNeedToBroadcastHullChange = false;
        lModule.mbDecisionFrame             = false;
        const unsigned luGates = gGateLogs;

        lModule.GenerateNetworkUpdateEvents(&lInput, &lOutput);

        Check(lOutput.mNetwork.GetActivateHullQueue().GetLength() == 0, "online, nothing pending: no broadcast");
        Check(gGateLogs == luGates, "not a decision frame: the hash leg is not reached");
    }

    Check(gAsserts == 0, "no assert fired");
    std::printf("FxNetcrashNetworkUpdate: %u checks, %u failures (%u asserts)\n", gChecks, gFailures, gAsserts);
    return gFailures ? 1 : 0;
}
