// FX-GATE (crash parity 2026-09-25): XboxMath::XMScalarSinCos (src/SDKs/XboxMath/XMScalarSinCos.h) against the
// console's own XMScalarSinCos @0x821F0C08 run whole on emu64 (FxGateXMScalarSinCosData.h), bit for bit. With
// FXGATE_AS_STD the same rows are run through std::sin / std::cos, which is what the PC used before: RED.
#include "SDKs/XboxMath/XMScalarSinCos.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "FxGateXMScalarSinCosData.h"

namespace
{
    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
    bool IsNaNBits(unsigned lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
    bool Same(unsigned luA, unsigned luB) { return (IsNaNBits(luA) && IsNaNBits(luB)) || luA == luB; }
}

int main()
{
    unsigned luChecks = 0, luFailures = 0, luDiffering = 0;
    for (const XMScalarSinCosRow& lrRow : kaXMScalarSinCosRows)
    {
        float lfSin = 0.0f, lfCos = 0.0f;
#ifdef FXGATE_AS_STD
        lfSin = std::sin(Float(lrRow.muIn));
        lfCos = std::cos(Float(lrRow.muIn));
#else
        XboxMath::XMScalarSinCos(&lfSin, &lfCos, Float(lrRow.muIn));
#endif
        ++luChecks;
        luDiffering += lrRow.mbDiffers ? 1u : 0u;
        if (!Same(Bits(lfSin), lrRow.muSin) || !Same(Bits(lfCos), lrRow.muCos))
        {
            if (luFailures < 6)
                std::printf("FAIL  XMScalarSinCos(0x%08X = %.9g): 0x%08X 0x%08X, console 0x%08X 0x%08X\n", lrRow.muIn,
                            Float(lrRow.muIn), Bits(lfSin), Bits(lfCos), lrRow.muSin, lrRow.muCos);
            ++luFailures;
        }
    }
    std::printf("%u rows where the correctly rounded sin / cos differ from the console's\n", luDiffering);
    std::printf("FxGateXMScalarSinCos: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures == 0 ? 0 : 1;
}
