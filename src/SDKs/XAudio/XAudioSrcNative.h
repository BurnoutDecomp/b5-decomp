#pragma once

// ===========================================================================
// XAUDIO::SRC::Native -- the "native" (1:1, no-resample) sample-rate-conversion
// pass used by the XAudio middleware in BURNOUT_X360_ARTIST.XEX. This header is
// the canonical OWNING home for the family:
//
//     XAUDIO::SRC::Native::Process<SHORTLE,1>   @ 0x82977CF8
//     XAUDIO::SRC::Native::Process<SHORTLE,2>   @ 0x82977E30
//     XAUDIO::SRC::Native::Process<SHORTLE,4>   @ 0x82977F90
//     XAUDIO::SRC::Native::Process<SHORTLE,6>   @ 0x82978148
//     XAUDIO::SRC::Native::Process<float,0>     @ 0x82976740
//     XAUDIO::SRC::Native::Process<float,1>     @ 0x82976858
//     XAUDIO::SRC::Native::Process<float,2>     @ 0x82976968
//     XAUDIO::SRC::Native::Process<float,6>     @ 0x82976BC0
//     XAUDIO::SRC::Native::Process<short,0>     @ 0x82976D10
//     XAUDIO::SRC::Native::Process<short,1>     @ 0x82976E50
//     XAUDIO::SRC::Native::Process<short,2>     @ 0x82976F80
//     XAUDIO::SRC::Native::Process<short,4>     @ 0x829770D8
//     XAUDIO::SRC::Native::Process<short,6>     @ 0x82977278
//     XAUDIO::SRC::Native::Process<unsigned char,0> @ 0x82977468
//     XAUDIO::SRC::Native::Process<unsigned char,1> @ 0x82977590
//     XAUDIO::SRC::Native::Process<unsigned char,2> @ 0x829776C0
//     XAUDIO::SRC::Native::Process<unsigned char,4> @ 0x82977818
//     XAUDIO::SRC::Native::Process<unsigned char,6> @ 0x829779B8
//
// There is no reference source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 asm of these 18 template instantiations
// (0 VMX -- pure scalar). `XAUDIO` is an external middleware boundary, so its
// API identifiers (the `SRC::Native::Process` name, the `XAUDIOSRCHDR` header,
// and the `SHORTLE` sample tag) are preserved verbatim per the naming
// convention; reconstructed members/locals use the project Hungarian.
//
// WHAT IT DOES (one template over <TSample, CHANNELS>):
//   Reads interleaved PCM frames of `TSample` out of the source span, converts
//   each sample to normalised f32, applies a per-frame-ramped volume, and
//   scatters the channels into a PLANAR f32 output (one 256-frame plane per
//   channel). "native" == the destination rate equals the source rate, so it
//   is a straight frame-for-frame copy (no interpolation). CHANNELS==0 is the
//   generic variant that loops over the runtime channel count; CHANNELS>0 is
//   the fixed-N variant (the X360 build hand-unrolls the inner loop and adds a
//   source prefetch pass). All arithmetic is single precision, matching the
//   fmuls/fadds/fdivs/frsp the image emits.
// ===========================================================================

#include "SDKs/XAudio/XAudioSrcCommon.h"

namespace XAUDIO
{
namespace SRC
{

// ---------------------------------------------------------------------------
// NativeStorage -- maps a sample-format TAG to the integral/float word actually
// read out of the interleaved source. Every tag reads as itself EXCEPT SHORTLE,
// which is an empty policy tag: its samples live in the stream as raw 16-bit
// little-endian words, so the storage type is u16. This drives the source
// pointer stride to `2 * channels` bytes for SHORTLE, matching the image's
// `lhz r,0(r11); addi r11,r11,2` step (whereas sizeof(SHORTLE)==1).
// ---------------------------------------------------------------------------
template <typename TSample>
struct NativeStorage
{
    typedef TSample Type;
};

template <>
struct NativeStorage<SHORTLE>
{
    typedef u16 Type;
};

// ---------------------------------------------------------------------------
// Reconstructed conversion constants (mathematical normalisation factors, not
// data tables). 1/32768 folds a signed 16-bit sample to [-1,1); 128/(1/128)
// centre-and-scale an unsigned 8-bit sample.
// ---------------------------------------------------------------------------
const s32 KI_NativeDestChannelStride = 256; // planar output stride (frames/plane)
const s32 KI_NativeCacheLineBytes    = 128; // source-prefetch granularity
const f32 KF_NativeShortScale        = 1.0f / 32768.0f;
const f32 KF_NativeByteBias          = 128.0f;
const f32 KF_NativeByteScale         = 1.0f / 128.0f;

// ---------------------------------------------------------------------------
// DecodeNativeSample -- convert one source sample of each supported type to a
// normalised f32 in [-1,1]. One overload per <TSample>; the template selects by
// ordinary/ADL lookup so a single Process() body serves every instantiation.
// ---------------------------------------------------------------------------

// float source is already normalised -- passthrough.
inline f32 DecodeNativeSample(f32 lfSample)
{
    return lfSample;
}

// native (big-endian) signed 16-bit -> [-1,1).
inline f32 DecodeNativeSample(s16 liSample)
{
    return static_cast<f32>(liSample) * KF_NativeShortScale;
}

// unsigned 8-bit, biased at 128 -> [-1,1).
inline f32 DecodeNativeSample(u8 luSample)
{
    return (static_cast<f32>(luSample) - KF_NativeByteBias) * KF_NativeByteScale;
}

// ---------------------------------------------------------------------------
// DecodeNativeSampleAs<TSample> -- decode one raw storage word (of type
// NativeStorage<TSample>::Type) to a normalised f32. The primary template
// forwards to the by-value overloads above (TSample == storage for the
// float/short/u8 paths); the SHORTLE specialisation takes a raw little-endian
// u16 word, byte-swaps + sign-extends it, then normalises -- SHORTLE is an
// empty tag and is never dereferenced. `TSample` cannot be deduced from the
// (non-deduced) parameter type, so Process() names it explicitly.
// ---------------------------------------------------------------------------
template <typename TSample>
inline f32 DecodeNativeSampleAs(typename NativeStorage<TSample>::Type value)
{
    return DecodeNativeSample(value);
}

template <>
inline f32 DecodeNativeSampleAs<SHORTLE>(u16 uRaw)
{
    const s16 liSwapped = static_cast<s16>((uRaw << 8) | (uRaw >> 8));
    return static_cast<f32>(liSwapped) * KF_NativeShortScale;
}

// ---------------------------------------------------------------------------
// NativePrefetch -- source cache-line touch. On the X360 this was a `dcbt`
// data-cache-block-touch hint; it has no observable effect on results, so the
// host build lowers it to a no-op while keeping the loop that drives it (the
// loop is real control flow present in the image for the CHANNELS>0 variants).
// ---------------------------------------------------------------------------
inline void NativePrefetch(const void* lpAddress)
{
    (void)lpAddress;
}

// ---------------------------------------------------------------------------
// Native -- static holder for the SRC "native" conversion pass. No instances,
// no vtable: it is a utility scope exactly as the image emits it (all members
// are static template functions).
// ---------------------------------------------------------------------------
struct Native
{
    // Convert min(source-available, dest-available) frames 1:1 from the
    // interleaved `TSample` source into the planar f32 destination, ramping the
    // volume across the block, then write the advanced cursors and volume back
    // into the header. CHANNELS==0 loops over the runtime channel count;
    // CHANNELS>0 fixes the channel count at compile time and prefetches source.
    template <typename TSample, int CHANNELS>
    static void Process(XAUDIOSRCHDR* pHdr)
    {
        // The raw word actually read from the source stream. Identical to
        // TSample for the float/short/u8 paths; u16 for the empty SHORTLE tag,
        // so the source stride is `2 * channels` bytes (the image's lhz step).
        typedef typename NativeStorage<TSample>::Type Storage;

        // Fixed-N variants pin the channel count; the generic variant reads it
        // from the header.
        const s32 liChannels = (CHANNELS != 0) ? CHANNELS : pHdr->muChannels;

        const s32 liSrcPos   = pHdr->miSourcePos;
        const s32 liDstPos    = pHdr->miDestPos;
        const s32 liSrcAvail  = pHdr->miSourceFrames - liSrcPos;
        const s32 liDstAvail  = pHdr->miDestFrames - liDstPos;

        // The initial source pointer always steps by the runtime channel count
        // (image: `sizeof(Storage) * muChannels * pos`); the destination steps
        // by whole frames into the planar buffer.
        const Storage* lpSrc =
            static_cast<const Storage*>(pHdr->mpSource) + pHdr->muChannels * liSrcPos;
        f32* lpDst = pHdr->mpDest + liDstPos;

        // Process only as many frames as both sides can supply.
        s32 liFrames = (liSrcAvail >= liDstAvail) ? liDstAvail : liSrcAvail;

        // Fixed-N variants prefetch the source span one cache line at a time.
        if (CHANNELS != 0)
        {
            const s32 liSpanBytes =
                static_cast<s32>(sizeof(Storage)) * CHANNELS * liFrames;
            const s32 liLines =
                (liSpanBytes + KI_NativeCacheLineBytes - 1) / KI_NativeCacheLineBytes;
            for (s32 liLine = 0; liLine < liLines; ++liLine)
            {
                NativePrefetch(reinterpret_cast<const u8*>(lpSrc) +
                               liLine * KI_NativeCacheLineBytes);
            }
        }

        // Volume ramps linearly across the destination-available span.
        f32 lfVol = pHdr->mfVolCurrent;
        const f32 lfVolStep =
            (pHdr->mfVolTarget - pHdr->mfVolCurrent) / static_cast<f32>(liDstAvail);

        // Frame loop -- the image emits an unconditional do/while; real callers
        // always pass at least one frame.
        do
        {
            for (s32 liCh = 0; liCh < liChannels; ++liCh)
            {
                lpDst[liCh * KI_NativeDestChannelStride] =
                    DecodeNativeSampleAs<TSample>(lpSrc[liCh]) * lfVol;
            }
            lpSrc += liChannels;
            ++lpDst;
            lfVol += lfVolStep;
        }
        while (--liFrames != 0);

        // Write back the advanced source cursor (in frames, clamped to the
        // total), the ramped volume, and the advanced destination cursor.
        const s32 liSrcStride = static_cast<s32>(sizeof(Storage)) * pHdr->muChannels;
        s32 liSrcConsumed = static_cast<s32>(
            reinterpret_cast<const u8*>(lpSrc) -
            static_cast<const u8*>(pHdr->mpSource)) / liSrcStride;
        if (liSrcConsumed >= pHdr->miSourceFrames)
            liSrcConsumed = pHdr->miSourceFrames;

        s32 liDstWritten = static_cast<s32>(lpDst - pHdr->mpDest);
        if (liDstWritten > pHdr->miDestFrames)
            liDstWritten = pHdr->miDestFrames;

        pHdr->miSourcePos  = liSrcConsumed;
        pHdr->mfVolCurrent = lfVol;
        pHdr->miDestPos    = liDstWritten;
    }
};

} // namespace SRC
} // namespace XAUDIO
