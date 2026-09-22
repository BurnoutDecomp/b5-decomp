// The production method is extracted verbatim except for the receiver name.
// Members use production types; the fixture avoids unrelated vehicle services.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

static std::vector<std::string> gAsserts;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { gAsserts.emplace_back(message); return 0; }
void* EndAssert() { return nullptr; }
} }

using BrnPhysics::Vehicle::VehiclePhysics;
using BrnPhysics::Vehicle::VehicleAttribs;
namespace vpu = rw::math::vpu;
struct AirRamFixture
{
    static const u8 KU_MAX_AIR_RAMS = VehiclePhysics::KU_MAX_AIR_RAMS;
    using AirRamEffect = VehiclePhysics::AirRamEffect;
    decltype(VehiclePhysics::mpAttribs) mpAttribs;
    decltype(VehiclePhysics::mHalfExtent) mHalfExtent{2, 3, 4, 99};
    decltype(VehiclePhysics::mUsedAirRams) mUsedAirRams{};
    decltype(VehiclePhysics::mAirRamEffect) mAirRamEffect{};
    void AddAirRam(u32, f32, f32, Vector3, Vector3, f32);
};
#include "air_ram_method.inc"

int main()
{
    unsigned checks = 0, failures = 0;
    auto Check = [&](bool pass, const char* label) {
        ++checks;
        if (!pass) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
    };
    auto Vector = [&](Vector3 a, Vector3 b, const char* label) {
        Check(std::fabs(a.x-b.x) < .002f && std::fabs(a.y-b.y) < .002f &&
              std::fabs(a.z-b.z) < .002f && std::fabs(a.w-b.w) < .002f, label);
    };
    VehicleAttribs attribs{};
    attribs.mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce = {1000, 17, 23, 29};
    AirRamFixture car{&attribs};
    auto Reset = [&]() { car = AirRamFixture{&attribs}; gAsserts.clear(); };

    // Both custom paths retain w and do not normalize the supplied direction.
    for (u32 flags : {1u, 6u}) {
        Reset();
        car.AddAirRam(flags | 0x100, .02f, .25f, {2,-3,4,.02f}, {5,6,7,8}, .5f);
        const auto& ram = car.mAirRamEffect[0];
        Vector(ram.mImpulse, {2000,-3000,4000,20}, "custom impulse including magnitude lane");
        Vector(ram.mPosition, {5,6,7,8}, "custom position including w");
        Check(ram.meImpulseSpace == ((flags == 1) ? rw::physics::WORLD_SPACE : rw::physics::BODY_SPACE), "input space");
        Check(ram.mfDecay == .25f && ram.mfTimerTillFire == .5f, "decay and timer");
        Check(car.mUsedAirRams.IsBitSet(0) && !car.mUsedAirRams.IsBitSet(1) && gAsserts.empty(), "first slot and valid input");
    }
    // All seven nonempty axis combinations; all sixteen position flag combinations.
    for (u32 axes = 1; axes < 8; ++axes) for (u32 position = 0; position < 16; ++position) {
        Reset();
        car.AddAirRam(2 | (axes << 3) | (position << 9), .02f, 0, {99,98,97,96}, {9,8,7,6}, 0);
        const float scale = 1000 / std::sqrt(float(!!(axes&1) + !!(axes&2) + !!(axes&4)));
        Vector(car.mAirRamEffect[0].mImpulse, {(axes&2)?scale:0, (axes&1)?scale:0, (axes&4)?scale:0, 0}, "normalized axes");
        Vector(car.mAirRamEffect[0].mPosition, {(position&4)?-2.f:((position&8)?2.f:0.f), 0,
            (position&1)?4.f:((position&2)?-4.f:0.f), 0}, "position flag precedence");
        Check(gAsserts.empty(), "valid axis flags");
    }
    Reset();
    for (unsigned i = 0; i < 4; ++i) {
        car.AddAirRam(0x101, .02f, 0, {float(4-i),0,0,100.f*i}, {float(i),0,0,0}, 0);
        Check(car.mUsedAirRams.IsBitSet(i), "ascending free slots");
    }
    car.AddAirRam(0x101, .02f, 0, {9,0,0,0}, {42,0,0,0}, 0);
    Check(car.mAirRamEffect[3].mPosition.x == 42 && car.mAirRamEffect[0].mPosition.x == 0,
        "full pool evicts smallest xyz impulse irrespective of w");
    Reset();
    for (unsigned i = 0; i < 4; ++i) car.AddAirRam(1, .02f, 0, {1,0,0,0}, {}, 0);
    car.AddAirRam(0x101, .02f, 0, {1,0,0,0}, {42,0,0,0}, 0);
    Check(car.mAirRamEffect[0].mPosition.x == 42, "equal magnitudes evict first slot");

    // ARTIST validity is xyz == xyz, so infinity is accepted and w is unchecked.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    Reset(); car.AddAirRam(1, .02f, 0, {inf,0,0,nan}, {}, 0);
    Check(gAsserts.empty() && std::isnan(car.mAirRamEffect[0].mImpulse.w), "validity checks only xyz NaNs");
    for (u32 flags : {1u, 6u}) {
        Reset(); car.AddAirRam(flags, .02f, 0, {0,nan,0,0}, {}, 0);
        Check(gAsserts.size() == 1 && gAsserts[0] == "IsValid( lCustomImpulse )", "invalid custom impulse assertion");
    }
    Reset(); car.AddAirRam(0x101, .02f, 0, {1,0,0,0}, {0,0,nan,0}, 0);
    Check(gAsserts.size() == 1 && gAsserts[0] == "IsValid( lCustomPosition )", "invalid custom position assertion");
    Reset(); car.AddAirRam(3, .02f, 0, {1,0,0,0}, {}, 0);
    Check(gAsserts.size() == 1, "conflicting space tags assert");
    Reset(); car.AddAirRam(4, .02f, 0, {1,0,0,0}, {}, 0);
    Check(gAsserts.size() == 1, "custom body direction still requires body tag");
    Reset(); car.AddAirRam(2, .02f, 0, {}, {}, 0);
    Check(gAsserts.size() == 1 && gAsserts[0] == "Body-space airram impulse must specify at least one axis", "missing axis assertion");
    Check(std::isnan(car.mAirRamEffect[0].mImpulse.x), "continued invalid normalization is not silently zeroed");

    std::printf("AirRam: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
