// princeton_digital reverb DSP primitives -- generic definitions plus the
// explicit per-N instantiations the console build emits. No prior source exists
// for this TU; bodies are reconstructed from the console code of each instance
// member. The properties_set coefficient tables are dumped from the image.

#include "SDKs/XAudio/princeton_digital.h"

#include <math.h>     // pow, sqrt, cos, log10
#include <string.h>   // memcpy

namespace princeton_digital
{

// ---------------------------------------------------------------------------
// Prime table for nearest_prime. table[k] is the k-th prime (1-based), with
// table[0] == 1 as the sentinel used by the a1 <= 1 early-out. table[1229] is
// 9973 (the 1229th prime), which is the upper clamp value. The console holds
// this as a fixed rodata array; since it is the deterministic ascending-prime
// sequence the algorithm itself defines, it is generated at compile time here.
// ---------------------------------------------------------------------------
namespace
{
struct PrimeTable
{
    static const int KI_COUNT = 1230; // indices 0..1229
    s32 mTable[KI_COUNT];

    PrimeTable()
    {
        mTable[0] = 1; // sentinel
        int filled = 1;
        for (s32 c = 2; filled < KI_COUNT; ++c)
        {
            bool isPrime = true;
            for (s32 p = 2; p * p <= c; ++p)
            {
                if (c % p == 0)
                {
                    isPrime = false;
                    break;
                }
            }
            if (isPrime)
                mTable[filled++] = c;
        }
    }
};

const PrimeTable &prime_table()
{
    static const PrimeTable kTable;
    return kTable;
}
} // namespace

// Binary search for the tabulated prime nearest `value`.
s32 nearest_prime(u32 value)
{
    if (value <= 1)
        return 1;
    if (value >= 0x26F5u) // 9973
        return 9973;

    const s32 *table = prime_table().mTable;
    u32 lo = 0;
    s32 hi = 1229;
    u32 mid = 1;
    u32 prev;
    do
    {
        prev = mid;
        mid = static_cast<u32>(hi + static_cast<s32>(lo)) >> 1;
        const u32 here = static_cast<u32>(table[mid]);
        if (here == value)
            break;
        if (here <= value)
            lo = mid + 1;
        else
            hi = static_cast<s32>(mid) - 1;
    } while (mid != prev);

    return table[mid];
}

// ---------------------------------------------------------------------------
// vardelay_t<T,N>::preprocess -- modulated delay line that linearly crossfades
// between two tap lengths, ramping the mix by 0.0002/sample.
//
// The block is 256 samples. Each output sample blends two ring taps:
//   out = maBuffer[(w - mTapNew2)&(N-1)] * mixDown
//       + maBuffer[(w - mTapNew )&(N-1)] * mixUp
// where, per sample, mixDown ramps -0.0002 (from mMix) and mixUp ramps +0.0002
// (from 1 - mMix); w is the ring write index, advancing as the incoming sample
// is stored at maBuffer[w] (raw, unmasked pointer). The final write index
// advances by one full block (masked to the ring) and mState carries out[256]
// to the next block.
//
// The selector v5 = trunc(mMix*10000) >> 1 (~mMix*5000) is the number of samples
// left in the CURRENT crossfade:
//   * v5 >= 256 (FAST path): the whole block stays inside one crossfade -- no tap
//     rotation; mMix is left mid-ramp.
//   * v5 <  256 (SLOW path): the current crossfade completes at sample v5, then a
//     tap rotation runs (mTapNew2 <- mTapNew, mTapNew <- mTapTarget; mMix reset to
//     0.0 if the target equals the old new-tap, else 1.0) and a second crossfade
//     runs over the remaining 256-v5 samples with the rotated taps.
// The console unrolls each phase 4-wide; the unrolling is a pure perf transform,
// so the per-sample kernel below is the store-for-store equivalent.
// ---------------------------------------------------------------------------
namespace
{
// One crossfade phase: blend the two taps for `count` samples, writing the
// incoming samples into the ring at the (raw, unmasked) write pointer. Advances
// `writeIndex`, `in`, `out`, and the running mix coefficients in place so the
// caller can chain phases.
template <typename T, int N>
void vardelay_crossfade(T *buffer, s32 &writeIndex, s32 tapNew, s32 tapNew2,
                        const T *&in, T *&out, T &mixDown, T &mixUp, int count)
{
    const T kRamp = static_cast<T>(0.00019999999);

    const s32 readNew = writeIndex - tapNew;
    const s32 readNew2 = writeIndex - tapNew2;
    T *bufW = &buffer[writeIndex];

    for (int i = 0; i < count; ++i)
    {
        const T sampleA = static_cast<T>(buffer[(readNew + i) & (N - 1)] * mixUp);
        const T sampleB = buffer[(readNew2 + i) & (N - 1)];
        *bufW++ = *in++;
        *out++ = static_cast<T>(sampleB * mixDown) + sampleA;
        mixUp = static_cast<T>(mixUp + kRamp);
        mixDown = static_cast<T>(mixDown - kRamp);
    }
    writeIndex += count;
}
} // namespace

template <typename T, int N>
T *vardelay_t<T, N>::preprocess(T *in, T *out)
{
    const int kBlock = 256;

    out[0] = mState;
    const T *src = in;
    T *dst = out + 1;

    s32 writeIndex = mLine.muIndex;
    const u32 remain = static_cast<u32>(static_cast<long long>(mMix * static_cast<T>(10000.0))) >> 1;

    if (remain >= static_cast<u32>(kBlock))
    {
        T mixDown = mMix;
        T mixUp = static_cast<T>(T(1) - mMix);
        vardelay_crossfade<T, N>(mLine.maBuffer, writeIndex, mTapNew, mTapNew2,
                                 src, dst, mixDown, mixUp, kBlock);
        mMix = mixDown;
    }
    else
    {
        const int n1 = static_cast<int>(remain);
        {
            T mixDown = mMix;
            T mixUp = static_cast<T>(T(1) - mMix);
            vardelay_crossfade<T, N>(mLine.maBuffer, writeIndex, mTapNew, mTapNew2,
                                     src, dst, mixDown, mixUp, n1);
            mMix = mixDown;
        }

        const s32 target = mTapTarget;
        const s32 prevNew = mTapNew;
        mMix = (target == prevNew) ? T(0) : T(1);
        mTapNew2 = prevNew;
        mTapNew = target;

        const int n2 = kBlock - n1;
        {
            T mixDown = mMix;
            T mixUp = static_cast<T>(T(1) - mMix);
            vardelay_crossfade<T, N>(mLine.maBuffer, writeIndex, mTapNew, mTapNew2,
                                     src, dst, mixDown, mixUp, n2);
            mMix = mixDown;
        }
    }

    mState = out[kBlock];
    // Both paths advance writeIndex by exactly one full block; the stored index
    // is masked to the ring (clrlwi by the ring size: 0xFF for 256, 0x3FFF for
    // 16384).
    mLine.muIndex = writeIndex & (N - 1);
    return in;
}

// ---------------------------------------------------------------------------
// allpass_t<T,N>::preprocess(in, out) -- the two-buffer form of the block
// all-pass: out[i] gets the outputs one sample late (out[0] is the previous
// block's tail), the last output is carried in mState and written to in[256].
// The write position walks the ring unmasked from muIndex.
// ---------------------------------------------------------------------------
template <typename T, int N>
void allpass_t<T, N>::preprocess(T *in, T *out)
{
    const int kBlock = 256;
    const s32 w = mLine.muIndex;
    const s32 r = w - miLength;
    const T fb = mFeedbackGain;
    const T ff = mFeedforwardGain;

    T state = mState;
    for (int i = 0; i < kBlock; ++i)
    {
        out[i] = state;
        const T delayed = mLine.maBuffer[(r + i) & (N - 1)];
        const T y = ff * delayed + in[i];
        mLine.maBuffer[w + i] = y;
        state = fb * y + delayed;
    }
    mState = state;
    in[kBlock] = state;
    mLine.muIndex = (w + kBlock) & (N - 1);
}

// ---------------------------------------------------------------------------
// twotap_t<T,N>::preprocess -- 256 samples through the two taps: outA[0]/outB[0]
// get the previous tails, outA[1+i]/outB[1+i] the scaled taps of sample i; the
// input is written at the raw write position and the index advances one block.
// ---------------------------------------------------------------------------
template <typename T, int N>
twotap_t<T, N> *twotap_t<T, N>::preprocess(T *in, T *outA, T *outB)
{
    const int kBlock = 256;
    const s32 w = mLine.muIndex;
    const s32 readA = w - miTapA;
    const s32 readB = w - miTapB;
    const T gA = mGainA;
    const T gB = mGainB;
    T *bufW = &mLine.maBuffer[w];

    outA[0] = mStateA;
    outB[0] = mStateB;
    T lastA = mStateA;
    T lastB = mStateB;
    for (int i = 0; i < kBlock; ++i)
    {
        const T x = in[i];
        lastA = mLine.maBuffer[(readA + i) & (N - 1)] * gA;
        lastB = mLine.maBuffer[(readB + i) & (N - 1)] * gB;
        bufW[i] = x;
        outA[1 + i] = lastA;
        outB[1 + i] = lastB;
    }
    mLine.muIndex = (w + kBlock) & (N - 1);
    mStateA = lastA;
    mStateB = lastB;
    return this;
}

// ---------------------------------------------------------------------------
// occlusion_t<T,N>::recalculate -- split the total level (mLevelA + mLevelB, in
// decibels) between the broadband input gain and the high-frequency pole:
//   share  = levelA / total when the total is negative, else 1.0
//   g      = (10^((1 - share) * total * 0.1))^0.5      high-frequency gain
//   mCoefB = (10^(share * total * 0.05))^0.5           broadband gain
//   mCoefA = (1 - c*g - sqrt(2*(1 - c)*g - (1 - c^2)*g^2)) / (1 - g)
//            with c = cos(2*pi*mReferenceHz / mSampleRate), or 0 when g >= 0.995.
// Mixed single/double precision as in the console code.
// ---------------------------------------------------------------------------
template <typename T, int N>
void occlusion_t<T, N>::recalculate()
{
    const T total = mLevelB + mLevelA;
    T share;
    if (total < T(0.0))
        share = mLevelA / total;
    else
        share = T(1.0);

    const T omega = static_cast<T>(static_cast<double>(mReferenceHz) * 6.283185308 / static_cast<double>(mSampleRate));

    const T hfExponent = static_cast<T>((T(1.0) - share) * total) * T(0.1);
    const T hfLevel = static_cast<T>(pow(10.0, static_cast<double>(hfExponent)));
    const T hfGain = static_cast<T>(pow(static_cast<double>(hfLevel), 0.5));

    const T bandExponent = static_cast<T>(share * total) * T(0.05);
    mCoefB = static_cast<T>(pow(10.0, static_cast<double>(bandExponent)));
    mCoefB = static_cast<T>(pow(static_cast<double>(mCoefB), 0.5));

    if (hfGain < T(0.995))
    {
        const T c = static_cast<T>(cos(static_cast<double>(omega)));
        const double g = hfGain;
        const double cd = c;
        const double gg = static_cast<double>(static_cast<T>(hfGain * hfGain));
        const double cc = static_cast<double>(static_cast<T>(c * c));
        const double cg = static_cast<double>(static_cast<T>(c * hfGain));
        const T denom = static_cast<T>(1.0 - g);
        const double root = sqrt((1.0 - cd) * g * 2.0 - (1.0 - cc) * gg);
        mCoefA = static_cast<T>(static_cast<T>((1.0 - cg) - root) / denom);
    }
    else
    {
        mCoefA = T(0.0);
    }
}

// ---------------------------------------------------------------------------
// Coefficient tables of properties_set (dumped from the image). Indexed by the
// properties_t integers as noted.
// ---------------------------------------------------------------------------
namespace
{
// tank1 damping coefficient, by a11.
const f32 kafDampCoef[15] =
{
    0.8584595f, 0.7992554f, 0.7518921f, 0.7104492f, 0.66900635f,
    0.6334839f, 0.60388184f, 0.5742798f, 0.55059814f, 0.5269165f,
    0.50323486f, 0.48547363f, 0.4677124f, 0.44995117f, 0.43218994f,
};
// early output tap-A gain (x decay scale), by a4 / a5.
const f32 kafEarlyOutGainA[31] =
{
    0.999f, 0.97f, 0.95f, 0.92f, 0.9f, 0.85f, 0.8f, 0.75f,
    0.7f, 0.6f, 0.5f, 0.55f, 0.6f, 0.65f, 0.7f, 0.75f,
    0.8f, 0.85f, 0.9f, 0.95f, 0.99f, 0.95f, 0.9f, 0.85f,
    0.8f, 0.75f, 0.7f, 0.65f, 0.62f, 0.6f, 0.6f,
};
// tank1 filter b0, by a11.
const f32 kafFilterB0[15] =
{
    0.14151001f, 0.20071411f, 0.2480774f, 0.28952026f, 0.33096313f,
    0.3664856f, 0.39608765f, 0.4256897f, 0.44937134f, 0.47305298f,
    0.49673462f, 0.51449585f, 0.5322571f, 0.5500183f, 0.56777954f,
};
// early output tap-B gain (x decay scale) and tank2 delay level, by a4 / a5.
const f32 kafEarlyOutGainB[32] =
{
    -0.999f, -0.999f, -0.999f, -0.999f, -0.999f, -0.999f, -0.999f, -0.999f,
    -0.999f, -0.999f, -0.999f, -0.95f, -0.9f, -0.85f, -0.8f, -0.75f,
    -0.7f, -0.6f, -0.5f, -0.45f, -0.5f, -0.55f, -0.6f, -0.65f,
    -0.7f, -0.75f, -0.8f, -0.85f, -0.9f, -0.95f, -0.999f, 0.0f,
};
// early taps-1 tap-C gain, by a2 / a3.
const f32 kafTaps1GainC[32] =
{
    -0.3f, -0.29f, -0.28f, -0.27f, -0.264f, -0.25f, -0.24f, -0.22f,
    -0.192f, -0.17f, -0.144f, -0.13f, -0.12f, -0.11f, -0.096f, -0.082f,
    -0.072f, -0.06f, -0.048f, -0.04f, -0.035f, -0.03f, -0.025f, -0.02f,
    -0.015f, -0.01f, -0.007f, -0.005f, -0.0024f, -0.0024f, -0.0024f, 0.0f,
};
// early taps-1 cross-mix gain, by a3 / a2.
const f32 kafTaps1Mix[32] =
{
    -0.168f, -0.18f, -0.2f, -0.22f, -0.24f, -0.27f, -0.3f, -0.28f,
    -0.264f, -0.25f, -0.24f, -0.23f, -0.216f, -0.2f, -0.192f, -0.175f,
    -0.168f, -0.155f, -0.144f, -0.13f, -0.12f, -0.11f, -0.096f, -0.085f,
    -0.072f, -0.06f, -0.048f, -0.04f, -0.03f, -0.02f, -0.012f, 0.0f,
};
// early taps-2 own-mix gain, by a2 / a3.
const f32 kafTaps2Mix[32] =
{
    -0.024f, -0.03f, -0.036f, -0.042f, -0.048f, -0.08f, -0.12f, -0.15f,
    -0.18f, -0.21f, -0.24f, -0.27f, -0.3f, -0.33f, -0.36f, -0.42f,
    -0.48f, -0.45f, -0.42f, -0.39f, -0.36f, -0.33f, -0.3f, -0.27f,
    -0.24f, -0.22f, -0.2f, -0.18f, -0.15f, -0.1f, -0.06f, 0.0f,
};
// early taps-2 tap-C level and tank1 delay level, by a4 / a5.
const f32 kafTapLevel[32] =
{
    0.125f, 0.140625f, 0.15625f, 0.171875f, 0.1875f, 0.21875f, 0.25f, 0.296875f,
    0.34375f, 0.375f, 0.40625f, 0.46875f, 0.5f, 0.53125f, 0.5625f, 0.59375f,
    0.625f, 0.65625f, 0.6875f, 0.71875f, 0.75f, 0.765625f, 0.78125f, 0.796875f,
    0.8125f, 0.828125f, 0.8359375f, 0.84375f, 0.8515625f, 0.859375f, 0.875f, 0.0f,
};
// tank1 comb gain, by a8 * 10 + a9.
const f32 kafCombGain[130] =
{
    -0.9881592f, -0.98342896f, -0.9789429f, -0.9743042f, -0.96990967f, -0.96551514f, -0.9612427f, -0.9569702f, -0.9527893f, -0.94873047f,
    -0.9862366f, -0.9807129f, -0.97546387f, -0.97006226f, -0.9649353f, -0.95980835f, -0.954834f, -0.9498291f, -0.9449463f, -0.94021606f,
    -0.9842529f, -0.9779663f, -0.97195435f, -0.9658203f, -0.95996094f, -0.95410156f, -0.94836426f, -0.9426575f, -0.9371033f, -0.93167114f,
    -0.98327637f, -0.976593f, -0.97021484f, -0.96365356f, -0.957428f, -0.9512024f, -0.9451599f, -0.9390869f, -0.9331665f, -0.9273987f,
    -0.9822998f, -0.9752197f, -0.9684448f, -0.96154785f, -0.95495605f, -0.94836426f, -0.94192505f, -0.93548584f, -0.92922974f, -0.9231262f,
    -0.98150635f, -0.9741211f, -0.967041f, -0.95980835f, -0.9529419f, -0.9460449f, -0.93933105f, -0.9326172f, -0.9260864f, -0.91970825f,
    -0.9811096f, -0.9735718f, -0.9663391f, -0.9589844f, -0.9519348f, -0.9449158f, -0.9380493f, -0.93118286f, -0.92453f, -0.91799927f,
    -0.9807129f, -0.97302246f, -0.9656372f, -0.95809937f, -0.95092773f, -0.9437561f, -0.9367676f, -0.92974854f, -0.9229431f, -0.9162903f,
    -0.9803467f, -0.97247314f, -0.9649658f, -0.9572754f, -0.9499512f, -0.94262695f, -0.93548584f, -0.9283447f, -0.9213867f, -0.9146118f,
    -0.9803467f, -0.97247314f, -0.9649658f, -0.9572754f, -0.9499512f, -0.94262695f, -0.93548584f, -0.9283447f, -0.9213867f, -0.9146118f,
    -0.9803467f, -0.97247314f, -0.9649658f, -0.9572754f, -0.9499512f, -0.94262695f, -0.93548584f, -0.9283447f, -0.9213867f, -0.9146118f,
    -0.9803467f, -0.97247314f, -0.9649658f, -0.9572754f, -0.9499512f, -0.94262695f, -0.93548584f, -0.9283447f, -0.9213867f, -0.9146118f,
    -0.9803467f, -0.97247314f, -0.9649658f, -0.9572754f, -0.9499512f, -0.94262695f, -0.93548584f, -0.9283447f, -0.9213867f, -0.9146118f,
};
// tank1 filter a1, by a8 * 10 + a9.
const f32 kafFilterA1[130] =
{
    0.9803467f, 0.97247314f, 0.9649658f, 0.9572754f, 0.9499512f, 0.94262695f, 0.93548584f, 0.9283447f, 0.9213867f, 0.9146118f,
    0.9803467f, 0.97247314f, 0.9649658f, 0.9572754f, 0.9499512f, 0.94262695f, 0.93548584f, 0.9283447f, 0.9213867f, 0.9146118f,
    0.9803467f, 0.97247314f, 0.9649658f, 0.9572754f, 0.9499512f, 0.94262695f, 0.93548584f, 0.9283447f, 0.9213867f, 0.9146118f,
    0.9803467f, 0.97247314f, 0.9649658f, 0.9572754f, 0.9499512f, 0.94262695f, 0.93548584f, 0.9283447f, 0.9213867f, 0.9146118f,
    0.9803467f, 0.97247314f, 0.9649658f, 0.9572754f, 0.9499512f, 0.94262695f, 0.93548584f, 0.9283447f, 0.9213867f, 0.9146118f,
    0.9803467f, 0.97247314f, 0.9649658f, 0.9572754f, 0.9499512f, 0.94262695f, 0.93548584f, 0.9283447f, 0.9213867f, 0.9146118f,
    0.9803467f, 0.97247314f, 0.9649658f, 0.9572754f, 0.9499512f, 0.94262695f, 0.93548584f, 0.9283447f, 0.9213867f, 0.9146118f,
    0.9803467f, 0.97247314f, 0.9649658f, 0.9572754f, 0.9499512f, 0.94262695f, 0.93548584f, 0.9283447f, 0.9213867f, 0.9146118f,
    0.9803467f, 0.97247314f, 0.9649658f, 0.9572754f, 0.9499512f, 0.94262695f, 0.93548584f, 0.9283447f, 0.9213867f, 0.9146118f,
    0.9807129f, 0.97283936f, 0.9656372f, 0.95809937f, 0.95092773f, 0.9437561f, 0.9367676f, 0.92974854f, 0.9229431f, 0.9162903f,
    0.9811096f, 0.9735718f, 0.9663391f, 0.9589844f, 0.9519348f, 0.9449158f, 0.9380493f, 0.93118286f, 0.92453f, 0.91799927f,
    0.9819031f, 0.9746704f, 0.9677429f, 0.96066284f, 0.953949f, 0.9472046f, 0.9406433f, 0.9340515f, 0.92767334f, 0.92141724f,
    0.98269653f, 0.97576904f, 0.96902466f, 0.9623718f, 0.9559326f, 0.9494934f, 0.9432068f, 0.93692017f, 0.93081665f, 0.9248352f,
};
// tank1 filter b1, by a10 * 15 + a11.
const f32 kafFilterB1[136] =
{
    0.00421143f, 0.02093506f, 0.10089111f, 0.09942627f, 0.09112549f, 0.07855225f, 0.06414795f, 0.04629517f,
    0.02944946f, 0.01031494f, 0.46743774f, 0.48547363f, 0.5036011f, 0.5216675f, 0.5553894f, 0.00180054f,
    0.03201294f, 0.03897095f, 0.03936768f, 0.03384399f, 0.0241394f, 0.01242065f, 0.29281616f, 0.31710815f,
    0.34155273f, 0.36618042f, 0.38461304f, 0.40325928f, 0.42190552f, 0.44058228f, -0.0065918f, 0.00259399f,
    0.00653076f, 0.00585938f, 0.00042725f, 0.14050293f, 0.16775513f, 0.1958313f, 0.21881104f, 0.24215698f,
    0.2705078f, 0.28399658f, 0.30218506f, 0.32052612f, 0.34191895f, -0.06344604f, -0.0569458f, -0.04238892f,
    -0.02407837f, -0.00140381f, 0.02096558f, 0.04150391f, 0.06362915f, 0.08233643f, 0.10177612f, 0.12210083f,
    0.13772583f, 0.15377808f, 0.17019653f, 0.18692017f, -0.09091187f, -0.1036377f, -0.10513306f, -0.10070801f,
    -0.09152222f, -0.08010864f, -0.06829834f, -0.05459595f, -0.04223633f, -0.02874756f, -0.01422119f, -0.00268555f,
    0.00939941f, 0.01861572f, 0.0352478f, -0.10440063f, -0.12747192f, -0.13851929f, -0.14300537f, -0.14297485f,
    -0.13943481f, -0.13415527f, -0.12683105f, -0.11956787f, -0.11102295f, -0.10137939f, -0.09338379f, -0.0847168f,
    -0.07543945f, -0.06573486f, -0.1137085f, -0.14550781f, -0.1645813f, -0.1767273f, -0.18481445f, -0.18887329f,
    -0.18948364f, -0.18841553f, -0.18621826f, -0.18273926f, -0.1781311f, -0.17382812f, -0.1689148f, -0.16339111f,
    -0.15716553f, -0.11819458f, -0.15408325f, -0.17718506f, -0.19332886f, -0.205719f, -0.2133789f, -0.2177124f,
    -0.22015381f, -0.22076416f, -0.22027588f, -0.2185669f, -0.21652222f, -0.21386719f, -0.21054077f, -0.206604f,
    -0.1204834f, -0.15847778f, -0.1836853f, -0.2019043f, -0.21661377f, -0.22640991f, -0.23260498f, -0.23699951f,
    -0.23925781f, -0.24032593f, -0.24035645f, -0.23953247f, -0.23809814f, -0.23605347f, -0.23342896f, 0.0f,
};
// tank1 damping gain, by a10 * 15 + a11.
const f32 kafDampGain[136] =
{
    0.12136841f, 0.1131897f, 0.08883667f, 0.09301758f, 0.09799194f, 0.10293579f, 0.10757446f, 0.11273193f,
    0.11730957f, 0.12231445f, 0.06439209f, 0.06430054f, 0.06420898f, 0.06414795f, 0.0640564f, 0.12341309f,
    0.10778809f, 0.10803223f, 0.11001587f, 0.11340332f, 0.11724854f, 0.1211853f, 0.07403564f, 0.07327271f,
    0.0725708f, 0.07192993f, 0.07150269f, 0.07110596f, 0.07073975f, 0.07037354f, 0.13110352f, 0.12338257f,
    0.12179565f, 0.12249756f, 0.12481689f, 0.09033203f, 0.08779907f, 0.08560181f, 0.08404541f, 0.08267212f,
    0.08129883f, 0.08053589f, 0.07971191f, 0.07894897f, 0.07824707f, 0.22662354f, 0.17453003f, 0.15075684f,
    0.13635254f, 0.1255188f, 0.1182251f, 0.11312866f, 0.10873413f, 0.10562134f, 0.10284424f, 0.10031128f,
    0.09860229f, 0.09695435f, 0.09545898f, 0.09402466f, 0.3496399f, 0.25854492f, 0.21694946f, 0.19168091f,
    0.17279053f, 0.15997314f, 0.15106201f, 0.1434021f, 0.13796997f, 0.13308716f, 0.12869263f, 0.12564087f,
    0.12283325f, 0.12017822f, 0.11767578f, 0.47262573f, 0.34262085f, 0.2831421f, 0.24707031f, 0.22009277f,
    0.20178223f, 0.18902588f, 0.17807007f, 0.1703186f, 0.16333008f, 0.15704346f, 0.15270996f, 0.14865112f,
    0.14486694f, 0.1413269f, 0.6369934f, 0.45458984f, 0.37145996f, 0.32092285f, 0.2831421f, 0.25750732f,
    0.23965454f, 0.22427368f, 0.21347046f, 0.20370483f, 0.19491577f, 0.18878174f, 0.18310547f, 0.17782593f,
    0.17285156f, 0.7593384f, 0.5385132f, 0.4375f, 0.3763733f, 0.33035278f, 0.29925537f, 0.2776184f,
    0.25894165f, 0.24575806f, 0.23394775f, 0.22323608f, 0.21585083f, 0.20898438f, 0.20254517f, 0.19650269f,
    0.84173584f, 0.5944824f, 0.4817505f, 0.41314697f, 0.36187744f, 0.3270874f, 0.30288696f, 0.28204346f,
    0.26739502f, 0.25411987f, 0.24221802f, 0.23391724f, 0.22619629f, 0.21899414f, 0.21228027f, 0.0f,
};
// diffusion all-pass gains, by a6 (early) / a7 (tank).
const f32 kafDiffusion[16] =
{
    0.0f, 0.015625f, 0.03125f, 0.046875f, 0.0703125f, 0.09375f, 0.125f, 0.15625f,
    0.1953125f, 0.234375f, 0.2734375f, 0.3125f, 0.375f, 0.4375f, 0.5f, 0.625f,
};

// properties_t milliseconds -> seconds.
const f32 KF_MS_TO_SECONDS = 0.001f;
} // namespace

// ---------------------------------------------------------------------------
// stereo_room_t<T>::stereo_room_t -- default properties at 48 kHz, unity output
// level, every filter's taps/gains at their defaults, every ring reset.
// ---------------------------------------------------------------------------
template <typename T>
stereo_room_t<T>::stereo_room_t()
{
    mSampleRate = T(48000.0);
    mWetDryMix  = T(1.0);
    mOutputGain = static_cast<T>(pow(10.0, 0.0));
    mInputMode  = 0;

    for (int ch = 0; ch < 2; ++ch)
    {
        vardelay_t<T, 16384> &pre = maPreDelay[ch];
        pre.mSampleRate = T(48000.0);
        pre.mMix        = T(0.0);
        pre.mTapNew     = 40;
        pre.mState      = T(0.0);
        pre.mTapNew2    = 40;
        pre.mTapTarget  = 40;
        pre.mLine.mFillValue = T(0.0);
        pre.mLine.muIndex    = 0;
        pre.mLine.reset();
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        occlusion_t<T, 2> &occ = maOcclusion[ch];
        occ.mSampleRate  = mSampleRate;
        occ.mReferenceHz = mSampleRate * T(0.5);
        occ.mLevelA      = T(0.0);
        occ.mLevelB      = T(0.0);
        occ.mCoefB       = T(0.0);
        occ.mCoefA       = T(0.0);
        occ.recalculate();
    }

    // Early networks: identical except for the diffusion lengths.
    static const s32 kaiDiffuse1[2] = { 83, 97 };
    static const s32 kaiTaps2A[2]   = { 2039, 1297 };
    static const s32 kaiDiffuse2[2] = { 211, 223 };
    static const s32 kaiDiffuse3[2] = { 311, 293 };
    for (int ch = 0; ch < 2; ++ch)
    {
        early_t &e = maEarly[ch];
        e.mTaps2Gain = T(-0.0675);
        e.mTaps1Gain = T(-0.0125);

        e.mTaps1.miTapA     = 509;
        e.mTaps1.mGainA     = T(0.2);
        e.mTaps1.miTapB     = 508;
        e.mTaps1.mStateA    = T(0.0);
        e.mTaps1.miTapC     = 1;
        e.mTaps1.mGainB     = T(0.4);
        e.mTaps1.mReserved  = T(0.0);
        e.mTaps1.mGainC     = T(-0.001);
        e.mTaps1.mStateB    = T(0.0);
        e.mTaps1.mLine.muIndex    = 0;
        e.mTaps1.mLine.mFillValue = T(0.0);
        e.mTaps1.mLine.reset();

        e.mDiffuse1.mFeedbackGain    = T(-0.375);
        e.mDiffuse1.miLength         = kaiDiffuse1[ch];
        e.mDiffuse1.mFeedforwardGain = T(0.375);
        e.mDiffuse1.mState           = T(0.0);
        e.mDiffuse1.mLine.muIndex    = 0;
        e.mDiffuse1.mLine.mFillValue = T(0.0);
        e.mDiffuse1.mLine.reset();

        e.mTaps2.mGainA     = T(0.2);
        e.mTaps2.mStateA    = T(0.0);
        e.mTaps2.mGainB     = T(0.6);
        e.mTaps2.mReserved  = T(0.0);
        e.mTaps2.miTapA     = kaiTaps2A[ch];
        e.mTaps2.mStateB    = T(0.0);
        e.mTaps2.miTapB     = kaiTaps2A[ch] - 1;
        e.mTaps2.mGainC     = T(0.05);
        e.mTaps2.miTapC     = 600;
        e.mTaps2.mLine.mFillValue = T(0.0);
        e.mTaps2.mLine.muIndex    = 0;
        e.mTaps2.mLine.reset();

        e.mDiffuse2.mFeedbackGain    = T(-0.5);
        e.mDiffuse2.miLength         = kaiDiffuse2[ch];
        e.mDiffuse2.mFeedforwardGain = T(0.5);
        e.mDiffuse2.mState           = T(0.0);
        e.mDiffuse2.mLine.muIndex    = 0;
        e.mDiffuse2.mLine.mFillValue = T(0.0);
        e.mDiffuse2.mLine.reset();

        e.mDiffuse3.mFeedbackGain    = T(-0.5);
        e.mDiffuse3.miLength         = kaiDiffuse3[ch];
        e.mDiffuse3.mFeedforwardGain = T(0.5);
        e.mDiffuse3.mState           = T(0.0);
        e.mDiffuse3.mLine.muIndex    = 0;
        e.mDiffuse3.mLine.mFillValue = T(0.0);
        e.mDiffuse3.mLine.reset();

        e.mOutput.mGainA  = T(-0.7);
        e.mOutput.miTapA  = 1020;
        e.mOutput.mStateA = T(0.0);
        e.mOutput.miTapB  = 1;
        e.mOutput.mGainB  = T(0.04375);
        e.mOutput.mStateB = T(0.0);
        e.mOutput.mLine.muIndex    = 0;
        e.mOutput.mLine.mFillValue = T(0.0);
        e.mOutput.mLine.reset();
    }

    // Tank first halves.
    static const s32 kaiTankAp1[2]   = { 409, 383 };
    static const s32 kaiTankAp2[2]   = { 257, 233 };
    static const s32 kaiTankDelay[2] = { 3432, 3820 };
    for (int ch = 0; ch < 2; ++ch)
    {
        tank1_t &t = maTank1[ch];
        t.mAllpass1.mFeedbackGain    = T(-0.618);
        t.mAllpass1.miLength         = kaiTankAp1[ch];
        t.mAllpass1.mFeedforwardGain = T(0.618);
        t.mAllpass1.mState           = T(0.0);
        t.mAllpass1.mLine.muIndex    = 0;
        t.mAllpass1.mLine.mFillValue = T(0.0);
        t.mAllpass1.mLine.reset();

        t.mAllpass2.mFeedbackGain    = T(-0.5);
        t.mAllpass2.miLength         = kaiTankAp2[ch];
        t.mAllpass2.mFeedforwardGain = T(0.5);
        t.mAllpass2.mState           = T(0.0);
        t.mAllpass2.mLine.muIndex    = 0;
        t.mAllpass2.mLine.mFillValue = T(0.0);
        t.mAllpass2.mLine.reset();

        t.mComb.mGain     = T(-0.99845);
        t.mComb.miTap     = 1;
        t.mComb.mOut      = T(0.0);
        t.mComb.mReserved = T(0.0);
        t.mComb.maHist[0] = T(0.0);
        t.mComb.maHist[1] = T(0.0);

        t.mFilter.miTap     = 1;
        t.mFilter.mB0       = T(0.23906);
        t.mFilter.mA1       = T(0.98453);
        t.mFilter.mB1       = T(-0.0901);
        t.mFilter.mOut      = T(0.0);
        t.mFilter.mReserved = T(0.0);
        t.mFilter.maW[0]    = T(0.0);
        t.mFilter.maW[1]    = T(0.0);

        t.mDamp.miTap     = 1;
        t.mDamp.mOut      = T(0.0);
        t.mDamp.mGain     = T(0.2006);
        t.mDamp.mCoef     = T(0.76094);
        t.mDamp.mReserved = T(0.0);
        t.mDamp.maHist[0] = T(0.0);
        t.mDamp.maHist[1] = T(0.0);

        const T unity = static_cast<T>(pow(10.0, 0.0));
        t.mDelay.mGainA  = T(1.0);
        t.mDelay.miTapA  = kaiTankDelay[ch];
        t.mDelay.miTapB  = kaiTankDelay[ch];
        t.mDelay.mGainB  = unity * T(0.9999);
        t.mDelay.mStateA = T(0.0);
        t.mDelay.mStateB = T(0.0);
        t.mDelay.mLine.muIndex    = 0;
        t.mDelay.mLine.mFillValue = T(0.0);
        t.mDelay.mLine.reset();

        t.mDelayTap.mGain     = T(0.6);
        t.mDelayTap.mOut      = T(0.0);
        t.mDelayTap.miTap     = 1;
        t.mDelayTap.mReserved = T(0.0);
        t.mDelayTap.maHist[0] = T(0.0);
        t.mDelayTap.maHist[1] = T(0.0);
    }

    // Tank second halves.
    static const s32 kaiTank2Ap1[2]   = { 1511, 1657 };
    static const s32 kaiTank2Ap2[2]   = { 1061, 1103 };
    static const s32 kaiTank2Ap3[2]   = { 853, 887 };
    static const s32 kaiTank2Ap4[2]   = { 541, 491 };
    static const s32 kaiTank2Delay[2] = { 1510, 1438 };
    static const T   kafTone[2]       = { T(0.3), T(-0.3) };
    for (int ch = 0; ch < 2; ++ch)
    {
        tank2_t &t = maTank2[ch];
        t.mAllpass1.mFeedbackGain    = T(-0.618);
        t.mAllpass1.miLength         = kaiTank2Ap1[ch];
        t.mAllpass1.mFeedforwardGain = T(0.618);
        t.mAllpass1.mState           = T(0.0);
        t.mAllpass1.mLine.muIndex    = 0;
        t.mAllpass1.mLine.mFillValue = T(0.0);
        t.mAllpass1.mLine.reset();

        t.mAllpass2.mInputGain    = T(0.618);
        t.mAllpass2.miLength      = kaiTank2Ap2[ch];
        t.mAllpass2.mFeedbackGain = T(0.618);
        t.mAllpass2.mDelayGain    = T(-0.618);
        t.mAllpass2.mOut          = T(0.0);
        t.mAllpass2.mLine.muIndex    = 0;
        t.mAllpass2.mLine.mFillValue = T(0.0);
        t.mAllpass2.mLine.reset();

        t.mAllpass3.mFeedbackGain    = T(-0.618);
        t.mAllpass3.miLength         = kaiTank2Ap3[ch];
        t.mAllpass3.mFeedforwardGain = T(0.618);
        t.mAllpass3.mState           = T(0.0);
        t.mAllpass3.mLine.muIndex    = 0;
        t.mAllpass3.mLine.mFillValue = T(0.0);
        t.mAllpass3.mLine.reset();

        t.mAllpass4.mFeedbackGain    = T(-0.5);
        t.mAllpass4.miLength         = kaiTank2Ap4[ch];
        t.mAllpass4.mFeedforwardGain = T(0.5);
        t.mAllpass4.mState           = T(0.0);
        t.mAllpass4.mLine.muIndex    = 0;
        t.mAllpass4.mLine.mFillValue = T(0.0);
        t.mAllpass4.mLine.reset();

        const T unity = static_cast<T>(pow(10.0, 0.0));
        t.mDelay.mGainA  = T(1.0);
        t.mDelay.miTapA  = kaiTank2Delay[ch];
        t.mDelay.mGainB  = unity * T(0.9999);
        t.mDelay.mStateA = T(0.0);
        t.mDelay.mStateB = T(0.0);
        t.mDelay.miTapB  = kaiTank2Delay[ch];
        t.mDelay.mLine.muIndex    = 0;
        t.mDelay.mLine.mFillValue = T(0.0);
        t.mDelay.mLine.reset();

        t.mTone.mOut      = T(0.0);
        t.mTone.miTap     = 1;
        t.mTone.mGain     = kafTone[ch];
        t.mTone.mCoef     = kafTone[ch];
        t.mTone.mReserved = T(0.0);
        t.mTone.maHist[0] = T(0.0);
        t.mTone.maHist[1] = T(0.0);
    }

    static const s32 kaiOutDiffuse[4] = { 131, 113, 107, 127 };
    for (int i = 0; i < 4; ++i)
    {
        allpass_t<T, 512> &ap = maOutDiffuse[i];
        ap.miLength         = kaiOutDiffuse[i];
        ap.mFeedbackGain    = T(-0.309);
        ap.mFeedforwardGain = T(0.309);
        ap.mState           = T(0.0);
        ap.mLine.muIndex    = 0;
        ap.mLine.mFillValue = T(0.0);
        ap.mLine.reset();
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        vardelay_t<T, 256> &rear = maRearDelay[ch];
        rear.mSampleRate = T(48000.0);
        rear.mTapTarget  = 20;
        rear.mMix        = T(0.0);
        rear.mTapNew2    = 20;
        rear.mState      = T(0.0);
        rear.mTapNew     = 20;
        rear.mLine.mFillValue = T(0.0);
        rear.mLine.muIndex    = 0;
        rear.mLine.reset();
    }
}

// ---------------------------------------------------------------------------
// stereo_room_t<T>::properties_set -- take a copy of the block, then derive:
//   * the tank delay taps from a1 (ms, snapped to a prime, at most 4095);
//   * a room scale from f20 (diffusion %) clamped to [0.3, 1.0] and a size split
//     from f19 (room size %) into four 0..1 shares (one per 25% band);
//   * every early/tank tap length (default length x scale, snapped to a prime);
//   * the loop gain for a 60 dB decay over f18 seconds across the summed tank
//     loop lengths, which sets the tank feedback taps and tone filters;
//   * the gains from the coefficient tables;
//   * the tank tap levels from f17 (dB), the output level from f16 (dB);
//   * the occlusion corner/levels (f13, f14, f15);
//   * the pre-delay (a0) and rear-delay (a12) targets, applied immediately when
//     the line is not mid-crossfade.
// ---------------------------------------------------------------------------
template <typename T>
stereo_room_t<T> *stereo_room_t<T>::properties_set(const properties_t &props)
{
    memcpy(&mProperties, &props, sizeof(properties_t));

    const T msToSeconds = KF_MS_TO_SECONDS;

    s32 tankTap;
    if (props.a1 == 0)
        tankTap = nearest_prime(1);
    else
        tankTap = nearest_prime(static_cast<u32>(static_cast<s64>(
            mSampleRate * static_cast<T>(static_cast<u32>(props.a1)) * msToSeconds)));
    if (static_cast<u32>(tankTap) > 0xFFFu)
        tankTap = 0xFFF;
    maTank1[0].mDelay.miTapB = tankTap;
    maTank1[1].mDelay.miTapB = tankTap;
    maTank2[0].mDelay.miTapB = tankTap;
    maTank2[1].mDelay.miTapB = tankTap;

    const T unity = T(1.0);

    T diffusionSpan;
    if (props.f20 < T(50.0))
        diffusionSpan = (props.f20 * T(0.4) + T(30.0)) * T(0.01) * mSampleRate;
    else
        diffusionSpan = props.f20 * T(0.01) * mSampleRate;

    T scale = diffusionSpan * T(2.0833333e-05);
    if (scale < T(0.3))
        scale = T(0.3);

    T span = scale * T(2.0);
    if (span < T(0.3))
        span = T(0.3);
    else if (span > unity)
        span = unity;

    // Room size split into four bands of 25%.
    T size3, size2, size1, size0;
    const T size = props.f19;
    if (!(size < T(75.0)))
    {
        size2 = unity;
        size1 = unity;
        size0 = unity;
        size3 = (size - T(75.0)) * T(0.04);
    }
    else if (!(size < T(50.0)))
    {
        size1 = unity;
        size0 = unity;
        size3 = T(0.0);
        size2 = (size - T(50.0)) * T(0.04);
    }
    else
    {
        size3 = T(0.0);
        size2 = size3;
        if (!(size < T(25.0)))
        {
            size0 = unity;
            size1 = (size - T(25.0)) * T(0.04);
        }
        else
        {
            size1 = size3;
            size0 = size * T(0.04);
        }
    }

    // Early network taps.
    const u32 taps1Len = static_cast<u32>(static_cast<s64>(span * T(509.0)));
    s32 p = nearest_prime(taps1Len);
    maEarly[0].mTaps1.miTapA = p;
    maEarly[0].mTaps1.miTapB = p - 1;
    p = nearest_prime(static_cast<u32>(static_cast<s64>(span * T(2039.0))));
    maEarly[0].mTaps2.miTapA = p;
    maEarly[0].mTaps2.miTapB = p - 1;

    const u32 outBase = (props.a1 == 0) ? 1020u : 9u;
    const u32 outLen = static_cast<u32>(static_cast<s64>(static_cast<T>(outBase) * span));
    maEarly[0].mOutput.miTapA = nearest_prime(outLen);

    p = nearest_prime(taps1Len);
    maEarly[1].mTaps1.miTapA = p;
    maEarly[1].mTaps1.miTapB = p - 1;
    p = nearest_prime(static_cast<u32>(static_cast<s64>(span * T(1297.0))));
    maEarly[1].mTaps2.miTapA = p;
    maEarly[1].mTaps2.miTapB = p - 1;
    maEarly[1].mOutput.miTapA = nearest_prime(outLen);

    // Tank loop lengths.
    const T size3Span = size3 * span;
    p = nearest_prime(static_cast<u32>(static_cast<s64>(size3Span * T(409.0))));
    if (p == 0)
        p = 1;
    maTank1[0].mAllpass1.miLength = p;
    maTank1[0].mAllpass2.miLength = nearest_prime(static_cast<u32>(static_cast<s64>(span * T(257.0))));
    p = nearest_prime(static_cast<u32>(static_cast<s64>(size3 * scale * T(3432.0))));
    if (p == 0)
        p = 1;
    maTank1[0].mDelay.miTapA = p;

    p = nearest_prime(static_cast<u32>(static_cast<s64>(size3Span * T(383.0))));
    if (p == 0)
        p = 1;
    maTank1[1].mAllpass1.miLength = p;
    maTank1[1].mAllpass2.miLength = nearest_prime(static_cast<u32>(static_cast<s64>(span * T(233.0))));
    p = nearest_prime(static_cast<u32>(static_cast<s64>(size2 * scale * T(3820.0))));
    if (p == 0)
        p = 1;
    maTank1[1].mDelay.miTapA = p;

    const T size1Span = size1 * span;
    p = nearest_prime(static_cast<u32>(static_cast<s64>(size1Span * T(1511.0))));
    if (p == 0)
        p = 1;
    maTank2[0].mAllpass1.miLength = p;
    const T size2Span = size2 * span;
    p = nearest_prime(static_cast<u32>(static_cast<s64>(size2Span * T(1061.0))));
    if (p == 0)
        p = 1;
    maTank2[0].mAllpass2.miLength = p - 1;
    maTank2[0].mAllpass3.miLength = nearest_prime(static_cast<u32>(static_cast<s64>(span * T(853.0))));
    p = nearest_prime(static_cast<u32>(static_cast<s64>(size3Span * T(541.0))));
    if (p == 0)
        p = 1;
    maTank2[0].mAllpass4.miLength = p;
    p = nearest_prime(static_cast<u32>(static_cast<s64>(size0 * scale * T(1510.0))));
    if (p == 0)
        p = 1;
    maTank2[0].mDelay.miTapA = p;

    p = nearest_prime(static_cast<u32>(static_cast<s64>(size1Span * T(1657.0))));
    if (p == 0)
        p = 1;
    maTank2[1].mAllpass1.miLength = p;
    p = nearest_prime(static_cast<u32>(static_cast<s64>(size2Span * T(1103.0))));
    if (p == 0)
        p = 1;
    maTank2[1].mAllpass2.miLength = p;
    p = nearest_prime(static_cast<u32>(static_cast<s64>(size3Span * T(887.0))));
    if (p == 0)
        p = 1;
    maTank2[1].mAllpass3.miLength = p;
    maTank2[1].mAllpass4.miLength = nearest_prime(static_cast<u32>(static_cast<s64>(size3Span * T(491.0))));
    p = nearest_prime(static_cast<u32>(static_cast<s64>(scale * T(1438.0))));
    if (p == 0)
        p = 1;

    // Loop gain for a 60 dB decay over f18 seconds across half the summed loop.
    const u32 loopSum = static_cast<u32>(
        maTank1[1].mAllpass2.miLength + maTank2[1].mAllpass1.miLength + maTank1[1].mAllpass1.miLength +
        maTank2[1].mAllpass4.miLength + maTank2[1].mAllpass3.miLength + maTank1[0].mDelay.miTapA +
        maTank2[1].mAllpass2.miLength + maTank2[0].mAllpass3.miLength + maTank2[0].mAllpass2.miLength +
        maTank2[0].mDelay.miTapA + maTank1[0].mAllpass2.miLength + maTank2[0].mAllpass4.miLength +
        maTank1[0].mAllpass1.miLength + maTank2[0].mAllpass1.miLength + p + maTank1[1].mDelay.miTapA);
    maTank2[1].mDelay.miTapA = p;
    const T loopSeconds = static_cast<T>(loopSum >> 1) / mSampleRate;
    const double loopGain = pow(10.0, -3.0 / (static_cast<double>(props.f18) / static_cast<double>(loopSeconds)));
    const T feedback = static_cast<T>(sqrt(loopGain * 0.5));
    const T tone = feedback * T(0.5);
    const T decayScale = T(0.9) - feedback * T(0.63);

    const s32 i2 = props.a2;
    const s32 i3 = props.a3;
    const s32 i4 = props.a4;
    const s32 i5 = props.a5;
    const s32 combIndex = props.a8 * 10 + props.a9;
    const s32 filterIndex = props.a10 * 15 + props.a11;

    const T earlyDiffusion = kafDiffusion[props.a6];
    const T tankDiffusion  = kafDiffusion[props.a7];

    const T tapLevel0   = kafTapLevel[i4];
    const T tapLevel1   = kafTapLevel[i5];
    const T outGainA0   = kafEarlyOutGainA[i4] * decayScale;
    const T outGainA1   = kafEarlyOutGainA[i5] * decayScale;
    const T outGainB0   = kafEarlyOutGainB[i4] * decayScale;
    const T outGainB1   = kafEarlyOutGainB[i5] * decayScale;
    const T tank2Level0 = outGainB0 * T(-0.125);
    const T tank2Level1 = outGainB1 * T(-0.125);

    const T combGain    = kafCombGain[combIndex];
    const T filterA1    = kafFilterA1[combIndex];
    const T filterB0    = kafFilterB0[props.a11];
    const T filterB1    = kafFilterB1[filterIndex];
    const T dampGain    = kafDampGain[filterIndex];
    const T dampCoef    = kafDampCoef[props.a11];

    // Pre-delay target.
    s32 preDelay;
    if (props.a0 == 0)
        preDelay = 1;
    else
        preDelay = static_cast<s32>(static_cast<s64>(
            static_cast<T>(static_cast<u32>(props.a0)) * mSampleRate * msToSeconds));
    for (int ch = 0; ch < 2; ++ch)
    {
        vardelay_t<T, 16384> &pre = maPreDelay[ch];
        pre.mTapTarget = preDelay;
        if (pre.mTapNew2 == pre.mTapNew)
        {
            pre.mTapNew = preDelay;
            pre.mMix = unity;
        }
    }

    // Early gains.
    maEarly[0].mTaps1.mGainA = T(0.2);
    maEarly[0].mTaps1.mGainB = T(0.4);
    maEarly[0].mTaps1.mGainC = kafTaps1GainC[i2];
    maEarly[0].mDiffuse1.mFeedforwardGain = earlyDiffusion;
    maEarly[0].mDiffuse1.mFeedbackGain    = -earlyDiffusion;
    maEarly[0].mTaps2.mGainA = T(0.2);
    maEarly[0].mTaps2.mGainB = T(0.6);
    maEarly[0].mTaps2.mGainC = tapLevel0 * T(-0.125);
    maEarly[0].mDiffuse2.mFeedforwardGain = earlyDiffusion;
    maEarly[0].mDiffuse2.mFeedbackGain    = -earlyDiffusion;
    maEarly[0].mDiffuse3.mFeedforwardGain = earlyDiffusion;
    maEarly[0].mDiffuse3.mFeedbackGain    = -earlyDiffusion;
    maEarly[1].mTaps1.mGainA = T(0.2);
    maEarly[0].mOutput.mGainA = outGainA0;
    maEarly[0].mOutput.mGainB = outGainB0;
    maEarly[0].mTaps1Gain = kafTaps1Mix[i3];
    maEarly[0].mTaps2Gain = kafTaps2Mix[i2];
    maEarly[1].mTaps1.mGainB = T(0.4);
    maEarly[1].mTaps1.mGainC = kafTaps1GainC[i3];
    maEarly[1].mDiffuse1.mFeedforwardGain = earlyDiffusion;
    maEarly[1].mDiffuse1.mFeedbackGain    = -earlyDiffusion;
    maEarly[1].mTaps2.mGainA = T(0.2);
    maEarly[1].mTaps2.mGainB = T(0.6);
    maEarly[1].mTaps2.mGainC = tapLevel1 * T(-0.125);
    maEarly[1].mDiffuse2.mFeedforwardGain = earlyDiffusion;
    maEarly[1].mDiffuse2.mFeedbackGain    = -earlyDiffusion;
    maEarly[1].mDiffuse3.mFeedforwardGain = earlyDiffusion;
    maEarly[1].mDiffuse3.mFeedbackGain    = -earlyDiffusion;
    maEarly[1].mOutput.mGainA = outGainA1;
    maEarly[1].mOutput.mGainB = outGainB1;
    maEarly[1].mTaps1Gain = kafTaps1Mix[i2];
    maEarly[1].mTaps2Gain = kafTaps2Mix[i3];

    // Tank gains.
    for (int ch = 0; ch < 2; ++ch)
    {
        tank1_t &t = maTank1[ch];
        t.mAllpass1.mFeedforwardGain = tankDiffusion;
        t.mAllpass1.mFeedbackGain    = -tankDiffusion;
        t.mAllpass2.mFeedforwardGain = tankDiffusion;
        t.mAllpass2.mFeedbackGain    = -tankDiffusion;
        t.mDamp.mGain    = dampGain;
        t.mDamp.mCoef    = dampCoef;
        t.mComb.mGain    = combGain;
        t.mFilter.mB1    = filterB1;
        t.mFilter.mB0    = filterB0;
        t.mFilter.mA1    = filterA1;
        t.mDelayTap.mGain = feedback;
    }
    maTank2[0].mAllpass4.mFeedforwardGain = tankDiffusion;
    maTank2[0].mAllpass4.mFeedbackGain    = -tankDiffusion;
    maTank2[0].mTone.mGain = tone;
    maTank2[0].mTone.mCoef = tone;
    maTank2[1].mAllpass4.mFeedforwardGain = tankDiffusion;
    maTank2[1].mAllpass4.mFeedbackGain    = -tankDiffusion;
    maTank2[1].mTone.mGain = -tone;
    maTank2[1].mTone.mCoef = -tone;

    // Tank tap levels (f17, dB) and the output level (f16, dB).
    maTank1[0].mDelay.mGainB = static_cast<T>(pow(10.0, static_cast<double>(props.f17) * 0.05)) * tapLevel0;
    maTank1[1].mDelay.mGainB = static_cast<T>(pow(10.0, static_cast<double>(props.f17) * 0.05)) * tapLevel1;
    maTank2[0].mDelay.mGainB = static_cast<T>(pow(10.0, static_cast<double>(props.f17) * 0.05)) * tank2Level0;
    maTank2[1].mDelay.mGainB = static_cast<T>(pow(10.0, static_cast<double>(props.f17) * 0.05)) * tank2Level1;
    mOutputGain = static_cast<T>(pow(10.0, static_cast<double>(props.f16) * 0.05));

    const T outDiffusion = tankDiffusion * T(0.6);
    for (int i = 0; i < 4; ++i)
    {
        maOutDiffuse[i].mFeedforwardGain = outDiffusion;
        maOutDiffuse[i].mFeedbackGain    = -outDiffusion;
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        occlusion_t<T, 2> &occ = maOcclusion[ch];
        occ.mReferenceHz = props.f13;
        occ.recalculate();
        occ.mLevelA = props.f14;
        occ.recalculate();
        occ.mLevelB = props.f15;
        occ.recalculate();
    }

    // Rear-delay target.
    s32 rearDelay;
    if (props.a12 == 0)
        rearDelay = 1;
    else
        rearDelay = static_cast<s32>(static_cast<s64>(
            static_cast<T>(static_cast<u32>(props.a12)) * mSampleRate * msToSeconds));
    for (int ch = 0; ch < 2; ++ch)
    {
        vardelay_t<T, 256> &rear = maRearDelay[ch];
        rear.mTapTarget = rearDelay;
        if (rear.mTapNew2 == rear.mTapNew)
        {
            rear.mTapNew = rearDelay;
            rear.mMix = unity;
        }
    }
    return this;
}

// ---------------------------------------------------------------------------
// stereo_room_t<T>::process -- one 256-sample block.
//
// 1. Per channel: pre-delay, occlusion, early network (taps1 -> diffuse1,
//    taps2 -> diffuse2 -> diffuse3 -> output taps).
// 2. Cross mix of the early taps into the tank feeds, scaled by mOutputGain.
// 3. The tank, one sample at a time. Every stage reads the other stages'
//    outputs from the previous sample (all outputs are latched first), so the
//    stages update as one parallel network. The four feed buffers are rewritten
//    in place with the tank outputs.
// 4. Output diffusion of the two wet outputs, rear delays of the two side feeds.
// 5. out[0]/out[1] = (1 - wet)*dry + wet*tank, out[3]/out[4] = 2*wet*rear.
// ---------------------------------------------------------------------------
template <typename T>
void stereo_room_t<T>::process(T *const *in, T *const *out)
{
    const int kBlock = 256;

    T *inL;
    T *inR;
    if (mInputMode == 1)
    {
        inL = in[0];
        inR = inL;
    }
    else if (mInputMode == 2)
    {
        inL = in[1];
        inR = inL;
    }
    else
    {
        inL = in[0];
        inR = in[1];
    }

    // Block buffers (outputs of the block filters are 257 long: the previous
    // tail first; the all-passes also write one sample past the block).
    T lafPreDelayed[kBlock + 2];
    T lafOccluded[kBlock + 2];
    T lafTaps1A[kBlock + 2];
    T lafTaps1BOut[kBlock + 2];
    T lafTaps2A[kBlock + 2];
    T lafTaps2AOut[kBlock + 2];
    T lafDiffusedA[kBlock + 2];
    T lafWetA[kBlock + 2];
    T lafSideA[kBlock + 4];
    T lafTaps1B[kBlock + 2];
    T lafTaps1BOutB[kBlock + 2];
    T lafTaps2B[kBlock + 2];
    T lafTaps2BOut[kBlock + 2];
    T lafDiffusedB[kBlock + 2];
    T lafWetB[kBlock + 2];
    T lafSideB[kBlock + 4];
    T lafMixA[kBlock];
    T lafMixB[kBlock];

    early_t &earlyA = maEarly[0];
    early_t &earlyB = maEarly[1];

    // Channel A front end.
    maPreDelay[0].preprocess(inL, lafPreDelayed);
    maOcclusion[0].preprocess(lafPreDelayed, lafOccluded, kBlock);
    earlyA.mTaps1.preprocess3(lafOccluded, lafTaps1A, lafTaps1BOut);
    earlyA.mDiffuse1.preprocess(lafTaps1A);
    earlyA.mTaps2.preprocess3(lafTaps1A, lafTaps2A, lafTaps2AOut);
    earlyA.mDiffuse2.preprocess(lafTaps2A);
    earlyA.mDiffuse3.preprocess(lafTaps2A, lafDiffusedA);
    earlyA.mOutput.preprocess(lafDiffusedA, lafWetA, &lafSideA[2]);

    // Channel B front end.
    maPreDelay[1].preprocess(inR, lafPreDelayed);
    maOcclusion[1].preprocess(lafPreDelayed, lafOccluded, kBlock);
    earlyB.mTaps1.preprocess3(lafOccluded, lafTaps1B, lafTaps1BOutB);
    earlyB.mDiffuse1.preprocess(lafTaps1B);
    earlyB.mTaps2.preprocess3(lafTaps1B, lafTaps2B, lafTaps2BOut);
    earlyB.mDiffuse2.preprocess(lafTaps2B);
    earlyB.mDiffuse3.preprocess(lafTaps2B, lafDiffusedB);
    earlyB.mOutput.preprocess(lafDiffusedB, lafWetB, &lafSideB[2]);

    // Cross mix into the tank feeds.
    const T gain = mOutputGain;
    for (int i = 0; i < kBlock; ++i)
    {
        lafMixA[i] = ((lafTaps1B[i] * earlyB.mTaps1Gain + lafTaps2A[i] * earlyA.mTaps2Gain) + lafTaps1BOut[i]) * gain;
        lafMixB[i] = ((lafTaps1A[i] * earlyA.mTaps1Gain + lafTaps2B[i] * earlyB.mTaps2Gain) + lafTaps1BOutB[i]) * gain;
    }

    // The tank.
    T *sideA = &lafSideA[2];
    T *sideB = &lafSideB[2];
    tank1_t &t1a = maTank1[0];
    tank1_t &t1b = maTank1[1];
    tank2_t &t2a = maTank2[0];
    tank2_t &t2b = maTank2[1];
    const T kDelayDrive = T(8.0);
    const T kOutputDrive = T(2.0);
    for (int n = 0; n < kBlock; ++n)
    {
        // Latch the previous sample's stage outputs.
        const T ap1   = t1a.mAllpass1.mState;
        const T ap2   = t1a.mAllpass2.mState;
        const T combA = t1a.mComb.mOut;
        const T filtA = t1a.mFilter.mOut;
        const T dampA = t1a.mDamp.mOut;
        const T dlyA  = t1a.mDelay.mStateA;
        const T fbA   = t1a.mDelay.mStateB;
        const T tapA  = t1a.mDelayTap.mOut;
        const T ap3   = t1b.mAllpass1.mState;
        const T ap4   = t1b.mAllpass2.mState;
        const T combB = t1b.mComb.mOut;
        const T filtB = t1b.mFilter.mOut;
        const T dampB = t1b.mDamp.mOut;
        const T dlyB  = t1b.mDelay.mStateA;
        const T fbB   = t1b.mDelay.mStateB;
        const T tapB  = t1b.mDelayTap.mOut;
        const T ap5   = t2a.mAllpass1.mState;
        const T apy1  = t2a.mAllpass2.mOut;
        const T ap6   = t2a.mAllpass3.mState;
        const T ap7   = t2a.mAllpass4.mState;
        const T dlyC  = t2a.mDelay.mStateA;
        const T fbC   = t2a.mDelay.mStateB;
        const T toneA = t2a.mTone.mOut;
        const T ap8   = t2b.mAllpass1.mState;
        const T apy2  = t2b.mAllpass2.mOut;
        const T ap9   = t2b.mAllpass3.mState;
        const T ap10  = t2b.mAllpass4.mState;
        const T dlyD  = t2b.mDelay.mStateA;
        const T fbD   = t2b.mDelay.mStateB;
        const T toneB = t2b.mTone.mOut;

        const T feedA  = lafWetA[n];
        const T feedB  = lafWetB[n];
        const T feedA2 = sideA[n];
        const T feedB2 = sideB[n];
        const T tapsA  = lafTaps2AOut[n];
        const T tapsB  = lafTaps2BOut[n];

        t1a.mAllpass1.tick(feedA + toneB + toneA);
        t1a.mAllpass2.tick(ap1);
        t1a.mComb.comb(tapsA + ap2);
        t1a.mFilter.tick(combA);
        t1a.mDamp.iir(filtA);
        t1a.mDelay.tick(dampA * kDelayDrive);
        t1a.mDelayTap.delay(dlyA);

        t1b.mAllpass1.tick(feedB + (toneB - toneA));
        t1b.mAllpass2.tick(ap3);
        t1b.mComb.comb(tapsB + ap4);
        t1b.mFilter.tick(combB);
        t1b.mDamp.iir(filtB);
        t1b.mDelay.tick(dampB * kDelayDrive);
        t1b.mDelayTap.delay(dlyB);

        t2a.mAllpass1.tick(feedB2 + tapB + tapA);
        t2a.mAllpass2.tick(feedA2 + ap5);
        t2a.mAllpass3.tick(apy1);
        t2a.mAllpass4.tick(feedA2 + ap6);
        t2a.mDelay.tick(tapsA + ap7);
        t2a.mTone.fir(dlyC);

        t2b.mAllpass1.tick((tapB - tapA) + feedA2);
        t2b.mAllpass2.tick(feedB2 + ap8);
        t2b.mAllpass3.tick(apy2);

        lafWetB[n] = ((lafMixB[n] + fbC) + fbB) * kOutputDrive;
        lafWetA[n] = ((lafMixA[n] + fbD) + fbA) * kOutputDrive;
        sideA[n]   = fbA;
        sideB[n]   = fbB;

        t2b.mAllpass4.tick(feedB2 + ap9);
        t2b.mDelay.tick(tapsB + ap10);
        t2b.mTone.fir(dlyD);
    }

    maOutDiffuse[0].preprocess(lafWetA);
    maOutDiffuse[1].preprocess(lafWetA);
    maOutDiffuse[2].preprocess(lafWetB);
    maOutDiffuse[3].preprocess(lafWetB);
    maRearDelay[0].preprocess(sideA, lafSideA);
    maRearDelay[1].preprocess(sideB, lafSideB);

    const T wet = mWetDryMix;
    const T dry = T(1.0) - wet;
    const T rear = wet + wet;
    T *outL = out[0];
    T *outR = out[1];
    T *outRearL = out[3];
    T *outRearR = out[4];
    for (int i = 0; i < kBlock; ++i)
    {
        outRearL[i] = rear * lafSideA[i];
        outL[i]     = dry * inL[i] + wet * lafWetA[i];
        outR[i]     = dry * inR[i] + wet * lafWetB[i];
        outRearR[i] = rear * lafSideB[i];
    }
}

// ---------------------------------------------------------------------------
// i3dl2_to_properties -- file-local helper that converts an I3DL2 parameter
// block into a stereo_room_t::properties_t (written through pOut2; pOut and the
// sample rate are not read). Levels in millibels become decibels, times in
// seconds become milliseconds (clamped below the table limits), the decay HF
// ratio selects the a8 / a10 table rows by log10 steps, diffusion selects the
// diffusion rows.
// ---------------------------------------------------------------------------
namespace
{
void i3dl2_to_properties(stereo_room_t<f32>::properties_t *pOut,
                         const i3dl2_reverb_t<f32> *pI3dl2,
                         stereo_room_t<f32>::properties_t *pOut2,
                         f32 fSampleRate)
{
    (void)pOut;
    (void)fSampleRate;
    static const s32 kiReflectionsDelayLimit = 300;
    static const s32 kiReverbDelayLimit = 85;
    static const s32 kiDefaultEarlyRow = 6;
    static const s32 kiDefaultOutRow = 27;
    static const s32 kiDefaultRearDelay = 5;

    stereo_room_t<f32>::properties_t &props = *pOut2;
    props.a12 = kiDefaultRearDelay;
    props.a2 = kiDefaultEarlyRow;
    props.a3 = kiDefaultEarlyRow;
    props.a4 = kiDefaultOutRow;
    props.a5 = kiDefaultOutRow;
    props.f20 = 100.0f;

    const f32 kHundredth = 0.01f;
    props.f14 = static_cast<f32>(pI3dl2->miRoom) * kHundredth;
    props.a9 = 4;
    props.a11 = 6;
    props.f15 = static_cast<f32>(pI3dl2->miRoomHF) * kHundredth;

    f32 decay;
    if (!(pI3dl2->mfDecayHFRatio < 1.0f))
    {
        props.a10 = 8;
        s32 step = static_cast<s32>(static_cast<f32>(log10(static_cast<double>(pI3dl2->mfDecayHFRatio))) * -4.0);
        if (step < -8)
            step = -8;
        if (step < 0)
            props.a8 = step + 8;
        else
            props.a8 = 8;
        decay = pI3dl2->mfDecayTime * pI3dl2->mfDecayHFRatio;
    }
    else
    {
        props.a8 = 8;
        s32 step = static_cast<s32>(static_cast<f32>(log10(static_cast<double>(pI3dl2->mfDecayHFRatio))) * 4.0);
        if (step < -8)
            step = -8;
        if (step < 0)
            props.a10 = step + 8;
        else
            props.a10 = 8;
        decay = pI3dl2->mfDecayTime;
    }
    props.f18 = decay;

    props.f16 = static_cast<f32>(pI3dl2->miReflections) * kHundredth;
    const f32 kSecondsToMs = 1000.0f;
    f32 reflectionsMs = pI3dl2->mfReflectionsDelay * kSecondsToMs;
    const f32 reflectionsLimit = static_cast<f32>(static_cast<s64>(kiReflectionsDelayLimit));
    if (!(reflectionsMs < reflectionsLimit))
        reflectionsMs = reflectionsLimit - 1.0f;
    if (!(reflectionsMs > 1.0f))
        reflectionsMs = 1.0f;
    props.a0 = static_cast<s32>(static_cast<s64>(reflectionsMs));

    props.f17 = static_cast<f32>(pI3dl2->miReverb) * kHundredth;
    f32 reverbMs = pI3dl2->mfReverbDelay * kSecondsToMs;
    const f32 reverbLimit = static_cast<f32>(static_cast<s64>(kiReverbDelayLimit));
    if (!(reverbMs < reverbLimit))
        reverbMs = reverbLimit - 1.0f;
    props.a1 = static_cast<s32>(static_cast<s64>(reverbMs));

    const s32 diffusionRow = static_cast<s32>(static_cast<s64>(pI3dl2->mfDiffusion * 0.15f));
    props.a6 = diffusionRow;
    props.a7 = diffusionRow;
    props.f19 = pI3dl2->mfDensity;
    props.f13 = pI3dl2->mfHFReference;
}
} // namespace

// ---------------------------------------------------------------------------
// stereo_room_3dl2_t<T>::stereo_room_3dl2_t -- seed the I3DL2 parameter block
// with the "generic" preset. The virtual base stereo_room_t<T> is constructed by
// the compiler-inserted most-derived path. The store order follows the asm.
// ---------------------------------------------------------------------------
template <typename T>
stereo_room_3dl2_t<T>::stereo_room_3dl2_t()
{
    mProps.mfRoomRolloffFactor = static_cast<T>(0.0);
    mProps.mfDecayTime         = static_cast<T>(1.0);
    mProps.miRoom              = -10000;
    mProps.miRoomHF            = 0;
    mProps.miReflections       = -10000;
    mProps.mfDecayHFRatio      = static_cast<T>(0.5);
    mProps.miReverb            = -10000;
    mProps.mfReflectionsDelay  = static_cast<T>(0.02);
    mProps.mfReverbDelay       = static_cast<T>(0.039999999);
    mProps.mfDiffusion         = static_cast<T>(100.0);
    mProps.mfDensity           = static_cast<T>(100.0);
    mProps.mfHFReference       = static_cast<T>(5000.0);
}

// ---------------------------------------------------------------------------
// stereo_room_3dl2_t<T>::set -- `index` selects which I3DL2 field to overwrite
// from `src` (0 = the whole 48-byte block, 1..12 = one field in I3DL2 order);
// any other index leaves the block unchanged. Then, regardless of index,
// re-derive the DSP parameter block (i3dl2_to_properties) and push it into the
// underlying room (stereo_room_t::properties_set).
// ---------------------------------------------------------------------------
template <typename T>
stereo_room_t<T> *stereo_room_3dl2_t<T>::set(int index, const i3dl2_reverb_t<T> *src)
{
    stereo_room_t<T> &room = *this;

    switch (index)
    {
        case 0:  memcpy(&mProps, src, 48);                              break;
        case 1:  mProps.miRoom             = src->miRoom;               break;
        case 2:  mProps.miRoomHF           = src->miRoomHF;             break;
        case 3:  mProps.mfRoomRolloffFactor = src->mfRoomRolloffFactor; break;
        case 4:  mProps.mfDecayTime        = src->mfDecayTime;          break;
        case 5:  mProps.mfDecayHFRatio     = src->mfDecayHFRatio;       break;
        case 6:  mProps.miReflections      = src->miReflections;        break;
        case 7:  mProps.mfReflectionsDelay = src->mfReflectionsDelay;   break;
        case 8:  mProps.miReverb           = src->miReverb;             break;
        case 9:  mProps.mfReverbDelay      = src->mfReverbDelay;        break;
        case 10: mProps.mfDiffusion        = src->mfDiffusion;          break;
        case 11: mProps.mfDensity          = src->mfDensity;            break;
        case 12: mProps.mfHFReference      = src->mfHFReference;        break;
        default:                                                        break;
    }

    typename stereo_room_t<T>::properties_t props;
    i3dl2_to_properties(&props, &mProps, &props, room.mSampleRate);
    return room.properties_set(props);
}

// ---------------------------------------------------------------------------
// Explicit instantiations -- one per <float,N> that the console image emits.
// ---------------------------------------------------------------------------

// allpass_t<float,{128,256,512}>::preprocess; the 1024/2048 all-passes run
// inline in the reverb tank only.
template struct allpass_t<f32, 128>;
template struct allpass_t<f32, 256>;
template struct allpass_t<f32, 512>;

// delay_t<float,{128,256,512,1024,2048,4096,16384}>::reset
template struct delay_t<f32, 128>;
template struct delay_t<f32, 256>;
template struct delay_t<f32, 512>;
template struct delay_t<f32, 1024>;
template struct delay_t<f32, 2048>;
template struct delay_t<f32, 4096>;
template struct delay_t<f32, 16384>;

// threetap_t<float,{512,2048}>::preprocess3
template struct threetap_t<f32, 512>;
template struct threetap_t<f32, 2048>;

// twotap_t<float,1024>::preprocess
template struct twotap_t<f32, 1024>;

// vardelay_t<float,{256,16384}>::preprocess
template struct vardelay_t<f32, 256>;
template struct vardelay_t<f32, 16384>;

// occlusion_t<float,2>::preprocess / recalculate
template struct occlusion_t<f32, 2>;

// stereo_room_t<float> -- properties_t ctor, room ctor, properties_set, process,
// input_mode_set, wet_dry_mix_set and the scalar deleting destructor.
template struct stereo_room_t<f32>;

// stereo_room_3dl2_t<float> -- ctor, set, and the compiler-emitted scalar
// deleting destructor.
template struct stereo_room_3dl2_t<f32>;

} // namespace princeton_digital
