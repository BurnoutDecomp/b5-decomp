// FX-CRASHVFX (crash parity 2026-09-25): BrnEffects::Utils::DebrisColourRandomiser::Randomise @0x8227E698 -- the
// per-particle debris colour of every ParticleModule::SpawnDebris batch (the jump wheel debris today, the crash
// debris bursts once HandleFireDebrisBurstEvent lands).
//
// run_fxcrashvfx_debris_colour.py compiles the revision's WHOLE BrnEffectsDebrisColourRandomiser.cpp (against the
// revision's header and the real CgsNumeric::Random) onto this fixture. The expected values are the CONSOLE'S OWN
// OUTPUTS (FxCrashVfxDebrisColourData.h, written by scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_colour_data.py
// from the function's real instruction words on emu64): the colour, bit for bit, and the ring / seed / cursor.
//
// What the console does and the tree did not: each of the two draws is ONE number -- word 0 of the ring's vector
// slot, splatted (lvsl(0) / vspltw / vperm) -- and each combine is ONE vmaddfp. The tree drew four different
// fractions per draw (the slot's four words) and rounded the product before the add.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"

#include <cstdio>
#include <cstring>

#include "fxcrashvfx_debris_colour.inc"
#include "FxCrashVfxDebrisColourData.h"

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
        ++gFailures;
    std::printf("%s  %s\n", lbPassed ? "pass" : "FAIL", lpcLabel);
}

// CgsRandom.cpp's RandomInt asserts; the fixture links the real Random, so it provides the assert entry points.
namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
}

static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }

static void Load(Vector4& lrv, const u32 lau[4])
{
    lrv.x = Float(lau[0]); lrv.y = Float(lau[1]); lrv.z = Float(lau[2]); lrv.w = Float(lau[3]);
}

int main()
{
    const u32 luNumCases = static_cast<u32>(sizeof(kaDebrisColourCases) / sizeof(kaDebrisColourCases[0]));
    u32 luLanes = 0, luRings = 0, luShown = 0;
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const DebrisColourCase& lrCase = kaDebrisColourCases[luCase];

        CgsNumeric::Random lRandom;
        for (u32 i = 0; i < 8; ++i)
            lRandom.mauIntegerBuffer[i] = lrCase.maRing[i];
        lRandom.muSeed              = lrCase.muSeed;
        lRandom.muOldestBufferIndex = lrCase.muIndex;

        BrnEffects::Utils::DebrisColourRandomiser lRandomiser;
        Load(lRandomiser.mVecBase, lrCase.maBase);
        Load(lRandomiser.mVecRange, lrCase.maRange);

        Vector4 lvOut;
        lRandomiser.Randomise(lvOut, lRandom);
        const u32 lauOut[4] = { Bits(lvOut.x), Bits(lvOut.y), Bits(lvOut.z), Bits(lvOut.w) };

        u32 luSame = 0;
        for (u32 k = 0; k < 4; ++k)
            luSame += (lauOut[k] == lrCase.maOut[k]) ? 1u : 0u;
        luLanes += luSame;
        if (luSame != 4u && luShown < 4u)
        {
            ++luShown;
            std::printf("      case %u: got %08X %08X %08X %08X want %08X %08X %08X %08X\n", luCase,
                        lauOut[0], lauOut[1], lauOut[2], lauOut[3],
                        lrCase.maOut[0], lrCase.maOut[1], lrCase.maOut[2], lrCase.maOut[3]);
        }

        bool lbRing = lRandom.muSeed == lrCase.muSeedOut && lRandom.muOldestBufferIndex == lrCase.muIndexOut;
        for (u32 i = 0; i < 8; ++i)
            lbRing = lbRing && lRandom.mauIntegerBuffer[i] == lrCase.maRingOut[i];
        luRings += lbRing ? 1u : 0u;
    }

    char lacLabel[256];
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "Randomise: every lane of every colour is the console's (splatted draws, fused combines): %u/%u",
                  luLanes, 4u * luNumCases);
    Check(luLanes == 4u * luNumCases, lacLabel);
    std::snprintf(lacLabel, sizeof(lacLabel),
                  "Randomise: the ring / seed / cursor after the two draws are the console's: %u/%u",
                  luRings, luNumCases);
    Check(luRings == luNumCases, lacLabel);

    std::printf("FxCrashVfxDebrisColour: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
