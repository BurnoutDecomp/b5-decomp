#include "SDKs/XAudio/XAudioSrcDouble.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ===========================================================================
// XAUDIO::SRC::Double::Process<T, CHANNELS> -- 2x sample-rate converter.
//
// One templated scalar kernel reconstructed from the 18 X360 instantiations
// (they differ only by input format `T` and interleave `CHANNELS`; the PPC
// versions unroll the channel loop and strength-reduce the format math -- both
// are de-optimised back to structured C++ here).
//
// Algorithm, per input frame `f` (count = min(inputAvailable, outputRoom)):
//   for each channel c:
//     s    = Decode(input[c])                 // -> normalised f32
//     prev = history[c]
//     out_c[0] = 0.5 * (s + prev) * phase     // interpolated (doubled) sample
//     out_c[1] = (phase + phaseInc) * s       // scaled original sample
//     history[c] = s
//   phase += 2 * phaseInc                      // linear glide across the block
// Output is planar: channel c occupies the 256-float bank at out + c*256, and
// the descriptor's output cursor advances 2 floats/frame (the block is 2x).
// The glide increment is calibrated over the *full* output room, not the
// clamped frame count, matching the X360 kernel exactly.
// ===========================================================================

namespace XAUDIO
{
namespace SRC
{

namespace
{

// Normalisation constants (exact powers of two, as in the X360 rodata).
const f32 KF_S16_TO_UNIT = 1.0f / 32768.0f;   // 0.000030517578
const f32 KF_U8_TO_UNIT  = 1.0f / 128.0f;     // 0.0078125
const f32 KF_UNIT_TO_S16 = 32768.0f;
const f32 KF_U8_BIAS     = 128.0f;

inline u16 ByteSwap16(u16 uValue)
{
    return static_cast<u16>((uValue << 8) | (uValue >> 8));
}

// Per-format decode (raw -> normalised f32) / encode (normalised f32 -> raw,
// with the original clamp rails) and the history-slot member each format
// persists between calls. `KNeedsRequant` is false only for the native float
// path, which keeps its history as a plain f32 (no on-entry/on-exit round-trip).
template <typename T>
struct DoubleFormat;

template <>
struct DoubleFormat<float>
{
    typedef f32 Storage;
    static const bool KNeedsRequant = false;

    static f32 Decode(Storage rawSample) { return rawSample; }
    static Storage Encode(f32 fSample) { return fSample; }
    static Storage LoadHistory(const SrcHistorySample& history) { return history.mFloat; }
    static void StoreHistory(SrcHistorySample& history, Storage rawSample) { history.mFloat = rawSample; }
};

template <>
struct DoubleFormat<short>
{
    typedef s16 Storage;
    static const bool KNeedsRequant = true;

    static f32 Decode(Storage rawSample)
    {
        return static_cast<f32>(rawSample) * KF_S16_TO_UNIT;
    }
    static Storage Encode(f32 fSample)
    {
        s32 iSample = static_cast<s32>(fSample * KF_UNIT_TO_S16);
        if (iSample < 32767)
        {
            if (iSample > -32768)
            {
                return static_cast<s16>(iSample);
            }
            return static_cast<s16>(-32738);  // faithful low-rail quirk of the X360 kernel
        }
        return static_cast<s16>(32767);
    }
    static Storage LoadHistory(const SrcHistorySample& history) { return history.mShort; }
    static void StoreHistory(SrcHistorySample& history, Storage rawSample) { history.mShort = rawSample; }
};

template <>
struct DoubleFormat<SHORTLE>
{
    typedef u16 Storage;
    static const bool KNeedsRequant = true;

    static f32 Decode(Storage rawSample)
    {
        s16 iSwapped = static_cast<s16>(ByteSwap16(rawSample));
        return static_cast<f32>(iSwapped) * KF_S16_TO_UNIT;
    }
    static Storage Encode(f32 fSample)
    {
        s32 iSample = static_cast<s32>(fSample * KF_UNIT_TO_S16);
        if (iSample < 32767)
        {
            if (iSample > -32768)
            {
                return ByteSwap16(static_cast<u16>(static_cast<s16>(iSample)));
            }
            return static_cast<u16>(0x0080);  // byte-swapped low rail (0x8000)
        }
        return static_cast<u16>(0xFF7F);       // byte-swapped high rail (0x7FFF)
    }
    static Storage LoadHistory(const SrcHistorySample& history) { return history.mShortLE; }
    static void StoreHistory(SrcHistorySample& history, Storage rawSample) { history.mShortLE = rawSample; }
};

template <>
struct DoubleFormat<unsigned char>
{
    typedef u8 Storage;
    static const bool KNeedsRequant = true;

    static f32 Decode(Storage rawSample)
    {
        return (static_cast<f32>(rawSample) - KF_U8_BIAS) * KF_U8_TO_UNIT;
    }
    static Storage Encode(f32 fSample)
    {
        s32 iSample = static_cast<s32>(KF_U8_BIAS - fSample * -KF_U8_BIAS);
        if (iSample < 255)
        {
            if (iSample > 0)
            {
                return static_cast<u8>(iSample);
            }
            return static_cast<u8>(0);
        }
        return static_cast<u8>(0xFF);
    }
    static Storage LoadHistory(const SrcHistorySample& history) { return history.mByte; }
    static void StoreHistory(SrcHistorySample& history, Storage rawSample) { history.mByte = rawSample; }
};

} // anonymous namespace

template <typename T, int CHANNELS>
void Double::Process(XAUDIOSRCHDR* pHdr)
{
    typedef DoubleFormat<T> Format;
    typedef typename Format::Storage Storage;

    // Fixed interleave (CHANNELS>0) or the runtime channel count (CHANNELS==0).
    const s32 liChannels = (CHANNELS != 0) ? CHANNELS : static_cast<s32>(pHdr->muChannels);

    const s32 liOutputPos = pHdr->miDestPos;
    const s32 liInputPos  = pHdr->miSourcePos;

    // Output room is halved: the block doubles the rate (2 floats per frame).
    const s32 liOutputRoom = (pHdr->miDestFrames - liOutputPos) / 2;
    const s32 liInputAvail = pHdr->miSourceFrames - liInputPos;
    const s32 liFrameCount = (liInputAvail >= liOutputRoom) ? liOutputRoom : liInputAvail;

    const Storage* lpInputBase = static_cast<const Storage*>(pHdr->mpSource);
    const Storage* lpInput     = lpInputBase + liChannels * liInputPos;
    f32* lpOutputBase = static_cast<f32*>(pHdr->mpDest);
    f32* lpOutput     = lpOutputBase + liOutputPos;

    // Glide increment across the full output room (not the clamped count).
    const f64 lfPhaseRange = static_cast<f64>(pHdr->mfVolTarget) - static_cast<f64>(pHdr->mfVolCurrent);
    const f64 lfPhaseInc   = lfPhaseRange / static_cast<f64>(liOutputRoom);
    const f64 lfPhaseStep  = lfPhaseInc * 2.0;
    f64 lfPhase = static_cast<f64>(pHdr->mfVolCurrent);

    // Integer formats persist history in raw form; decode it back to normalised
    // floats before the run. The native-float path skips this round-trip.
    if (Format::KNeedsRequant)
    {
        for (s32 c = 0; c < liChannels; ++c)
        {
            pHdr->mScratch[c].mFloat = Format::Decode(Format::LoadHistory(pHdr->mScratch[c]));
        }
    }

    for (s32 f = 0; f < liFrameCount; ++f)
    {
        for (s32 c = 0; c < liChannels; ++c)
        {
            const f32 lfSample = Format::Decode(lpInput[c]);
            const f32 lfPrev   = pHdr->mScratch[c].mFloat;
            pHdr->mScratch[c].mFloat = lfSample;

            f32* lpChannel = lpOutput + c * 256;
            lpChannel[0] = static_cast<f32>((static_cast<f64>(lfSample) + static_cast<f64>(lfPrev)) * lfPhase * 0.5);
            lpChannel[1] = static_cast<f32>((lfPhase + lfPhaseInc) * static_cast<f64>(lfSample));
        }

        lpInput  += liChannels;
        lpOutput += 2;
        lfPhase  += lfPhaseStep;
    }

    // Advance the descriptor cursors, clamped to the buffer bounds.
    CGS_ASSERT(liChannels != 0, "XAUDIO::SRC::Double::Process: channel count must be non-zero");
    s32 liNewInputPos = static_cast<s32>(lpInput - lpInputBase) / liChannels;
    if (liNewInputPos >= pHdr->miSourceFrames)
    {
        liNewInputPos = pHdr->miSourceFrames;
    }
    s32 liNewOutputPos = static_cast<s32>(lpOutput - lpOutputBase);
    if (liNewOutputPos >= pHdr->miDestFrames)
    {
        liNewOutputPos = pHdr->miDestFrames;
    }
    pHdr->miSourcePos  = liNewInputPos;
    pHdr->miDestPos = liNewOutputPos;
    pHdr->mfVolCurrent     = static_cast<f32>(lfPhase);

    // Re-quantise the history back to the raw input format for the next call.
    if (Format::KNeedsRequant)
    {
        for (s32 c = 0; c < liChannels; ++c)
        {
            Format::StoreHistory(pHdr->mScratch[c], Format::Encode(pHdr->mScratch[c].mFloat));
        }
    }
}

// ---------------------------------------------------------------------------
// The 18 explicit instantiations attested in the X360 image.
// ---------------------------------------------------------------------------
template void Double::Process<SHORTLE, 0>(XAUDIOSRCHDR*);
template void Double::Process<SHORTLE, 1>(XAUDIOSRCHDR*);
template void Double::Process<SHORTLE, 2>(XAUDIOSRCHDR*);
template void Double::Process<SHORTLE, 4>(XAUDIOSRCHDR*);

template void Double::Process<float, 0>(XAUDIOSRCHDR*);
template void Double::Process<float, 1>(XAUDIOSRCHDR*);
template void Double::Process<float, 2>(XAUDIOSRCHDR*);
template void Double::Process<float, 4>(XAUDIOSRCHDR*);
template void Double::Process<float, 6>(XAUDIOSRCHDR*);

template void Double::Process<short, 0>(XAUDIOSRCHDR*);
template void Double::Process<short, 1>(XAUDIOSRCHDR*);
template void Double::Process<short, 4>(XAUDIOSRCHDR*);
template void Double::Process<short, 6>(XAUDIOSRCHDR*);

template void Double::Process<unsigned char, 0>(XAUDIOSRCHDR*);
template void Double::Process<unsigned char, 1>(XAUDIOSRCHDR*);
template void Double::Process<unsigned char, 2>(XAUDIOSRCHDR*);
template void Double::Process<unsigned char, 4>(XAUDIOSRCHDR*);
template void Double::Process<unsigned char, 6>(XAUDIOSRCHDR*);

} // namespace SRC
} // namespace XAUDIO
