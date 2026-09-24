// FX-TRAFFIC4 item 1 (crash parity wave 5, 2026-09-24): GenerateDriverInputs' static (parked) section.
// The PRODUCTION TrafficEntityModule::GenerateDriverInputs @0x82748E78, extracted from
// src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp by
// run_fxtraffic4_static_section.py with its constants and the accessors it reaches (GetVehicle,
// GetVehicleTransform, GetTrafficPhysicsInfoForVehicl and the Vehicle predicates / manoeuvre
// setters). The manoeuvre arms, UpdateVehicleStuckTimers and TryClearupOffscreenTraffic are
// RECORDING doubles here -- they are not what is under test -- and the output buffer's driver
// interface accessor is a test double returning the buffer's own VehicleDriverInputInterface.
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   0x82749224  cmplwi r19, 0x190 ; bge loc_82749B48 -- the first index >= 400 ENDS the standard loop
//   0x82749B50  the stack set is rebuilt IN PLACE: alive & physical, after the standard arms ran
//   0x82749B88  iterator at End() -> return
//   0x82749E54  IsBitSet(iterator) in the REBUILT set: clear -> ++ first (a car demoted by a standard
//               arm is not visited; a car promoted BELOW the stop index is not visited this frame)
//   0x8274A1E8  TryClearupOffscreenTraffic(iterator) for every remaining set bit, result IGNORED
//   no driver record is ever sent for an index >= 400
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

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
        std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }             // the witness stream stays off
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

typedef BrnPhysics::Vehicle::BrnTrafficDriverControls Controls;
typedef CgsContainers::FastBitArray<BrnTraffic::VehicleSoaData::KU_MAX_VEHICLES> SoaBits;

// ---- the recording doubles' state -----------------------------------------------------------------
static std::vector<int> gClearupVisits;           // TryClearupOffscreenTraffic, in call order
static std::vector<int> gArmVehicles;             // any manoeuvre arm, in call order
static int              gClearupTrueFor = -1;     // TryClearupOffscreenTraffic returns true for this index
static void (*gpArmHook)(int) = nullptr;          // runs inside the arm of each standard car

namespace BrnTraffic
{
    bool                     TrafficDiagEnabled() { return false; }
    CgsDev::Log::DebugPrint* TrafficDiagStream()  { return nullptr; }

    struct DriverFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::mVehicleSoaData)              mVehicleSoaData;
        decltype(M::maVehicles)                   maVehicles;
        decltype(M::maVehicleTransforms)          maVehicleTransforms;
        decltype(M::maTrafficPhysicsInfoList)     maTrafficPhysicsInfoList;
        decltype(M::maTrafficPhysicsInfoListBits) maTrafficPhysicsInfoListBits;
        decltype(M::mfSimTimeStep)                mfSimTimeStep;
        decltype(M::mbPlayingShowtimeMode)        mbPlayingShowtimeMode;
        decltype(M::mbAllowDivergentBehaviour)    mbAllowDivergentBehaviour;

        Vehicle*            GetVehicle(u32 luIndex);
        Matrix44Affine      GetVehicleTransform(u32 luIndex) const;
        TrafficPhysicsInfo* GetTrafficPhysicsInfoForVehicl(u32 luVehicle);

        void UpdateVehicleStuckTimers(void*, f32, f32) {}
        bool TryClearupOffscreenTraffic(const SoaBits::Iterator& lrItVehicle)
        {
            gClearupVisits.push_back(lrItVehicle.GetIndex());
            return lrItVehicle.GetIndex() == gClearupTrueFor;
        }
        void Arm(u32 luVehicle, Controls* lpControls)
        {
            gArmVehicles.push_back(static_cast<int>(luVehicle));
            lpControls->mfGas = 0.25f;
            if (gpArmHook != nullptr)
            {
                gpArmHook(static_cast<int>(luVehicle));
            }
        }
        void UpdateStuckReverseManoeuvre(u32 v, Controls* c)                       { Arm(v, c); }
        void UpdateExtremeSwerving(u32 v, BrnTrafficIO::OutputBuffer_PrePhysics*, Controls* c) { Arm(v, c); }
        void UpdateSympatheticCrashing(u32 v, EntityId, BrnTrafficIO::OutputBuffer_PrePhysics*, Controls* c, f32) { Arm(v, c); }
        void UpdateRecoveringFromSlam(u32 v, Controls* c)                          { Arm(v, c); }
        void UpdateNormalPhysical(u32 v, Controls* c)                              { Arm(v, c); }
        void Update3PointTurnManoeuvre(u32 v, Controls* c)                         { Arm(v, c); }
        void UpdateGiveUpManoeuvre(u32 v, Controls* c)                             { Arm(v, c); }

        void GenerateDriverInputs(BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput);
    };
}

// The output buffer's write accessor: a test double (the production one only adds a write-lock assert).
BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics::VehicleDriverInputInterface*
BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics::GetVehicleDriverInterface()
{
    return &mVehicleDriverInterface;
}

// The production bodies under test.
#include "static_section.inc"

using namespace BrnTraffic;
typedef DriverFixture Fixture;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaFixture[sizeof(Fixture)];
alignas(64) static unsigned char gaOutput[sizeof(BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics)];
static Fixture& F() { return *reinterpret_cast<Fixture*>(gaFixture); }
static BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* Out() { return reinterpret_cast<BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics*>(gaOutput); }

static u32 guNextSlot = 0;

static void Fresh()
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    std::memset(gaOutput, 0, sizeof(gaOutput));
    Out()->GetVehicleDriverInterface()->GetUpdateDriverQueue()->Construct();
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        F().maVehicleTransforms[luVehicle].SetIdentity();
    }
    F().mfSimTimeStep = 1.0f / 60.0f;
    gClearupVisits.clear();
    gArmVehicles.clear();
    gClearupTrueFor = -1;
    gpArmHook = nullptr;
    guNextSlot = 0;
}

// A physical, alive car: species by index range, NORMAL reason (5) for the standard ones.
static void Physical(u32 luVehicle)
{
    Vehicle& lr = F().maVehicles[luVehicle];
    lr.mxFlags   = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL;
    lr.muSpecies = static_cast<u8>(GetVehicleSpecies(luVehicle));
    lr.miPhysicalReason = 5;
    lr.muCrashTrafficType = 2;   // eCrashTrafficType_Spontaneous: not recovering, not checked
    lr.miManoeuvre = Vehicle::E_MANOEUVRE_NONE;
    lr.miPhysicalPartsIndex = static_cast<s8>(guNextSlot);
    F().maTrafficPhysicsInfoListBits.SetBit(guNextSlot);
    F().maTrafficPhysicsInfoList[guNextSlot].muOwningVehicleIndex = static_cast<u16>(luVehicle);
    ++guNextSlot;
    F().mVehicleSoaData.mAliveVehicles.SetBit(luVehicle);
    F().mVehicleSoaData.mPhysicalVehicles.SetBit(luVehicle);
}

// The miVehicleID of every record GenerateDriverInputs posted, in order.
static std::vector<int> Records()
{
    std::vector<int> l;
    const auto* lpQueue = Out()->GetVehicleDriverInterface()->GetUpdateDriverQueue();
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 liType = lpQueue->GetFirstEvent(&lpEvent, &liSize);
    for (s32 li = 0; li < lpQueue->GetLength(); ++li)
    {
        (void)liType;
        l.push_back(reinterpret_cast<const Controls*>(lpEvent)->miVehicleID);
        if (li + 1 < lpQueue->GetLength())
        {
            liType = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }
    return l;
}

static bool Is(const std::vector<int>& a, std::initializer_list<int> b) { return a == std::vector<int>(b); }

// Hooks run inside vehicle 5's arm (a standard car demoting / promoting others mid-loop).
static void DemoteFirstStatic(int liVehicle)
{
    if (liVehicle == 5) { F().mVehicleSoaData.mPhysicalVehicles.UnSetBit(410); }
}
static void PromoteTwoStatics(int liVehicle)
{
    if (liVehicle == 5)
    {
        Physical(405);   // below the index the standard loop will stop at (410)
        Physical(450);   // above it
    }
}

int main()
{
    // ---- S1: standard cars drive, parked cars are only cleared up --------------------------------
    Fresh();
    Physical(5); Physical(7); Physical(410); Physical(520);
    F().GenerateDriverInputs(Out());
    Check(Is(gClearupVisits, { 5, 7, 410, 520 }),
          "S1 TryClearupOffscreenTraffic runs for the standard cars (0x82749234) AND for every physical parked car (0x8274A1E8)");
    Check(Is(gArmVehicles, { 5, 7 }) && Is(Records(), { 5, 7 }),
          "S2 only the standard cars reach an arm and get a driver record; the parked cars get none");

    // ---- S3: the clear-up's result is ignored in the static section ------------------------------
    Fresh();
    Physical(5); Physical(410); Physical(520);
    gClearupTrueFor = 410;
    F().GenerateDriverInputs(Out());
    Check(Is(gClearupVisits, { 5, 410, 520 }),
          "S3 a parked car the valve clears (true) does not stop the walk: the result is ignored (0x8274A1EC)");

    // ---- S4: a standard car cleared by the valve still skips its arm (unchanged first loop) ------
    Fresh();
    Physical(5); Physical(7); Physical(410);
    gClearupTrueFor = 5;
    F().GenerateDriverInputs(Out());
    Check(Is(gArmVehicles, { 7 }) && Is(Records(), { 7 }) && Is(gClearupVisits, { 5, 7, 410 }),
          "S4 in the standard loop a true clear-up `continue`s (0x8274923C): no arm, no record for that car");

    // ---- S5: the set is REBUILT after the standard loop -------------------------------------------
    Fresh();
    Physical(5); Physical(410); Physical(520);
    gpArmHook = DemoteFirstStatic;
    F().GenerateDriverInputs(Out());
    Check(Is(gClearupVisits, { 5, 520 }),
          "S5 the stop index (410) demoted by a standard arm: IsBitSet in the rebuilt set is clear, so ++ first -- 520 only");

    Fresh();
    Physical(5); Physical(410);
    gpArmHook = PromoteTwoStatics;
    F().GenerateDriverInputs(Out());
    Check(Is(gClearupVisits, { 5, 410, 450 }),
          "S6 cars promoted by a standard arm: 450 (above the stop index) is visited, 405 (below it) waits a frame");

    // ---- S7: nothing parked -> nothing past the standard loop -----------------------------------
    Fresh();
    Physical(5); Physical(7);
    F().GenerateDriverInputs(Out());
    Check(Is(gClearupVisits, { 5, 7 }) && Is(Records(), { 5, 7 }),
          "S7 no physical parked car: the walk ends at End() and the static section does nothing");

    // ---- S8: only parked cars ---------------------------------------------------------------------
    Fresh();
    Physical(430); Physical(599);
    F().GenerateDriverInputs(Out());
    Check(Is(gClearupVisits, { 430, 599 }) && Records().empty() && gArmVehicles.empty(),
          "S8 only parked / trailer cars: the first index (430) ends the standard loop at once; both are cleared up, no record");

    Check(gAsserts == 0, "A1 no assert fired");

    std::printf("FxTraffic4StaticSection: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
