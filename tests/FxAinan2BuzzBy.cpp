// FX-AINAN2 regression (crash parity 2026-09-24): BuzzBy, extracted verbatim from BrnAIBuzzBy.cpp
// by run_fxainan2_buzzby.py.
//
// IsPositionInNoBuzzZone @0x82766FC0 walks seven zones: centre i at unk_8300DB30 + 16*i (filled
// at start-up by the CRT thunk @0x82C68F28 from .rdata floats), radius i at unk_820C4334 + 4*i
// (230 / 210 / 260 / 240 / 35 / 185 / 175, x360rd), `vmsum3fp128` distance squared against
// `fmuls r, r` and `blt -> return 1`. The PC tables were zero placeholders, so no position was
// ever inside a zone.
// AICarCanBuzz @0x82767020: `li r3,0 ; fcmpu dist(+0x1508), flt_820C4318 (200) ; blt ; li r3,1`
// -- a NaN distance falls through blt and CAN buzz; the old `>= 200` said no.
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

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
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    alignas(16) unsigned char gaBuzzBy[sizeof(BuzzBy)];
    BuzzBy& Buzz() { return *reinterpret_cast<BuzzBy*>(gaBuzzBy); }

    // The seven centres the console's thunk writes (x360rd of its source floats).
    const Vector3 KA_CENTRES[7] = {
        { -2430.80005f,  60.0f,        1906.59998f, 0.0f },
        { -2604.80005f,  82.0f,        1900.59998f, 0.0f },
        { -2229.80005f,  19.5f,        625.400024f, 0.0f },
        { -2205.80005f,  19.5f,        808.400024f, 0.0f },
        { -1981.80005f,  96.5f,        698.400024f, 0.0f },
        { -1059.69995f,  105.199997f, -1509.30005f, 0.0f },
        { -1054.69995f,  105.199997f, -1270.30005f, 0.0f },
    };
}

int main()
{
    std::memset(gaBuzzBy, 0, sizeof(gaBuzzBy));

    // ---- AICarCanBuzz ------------------------------------------------------------------------
    AICar lCar{};
    lCar.mfBuzzDistanceToPlayer = 250.0f;
    Check(Buzz().AICarCanBuzz(&lCar), "control: 250 m can buzz");
    lCar.mfBuzzDistanceToPlayer = 150.0f;
    Check(!Buzz().AICarCanBuzz(&lCar), "control: 150 m cannot");
    lCar.mfBuzzDistanceToPlayer = 200.0f;
    Check(Buzz().AICarCanBuzz(&lCar), "control: exactly 200 m can (blt not taken)");
    lCar.mfBuzzDistanceToPlayer = KF_NAN;
    Check(Buzz().AICarCanBuzz(&lCar), "a NaN distance falls through blt 0x82767070 -> can buzz");

    // ---- IsPositionInNoBuzzZone ----------------------------------------------------------------
    char lacLabel[96];
    for (int li = 0; li < 7; ++li)
    {
        std::snprintf(lacLabel, sizeof(lacLabel), "zone %d: its centre (thunk 0x82C68F28) is inside", li);
        Check(Buzz().IsPositionInNoBuzzZone(KA_CENTRES[li]), lacLabel);
    }
    Vector3 lProbe = KA_CENTRES[4];
    lProbe.y += 34.0f;
    Check(Buzz().IsPositionInNoBuzzZone(lProbe), "zone 4: 34 m up is inside its 35 m radius (unk_820C4344)");
    lProbe.y += 2.0f;
    Check(!Buzz().IsPositionInNoBuzzZone(lProbe), "control: zone 4: 36 m up is outside");
    Check(!Buzz().IsPositionInNoBuzzZone(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }),
          "control: the world origin is in no zone");
    Check(!Buzz().IsPositionInNoBuzzZone(Vector3{ KF_NAN, KF_NAN, KF_NAN, 0.0f }),
          "control: a NaN position is in no zone (blt not taken)");

    Check(gAssertions == 0u, "control: no assert fires");
    std::printf("FxAinan2BuzzBy: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
