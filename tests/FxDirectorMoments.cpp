// FX-DIRECTOR (crash parity 2026-09-24, the moment tick + factory): the PRODUCTION
//   MomentController::MomentHandle::Prepare  @0x821F7298
//   MomentController::UpdateAllMoments       @0x82239DE8
//   MomentController::Construct / Prepare / Destruct   (inlined by MainDirector on the console)
//   MainDirector::UpdateMoments              @0x82250268
// extracted from src/GameSource/Director/{MomentController/BrnMomentController.cpp, BrnMainDirector.cpp}
// by run_fxdirector_moments.py, compiled against the real MomentController / Moment / MomentSharedInfo /
// DebugPrinter / TimerStatusInterface / VehicleInfo types, with recording FakeMoments placed in the
// controller's REAL moment pool. A revision without a body gets a labelled empty stand-in (and fails).
//
// Checked against the ARTIST asm:
//   MomentHandle::Prepare  stw parent +0x14 ; stb 1 +0x00 ; handle +0x04..+0x10 ; the object's vtable
//                          slot 0 (Construct) ; assert mbIsAllocated ; GetMoment().Prepare(manager),
//                          asserted (:242) ; li r3, 1
//   UpdateAllMoments       f31 = info +0x51C (mfTimestep); 20 slots, allocated only (IsObjectAllocated);
//                          name via vtable +0x18, NULL -> "<NULLSTRING>"; +0x178 -> +0x20 ? ActualPrint
//                          (+0x14) ; else +0x17A && +0x17B -> ActualPrint(0xFF00FFFF) ; else +0x20 ?
//                          ActualPrint(+0x18); ValidityAccount::Print(+0x148, info +0x500); PreUpdate
//                          (stb 1 +0x178/+0x179/+0x17A, the account masked); vtable +0x08 Update(f31,
//                          manager, info)
//   UpdateMoments          lbz 0x7AC8 (sim paused) -> return; f31 = game [+8]*[+4], flt_82001CC0 (0.0)
//                          under +0x3543C; the MomentSharedInfo slots; bl UpdateAllMoments(this+0x172D0,
//                          this+0x1CB10, info)
#include "GameSource/Director/MomentController/BrnMomentController.h"
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"
#include "GameSource/Director/DirectorModule/BrnDirectorInputOutput.h"
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static std::string gLastAssert;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; gLastAssert = lpcText; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// VehicleInfo's constructor clears its RaceCarState, and its copies (BrnPlayerInfo.cpp, linked) copy
// one -- the physics TU that owns those three is not linked here. The console's copy is an XMemCpy
// of the whole 0x460-byte state (VehicleInfo::operator= @0x821F49C8).
void BrnPhysics::Vehicle::RaceCarState::Clear() { std::memset(static_cast<void*>(this), 0, sizeof(*this)); }
BrnPhysics::Vehicle::RaceCarState::RaceCarState(const RaceCarState& lrOther)
{
    std::memcpy(static_cast<void*>(this), static_cast<const void*>(&lrOther), sizeof(*this));
}
void BrnPhysics::Vehicle::RaceCarState::operator=(const RaceCarState& lrOther)
{
    std::memcpy(static_cast<void*>(this), static_cast<const void*>(&lrOther), sizeof(*this));
}

// ---- recording stand-ins for the out-of-line leaves the functions under test reach -----------------
static int gSequence = 0;
static int giMaskCalls = 0;
static int giValidityPrints = 0;
static const void* gpValidityPrinter = nullptr;
static int giBankConstructs = 0;
struct PrintRecord { std::string mText; u32 muColour; };
static std::vector<PrintRecord> gPrints;

namespace BrnDirector
{
namespace Camera
{
    void Camera::Construct() {}
    void ValidityAccount::MaskToFailFlags() { ++giMaskCalls; }
    void ValidityAccount::Print(DebugPrinter& lrDebugPrinter) const { ++giValidityPrints; gpValidityPrinter = &lrDebugPrinter; }
}
    // The recording ActualPrint: every line the moment tick hands the printer (the real one tests
    // mbEnabled itself and draws; the forwarders' own +0x20 test is what the checks below see).
    void DebugPrinter::ActualPrint(const char* lpcMessage, CgsDev::RGBA luColour)
    {
        gPrints.push_back(PrintRecord{ std::string(lpcMessage), static_cast<u32>(luColour) });
    }
    void MomentParameterBank::Construct() { ++giBankConstructs; }
    void Moment::SetParameters(const Parameters*) {}
    void Moment::Destruct() {}

    // A recording moment for the pool: every virtual the functions under test call is logged.
    class FakeMoment : public Moment
    {
    public:
        const char* mpcName = "FakeMoment";
        bool  mbPrepareResult = true;
        int   miConstructCalls = 0, miPrepareCalls = 0, miUpdateCalls = 0;
        int   miConstructOrder = 0, miPrepareOrder = 0;
        void* mpPrepareManager = nullptr;
        void* mpUpdateManager = nullptr;
        const void* mpUpdateInfo = nullptr;
        f32   mfUpdateTimestep = -1.0f;
        bool  mbFlagsAtUpdate[3] = { false, false, false };
        int   miMaskCallsAtUpdate = -1;

        void Construct() override
        {
            ++miConstructCalls;
            miConstructOrder = ++gSequence;
            Moment::Construct();
        }
        bool Prepare(void* lpBehaviourController) override
        {
            ++miPrepareCalls;
            miPrepareOrder = ++gSequence;
            mpPrepareManager = lpBehaviourController;
            SetState(E_STATE_INVALID_SEARCHING);
            return mbPrepareResult;
        }
        void Update(f32 lfTimeStep, void* lpBehaviourController, const void* lpSharedInfo) override
        {
            ++miUpdateCalls;
            mfUpdateTimestep = lfTimeStep;
            mpUpdateManager = lpBehaviourController;
            mpUpdateInfo = lpSharedInfo;
            mbFlagsAtUpdate[0] = CanSwitchToMeNow();
            mbFlagsAtUpdate[1] = CanSwitchFromMeNow();
            mbFlagsAtUpdate[2] = ConditionsAreMet();
            miMaskCallsAtUpdate = giMaskCalls;
        }
        bool Release() override { return true; }
        const char* GetName() const override { return mpcName; }
        void SetFlags(bool lbSwitchTo, bool lbSwitchFrom, bool lbConditions, bool lbInhibited)
        {
            mbCanSwitchToMeNow = lbSwitchTo;
            mbCanSwitchFromMeNow = lbSwitchFrom;
            mbConditionsMet = lbConditions;
            mbIsInhibited = lbInhibited;
        }
    protected:
        EType GetInstanceType() override { return E_MOMENT_TUMBLING; }
    };

namespace DirectorIO
{
    // Stand-in for DirectorIO::InputBuffer (forward-declared by BrnDirectorInputOutput.h): the accessor
    // surface UpdateMoments reads, over the real timer / vehicle-info / crash-info / bit-array types.
    struct InputBuffer
    {
        Camera::VehicleInfo             maRaceCarInfo[8];
        CgsContainers::BitArray<8>      mUsedRaceCars;
        CgsSystem::TimerStatusInterface mTimerStatus;
        Camera::PlayerCrashInfo         mPlayerCrashInfo;
        u8                              maContacts[64];
        bool                            mbSimPaused;

        bool IsSimPaused() const { return mbSimPaused; }
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const { return &mTimerStatus; }
        const Camera::VehicleInfo* GetRaceCarInfo() const { return maRaceCarInfo; }
        const CgsContainers::BitArray<8>* GetUsedRaceCars() const { return &mUsedRaceCars; }
        const void* GetContacts() const { return maContacts; }
        const Camera::PlayerCrashInfo* GetPlayerCrashInfo() const { return &mPlayerCrashInfo; }
    };
}

// Stand-in behaviour manager: the one accessor UpdateMoments reaches through it, over a REAL bank.
alignas(16) static u8 gaBankStorage[sizeof(Camera::BehaviourParameterBank)];
struct BehaviourManagerStandIn
{
    Camera::BehaviourParameterBank& GetBehaviourParameterBank()
    {
        return *reinterpret_cast<Camera::BehaviourParameterBank*>(gaBankStorage);
    }
};

// Stand-in moment controller: records what UpdateMoments hands UpdateAllMoments (copied out during
// the call -- the record is a stack local of UpdateMoments).
struct MomentControllerRecorder
{
    int   miCalls = 0;
    const void* mpManager = nullptr;
    f32   mfTimestep = -1.0f, mfSimTimestep = -1.0f;
    bool  mbAllowJump = false, mbAllowStunt = false, mbAllowHardStop = false;
    bool  mbForceFastTopDown = false, mbForceCollisionPolicys = true;
    const void* mapPointers[17] = {};
    EActiveRaceCarIndex mePlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    f32   mfPlayerHardestImpact = -1.0f;
    bool  mbUsedCar2 = false, mbUsedCar3 = true;

    void UpdateAllMoments(BehaviourManagerStandIn& lrManager, const MomentSharedInfo& lrInfo)
    {
        ++miCalls;
        mpManager = &lrManager;
        mfTimestep = lrInfo.mfTimestep;
        mfSimTimestep = lrInfo.mfSimTimestep;
        mbAllowJump = lrInfo.mbAllowJumpMoment;
        mbAllowStunt = lrInfo.mbAllowStuntMoment;
        mbAllowHardStop = lrInfo.mbAllowHardStopMoment;
        mbForceFastTopDown = lrInfo.mbForceNextWorldCrashToBeFastTopDown;
        mbForceCollisionPolicys = lrInfo.mbForceCollisionPolicysToStart;
        mapPointers[0]  = lrInfo.mpRandom;
        mapPointers[1]  = lrInfo.mpDebugLog;
        mapPointers[2]  = lrInfo.mpDebugPrinter;
        mapPointers[3]  = lrInfo.mpGameState;
        mapPointers[4]  = lrInfo.mpAllVehicleData;
        mapPointers[5]  = lrInfo.mpPlayerCar;
        mapPointers[6]  = lrInfo.mpRaceCars;
        mapPointers[7]  = lrInfo.mpPlayerCarTransform;
        mapPointers[8]  = lrInfo.mpBehaviourParameterBank;
        mapPointers[9]  = lrInfo.mpNamedBehaviourParams;
        mapPointers[10] = lrInfo.mpPlayerTracker;
        mapPointers[11] = lrInfo.mpContacts;
        mapPointers[12] = lrInfo.mpDirectorResourceManager;
        mapPointers[13] = lrInfo.mpShotSelector;
        mapPointers[14] = lrInfo.mpCrashAnalysis;
        mapPointers[15] = lrInfo.mpEffectInterface;
        mapPointers[16] = lrInfo.mpPlayerCrashInfo;
        mePlayerIndex = lrInfo.mePlayerActiveRaceCarIndex;
        mfPlayerHardestImpact = lrInfo.mPlayerInfo.mfHardestImpact;
        mbUsedCar2 = lrInfo.mUsedRaceCars.IsBitSet(2);
        mbUsedCar3 = lrInfo.mUsedRaceCars.IsBitSet(3);
    }
};

alignas(16) static u8 gaGameStateStorage[64];
alignas(16) static u8 gaAllVehicleDataStorage[64];
alignas(16) static u8 gaVehicleTrackerStorage[64];
}

// The revision's own EStateFlagTailByte enum (MainDirectorFlagTail), generated by the runner.
#include "director_moments_flags.inc"

namespace BrnDirector
{
// Stand-in MainDirector: the members UpdateMoments reads, BY THE PRODUCTION NAMES. The three
// sub-objects it only takes the address of are references onto distinct storage.
class MainDirector : public MainDirectorFlagTail
{
public:
    MainDirector()
        : maGameState(*reinterpret_cast<GameState*>(gaGameStateStorage))
        , mAllVehicleData(*reinterpret_cast<AllVehicleData*>(gaAllVehicleDataStorage))
        , mVehicleTracker(*reinterpret_cast<VehicleTracker*>(gaVehicleTrackerStorage))
    {
    }

    void UpdateMoments(const DirectorInputOutput* lpIO, s32 liPlayerCarIndex);

    u8                       maRandom[0x40] = {};
    u8                       maDebugLog[0x40] = {};
    u8                       maDebugPrinterB[0x40] = {};
    GameState&               maGameState;
    AllVehicleData&          mAllVehicleData;
    VehicleTracker&          mVehicleTracker;
    BehaviourManagerStandIn  mBehaviourManager;
    MomentControllerRecorder mMomentController;
    u8                       maShotSelector[0x40] = {};
    alignas(4) u8            maCrashAnalyser[0x24] = {};
    u8                       maEffectInterface[0x40] = {};
    u8                       maStateFlagTail[0x35450 - 0x35430] = {};
    bool                     mbAllowJumpMoment = false;
    bool                     mbAllowStuntMoment = false;
    bool                     mbAllowHardStopMoment = false;
    bool                     mbShowAllCameraNames = false;
};
}

// The production bodies under test (or the runner's labelled stand-ins).
#include "director_moments.inc"

using namespace BrnDirector;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

// The harness's own reset between sections, independent of the controller lifecycle under test.
static void ResetPool(MomentController& lrController)
{
    lrController.mMomentPool.mObjectPool.Clear();
}

int main()
{
    static MomentController lController;
    BehaviourManagerStandIn lManagerStandIn;
    Camera::BehaviourManager* lpManager = reinterpret_cast<Camera::BehaviourManager*>(&lManagerStandIn);

    // ---- 1. the lifecycle (MainDirector's inlined Construct / PREPARE stage 5 / Destruct) ---------
    lController.Construct();
    Check(giBankConstructs == 1, "Construct: MomentParameterBank::Construct (0x8225B554)");
    Check(lController.Prepare(), "Prepare: returns true (stage 5 has no failure branch)");
    Check(lController.mMomentPool.GetNumFreeObjects() == 20,
          "Prepare: the pool's free queue refilled, count 20 (stage 5)");
    ResetPool(lController);
    {
        AbstractPoolVoidHandle lHandle = lController.mMomentPool.AllocateVoid<FakeMoment>();
        const s32 liSlot = lHandle.miIndex;
        const bool lbAllocated = lController.mMomentPool.IsObjectAllocated(liSlot);
        lController.Destruct();
        Check(lbAllocated && !lController.mMomentPool.IsObjectAllocated(liSlot),
              "Destruct: the pool's occupancy word cleared (0x8224FCC0)");
    }
    ResetPool(lController);

    // ---- 2. MomentHandle::Prepare: Construct, then Prepare(manager), then return true -------------
    {
        AbstractPoolVoidHandle lVoid = lController.mMomentPool.AllocateVoid<FakeMoment>();
        FakeMoment* lpFake = static_cast<FakeMoment*>(lVoid.Get());
        MomentController::MomentHandle lHandle;
        lHandle.Construct();
        gSequence = 0;
        const unsigned luAssertsBefore = gAsserts;
        const bool lbResult = lHandle.Prepare(lVoid, lController, *lpManager);
        Check(lbResult, "MomentHandle::Prepare returns true (li r3, 1)");
        Check(lHandle.IsAllocated(), "MomentHandle::Prepare: mbIsAllocated = 1 (stb 1, 0(r31))");
        Check(lHandle.mpParentMomentController == &lController, "MomentHandle::Prepare: parent stored (+0x14)");
        Check(lHandle.mMomentPoolHandle.Get() == lpFake, "MomentHandle::Prepare: the pool handle stored (+0x04..+0x10)");
        Check(lpFake->miConstructCalls == 1, "MomentHandle::Prepare: the moment's vtable slot 0 (Construct) runs once");
        Check(lpFake->miPrepareCalls == 1 && lpFake->mpPrepareManager == lpManager,
              "MomentHandle::Prepare: GetMoment().Prepare(lBehaviourManager) runs once, with the manager");
        Check(lpFake->miConstructOrder == 1 && lpFake->miPrepareOrder == 2,
              "MomentHandle::Prepare: Construct BEFORE Prepare");
        Check(lpFake->GetType() == Moment::E_MOMENT_TUMBLING && lpFake->GetState() == Moment::E_STATE_INVALID_SEARCHING,
              "MomentHandle::Prepare: the moment is Constructed (type latched) and Prepared (SEARCHING)");
        Check(gAsserts == luAssertsBefore, "MomentHandle::Prepare: no assert on the good path");
        lHandle.Release();

        AbstractPoolVoidHandle lVoid2 = lController.mMomentPool.AllocateVoid<FakeMoment>();
        FakeMoment* lpFake2 = static_cast<FakeMoment*>(lVoid2.Get());
        lpFake2->mbPrepareResult = false;
        MomentController::MomentHandle lHandle2;
        lHandle2.Construct();
        const unsigned luAssertsBefore2 = gAsserts;
        const bool lbResult2 = lHandle2.Prepare(lVoid2, lController, *lpManager);
        Check(lbResult2 && gAsserts == luAssertsBefore2 + 1 && gLastAssert == "GetMoment().Prepare(lBehaviourManager)",
              "MomentHandle::Prepare: a failed moment Prepare fires the :242 assert and still returns true");
        lHandle2.Release();
    }

    // ---- 3. UpdateAllMoments ----------------------------------------------------------------------
    {
        ResetPool(lController);
        AbstractPoolVoidHandle lA = lController.mMomentPool.AllocateVoid<FakeMoment>();
        AbstractPoolVoidHandle lB = lController.mMomentPool.AllocateVoid<FakeMoment>();
        AbstractPoolVoidHandle lC = lController.mMomentPool.AllocateVoid<FakeMoment>();
        FakeMoment* lpA = static_cast<FakeMoment*>(lA.Get());
        FakeMoment* lpB = static_cast<FakeMoment*>(lB.Get());
        FakeMoment* lpC = static_cast<FakeMoment*>(lC.Get());
        lpA->mpcName = "FakeA";
        lpB->mpcName = "FakeB";
        lpC->mpcName = nullptr;
        lpA->SetFlags(true,  false, false, false);   // can switch to me           -> PrintActive
        lpC->SetFlags(false, false, true,  true);    // conditions met + inhibited -> 0xFF00FFFF
        lB.Release();                                // B's slot freed: must be skipped
        // The pool hands out slots from the top of its free queue; the name lines follow SLOT order.
        const bool lbAFirst = lA.miIndex < lC.miIndex;

        DebugPrinter lPrinter;
        std::memset(static_cast<void*>(&lPrinter), 0, sizeof(lPrinter));
        DebugPrinter::DebugPrinterInfo lInfo = lPrinter.GetDebugPrinterInfo();
        lInfo.muActiveColour   = 0xFF00FF00u;
        lInfo.muInactiveColour = 0xFF0000FFu;
        lPrinter.SetDebugPrinterInfo(lInfo);
        lPrinter.SetEnabled();

        static MomentSharedInfo lSharedInfo;
        lSharedInfo.mfTimestep = 0.0125f;
        lSharedInfo.mfSimTimestep = 0.5f;
        lSharedInfo.mpDebugPrinter = &lPrinter;

        gPrints.clear();
        giMaskCalls = 0;
        giValidityPrints = 0;
        lController.UpdateAllMoments(*lpManager, lSharedInfo);

        const PrintRecord* lpLineA = nullptr;
        const PrintRecord* lpLineC = nullptr;
        if (gPrints.size() == 2)
        {
            lpLineA = &gPrints[lbAFirst ? 0 : 1];
            lpLineC = &gPrints[lbAFirst ? 1 : 0];
        }
        Check(lpA->miUpdateCalls == 1 && lpC->miUpdateCalls == 1, "UpdateAllMoments: every allocated moment is Updated once");
        Check(lpB->miUpdateCalls == 0, "UpdateAllMoments: a freed slot is skipped (IsObjectAllocated)");
        Check(lpA->mfUpdateTimestep == 0.0125f && lpC->mfUpdateTimestep == 0.0125f,
              "UpdateAllMoments: Update gets the shared info's mfTimestep (lfs f31, 0x51C), not the sim step");
        Check(lpA->mpUpdateManager == lpManager && lpA->mpUpdateInfo == &lSharedInfo &&
              lpC->mpUpdateManager == lpManager && lpC->mpUpdateInfo == &lSharedInfo,
              "UpdateAllMoments: Update gets the manager and the shared info");
        Check(lpA->mbFlagsAtUpdate[0] && lpA->mbFlagsAtUpdate[1] && lpA->mbFlagsAtUpdate[2] &&
              lpC->mbFlagsAtUpdate[0] && lpC->mbFlagsAtUpdate[1] && lpC->mbFlagsAtUpdate[2],
              "UpdateAllMoments: PreUpdate raised +0x178/+0x179/+0x17A before Update");
        Check(giMaskCalls == 2 && lpA->miMaskCallsAtUpdate >= 1 && lpC->miMaskCallsAtUpdate >= 1 &&
              lpA->miMaskCallsAtUpdate != lpC->miMaskCallsAtUpdate,
              "UpdateAllMoments: each moment's validity account is masked (PreUpdate) right before its Update");
        Check(gPrints.size() == 2, "UpdateAllMoments: one name line per allocated moment");
        Check(lpLineA != nullptr && lpLineA->mText == "FakeA" && lpLineA->muColour == 0xFF00FF00u,
              "UpdateAllMoments: a switchable moment's name in muActiveColour (+0x14)");
        Check(lpLineC != nullptr && lpLineC->mText == "<NULLSTRING>" && lpLineC->muColour == 0xFF00FFFFu,
              "UpdateAllMoments: conditions met + inhibited -> 0xFF00FFFF; a NULL name prints <NULLSTRING>");
        Check(giValidityPrints == 2 && gpValidityPrinter == &lPrinter,
              "UpdateAllMoments: each camera validity account is printed to the moment printer (+0x500)");

        // A disabled printer: PrintActive / PrintInactive test +0x20 first; the explicit colour does not.
        lpA->SetFlags(true, false, false, false);
        lpC->SetFlags(false, false, true, true);
        lPrinter.SetDisabled();
        gPrints.clear();
        lController.UpdateAllMoments(*lpManager, lSharedInfo);
        Check(gPrints.size() == 1 && gPrints[0].muColour == 0xFF00FFFFu,
              "UpdateAllMoments: a disabled printer drops PrintActive/PrintInactive (+0x20) but not the 0xFF00FFFF line");

        // Neither switchable nor (met AND inhibited) -> muInactiveColour.
        lPrinter.SetEnabled();
        lpA->SetFlags(false, true, true, false);
        lpC->SetFlags(false, false, false, true);
        gPrints.clear();
        lController.UpdateAllMoments(*lpManager, lSharedInfo);
        Check(gPrints.size() == 2 && gPrints[0].muColour == 0xFF0000FFu && gPrints[1].muColour == 0xFF0000FFu,
              "UpdateAllMoments: otherwise muInactiveColour (+0x18)");
        Check(lpA->miUpdateCalls == 3 && lpC->miUpdateCalls == 3 && lpB->miUpdateCalls == 0,
              "UpdateAllMoments: the allocated moments are Updated every frame");
    }

    // ---- 4. MainDirector::UpdateMoments ------------------------------------------------------------
    {
        static DirectorIO::InputBuffer lInput;
        std::memset(static_cast<void*>(&lInput), 0, sizeof(lInput));
        lInput.mTimerStatus.mGameTimerStatus.mfBaseTimeStep       = 1.0f / 30.0f;
        lInput.mTimerStatus.mGameTimerStatus.mfTimeStepMultiplier = 1.0f;
        lInput.mTimerStatus.mSimTimerStatus.mfBaseTimeStep        = 1.0f / 30.0f;
        lInput.mTimerStatus.mSimTimerStatus.mfTimeStepMultiplier  = 0.5f;
        lInput.mUsedRaceCars.SetBit(2);
        lInput.maRaceCarInfo[2].mfHardestImpact = 12.5f;
        lInput.maRaceCarInfo[1].mfHardestImpact = 99.0f;
        DirectorResourceManager* lpResource = reinterpret_cast<DirectorResourceManager*>(&lInput.maContacts[8]);
        DirectorInputOutput lIO;
        std::memset(static_cast<void*>(&lIO), 0, sizeof(lIO));
        lIO.mpInputBuffer = &lInput;
        lIO.mpResourceManager = lpResource;

        static MainDirector lDirector;
        lDirector.mbAllowJumpMoment     = false;
        lDirector.mbAllowStuntMoment    = true;
        lDirector.mbAllowHardStopMoment = true;
        lDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_FORCE_NEXT_WORLD_CRASH_FAST_TOP_DOWN] = 1;

        lInput.mbSimPaused = true;
        lDirector.UpdateMoments(&lIO, 2);
        Check(lDirector.mMomentController.miCalls == 0, "UpdateMoments: a paused sim updates no moment (lbz 0x7AC8 ; bne)");

        lInput.mbSimPaused = false;
        lDirector.UpdateMoments(&lIO, 2);
        const MomentControllerRecorder& lrRec = lDirector.mMomentController;
        Check(lrRec.miCalls == 1 && lrRec.mpManager == &lDirector.mBehaviourManager,
              "UpdateMoments: UpdateAllMoments(controller, the behaviour manager, info) once");
        Check(std::fabs(lrRec.mfTimestep - 1.0f / 30.0f) < 1e-7f, "UpdateMoments: mfTimestep = the game step ([+8]*[+4])");
        Check(std::fabs(lrRec.mfSimTimestep - 1.0f / 60.0f) < 1e-7f, "UpdateMoments: mfSimTimestep = the sim step ([+0x20]*[+0x1C])");
        Check(!lrRec.mbAllowJump && lrRec.mbAllowStunt && lrRec.mbAllowHardStop,
              "UpdateMoments: the three allow flags (+0x3542C..+0x3542E)");
        Check(lrRec.mbForceFastTopDown && !lrRec.mbForceCollisionPolicys,
              "UpdateMoments: mbForceNextWorldCrashToBeFastTopDown (+0x35437) and a false mbForceCollisionPolicysToStart");
        Check(lrRec.mePlayerIndex == E_ACTIVE_RACE_CAR_INDEX_2 && lrRec.mfPlayerHardestImpact == 12.5f &&
              lrRec.mbUsedCar2 && !lrRec.mbUsedCar3,
              "UpdateMoments: the player's index, a copy of ITS VehicleInfo (mulli 0x4F0), and the used-car bits");
        Check(lrRec.mapPointers[0] == lDirector.maRandom && lrRec.mapPointers[1] == lDirector.maDebugLog &&
              lrRec.mapPointers[2] == lDirector.maDebugPrinterB && lrRec.mapPointers[3] == &lDirector.maGameState &&
              lrRec.mapPointers[4] == &lDirector.mAllVehicleData,
              "UpdateMoments: random / debug log / the MOMENT printer (+0x3378C) / game state / all-vehicle data");
        Check(lrRec.mapPointers[5] == &lInput.maRaceCarInfo[2] && lrRec.mapPointers[6] == lInput.maRaceCarInfo &&
              lrRec.mapPointers[7] == &lInput.maRaceCarInfo[2].mRaceCarState.mTransform,
              "UpdateMoments: the player car, the race-car array, the player transform (+0x1F0)");
        Check(lrRec.mapPointers[8] == gaBankStorage &&
              lrRec.mapPointers[9] == &lDirector.mBehaviourManager.GetBehaviourParameterBank().GetNamedParameters(),
              "UpdateMoments: the behaviour parameter bank and its named parameters (+0x2F040 / +0x2F050)");
        Check(lrRec.mapPointers[10] == &lDirector.mVehicleTracker && lrRec.mapPointers[11] == lInput.maContacts &&
              lrRec.mapPointers[12] == lpResource,
              "UpdateMoments: the player tracker (+0x339E0), the contacts, the resource manager (lwz 8(r30))");
        Check(lrRec.mapPointers[13] == lDirector.maShotSelector && lrRec.mapPointers[14] == lDirector.maCrashAnalyser &&
              lrRec.mapPointers[15] == lDirector.maEffectInterface && lrRec.mapPointers[16] == &lInput.mPlayerCrashInfo,
              "UpdateMoments: the shot selector (+0x121F0), the crash analysis (+0x1245C), the effect interface, the crash info");

        lDirector.maStateFlagTail[MainDirector::E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP] = 1;
        lDirector.UpdateMoments(&lIO, 2);
        Check(lrRec.miCalls == 2 && lrRec.mfTimestep == 0.0f && std::fabs(lrRec.mfSimTimestep - 1.0f / 60.0f) < 1e-7f,
              "UpdateMoments: mbDebugZeroTimestep (+0x3543C) zeroes the game step only (flt_82001CC0 == 0.0)");
    }

    Check(gAsserts == 1, "exactly the one intended assert fired");

    std::printf("FxDirectorMoments: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
