// Harness for run_wheel_twist_draw.py (crash parity G19-D2): the shipped UpdateWheels twist-limit
// statement is pasted into TwistLimit() through extracted.inc and run against the real CgsRandom.
// Console 0x82626580..0x826265E0 is the inlined RandomVecFloat: slot = (cursor+3)&4, value =
// ring[slot] - 1.0, ring[slot] refilled from the old seed's high word, cursor = slot+1, x 1.57.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#undef protected
#undef private
#include "BrnCommonTypes.h"                               // VecFloat (the TU's constant type)
#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"   // rw::math::vpu::VecFloat::GetFloat
#include <cmath>
#include <cstdio>
#include <cstring>

// CgsRandom.cpp's range draws carry dev asserts; the harness never trips them.
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { return 0; }
void* EndAssert() { return nullptr; }
} }

#include "extracted.inc"

int main()
{
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName, unsigned c) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL (cursor %u): %s\n", c, lpcName); }
    };
    for (unsigned c = 0; c < 8; ++c)
    {
        CgsNumeric::Random lRandom;
        lRandom.Construct();
        for (unsigned i = 0; i < 8; ++i)   // distinct ring values 1.0 + i/16
        {
            const float f = 1.0f + static_cast<float>(i) / 16.0f;
            std::memcpy(&lRandom.mauIntegerBuffer[i], &f, 4);
        }
        lRandom.muOldestBufferIndex = c;
        const u64 luSeed = lRandom.muSeed;
        const unsigned slot = (c + 3u) & 4u;
        float lfExpectRing; std::memcpy(&lfExpectRing, &lRandom.mauIntegerBuffer[slot], 4);

        const f32 lfLimit = TwistLimit(&lRandom);

        Check(std::fabs(lfLimit - (lfExpectRing - 1.0f) * 1.57f) < 1e-6f, "value = (ring[(c+3)&4] - 1) * 1.57", c);
        Check(lRandom.muOldestBufferIndex == slot + 1u, "cursor = slot + 1", c);
        Check(lRandom.mauIntegerBuffer[slot] == (0x3F800000u | (static_cast<u32>(luSeed >> 32) >> 9)),
              "ring[slot] refilled from the old seed's high word", c);
        Check(lRandom.muSeed == luSeed * CgsNumeric::KU_RANDOM_MULTIPLIER + 1, "seed stepped once", c);
    }
    std::printf("WheelTwistDraw: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
