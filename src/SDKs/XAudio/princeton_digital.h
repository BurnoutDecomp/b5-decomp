#pragma once

// princeton_digital -- reverb DSP primitives used by the XAUDIO::CReverbEffect
// middleware. This is an external/middleware library, so the lowercase
// `princeton_digital` namespace and the lower_snake member names (preprocess,
// reset, preprocess3, ...) are kept verbatim to match the external API; project
// PascalCase conventions apply only to owned game code.
//
// Each filter is a power-of-two ring-buffer DSP block. The X360 build emits a
// fully hand-unrolled instance of every template member per <float,N>; here we
// home each generic template ONCE and emit thin explicit instantiations per N
// (see princeton_digital.cpp). N is required to be a power of two so the ring
// index wraps with `& (N - 1)` (matching the `& 0x...FC` byte masks in the
// X360 pseudocode, which are `(N*4 - 4)`).
//
// Layouts are reconstructed from the X360 pseudocode of each instance member
// (member access by name only; offsets are documented in comments next to the
// field, never asserted across pointers). No prior source exists for this TU.

#include "types.hpp"

namespace princeton_digital
{

// Binary search over the table of the first 1229 primes (table[k] == k-th
// prime, table[0] == 1 sentinel; table[1229] == 9973). Returns the prime at the
// converged index -- the nearest tabulated prime to `value`. The table is a
// deterministic mathematical sequence (ascending primes), generated at compile
// time below; it is not a magic data blob.  @ 0x8296C2B0
s32 nearest_prime(u32 value);

// ---------------------------------------------------------------------------
// delay_t<T,N> -- plain N-sample delay line.  reset @ 0x82965xxx per N.
// Layout (dword index): +0 muIndex, +1 mFillValue, +2 mScratch, +3.. buffer[N].
// ---------------------------------------------------------------------------
template <typename T, int N>
struct delay_t
{
    s32 muIndex;        // +0  ring write/read index
    T   mFillValue;     // +4  value the line resets to
    T   mScratch;       // +8  (untouched by reset; tap state)
    T   maBuffer[N];    // +12 ring buffer

    // Fill the whole ring with mFillValue and rewind the index to 0.
    delay_t<T, N> *reset()
    {
        for (int i = 0; i < N; ++i)
            maBuffer[i] = mFillValue;
        muIndex = 0;
        return this;
    }
};

// ---------------------------------------------------------------------------
// allpass_t<T,N> -- Schroeder all-pass (feedforward/feedback gain pair) over an
// N-sample ring buffer, processing a fixed block of 256 samples in place.
// Layout: +0 mLength(int-as-T bits), +1 mFeedbackGain, +2 mFeedforwardGain,
//         +3 mState, +4 muIndex, +5 mPad, +6.. buffer[N].   preprocess @ 0x82960xxx.
// ---------------------------------------------------------------------------
template <typename T, int N>
struct allpass_t
{
    T   mLength;            // +0  delay length, stored as integer in T's bits
    T   mFeedbackGain;      // +4  v7
    T   mFeedforwardGain;   // +8  v6
    T   mState;             // +12 v5: last emitted sample (carried between blocks)
    s32 muIndex;            // +16 v2: current ring offset
    T   mPad;               // +20
    T   maBuffer[N];        // +24 ring buffer (member +6 dwords)

    // Process kBlock samples through the all-pass in place. Store-for-store vs X360
    // @0x829602D0 (verified against asm): the value WRITTEN into the ring is
    // y = ff*delayed + in (asm `*(v11-2) = v6*delayed + in`, v6=ff@result[2]); the
    // carried output is fb*y + delayed (asm `v22 = v7*y + delayed`, v7=fb@result[1]),
    // emitted one sample later (`*a2 = v5`). muIndex is reset to 0 unconditionally at
    // the end (asm `result[4] = 0.0`).
    T *preprocess(T *io)
    {
        const T ff = mFeedforwardGain;
        const T fb = mFeedbackGain;
        const s32 len = static_cast<s32>(reinterpret_cast<u32 &>(mLength));
        const int kBlock = 256;

        // Read offset trails the write offset by the delay length.
        u32 r = static_cast<u32>(muIndex);
        u32 w = static_cast<u32>(muIndex + len);
        T state = mState;
        for (int i = 0; i < kBlock; ++i)
        {
            const T in = io[i];
            const T delayed = maBuffer[r & (N - 1)];
            const T y = static_cast<T>(ff * delayed) + in;       // feedforward: y = ff*delayed + in
            maBuffer[w & (N - 1)] = y;                            // write y into the line
            io[i] = state;                                        // emit previous sample
            state = static_cast<T>(fb * y) + delayed;            // carry the all-pass output
            ++r;
            ++w;
        }
        mState = state;
        muIndex = 0;                                              // asm resets the index unconditionally
        return io;
    }
};

// ---------------------------------------------------------------------------
// threetap_t<T,N> -- three-tap delay (two taps summed into out A, one into B).
// Layout: +0 mTapA(int bits), +1 mGainA, +2 mStateA, +3 mTapBOffset(int),
//         +4 mGainB, +6 mTapCOffset(int), +7 mGainC, +8 mStateB,
//         +9 muIndex(int), +11.. buffer[N].   preprocess3 @ 0x82960xxx.
// ---------------------------------------------------------------------------
template <typename T, int N>
struct threetap_t
{
    T   mTapA;          // +0  primary tap length (int bits)
    T   mGainA;         // +4  v12
    T   mStateA;        // +8  v: last out-A
    T   mTapBOffset;    // +12 v: secondary tap delta (int bits)
    T   mGainB;         // +16 v10
    T   mPad;           // +20
    T   mTapCOffset;    // +24 v7: tertiary tap delta (int bits)
    T   mGainC;         // +28 v13
    T   mStateB;        // +32 v8: last out-B
    s32 muIndex;        // +36 v4: write index
    T   mPad2;          // +40
    T   maBuffer[N];    // +44 ring buffer (member +11 dwords)

    // Block of 256 samples: write input, read three taps; outA = tapB*gA + tapA*gA',
    // outB = tapC*gC. Emits the previous block's tail first (state carry).
    T *preprocess3(T *in, T *outA, T *outB)
    {
        const s32 idx = muIndex;
        const s32 lenA = static_cast<s32>(reinterpret_cast<u32 &>(mTapA));
        const s32 offB = static_cast<s32>(reinterpret_cast<u32 &>(mTapBOffset));
        const s32 offC = static_cast<s32>(reinterpret_cast<u32 &>(mTapCOffset));
        const T gA = mGainA;
        const T gA2 = mGainB;   // v10 second feed of tap A path
        const T gC = mGainC;
        const int kBlock = 256;

        s32 r = idx - lenA;                 // base read offset (v9 = idx - mTapA)
        // Store-for-store vs X360 @0x82960200: secondary/tertiary taps read at
        // idx-mTapBOffset / idx-mTapCOffset. The asm forms v11 = mTapA - mTapBOffset
        // (then reads at v11 + v9 = idx - mTapBOffset), so the delta is (lenA - offB),
        // NOT (offB - lenA).
        const s32 dB = lenA - offB; // v11 = mTapA - mTapBOffset
        const s32 dC = lenA - offC; // v16 = mTapA - mTapCOffset
        T *bufW = &maBuffer[idx];

        outA[0] = mStateA;
        outB[0] = mStateB;
        T lastA = mStateA;
        T lastB = mStateB;
        for (int i = 0; i < kBlock; ++i)
        {
            const T tapA = static_cast<T>(maBuffer[(r) & (N - 1)] * gA);
            const T tapB = maBuffer[(dB + r) & (N - 1)];
            const T tapC = maBuffer[(dC + r) & (N - 1)];
            bufW[i] = in[i];
            lastB = static_cast<T>(tapC * gC);
            lastA = static_cast<T>(tapB * gA2) + tapA;
            outA[i + 1] = lastA;
            outB[i + 1] = lastB;
            ++r;
        }
        mStateA = lastA;
        mStateB = lastB;
        muIndex = static_cast<s32>((idx + kBlock) & (N - 1));
        return in;
    }
};

// ---------------------------------------------------------------------------
// occlusion_t<T,N> -- one-pole low-pass "occlusion" filter pair.
// Layout: +0..+3 reserved, +4 mState0, +5 mState1, +6 mCoefB, +7 mCoefA.
// preprocess @ 0x829600D0.  y = a*y_prev + (b*(1-a))*x ; two interleaved poles.
// ---------------------------------------------------------------------------
template <typename T, int N>
struct occlusion_t
{
    T   mReserved[4];   // +0
    T   mState0;        // +16 v5 (result[4])
    T   mState1;        // +20 v4 (result[5])
    T   mCoefB;         // +24 v11 (result[6]) input gain
    T   mCoefA;         // +28 v8  (result[7]) feedback pole

    // Filter `count` samples from `in` into `out`. First sample carries state.
    T *preprocess(T *in, T *out, u32 count)
    {
        T y1 = mState1;        // v4
        T y0 = mState0;        // v5
        const T a = mCoefA;    // pole
        const T bc = static_cast<T>(mCoefB * (T(1) - mCoefA)); // input coefficient

        out[0] = mState1;
        T *o = out + 1;
        for (u32 i = 0; i < count; ++i)
        {
            const T x = in[i];
            const T prev = y0;
            y0 = static_cast<T>(bc * x) + static_cast<T>(a * y0);
            *o = static_cast<T>(bc * prev) + static_cast<T>(a * y1);
            y1 = *o;
            ++o;
        }
        mState0 = y0;
        mState1 = y1;
        return in;
    }
};

// ---------------------------------------------------------------------------
// vardelay_t<T,N> -- variable (modulated) delay line with a linear crossfade
// between an old and new tap length, ramping the mix at 1/5000 (0.0002) per
// sample.   preprocess @ 0x82960B08 / 0x8295FA78.
// Layout: +0 mScratch, +1 mMix(T), +2 mTapNew(int), +3 mTapNew2(int),
//         +5 mFeedTap(T,int bits stored), +6 mState(T), +8.. buffer[N].
// (See princeton_digital.cpp for the explicit instantiations.)
// ---------------------------------------------------------------------------
template <typename T, int N>
struct vardelay_t
{
    T   mScratch;       // +0
    T   mMix;           // +4  result[1]/+4: crossfade coefficient (ramps by 0.0002)
    s32 mTapNew;        // +8  result+8: new delay tap A
    s32 mTapNew2;       // +12 result+12: new delay tap B
    s32 mTapTarget;     // +16 result+16: LIVE pending/target tap -- the X360 tap-rotation
                        //     reads *(result+16) and copies it into *(result+8) (mTapNew);
                        //     NOT padding. (Was mislabeled mPad; see preprocess note.)
    T   mState;         // +20 result+20: last emitted sample
    s32 mTapOld;        // +24 result+24: previous delay tap
    T   mPad2;          // +28
    T   maBuffer[N];    // +32 ring buffer (member +8 dwords)

    // Process a 256-sample block in place, crossfading old->new tap. Reconstructed
    // semantic body for the per-sample path; the X360 build unrolls it 4-wide.
    T *preprocess(T *in, T *out);
};

// ---------------------------------------------------------------------------
// stereo_room_t<T>::properties_t -- POD parameter block default ctor.
// Field layout from @ 0x829651C8 (dword offsets, mixed int/T).
// ---------------------------------------------------------------------------
template <typename T>
struct stereo_room_t
{
    struct properties_t
    {
        s32 a0;   // +0   = 5
        s32 a1;   // +4   = 5
        s32 a2;   // +8   = 6
        s32 a3;   // +12  = 6
        s32 pad4; // +16
        s32 pad5; // +20
        s32 a6;   // +24  = 8
        s32 a7;   // +28  = 8
        s32 a8;   // +32  = 8
        s32 a9;   // +36  = 4
        s32 a10;  // +40  = 8
        s32 a11;  // +44  = 4
        s32 a12;  // +48  = 5
        T   f13;  // +52  = 5000.0
        T   f14;  // +56  = 0.0
        T   f15;  // +60  = 0.0
        T   f16;  // +64  = 0.0
        T   f17;  // +68  = 0.0
        T   f18;  // +72  = 1.0
        T   f19;  // +76  = 100.0
        T   f20;  // +80  = 100.0

        properties_t()
        {
            f13 = T(5000.0);
            a0  = 5;
            f18 = T(1.0);
            a1  = 5;
            a2  = 6;
            f14 = T(0.0);
            a3  = 6;
            f15 = T(0.0);
            a12 = 5;
            f16 = T(0.0);
            f17 = T(0.0);
            a6  = 8;
            f19 = T(100.0);
            a7  = 8;
            f20 = T(100.0);
            a8  = 8;
            a9  = 4;
            a10 = 8;
            a11 = 4;
        }
    };
};

} // namespace princeton_digital
