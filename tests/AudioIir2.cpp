#include <cmath>
#include <cstdio>
#include <cstring>
#include "rw/audio/core/Iir2.h"
#include "audio_iir2.inc"

using namespace rw::audio::core;

namespace
{
enum OpCode { E_LOAD, E_STORE, E_MUL, E_MADD, E_NMSUB, E_ADD, E_MOVE };
struct Instruction { OpCode meOp; int miDst, miA, miB, miC; };

// Frozen floating-point trace from ARTIST 82B69E18..82B69F28. Registers and
// instruction order stay literal here so the oracle does not share the host
// implementation's recurrence or feedforward factoring. Load/store indices
// are relative to the eight-sample group (the PPC increments r5 halfway through).
const Instruction KA_TRACE[] = {
    {E_LOAD, 8, 0, 0, 0}, // 0x82b69e18
    {E_MUL, 27, 8, 12, 0}, // 0x82b69e1c
    {E_LOAD, 7, 1, 0, 0}, // 0x82b69e20
    {E_MUL, 26, 7, 12, 0}, // 0x82b69e24
    {E_LOAD, 5, 3, 0, 0}, // 0x82b69e28
    {E_MUL, 25, 7, 11, 0}, // 0x82b69e2c
    {E_LOAD, 6, 2, 0, 0}, // 0x82b69e30
    {E_MUL, 24, 6, 11, 0}, // 0x82b69e34
    {E_LOAD, 4, 4, 0, 0}, // 0x82b69e38
    {E_LOAD, 3, 5, 0, 0}, // 0x82b69e3c
    {E_LOAD, 2, 6, 0, 0}, // 0x82b69e40
    {E_LOAD, 29, 7, 0, 0}, // 0x82b69e44
    {E_MADD, 28, 28, 10, 27}, // 0x82b69e50
    {E_MADD, 26, 8, 11, 26}, // 0x82b69e54
    {E_MUL, 27, 5, 11, 0}, // 0x82b69e58
    {E_MADD, 8, 8, 10, 25}, // 0x82b69e5c
    {E_MADD, 7, 7, 10, 24}, // 0x82b69e60
    {E_MUL, 25, 4, 11, 0}, // 0x82b69e64
    {E_MADD, 28, 30, 11, 28}, // 0x82b69e68
    {E_MADD, 30, 30, 10, 26}, // 0x82b69e6c
    {E_MADD, 27, 6, 10, 27}, // 0x82b69e70
    {E_MADD, 8, 6, 12, 8}, // 0x82b69e74
    {E_MADD, 7, 5, 12, 7}, // 0x82b69e78
    {E_MADD, 5, 5, 10, 25}, // 0x82b69e7c
    {E_ADD, 6, 28, 9, 0}, // 0x82b69e80
    {E_ADD, 30, 30, 9, 0}, // 0x82b69e84
    {E_MADD, 28, 4, 12, 27}, // 0x82b69e88
    {E_ADD, 27, 8, 9, 0}, // 0x82b69e8c
    {E_ADD, 26, 7, 9, 0}, // 0x82b69e90
    {E_MADD, 5, 3, 12, 5}, // 0x82b69e94
    {E_NMSUB, 8, 1, 0, 6}, // 0x82b69e98
    {E_ADD, 28, 28, 9, 0}, // 0x82b69e9c
    {E_ADD, 5, 5, 9, 0}, // 0x82b69ea0
    {E_NMSUB, 8, 31, 13, 8}, // 0x82b69ea4
    {E_STORE, 8, 0, 0, 0}, // 0x82b69ea8
    {E_NMSUB, 7, 8, 0, 30}, // 0x82b69eac
    {E_MUL, 30, 3, 11, 0}, // 0x82b69eb0
    {E_NMSUB, 7, 1, 13, 7}, // 0x82b69eb4
    {E_STORE, 7, 1, 0, 0}, // 0x82b69eb8
    {E_MADD, 4, 4, 10, 30}, // 0x82b69ebc
    {E_MOVE, 30, 29, 0, 0}, // 0x82b69ec0
    {E_NMSUB, 6, 7, 0, 27}, // 0x82b69ec4
    {E_NMSUB, 6, 8, 13, 6}, // 0x82b69ec8
    {E_STORE, 6, 2, 0, 0}, // 0x82b69ecc
    {E_NMSUB, 1, 6, 0, 26}, // 0x82b69ed0
    {E_NMSUB, 1, 7, 13, 1}, // 0x82b69ed4
    {E_STORE, 1, 3, 0, 0}, // 0x82b69ed8
    {E_MADD, 7, 2, 12, 4}, // 0x82b69edc
    {E_NMSUB, 31, 1, 0, 28}, // 0x82b69ee0
    {E_MUL, 28, 2, 11, 0}, // 0x82b69ee4
    {E_ADD, 7, 7, 9, 0}, // 0x82b69ee8
    {E_NMSUB, 31, 6, 13, 31}, // 0x82b69eec
    {E_STORE, 31, 4, 0, 0}, // 0x82b69ef0
    {E_MADD, 8, 3, 10, 28}, // 0x82b69ef4
    {E_MOVE, 28, 2, 0, 0}, // 0x82b69ef8
    {E_NMSUB, 6, 31, 0, 5}, // 0x82b69efc
    {E_MADD, 5, 29, 12, 8}, // 0x82b69f00
    {E_NMSUB, 8, 1, 13, 6}, // 0x82b69f04
    {E_STORE, 8, 5, 0, 0}, // 0x82b69f08
    {E_ADD, 6, 5, 9, 0}, // 0x82b69f0c
    {E_NMSUB, 7, 8, 0, 7}, // 0x82b69f10
    {E_NMSUB, 7, 31, 13, 7}, // 0x82b69f14
    {E_STORE, 7, 6, 0, 0}, // 0x82b69f18
    {E_MOVE, 31, 7, 0, 0}, // 0x82b69f1c
    {E_NMSUB, 7, 7, 0, 6}, // 0x82b69f20
    {E_NMSUB, 1, 8, 13, 7}, // 0x82b69f24
    {E_STORE, 1, 7, 0, 0}, // 0x82b69f28
};

void ConsoleFilter(Iir2State& lrState, f32* lpDst, const f32* lpSrc,
                   const Iir2Coeffs& lrCoeffs)
{
    f32 laRegs[32] = {};
    laRegs[0] = lrCoeffs.mfA1; laRegs[13] = lrCoeffs.mfA2;
    laRegs[12] = lrCoeffs.mfB0; laRegs[11] = lrCoeffs.mfB1;
    laRegs[10] = lrCoeffs.mfB2; laRegs[9] = 1.0e-18f;
    laRegs[30] = lrState.mfX1; laRegs[28] = lrState.mfX2;
    laRegs[1] = lrState.mfY1; laRegs[31] = lrState.mfY2;
    for (int liBase = 0; liBase < Iir2::KI_BLOCK_SAMPLES; liBase += 8)
    {
        for (const Instruction& lrOp : KA_TRACE)
        {
            switch (lrOp.meOp)
            {
            case E_LOAD: laRegs[lrOp.miDst] = lpSrc[liBase + lrOp.miA]; break;
            case E_STORE: lpDst[liBase + lrOp.miA] = laRegs[lrOp.miDst]; break;
            case E_MUL: laRegs[lrOp.miDst] = laRegs[lrOp.miA] * laRegs[lrOp.miB]; break;
            case E_ADD: laRegs[lrOp.miDst] = laRegs[lrOp.miA] + laRegs[lrOp.miB]; break;
            case E_MOVE: laRegs[lrOp.miDst] = laRegs[lrOp.miA]; break;
            // All inputs here are binary32. Binary64 holds their product
            // exactly; these bounded test vectors also avoid double-rounding
            // ties, giving a separate model of each single-rounding PPC site.
            case E_MADD:
                laRegs[lrOp.miDst] = static_cast<f32>(
                    double(laRegs[lrOp.miA]) * double(laRegs[lrOp.miB]) + double(laRegs[lrOp.miC]));
                break;
            case E_NMSUB:
                laRegs[lrOp.miDst] = -static_cast<f32>(
                    double(laRegs[lrOp.miA]) * double(laRegs[lrOp.miB]) - double(laRegs[lrOp.miC]));
                break;
            }
        }
    }
    lrState = {laRegs[30], laRegs[28], laRegs[1], laRegs[31]};
}

int giChecks = 0, giFailures = 0;
void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL %s\n", lpcName); }
}

bool Same(const f32* lpActual, const f32* lpExpected, int liCount)
{
    return std::memcmp(lpActual, lpExpected, liCount * sizeof(f32)) == 0;
}
}

int main()
{
    // Stable feedback with several feedforward patterns, including cancellation,
    // a narrow low-pass pole, and identity/bias-only cases. This tests the kernel
    // independently of the six shapes' coefficient-design functions.
    const Iir2Coeffs laCoeffs[] = {
        {-1.7719234f, .80080265f, .0072198f, .0144396f, .0072198f},
        {-1.9981f, .998102f, .0000005f, .000001f, .0000005f},
        {-.8391f, .4923f, .58131f, -.98712f, .50391f},
        {.573812f, .237197f, -.318367f, .623719f, .713891f},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 0, 0}
    };
    for (const Iir2Coeffs& lrCoeffs : laCoeffs)
    {
        for (int liMode = 0; liMode < 3; ++liMode)
        {
            Iir2State lActual = {.173812f, -.219712f, .318367f, -.123719f};
            Iir2State lExpected = lActual;
            Iir2State lInPlace = lActual;
            for (int liBlock = 0; liBlock < 12; ++liBlock)
            {
                f32 laSrc[256], laExpected[256], laInPlace[256], laGuarded[258];
                for (int li = 0; li < 256; ++li)
                {
                    const int liSample = liBlock * 256 + li;
                    laSrc[li] = liMode == 0 ? ((liSample == 0) ? 1.f : 0.f) :
                        liMode == 1 ? static_cast<f32>((liSample * 73 % 257) - 128) * .0073127f :
                                      static_cast<f32>(std::sin(liSample * .137123) * .713891);
                    laInPlace[li] = laSrc[li];
                    laGuarded[li + 1] = -123.f;
                }
                laGuarded[0] = 31.25f; laGuarded[257] = -62.5f;
                ConsoleFilter(lExpected, laExpected, laSrc, lrCoeffs);
                Iir2::Filter(&lActual, laGuarded + 1, laSrc, &lrCoeffs);
                Iir2::Filter(&lInPlace, laInPlace, laInPlace, &lrCoeffs);
                Check(Same(laGuarded + 1, laExpected, 256), "samples match console instruction trace");
                Check(std::memcmp(&lActual, &lExpected, sizeof(lActual)) == 0, "persistent history matches console");
                Check(Same(laInPlace, laExpected, 256) &&
                      std::memcmp(&lInPlace, &lExpected, sizeof(lInPlace)) == 0, "in-place filtering matches console");
                Check(laGuarded[0] == 31.25f && laGuarded[257] == -62.5f, "exactly 256 output samples written");
                bool lbFinite = true;
                for (f32 lfSample : laExpected) lbFinite = lbFinite && std::isfinite(lfSample);
                Check(lbFinite, "test recurrence stays finite");
            }
        }
    }
    // Exact cancellation at the first fnmsubs yields negative zero. Moving
    // negation inside the FMA instead would yield positive zero here.
    const Iir2Coeffs lCancellation = {1, 0, 0, 0, 0};
    Iir2State lActualCancel = {0, 0, 1.0e-18f, 0};
    Iir2State lExpectedCancel = lActualCancel;
    f32 laSilence[256] = {}, laActualCancel[256], laExpectedCancel[256];
    ConsoleFilter(lExpectedCancel, laExpectedCancel, laSilence, lCancellation);
    Iir2::Filter(&lActualCancel, laActualCancel, laSilence, &lCancellation);
    Check(laActualCancel[0] == 0.f && std::signbit(laActualCancel[0]), "fused cancellation preserves negative zero");
    Check(Same(laActualCancel, laExpectedCancel, 256) &&
          std::memcmp(&lActualCancel, &lExpectedCancel, sizeof(lActualCancel)) == 0, "cancellation samples and history match console");
    Iir2State lReset = {1, 2, 3, 4};
    Iir2::ClearBuffer(&lReset);
    const Iir2State lZero = {};
    Check(std::memcmp(&lReset, &lZero, sizeof(lReset)) == 0, "reset clears all four history values");
    std::printf("AudioIir2: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
