// FX-TRAFFIC2 (crash parity 2026-09-24, CHAIN-STOMPEES parts a + b): the PRODUCTION
//   TrafficEntityModule::GeneratePotentialLeapedAndStompedCarsOutput      @0x8271F298
//   TrafficEntityModule::GetFirstUnusedShowtimeVehicleInfo (DWARF :1872, inlined there)
//   OutputBuffer_PreScene::AddPotentialScoree                             @0x8271D2E8
//   TrafficToRaceCarInterface_PreScene::AddPotentialStompee               @0x82706028
//   TrafficToRaceCarInterface_PreScene::GetPotentialStompees / ClearStompees (DWARF :122 / :126)
//   CrashModeScoring::GetVehicleScoreData (static)                        @0x82312AB0
// extracted from the b5 sources by run_fxtraffic2_stompees.py and hosted on a fixture that has
// the module's real member types. Every expected value below is derived from the ARTIST asm
// (the decode is in the producer's banner in BrnTrafficEntityModule.cpp):
//   ClearStompees + muShowtimeVehicleInfoCount = 0, then only in Showtime (+0x717DD);
//   ground height 10.0 (flt_820BA5E4) unless the above-ground test is valid; ground point drops
//   by it; mis-bounce timer += dt below 4.0 (flt_820BA8DC), else 0;
//   t = SolveQuadratic(-4.905 (flt_820BD6C0), v.y, h): min root, else max root, negative -> 0
//   and the ring stays at -1; else d = |vXZ| t, landing = pos + vXZ t, ring
//   [fsel(d - 20 >= 0 ? d - 20 : 0)^2, (d + 20)^2] (20 = flt_820BA7E4; NaN d gives a 0 minimum);
//   per live car: a Showtime slot (32 max), landing-time position pos + v t, XZ distances;
//   stompee iff on screen (+0x28460) and the LANDING distance is inside the ring (both ends
//   inclusive, and NaN is inside: blt / bgt skips); slot bit 0 iff in the ring and PHYSICAL & ALIVE; slot bit 1 (crash
//   magnet) iff the crash slider FACTOR (+0x72378) > 1.5 (flt_820BA5DC) and the CURRENT
//   distance^2 is not > 2500 (flt_820BA858, `bgt` skip: NaN passes) and on screen; the slot is kept iff a bit is set;
//   the debug view draws spheres of 20 and sqrt(2500) on the player (0x46006400 / 0x46006464);
//   the SCORE leg (on screen and d^2 < 45^2): GetVehicleScoreData(class +3, asset CgsID) then
//   AddPotentialScoree(pos with y + the class's height tweak, d^2, score, multiplier, (u16)vehicle).
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Numeric/CgsPolynomial.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "rw/math/fpu/scalar_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstddef>
#include <limits>
#include <cstdio>
#include <cstring>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static const char* gpcLastAssert = "";

// GetVehicleScoreData's unknown-type diagnostic un-compresses the id; not under test.
void CgsIDUnCompress(CgsID, char* lpcString) { lpcString[0] = 0; }

// ---- the debug view: recorders ----------------------------------------------------------
struct SphereCall { Vector3 mCentre; f32 mfRadius; u32 muColour; };
static std::vector<SphereCall> gSpheres;
static unsigned guDebugInterfaces = 0, guDebugReleases = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        gpcLastAssert = lpcMessage;
        std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }

alignas(16) static unsigned char gaRenderStorage[sizeof(DebugRender)];

DebugInterface::DebugInterface() : mpDebugManager(nullptr), mbIsAutomaticClass(true) { ++guDebugInterfaces; }
DebugInterface::~DebugInterface() { ++guDebugReleases; }
DebugRender& DebugInterface::GetRender() { return *reinterpret_cast<DebugRender*>(gaRenderStorage); }
void DebugRender::DrawSphere(Vector3 lv3Centre, f32 lfRadius, RGBA lColour)
{
    gSpheres.push_back(SphereCall{ lv3Centre, lfRadius, static_cast<u32>(lColour) });
}
}

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    // The committed bodies (BrnRCEntityActiveRaceCarOutputInterface.cpp) without their asserts --
    // they are inputs here, not under test.
    const RCEntityActiveRaceCarOutputInterface::RaceCarState*
    RCEntityActiveRaceCarOutputInterface::GetPlayerRaceCarState() const
    {
        return &maRaceCarStates[mePlayerActiveRaceCarIndex];
    }
    Vector3 RCEntityActiveRaceCarOutputInterface::GetPlayerPosition() const
    {
        return maRaceCarStates[mePlayerActiveRaceCarIndex].mTransform.Pos();
    }
}
}

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // The lock-checked accessors (0x82710BD8 / 0x82710DD0) without the lock bookkeeping.
    const InputBuffer_PreScene::ActiveRaceCarOutputInterface*
    InputBuffer_PreScene::GetActiveRaceCarOutputInterface() const
    {
        return &mActiveRaceCarOutputInterface;
    }
    OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene*
    OutputBuffer_PreScene::GetTrafficToRaceCarInterface_PreScene()
    {
        return &mTrafficToRaceCarInterface_PreScene;
    }
}

    // The TU-local helpers the production body uses (BrnTrafficEntityModule.cpp anonymous namespace).
    static unsigned guBlockedScoreLegLogs = 0;
    inline void LogMissingLeg_T1(bool&, const char*) { ++guBlockedScoreLegLogs; }
    inline CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }
    const u8 KU_SHOWTIME_INFO_FLAG_CRASH_MAGNET = 0x02u;

    struct StompFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::mbPlayingShowtimeMode)          mbPlayingShowtimeMode;
        decltype(M::mfSimTimeStep)                  mfSimTimeStep;
        decltype(M::mfCrashSliderCrashScoreFactor)  mfCrashSliderCrashScoreFactor;
        decltype(M::mfCrashSliderFinalValue)        mfCrashSliderFinalValue;
        decltype(M::maShowtimeVehicleInfoList)      maShowtimeVehicleInfoList;
        decltype(M::muShowtimeVehicleInfoCount)     muShowtimeVehicleInfoCount;
        decltype(M::mShowtimePlayerLandingPos2D)    mShowtimePlayerLandingPos2D;
        decltype(M::mShowtimePlayerGroundPos)       mShowtimePlayerGroundPos;
        decltype(M::mfShowtimeMisBounceTimer)       mfShowtimeMisBounceTimer;
        decltype(M::mbDEBUGShowtimeStuff)           mbDEBUGShowtimeStuff;
        decltype(M::maVehicles)                     maVehicles;
        decltype(M::maVehicleTransforms)            maVehicleTransforms;
        decltype(M::mVehicleSoaData)                mVehicleSoaData;
        decltype(M::mpData)                         mpData;

        ShowtimeVehicleInfo* GetFirstUnusedShowtimeVehicleInfo(u32& luInfoIndex);
        void GeneratePotentialLeapedAndStompedCarsOutput(const BrnTrafficIO::InputBuffer_PreScene* lpInput,
                                                         BrnTrafficIO::OutputBuffer_PreScene* lpOutput);
    };
}

// The production bodies under test.
#include "stompees.inc"

using namespace BrnTraffic;
using namespace BrnTraffic::BrnTrafficIO;
typedef StompFixture Fixture;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static bool Near(f32 lfA, f32 lfB, f32 lfTolerance = 1.0e-3f)
{
    return std::fabs(lfA - lfB) <= lfTolerance;
}

static bool NearVec(const Vector3& lrA, f32 lfX, f32 lfY, f32 lfZ, f32 lfTolerance = 1.0e-3f)
{
    return Near(lrA.x, lfX, lfTolerance) && Near(lrA.y, lfY, lfTolerance) && Near(lrA.z, lfZ, lfTolerance);
}

alignas(64) static unsigned char gaFixture[sizeof(Fixture)];
alignas(64) static unsigned char gaInput[sizeof(InputBuffer_PreScene)];
alignas(64) static unsigned char gaOutput[sizeof(OutputBuffer_PreScene)];
alignas(64) static unsigned char gaTrafficData[sizeof(TrafficData)];
static VehicleTypeData gaVehicleTypes[1];
static VehicleAsset    gaVehicleAssets[1];
// Every fixture car is vehicle type 0: a VAN (class 1) whose asset is the first
// TARGETVEHICLE row of GetVehicleScoreData's table (0xBF2E42A8A7700000: 6000 points, multiplier 1).
static const CgsID KID_TARGET_VEHICLE = 0xBF2E42A8A7700000ULL;

static Fixture& F()        { return *reinterpret_cast<Fixture*>(gaFixture); }
static InputBuffer_PreScene&  In()  { return *reinterpret_cast<InputBuffer_PreScene*>(gaInput); }
static OutputBuffer_PreScene& Out() { return *reinterpret_cast<OutputBuffer_PreScene*>(gaOutput); }
static TrafficToRaceCarInterface_PreScene& Iface() { return Out().mTrafficToRaceCarInterface_PreScene; }
static BrnPhysics::Vehicle::RaceCarState& Player()
{
    return In().mActiveRaceCarOutputInterface.maRaceCarStates[0];
}

static const f32 KF_DT = 1.0f / 30.0f;

// The player: at (100, 20, 200), velocity (30, vy, 40) -> |vXZ| == 50.
static void Fresh(bool lbShowtime, bool lbValidGround, f32 lfHeight, f32 lfVelocityY)
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    std::memset(gaInput, 0, sizeof(gaInput));
    std::memset(gaOutput, 0, sizeof(gaOutput));
    Fixture& lr = F();
    lr.mbPlayingShowtimeMode         = lbShowtime;
    lr.mfSimTimeStep                 = KF_DT;
    lr.mfCrashSliderCrashScoreFactor = 1.5f;     // after-spike factor: no crash magnets
    lr.mfCrashSliderFinalValue       = 1.0f;
    lr.mfShowtimeMisBounceTimer      = 1.25f;
    lr.mShowtimePlayerLandingPos2D   = { -7.0f, -7.0f, -7.0f, -7.0f };   // sentinels
    lr.mShowtimePlayerGroundPos      = { -9.0f, -9.0f, -9.0f, -9.0f };
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        lr.maVehicleTransforms[luVehicle].SetIdentity();
        lr.maVehicleTransforms[luVehicle].wAxis = { 9000.0f, 0.0f, 9000.0f, 1.0f };
    }
    In().mActiveRaceCarOutputInterface.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    In().mActiveRaceCarOutputInterface.mbIsPlayerCarActive        = true;
    Player().mTransform.SetIdentity();
    Player().mTransform.wAxis                        = { 100.0f, 20.0f, 200.0f, 1.0f };
    Player().mLinearVelocity                         = { 30.0f, lfVelocityY, 40.0f, 0.0f };
    Player().mAboveGroundTestResult.mbValid          = lbValidGround;
    Player().mAboveGroundTestResult.mfVerticalDistance = lfHeight;
    Out().mPotentialScorees.Construct();
    // Stale records: ClearStompees resets the count only.
    Iface().miPotentialStompeeCount = 5;
    Iface().mPotentialStompees[0].mfDistanceSquared = 77.0f;
    lr.muShowtimeVehicleInfoCount = 7;
    std::memset(gaTrafficData, 0, sizeof(gaTrafficData));
    std::memset(gaVehicleTypes, 0, sizeof(gaVehicleTypes));
    gaVehicleTypes[0].muVehicleClass = static_cast<u8>(E_VEHICLECLASS_VAN);
    gaVehicleTypes[0].muAssetId      = 0;
    gaVehicleAssets[0].SetVehicleId(KID_TARGET_VEHICLE);
    reinterpret_cast<TrafficData*>(gaTrafficData)->mpaVehicleTypes  = gaVehicleTypes;
    reinterpret_cast<TrafficData*>(gaTrafficData)->mpaVehicleAssets = gaVehicleAssets;
    lr.mpData.mpResourceMemory = gaTrafficData;
    gSpheres.clear();
    guDebugInterfaces = guDebugReleases = 0;
    guBlockedScoreLegLogs = 0;
}

static void Car(u32 luVehicle, f32 lfX, f32 lfZ, f32 lfVX, f32 lfVZ, bool lbOnScreen, bool lbPhysical,
                bool lbAlive = true)
{
    Fixture& lr = F();
    lr.maVehicleTransforms[luVehicle].wAxis = { lfX, 0.0f, lfZ, 1.0f };
    lr.maVehicles[luVehicle].mLinearVelocity = { lfVX, 0.0f, lfVZ, 0.0f };
    lr.maVehicles[luVehicle].mxFlags = static_cast<u8>((lbAlive ? Vehicle::E_FLAG_ALIVE : 0)
                                                     | (lbPhysical ? Vehicle::E_FLAG_PHYSICAL : 0));
    if (lbOnScreen)
    {
        lr.mVehicleSoaData.mVehiclesRenderedLastFrame.SetBit(luVehicle);
    }
}

static void Run()
{
    F().GeneratePotentialLeapedAndStompedCarsOutput(&In(), &Out());
}

static s32 NumStompees()
{
    s32 liCount = -99;
    const VehicleStompingData* lpStompees = Iface().GetPotentialStompees(&liCount);
    (void)lpStompees;
    return liCount;
}

static bool StompeeIs(s32 liSlot, u32 luVehicle, f32 lfX, f32 lfZ, f32 lfDistSq)
{
    s32 liCount = 0;
    const VehicleStompingData* lpStompees = Iface().GetPotentialStompees(&liCount);
    if (liSlot >= liCount)
    {
        return false;
    }
    const VehicleStompingData& lr = lpStompees[liSlot];
    return lr.mStompeeEntityId.muValue == ((luVehicle << 10) | 0x02000000u)
        && NearVec(lr.mStompeePosition, lfX, 0.0f, lfZ)
        && Near(lr.mfDistanceSquared, lfDistSq, 0.05f);
}

int main()
{
    // ================= part (b): the interface accessors =================
    {
        Fresh(false, false, 0.0f, 0.0f);
        Iface().miPotentialStompeeCount = 3;
        s32 liCount = -1;
        const VehicleStompingData* lpStompees = Iface().GetPotentialStompees(&liCount);
        Check(lpStompees == &Iface().mPotentialStompees[0] && liCount == 3,
              "GetPotentialStompees (:122) returns mPotentialStompees (+0x40) and writes miPotentialStompeeCount (+0x208)");
        Check(reinterpret_cast<const unsigned char*>(lpStompees) - reinterpret_cast<const unsigned char*>(&Iface()) == 64,
              "the stompee records sit at +0x40, where ProcessLeapedAndStompedCars @0x822BD638 reads them");
        Check(sizeof(VehicleStompingData) == 32 && offsetof(VehicleStompingData, mStompeeEntityId) == 16,
              "record stride 32, EntityId at +0x10 (0x822BD660 `lwz r6, 0x10(r11)`)");
        Iface().ClearStompees();
        Check(Iface().miPotentialStompeeCount == 0 && Iface().mPotentialStompees[0].mfDistanceSquared == 77.0f,
              "ClearStompees (:126) zeroes the count only (0x8271F2E4 `stw 0, 0x208`), the records stay");
    }

    // ================= not Showtime =================
    {
        Fresh(false, true, 19.62f, 0.0f);
        Car(1, 160.0f, 280.0f, 0.0f, 0.0f, true, true);
        Run();
        Check(NumStompees() == 0 && F().muShowtimeVehicleInfoCount == 0,
              "outside Showtime: the stompees and the Showtime list are still cleared (0x8271F2E4 / 0x8271F2F4)");
        Check(NearVec(F().mShowtimePlayerGroundPos, -9.0f, -9.0f, -9.0f) && Near(F().mfShowtimeMisBounceTimer, 1.25f)
              && NearVec(F().mShowtimePlayerLandingPos2D, -7.0f, -7.0f, -7.0f),
              "outside Showtime nothing else is touched (0x8271F2F8 `beq` -> epilogue)");
    }

    // ================= Showtime, airborne 19.62 m above a valid ground, vy = 0 =================
    // t^2 = 19.62 / 4.905 = 4 -> roots -2 / +2; min is negative -> max: t = 2. d = 50 * 2 = 100,
    // landing (160, 20, 280), ring [80^2, 120^2] = [6400, 14400]. Player at (100, 20, 200).
    {
        Fresh(true, true, 19.62f, 0.0f);
        Car(1, 160.0f, 280.0f, 0.0f, 0.0f, true, false);   // landing d^2 10000: stompee, not physical
        Car(2, 100.0f, 290.0f, 0.0f, -10.0f, true, true);  // NOW 8100 (in ring) but at t: z 270 -> 4900: OUT
        Car(3, 100.0f, 100.0f, 0.0f, 0.0f, false, true);   // 10000, OFF screen: no stompee, but bit 0
        Car(4, 130.0f, 200.0f, 35.0f, 0.0f, true, true);   // now 900; at t x 200 -> 10000: stompee + bit 0
        Car(5, 150.0f, 200.0f, 0.0f, 0.0f, true, true, false);   // dead: skipped outright
        Car(6, 100.0f, 320.0f, 0.0f, 0.0f, true, false);   // 14400 == max: IN (bgt)
        Car(7, 180.0f, 200.0f, 0.0f, 0.0f, true, false);   // 6400 == min: IN (blt)
        Car(8, 100.0f, 320.5f, 0.0f, 0.0f, true, false);   // 14520.25: OUT
        Run();

        Check(NearVec(F().mShowtimePlayerGroundPos, 100.0f, 0.38f, 200.0f),
              "valid ground: the ground point is the player dropped by +0x1E0 (0x8271F38C..0x8271F394)");
        Check(Near(F().mfShowtimeMisBounceTimer, 0.0f),
              "height 19.62 >= 4.0 (flt_820BA8DC): the mis-bounce timer is reset (0x8271F3C4)");
        Check(NearVec(F().mShowtimePlayerLandingPos2D, 160.0f, 20.0f, 280.0f),
              "landing point = pos + vXZ * t, t = the positive root 2.0 (0x8271F570 / 0x8271F57C)");
        Check(NumStompees() == 4, "four stompees: cars 1, 4, 6, 7");
        Check(StompeeIs(0, 1, 160.0f, 280.0f, 10000.0f), "stompee 0 = car 1, landing-time position, d^2 10000");
        Check(StompeeIs(1, 4, 200.0f, 200.0f, 10000.0f),
              "stompee 1 = car 4 at its LANDING-time position (vmaddfp128 v127 = v*t + pos @0x8271F718)");
        Check(StompeeIs(2, 6, 100.0f, 320.0f, 14400.0f), "the ring's outer edge is inclusive (0x8271F924 `bgt`)");
        Check(StompeeIs(3, 7, 180.0f, 200.0f, 6400.0f), "the ring's inner edge is inclusive (0x8271F91C `blt`)");
        Check(F().muShowtimeVehicleInfoCount == 2
              && F().maShowtimeVehicleInfoList[0].muVehicleIndex == 3 && F().maShowtimeVehicleInfoList[0].muFlags == 1
              && F().maShowtimeVehicleInfoList[1].muVehicleIndex == 4 && F().maShowtimeVehicleInfoList[1].muFlags == 1,
              "Showtime slots kept only when flagged: cars 3 and 4 (physical, alive, in the ring -> bit 0)");
        Check(guBlockedScoreLegLogs == 0 && Out().mPotentialScorees.GetCount() == 1,
              "the score leg publishes the on-screen car within 45 m only (car 4: d^2 900 < 2025), no stub log");
        {
            const VehicleScoreData& lrScoree = Out().mPotentialScorees.maElements[0];
            Check(lrScoree.muVehicleIndex == 4 && Near(lrScoree.mfDistanceSquared, 900.0f)
                  && lrScoree.miScore == 6000 && lrScoree.miMultiplier == 1
                  && NearVec(lrScoree.mPosition, 130.0f, 3.2f, 200.0f),
                  "the scoree: GetVehicleScoreData(VAN, TARGETVEHICLE id) = 6000 x1, position y + 3.2 "
                  "(KF_SCORE_HEIGHT_TWEAK_BY_VEHICLE_CLASS[VAN], vrlimi128 lane y), d^2 900, index 4");
        }
        Check(gSpheres.empty() && guDebugInterfaces == 0, "no debug view unless mbDEBUGShowtimeStuff (+0x72874)");
    }

    // ================= crash magnets: the FACTOR (+0x72378), not the final value =================
    {
        Fresh(true, true, 19.62f, 0.0f);
        F().mfCrashSliderCrashScoreFactor = 10.0f;   // a crash spike (UpdateCrashSlider stores 10.0)
        F().mfCrashSliderFinalValue       = 0.0f;
        Car(4, 130.0f, 200.0f, 35.0f, 0.0f, true, true);    // 900 <= 2500, on screen: bits 0 + 1
        Car(9, 150.0f, 200.0f, 0.0f, 0.0f, true, false);    // 2500 exactly: IN (bgt) -> bit 1 only
        Car(10, 100.0f, 250.5f, 0.0f, 0.0f, true, false);   // 2550.25: out
        Car(11, 110.0f, 200.0f, 0.0f, 0.0f, false, false);  // 100 but OFF screen: no bit
        Run();
        Check(F().muShowtimeVehicleInfoCount == 2
              && F().maShowtimeVehicleInfoList[0].muVehicleIndex == 4 && F().maShowtimeVehicleInfoList[0].muFlags == 3
              && F().maShowtimeVehicleInfoList[1].muVehicleIndex == 9 && F().maShowtimeVehicleInfoList[1].muFlags == 2,
              "factor 10 > 1.5 (flt_820BA5DC): on-screen cars within 2500 (inclusive) are crash magnets (bit 1)");

        Fresh(true, true, 19.62f, 0.0f);
        F().mfCrashSliderCrashScoreFactor = 1.5f;    // after the spike: 1.5 is NOT > 1.5
        F().mfCrashSliderFinalValue       = 1.0f;
        Car(9, 150.0f, 200.0f, 0.0f, 0.0f, true, false);
        Run();
        Check(F().muShowtimeVehicleInfoCount == 0,
              "factor 1.5 (after a spike): no crash magnet whatever mfCrashSliderFinalValue says (`ble`)");
    }

    // ================= no valid ground test: height 10.0 =================
    // t = sqrt(10 / 4.905) = 1.427843, d = 71.39215, ring [51.39215^2, 91.39215^2].
    {
        Fresh(true, false, 123.0f, 0.0f);
        Car(1, 100.0f, 271.0f, 0.0f, 0.0f, true, false);   // 71^2 = 5041: in
        Car(2, 100.0f, 250.0f, 0.0f, 0.0f, true, false);   // 50^2 = 2500 < 2641.15: out
        Run();
        const f32 lfT = std::sqrt(10.0f / 4.905f);
        Check(NearVec(F().mShowtimePlayerGroundPos, 100.0f, 20.0f, 200.0f)
              && Near(F().mfShowtimeMisBounceTimer, 1.25f),
              "invalid ground: the ground point is the car itself and the mis-bounce timer is left alone (0x8271F3D4)");
        Check(NearVec(F().mShowtimePlayerLandingPos2D, 100.0f + 30.0f * lfT, 20.0f, 200.0f + 40.0f * lfT),
              "invalid ground: the solve uses the 10.0 default height (flt_820BA5E4)");
        Check(NumStompees() == 1 && StompeeIs(0, 1, 100.0f, 271.0f, 5041.0f),
              "ring from d = 50 * sqrt(10 / 4.905): 71 m is in, 50 m is out");
    }

    // ================= low ground: the mis-bounce timer runs =================
    {
        Fresh(true, true, 2.0f, 0.0f);
        Run();
        Check(Near(F().mfShowtimeMisBounceTimer, 1.25f + KF_DT, 1.0e-6f),
              "height 2.0 < 4.0: mis-bounce timer += mfSimTimeStep (0x8271F3AC..0x8271F3B8)");
    }

    // ================= both roots negative: t = 0, landing = here, empty ring =================
    // h = -1, vy = -10: roots -1.933 / -0.105.
    {
        Fresh(true, true, -1.0f, -10.0f);
        Car(1, 100.0f, 200.0f, 0.0f, 0.0f, true, true);    // on top of the player: d^2 0
        Run();
        Check(NearVec(F().mShowtimePlayerLandingPos2D, 100.0f, 20.0f, 200.0f),
              "negative landing time: the landing point is the car (0x8271F584..0x8271F594)");
        Check(NumStompees() == 0 && F().muShowtimeVehicleInfoCount == 0,
              "negative landing time: the ring stays [-1, -1], so nothing is a stompee or flagged");
    }

    // ================= no real root: SolveQuadratic's mask is false =================
    // h = -1, vy = 0: b^2 - 4ac = -19.62 < 0.
    {
        Fresh(true, true, -1.0f, 0.0f);
        Car(1, 100.0f, 200.0f, 0.0f, 0.0f, true, true);
        Run();
        Check(NearVec(F().mShowtimePlayerLandingPos2D, -7.0f, -7.0f, -7.0f),
              "negative discriminant: the landing point is not written (0x8271F4C8 `bne` past the block)");
        Check(NumStompees() == 0, "negative discriminant: no stompee (t = 0, ring [-1, -1])");
    }

    // ================= the 32-slot cap and the 8-stompee buffer =================
    {
        Fresh(true, true, 19.62f, 0.0f);
        for (u32 luCar = 0; luCar < 40; ++luCar)
        {
            // 40 physical on-screen cars inside the landing ring [80, 120], nearer first:
            // d = 81 + car / 2.
            Car(20 + luCar, 100.0f, 200.0f + 81.0f + 0.5f * static_cast<f32>(luCar), 0.0f, 0.0f, true, true);
        }
        Run();
        Check(F().muShowtimeVehicleInfoCount == 32 && F().maShowtimeVehicleInfoList[31].muVehicleIndex == 51,
              "the Showtime list stops at KU_MAX_SHOWTIME_TRAFFIC_VEHICLES (32): GetFirstUnused -> NULL (0x8271F6A0)");
        Check(NumStompees() == 8 && StompeeIs(0, 20, 100.0f, 281.0f, 6561.0f) && StompeeIs(7, 27, 100.0f, 284.5f, 7140.25f),
              "the stompee buffer keeps the 8 nearest (AddPotentialStompee's full regime keeps nearer records)");
    }

    // ================= the debug view =================
    {
        Fresh(true, true, 19.62f, 0.0f);
        F().mbDEBUGShowtimeStuff = true;
        Run();
        Check(guDebugInterfaces == 1 && guDebugReleases == 1 && gSpheres.size() == 2,
              "debug view: one automatic DebugInterface, two DrawSphere calls");
        Check(gSpheres.size() == 2 && NearVec(gSpheres[0].mCentre, 100.0f, 20.0f, 200.0f) && gSpheres[0].mfRadius == 20.0f
              && gSpheres[0].muColour == 0x46006400u && gSpheres[1].mfRadius == 50.0f && gSpheres[1].muColour == 0x46006464u,
              "debug spheres: player position, radius 20 (flt_820BA7E4) / sqrt(2500), colours 0x46006400 / 0x46006464");
    }

    // ================= NaN polarity: after fcmpu, blt/bgt are NOT taken on an unordered compare,
    // ================= ble/bge ARE (ble == "not greater", bge == "not less") =================
    {
        const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();

        // A car at a NaN position during a crash spike.
        Fresh(true, true, 19.62f, 0.0f);
        F().mfCrashSliderCrashScoreFactor = 10.0f;
        Car(12, lfNaN, 200.0f, 0.0f, 0.0f, true, true);
        Run();
        Check(NumStompees() == 1
              && Iface().mPotentialStompees[0].mStompeeEntityId.muValue == ((12u << 10) | 0x02000000u),
              "NaN landing distance is INSIDE the ring: 0x8271F91C `blt` / 0x8271F924 `bgt` skips not taken");
        Check(F().muShowtimeVehicleInfoCount == 1 && F().maShowtimeVehicleInfoList[0].muVehicleIndex == 12
              && F().maShowtimeVehicleInfoList[0].muFlags == 3,
              "... bit 0 (in the ring, physical, alive) and bit 1: the magnet distance's 0x8271F998 `bgt` skip is not taken on NaN");
        Check(Out().mPotentialScorees.GetCount() == 0,
              "... but no scoree: the score radius skip 0x8271F848 `bge` IS taken on NaN");

        // A NaN travel distance (the player's x velocity is NaN; t = 2 from the height alone): the
        // ring minimum is fsel's 0 (0x8271F574, NaN >= 0 is false), the maximum NaN, so any
        // on-screen car at a finite distance is inside.
        Fresh(true, true, 19.62f, 0.0f);
        Player().mLinearVelocity.x = lfNaN;
        Car(13, 100.0f, 210.0f, 0.0f, 0.0f, true, false);   // 10 m from the player: d^2 100
        const unsigned luAssertsBeforeNaN = gAsserts;
        Run();
        Check(gAsserts == luAssertsBeforeNaN + 1 && std::strcmp(gpcLastAssert, "RwMath::IsValid( lVector )") == 0,
              "NaN travel distance: Magnitude2D's IsValid tripwire fires once and the producer carries on (non-gating)");
        gAsserts = luAssertsBeforeNaN;
        Check(NumStompees() == 1 && StompeeIs(0, 13, 100.0f, 210.0f, 100.0f),
              "NaN travel distance: ring [fsel 0, NaN] holds a car 10 m away (`blt` 100 < 0 no, `bgt` 100 > NaN no)");

        // AddPotentialScoree, full: 0x8271D3B0 `ble found` is taken on NaN, so a NaN distance takes
        // the FIRST slot.
        Fresh(false, false, 0.0f, 0.0f);
        const Vector3 lPos = { 1.0f, 2.0f, 3.0f, 0.0f };
        for (u32 luIndex = 0; luIndex < 20; ++luIndex)
        {
            Out().AddPotentialScoree(lPos, 400.0f + 10.0f * static_cast<f32>(luIndex), 1000, 0, static_cast<u16>(luIndex));
        }
        Out().AddPotentialScoree(lPos, lfNaN, 1000, 0, 55);
        Check(Out().mPotentialScorees.GetCount() == 20 && Out().mPotentialScorees.maElements[0].muVehicleIndex == 55
              && Out().mPotentialScorees.maElements[1].muVehicleIndex == 1,
              "AddPotentialScoree full: a NaN distance overwrites slot 0 (`ble` taken on an unordered compare)");
    }

    // ================= AddPotentialScoree @0x8271D2E8 =================
    {
        Fresh(false, false, 0.0f, 0.0f);
        ScoringVehicleArray& lrScorees = Out().mPotentialScorees;
        const Vector3 lPos = { 1.0f, 2.0f, 3.0f, 0.0f };
        Out().AddPotentialScoree(lPos, 400.0f, 1250, 1, 17);
        Check(lrScorees.GetCount() == 1 && lrScorees.maElements[0].mfDistanceSquared == 400.0f
              && NearVec(lrScorees.maElements[0].mPosition, 1.0f, 2.0f, 3.0f)
              && lrScorees.maElements[0].miScore == 1250 && lrScorees.maElements[0].miMultiplier == 1
              && lrScorees.maElements[0].muVehicleIndex == 17,
              "AddPotentialScoree appends (Grow) and stores dist/pos/multiplier/index/score (0x8271D3EC..0x8271D404)");
        for (u32 luIndex = 1; luIndex < 20; ++luIndex)
        {
            Out().AddPotentialScoree(lPos, 400.0f + 10.0f * static_cast<f32>(luIndex), 1000, 0, static_cast<u16>(luIndex));
        }
        Check(lrScorees.GetCount() == 20, "twenty fill the array (KI_MAX_CAR_SCORES_TO_SHOW)");
        Out().AddPotentialScoree(lPos, 455.0f, 2000, 0, 99);
        Check(lrScorees.GetCount() == 20 && lrScorees.maElements[6].muVehicleIndex == 99
              && lrScorees.maElements[5].muVehicleIndex == 5,
              "full: the FIRST stored car at least as far (460 >= 455) is overwritten, nothing shifts (`ble`)");
        Out().AddPotentialScoree(lPos, 470.0f, 2000, 0, 98);
        Check(lrScorees.maElements[7].muVehicleIndex == 98,
              "full: an equal distance counts as 'at least as far' (470 <= 470 -> overwrite)");
        Out().AddPotentialScoree(lPos, 9999.0f, 2000, 0, 97);
        bool lbFound97 = false;
        for (u32 luIndex = 0; luIndex < 20; ++luIndex) { lbFound97 |= (lrScorees.maElements[luIndex].muVehicleIndex == 97); }
        Check(!lbFound97, "full: a car farther than every stored one is dropped");
        const unsigned luAssertsBefore = gAsserts;
        Fresh(false, false, 0.0f, 0.0f);
        Out().AddPotentialScoree(lPos, 1.0f, 40000, 0, 1);
        Check(gAsserts == luAssertsBefore + 1
              && std::strcmp(gpcLastAssert, "(int32_t) lpData->miScore == liVehicleScore") == 0,
              "a score that does not survive the s16 store fires the console's assert (0x8271D408)");
        gAsserts = luAssertsBefore;
    }

    Check(gAsserts == 0, "no assert fires in the producer scenarios");
    std::printf("FxTraffic2Stompees: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
