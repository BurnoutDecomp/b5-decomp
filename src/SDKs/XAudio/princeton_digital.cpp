// princeton_digital reverb DSP primitives -- generic definitions plus the
// explicit per-N instantiations the X360 build emits. No prior source exists
// for this TU; bodies are reconstructed from the X360 pseudocode of each
// instance member at the @addresses noted in princeton_digital.h.

#include "SDKs/XAudio/princeton_digital.h"

#include <string.h>   // memcpy (whole-block I3DL2 parameter copy in set)

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
// i3dl2_to_properties -- file-local (anonymous-namespace) helper that converts
// an I3DL2 parameter block into a stereo_room_t::properties_t, given the room's
// sample rate. Its body is NOT recovered in this dossier (no pseudocode/asm was
// provided for `anonymous namespace'::i3dl2_to_properties); it is a callee to be
// reconstructed in its own pass, declared here only so stereo_room_3dl2_t::set
// can forward into it faithfully. Signature matches the X360 call site
// (@0x829674AC): (out, i3dl2, out2, sampleRate) with out == out2 (in place).
// ---------------------------------------------------------------------------
namespace
{
void i3dl2_to_properties(stereo_room_t<f32>::properties_t *pOut,
                         const i3dl2_reverb_t<f32> *pI3dl2,
                         stereo_room_t<f32>::properties_t *pOut2,
                         f32 fSampleRate);
} // namespace

// ---------------------------------------------------------------------------
// stereo_room_3dl2_t<T>::stereo_room_3dl2_t -- @0x829674D8. Seed the I3DL2
// parameter block with the "generic" preset. The virtual base stereo_room_t<T>
// is constructed by the compiler-inserted most-derived path (the `if (a2)`
// guard + the vbtable/vftable stores the asm performs before this body). The
// store order below follows the asm exactly.
// ---------------------------------------------------------------------------
template <typename T>
stereo_room_3dl2_t<T>::stereo_room_3dl2_t()
{
    mProps.mfRoomRolloffFactor = static_cast<T>(0.0);        // +12
    mProps.mfDecayTime         = static_cast<T>(1.0);        // +16
    mProps.miRoom              = -10000;                     // +4
    mProps.miRoomHF            = 0;                          // +8
    mProps.miReflections       = -10000;                    // +24
    mProps.mfDecayHFRatio      = static_cast<T>(0.5);        // +20
    mProps.miReverb            = -10000;                     // +32
    mProps.mfReflectionsDelay  = static_cast<T>(0.02);       // +28
    mProps.mfReverbDelay       = static_cast<T>(0.039999999);// +36
    mProps.mfDiffusion         = static_cast<T>(100.0);      // +40
    mProps.mfDensity           = static_cast<T>(100.0);      // +44
    mProps.mfHFReference       = static_cast<T>(5000.0);     // +48
}

// ---------------------------------------------------------------------------
// stereo_room_3dl2_t<T>::set -- @0x82967380. `index` selects which I3DL2 field
// to overwrite from `src` (0 = the whole 48-byte block, 1..12 = one field in
// I3DL2 order); any other index leaves the block unchanged. Then, regardless of
// index, re-derive the DSP parameter block (i3dl2_to_properties) and push it
// into the underlying room (stereo_room_t::properties_set). The jump-table
// switch is preserved store-for-store from the asm.
// ---------------------------------------------------------------------------
template <typename T>
stereo_room_t<T> *stereo_room_3dl2_t<T>::set(int index, const i3dl2_reverb_t<T> *src)
{
    stereo_room_t<T> &room = *this;   // virtual-base subobject (asm: this + vbtable[1])

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

    typename stereo_room_t<T>::properties_t props;   // v6: default-constructed
    i3dl2_to_properties(&props, &mProps, &props, room.mSampleRate);
    return room.properties_set(props);
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

// stereo_room_3dl2_t<float> -- ctor @0x829674D8, set @0x82967380, and the
// compiler-emitted scalar deleting destructor @0x82962B40.
template struct stereo_room_3dl2_t<f32>;

} // namespace princeton_digital
