// FX-CRASHVFX (crash parity 2026-09-24, item 1 callee wall): BrnEffects::BurstAccumulator::Update @0x8227EC90 --
// the world-grinding / vehicle-vehicle spark-burst accumulator ProcessRaceCarContacts @0x82297C08 and
// HandleVehicleVehicleSparks @0x82296790 call before every DoSparkShower.
//
// run_fxcrashvfx_burst_accumulator.py extracts the PRODUCTION Update body out of ActiveRaceCarData.cpp and compiles
// it against the revision's own ActiveRaceCarData.h and the real CgsNumeric::Random (CgsRandom.cpp). The expected
// values are the CONSOLE'S OWN OUTPUTS (FxCrashVfxBurstAccumulatorData.h, written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_burst_data.py from the function's real instruction words):
// the returned count, all six members, and the ring / seed / cursor, bit for bit. The cases pin the three
// things the old body got wrong: a NaN time resets (fcmpu unordered is not `blt`), the threshold's last
// multiply-add is fused, and a NaN size bursts ZERO (fctidz's low word), not 0x80000000.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Effects/ActiveRaceCarData.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "FxCrashVfxBurstAccumulatorData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
    else
    {
        std::printf("pass  %s\n", lpcLabel);
    }
}

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnEffects
{
#include "fxcrashvfx_burst_update.inc"
}

static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }

int main()
{
    for (u32 luCase = 0; luCase < sizeof(kaBurstCases) / sizeof(kaBurstCases[0]); ++luCase)
    {
        const BurstCaseData& lrCase = kaBurstCases[luCase];
        char lacLabel[320];

        BrnEffects::BurstAccumulator lAccumulator;
        static_assert(sizeof(lAccumulator) == 0x18, "six f32");
        std::memcpy(&lAccumulator, lrCase.maAccumulator, sizeof(lAccumulator));

        CgsNumeric::Random lRandom;
        for (u32 i = 0; i < 8; ++i)
            lRandom.mauIntegerBuffer[i] = lrCase.maRing[i];
        lRandom.muSeed              = lrCase.muSeed;
        lRandom.muOldestBufferIndex = lrCase.muIndex;

        const unsigned luAssertsBefore = gAsserts;
        const u32 luReturned = FXCRASHVFX_BURST_UPDATE(lAccumulator, Float(lrCase.muSize), Float(lrCase.muTime), lRandom);

        u32 lauOut[6];
        std::memcpy(lauOut, &lAccumulator, sizeof(lauOut));
        u32 luMembers = 0;
        for (u32 i = 0; i < 6; ++i)
            luMembers += (lauOut[i] == lrCase.maAccumulatorOut[i]) ? 1u : 0u;
        bool lbRing = lRandom.muSeed == lrCase.muSeedOut && lRandom.muOldestBufferIndex == lrCase.muIndexOut;
        for (u32 i = 0; i < 8; ++i)
            lbRing = lbRing && lRandom.mauIntegerBuffer[i] == lrCase.maRingOut[i];

        std::snprintf(lacLabel, sizeof(lacLabel), "case %u (%s): returns %u (console %u)", luCase, lrCase.mpcName,
                      luReturned, lrCase.muReturn);
        Check(luReturned == lrCase.muReturn, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "case %u: all six members are the console's bits: %u/6", luCase,
                      luMembers);
        Check(luMembers == 6u, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "case %u: the ring, seed and cursor are the console's", luCase);
        Check(lbRing, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "case %u: asserts %u (console %u)", luCase,
                      gAsserts - luAssertsBefore, lrCase.muAsserts);
        Check(gAsserts - luAssertsBefore == lrCase.muAsserts, lacLabel);
    }
    std::printf("FxCrashVfxBurstAccumulator: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
