// FX-TRAFFIC3 item 4 (crash parity wave 5, 2026-09-24): the PRODUCTION TrafficEntityModule::
// GenerateNearbyParkedTrafficOutput @0x8271FA18 and its constants, extracted from
// src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp by
// run_fxtraffic3_parked_traffic.py together with every body it reaches (GetStaticVehicle,
// GetStaticTrafficParam, GetVehicleIndexFromStaticIndex, GetHull, Vehicle::IsAlarmOn / IsCrashing,
// StaticTrafficParam::GetHull / GetIndexInHull, the two IO getters, the player position / direction
// accessors and BrnMath::GetPointToInfiniteLineDistance). BrnWorld::CheckVehicleForPowerPark is the
// production header inline; TrafficToRaceCarInterface_PreScene::Set/GetNearbyParkedTrafficData are
// the production header inlines.
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   0x8271FA38  mbPlayerIsPowerParking (+0x717E5) == 0 -> return (0x8271FA40), NOTHING written
//   0x8271FA58  four FLT_MAX seeds (flt_820BA23C == 0x7F7FFFFF)
//   0x8271FAAC  maVehicles[400 + i] alive (+5 & 1), no alarm (+7 & 0x10), not IsCrashing
//   0x8271FB18  the hull's StaticTrafficVehicle record muFlags (+0x43) bit 0 set -> skip (bit 1 is not tested)
//   0x8271FB90  CheckVehicleForPowerPark(player Pos, player At, vehicle row 3, vehicle row 2, &closest,
//               &second, &angle, &perp) in index order; true -> ++count
//   0x8271FBC4  +0x20C count, +0x210 closest, +0x214 second, +0x218 angle, +0x21C perp
//   CheckVehicleForPowerPark @0x822B1FA0: counted iff x*x + z*z <= 225 (flt_82018E3C)
//
// Built twice by the runner. The second build (/DFXTRAFFIC3_PARKED_DRYRUN) sets BRN_PARKED_DRYRUN
// before the first call and checks the PC-only diagnostic never publishes and never asserts; the
// console's paths are the first build's.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
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

namespace BrnTraffic
{
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }

    struct ParkedFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::mbPlayerIsPowerParking) mbPlayerIsPowerParking;
        decltype(M::maVehicles)             maVehicles;
        decltype(M::maVehicleTransforms)    maVehicleTransforms;
        decltype(M::maStaticTrafficParams)  maStaticTrafficParams;
        decltype(M::mpData)                 mpData;

        Vehicle*            GetStaticVehicle(u32 luIndex);
        StaticTrafficParam* GetStaticTrafficParam(u32 luIndex);
        u32                 GetVehicleIndexFromStaticIndex(u32 luStaticVehicle);
        const Hull*         GetHull(u32 luIndex) const;

        void GenerateNearbyParkedTrafficOutput(const BrnTrafficIO::InputBuffer_PreScene* lpInput,
                                               BrnTrafficIO::OutputBuffer_PreScene* lpOutput);
    };
}

// The production bodies under test.
#include "parked_traffic.inc"

using namespace BrnTraffic;
typedef ParkedFixture Fixture;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaFixture[sizeof(Fixture)];   // raw: no ResourcePtr ctor/dtor
alignas(64) static unsigned char gaInput[sizeof(BrnTrafficIO::InputBuffer_PreScene)];
alignas(64) static unsigned char gaOutput[sizeof(BrnTrafficIO::OutputBuffer_PreScene)];
alignas(64) static unsigned char gaTrafficData[sizeof(TrafficData)];
alignas(64) static unsigned char gaHull[sizeof(Hull)];
static StaticTrafficVehicle gaRecords[KU_MAX_STATIC_TRAFFIC];
static Hull* gapHulls[1];

static Fixture& F() { return *reinterpret_cast<Fixture*>(gaFixture); }
static BrnTrafficIO::InputBuffer_PreScene& In() { return *reinterpret_cast<BrnTrafficIO::InputBuffer_PreScene*>(gaInput); }
static BrnTrafficIO::OutputBuffer_PreScene& Out() { return *reinterpret_cast<BrnTrafficIO::OutputBuffer_PreScene*>(gaOutput); }
static BrnTrafficIO::TrafficToRaceCarInterface_PreScene& Iface() { return Out().mTrafficToRaceCarInterface_PreScene; }

static const Vector3 KV_PLAYER     = { 100.0f, 0.0f, 200.0f, 1.0f };
static const Vector3 KV_PLAYER_DIR = { 0.0f, 0.0f, 1.0f, 0.0f };

static const u8 KU8_NOT_CRASHING = 0xFFu;   // any crash type but 0 (IsCrashing reads 0 as crashing)

// Place static car luIndex (global 400 + luIndex) at KV_PLAYER + offset, heading lDir.
static void Park(u32 luIndex, f32 lfDX, f32 lfDZ, Vector3 lDir, u8 luFlags, u8 luEffect = 0,
                 u8 luCrashType = KU8_NOT_CRASHING, u8 luRecordFlags = 0)
{
    Fixture& lr = F();
    Vehicle& lrVehicle = lr.maVehicles[400 + luIndex];
    lrVehicle.mxFlags            = luFlags;
    lrVehicle.mxEffectState      = luEffect;
    lrVehicle.muCrashTrafficType = luCrashType;
    lrVehicle.muSpecies          = Vehicle::E_SPECIES_STATIC;
    Matrix44Affine& lrTransform = lr.maVehicleTransforms[400 + luIndex];
    lrTransform.SetIdentity();
    lrTransform.zAxis = lDir;
    lrTransform.xAxis = { lDir.z, 0.0f, -lDir.x, 0.0f };
    lrTransform.wAxis = { KV_PLAYER.x + lfDX, 0.0f, KV_PLAYER.z + lfDZ, 1.0f };
    lr.maStaticTrafficParams[luIndex].mxFlags = StaticTrafficParam::E_FLAG_ALIVE;
    lr.maStaticTrafficParams[luIndex].muHull = 0;
    lr.maStaticTrafficParams[luIndex].muStaticTrafficIndexOnHull = static_cast<u8>(luIndex);
    gaRecords[luIndex].muFlags = luRecordFlags;
}

static void Fresh(bool lbPowerParking)
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    std::memset(gaInput, 0, sizeof(gaInput));
    std::memset(gaOutput, 0, sizeof(gaOutput));
    std::memset(gaTrafficData, 0, sizeof(gaTrafficData));
    std::memset(gaHull, 0, sizeof(gaHull));
    std::memset(gaRecords, 0, sizeof(gaRecords));

    Hull& lrHull = *reinterpret_cast<Hull*>(gaHull);
    lrHull.muNumStaticTraffic = static_cast<u8>(KU_MAX_STATIC_TRAFFIC);
    lrHull.mpaStaticTrafficVehicles = gaRecords;
    gapHulls[0] = &lrHull;
    TrafficData& lrData = *reinterpret_cast<TrafficData*>(gaTrafficData);
    lrData.muNumHulls = 1;
    lrData.mpapHulls  = gapHulls;
    F().mpData.mpResourceMemory = gaTrafficData;
    F().mbPlayerIsPowerParking  = lbPowerParking;

    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        F().maVehicleTransforms[luVehicle].SetIdentity();
        F().maVehicles[luVehicle].muCrashTrafficType = KU8_NOT_CRASHING;
    }

    In().mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForRead);
    Out().mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForWrite);
    BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lrCars = In().mActiveRaceCarOutputInterface;
    lrCars.mePlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
    lrCars.mbIsPlayerCarActive = true;
    lrCars.maRaceCarStates[0].mTransform.SetIdentity();
    lrCars.maRaceCarStates[0].mTransform.zAxis = KV_PLAYER_DIR;
    lrCars.maRaceCarStates[0].mTransform.wAxis = KV_PLAYER;

    // Sentinels: what a skipped publish must leave behind.
    Iface().SetNearbyParkedTrafficData(77u, -1.0f, -2.0f, -3.0f, -4.0f);
}

static void Scene()
{
    const Vector3 lAhead = { 0.0f, 0.0f, 1.0f, 0.0f };
    const Vector3 lSide  = { 0.6f, 0.0f, 0.8f, 0.0f };
    Park(0, 5.0f, 0.0f, lAhead, Vehicle::E_FLAG_ALIVE);                                   // d^2 25
    Park(1, 0.0f, 3.0f, lSide,  Vehicle::E_FLAG_ALIVE);                                   // d^2 9 (closest)
    Park(2, 1.0f, 0.0f, lAhead, Vehicle::E_FLAG_ALIVE, 0x10);                             // alarm on
    Park(3, 2.0f, 0.0f, lAhead, Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL, 0, 0);  // crashing
    Park(4, 0.0f, 2.0f, lAhead, Vehicle::E_FLAG_ALIVE, 0, KU8_NOT_CRASHING, 0x01);        // divergent-only
    Park(5, 0.0f, 1.0f, lAhead, 0);                                                       // dead
    Park(6, 20.0f, 0.0f, lAhead, Vehicle::E_FLAG_ALIVE);                                  // d^2 400 > 225
    Park(7, 0.0f, -4.0f, lAhead, Vehicle::E_FLAG_ALIVE, 0, KU8_NOT_CRASHING, 0x02);       // showtime-only rec, d^2 16
}

struct Published { u32 muCount; f32 mfClosest, mfSecond, mfAngle, mfPerp; };

static Published Read()
{
    Published l;
    Iface().GetNearbyParkedTrafficData(&l.muCount, &l.mfClosest, &l.mfSecond, &l.mfAngle, &l.mfPerp);
    return l;
}

#ifndef FXTRAFFIC3_PARKED_DRYRUN
int main()
{
    // ---- gate closed: nothing written --------------------------------------------------------
    Fresh(false);
    Scene();
    F().GenerateNearbyParkedTrafficOutput(&In(), &Out());
    {
        const Published l = Read();
        Check(l.muCount == 77u && l.mfClosest == -1.0f && l.mfSecond == -2.0f && l.mfAngle == -3.0f && l.mfPerp == -4.0f,
              "G1 mbPlayerIsPowerParking false: the interface is left untouched (0x8271FA40 beq out)");
    }

    // ---- gate open: the scene ------------------------------------------------------------------
    Fresh(true);
    Scene();
    F().GenerateNearbyParkedTrafficOutput(&In(), &Out());
    const Published lGot = Read();

    // The oracle: the console's aggregation, in index order, over the cars the ARTIST filters keep.
    f32 lfClosest = FLT_MAX, lfSecond = FLT_MAX, lfAngle = FLT_MAX, lfPerp = FLT_MAX;
    u32 luCount = 0;
    const u32 kauKept[] = { 0, 1, 6, 7 };
    for (u32 luKept : kauKept)
    {
        const Matrix44Affine& lr = F().maVehicleTransforms[400 + luKept];
        if (BrnWorld::CheckVehicleForPowerPark(KV_PLAYER, KV_PLAYER_DIR, lr.Pos(), lr.At(),
                                               lfClosest, lfSecond, lfAngle, lfPerp))
        {
            ++luCount;
        }
    }

    Check(lGot.muCount == 3u, "P1 three parked cars count: 0 (25), 1 (9), 7 (16); 6 is outside 15 m");
    Check(lGot.mfClosest == 9.0f, "P2 the closest squared distance is car 1's 9 (alarm / crashing / divergent-only / dead nearer cars skipped)");
    Check(lGot.mfSecond == 16.0f, "P3 the second-closest is car 7's 16: a showtime-only record (bit 1) is NOT skipped");
    Check(lGot.muCount == luCount && lGot.mfClosest == lfClosest && lGot.mfSecond == lfSecond,
          "P4 the count and distances equal CheckVehicleForPowerPark over the kept cars in index order");
    Check(lGot.mfAngle == lfAngle && lGot.mfPerp == lfPerp,
          "P5 angle / perpendicular come from (player Pos, player At, vehicle row 3, vehicle row 2) -- the console's v1..v4");
    Check(lGot.mfAngle != FLT_MAX && lGot.mfPerp != FLT_MAX, "P6 the closest car refreshed the angle and perpendicular seeds");

    // The same scene without car 2's alarm: it becomes the closest (d^2 1).
    Fresh(true);
    Scene();
    F().maVehicles[400 + 2].mxEffectState = 0;
    F().GenerateNearbyParkedTrafficOutput(&In(), &Out());
    Check(Read().muCount == 4u && Read().mfClosest == 1.0f, "P7 without its alarm car 2 counts and is the closest");

    // The divergent-only record bit alone decides car 4.
    Fresh(true);
    Scene();
    gaRecords[4].muFlags = 0x02;
    F().GenerateNearbyParkedTrafficOutput(&In(), &Out());
    Check(Read().muCount == 4u && Read().mfClosest == 4.0f, "P8 with record bit 0 clear car 4 counts (d^2 4)");

    // ---- gate open, nothing eligible: the seeds are published ----------------------------------
    Fresh(true);
    F().GenerateNearbyParkedTrafficOutput(&In(), &Out());
    {
        const Published l = Read();
        Check(l.muCount == 0u && l.mfClosest == FLT_MAX && l.mfSecond == FLT_MAX && l.mfAngle == FLT_MAX && l.mfPerp == FLT_MAX,
              "P9 no parked car: count 0 and the four FLT_MAX seeds (flt_820BA23C) are published");
    }

    // ---- the publish lands on the console seats (+524 .. +540) ---------------------------------
    Fresh(true);
    Scene();
    F().GenerateNearbyParkedTrafficOutput(&In(), &Out());
    Check(Iface().muNearbyStaticVehicleCount == 3u && Iface().mfClosestDistanceSq == 9.0f
          && Iface().mfSecondClosestDistanceSq == 16.0f && Iface().mfClosestAngleDiff == lfAngle
          && Iface().mfClosestPerpendicularDist == lfPerp,
          "P10 SetNearbyParkedTrafficData stores count / closest / second / angle / perp in member order");

    Check(gAsserts == 0, "A1 no assert fired on any path");

    std::printf("FxTraffic3ParkedTraffic: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
#else
// [DIAG] BRN_PARKED_DRYRUN (NOT IN THE X360 BINARY): a closed gate measures without publishing.
int main()
{
    _putenv_s("BRN_PARKED_DRYRUN", "1");   // before the first call: ParkedDryRunEnabled caches it

    Fresh(false);
    Scene();
    F().GenerateNearbyParkedTrafficOutput(&In(), &Out());
    {
        const Published l = Read();
        Check(l.muCount == 77u && l.mfClosest == -1.0f && l.mfSecond == -2.0f && l.mfAngle == -3.0f && l.mfPerp == -4.0f,
              "D1 closed gate, dry run, player active: the loop runs but the interface is left untouched");
    }

    Fresh(false);
    Scene();
    In().mActiveRaceCarOutputInterface.mbIsPlayerCarActive = false;
    F().GenerateNearbyParkedTrafficOutput(&In(), &Out());
    Check(Read().muCount == 77u && gAsserts == 0,
          "D2 closed gate, dry run, player car inactive: no loop, so no GetPlayerPosition assert, nothing written");

    Fresh(true);
    Scene();
    F().GenerateNearbyParkedTrafficOutput(&In(), &Out());
    Check(Read().muCount == 3u && Read().mfClosest == 9.0f && Read().mfSecond == 16.0f,
          "D3 an open gate publishes as the console does with the dry-run variable set");

    Check(gAsserts == 0, "D4 no assert fired on any dry-run path");

    std::printf("FxTraffic3ParkedTrafficDryRun: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
#endif
