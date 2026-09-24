// FX-CRASHVFX (crash parity 2026-09-24): BrnEffects::Utils::Vector3Randomiser::RandomiseXYZ @0x82277EC8 and
// Vector4Randomiser::RandomiseXYZW @0x82277FB8 -- the per-lane draws behind every crash-dust position and velocity
// (ProcessRaceCarContacts), the spark-line jitter and the jump ejection cone.
//
// run_fxcrashvfx_randomisers.py extracts the two PRODUCTION bodies out of BrnEffectsUtils.cpp and compiles them
// against the revision's header and the real CgsNumeric::Random. The expected values are the CONSOLE'S OWN OUTPUTS
// (FxCrashVfxRandomiserData.h, written by scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_randomiser_data.py from
// the functions' real instruction words): the returned vector and the ring / seed / cursor, bit for bit. The
// combine is a single vmaddfp -- fused -- and the cases are drawn so a rounded-product combine misses lanes.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Effects/BrnEffectsUtils.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "FxCrashVfxRandomiserData.h"

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnEffects
{
namespace Utils
{
#include "fxcrashvfx_randomisers.inc"
}
}

static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }

int main()
{
    u32 laLanes[2] = { 0, 0 }, laCases[2] = { 0, 0 }, laRings[2] = { 0, 0 };
    for (u32 luCase = 0; luCase < sizeof(kaRandomiserCases) / sizeof(kaRandomiserCases[0]); ++luCase)
    {
        const RandomiserCaseData& lrCase = kaRandomiserCases[luCase];
        const u32 luKind = lrCase.muKind == 3u ? 0u : 1u;

        CgsNumeric::Random lRandom;
        for (u32 i = 0; i < 8; ++i)
            lRandom.mauIntegerBuffer[i] = lrCase.maRing[i];
        lRandom.muSeed              = lrCase.muSeed;
        lRandom.muOldestBufferIndex = lrCase.muIndex;

        u32 lauOut[4];
        if (lrCase.muKind == 3u)
        {
            BrnEffects::Utils::Vector3Randomiser lRandomiser;
            lRandomiser.mVecA.x = Float(lrCase.maA[0]); lRandomiser.mVecA.y = Float(lrCase.maA[1]);
            lRandomiser.mVecA.z = Float(lrCase.maA[2]); lRandomiser.mVecA.w = Float(lrCase.maA[3]);
            lRandomiser.mVecB.x = Float(lrCase.maB[0]); lRandomiser.mVecB.y = Float(lrCase.maB[1]);
            lRandomiser.mVecB.z = Float(lrCase.maB[2]); lRandomiser.mVecB.w = Float(lrCase.maB[3]);
            const Vector3 lv = lRandomiser.RandomiseXYZ(lRandom);
            lauOut[0] = Bits(lv.x); lauOut[1] = Bits(lv.y); lauOut[2] = Bits(lv.z); lauOut[3] = Bits(lv.w);
        }
        else
        {
            BrnEffects::Utils::Vector4Randomiser lRandomiser;
            lRandomiser.mVecA.x = Float(lrCase.maA[0]); lRandomiser.mVecA.y = Float(lrCase.maA[1]);
            lRandomiser.mVecA.z = Float(lrCase.maA[2]); lRandomiser.mVecA.w = Float(lrCase.maA[3]);
            lRandomiser.mVecB.x = Float(lrCase.maB[0]); lRandomiser.mVecB.y = Float(lrCase.maB[1]);
            lRandomiser.mVecB.z = Float(lrCase.maB[2]); lRandomiser.mVecB.w = Float(lrCase.maB[3]);
            const Vector4 lv = lRandomiser.RandomiseXYZW(lRandom);
            lauOut[0] = Bits(lv.x); lauOut[1] = Bits(lv.y); lauOut[2] = Bits(lv.z); lauOut[3] = Bits(lv.w);
        }

        u32 luLanes = 0;
        for (u32 k = 0; k < 4; ++k)
            luLanes += (lauOut[k] == lrCase.maOut[k]) ? 1u : 0u;
        bool lbRing = lRandom.muSeed == lrCase.muSeedOut && lRandom.muOldestBufferIndex == lrCase.muIndexOut;
        for (u32 i = 0; i < 8; ++i)
            lbRing = lbRing && lRandom.mauIntegerBuffer[i] == lrCase.maRingOut[i];

        laLanes[luKind] += luLanes;
        laCases[luKind] += 1u;
        laRings[luKind] += lbRing ? 1u : 0u;
    }

    char lacLabel[256];
    std::snprintf(lacLabel, sizeof(lacLabel), "RandomiseXYZ: every lane is the console's (fused vmaddfp): %u/%u",
                  laLanes[0], 4u * laCases[0]);
    Check(laLanes[0] == 4u * laCases[0], lacLabel);
    std::printf("%s\n", lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel), "RandomiseXYZ: ring / seed / cursor are the console's: %u/%u",
                  laRings[0], laCases[0]);
    Check(laRings[0] == laCases[0], lacLabel);
    std::printf("%s\n", lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel), "RandomiseXYZW: every lane is the console's (fused vmaddfp): %u/%u",
                  laLanes[1], 4u * laCases[1]);
    Check(laLanes[1] == 4u * laCases[1], lacLabel);
    std::printf("%s\n", lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel), "RandomiseXYZW: ring / seed / cursor are the console's: %u/%u",
                  laRings[1], laCases[1]);
    Check(laRings[1] == laCases[1], lacLabel);
    std::printf("%s\n", lacLabel);

    std::printf("FxCrashVfxRandomisers: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
