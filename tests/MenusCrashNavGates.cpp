// OWNERLIST 2026-09-27, lane L5 MENUS: the PRODUCTION distance gates of BrnDirector::ArbStateCrashNav (the pause /
// crash-nav camera state), extracted from src/GameSource/Director/Arbitrator/States/BrnArbStateCrashNav.cpp by
// run_menus_crashnav_gates.py: the three .rdata / .bss constants and PlayerToCameraDistanceSquared, compiled against
// stand-ins that carry the production member names (mpPlayerCar->mRaceCarState.mTransform.Pos(),
// Camera::GetTransform().wAxis).
//
// Checked against the ARTIST image and asm (ArbStateCrashNav::Update @0x8226DC98):
//   flt_82CDA4E0 == 3F800000 (1.0)                     the fly-by blurriness, stfs camera +0x134
//   unk_82FAAAF0 == splat(flt_8200D518 == 481C4000)     160000.0 -- the CRT thunk at 0x82C48488..0x82C484AC
//   unk_82FAA990 == splat(flt_8200D51C == 47EF4200)     122500.0 -- the CRT thunk at 0x82C484B0..0x82C484D4
//   case 3 @0x8226DE44..0x8226DE64: vsubfp (camera - player) ; vmsum3fp128 ; vcmpgtfp. d2, OUTER  -> turn about
//   case 4 @0x8226E020..0x8226E040: vsubfp ; vmsum3fp128 ; vcmpgtfp. INNER, d2                   -> come back in
//   vmsum3fp128 == ONE rounding of the f64 sum of the exact products (ROUNDING_RULE.md rule 1).
#include "types.hpp"
#include "rw/math/vpu/types.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

static f32 FromBits(u32 luBits)
{
    f32 lfValue;
    std::memcpy(&lfValue, &luBits, sizeof(lfValue));
    return lfValue;
}

namespace BrnDirector
{
namespace Camera
{
    // Stand-in camera: the production reads GetTransform().wAxis (the eye row, camera +0x30).
    struct TransformStandIn
    {
        rw::math::vpu::Vector3 wAxis;
    };
    struct Camera
    {
        TransformStandIn mTransform;
        const TransformStandIn& GetTransform() const { return mTransform; }
    };
}

// Stand-in shared info: the production reads mpPlayerCar->mRaceCarState.mTransform.Pos() (player car +0x220).
struct PlayerTransformStandIn
{
    rw::math::vpu::Vector3 mPos;
    const rw::math::vpu::Vector3& Pos() const { return mPos; }
};
struct RaceCarStateStandIn
{
    PlayerTransformStandIn mTransform;
};
struct VehicleInfoStandIn
{
    RaceCarStateStandIn mRaceCarState;
};
struct ArbStateSharedInfo
{
    const VehicleInfoStandIn* mpPlayerCar;
};
}

// namespace BrnDirector { namespace { <the production constants + PlayerToCameraDistanceSquared> } + accessors }
#include "crashnav_gates.inc"

using namespace BrnDirector;

static rw::math::vpu::Vector3 V(f32 x, f32 y, f32 z)
{
    rw::math::vpu::Vector3 lV;
    lV.x = x; lV.y = y; lV.z = z; lV.w = 0.0f;
    return lV;
}

// The two console comparisons, verbatim from the asm: case 3 turns about when d2 > OUTER (vcmpgtfp. v0, d2, OUTER),
// case 4 comes back in when INNER > d2 (vcmpgtfp. v0, INNER, d2). vcmpgtfp is false for a NaN lane.
static bool TurnsAbout(f32 lfD2) { return lfD2 > CrashNavGates::Outer(); }
static bool ComesBackIn(f32 lfD2) { return CrashNavGates::Inner() > lfD2; }

int main()
{
    Check(Bits(CrashNavGates::Outer()) == 0x481C4000u, "OUTER == splat(flt_8200D518) == 481C4000 (160000.0)");
    Check(Bits(CrashNavGates::Inner()) == 0x47EF4200u, "INNER == splat(flt_8200D51C) == 47EF4200 (122500.0)");
    Check(Bits(CrashNavGates::Blurriness()) == 0x3F800000u, "blurriness == flt_82CDA4E0 == 3F800000 (1.0)");

    VehicleInfoStandIn lCar = {};
    lCar.mRaceCarState.mTransform.mPos = V(2958.0f, 12.5f, -1764.0f);
    ArbStateSharedInfo lInfo = { &lCar };
    Camera::Camera lCamera = {};

    // A pause / crash-nav camera 300 m from the car: inside both gates.
    lCamera.mTransform.wAxis = V(2958.0f + 300.0f, 12.5f, -1764.0f);
    const f32 lf300 = CrashNavGates::Distance(lInfo, lCamera);
    Check(lf300 == 90000.0f, "300 m: d2 == 90000");
    Check(!TurnsAbout(lf300), "300 m: no turnabout (d2 90000 is not > OUTER 160000)");
    Check(ComesBackIn(lf300), "300 m: inside INNER (122500 > 90000)");

    lCamera.mTransform.wAxis = V(2958.0f, 12.5f + 401.0f, -1764.0f);
    Check(TurnsAbout(CrashNavGates::Distance(lInfo, lCamera)), "401 m: turns about (160801 > 160000)");

    lCamera.mTransform.wAxis = V(2958.0f, 12.5f, -1764.0f - 349.0f);
    Check(ComesBackIn(CrashNavGates::Distance(lInfo, lCamera)), "349 m: comes back in (122500 > 121801)");

    lCamera.mTransform.wAxis = V(2958.0f - 351.0f, 12.5f, -1764.0f);
    const f32 lf351 = CrashNavGates::Distance(lInfo, lCamera);
    Check(!ComesBackIn(lf351) && !TurnsAbout(lf351), "351 m: between the gates (neither edge)");

    // Rule 1: vmsum3fp128 rounds the f64 sum ONCE. For this offset the sequential f32 form rounds up (0x47F22ADE)
    // and the console's single rounding gives 0x47F22ADD.
    lCar.mRaceCarState.mTransform.mPos = V(0.0f, 0.0f, 0.0f);
    lCamera.mTransform.wAxis = V(FromBits(0xC313FB01u), FromBits(0xC392A4B2u), FromBits(0x42FD91E6u));
    const f32 lfModel = CrashNavGates::Distance(lInfo, lCamera);
    Check(Bits(lfModel) == 0x47F22ADDu, "vmsum3fp128 model: one rounding of the f64 sum (0x47F22ADD, not 0x47F22ADE)");

    // The order of the vsubfp does not change the squares: player and camera swapped give the same bits.
    lCar.mRaceCarState.mTransform.mPos = lCamera.mTransform.wAxis;
    lCamera.mTransform.wAxis = V(0.0f, 0.0f, 0.0f);
    Check(Bits(CrashNavGates::Distance(lInfo, lCamera)) == Bits(lfModel), "camera - player == player - camera, squared");

    // A NaN eye: vcmpgtfp is false on an unordered lane, so neither edge fires.
    lCamera.mTransform.wAxis = V(std::numeric_limits<f32>::quiet_NaN(), 0.0f, 0.0f);
    const f32 lfNaN = CrashNavGates::Distance(lInfo, lCamera);
    Check(!TurnsAbout(lfNaN) && !ComesBackIn(lfNaN), "NaN eye: no turnabout, no come-back");

    std::printf("MenusCrashNavGates: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
