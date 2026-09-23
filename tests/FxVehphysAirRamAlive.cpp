// FX-VEHPHYS (crash parity 2026-09-23, G54-D2): BrnPhysics::Vehicle::VehiclePhysics::UpdateAirRam
// @0x825FC8D8 -- the "still has magnitude to fire" threshold flt_82F2A430.
//
// run_fxvehphys_air_ram_alive.py EXTRACTS the production UpdateAirRam body from VehiclePhysics.cpp
// and compiles it into AirRamAliveFixture (mUsedAirRams / mAirRamEffect with their real types and a
// recording AddLocalImpulse).
//
// Console facts checked (ARTIST asm + image):
//   0x825FC9F8..0x825FCA08 mfTimerTillFire -= dt ; fire only when it is <= 0 (bgt skips)
//   0x825FCA10 lfs f0,flt_82F2A430 -> var_D0 ; 0x825FCA28 vmsum3fp128 |mImpulse(+0x1160)|^2
//   0x825FCA40 vcmpgtfp. |imp|^2 > K ; false -> 0x825FCAF4..0x825FCB08 clear the slot bit
//   true -> 0x825FCA98 AddLocalImpulse(mImpulse, [+0x1184] meImpulseSpace, mPosition, BODY_SPACE)
//           0x825FCAA4 (1 - mfDecay) ; 0x825FCAC8/0x825FCACC mImpulse *= it (4 lanes)
//   x360rd 0x82F2A430 = 0x3C23D70B (0.0100000007f == f32(0.1f)*f32(0.1f)); findinit: 1 site, the reader.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#undef protected
#undef private
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    struct AirRamAliveFixture
    {
        using AirRamEffect = VehiclePhysics::AirRamEffect;
        decltype(VehiclePhysics::mUsedAirRams)  mUsedAirRams{};
        decltype(VehiclePhysics::mAirRamEffect) mAirRamEffect{};

        struct Fired { Vector3 mImpulse; rw::physics::InputSpace meImpulseSpace; Vector3 mPosition; rw::physics::InputSpace mePositionSpace; };
        std::vector<Fired> maFired;
        void AddLocalImpulse(Vector3 lImpulse, rw::physics::InputSpace leImpulseSpace,
                             Vector3 lPosition, rw::physics::InputSpace lePositionSpace)
        {
            maFired.push_back(Fired{ lImpulse, leImpulseSpace, lPosition, lePositionSpace });
        }

        void UpdateAirRam(VecFloat lvfDeltaTime);
    };

#include "air_ram_alive.inc"
}
}

using namespace BrnPhysics::Vehicle;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, sizeof lu); return lu; }

int main()
{
    const f32 kfDt = 1.0f / 30.0f;
    const VecFloat kvfDt{ kfDt, kfDt, kfDt, kfDt };

    // The threshold itself: f32(0.1f)^2 is exactly the image word.
    Check(Bits(0.1f * 0.1f) == 0x3C23D70Bu, "f32(0.1f)*f32(0.1f) encodes 0x3C23D70B (the flt_82F2A430 word)");

    // (A) |imp|^2 == 0x3C23D70B exactly: 0x3C23D70B > 0x3C23D70B is FALSE on the console -> release.
    {
        AirRamAliveFixture lCar;
        lCar.mUsedAirRams.SetBit(0);
        lCar.mAirRamEffect[0].mImpulse = Vector3{ 0.1f, 0.0f, 0.0f, 0.0f };
        lCar.mAirRamEffect[0].mfTimerTillFire = 0.0f;
        lCar.mAirRamEffect[0].mfDecay = 0.25f;
        lCar.mAirRamEffect[0].meImpulseSpace = rw::physics::WORLD_SPACE;
        lCar.UpdateAirRam(kvfDt);
        Check(lCar.maFired.empty(), "(A) |imp|^2 == K: no AddLocalImpulse");
        Check(!lCar.mUsedAirRams.IsBitSet(0), "(A) |imp|^2 == K: the slot bit is cleared");
    }

    // (B) |imp|^2 = 0.04 > K: fires once with the stored tags, decays by (1 - 0.25), keeps its bit.
    {
        AirRamAliveFixture lCar;
        lCar.mUsedAirRams.SetBit(1);
        lCar.mAirRamEffect[1].mImpulse = Vector3{ 0.2f, 0.0f, 0.0f, 0.0f };
        lCar.mAirRamEffect[1].mPosition = Vector3{ 1.0f, 2.0f, 3.0f, 0.0f };
        lCar.mAirRamEffect[1].mfTimerTillFire = 0.0f;
        lCar.mAirRamEffect[1].mfDecay = 0.25f;
        lCar.mAirRamEffect[1].meImpulseSpace = rw::physics::BODY_SPACE;
        lCar.UpdateAirRam(kvfDt);
        Check(lCar.maFired.size() == 1 && lCar.maFired[0].mImpulse.x == 0.2f &&
              lCar.maFired[0].meImpulseSpace == rw::physics::BODY_SPACE &&
              lCar.maFired[0].mePositionSpace == rw::physics::BODY_SPACE &&
              lCar.maFired[0].mPosition.z == 3.0f, "(B) fires once with the stored impulse space");
        Check(std::fabs(lCar.mAirRamEffect[1].mImpulse.x - 0.15f) < 1.0e-7f && lCar.mUsedAirRams.IsBitSet(1),
              "(B) the impulse decays to 0.15 and the slot stays live");
    }

    // (C) a timer still running: nothing fires, nothing is released.
    {
        AirRamAliveFixture lCar;
        lCar.mUsedAirRams.SetBit(2);
        lCar.mAirRamEffect[2].mImpulse = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        lCar.mAirRamEffect[2].mfTimerTillFire = 1.0f;
        lCar.UpdateAirRam(kvfDt);
        Check(lCar.maFired.empty() && lCar.mUsedAirRams.IsBitSet(2) &&
              std::fabs(lCar.mAirRamEffect[2].mfTimerTillFire - (1.0f - kfDt)) < 1.0e-7f,
              "(C) a running timer counts down and holds the slot");
    }

    std::printf("FxVehphysAirRamAlive: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
