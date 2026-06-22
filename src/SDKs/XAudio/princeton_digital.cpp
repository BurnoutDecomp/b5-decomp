// princeton_digital reverb DSP primitives -- generic definitions plus the
// explicit per-N instantiations the X360 build emits. No prior source exists
// for this TU; bodies are reconstructed from the X360 pseudocode of each
// instance member at the @addresses noted in princeton_digital.h.

#include "SDKs/XAudio/princeton_digital.h"

namespace princeton_digital
{

// ---------------------------------------------------------------------------
// Prime table for nearest_prime. table[k] is the k-th prime (1-based), with
// table[0] == 1 as the sentinel used by the a1 <= 1 early-out. table[1229] is
// 9973 (the 1229th prime), which is the upper clamp value. The X360 build holds
// this as a fixed rodata array (dword_8210A190); since it is the deterministic
// ascending-prime sequence the algorithm itself defines, it is generated at
// compile time here rather than fabricated.
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

// @ 0x8296C2B0 -- binary search for the tabulated prime nearest `value`.
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
// vardelay_t<T,N>::preprocess -- modulated delay line with an old->new tap
// crossfade (mMix ramps by 0.0002/sample).
// *** APPROXIMATE -- NOT store-for-store; the vardelay TUs are BLOCKED. ***
// The X360 build (@0x82960B08 / 0x8295FA78) is a 4-wide-unrolled DUAL code path
// selected on round(mMix*10000)>>1 >= 0x100, ramps the mix BOTH up (+0.0002) and
// down (-0.0002), and runs a tap-rotation state machine at the end of the slow
// path (mTapTarget@+16 -> mTapNew@+8, mTapNew@+8 -> mTapNew2@+12, mMix reset to
// 0/1). This body reconstructs only the single-tap up-crossfade and is therefore
// behaviour-divergent; a faithful reconstruction needs that full 4-wide VMX
// dual-path + tap-rotation machine. Both vardelay instances are work-blocked
// pending that. Kept compilable so the family header gates.
// ---------------------------------------------------------------------------
template <typename T, int N>
T *vardelay_t<T, N>::preprocess(T *in, T *out)
{
    const int kBlock = 256;
    const T kRamp = static_cast<T>(0.00019999999);

    out[0] = mState;
    T *o = out + 1;

    s32 tapOld = mTapOld;
    const s32 deltaNew = mTapNew - mTapOld;     // v101-style tap delta (new vs base)
    const s32 deltaNew2 = mTapOld - mTapNew2;   // secondary tap delta
    T mix = mMix;

    for (int i = 0; i < kBlock; ++i)
    {
        const T x = in[i];
        const T sampleA = static_cast<T>(maBuffer[tapOld & (N - 1)] * (T(1) - mix));
        const T sampleB = maBuffer[(deltaNew2 + tapOld) & (N - 1)];
        maBuffer[(mTapOld + i) & (N - 1)] = x; // write incoming sample into the line
        *o = static_cast<T>(sampleB * mix) + sampleA;
        mix = static_cast<T>(mix + kRamp);
        ++o;
        ++tapOld;
    }
    (void)deltaNew;

    mMix = mix;
    mState = out[kBlock];
    mTapOld = mTapNew;
    return in;
}

// ---------------------------------------------------------------------------
// Explicit instantiations -- one per <float,N> that the X360 image emits.
// ---------------------------------------------------------------------------

// allpass_t<float,{128,256,512}>::preprocess
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

// vardelay_t<float,{256,16384}>::preprocess
template struct vardelay_t<f32, 256>;
template struct vardelay_t<f32, 16384>;

// occlusion_t<float,2>::preprocess
template struct occlusion_t<f32, 2>;

// stereo_room_t<float>::properties_t::properties_t
template struct stereo_room_t<f32>;

} // namespace princeton_digital
