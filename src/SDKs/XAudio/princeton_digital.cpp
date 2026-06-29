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
// vardelay_t<T,N>::preprocess -- modulated delay line that linearly crossfades
// between two tap lengths, ramping the mix by 0.0002/sample.
//
// Store-for-store reconstruction of the X360 dual code path @0x82960B08
// (<float,256>) / @0x8295FA78 (<float,16384>). The block is 256 samples. Each
// output sample blends two ring taps:
//   out = maBuffer[(w - mTapNew2)&(N-1)] * mixDown
//       + maBuffer[(w - mTapNew )&(N-1)] * mixUp
// where, per sample, mixDown ramps -0.0002 (from mMix) and mixUp ramps +0.0002
// (from 1 - mMix); w is the ring write index, advancing as the incoming sample
// is stored at maBuffer[w] (raw, unmasked pointer -- exactly as the asm). The
// final write index advances by one full block (& 0x00FFFFFF) and mState carries
// out[256] to the next block.
//
// The selector v5 = trunc(mMix*10000) >> 1 (~mMix*5000) is the number of samples
// left in the CURRENT crossfade:
//   * v5 >= 256 (FAST path): the whole block stays inside one crossfade -- no tap
//     rotation; mMix is left mid-ramp.
//   * v5 <  256 (SLOW path): the current crossfade completes at sample v5, then a
//     tap rotation runs (mTapNew2 <- mTapNew, mTapNew <- mTapTarget; mMix reset to
//     0.0 if the target equals the old new-tap, else 1.0) and a second crossfade
//     runs over the remaining 256-v5 samples with the rotated taps.
// The X360 build unrolls each phase 4-wide; the unrolling is a pure perf
// transform, so the per-sample kernel below is the store-for-store equivalent.
// ---------------------------------------------------------------------------
namespace
{
// One crossfade phase: blend the two taps for `count` samples, writing the
// incoming samples into the ring at the (raw, unmasked) write pointer. Advances
// `writeIndex`, `in`, `out`, and the running mix coefficients in place so the
// caller can chain phases. Mirrors the slow-path remainder kernels at
// loc_82960CDC / loc_82960EEC (and the fast path at loc_82960FA4), with the
// 4-wide head loops folded back into the scalar form they unroll.
template <typename T, int N>
void vardelay_crossfade(T *buffer, s32 &writeIndex, s32 tapNew, s32 tapNew2,
                        const T *&in, T *&out, T &mixDown, T &mixUp, int count)
{
    const T kRamp = static_cast<T>(0.00019999999);

    const s32 readNew = writeIndex - tapNew;    // old-tap read base (v43/v90 = w - mTapNew)
    const s32 readNew2 = writeIndex - tapNew2;  // new-tap read base (= w - mTapNew2)
    T *bufW = &buffer[writeIndex];               // raw write pointer (unmasked, as asm)

    for (int i = 0; i < count; ++i)
    {
        const T sampleA = static_cast<T>(buffer[(readNew + i) & (N - 1)] * mixUp);
        const T sampleB = buffer[(readNew2 + i) & (N - 1)];
        *bufW++ = *in++;                                         // store incoming sample
        *out++ = static_cast<T>(sampleB * mixDown) + sampleA;   // blended output
        mixUp = static_cast<T>(mixUp + kRamp);                  // up-ramp  (+0.0002)
        mixDown = static_cast<T>(mixDown - kRamp);              // down-ramp (-0.0002)
    }
    writeIndex += count;
}
} // namespace

template <typename T, int N>
T *vardelay_t<T, N>::preprocess(T *in, T *out)
{
    const int kBlock = 256;

    out[0] = mState;                 // carry the previous block's tail (asm: *a3 = mState)
    const T *src = in;               // a2
    T *dst = out + 1;                // a3 + 1

    s32 writeIndex = mTapOld;        // v4 -- ring write index, advances by kBlock total
    // v5 = samples remaining in the current crossfade (~mMix * 5000).
    const u32 remain = static_cast<u32>(static_cast<long long>(mMix * static_cast<T>(10000.0))) >> 1;

    if (remain >= static_cast<u32>(kBlock))
    {
        // FAST path: the entire block lies within one crossfade -- no rotation.
        T mixDown = mMix;                       // v104 (new tap), ramps -0.0002
        T mixUp = static_cast<T>(T(1) - mMix);  // v102 (old tap), ramps +0.0002
        vardelay_crossfade<T, N>(maBuffer, writeIndex, mTapNew, mTapNew2,
                                 src, dst, mixDown, mixUp, kBlock);
        mMix = mixDown;                          // leave the mix mid-ramp
    }
    else
    {
        // SLOW path: finish the current crossfade, rotate taps, then start the next.
        const int n1 = static_cast<int>(remain);   // samples left in the current fade
        {
            T mixDown = mMix;
            T mixUp = static_cast<T>(T(1) - mMix);
            vardelay_crossfade<T, N>(maBuffer, writeIndex, mTapNew, mTapNew2,
                                     src, dst, mixDown, mixUp, n1);
            mMix = mixDown;
        }

        // Tap rotation (loc_82960D2C): shift new->old, target->new, and seed the
        // fresh mix. mMix = 0.0 when the target matches the outgoing new tap (no
        // further fade pending), else 1.0 to begin a full old->new crossfade.
        const s32 target = mTapTarget;          // v51 = *(this+16)
        const s32 prevNew = mTapNew;            // v52 = *(this+8)
        mMix = (target == prevNew) ? T(0) : T(1);
        mTapNew2 = prevNew;                     // *(this+12) = old mTapNew
        mTapNew = target;                       // *(this+8)  = mTapTarget

        // Second crossfade over the remaining samples with the rotated taps.
        const int n2 = kBlock - n1;
        {
            T mixDown = mMix;                            // v87
            T mixUp = static_cast<T>(T(1) - mMix);       // v54
            vardelay_crossfade<T, N>(maBuffer, writeIndex, mTapNew, mTapNew2,
                                     src, dst, mixDown, mixUp, n2);
            mMix = mixDown;
        }
    }

    mState = out[kBlock];                              // *(this+20) = a3[256]
    // Both paths advance writeIndex by exactly one full block; the asm stores the
    // index masked to 24 bits (clrlwi r11, r10, 24).
    mTapOld = static_cast<s32>(static_cast<u32>(writeIndex) & 0x00FFFFFFu);
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
