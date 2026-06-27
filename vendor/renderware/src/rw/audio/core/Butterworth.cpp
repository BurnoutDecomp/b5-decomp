// =====================================================================================
// rw::audio::core::Butterworth -- cascaded 2nd-order Butterworth IIR section bodies.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. No Feb-2007 leak source and no DecFIGS DWARF exist.
//   Filter        @0x82B649E8 -- store-for-store
//   ClearBuffer   @0x82B64980 -- store-for-store
//   CreateInstance@0x82B6C420 -- store-for-store
//   GetSize       @0x82B6C408 -- store-for-store
//   CalculateFilterCoefficients @0x82B64698 -- KEYSTONE, not reconstructed (see below).
// See Butterworth.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/Butterworth.h"

#include <cstring> // memset (the X360 XMemSet)

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

// -------------------------------------------------------------------------------------
// CalculateFilterCoefficients @0x82B64698 -- KEYSTONE (NOT reconstructed).
//
// The X360 body is hand-written PowerPC that (1) computes a frequency pre-warp via tan,
// (2) calls an undecoded helper sub_82C09970 six times to evaluate a transfer-function
// polynomial, and (3) reads three undecoded rodata coefficient tables (unk_82F87B88,
// unk_82F87BD8, unk_82F87D68; strides 0x14 / 0x64) indexed by the filter order to build
// and normalise the cascade coefficients written into the +0x00..+0x24 header.
//
// Neither sub_82C09970 (an anonymous, fully un-typed leaf whose register-level argument
// shape IDA cannot recover) nor the three rodata tables are decoded, so a faithful,
// store-for-store reconstruction is not possible from the available exports. Per the
// project rule, this is flagged as a keystone rather than fabricated; the surrounding
// Butterworth bodies (Filter/ClearBuffer/CreateInstance/GetSize) are fully reconstructed
// and the coefficient header they consume is left for the keystone to fill.
// -------------------------------------------------------------------------------------
int Butterworth::CalculateFilterCoefficients(Butterworth * /*self*/)
{
    // KEYSTONE: requires decoding sub_82C09970 (@0x82C09970) and the three rodata
    // Butterworth design tables (unk_82F87B88 / unk_82F87BD8 / unk_82F87D68). Not
    // reconstructed; see the comment above. No fabricated coefficients are written.
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
