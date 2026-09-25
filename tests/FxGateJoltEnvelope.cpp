// FX-GATE: CgsInput::InputPads::UpdateJoltEnvelope (X360 0x828E7618), the body EXTRACTED from
// src/GameShared/GameClasses/System/Input/CgsInputPads.cpp by run_fxgate_jolt_envelope.py, against a
// model of the console's instructions:
//   0x828E7640 fcmpu t, 0.0 (flt_82001CC0) ; 0x828E7644 bge -> past the assert (cpp :843, li r5 0x34B)
//   0x828E766C fcmpu t, attack             ; 0x828E7670 bge -> on, else return peak (+0x10)
//   0x828E7680 fadds decay + attack        ; 0x828E7688 bge -> on, else
//              0x828E768C..0x828E76A0 fsubs t-attack ; fdivs /decay ; fsubs level-peak ;
//              fmadds f1 = ratio * (level - peak) + peak                           (ONE rounding)
//   0x828E76AC/0x828E76B0 (sustain + decay) + attack ; 0x828E76B8 bge -> on, else return level (+0x14)
//   0x828E76C8..0x828E76D0 ((release + sustain) + decay) + attack ; 0x828E76D8 bge -> 0x828E76F4
//              fmr f1, f30 (0.0), else 0x828E76DC..0x828E76EC fsubs t-sustainEnd ; fneg level ;
//              fdivs /release ; fmadds f1 = ratio * -level + level                (ONE rounding)
// bge is bc 4,lt: TAKEN on an unordered compare. So a NaN time skips the assert, falls past every
// `<` test and takes the 0x828E76D8 bge to 0.0; the two lerps round once (fmadds).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

typedef float f32;
typedef int   s32;

namespace
{
    int giFires = 0;
}

#define CGS_ASSERT(condition, msg) do { if (!(condition)) { ++giFires; } } while (0)

namespace InputIO
{
    // CgsInputModuleIO.h:34 -- the offsets the console reads: 0/4/8/0xC/0x10/0x14.
    struct JoltEnvelope
    {
        f32 mfAttackTime;
        f32 mfDecayTime;
        f32 mfSustainTime;
        f32 mfReleaseTime;
        f32 mfPeakSpeedValue;
        f32 mfSustainSpeedValue;
    };
}

struct InputPads
{
    f32 UpdateJoltEnvelope(const InputIO::JoltEnvelope& lEnvelope, f32 lfTime);
};

#include "extracted.inc"

namespace
{
    int giChecks   = 0;
    int giFailures = 0;

    uint32_t Bits(float lfValue)
    {
        uint32_t luBits;
        std::memcpy(&luBits, &lfValue, sizeof(luBits));
        return luBits;
    }

    float FromBits(uint32_t luBits)
    {
        float lfValue;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }

    // fcmpu's LT bit: set only for an ORDERED a < b.
    bool Lt(float lfA, float lfB)
    {
        return !std::isnan(lfA) && !std::isnan(lfB) && lfA < lfB;
    }

    float Console(const InputIO::JoltEnvelope& lrE, float lfTime, int& liFires)
    {
        if (Lt(lfTime, 0.0f)) ++liFires;                                     // 0x828E7644 bge not taken
        if (Lt(lfTime, lrE.mfAttackTime)) return lrE.mfPeakSpeedValue;       // 0x828E7670
        const float lfDecayEnd = lrE.mfDecayTime + lrE.mfAttackTime;         // 0x828E7680
        if (Lt(lfTime, lfDecayEnd))                                          // 0x828E7688
        {
            const float lfRatio = (lfTime - lrE.mfAttackTime) / lrE.mfDecayTime;
            return std::fmaf(lfRatio, lrE.mfSustainSpeedValue - lrE.mfPeakSpeedValue,
                             lrE.mfPeakSpeedValue);                          // 0x828E76A0 fmadds
        }
        const float lfSustainEnd = (lrE.mfSustainTime + lrE.mfDecayTime) + lrE.mfAttackTime;
        if (Lt(lfTime, lfSustainEnd)) return lrE.mfSustainSpeedValue;        // 0x828E76B8
        const float lfEnd = ((lrE.mfReleaseTime + lrE.mfSustainTime) + lrE.mfDecayTime) + lrE.mfAttackTime;
        if (!Lt(lfTime, lfEnd)) return 0.0f;                                 // 0x828E76D8 bge -> 0x828E76F4
        const float lfRatio = (lfTime - lfSustainEnd) / lrE.mfReleaseTime;
        return std::fmaf(lfRatio, -lrE.mfSustainSpeedValue, lrE.mfSustainSpeedValue);   // 0x828E76EC fmadds
    }

    void Expect(const InputIO::JoltEnvelope& lrE, float lfTime, const char* lpcWhat)
    {
        InputPads lPads;
        int liConsoleFires = 0;
        const float lfExpected = Console(lrE, lfTime, liConsoleFires);
        giFires = 0;
        const float lfActual = lPads.UpdateJoltEnvelope(lrE, lfTime);
        ++giChecks;
        if (Bits(lfActual) != Bits(lfExpected) || giFires != liConsoleFires)
        {
            ++giFailures;
            std::printf("FAIL: %s: t=%.9g (0x%08X) -> 0x%08X (%.9g), assert fires %d; console 0x%08X (%.9g), fires %d\n",
                        lpcWhat, lfTime, Bits(lfTime), Bits(lfActual), lfActual, giFires,
                        Bits(lfExpected), lfExpected, liConsoleFires);
        }
    }
}

int main()
{
    // A searched envelope whose decay lerp rounds differently fused and unfused.
    InputIO::JoltEnvelope lDecay = {};
    lDecay.mfAttackTime        = FromBits(0x3E4CCCCDu);   // 0.2
    lDecay.mfDecayTime         = FromBits(0x3E4CCCCDu);   // 0.2
    lDecay.mfSustainTime       = 0.5f;
    lDecay.mfReleaseTime       = 0.5f;
    lDecay.mfPeakSpeedValue    = FromBits(0x3F79A8D7u);
    lDecay.mfSustainSpeedValue = FromBits(0x3F39D17Au);

    // And one whose release lerp does.
    InputIO::JoltEnvelope lRelease = {};
    lRelease.mfAttackTime        = FromBits(0x3D4CCCCDu); // 0.05
    lRelease.mfDecayTime         = FromBits(0x3E99999Au); // 0.3
    lRelease.mfSustainTime       = FromBits(0x3E4CCCCDu); // 0.2
    lRelease.mfReleaseTime       = FromBits(0x3E99999Au); // 0.3
    lRelease.mfPeakSpeedValue    = 1.0f;
    lRelease.mfSustainSpeedValue = FromBits(0x3F5E3286u);

    // The oracle must disagree with a twice-rounded lerp on the two searched times, or the fused
    // checks below would not bite.
    {
        const float lfT  = FromBits(0x3E9C6DE4u);
        const float lfR  = (lfT - lDecay.mfAttackTime) / lDecay.mfDecayTime;
        const float lfD  = lDecay.mfSustainSpeedValue - lDecay.mfPeakSpeedValue;
        volatile float lfProduct = lfR * lfD;
        const float lfTwice = lfProduct + lDecay.mfPeakSpeedValue;
        ++giChecks;
        if (Bits(lfTwice) == Bits(std::fmaf(lfR, lfD, lDecay.mfPeakSpeedValue)))
        {
            ++giFailures;
            std::printf("FAIL: the decay case no longer separates fused from unfused\n");
        }
    }

    const float lfNaN = std::numeric_limits<float>::quiet_NaN();
    Expect(lDecay, lfNaN, "NaN time (0x828E7644 bge skips the assert, 0x828E76D8 bge returns 0.0)");
    Expect(lDecay, -lfNaN, "negative NaN time");
    Expect(lDecay, -1.0f, "negative time fires the assert and returns the peak");
    Expect(lDecay, 0.0f, "t = 0");
    Expect(lDecay, 0.1f, "attack");
    Expect(lDecay, lDecay.mfAttackTime, "t = attack (decay starts)");
    Expect(lDecay, FromBits(0x3E9C6DE4u), "decay lerp, one rounding (0x828E76A0 fmadds)");
    Expect(lDecay, 0.35f, "decay lerp");
    Expect(lDecay, 0.5f, "sustain");
    Expect(lDecay, 1.2f, "release lerp");
    Expect(lDecay, 1.4f, "t = end");
    Expect(lDecay, 2.0f, "after the end");
    Expect(lDecay, std::numeric_limits<float>::infinity(), "+inf");

    Expect(lRelease, FromBits(0x3F4A1EC0u), "release lerp, one rounding (0x828E76EC fmadds)");
    Expect(lRelease, 0.6f, "release lerp");
    Expect(lRelease, 0.84999996f, "just before the end");
    Expect(lRelease, lfNaN, "NaN time, second envelope");

    // A few thousand ordered times across both envelopes: the value and the assert agree everywhere.
    for (int liStep = -50; liStep < 2000; ++liStep)
    {
        const float lfTime = static_cast<float>(liStep) * 0.00073f;
        Expect(lDecay, lfTime, "sweep (first envelope)");
        Expect(lRelease, lfTime, "sweep (second envelope)");
    }

    std::printf("%d/%d checks passed\n", giChecks - giFailures, giChecks);
    return giFailures == 0 ? 0 : 1;
}
