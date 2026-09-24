// Harness for run_hinge_detachment.py (crash parity G20-D1/D2/D3/D6). Shipped text is pasted in
// through three generated includes:
//   spin.inc    -- UpdateSpinningDetachment's accumulate/decay/threshold block (G20-D3)
//   draws.inc   -- the forced (G20-D1) and spinning (G20-D2) part-ordinal draws
//   order.inc   -- ContactTime + UpdateContacts' sort of the contact rows (G20-D6)
#include "types.hpp"
#include "BrnCommonTypes.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

struct Body { Vector3 w; Vector3 GetAngularVelocity() const { return w; } };
static const f32 kfAngularVelocityDecay = 0.99f;
static const f32 KF_ANGULAR_DECAY_REFERENCE_RATE = 60.0f;
static const f32 kfAngularVelocityForDetachment = 8.0f;
inline f32 MagnitudeSquared3(const VecFloat& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }   // pre-fix text only

struct SpinObject
{
    VecFloat mAngularVelocitySum = { 0, 0, 0, 0 };
    Body mBody;
    bool mbPassed = false;
    const Body& GetVehicleBody() const { return mBody; }
    void SpinStep(VecFloat lvfTimeStep)
    {
        mbPassed = false;
#include "spin.inc"
        mbPassed = true;
    }
};

struct FakeRandom { u32 muValue; u32 RandomUInt() { return muValue; } };
#include "draws.inc"

namespace
{
#include "order.inc"
}

int main()
{
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };
    const VecFloat dt = { 1.0f / 60.0f, 1.0f / 60.0f, 1.0f / 60.0f, 1.0f / 60.0f };

    // G20-D3: a steady 6 rad/s roll integrates 0.1 rad a frame, decays at 0.99 -> crosses 8.0 rad
    // after ~164 frames (~2.7 s), never on the first frame.
    {
        SpinObject o; o.mBody.w = { 0, 0, 6.0f, 0 };
        int liFirst = -1;
        for (int f = 0; f < 400 && liFirst < 0; ++f) { o.SpinStep(dt); if (o.mbPassed) liFirst = f; }
        Check(liFirst > 120 && liFirst < 220, "steady 6 rad/s hinges after ~2.7 s of rolling, not at once");
    }
    // Alternating spin: the TOTAL angular travel (L1, |w|) still accumulates; a signed sum would cancel.
    {
        SpinObject o;
        int liFirst = -1;
        for (int f = 0; f < 400 && liFirst < 0; ++f)
        {
            o.mBody.w = { 0, (f & 1) ? -6.0f : 6.0f, 0, 0 };
            o.SpinStep(dt);
            if (o.mbPassed) liFirst = f;
        }
        Check(liFirst > 120 && liFirst < 220, "a car rocking back and forth accumulates travel too");
    }
    // A gentle 0.5 rad/s drift never gets there (steady state 0.825 rad).
    {
        SpinObject o; o.mBody.w = { 0.5f, 0, 0, 0 };
        bool lbEver = false;
        for (int f = 0; f < 600; ++f) { o.SpinStep(dt); lbEver = lbEver || o.mbPassed; }
        Check(!lbEver, "a slow spin never hinges a panel");
    }

    // G20-D1 / D2: the ordinals are modulo 11 / 20 of the draw.
    {
        FakeRandom r{ 0xFFFFFFF0u };
        Check(ForcedDraw(&r) == 0xFFFFFFF0u % 11u, "forced-hinge ordinal = draw % 11");
        Check(SpinDraw(r) == 0xFFFFFFF0u % 20u, "spin-hinge ordinal = draw % 20");
    }

    // G20-D6: an earlier car-car contact (time 0.1) runs before a later head-on wall (time 0.9)
    // whatever the keys; near-simultaneous rows (|dt| <= 0.1) fall back to the key.
    {
        _mContactOrder.miNumContacts = 3;
        _mContactOrder.maContactTimes[0] = ContactTime{ -0.9f, 0.9f, 0 };   // world, head-on, late
        _mContactOrder.maContactTimes[1] = ContactTime{  0.5f, 0.1f, 1 };   // car-car, early
        _mContactOrder.maContactTimes[2] = ContactTime{ -0.2f, 0.85f, 2 };  // world, near-simultaneous with row 0
        SortRows();
        Check(_mContactOrder.maContactTimes[0].mi16SensorIndex == 1, "the earliest contact is applied first");
        Check(_mContactOrder.maContactTimes[1].mi16SensorIndex == 0 &&
              _mContactOrder.maContactTimes[2].mi16SensorIndex == 2, "near-simultaneous rows are ordered by key");
    }

    // FX-NANPOL (2026-09-24): `fcmpu |dt|, 0.1 ; ble key` -- raw 0x4099000C @0x826298E8 (front
    // compare) and @0x82629974 (hole walk). ble (bc 4,gt) is TAKEN on unordered, so a row whose
    // impact time is NaN is ordered by its KEY, never by the (unordered) time compare.
    {
        const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();
        _mContactOrder.miNumContacts = 2;
        _mContactOrder.maContactTimes[0] = ContactTime{  0.5f, 0.2f, 0 };
        _mContactOrder.maContactTimes[1] = ContactTime{ -0.5f, lfNaN, 1 };   // NaN time, smaller key
        SortRows();
        Check(_mContactOrder.maContactTimes[0].mi16SensorIndex == 1 &&
              _mContactOrder.maContactTimes[1].mi16SensorIndex == 0,
              "front compare: a NaN impact time is ordered by its key (ble @0x826298E8 taken on unordered)");

        _mContactOrder.miNumContacts = 3;
        _mContactOrder.maContactTimes[0] = ContactTime{ -1.0f, 0.5f, 0 };
        _mContactOrder.maContactTimes[1] = ContactTime{  0.5f, 0.5f, 1 };
        _mContactOrder.maContactTimes[2] = ContactTime{  0.0f, lfNaN, 2 };   // not < row 0, < row 1 by key
        SortRows();
        Check(_mContactOrder.maContactTimes[0].mi16SensorIndex == 0 &&
              _mContactOrder.maContactTimes[1].mi16SensorIndex == 2 &&
              _mContactOrder.maContactTimes[2].mi16SensorIndex == 1,
              "hole walk: a NaN impact time is ordered by its key (ble @0x82629974 taken on unordered)");
    }

    std::printf("HingeDetachment: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
