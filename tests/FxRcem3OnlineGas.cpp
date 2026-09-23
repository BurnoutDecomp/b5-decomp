// FX-RCEM3 (crash-parity 2026-09-23, G67-D9): the ONLINE_RACE catch-up gas of
// RaceCarEntityModule::ProcessPlayerVehicleInput (ARTIST 0x82300530..0x82300588), extracted VERBATIM
// (the `if( lpScoring->miNumPlayersInGame > 1 )` block) by run_fxrcem3_online_gas.py.
//
// Console:  q  = (f32)(u32)(pos - 1) / (f32)(s32)(n - 1)          fcfid/frsp, fdivs
//           f0 = fmadds(q, flt_8201F7F8 = 0x3DCCCCD0, flt_82005450 = 0x3F666666)   ONE rounding
//           mfGas = f0 * lfGas                                     fmuls
// The expected values are computed here by an exact emulation (the product of two f32 fits a double
// exactly, and so does its sum with 0.9f for q in [0,1]), cross-checked against six values the
// independent verifier derived by exact rational arithmetic.
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
}

namespace Fixture {
struct CarScoreData { s32 miRacePosition; s32 GetRacePosition() const { return miRacePosition; } };
struct ScoringInterface { CarScoreData maCarScoreData[8]; s32 miNumPlayersInGame; };
struct Car { EActiveRaceCarIndex meIndex; EActiveRaceCarIndex GetActiveRaceCarIndex() const { return meIndex; } };
struct Controls { f32 mfGas; };

f32 Run(const ScoringInterface* lpScoring, const Car* lpActiveRaceCar, f32 lfGas)
{
    Controls lControls = { -1.0f };
#include "fxrcem3_online_gas.inc"
    return lControls.mfGas;
}
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }

// The console arithmetic, bit-exact: single-rounded divide, then a single-rounded fused multiply-add.
static f32 Console(u32 luPos, s32 liPlayers)
{
    const f32 lfNumerator = static_cast<f32>(luPos - 1u);                 // clrldi 32 ; fcfid ; frsp
    const f32 lfDenominator = static_cast<f32>(liPlayers - 1);           // extsw ; fcfid ; frsp
    const f32 lfQuotient = lfNumerator / lfDenominator;                  // fdivs
    const double ldExact = static_cast<double>(lfQuotient) * static_cast<double>(FromBits(0x3DCCCCD0u))
                         + static_cast<double>(FromBits(0x3F666666u));  // exact in double
    return static_cast<f32>(ldExact);                                    // fmadds: one rounding
}

int main()
{
    // The emulation reproduces the verifier's six pinned console values.
    struct Pin { u32 pos; s32 n; u32 bits; };
    const Pin laPins[] = { { 4, 5, 0x3F79999Au }, { 5, 6, 0x3F7AE148u }, { 6, 7, 0x3F7BBBBCu },
                           { 8, 2, 0x3FCCCCCEu }, { 8, 5, 0x3F89999Au }, { 7, 8, 0x3F7C57C5u } };
    for (const Pin& p : laPins)
        Check(Bits(Console(p.pos, p.n)) == p.bits, "emulation reproduces the verifier's pinned console value");

    Fixture::ScoringInterface lScoring; std::memset(&lScoring, 0, sizeof(lScoring));
    Fixture::Car lCar = { E_ACTIVE_RACE_CAR_INDEX_2 };
    int liStates = 0, liStateFailures = 0;
    for (s32 n = 2; n <= 8; ++n)
    {
        for (u32 pos = 1; pos <= 8; ++pos)
        {
            if (pos > static_cast<u32>(n) && pos != 8u) continue;   // 1..n, plus the ClearData seed 8
            lScoring.miNumPlayersInGame = n;
            lScoring.maCarScoreData[2].miRacePosition = static_cast<s32>(pos);
            const f32 lfGot = Fixture::Run(&lScoring, &lCar, 1.0f);
            const f32 lfWant = Console(pos, n);
            ++liStates;
            char lacName[128];
            std::snprintf(lacName, sizeof(lacName), "G67-D9 (pos %u, players %d): mfGas 0x%08X == console 0x%08X",
                          pos, n, Bits(lfGot), Bits(lfWant));
            if (Bits(lfGot) != Bits(lfWant)) ++liStateFailures;
            Check(Bits(lfGot) == Bits(lfWant), lacName);
        }
    }
    std::printf("G67-D9 states: %d, mismatches: %d\n", liStates, liStateFailures);

    // The trailing fmuls by lfGas.
    lScoring.miNumPlayersInGame = 5; lScoring.maCarScoreData[2].miRacePosition = 4;
    Check(Bits(Fixture::Run(&lScoring, &lCar, 0.75f)) == Bits(Console(4, 5) * 0.75f),
          "G67-D9 mfGas = factor * lfGas (fmuls f0, f0, f27 @0x82300584)");

    Check(guAssertions == 0, "valid fixtures fire no assertions");
    std::printf("FxRcem3OnlineGas: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
