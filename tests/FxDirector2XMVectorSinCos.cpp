// FX-DIRECTOR2 (crash parity 2026-09-25): XboxMath::XMVectorSinCos (src/SDKs/XboxMath/XMVectorSinCos.h) against the
// console.
//
// THE REFERENCE is the console itself: FxDirector2XMVectorSinCosGolden.h holds the sine / cosine the ARTIST words
// produce -- CameraRig::Construct @0x8220B0E8's inlined XMVectorSinCos block (0x8220B1B8..0x8220B3F0), run on a
// PPC/VMX128 emulator whose fused ops round exactly once (the generator, gen_sincos_golden.py, is FX-DIRECTOR2
// scratch; its table is committed beside this file). 243 angles: 0 and -0, +-pi and its neighbours, the
// range-reduction edges where vrfin flips (V / 2pi within two ulps of k + 1/2), small, large and huge values, the rig
// presets' degrees through the 0.017453292 fmuls, +-inf, NaN, and 160 random angles.
//
// Each case compares both results BIT FOR BIT (a NaN only has to be a NaN). The last check proves the table can tell
// the console's polynomial from the host library: std::sin / std::cos must miss it somewhere. Built with
// -DFXD2_SINCOS_AS_STD the function under test IS std::sin / std::cos, and the run must fail
// (run_fxdirector2_xmvector_sincos.py --as-std).
#include <cmath>
#include <cstdio>
#include <cstring>
#include "SDKs/XboxMath/XMVectorSinCos.h"
#include "FxDirector2XMVectorSinCosGolden.h"

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

static unsigned int Bits(float lfValue)
{
    unsigned int luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

static float Float(unsigned int luBits)
{
    float lfValue;
    std::memcpy(&lfValue, &luBits, sizeof(lfValue));
    return lfValue;
}

static bool Same(float lfValue, unsigned int luGolden)
{
    const float lfGolden = Float(luGolden);
    if (lfGolden != lfGolden)
        return lfValue != lfValue;
    return Bits(lfValue) == luGolden;
}

static void UnderTest(float lfAngle, float& lrfSin, float& lrfCos)
{
#if defined(FXD2_SINCOS_AS_STD)
    lrfSin = std::sin(lfAngle);
    lrfCos = std::cos(lfAngle);
#else
    XboxMath::XMVectorSinCos(&lrfSin, &lrfCos, lfAngle);
#endif
}

int main()
{
    const int liCases = static_cast<int>(sizeof(kaSinCosCases) / sizeof(kaSinCosCases[0]));
    int liStdMisses = 0, liFinite = 0;
    for (int i = 0; i < liCases; ++i)
    {
        const SinCosCase& lrCase = kaSinCosCases[i];
        const float lfAngle = Float(lrCase.muAngle);
        float lfSin = 0.0f, lfCos = 0.0f;
        UnderTest(lfAngle, lfSin, lfCos);
        char lacName[200];
        std::snprintf(lacName, sizeof(lacName),
                      "S%03d %s (0x%08X): sin 0x%08X cos 0x%08X, the console 0x%08X / 0x%08X", i, lrCase.mpcLabel,
                      lrCase.muAngle, Bits(lfSin), Bits(lfCos), lrCase.muSin, lrCase.muCos);
        Check(Same(lfSin, lrCase.muSin) && Same(lfCos, lrCase.muCos), lacName);

        if (std::isfinite(lfAngle) && std::fabs(lfAngle) < 1.0e6f)
        {
            ++liFinite;
            if (!Same(std::sin(lfAngle), lrCase.muSin) || !Same(std::cos(lfAngle), lrCase.muCos))
                ++liStdMisses;
        }
    }
    std::printf("std::sin / std::cos miss the console on %d of %d finite angles\n", liStdMisses, liFinite);
    Check(liStdMisses > 0, "the table discriminates: std::sin / std::cos miss the console somewhere");

    std::printf("FxDirector2XMVectorSinCos: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
