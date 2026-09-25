// FX-GATE item 7: CgsNumeric::Random::RandomUInt(min, max) draws from the INCLUSIVE range, as every console
// expansion does, and its three PC callers keep the console's modulus. The production RandomUInt(),
// RandomUInt(min, max) and RandomInt(min, max) bodies are EXTRACTED from CgsRandom.cpp, and each caller's
// draw expression is extracted from its function, by run_fxgate_random_uint_range.py.
//
// The console (no standalone symbol; inlined, assert "luMod > 0" = CgsRandom.h:303):
//   luMod = max - min + 1 ; draw = hi32(OLD seed) ; seed = seed * 0x5851F42D4C957F2D + 1 ;
//   return min + draw % luMod                              (unsigned; the ring is untouched)
//   HandleShowtimeTrafficBounce RandomUInt(150, 300): 0x82292DD8 mulhwu 0x36406C81 ; srwi 5 ; mulli 0x97 ;
//     subf ; addi 0x96 -> 150 + draw % 151.
// The callers:
//   SelectionHistory::Randomize @0x826C5900     draw & 0x1FF       (0x826C5B2C clrlwi 23)
//   SelectionHistory::FindRandomOldest @0x82702840  draw % ((n >> 1) + 1)  (0x8270294C srwi ; 0x82702950 addi 1 ;
//                                                0x827029B0 divwu)
//   RouteRequestManager::GenerateFreeRoamingDestination @0x827695F0  RandomInt(0, (u16)n - 1): draw % n
//                                               (0x8276960C clrlwi 16 ; 0x82769610 addi -1 ; 0x82769640 addi 1 ;
//                                                0x827696A0 divwu), asserts at CgsRandom.h:320 / :323
#include "types.hpp"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdio>

namespace
{
    int giFires = 0;
}

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++giFires; return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace CgsNumeric
{
    static const u64 KU_RANDOM_LCG_MULTIPLIER = 0x5851F42D4C957F2Dull;
#include "fxgate_random_uint_bodies.inc"
}

#include "fxgate_random_uint_callers.inc"

namespace
{
    unsigned gChecks = 0, gFailures = 0;

    void Check(bool lbPassed, const char* lpcLabel, u64 luSeed, u32 luActual, u32 luExpected)
    {
        ++gChecks;
        if (!lbPassed)
        {
            ++gFailures;
            std::printf("FAIL  %s: seed 0x%016llX -> %u, console %u\n", lpcLabel,
                        static_cast<unsigned long long>(luSeed), luActual, luExpected);
        }
    }

    u64 SeedWithHigh(u32 luHigh, u32 luLow = 0x13579BDFu)
    {
        return (static_cast<u64>(luHigh) << 32) | luLow;
    }

    u64 Step(u64 luSeed)
    {
        return luSeed * 0x5851F42D4C957F2Dull + 1u;
    }

    CgsNumeric::Random Seeded(u64 luSeed)
    {
        CgsNumeric::Random lRandom;
        lRandom.Construct();
        lRandom.muSeed = luSeed;
        return lRandom;
    }

    // One bounded unsigned draw against the console: value, the one step, no ring access.
    void ExpectUInt(u32 luMin, u32 luMax, u64 luSeed, const char* lpcLabel)
    {
        CgsNumeric::Random lRandom = Seeded(luSeed);
        const u32 luCursor = lRandom.muOldestBufferIndex;
        giFires = 0;
        const u32 luActual = lRandom.RandomUInt(luMin, luMax);
        const u32 luMod = luMax - luMin + 1u;
        const u32 luExpected = luMin + static_cast<u32>(luSeed >> 32) % luMod;
        Check(luActual == luExpected && lRandom.muSeed == Step(luSeed) && giFires == 0
                  && lRandom.muOldestBufferIndex == luCursor,
              lpcLabel, luSeed, luActual, luExpected);
    }
}

int main()
{
    // ---- RandomUInt(min, max) itself -------------------------------------------------------------
    // HandleShowtimeTrafficBounce's RandomUInt(150, 300): 150 + draw % 151 -- the maximum is drawn.
    for (u32 luHigh = 0; luHigh < 460; ++luHigh)
        ExpectUInt(150, 300, SeedWithHigh(luHigh), "RandomUInt(150, 300) = 150 + draw % 151 (0x82292DD8..0x82292E10)");
    {
        CgsNumeric::Random lRandom = Seeded(SeedWithHigh(150));
        const u32 luTop = lRandom.RandomUInt(150, 300);
        Check(luTop == 300, "RandomUInt(150, 300) reaches 300 (draw % 151 == 150)", SeedWithHigh(150), luTop, 300);
        lRandom = Seeded(SeedWithHigh(151));
        const u32 luBottom = lRandom.RandomUInt(150, 300);
        Check(luBottom == 150, "RandomUInt(150, 300) wraps to 150 at draw 151", SeedWithHigh(151), luBottom, 150);
    }
    // The distribution edges over a whole period of the modulus: every value in [min, max] once.
    {
        unsigned lauHits[151] = {};
        for (u32 luHigh = 0; luHigh < 151; ++luHigh)
        {
            CgsNumeric::Random lRandom = Seeded(SeedWithHigh(luHigh));
            const u32 luValue = lRandom.RandomUInt(150, 300);
            if (luValue >= 150 && luValue <= 300)
                ++lauHits[luValue - 150];
        }
        bool lbEven = true;
        for (unsigned luHits : lauHits)
            lbEven = lbEven && (luHits == 1);
        Check(lbEven, "151 consecutive draws cover [150, 300] exactly once each", 0, 0, 0);
    }
    // Degenerate and wide ranges: min == max still steps the seed; the top of the u32 range.
    ExpectUInt(7, 7, SeedWithHigh(0xDEADBEEFu), "RandomUInt(7, 7) = 7 and steps the seed");
    ExpectUInt(0, 0, SeedWithHigh(0x12345678u), "RandomUInt(0, 0) = 0 and steps the seed");
    ExpectUInt(0x80000000u, 0xFFFFFFFEu, SeedWithHigh(0xFFFFFFFFu), "RandomUInt(2^31, 2^32 - 2)");
    ExpectUInt(0, 0xFFFFFFFEu, SeedWithHigh(0xFFFFFFFEu), "RandomUInt(0, 2^32 - 2) reaches its maximum");
    ExpectUInt(3, 9, 0xC87CD8C91AD0891Bull, "RandomUInt(3, 9) on the Construct() seed");

    // ---- the callers --------------------------------------------------------------------------------
    // Randomize: draw & 0x1FF (0x826C5B2C clrlwi 23).
    for (u32 luHigh = 0; luHigh < 1100; luHigh += 7)
    {
        const u64 luSeed = SeedWithHigh(luHigh * 0x9E3779B9u);
        CgsNumeric::Random lRandom = Seeded(luSeed);
        giFires = 0;
        const u32 luActual = RandomizeDraw(lRandom);
        const u32 luExpected = static_cast<u32>(luSeed >> 32) & 0x1FFu;
        Check(luActual == luExpected && lRandom.muSeed == Step(luSeed) && giFires == 0,
              "Randomize's swap index = draw & 0x1FF (0x826C5B2C)", luSeed, luActual, luExpected);
    }
    // FindRandomOldest<u16, 32>: draw % ((n >> 1) + 1), n < 32 (the "luNumOfItems < NOldest" assert).
    for (u32 luItems = 0; luItems < 32; ++luItems)
    {
        for (u32 luHigh = 0; luHigh < 40; ++luHigh)
        {
            const u64 luSeed = SeedWithHigh(luHigh);
            CgsNumeric::Random lRandom = Seeded(luSeed);
            giFires = 0;
            const u32 luActual = FindRandomOldestDraw(lRandom, static_cast<u16>(luItems));
            const u32 luExpected = static_cast<u32>(luSeed >> 32) % ((luItems >> 1) + 1u);
            Check(luActual == luExpected && lRandom.muSeed == Step(luSeed) && giFires == 0,
                  "FindRandomOldest's pick = draw % ((n >> 1) + 1) (0x8270294C..0x827029B8)", luSeed, luActual,
                  luExpected);
        }
    }
    // GenerateFreeRoamingDestination: draw % (u16)n, the last section included; one section still steps.
    const u32 kauCounts[] = { 1u, 2u, 3u, 7u, 40u, 1000u, 4096u, 65535u, 65536u + 12u };
    for (u32 luCount : kauCounts)
    {
        for (u32 luHigh = 0; luHigh < 64; ++luHigh)
        {
            const u64 luSeed = SeedWithHigh(luHigh * 2654435761u);
            CgsNumeric::Random lRandom = Seeded(luSeed);
            giFires = 0;
            const u32 luActual = FreeRoamDraw(lRandom, luCount);
            const u32 luMod = static_cast<u32>(static_cast<u16>(luCount));
            const u32 luExpected = static_cast<u16>(static_cast<u32>(luSeed >> 32) % luMod);
            Check(luActual == luExpected && lRandom.muSeed == Step(luSeed) && giFires == 0,
                  "GenerateFreeRoamingDestination's section = draw % (u16)n (0x8276960C..0x827696AC)", luSeed,
                  luActual, luExpected);
        }
        const u64 luTopSeed = SeedWithHigh(static_cast<u16>(luCount) - 1u);
        CgsNumeric::Random lRandom = Seeded(luTopSeed);
        const u32 luTop = FreeRoamDraw(lRandom, luCount);
        Check(luTop == static_cast<u16>(luCount) - 1u, "GenerateFreeRoamingDestination can draw the last section",
              luTopSeed, luTop, static_cast<u16>(luCount) - 1u);
    }

    std::printf("FxGateRandomUIntRange: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
