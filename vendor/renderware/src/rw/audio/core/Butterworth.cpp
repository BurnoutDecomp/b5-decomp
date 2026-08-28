// =====================================================================================
// rw::audio::core::Butterworth -- cascaded 2nd-order Butterworth IIR section bodies.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. No Feb-2007 leak source and no DecFIGS DWARF exist.
//   Filter        @0x82B649E8 -- store-for-store
//   ClearBuffer   @0x82B64980 -- store-for-store
//   CreateInstance@0x82B6C420 -- store-for-store
//   GetSize       @0x82B6C408 -- store-for-store
//   CalculateFilterCoefficients @0x82B64698 -- the cascade designer, DECODED AND
//                              BODIED 2026-08-28 (phase E); see its header comment.
// See Butterworth.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/Butterworth.h"

#include <cstring> // memset (the X360 XMemSet)
#include <cmath>   // std::tan / std::pow / std::fma (the design maths)

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
// XMemSet @ the X360 build = byte-wise memset; reproduced with std::memset.
inline void XMemSet(void *dst, int value, u32 bytes)
{
    std::memset(dst, value, bytes);
}
} // namespace

// -------------------------------------------------------------------------------------
// GetSize @0x82B6C408
// align8(20*order + 55) + 20*order  -- header (rounded so buffer A starts 8-aligned)
// plus the two 20*order work buffers (the second packed immediately after the first).
// -------------------------------------------------------------------------------------
u32 Butterworth::GetSize(int order)
{
    return ((20u * order + 55u) & 0xFFFFFFF8u) + 20u * order;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82B6C420
// Lay out `self`: record the channel/row count at +0x28, place work-buffer A at the
// first 8-aligned address past the 55-byte header window (offset stored at +0x2C), then
// work-buffer B 8-aligned immediately after A's 20*order bytes (offset at +0x2E), and
// zero both buffers.
// -------------------------------------------------------------------------------------
Butterworth *Butterworth::CreateInstance(int order, Butterworth *self)
{
    char *base = reinterpret_cast<char *>(self);
    const u32 rowsBytes = 20u * order;

    // Buffer A: first 8-aligned address >= self + 55. (The X360 build is 32-bit; the
    // align-to-8 mask is applied on the pointer's integer value -- uintptr_t keeps the
    // semantics on the 64-bit gate host without truncating.)
    char *bufA = reinterpret_cast<char *>(
        (reinterpret_cast<uintptr_t>(base) + 55u) & ~static_cast<uintptr_t>(7u));
    self->mInputHistoryOffset = static_cast<u16>(bufA - base);
    self->mChannels = order & 0xFF;   // asm clrlwi r11,r11,24 masks to the low 8 bits before stw

    // Buffer B: 8-aligned address >= bufA + rowsBytes + 7.
    char *bufB = reinterpret_cast<char *>(
        (reinterpret_cast<uintptr_t>(bufA) + rowsBytes + 7u) & ~static_cast<uintptr_t>(7u));
    self->mOutputHistoryOffset = static_cast<u16>(bufB - base);

    XMemSet(bufA, 0, rowsBytes);
    XMemSet(bufB, 0, rowsBytes);
    return self;
}

// -------------------------------------------------------------------------------------
// ClearBuffer @0x82B64980
// Zero both work buffers (each 20*count bytes), addressed via the recorded relative
// offsets.
// -------------------------------------------------------------------------------------
void Butterworth::ClearBuffer()
{
    char *base = reinterpret_cast<char *>(this);
    char *bufA = base + mInputHistoryOffset;
    char *bufB = base + mOutputHistoryOffset;
    XMemSet(bufA, 0, 20u * mChannels);
    XMemSet(bufB, 0, 20u * mChannels);
}

// -------------------------------------------------------------------------------------
// Filter @0x82B649E8
// Run the 2nd-order section over a 256-sample block for every channel, threading the
// per-channel input/output history through the two work-buffer rows (5 f32 each: row A
// holds x[n-1..n-4]+scratch, row B holds y[n-1..n-4]+scratch), and ping-pong the ctx
// src/dst buffer slots at the end (asm: ctx+0x3000C <-> ctx+0x30010).
//
// Per output sample (the difference equation, fmadds/fsubs, +1e-18 denormal-flush bias
// flt_8214B108, store-for-store):
//   ff = b[1]*x1 + b[0]*x0 + b[2]*x2 + b[3]*x3 + b[4]*x4
//   fb = a[2]*y1 + a[1]*y0h + a[3]*y2 + a[4]*y3
//   y  = ff - fb + 1e-18
// with the input/output delay lines rotated each sample exactly as the asm shifts the
// fp registers.
// -------------------------------------------------------------------------------------
void Butterworth::Filter(AudioProcessContext *ctx)
{
    const f32 KF_DENORMAL_FLUSH = 1.0e-18f; // flt_8214B108

    AudioChannelBuffer **srcSlot = &ctx->mpSrcBuffer; // ctx + 0x3000C
    AudioChannelBuffer **dstSlot = &ctx->mpDstBuffer; // ctx + 0x30010
    AudioChannelBuffer *src = *srcSlot;               // r7 / channel descriptor (input)
    AudioChannelBuffer *dst = *dstSlot;               // r6 / channel descriptor (output)

    char *base = reinterpret_cast<char *>(this);
    f32 *bufA = reinterpret_cast<f32 *>(base + mInputHistoryOffset); // input-history rows
    f32 *bufB = reinterpret_cast<f32 *>(base + mOutputHistoryOffset); // output-history rows

    if (mChannels > 0)
    {
        f32 *rowA = bufA; // per-channel input-history row (5 f32), advances +5 per channel
        f32 *rowB = bufB; // per-channel output-history row (5 f32), advances +5 per channel

        for (s32 ch = 0; ch < static_cast<s32>(mChannels); ++ch)
        {
            // Input delay line x[n-1..n-4] (asm f12,f13,f0,f6 after the first shift);
            // the asm preloads rowA[0..3] which become x[n-1..n-4] once the first sample
            // shifts them down.
            f32 x1 = rowA[0]; // f11 preload -> x[n-1] after first shift (asm f12)
            f32 x2 = rowA[1]; // f12 preload -> x[n-2] (asm f13)
            f32 x3 = rowA[2]; // f13 preload -> x[n-3] (asm f0)
            f32 x4 = rowA[3]; // f0  preload -> x[n-4] (asm f6)

            // Output delay line y[n-1..n-4] (asm f8,f9,f10,f7 from rowB[1..4]).
            f32 y1 = rowB[1]; // f8  -> y[n-1]
            f32 y2 = rowB[2]; // f9  -> y[n-2]
            f32 y3 = rowB[3]; // f10 -> y[n-3]
            f32 y4 = rowB[4]; // f7  -> y[n-4]

            // Per-channel input/output sample pointers: base + 4*stride*ch (f32-typed).
            f32 *in  = src->mpSamples + src->muStride * ch;
            f32 *out = dst->mpSamples + dst->muStride * ch;

            f32 x5 = rowA[3]; // 5th (oldest) input tap x[n-4]; the asm carries it in f6.

            for (int i = 0; i < 256; ++i)
            {
                const f32 x0 = in[i]; // new input sample x[n]

                // Feedback A1*y[n-1] + A2*y[n-2] + A3*y[n-3] + A4*y[n-4]
                // (asm accumulates A2*f9, then A1*f8, A3*f10, A4*f7).
                f32 fb = mCoefficients.a[2] * y2;
                fb = mCoefficients.a[1] * y1 + fb;
                fb = mCoefficients.a[3] * y3 + fb;
                fb = mCoefficients.a[4] * y4 + fb;

                // Feedforward B0*x[n] + B1*x[n-1] + B2*x[n-2] + B3*x[n-3] + B4*x[n-4]
                // (asm: B1*f12, +B0*f11, +B2*f13, +B3*f0, +B4*f6).
                f32 ff = mCoefficients.b[1] * x1;
                ff = mCoefficients.b[0] * x0 + ff;
                ff = mCoefficients.b[2] * x2 + ff;
                ff = mCoefficients.b[3] * x3 + ff;
                ff = mCoefficients.b[4] * x4 + ff;

                const f32 y0 = (ff - fb) + KF_DENORMAL_FLUSH;
                out[i] = y0;

                // Rotate the delay lines (matches the asm fmr chains each iteration:
                // f6=f0; f0=f13; f13=f12; f12=f11; f11=new -- a 5-deep input shift).
                x5 = x4;
                x4 = x3;
                x3 = x2;
                x2 = x1;
                x1 = x0;

                y4 = y3;
                y3 = y2;
                y2 = y1;
                y1 = y0;
            }

            // Store the per-channel history back: rowA[0..4] = {x[n],x[n-1],x[n-2],
            // x[n-3],x[n-4]} (asm f11,f12,f13,f0,f6) and rowB[1..4] = {y[n],y[n-1],
            // y[n-2],y[n-3]} (asm f8,f9,f10,f7).
            rowA[0] = x1; // f11 (== last x[n])
            rowA[1] = x2; // f12
            rowA[2] = x3; // f13
            rowA[3] = x4; // f0
            rowA[4] = x5; // f6 -- oldest tap x[n-4]
            rowB[1] = y1; // f8 (== last y[n])
            rowB[2] = y2; // f9
            rowB[3] = y3; // f10
            rowB[4] = y4; // f7

            rowA += 5;
            rowB += 5;
        }
    }

    // Ping-pong the source/destination buffer slots.
    *dstSlot = src;
    *srcSlot = dst;
}

// =====================================================================================
// CalculateFilterCoefficients @0x82B64698 -- the cascade designer. NO LONGER A KEYSTONE:
// decoded and bodied 2026-08-28 (phase E; report
// progress/scratch_dossiers/butterworth_coefficients_decode_codex.md).
//
// It had TWO blockers and both are now gone:
//   * the "anonymous, fully un-typed helper sub_82C09970" it calls six times is the X360
//     CRT's double-precision pow(x, y) core (x in f1, y in f2, result in f1), identified
//     while unblocking Limiter1::Configure;
//   * the three rodata design tables are not bespoke tuning curves at all -- they are the
//     textbook pieces of a bilinear-transform Butterworth design, named by the same-era
//     ProStreet rwaudiocore map:
//       unk_82F87B88 = sButterworthPolynomials[4][5]  -- the normalized analog
//                      denominators, i.e. 1 / sqrt(2) / 2,2 / the order-4 row
//       unk_82F87BD8 = sCoefficientAMultipliers[4][5][5] -- the EXACT integer matrix
//                      expanding (1+z)^(N-j) (1-z)^j; all 100 cells verified to 0.0 error
//       unk_82F87D68 = sCoefficientsB[4][5]           -- Pascal's triangle, C(N,k)
//     The last two are exact integers and could be recomputed; the first is embedded as
//     recovered VALUES because the image stores slightly rounded source literals
//     (1.414214f, 2.613126f, 3.414214f -- up to 4 ULP off the exact irrationals), and
//     recomputing them with sqrt would silently change the target's single-precision
//     results.
//
// The design: clear the ten coefficient slots, pre-warp the cutoff with tan, build the
// basis {1, w, w^(2^shape), w^(3^shape), w^(4^shape)} with the six pow calls, combine the
// three tables into b[] and a[], normalise both by a[0], and finally scale b[] so the gain
// is unity at DC (low-pass) or Nyquist (high-pass).
//
// FLOATING-POINT FIDELITY NOTE: the console evaluates this in single precision with one
// fused multiply-add per accumulation step (fmadds). The host expresses those as explicit
// std::fma calls; do NOT let a build enable reassociation or implicit contraction around
// the non-FMA operations, or the designed coefficients will drift from the target's.
// The `shape` attribute is exponentiated as pow(2/3/4, shape) and then used as an exponent
// on the warp -- deliberately NOT collapsed to integer powers, because shape is a live
// float attribute and the nesting is what the console computes.
// =====================================================================================
namespace
{
    // ---- the three recovered design tables ------------------------------------------
    // Row index is (order - 1); orders 1..4 (the console's MAX_ORDER).
    const f32 KAF_BUTTERWORTH_POLYNOMIALS[4][5] = {
        { 1.0f, 1.0f,      0.0f,      0.0f,      0.0f },
        { 1.0f, 1.414214f, 1.0f,      0.0f,      0.0f },   // word 3FB504F7
        { 1.0f, 2.0f,      2.0f,      1.0f,      0.0f },
        { 1.0f, 2.613126f, 3.414214f, 2.613126f, 1.0f }    // words 40273D75 / 405A827B
    };

    // M[k][j] = coefficient of z^k in (1+z)^(N-j) (1-z)^j -- exact small integers.
    const f32 KAF_COEFFICIENT_A_MULTIPLIERS[4][5][5] = {
        { { 1,  1, 0, 0, 0 }, { 1, -1, 0, 0, 0 }, { 0, 0, 0, 0, 0 },
          { 0,  0, 0, 0, 0 }, { 0,  0, 0, 0, 0 } },
        { { 1,  1,  1, 0, 0 }, { 2,  0, -2, 0, 0 }, { 1, -1,  1, 0, 0 },
          { 0,  0,  0, 0, 0 }, { 0,  0,  0, 0, 0 } },
        { { 1,  1,  1,  1, 0 }, { 3,  1, -1, -3, 0 }, { 3, -1, -1,  3, 0 },
          { 1, -1,  1, -1, 0 }, { 0,  0,  0,  0, 0 } },
        { { 1,  1,  1,  1,  1 }, { 4,  2,  0, -2, -4 }, { 6,  0, -2,  0,  6 },
          { 4, -2,  0,  2, -4 }, { 1, -1,  1, -1,  1 } }
    };

    // C(N, k) -- Pascal's triangle, the numerator binomials.
    const f32 KAF_COEFFICIENTS_B[4][5] = {
        { 1, 1, 0, 0, 0 },
        { 1, 2, 1, 0, 0 },
        { 1, 3, 3, 1, 0 },
        { 1, 4, 6, 4, 1 }
    };

    const f32 KF_TWO_PI = 6.2831854820251465f;   // the pre-warp's 2*pi (NOT folded with the 0.5)
    const f32 KF_HALF_W = 0.5f;
}

int Butterworth::CalculateFilterCoefficients(Butterworth *self, f32 afCutoff,
                                             f32 afSampleRate, f32 afShape,
                                             s32 aiOrder, s32 aiType)
{
    // XMemSet(self, 0, 0x28) -- exactly the nested coefficient object, nothing else.
    XMemSet(&self->mCoefficients, 0, sizeof(self->mCoefficients));

    // ---- the frequency pre-warp -------------------------------------------------------
    // Low-pass warps to cot(theta), high-pass to tan(theta). ANY other selector leaves
    // both the warp and the basis at the zero just installed -- reproduced, not "fixed".
    f32 lafPowers[5] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    f32 lfWarp = 0.0f;
    if (aiType == KFILTER_LOWPASS || aiType == KFILTER_HIGHPASS)
    {
        const f32 lfAngle = ((afCutoff * KF_TWO_PI) / afSampleRate) * KF_HALF_W;
        const f32 lfTangent = static_cast<f32>(std::tan(static_cast<f64>(lfAngle)));
        lfWarp = (aiType == KFILTER_LOWPASS) ? (1.0f / lfTangent) : lfTangent;
        lafPowers[1] = lfWarp;
    }

    // ---- the {1, w, w^(2^shape), w^(3^shape), w^(4^shape)} basis: the six pow calls ----
    for (int liTerm = 2; liTerm <= 4; ++liTerm)
    {
        const f32 lfExponent = static_cast<f32>(
            std::pow(static_cast<f64>(liTerm), static_cast<f64>(afShape)));
        lafPowers[liTerm] = static_cast<f32>(
            std::pow(static_cast<f64>(lfWarp), static_cast<f64>(lfExponent)));
    }

    // The console indexes the tables with (order - 1) and never validates it; the callers
    // seed order 4 and the binary's contract is 1 <= order <= 4.
    const int liRow = aiOrder - 1;

    // ---- combine the three tables into the raw polynomials ----------------------------
    // The alternating sign is the high-pass spectral flip (z -> -z); low-pass keeps +1.
    for (int liK = 0; liK <= aiOrder; ++liK)
    {
        const f32 lfSign = (aiType != 0 && (liK & 1)) ? -1.0f : 1.0f;
        self->mCoefficients.a[liK] = 0.0f;
        self->mCoefficients.b[liK] = KAF_COEFFICIENTS_B[liRow][liK] * lfSign;

        for (int liJ = 0; liJ <= aiOrder; ++liJ)
        {
            const f32 lfTerm = (KAF_COEFFICIENT_A_MULTIPLIERS[liRow][liK][liJ]
                                * KAF_BUTTERWORTH_POLYNOMIALS[liRow][liJ])
                             * lafPowers[liJ];
            self->mCoefficients.a[liK] =
                std::fma(lfTerm, lfSign, self->mCoefficients.a[liK]);   // fmadds
        }
    }

    // ---- normalise both polynomials by a[0] (the console walks DOWNWARD) ---------------
    const f32 lfInverseA0 = 1.0f / self->mCoefficients.a[0];
    for (int liK = aiOrder; liK >= 0; --liK)
    {
        const f32 lfB = self->mCoefficients.b[liK];
        const f32 lfA = self->mCoefficients.a[liK];
        self->mCoefficients.b[liK] = lfB * lfInverseA0;
        self->mCoefficients.a[liK] = lfA * lfInverseA0;
    }

    // ---- unity-gain scaling: at z=1 for low-pass, z=-1 for high-pass -------------------
    f32 lfSumA = 0.0f;
    f32 lfSumB = 0.0f;
    for (int liK = 0; liK <= aiOrder; ++liK)
    {
        const f32 lfSign = (aiType != 0 && (liK & 1)) ? -1.0f : 1.0f;
        lfSumA = std::fma(self->mCoefficients.a[liK], lfSign, lfSumA);
        lfSumB = std::fma(self->mCoefficients.b[liK], lfSign, lfSumB);
    }
    const f32 lfGain = lfSumA / lfSumB;
    for (int liK = 0; liK <= aiOrder; ++liK)
        self->mCoefficients.b[liK] = self->mCoefficients.b[liK] * lfGain;

    // The machine leaves order+1 in r3; both callers ignore it (the same-middleware
    // ProStreet shape is void).
    return aiOrder + 1;
}

} // namespace core
} // namespace audio
} // namespace rw
