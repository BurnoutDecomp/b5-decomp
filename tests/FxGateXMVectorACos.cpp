// FX-GATE item 6: the shared XboxMath::XMVectorACos (src/SDKs/XboxMath/XMVectorACos.h) against the console's own
// XMVectorACos (X360 0x821F0980), whose REAL instruction words were run on the campaign interpreter emu64 for every
// row of FxGateXMVectorACosData.h (vmaddfp / vnmsubfp exact and rounded once, vrsqrtefp exact-then-rounded -- the
// same estimate model the header uses).
//
// Checks: every row bit for bit (a NaN row only as NaN), the landmarks the console gives and std::acos does not
// (acos(1) = 0x35000000, acos(-1) = 0x40490FD9), NaN for |x| > 1 and for NaN, and that std::acos really differs
// from the console on the grid (so a call site that keeps std::acos is not the console's).
#include "SDKs/XboxMath/XMVectorACos.h"
#include "FxGateXMVectorACosData.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    unsigned gChecks = 0, gFailures = 0;

    float Float(unsigned int luBits) { float lf; std::memcpy(&lf, &luBits, 4); return lf; }
    unsigned int Bits(float lf) { unsigned int lu; std::memcpy(&lu, &lf, 4); return lu; }

    void Check(bool lbPassed, const char* lpcLabel, unsigned int luIn, unsigned int luOut, unsigned int luWant)
    {
        ++gChecks;
        if (!lbPassed)
        {
            ++gFailures;
            std::printf("FAIL  %s: x 0x%08X (%.9g) -> 0x%08X (%.9g), console 0x%08X (%.9g)\n", lpcLabel, luIn,
                        Float(luIn), luOut, Float(luOut), luWant, Float(luWant));
        }
    }
}

int main()
{
    // 1. Every console row.
    unsigned luStdDiffers = 0, luFinite = 0;
    for (const FxGateACosRow& lrRow : kaFxGateACosRows)
    {
        const float lfIn  = Float(lrRow.muIn);
        const float lfOut = XboxMath::XMVectorACos(lfIn);
        const float lfWant = Float(lrRow.muOut);
        const bool lbOk = std::isnan(lfWant) ? std::isnan(lfOut) : (Bits(lfOut) == lrRow.muOut);
        Check(lbOk, "XMVectorACos row (emu64 on 0x821F0980)", lrRow.muIn, Bits(lfOut), lrRow.muOut);
        if (!std::isnan(lfWant))
        {
            ++luFinite;
            if (Bits(std::acos(lfIn)) != lrRow.muOut)
                ++luStdDiffers;
        }
    }

    // 2. The landmarks.
    const struct { unsigned int muIn, muOut; const char* mpcWhat; } kaLandmarks[] = {
        { 0x3F800000u, 0x35000000u, "acos(1) is 2^-21, not 0 (P(1) = 1.5707959)" },
        { 0xBF800000u, 0x40490FD9u, "acos(-1) is pi - 2 ulp" },
        { 0x00000000u, 0x3FC90FDBu, "acos(+0) is pi / 2 (D0 * 0.5)" },
        { 0x80000000u, 0x3FC90FDBu, "acos(-0) is pi / 2" },
        { 0x3F000000u, 0x3F860A92u, "acos(0.5)" },
        { 0xBF000000u, 0x40060A92u, "acos(-0.5)" },
    };
    for (const auto& lrLandmark : kaLandmarks)
    {
        const float lfOut = XboxMath::XMVectorACos(Float(lrLandmark.muIn));
        Check(Bits(lfOut) == lrLandmark.muOut, lrLandmark.mpcWhat, lrLandmark.muIn, Bits(lfOut), lrLandmark.muOut);
    }

    // 3. Out of the domain: NaN, including 1.00000012 (oma = 0 -> e = 0 * inf) and the NaNs.
    const unsigned int kauNaNInputs[] = { 0x3F800001u, 0xBF800001u, 0x3FC00000u, 0xC0000000u, 0x7F800000u,
                                          0xFF800000u, 0x7FC00000u, 0xFFC00000u, 0x7FA00000u };
    for (unsigned int luIn : kauNaNInputs)
    {
        const float lfOut = XboxMath::XMVectorACos(Float(luIn));
        Check(std::isnan(lfOut), "|x| > 1, +-inf and NaN give NaN", luIn, Bits(lfOut), 0x7FC00000u);
    }

    // 4. std::acos is not the console: it misses on a good share of the finite rows.
    ++gChecks;
    if (luStdDiffers * 10u < luFinite)
    {
        ++gFailures;
        std::printf("FAIL  std::acos matched the console on %u of %u finite rows -- the rows no longer separate them\n",
                    luFinite - luStdDiffers, luFinite);
    }
    std::printf("(std::acos differs from the console on %u of %u finite rows)\n", luStdDiffers, luFinite);

    std::printf("FxGateXMVectorACos: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
