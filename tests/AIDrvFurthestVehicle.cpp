// FX-AIDRV regression G03-D2 (crash parity 2026-09-22): the production
// AIDriver::GetIndexOfFurthestVehicle (and NearbyVehicles::GetVehiclePointer and the TU's To2DU),
// extracted verbatim from src/GameSource/World/AI/BrnAIDriver_Update.cpp by
// run_aidrv_furthest_vehicle.py, checked against the ARTIST export hole @0x8277D2E0.
//
// Console (ppcdis + vmx128.py raw fields; PS3 twin @0xA02810 identical):
//   lCar2DPosition = (pos.x, pos.z)  (vrlimi128 8,0 / 4,1 @0x8277D330/0x8277D344)
//   f31 = |lNewPosition - lCar2DPosition|  (2-lane magnitude, vsel 0 when lenSq == 0)
//   r23 = -1 ; for i < miCount: d = |mVehicle[i].mCentre - lCar2DPosition| ;
//     fcmpu d,f31 ; ble skip ; r23 = i         -- f31 is never rewritten
//   return r23
// i.e. the LAST entry farther from the car than the candidate (not the farthest), else -1. ble
// (0x8277D4CC, raw 0x40990008 = bc 4,cr6.gt) is TAKEN on unordered, so a NaN distance -- the
// entry's or the candidate's -- is never nominated (FX-NANPOL 2026-09-24 corrected this; the
// check below used to assert the opposite).
// The one fixture is AICar::GetPosition (it returns the plain member it reads, +0x1430).
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector2_operation.h"
#include <cmath>
#include <cstdio>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }

    AICar    gCar{};
    AIDriver gDriver;

    // The car sits at world (100, 5, 200): 2D (x, z) = (100, 200). Entries and candidates are
    // placed `d` metres from it along world +x (2D x) or world +z (2D y), so every distance is an
    // exact float (sqrt of a perfect square).
    Vector2 AtX(f32 lfDistance) { Vector2 lV; lV.x = 100.0f + lfDistance; lV.y = 200.0f; lV.z = 0.0f; lV.w = 0.0f; return lV; }
    Vector2 AtZ(f32 lfDistance) { Vector2 lV; lV.x = 100.0f; lV.y = 200.0f + lfDistance; lV.z = 0.0f; lV.w = 0.0f; return lV; }

    void Fill(const f32* lpfDistances, s32 liCount)
    {
        for (s32 liEntry = 0; liEntry < NearbyVehicles::KI_MAX_NEARBY_VEHICLES; ++liEntry)
            gDriver.mNearbyVehicles.mVehicle[liEntry].mCentre = AtX(1000.0f);   // stale slots: far away
        for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
            gDriver.mNearbyVehicles.mVehicle[liEntry].mCentre =
                (liEntry & 1) ? AtZ(lpfDistances[liEntry]) : AtX(lpfDistances[liEntry]);
        gDriver.mNearbyVehicles.miCount = liCount;
    }
}

int main()
{
    gCar.mPosition = Vector3{ 100.0f, 5.0f, 200.0f, 0.0f };
    gDriver.mpCarHost = &gCar;

    // A full 16-slot list (the only time the console asks).
    const f32 lafFull[16] = { 5, 50, 12, 30, 8, 70, 3, 25, 40, 9, 60, 15, 2, 45, 11, 7 };
    Fill(lafFull, 16);
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(20.0f)) == 13,
          "candidate 20 m: the LAST entry farther than it (13, 45 m) -- not the farthest (5, 70 m)");
    Check(gDriver.GetIndexOfFurthestVehicle(AtZ(1.0f)) == 15,
          "candidate 1 m: every entry is farther -> the last index, 15");
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(45.0f)) == 10,
          "candidate 45 m: a tie (13, 45 m) is kept (ble) -> 10 (60 m)");
    Check(gDriver.GetIndexOfFurthestVehicle(AtZ(65.0f)) == 5,
          "candidate 65 m: only entry 5 (70 m) is farther -> 5");
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(100.0f)) == -1,
          "candidate 100 m: nothing is farther -> -1 (drop the candidate)");
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(70.0f)) == -1,
          "candidate 70 m == the farthest entry: nothing strictly farther -> -1");

    // The 2D plane is (x, z): a candidate 30 m along world z is 30 m away, whatever world y is.
    Vector2 lHigh = AtZ(30.0f);
    Check(gDriver.GetIndexOfFurthestVehicle(lHigh) == 13,
          "candidate 30 m along world z (2D y) -> 13 (45 m); the car's world y (5) is not a lane");

    // The loop stops at miCount: stale slots beyond it (1000 m) are never nominated.
    const f32 lafThree[3] = { 10, 30, 20 };
    Fill(lafThree, 3);
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(15.0f)) == 2, "count 3, candidate 15 m -> 2 (20 m)");
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(25.0f)) == 1, "count 3, candidate 25 m -> 1 (30 m)");
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(35.0f)) == -1, "count 3, candidate 35 m -> -1 (stale 1000 m slots ignored)");

    // A candidate on the car (reference 0; the vsel zero guard): any entry off the car qualifies.
    const f32 lafZeroLast[2] = { 5, 0 };
    Fill(lafZeroLast, 2);
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(0.0f)) == 0, "candidate on the car: entry 1 (0 m) is not farther -> 0");

    // An unordered distance is skipped: fcmpu ; ble (bc 4,gt) is TAKEN on unordered.
    Fill(lafFull, 2);
    gDriver.mNearbyVehicles.mVehicle[0].mCentre.x = std::numeric_limits<f32>::quiet_NaN();
    gDriver.mNearbyVehicles.mVehicle[1].mCentre = AtX(3.0f);
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(5.0f)) == -1,
          "NaN entry distance is not nominated (ble @0x8277D4CC taken on unordered); entry 1 (3 m) is nearer -> -1");
    gDriver.mNearbyVehicles.mVehicle[1].mCentre = AtX(8.0f);
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(5.0f)) == 1,
          "a NaN entry before a farther one: only the ordered entry 1 (8 m) is nominated");

    // A NaN candidate makes every compare unordered (f31 never rewritten): nothing is nominated.
    Fill(lafFull, 16);
    Vector2 lNaNCandidate = AtX(5.0f);
    lNaNCandidate.x = std::numeric_limits<f32>::quiet_NaN();
    Check(gDriver.GetIndexOfFurthestVehicle(lNaNCandidate) == -1,
          "NaN candidate distance: every compare unordered -> -1 (drop the candidate)");

    // An empty list never nominates.
    Fill(lafFull, 0);
    Check(gDriver.GetIndexOfFurthestVehicle(AtX(5.0f)) == -1, "empty list -> -1");

    Check(gAssertions == 0, "no assertion fired");
    std::printf("AIDrvFurthestVehicle: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
