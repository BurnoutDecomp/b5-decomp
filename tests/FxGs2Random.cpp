// FX-GS2 (crash parity 2026-09-23, G12-D11 part 1): the PRODUCTION CgsNumeric::Random::SetSeed /
// RandomUInt / RandomInt (extracted from src/GameShared/GameClasses/Numeric/CgsRandom.cpp) and
// StreetManager::SetupParRivals' own Random set-up (extracted from its body) by run_fxgs2_random.py,
// against the real CgsRandom.h class (its inline Construct / AddRandomFloatToBuffer).
//
// The oracle below is written from the console, not from the tree:
//   SetSeed (inline at OnRoundStart 0x8236D308.., OnRoundEnd 0x8236D554.., Randomize 0x826C5918..,
//   PaybackComponent::Construct 0x8242E51C.., OnlineStuntRunMode::Start 0x8233A19C..):
//       ring[k] = 0x3F800000 | (hi32(s_k) >> 9) for k = 0..7, s_{k+1} = s_k * 0x5851F42D4C957F2D + 1,
//       muSeed = s_8, muOldestBufferIndex = 0
//   SetupParRivals @0x8233F560: r27 = 0xB5E330D02EC654DA (0x8233F5D8..E8); each pick is
//       hi32(r27) % luMod (divwu, 0x8233F8A8), then r27 = r27 * K + 1 (0x8233F89C/0x8233F8A4)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// The production bodies under test.
#include "random_methods.inc"

using CgsNumeric::Random;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

// ---- the console oracle ----------------------------------------------------------------------
static const u64 KU_K = 0x5851F42D4C957F2Dull;   // lis/ori 0x4C957F2D + insrdi 0x5851F42D (every site)
struct State { u32 mauRing[8]; u32 muIndex; u64 muSeed; };

static State ConsoleSetSeed(u64 luSeed)
{
    State lState;
    for (u32 luSlot = 0; luSlot < 8; ++luSlot)
    {
        lState.mauRing[luSlot] = 0x3F800000u | (static_cast<u32>(luSeed >> 32) >> 9);
        luSeed = luSeed * KU_K + 1u;
    }
    lState.muIndex = 0;
    lState.muSeed  = luSeed;
    return lState;
}

static State Read(const Random& lr)
{
    State lState;
    std::memcpy(lState.mauRing, lr.mauIntegerBuffer, sizeof(lState.mauRing));
    lState.muIndex = lr.muOldestBufferIndex;
    lState.muSeed  = lr.muSeed;
    return lState;
}

static bool SameRing(const State& a, const State& b) { return std::memcmp(a.mauRing, b.mauRing, sizeof(a.mauRing)) == 0; }
static bool Same(const State& a, const State& b) { return SameRing(a, b) && a.muIndex == b.muIndex && a.muSeed == b.muSeed; }

// A generator in a junk state, so a body that leaves a field alone is caught.
static void Junk(Random& lr) { std::memset(&lr, 0xA5, sizeof(lr)); }

int main()
{
    alignas(16) Random lRandom;

    // ---- SetSeed against the console's inline form ----
    {
        const u64 luSeed = 0x0123456789ABCDEFull;
        Junk(lRandom);
        lRandom.SetSeed(luSeed);
        const State lGot = Read(lRandom), lWant = ConsoleSetSeed(luSeed);
        Check(SameRing(lGot, lWant), "SetSeed primes all 8 ring slots with F(hi32(s_0..s_7)) in order  @0x8236D334..0x8236D4B4");
        Check(lGot.muIndex == 0u, "SetSeed leaves the cursor at 0 (the final (7 + 1) & 7 wrap)  @0x8236D4C4");
        Check(lGot.muSeed == lWant.muSeed, "SetSeed leaves the seed eight LCG steps on (no OR into the seed)  @0x8236D4B0");
    }
    {
        // Randomize passes a u32 (`clrldi r9, r4, 32`): hi32 == 0, so ring[0] == 1.0f.
        const u64 luSeed = static_cast<u64>(static_cast<u32>(0xDEADBEEFu));
        Junk(lRandom);
        lRandom.SetSeed(luSeed);
        Check(Same(Read(lRandom), ConsoleSetSeed(luSeed)), "SetSeed((u64)(u32)0xDEADBEEF) == the Randomize expansion  @0x826C5918..0x826C5AE8");
        Check(lRandom.mauIntegerBuffer[0] == 0x3F800000u, "...ring[0] = F(0) = 1.0f (0x3F800000)  @0x826C5958");
    }
    {
        // OnRoundStart / OnRoundEnd sign-extend the frame count (`extsw`).
        const u64 luSeed = static_cast<u64>(static_cast<s64>(static_cast<s32>(0x80000001u)));
        Junk(lRandom);
        lRandom.SetSeed(luSeed);
        Check(Same(Read(lRandom), ConsoleSetSeed(luSeed)) && lRandom.mauIntegerBuffer[0] == 0x3FFFFFFFu,
              "SetSeed((s64)(s32)0x80000001) == the OnRoundStart expansion (ring[0] = 0x3FFFFFFF)  @0x8236D314");
    }
    {
        // PaybackComponent::Construct / OnlineStuntRunMode::Start: an inlined Construct() then SetSeed --
        // SetSeed rewrites every field, so the pair ends in SetSeed's state.
        const u64 luSeed = static_cast<u64>(static_cast<s64>(static_cast<s32>(42)));
        alignas(16) Random lFresh;
        Junk(lFresh);
        lFresh.SetSeed(luSeed);
        Junk(lRandom);
        lRandom.Construct();
        lRandom.SetSeed(luSeed);
        Check(Same(Read(lRandom), Read(lFresh)) && Same(Read(lRandom), ConsoleSetSeed(luSeed)),
              "Construct() + SetSeed(42) == SetSeed(42) == the PaybackComponent expansion  @0x8242E5F8..0x8242E79C");
    }
    {
        // Construct is SetSeed(DWARF KU_RANDOM_DEFAULT_SEED = 2413850050) with the first step folded.
        alignas(16) Random lSeeded;
        Junk(lSeeded);
        lSeeded.SetSeed(2413850050ull);
        Junk(lRandom);
        lRandom.Construct();
        Check(Same(Read(lRandom), Read(lSeeded)), "Construct() == SetSeed(2413850050) (DWARF CgsRandom.h:34)");
    }

    // ---- SetupParRivals: its own seeding statements, then its draws ----
    {
        Junk(lRandom);
        ParRivalsSeed::Seed(lRandom);
        Check(lRandom.muSeed == 0xB5E330D02EC654DAull,
              "SetupParRivals' generator starts at the console's r27 = 0xB5E330D02EC654DA  @0x8233F5D8..0x8233F5E8");
        u64 luR27 = 0xB5E330D02EC654DAull;
        bool lbSame = true;
        const s32 laiCounts[5] = { 3, 5, 2, 7, 4 };   // liRivalsFound values: luMod = liMax - 0 + 1
        for (s32 liPick = 0; liPick < 5; ++liPick)
        {
            const u32 luMod  = static_cast<u32>(laiCounts[liPick]);
            const s32 liWant = static_cast<s32>(static_cast<u32>(luR27 >> 32) % luMod);   // srdi / divwu
            luR27 = luR27 * KU_K + 1u;                                                   // mulld / addi 1
            lbSame = lbSame && lRandom.RandomInt(0, laiCounts[liPick] - 1) == liWant;
        }
        Check(lbSame, "SetupParRivals' first five picks match the console's r27 draws  @0x8233F898..0x8233F8B8");
    }

    Check(gAsserts == 0, "valid draws fire no assert");
    std::printf("FxGs2Random: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
