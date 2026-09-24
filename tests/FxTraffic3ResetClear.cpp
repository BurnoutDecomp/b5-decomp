// FX-TRAFFIC3 (crash parity wave 5, 2026-09-24, CC-1): the PRODUCTION TrafficEntityModule::
// HandleResetRaceCarEvents @0x82742CE8 with its three constants, and the production
// KillAllTrafficInCylinder @0x82741C58 / GetVehicle / GetVehicleTransform it runs through, extracted
// from src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp (and
// InputBuffer_PostPhysics::GetVehicleManagerOutputInterface @0x82711700 from the getters TU) by
// run_fxtraffic3_reset_clear.py, hosted on a fixture with the real member types. RemoveVehicle is a
// RECORDER (not under test); it also records the removal-reason baton in force at each call.
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   0x82742D30  the queue is the manager output's +0x5B0 RaceCarResetEvent queue
//   0x82742D70  meActiveRaceCarIndex == meLocalPlayerIndex (+0x713F0, `cmpw`) AND
//   0x82742D80  mbResettingAfterWreck != 0, else the event is skipped
//   0x82742D94  KillAllTrafficInCylinder(mResetPosition, online (+0x717DC) ? 12.0 (flt_820BA7CC)
//               : 75.0 (flt_820BA7C4), 10.0 (flt_820BA5E4), r6 = 1 -> parked cars included)
//   KillAllTrafficInCylinder: alive only ; horizontal d^2 > r^2 skip ; |dy| >= h skip
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTrackWitness.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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
namespace Log { DebugPrint* gpDebugPrint = nullptr; }             // the witness streams stay off
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

namespace BrnTraffic
{
    const char* gpcTrafficRemoveReason = "unknown";
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }

    struct ResetFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::mbIsOnlineGameMode)  mbIsOnlineGameMode;
        decltype(M::meLocalPlayerIndex)  meLocalPlayerIndex;
        decltype(M::maVehicles)          maVehicles;
        decltype(M::maVehicleTransforms) maVehicleTransforms;

        Vehicle*       GetVehicle(u32 luIndex);
        Matrix44Affine GetVehicleTransform(u32 luIndex) const;

        void HandleResetRaceCarEvents(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput);
        void KillAllTrafficInCylinder(Vector3 lvCentre, f32 lfRadius, f32 lfHeight, bool lbIncludeStatic);
        void RemoveVehicle(u32 luVehicle);
    };

    struct Removal { u32 muVehicle; std::string mReason; };
    static std::vector<Removal> gRemoved;

    void ResetFixture::RemoveVehicle(u32 luVehicle)
    {
        gRemoved.push_back(Removal{ luVehicle, gpcTrafficRemoveReason });
    }
}

// The production bodies under test.
#include "reset_clear.inc"

using namespace BrnTraffic;
typedef ResetFixture Fixture;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaStorage[sizeof(Fixture)];
alignas(64) static unsigned char gaInputStorage[sizeof(BrnTrafficIO::InputBuffer_PostPhysics)];

static Fixture& F() { return *reinterpret_cast<Fixture*>(gaStorage); }
static BrnTrafficIO::InputBuffer_PostPhysics& Input() { return *reinterpret_cast<BrnTrafficIO::InputBuffer_PostPhysics*>(gaInputStorage); }

static const Vector3 KV_RESET = { 100.0f, 5.0f, 200.0f, 0.0f };

static void Place(u32 luVehicle, f32 lfX, f32 lfY, f32 lfZ, bool lbAlive = true, u8 luSpecies = Vehicle::E_SPECIES_STANDARD)
{
    Fixture& lr = F();
    lr.maVehicleTransforms[luVehicle].SetIdentity();
    lr.maVehicleTransforms[luVehicle].wAxis = { lfX, lfY, lfZ, 1.0f };
    lr.maVehicles[luVehicle].mxFlags   = lbAlive ? Vehicle::E_FLAG_ALIVE : 0;
    lr.maVehicles[luVehicle].muSpecies = luSpecies;
}

static void Fresh(bool lbOnline, EActiveRaceCarIndex leLocal)
{
    std::memset(gaStorage, 0, sizeof(gaStorage));
    Fixture& lr = F();
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        lr.maVehicleTransforms[luVehicle].SetIdentity();
        lr.maVehicleTransforms[luVehicle].wAxis = { 9000.0f, 5.0f, 9000.0f, 1.0f };   // far away, dead
    }
    lr.mbIsOnlineGameMode = lbOnline;
    lr.meLocalPlayerIndex = leLocal;

    // The scene around KV_RESET (100, 5, 200).
    Place(1, 174.9f, 5.0f, 200.0f);                                   // d = 74.9
    Place(2, 175.1f, 5.0f, 200.0f);                                   // d = 75.1
    Place(3, 100.0f, 14.9f, 200.0f);                                  // dy = 9.9
    Place(4, 100.0f, 15.0f, 200.0f);                                  // dy = 10.0 exactly
    Place(5, 110.0f, 5.0f, 200.0f, true, Vehicle::E_SPECIES_STATIC);  // parked, 10 m
    Place(6, 105.0f, 5.0f, 200.0f, false);                            // dead, 5 m
    Place(7, 111.9f, 5.0f, 200.0f);                                   // d = 11.9
    Place(8, 112.5f, 5.0f, 200.0f);                                   // d = 12.5
    Place(9, 100.0f, 5.0f, 290.0f);                                   // d = 90
    Place(10, 400.0f, 5.0f, 200.0f);                                  // near the OTHER position below

    std::memset(gaInputStorage, 0, sizeof(gaInputStorage));
    Input().mxStatusFlags.SetBit(CgsModule::IOBuffer::eStatusLockedForRead);
    Input().mVehicleManagerOutputInterface.mRaceCarResetEventQueue.Construct();
    gRemoved.clear();
}

static void Post(EActiveRaceCarIndex leCar, bool lbWreck, Vector3 lvPosition)
{
    BrnPhysics::Vehicle::RaceCarResetEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.meActiveRaceCarIndex  = leCar;
    lEvent.mbResettingAfterWreck = lbWreck;
    lEvent.mResetPosition        = lvPosition;
    Input().mVehicleManagerOutputInterface.AddRaceCarResetEvent(lEvent);
}

static bool Removed(u32 luVehicle)
{
    for (const Removal& lr : gRemoved)
    {
        if (lr.muVehicle == luVehicle) return true;
    }
    return false;
}

int main()
{
    const EActiveRaceCarIndex leP0 = static_cast<EActiveRaceCarIndex>(0);
    const EActiveRaceCarIndex leP1 = static_cast<EActiveRaceCarIndex>(1);

    // ---- offline, the local player's wreck reset ------------------------------------------
    Fresh(false, leP0);
    Post(leP0, true, KV_RESET);
    F().HandleResetRaceCarEvents(&Input());
    Check(Removed(1), "R1 offline radius is 75 m (flt_820BA7C4): d = 74.9 is cleared");
    Check(!Removed(2), "R2 d = 75.1 is outside the 75 m cylinder");
    Check(Removed(3), "R3 half-height 10 m (flt_820BA5E4): dy = 9.9 is cleared");
    Check(!Removed(4), "R4 dy = 10.0 exactly is outside (|dy| >= h)");
    Check(Removed(5), "R5 parked cars are included (r6 = 1)");
    Check(!Removed(6), "R6 a dead car is not touched");
    Check(Removed(7) && Removed(8), "R7 offline clears 11.9 m and 12.5 m alike");
    Check(!Removed(9), "R8 d = 90 is kept");
    Check(!gRemoved.empty() && gRemoved[0].mReason == "reset-after-wreck-cylinder",
          "R9 the removals carry the reset-after-wreck reason tag");
    Check(std::string(gpcTrafficRemoveReason) == "unknown", "R10 the reason tag is restored after the call");

    // ---- online: 12 m (flt_820BA7CC) ------------------------------------------------------
    Fresh(true, leP0);
    Post(leP0, true, KV_RESET);
    F().HandleResetRaceCarEvents(&Input());
    Check(Removed(7) && !Removed(8), "R11 online radius is 12 m: 11.9 cleared, 12.5 kept");
    Check(!Removed(1) && Removed(5), "R12 online: 74.9 m kept, the parked car at 10 m cleared");

    // ---- the event filters ----------------------------------------------------------------
    Fresh(false, leP0);
    Post(leP1, true, KV_RESET);
    F().HandleResetRaceCarEvents(&Input());
    Check(gRemoved.empty(), "R13 another car's wreck reset clears nothing");

    Fresh(false, leP0);
    Post(leP0, false, KV_RESET);
    F().HandleResetRaceCarEvents(&Input());
    Check(gRemoved.empty(), "R14 a reset that is not after a wreck clears nothing");

    Fresh(false, static_cast<EActiveRaceCarIndex>(-1));
    Post(leP0, true, KV_RESET);
    F().HandleResetRaceCarEvents(&Input());
    Check(gRemoved.empty(), "R15 no local player (-1): event 0 does not match (cmpw)");

    // Every event is walked, and each clear is around ITS OWN reset position.
    Fresh(false, leP0);
    Post(leP0, false, KV_RESET);                                        // skipped
    Post(leP0, true, Vector3{ 410.0f, 5.0f, 200.0f, 0.0f });            // car 10 is 10 m away
    F().HandleResetRaceCarEvents(&Input());
    Check(Removed(10) && !Removed(1) && !Removed(5), "R16 the clear is centred on the event's mResetPosition");

    Fresh(false, leP0);
    F().HandleResetRaceCarEvents(&Input());
    Check(gRemoved.empty(), "R17 an empty queue clears nothing");

    Check(gAsserts == 0, "A1 no assert fired on any path");

    std::printf("FxTraffic3ResetClear: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
