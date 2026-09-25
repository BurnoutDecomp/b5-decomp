// crash parity FX-NETCRASH (2026-09-25), online hull set piece 3: prediction and the incoming hull syncs.
// run_fxnetcrash_hull_prediction.py extracts the PRODUCTION bodies of
//   TrafficEntityModule::UpdateRaceCarHulls @0x82721460        (both arms)
//   TrafficEntityModule::PredictHullChanges @0x827348E8
//   TrafficEntityModule::AddPredictedHullChange @0x82734680
//   TrafficEntityModule::DEBUGDumpHullPredictions @0x827211B0
//   TrafficEntityModule::HandleIncomingNetworkData @0x82741AF8
// and compiles them as members of PredFixture. PredFixture carries the module members they reach with
// the real members' types, including the real Array<HullChangeInfo,400>. The following are linked
// production code: the Pvs grid (BrnTrafficPvs.cpp) and the network input interface
// (BrnTrafficNetworkInputInterface.cpp). Expected values come from the runner (prediction_cases.inc):
// the fused prediction case, and the expected sim-box hull list. Both are computed from the console's
// formulas, independent of the reconstruction.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "SharedClasses/Traffic/BrnTrafficPvs.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

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

// The [nettraf] hull-predict / hull-in witnesses (b5 ca4ac341). The real declaration is included
// above, so this silent definition must keep its signature.
namespace BrnNetHarnessPC
{
    void WitnessTag(const char*, const char*, const char*, ...) {}
}

namespace BrnTraffic
{
namespace
{
    inline void LogMissingLeg_T1(bool& lrbAlreadyLogged, const char*) { lrbAlreadyLogged = true; }
    CgsDev::Log::DebugPrint* NetCrashDiagStream() { return nullptr; }
    const s32 KI_NETCRASH_HULL_DIAG_MAX_LINES = 48;
}

    struct FakeRaceCarState
    {
        struct { Vector3 xAxis, yAxis, zAxis, wAxis; } mTransform;
        Vector3 mLinearVelocity;
    };

    struct FakeActiveRaceCars
    {
        bool                mbPlayerActive;
        EActiveRaceCarIndex mePlayer;
        FakeRaceCarState    maStates[E_ACTIVE_RACE_CAR_INDEX_COUNT];

        bool IsPlayerCarActive() const { return mbPlayerActive; }
        EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return mePlayer; }
        const FakeRaceCarState* GetRaceCarState(EActiveRaceCarIndex leIndex) const { return &maStates[leIndex]; }
    };

    struct FakePostPhysicsInput
    {
        FakeActiveRaceCars mCars;
        const FakeActiveRaceCars* GetActiveRaceCarOutputInterface() const { return &mCars; }
    };
    struct FakeOutput {};

    struct FakePreSceneInput
    {
        BrnTrafficIO::TrafficNetworkInputInterface mNetwork;
        const BrnTrafficIO::TrafficNetworkInputInterface* GetTrafficNetworkInputInterface() const { return &mNetwork; }
    };

    struct FakeData { Pvs* mpPvs; };
    struct FakeDataPtr
    {
        FakeData* mp;
        FakeData* operator->() { return mp; }
    };

    struct PredFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::mbAllowDivergentBehaviour)       mbAllowDivergentBehaviour;
        decltype(M::mbIsOnlineGameMode)              mbIsOnlineGameMode;
        decltype(M::mbNeedToBroadcastHullChange)     mbNeedToBroadcastHullChange;
        decltype(M::muCurrentlyPredictedHull)        muCurrentlyPredictedHull;
        decltype(M::mHullChangeToBroadcast)          mHullChangeToBroadcast;
        decltype(M::muUpdateCount)                   muUpdateCount;
        decltype(M::maPredictedHullChanges)          maPredictedHullChanges;
        decltype(M::maaRaceCarHulls)                 maaRaceCarHulls;
        decltype(M::mbHullSyncDivergence)            mbHullSyncDivergence;
        decltype(M::mbNetworkHasDetectedDivergence)  mbNetworkHasDetectedDivergence;
        decltype(M::meLocalPlayerIndex)              meLocalPlayerIndex;
        decltype(M::mfTrafficSimRadius)              mfTrafficSimRadius;
        decltype(M::mbInOfflineCarSelect)            mbInOfflineCarSelect;
        FakeDataPtr                                  mpData;

        bool IsDecisionFrame() { return true; }
        bool IsPlayingOnlineGameMode() const { return mbIsOnlineGameMode; }

        void UpdateRaceCarHulls(const FakePostPhysicsInput* lpInput);
        void PredictHullChanges(const FakePostPhysicsInput* lpInput, FakeOutput* lpOutput);
        void AddPredictedHullChange(const HullChangeInfo& lrInfo);
        void DEBUGDumpHullPredictions();
        void HandleIncomingNetworkData(const FakePreSceneInput* lpInput);
    };

#include "prediction_bodies.inc"
}

using namespace BrnTraffic;

#include "prediction_cases.inc"

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

// The grid: KU_GRID x KU_GRID cells of KF_CELL metres from (KF_GRID_MIN, KF_GRID_MIN) (prediction_cases.inc);
// cell c's PVS is { c ^ 1 }.
static Set<u16, 8> gaCellSets[KU_GRID * KU_GRID];
static Pvs         gPvs;
static FakeData    gData;

static HullChangeInfo Info(s32 liArc, u16 luHull, u16 luFrame)
{
    HullChangeInfo lInfo;
    lInfo.meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(liArc);
    lInfo.muNewActiveHull      = luHull;
    lInfo.muUpdateFrame        = luFrame;
    return lInfo;
}

static void Fresh(PredFixture& lrM, bool lbOnline)
{
    std::memset(&lrM, 0, sizeof(lrM));
    lrM.mbAllowDivergentBehaviour = !lbOnline;
    lrM.mbIsOnlineGameMode        = lbOnline;
    lrM.muCurrentlyPredictedHull  = KU_INVALID_HULL;
    lrM.meLocalPlayerIndex        = E_ACTIVE_RACE_CAR_INDEX_0;
    lrM.mfTrafficSimRadius        = VecFloat{ 195.0f, 195.0f, 195.0f, 195.0f };
    lrM.maPredictedHullChanges.Construct();
    for (u32 luSlot = 0; luSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luSlot)
    {
        lrM.maaRaceCarHulls[luSlot].Construct();
    }
    lrM.mpData.mp = &gData;
}

static bool Holds(const PredFixture& lrM, u32 luIndex, s32 liArc, u16 luHull, u16 luFrame)
{
    return luIndex < lrM.maPredictedHullChanges.GetLength()
        && lrM.maPredictedHullChanges[luIndex].meActiveRaceCarIndex == liArc
        && lrM.maPredictedHullChanges[luIndex].muNewActiveHull == luHull
        && lrM.maPredictedHullChanges[luIndex].muUpdateFrame == luFrame;
}

int main()
{
    for (u32 luCell = 0; luCell < KU_GRID * KU_GRID; ++luCell)
    {
        gaCellSets[luCell].Construct();
        gaCellSets[luCell].Insert(static_cast<u16>(luCell ^ 1u));
    }
    gPvs.mGridMin       = Vector3{ KF_GRID_MIN, 0.0f, KF_GRID_MIN, 0.0f };
    gPvs.mCellSize      = Vector3{ KF_CELL, 1.0f, KF_CELL, 0.0f };
    gPvs.mRecipCellSize = Vector3{ 1.0f / KF_CELL, 1.0f, 1.0f / KF_CELL, 0.0f };
    gPvs.muNumCells_X   = KU_GRID;
    gPvs.muNumCells_Z   = KU_GRID;
    gPvs.muNumCells     = KU_GRID * KU_GRID;
    gPvs.mpaHullPvs     = gaCellSets;
    gData.mpPvs         = &gPvs;

    FakeOutput lOutput;

    // ---- AddPredictedHullChange -------------------------------------------------------------------
    {
        PredFixture lM; Fresh(lM, true);
        lM.muUpdateCount = 100;
        lM.AddPredictedHullChange(Info(1, 12, 150));
        Check(Holds(lM, 0, 1, 12, 150) && !lM.mbHullSyncDivergence, "Add: a future change is appended, no divergence");
        lM.AddPredictedHullChange(Info(2, 13, 90));
        Check(Holds(lM, 1, 2, 13, 90) && lM.mbHullSyncDivergence,
              "Add: a HISTORICAL change ((u16)(90 - 100) >= 0x7FFF) latches mbHullSyncDivergence and is still appended");

        PredFixture lW; Fresh(lW, true);
        lW.muUpdateCount = 0xFFF0;
        lW.AddPredictedHullChange(Info(1, 12, 0x0010));
        Check(!lW.mbHullSyncDivergence, "Add: the frame test wraps (0x0010 is 0x20 ahead of 0xFFF0)");
    }
    {
        // A full buffer: 400 entries, frames 1000 + i except slot 7 = 950 (the smallest) -- and no
        // entry is in the past, so the smallest frame is evicted.
        PredFixture lM; Fresh(lM, true);
        lM.muUpdateCount = 900;
        for (u32 luI = 0; luI < 400; ++luI)
        {
            lM.maPredictedHullChanges.Append(Info(1, 5, static_cast<u16>(luI == 7 ? 950 : 1000 + luI)));
        }
        lM.AddPredictedHullChange(Info(3, 77, 1500));
        Check(lM.maPredictedHullChanges.GetLength() == 400 && lM.mbHullSyncDivergence,
              "Add full: stays at 400, divergence latched (ran out of room)");
        Check(Holds(lM, 7, 1, 5, 1399) && Holds(lM, 399, 3, 77, 1500),
              "Add full: the smallest frame (slot 7) is evicted by EraseFast (the last entry moves in) and the new one appended");

        // Same, but slot 3 is already in the past (frame 800 < 900): it goes first, even though slot 7 is smaller still.
        PredFixture lP; Fresh(lP, true);
        lP.muUpdateCount = 900;
        for (u32 luI = 0; luI < 400; ++luI)
        {
            lP.maPredictedHullChanges.Append(Info(1, 5, static_cast<u16>(luI == 3 ? 800 : (luI == 7 ? 700 : 1000 + luI))));
        }
        lP.mbHullSyncDivergence = true;
        lP.AddPredictedHullChange(Info(3, 78, 1500));
        Check(Holds(lP, 3, 1, 5, 1399) && Holds(lP, 7, 1, 5, 700),
              "Add full: the FIRST entry already in the past (slot 3) is evicted, not the smallest (slot 7)");
    }

    // ---- PredictHullChanges -------------------------------------------------------------------------
    {
        PredFixture lM; Fresh(lM, true);
        lM.muUpdateCount = 200;
        FakePostPhysicsInput lIn; std::memset(&lIn, 0, sizeof(lIn));
        lIn.mCars.mbPlayerActive = true;
        lIn.mCars.mePlayer = E_ACTIVE_RACE_CAR_INDEX_2;
        lIn.mCars.maStates[2].mTransform.wAxis = Vector3{ FromBits(KU_PRED_POS_X), 0.0f, FromBits(KU_PRED_POS_Z), 1.0f };
        lIn.mCars.maStates[2].mLinearVelocity  = Vector3{ FromBits(KU_PRED_VEL_X), 0.0f, FromBits(KU_PRED_VEL_Z), 0.0f };
        lM.PredictHullChanges(&lIn, &lOutput);
        Check(lM.maPredictedHullChanges.GetLength() == 1 && Holds(lM, 0, 2, KU_PRED_HULL_FUSED, 250),
              "Predict: the FUSED position*5 prediction picks the hull, scheduled muUpdateCount + 50");
        Check(lM.muCurrentlyPredictedHull == KU_PRED_HULL_FUSED, "Predict: muCurrentlyPredictedHull = the new hull");
        Check(lM.mbNeedToBroadcastHullChange && lM.mHullChangeToBroadcast.meActiveRaceCarIndex == 2
              && lM.mHullChangeToBroadcast.muNewActiveHull == KU_PRED_HULL_FUSED && lM.mHullChangeToBroadcast.muUpdateFrame == 250,
              "Predict: the same record is flagged for broadcast (stdx +0x558E0)");

        lM.mbNeedToBroadcastHullChange = false;
        lM.PredictHullChanges(&lIn, &lOutput);
        Check(lM.maPredictedHullChanges.GetLength() == 1 && !lM.mbNeedToBroadcastHullChange,
              "Predict: an unchanged prediction adds nothing");

        PredFixture lOff; Fresh(lOff, true);
        FakePostPhysicsInput lNone; std::memset(&lNone, 0, sizeof(lNone));
        lOff.PredictHullChanges(&lNone, &lOutput);
        Check(lOff.maPredictedHullChanges.GetLength() == 0, "Predict: no player car, no prediction");
    }

    // ---- HandleIncomingNetworkData ---------------------------------------------------------------------
    {
        PredFixture lM; Fresh(lM, true);
        lM.muUpdateCount = 300;
        FakePreSceneInput lIn;
        lIn.mNetwork.Construct();
        lIn.mNetwork.ActivateHull(E_ACTIVE_RACE_CAR_INDEX_1, 40, 330);
        lIn.mNetwork.ActivateHull(E_ACTIVE_RACE_CAR_INDEX_3, 41, 331);
        lIn.mNetwork.SetDiverged(true);
        lM.HandleIncomingNetworkData(&lIn);
        Check(lM.mbNetworkHasDetectedDivergence, "Incoming online: the network's divergence verdict is copied (stbx +0x72B54)");
        Check(lM.maPredictedHullChanges.GetLength() == 2 && Holds(lM, 0, 1, 40, 330) && Holds(lM, 1, 3, 41, 331),
              "Incoming online: every peer change becomes a predicted change, in queue order");

        PredFixture lOff; Fresh(lOff, false);
        lOff.HandleIncomingNetworkData(&lIn);
        Check(!lOff.mbNetworkHasDetectedDivergence && lOff.maPredictedHullChanges.GetLength() == 0,
              "Incoming offline: nothing (lbzx +0x717E7 gate)");
    }

    // ---- UpdateRaceCarHulls, ONLINE arm -----------------------------------------------------------------
    {
        PredFixture lM; Fresh(lM, true);
        lM.muUpdateCount = 500;
        lM.maaRaceCarHulls[1].Append(33);                        // replaced when its change is due
        lM.maaRaceCarHulls[2].Append(44);                        // untouched (its change is not due)
        lM.maPredictedHullChanges.Append(Info(1, 6, 500));       // due now
        lM.maPredictedHullChanges.Append(Info(2, 7, 503));       // in the future
        lM.maPredictedHullChanges.Append(Info(3, 8, 498));       // historical
        FakePostPhysicsInput lIn; std::memset(&lIn, 0, sizeof(lIn));
        lM.UpdateRaceCarHulls(&lIn);
        Check(lM.maaRaceCarHulls[1].GetLength() == 2 && lM.maaRaceCarHulls[1][0] == 6 && lM.maaRaceCarHulls[1][1] == (6 ^ 1),
              "Online: the due change replaces slot 1's hulls with {hull, its PVS}");
        Check(lM.maaRaceCarHulls[2].GetLength() == 1 && lM.maaRaceCarHulls[2][0] == 44, "Online: a future change is not applied");
        Check(lM.mbHullSyncDivergence, "Online: a historical change latches mbHullSyncDivergence");
        Check(lM.maPredictedHullChanges.GetLength() == 1 && Holds(lM, 0, 2, 7, 503),
              "Online: the applied and the historical changes are erased, the future one stays");
    }

    // ---- UpdateRaceCarHulls, OFFLINE arm (moved verbatim from RecalculateActiveHulls) --------------------
    {
        PredFixture lM; Fresh(lM, false);
        lM.maaRaceCarHulls[5].Append(9);                         // every slot is cleared first
        FakePostPhysicsInput lIn; std::memset(&lIn, 0, sizeof(lIn));
        lIn.mCars.mbPlayerActive = true;
        lIn.mCars.mePlayer = E_ACTIVE_RACE_CAR_INDEX_0;
        lIn.mCars.maStates[0].mTransform.wAxis = Vector3{ KF_BOX_X, 0.0f, KF_BOX_Z, 1.0f };
        lM.UpdateRaceCarHulls(&lIn);
        bool lbSame = lM.maaRaceCarHulls[0].GetLength() == KU_BOX_COUNT;
        for (u32 luI = 0; lbSame && luI < KU_BOX_COUNT; ++luI)
        {
            lbSame = lM.maaRaceCarHulls[0][luI] == KA_BOX_HULLS[luI];
        }
        Check(lbSame, "Offline: the sim box's cells, X outer / Z inner, inclusive");
        Check(lM.maaRaceCarHulls[5].GetLength() == 0, "Offline: every slot was cleared");
    }

    Check(gAsserts == 0, "no assert fired");
    std::printf("FxNetcrashHullPrediction: %u checks, %u failures (%u asserts)\n", gChecks, gFailures, gAsserts);
    return gFailures ? 1 : 0;
}
