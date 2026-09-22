// FX-AIDRV regression G01-D3 (crash parity 2026-09-22): AICar::GetRandomNumber and AICar::Reset
// share ONE stream -- the CgsNumeric::Random at .data 0x8300D5D0. Both production bodies (and the
// file-scope stream declaration) are extracted verbatim from BrnAICar_Update.cpp by
// run_aidrv_car_random.py; a revision without GetRandomNumber gets a NaN stub.
//
// Console:
//   AICar::Reset @0x82792800 re-runs the inlined Random::Construct on 0x8300D5D0
//   (0x827928E8..0x8279296C): ring words 3F800000 3FE43E6C 3F98B09C 3FDA23E0 3FE21EDC 3FDDEB96
//   3F9C9A72 3F923D76 (stw immediates), muSeed = B5E330D0_2EC654DA (insrdi @0x82792954, std
//   @0x82792958), oldest index 0 (stw @0x8279296C).
//   The draw (inlined GetRandomNumber, ResetAwayFromPlayer 0x827841C0..0x82784220): f31 =
//   ring[oldest] ; ring[oldest] = 0x3F800000 | (oldSeed.hi >> 9) ; seed = seed*K+1 ;
//   oldest = (oldest+1)&7 ; return f31 - 1.0.
// So after a Reset the next eight draws are the eight ring words minus 1.0 and the ninth is the
// refill of slot 0 from the reset seed's high word: 0x3F800000 | (0xB5E330D0 >> 9) = 0x3FDAF198.
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnAI {
void AICar::SetDirection(Vector3 lDirection) { mDirection = lDirection; }
void AICar::SetRight(Vector3 lRight) { mRight = lRight; }
void Aggressiveness::SetAggression(f32 lfAggression) { mfAggressionLevel = lfAggression; mbAggressionLevelSet = true; }
void Aggressiveness::SetProximityToSpeedMatch(f32 lfValue) { mfProximitySpeedMatch = lfValue; }
void Aggressiveness::SetTimeForSpeedMatch(f32 lfValue) { mfTimeForSpeedMatch = lfValue; }
void Aggressiveness::SetRelativeSpeedForMatch(f32 lfValue) { mfRelativeSpeedForSpeedMatch = lfValue; }
void Aggressiveness::SetAcclerationRateForSpeedMatch(f32 lfValue) { mfAcclerationRateForSpeedMatch = lfValue; }
}

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
    f32 FromBits(u32 luBits) { f32 lfValue; std::memcpy(&lfValue, &luBits, sizeof(lfValue)); return lfValue; }
    bool SameBits(f32 lfA, f32 lfB) { return std::memcmp(&lfA, &lfB, sizeof(lfA)) == 0; }

    // The console's stored ring (Reset 0x82792900..0x82792964) and the first refill.
    const u32 KAU_RESET_RING[8] = { 0x3F800000u, 0x3FE43E6Cu, 0x3F98B09Cu, 0x3FDA23E0u,
                                    0x3FE21EDCu, 0x3FDDEB96u, 0x3F9C9A72u, 0x3F923D76u };
    const u32 KU_SLOT0_REFILL = 0x3F800000u | (0xB5E330D0u >> 9);   // == 0x3FDAF198

    void ExpectFreshStream(const AICar& lrCar, const char* lpcWhen)
    {
        char lacLabel[160];
        for (int liDraw = 0; liDraw < 8; ++liDraw)
        {
            const f32 lfExpected = FromBits(KAU_RESET_RING[liDraw]) - 1.0f;
            std::snprintf(lacLabel, sizeof(lacLabel), "%s: draw %d == ring word %08X - 1.0",
                          lpcWhen, liDraw, KAU_RESET_RING[liDraw]);
            Check(SameBits(lrCar.GetRandomNumber(), lfExpected), lacLabel);
        }
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "%s: draw 8 == slot-0 refill 0x3F800000|(0xB5E330D0>>9) - 1.0", lpcWhen);
        Check(SameBits(lrCar.GetRandomNumber(), FromBits(KU_SLOT0_REFILL) - 1.0f), lacLabel);
    }
}

int main()
{
    static AICar sCar{};
    static AICar sOtherCar{};

    // Reset seeds the stream the console stores (ring immediates, seed, oldest index 0).
    sCar.Reset(static_cast<EPersonalityType>(0), false);
    bool lbRing = true;
    for (int liSlot = 0; liSlot < 8; ++liSlot)
        lbRing = lbRing && gAICarRandom.mauIntegerBuffer[liSlot] == KAU_RESET_RING[liSlot];
    Check(lbRing, "Reset stores the console's eight ring words (0x82792900..0x82792964)");
    Check(gAICarRandom.muSeed == 0xB5E330D02EC654DAull, "Reset stores seed B5E330D0_2EC654DA (0x82792958)");
    Check(gAICarRandom.muOldestBufferIndex == 0, "Reset stores oldest index 0 (0x8279296C)");

    // GetRandomNumber draws that stream.
    ExpectFreshStream(sCar, "after Reset");

    // A later Reset of ANY car re-seeds the one shared stream: the next draw -- from any car --
    // restarts at 0.0. (This is what the ResetOnTrack strategies inherit: a reset-on-track that
    // follows an ATTACH_AI_CONTROL Reset draws from the constructed state.)
    for (int liDraw = 0; liDraw < 5; ++liDraw)
        sCar.GetRandomNumber();
    sOtherCar.Reset(static_cast<EPersonalityType>(1), true);
    ExpectFreshStream(sCar, "after another car's Reset");

    Check(gAssertions == 0, "no assertion fired");
    std::printf("AIDrvCarRandom: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
