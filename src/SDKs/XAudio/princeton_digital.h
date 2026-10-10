#pragma once

// princeton_digital -- reverb DSP primitives used by the XAUDIO::CReverbEffect
// middleware. This is an external/middleware library, so the lowercase
// `princeton_digital` namespace and the lower_snake member names (preprocess,
// reset, preprocess3, ...) are kept verbatim to match the external API; project
// PascalCase conventions apply only to owned game code.
//
// Every delay-based filter embeds a power-of-two ring buffer (delay_t<T,N>); the
// console resets those sub-objects through delay_t<T,N>::reset and masks ring
// indices with `& (N - 1)`. The out-of-line block filters (preprocess /
// preprocess3) run 256-sample blocks; the reverb tank in stereo_room_t::process
// runs the same filters one sample at a time through the inline tick() members.
// The small fir2_t / iir2_t / lowpass2_t / allpass2_t structs exist only inline
// in the console image (no out-of-line member survives), so their names are
// descriptive; their layouts and arithmetic come from the console code.
//
// Layouts are reconstructed from the console code (member access by name only;
// offsets are documented in comments next to the field, never asserted across
// pointers). No prior source exists for this TU.

#include "types.hpp"

namespace princeton_digital
{

// Binary search over the table of the first 1229 primes (table[k] == k-th
// prime, table[0] == 1 sentinel; table[1229] == 9973). Returns the prime at the
// converged index -- the nearest tabulated prime to `value`.
s32 nearest_prime(u32 value);

// ---------------------------------------------------------------------------
// delay_t<T,N> -- N-sample ring buffer embedded in every delay-based filter.
// Layout: +0 muIndex, +4 mFillValue, +8 buffer[N].
// ---------------------------------------------------------------------------
template <typename T, int N>
struct delay_t
{
    s32 muIndex;        // +0  ring write index
    T   mFillValue;     // +4  value the line resets to
    T   maBuffer[N];    // +8  ring buffer

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
// N-sample ring buffer.
// Layout: +0 miLength, +4 mFeedbackGain, +8 mFeedforwardGain, +12 mState,
//         +16 mLine (delay_t<T,N>).
// ---------------------------------------------------------------------------
template <typename T, int N>
struct allpass_t
{
    s32 miLength;           // +0  delay length in samples
    T   mFeedbackGain;      // +4
    T   mFeedforwardGain;   // +8
    T   mState;             // +12 last emitted sample
    delay_t<T, N> mLine;    // +16

    // Process 256 samples in place. The value WRITTEN into the ring is
    // y = ff*delayed + in at the write position; the read position trails it by
    // the delay length. The carried output fb*y + delayed is emitted one sample
    // later: io[0] gets the previous block's tail, and the final output is kept
    // in mState and also written to io[256]. The write index advances by one
    // block (for N == 128 the two half-block passes both start at the ring
    // origin, which is where the index always sits).
    T *preprocess(T *io)
    {
        const T ff = mFeedforwardGain;
        const T fb = mFeedbackGain;
        const s32 w = mLine.muIndex;
        const s32 r = w - miLength;
        const int kBlock = 256;

        T state = mState;
        for (int i = 0; i < kBlock; ++i)
        {
            const T delayed = mLine.maBuffer[(r + i) & (N - 1)];
            const T y = ff * delayed + io[i];
            mLine.maBuffer[(w + i) & (N - 1)] = y;
            io[i] = state;
            state = fb * y + delayed;
        }
        mState = state;
        io[kBlock] = state;
        mLine.muIndex = (w + kBlock) & (N - 1);
        return io;
    }

    // Process 256 samples from `in` into `out`: out[0..255] are the outputs
    // delayed by one sample (out[0] is the previous block's tail), the final
    // output is carried in mState and also written to in[256].
    void preprocess(T *in, T *out);

    // One sample: write y = ff*delayed + x, advance the ring, keep
    // fb*y + delayed as the output.
    T tick(T x)
    {
        const T delayed = mLine.maBuffer[(mLine.muIndex - miLength) & (N - 1)];
        const T y = mFeedforwardGain * delayed + x;
        mLine.maBuffer[mLine.muIndex] = y;
        mLine.muIndex = (mLine.muIndex + 1) & (N - 1);
        mState = mFeedbackGain * y + delayed;
        return mState;
    }
};

// ---------------------------------------------------------------------------
// allpass2_t<T,N> -- all-pass variant with separate input/output gains, used in
// the reverb tank. Layout: +0 miLength, +4 mInputGain, +8 mFeedbackGain,
// +12 mDelayGain, +16 mOut, +20 mLine (delay_t<T,N>).
// ---------------------------------------------------------------------------
template <typename T, int N>
struct allpass2_t
{
    s32 miLength;       // +0
    T   mInputGain;     // +4
    T   mFeedbackGain;  // +8
    T   mDelayGain;     // +12
    T   mOut;           // +16
    delay_t<T, N> mLine; // +20

    // One sample: the ring gets fb*delayed + x, the output is
    // inGain*x + delayGain*delayed.
    T tick(T x)
    {
        const T delayed = mLine.maBuffer[(mLine.muIndex - miLength) & (N - 1)];
        mLine.maBuffer[mLine.muIndex] = delayed * mFeedbackGain + x;
        mOut = mInputGain * x + delayed * mDelayGain;
        mLine.muIndex = (mLine.muIndex + 1) & (N - 1);
        return mOut;
    }
};

// ---------------------------------------------------------------------------
// threetap_t<T,N> -- three-tap delay (two taps summed into out A, one into B).
// Layout: +0 miTapA, +4 mGainA, +8 mStateA, +12 miTapB, +16 mGainB,
//         +20 mReserved, +24 miTapC, +28 mGainC, +32 mStateB, +36 mLine.
// ---------------------------------------------------------------------------
template <typename T, int N>
struct threetap_t
{
    s32 miTapA;         // +0  primary tap length
    T   mGainA;         // +4
    T   mStateA;        // +8  last out-A
    s32 miTapB;         // +12 secondary tap length
    T   mGainB;         // +16
    T   mReserved;      // +20
    s32 miTapC;         // +24 tertiary tap length
    T   mGainC;         // +28
    T   mStateB;        // +32 last out-B
    delay_t<T, N> mLine; // +36

    // Block of 256 samples: write input, read three taps; outA = tapA*gA +
    // tapB*gB, outB = tapC*gC. Emits the previous block's tail first.
    T *preprocess3(T *in, T *outA, T *outB)
    {
        const s32 idx = mLine.muIndex;
        const s32 lenA = miTapA;
        const s32 offB = miTapB;
        const s32 offC = miTapC;
        const T gA = mGainA;
        const T gA2 = mGainB;
        const T gC = mGainC;
        const int kBlock = 256;

        s32 r = idx - lenA;
        // The secondary/tertiary taps read at idx-miTapB / idx-miTapC, formed
        // as deltas from the primary read position.
        const s32 dB = lenA - offB;
        const s32 dC = lenA - offC;
        T *bufW = &mLine.maBuffer[idx];

        outA[0] = mStateA;
        outB[0] = mStateB;
        T lastA = mStateA;
        T lastB = mStateB;
        for (int i = 0; i < kBlock; ++i)
        {
            const T tapA = static_cast<T>(mLine.maBuffer[(r) & (N - 1)] * gA);
            const T tapB = mLine.maBuffer[(dB + r) & (N - 1)];
            const T tapC = mLine.maBuffer[(dC + r) & (N - 1)];
            bufW[i] = in[i];
            lastB = static_cast<T>(tapC * gC);
            lastA = static_cast<T>(tapB * gA2) + tapA;
            outA[i + 1] = lastA;
            outB[i + 1] = lastB;
            ++r;
        }
        mStateA = lastA;
        mStateB = lastB;
        mLine.muIndex = static_cast<s32>((idx + kBlock) & (N - 1));
        return in;
    }
};

// ---------------------------------------------------------------------------
// twotap_t<T,N> -- two independently tapped and scaled reads of one delay line.
// Layout: +0 miTapA, +4 mGainA, +8 mStateA, +12 miTapB, +16 mGainB,
//         +20 mStateB, +24 mLine.
// ---------------------------------------------------------------------------
template <typename T, int N>
struct twotap_t
{
    s32 miTapA;         // +0
    T   mGainA;         // +4
    T   mStateA;        // +8  last tap-A output
    s32 miTapB;         // +12
    T   mGainB;         // +16
    T   mStateB;        // +20 last tap-B output
    delay_t<T, N> mLine; // +24

    // Block of 256 samples: outA/outB get the previous tails first, then the
    // two scaled taps of every sample; the input is written at the (unmasked)
    // write position.
    twotap_t<T, N> *preprocess(T *in, T *outA, T *outB);

    // One sample: refresh both taps, then write x and advance the ring.
    void tick(T x)
    {
        mStateB = mLine.maBuffer[(mLine.muIndex - miTapB) & (N - 1)] * mGainB;
        mStateA = mLine.maBuffer[(mLine.muIndex - miTapA) & (N - 1)] * mGainA;
        mLine.maBuffer[mLine.muIndex] = x;
        mLine.muIndex = (mLine.muIndex + 1) & (N - 1);
    }
};

// ---------------------------------------------------------------------------
// fir2_t<T> -- two-sample history with one scaled tap; miTap selects which of
// the two history samples is read. Layout: +0 miTap, +4 mGain, +8 mOut,
// +12 mReserved, +16 maHist[2].
// ---------------------------------------------------------------------------
template <typename T>
struct fir2_t
{
    s32 miTap;          // +0
    T   mGain;          // +4
    T   mOut;           // +8
    T   mReserved;      // +12
    T   maHist[2];      // +16 previous inputs, newest first

    // out = x + gain * tap.
    T comb(T x)
    {
        const T tap = maHist[miTap & 1];
        maHist[1] = maHist[0];
        mOut = tap * mGain + x;
        maHist[0] = x;
        return mOut;
    }

    // out = gain * tap (the input only enters the history).
    T delay(T x)
    {
        mOut = maHist[miTap & 1] * mGain;
        maHist[1] = maHist[0];
        maHist[0] = x;
        return mOut;
    }
};

// ---------------------------------------------------------------------------
// iir2_t<T> -- one-pole/one-zero section over a two-sample state history.
// Layout: +0 miTap, +4 mB0, +8 mA1, +12 mB1, +16 mOut, +20 mReserved,
// +24 maW[2].
// ---------------------------------------------------------------------------
template <typename T>
struct iir2_t
{
    s32 miTap;          // +0
    T   mB0;            // +4
    T   mA1;            // +8
    T   mB1;            // +12
    T   mOut;           // +16
    T   mReserved;      // +20
    T   maW[2];         // +24 state history, newest first

    // w = x + a1*w[tap]; out = b0*w + b1*w[tap].
    T tick(T x)
    {
        const T wTap = maW[miTap & 1];
        maW[1] = maW[0];
        const T w = mA1 * wTap + x;
        maW[0] = w;
        mOut = w * mB0 + wTap * mB1;
        return mOut;
    }
};

// ---------------------------------------------------------------------------
// lowpass2_t<T> -- gain/coefficient pair over a two-sample history, used either
// as a recursive section (iir) or as a two-tap FIR (fir).
// Layout: +0 miTap, +4 mOut, +8 mGain, +12 mCoef, +16 mReserved, +20 maHist[2].
// ---------------------------------------------------------------------------
template <typename T>
struct lowpass2_t
{
    s32 miTap;          // +0
    T   mOut;           // +4
    T   mGain;          // +8
    T   mCoef;          // +12
    T   mReserved;      // +16
    T   maHist[2];      // +20 history, newest first

    // w = x + coef*hist[tap]; out = gain*w.
    T iir(T x)
    {
        const T newest = maHist[0];
        const T w = maHist[miTap & 1] * mCoef + x;
        maHist[0] = w;
        maHist[1] = newest;
        mOut = w * mGain;
        return mOut;
    }

    // out = gain*x + coef*hist[tap].
    T fir(T x)
    {
        const T tap = maHist[miTap & 1];
        maHist[1] = maHist[0];
        maHist[0] = x;
        mOut = tap * mCoef + mGain * x;
        return mOut;
    }
};

// ---------------------------------------------------------------------------
// occlusion_t<T,N> -- one-pole low-pass "occlusion" filter pair.
// Layout: +0 mSampleRate, +4 mReferenceHz, +8 mLevelA, +12 mLevelB,
//         +16 mState0, +20 mState1, +24 mCoefB, +28 mCoefA.
// y = a*y_prev + (b*(1-a))*x ; two interleaved poles.
// ---------------------------------------------------------------------------
template <typename T, int N>
struct occlusion_t
{
    T   mSampleRate;    // +0
    T   mReferenceHz;   // +4  corner frequency
    T   mLevelA;        // +8  level, dB
    T   mLevelB;        // +12 level, dB
    T   mState0;        // +16
    T   mState1;        // +20
    T   mCoefB;         // +24 input gain
    T   mCoefA;         // +28 feedback pole

    // Derive mCoefB / mCoefA from the levels, the corner and the sample rate.
    void recalculate();

    // Filter `count` samples from `in` into `out`. First sample carries state.
    T *preprocess(T *in, T *out, u32 count)
    {
        T y1 = mState1;
        T y0 = mState0;
        const T a = mCoefA;
        const T bc = static_cast<T>(mCoefB * (T(1) - mCoefA));

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
// sample. Layout: +0 mSampleRate, +4 mMix, +8 mTapNew, +12 mTapNew2,
// +16 mTapTarget, +20 mState, +24 mLine.
// ---------------------------------------------------------------------------
template <typename T, int N>
struct vardelay_t
{
    T   mSampleRate;    // +0
    T   mMix;           // +4  crossfade coefficient (ramps by 0.0002)
    s32 mTapNew;        // +8  new delay tap A
    s32 mTapNew2;       // +12 new delay tap B
    s32 mTapTarget;     // +16 pending tap, rotated into mTapNew
    T   mState;         // +20 last emitted sample
    delay_t<T, N> mLine; // +24

    // Process a 256-sample block, crossfading old->new tap.
    T *preprocess(T *in, T *out);
};

// ---------------------------------------------------------------------------
// stereo_room_t<T> -- the stereo reverb room: per channel a modulated pre-delay,
// an occlusion low-pass and an early-reflection network; then a shared
// cross-coupled reverb tank processed sample by sample; then output diffusion
// and rear delays, mixed wet/dry into four outputs.
// ---------------------------------------------------------------------------
template <typename T>
struct stereo_room_t
{
    // Parameter block (84 bytes). The ints index the coefficient tables in
    // properties_set; the floats are levels, times and frequencies.
    struct properties_t
    {
        s32 a0;   // +0   early delay, ms
        s32 a1;   // +4   tank tap delay, ms
        s32 a2;   // +8
        s32 a3;   // +12
        s32 a4;   // +16
        s32 a5;   // +20
        s32 a6;   // +24
        s32 a7;   // +28
        s32 a8;   // +32
        s32 a9;   // +36
        s32 a10;  // +40
        s32 a11;  // +44
        s32 a12;  // +48  rear delay, ms
        T   f13;  // +52  occlusion corner, Hz
        T   f14;  // +56  occlusion level A, dB
        T   f15;  // +60  occlusion level B, dB
        T   f16;  // +64  output level, dB
        T   f17;  // +68  tank tap level, dB
        T   f18;  // +72  decay time, s
        T   f19;  // +76  room size, percent
        T   f20;  // +80  diffusion, percent

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

    // Early-reflection network of one channel.
    struct early_t
    {
        T   mTaps1Gain;                 // +0  mTaps1 out-A into the OTHER channel's mix
        T   mTaps2Gain;                 // +4  mTaps2 out-A into this channel's mix
        threetap_t<T, 512>  mTaps1;     // +8
        allpass_t<T, 128>   mDiffuse1;
        threetap_t<T, 2048> mTaps2;
        allpass_t<T, 256>   mDiffuse2;
        allpass_t<T, 512>   mDiffuse3;
        twotap_t<T, 1024>   mOutput;
    };

    // First half of a tank branch.
    struct tank1_t
    {
        allpass_t<T, 512>   mAllpass1;
        allpass_t<T, 512>   mAllpass2;
        fir2_t<T>           mComb;
        iir2_t<T>           mFilter;
        lowpass2_t<T>       mDamp;
        twotap_t<T, 4096>   mDelay;
        fir2_t<T>           mDelayTap;
    };

    // Second half of a tank branch.
    struct tank2_t
    {
        allpass_t<T, 2048>  mAllpass1;
        allpass2_t<T, 2048> mAllpass2;
        allpass_t<T, 2048>  mAllpass3;
        allpass_t<T, 1024>  mAllpass4;
        twotap_t<T, 4096>   mDelay;
        lowpass2_t<T>       mTone;
    };

    // vtable ptr @ +0 -- implicit.
    properties_t mProperties;   // +4   84-byte parameter block
    T   mSampleRate;            // +88  sample rate (48000)
    T   mWetDryMix;             // +92  wet/dry blend, 0..1
    T   mOutputGain;            // +96  output level
    s32 mInputMode;             // +100 0=stereo, 1=left only, 2=right only
    vardelay_t<T, 16384> maPreDelay[2];   // +0x68
    occlusion_t<T, 2>    maOcclusion[2];  // +0x200A8
    early_t              maEarly[2];      // +0x200E8
    tank1_t              maTank1[2];      // +0x28E78
    tank2_t              maTank2[2];      // +0x32FF0
    allpass_t<T, 512>    maOutDiffuse[4]; // +0x49130
    vardelay_t<T, 256>   maRearDelay[2];  // +0x4B190

    // Build the room with the default properties at 48 kHz and reset every
    // delay line.
    stereo_room_t();

    // The scalar deleting destructor re-stores the vtable at +0 then, when the
    // delete flag is set, hands the block to operator delete
    // (XAUDIO::CXMemMemoryManager::XMemFree). The room owns no heap members and
    // the sub-filters are plain ring buffers, so the destructor body is empty.
    virtual ~stereo_room_t() {}

    // Select the input channel mixing mode. Returns `this`.
    stereo_room_t *input_mode_set(s32 mode)
    {
        mInputMode = mode;
        return this;
    }

    // Set the wet/dry blend from a 0..100 percentage (scaled by 0.0099999998 ==
    // 1/100 as dumped). Returns `this`.
    stereo_room_t *wet_dry_mix_set(T mix)
    {
        mWetDryMix = static_cast<T>(mix * static_cast<T>(0.0099999998));
        return this;
    }

    // Push a properties_t block into the DSP sub-filter bank: tap lengths from
    // the times and room size (snapped to primes), gains from the coefficient
    // tables and the decay time. Returns `this`.
    stereo_room_t *properties_set(const properties_t &props);

    // Process one 256-sample block. in[0]/in[1] are the left/right inputs (or
    // one of them for both channels, per mInputMode); out[0]/out[1] receive the
    // front wet/dry mix and out[3]/out[4] the rear delays.
    void process(T *const *in, T *const *out);
};

// ---------------------------------------------------------------------------
// i3dl2_reverb_t<T> -- the I3DL2 / EAX reverb parameter block (12 fields,
// 48 bytes), in the standard I3DL2 field order. This is the value `set` copies
// (whole-block or one field at a time) and that i3dl2_to_properties converts
// into a stereo_room_t::properties_t. The default values below are baked into
// the stereo_room_3dl2_t ctor: they are the I3DL2 "generic" preset
// (Room/RoomHF/Reflections/Reverb in millibels, times in seconds,
// diffusion/density in percent, HFReference in Hz).
// ---------------------------------------------------------------------------
template <typename T>
struct i3dl2_reverb_t
{
    s32 miRoom;               // +0   attenuation at/above 0 Hz, mB   (default -10000)
    s32 miRoomHF;             // +4   extra HF attenuation, mB        (default 0)
    T   mfRoomRolloffFactor;  // +8   distance rolloff               (default 0.0)
    T   mfDecayTime;          // +12  reverb decay time, s           (default 1.0)
    T   mfDecayHFRatio;       // +16  HF-to-mid decay ratio          (default 0.5)
    s32 miReflections;        // +20  early-reflection level, mB     (default -10000)
    T   mfReflectionsDelay;   // +24  early-reflection delay, s      (default 0.02)
    s32 miReverb;             // +28  late-reverb level, mB          (default -10000)
    T   mfReverbDelay;        // +32  late-reverb delay, s           (default 0.04)
    T   mfDiffusion;          // +36  echo density, %                (default 100.0)
    T   mfDensity;            // +40  modal density, %               (default 100.0)
    T   mfHFReference;        // +44  HF reference, Hz               (default 5000.0)
};

// ---------------------------------------------------------------------------
// stereo_room_3dl2_t<T> -- an I3DL2-parameterised front end over stereo_room_t.
// It owns an i3dl2_reverb_t parameter block and, on every parameter change,
// converts it (via i3dl2_to_properties) into a stereo_room_t::properties_t and
// pushes that into the underlying DSP room (stereo_room_t::properties_set).
//
// LAYOUT (from the ctor/set/destructor asm): the object begins with an MSVC
// vbptr at +0, the 48-byte i3dl2_reverb_t block at +4..+51, and the
// stereo_room_t<T> subobject at +52. A base placed AFTER the derived's own
// members, reached through a vbtable offset, and constructed only under the
// most-derived flag, is exactly MSVC virtual inheritance -- so stereo_room_t<T>
// is modelled as a virtual base. The derived overrides the (virtual) destructor,
// which is why the base subobject's vftable is rewritten to this class's.
// ---------------------------------------------------------------------------
template <typename T>
struct stereo_room_3dl2_t : virtual stereo_room_t<T>
{
    i3dl2_reverb_t<T> mProps;   // +4  the live I3DL2 parameter block

    // Construct: seed mProps with the I3DL2 "generic" preset. The virtual base
    // stereo_room_t<T> is constructed by the compiler-inserted most-derived path.
    stereo_room_3dl2_t();

    // Update the I3DL2 parameters, then re-derive and apply the DSP properties.
    // `index` selects one field (1..12) or the whole block (0); `src` is a bare
    // I3DL2 parameter block. Returns the underlying room.
    stereo_room_t<T> *set(int index, const i3dl2_reverb_t<T> *src);

    // This class owns no heap members (the I3DL2 block is POD and the DSP
    // filters live inside the stereo_room_t base), so the destructor body is
    // empty; the compiler emits the vtable-restoring + XMemFree-routing thunk.
    virtual ~stereo_room_3dl2_t() {}
};

} // namespace princeton_digital
