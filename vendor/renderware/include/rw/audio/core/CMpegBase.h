#pragma once

// =====================================================================================
// rw::audio::core::CMpegBase
//
// EARenderWare "rwaudio" MPEG-audio decoder base. The shared MPEG-1/2/2.5 Layer I/II/III
// frame machinery that the two concrete rwaudio MPEG decoders -- rw::audio::core::Layer3Dec
// and rw::audio::core::EALayer3Core -- derive from (both construct a CMpegBase sub-object
// and call CMpegBase::AllocateSynth from their ctors, and route every bitstream read
// through CMpegBase::GetBits). It owns:
//   * the frame-header fields decoded from a 32-bit MPEG sync word (version / layer /
//     bitrate / sample-rate / mode / flags), plus the derived per-frame sample rate,
//     bitrate and frame size (ProcessHeader / GetHeader);
//   * a big-endian bit reader over the encoded byte stream (GetBits, reached through the
//     +0x20 byte cursor / +0x2C accumulator / +0x30 bit count);
//   * the poly-phase synthesis history buffer (AllocateSynth allocates it through the
//     shared rwaudio System allocator; the destructor frees it).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this
// TU. Sibling to the committed rw::audio::core homes (Decoder, Xas1Dec, XasDec, ...).
//
// LAYOUT NOTE (X360 byte offsets, from the asm). The compile target is x64 PC, so the two
// pointer members (vptr / mpBitPointer / mpPolySynthHistory) widen; the fixed header
// offsets below describe the X360 image and are reached only by named member -- never by
// hardcoded offset. Gaps the reconstructed methods never touch are preserved as padding.
//   +0x00  mpVTable                  (polymorphic base -> vector deleting destructor)
//   +0x04  muSampleRate              (Hz; the divisor in the frame-size computation)
//   +0x08  muSampleRateCopy          (duplicate written alongside muSampleRate)
//   +0x0C  miBitRate                 (kbps table value for this frame)
//   +0x10  miFrameSize               (encoded frame size in bytes, less 4)
//   +0x16  mucLayerValue             (4 - layer id: 1 => Layer I, 2 => II, 3 => III)
//   +0x18  mucChannelCount           (1 mono / 2 stereo)
//   +0x1C  muHeaderWord              (pre-read header word, used when mpBitPointer is null)
//   +0x20  mpBitPointer              (byte cursor for GetBits / GetHeader)
//   +0x2C  muBitBuffer               (left-justified bit accumulator)
//   +0x30  miBitsAvailable           (valid bits currently in muBitBuffer)
//   +0x38  mucPolySynthPhase[2]      (per-channel rotating poly-synth history phase, one u8
//                                     per channel -- PolySynth loads 0x38(this + channel))
//   +0x3A  mucIsMpeg25               (MPEG 2.5 flag)
//   +0x3B  mucIsLsf                  (low-sampling-frequency: MPEG 2 or 2.5)
//   +0x3C  mucSampleRateIndex2       (secondary sample-rate table index)
//   +0x3E  mucVersionLowBit          (version-id low bit)
//   +0x3F  mucProtectionBit          (CRC-protection-absent bit)
//   +0x40  mucBitRateIndex           (raw 4-bit bitrate index)
//   +0x41  mucSampleRateIndex        (combined 0..8 sample-rate table index)
//   +0x42  mucPaddingBit             (frame padding flag)
//   +0x43  mucMode                   (channel mode)
//   +0x44  mucModeExtension          (mode extension)
//   +0x45  mucCopyright              (copyright flag)
//   +0x46  mucOriginal               (original/home flag)
//   +0x48  mpPolySynthHistory        (poly-synth history block, or null)
//   +0x4C  mpPolySynthWork           (poly-synth work base PolySynth reads; the derived
//                                     decoders arm it from mpPolySynthHistory -- e.g.
//                                     Layer3Dec::Decode @0x82B919C0 does `work = history`
//                                     each pass, and Layer3Dec::Open zeroes 2304 bytes per
//                                     channel through it. 2304 bytes/channel = 2 banks of
//                                     288 f32 = 18 rows x 16 rotating history columns.)
// =====================================================================================

#include "types.hpp" // u8, u16, u32, s32

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// CMpegBase -- see the file header for the byte layout. Polymorphic base; the compiler
// emits a vector deleting destructor (modelled here as a static thunk, matching the
// rwaudio plug-in family convention rather than a C++ `virtual ~`).
// -------------------------------------------------------------------------------------
class CMpegBase
{
public:
    // @0x82B8BFF0 -- allocate the poly-synth history block (2304 bytes per channel),
    // store it at mpPolySynthHistory and zero it. Returns 0 on success, -1 on failure.
    s32 AllocateSynth(s32 iChannelCount);

    // @0x82B8AAE8 -- read `iNumBits` bits (MSB-first) from the byte stream, refilling the
    // accumulator a byte at a time; returns the bits right-justified.
    u32 GetBits(s32 iNumBits);

    // @0x82B8F3F0 -- read the next frame header (big-endian 32-bit from mpBitPointer, or
    // the pre-stored muHeaderWord when the cursor is null) and process it. Returns 0 on a
    // valid header, -1 on a malformed one.
    s32 GetHeader();

    // @0x82B8F128 -- decode a 32-bit MPEG sync word into the header fields and derive the
    // sample rate, bitrate and frame size. Returns the samples-per-frame count (384 / 1152
    // / 576), or -1 if the header is invalid.
    s32 ProcessHeader(u32 uHeader);

    // @0x82B8EE50 -- poly-phase synthesis filter stage: rotate this channel's history
    // phase, run the 32-point synthesis DCT (the file-static Dct32 helper, sub_82B8E258 in
    // the XEX) into the two rotating history columns, then window 32 PCM output samples
    // from the history rows. `pInSamples` is one 32-float subband line; `pOutSamples`
    // receives 32 samples. (The asm leaves no meaningful value in r3 and both callers --
    // Layer3Dec::Decode / EALayer3Core::Decode -- ignore it, so the return is void.)
    void PolySynth(s32 iChannel, f32 *pOutSamples, const f32 *pInSamples);

    // @0x82B94FB0 -- vector deleting destructor: reinstall the CMpegBase vtable, free the
    // poly-synth history, and (when bit 0 of `flags` is set) delete `self`.
    static void *VectorDeletingDestructor(CMpegBase *self, char flags);

    // ---- fixed header (X360 offsets in the file-header comment) ----
    void *mpVTable;                 // +0x00
    u32   muSampleRate;             // +0x04
    u32   muSampleRateCopy;         // +0x08
    s32   miBitRate;                // +0x0C
    s32   miFrameSize;              // +0x10
    u8    mPad14[2];                // +0x14 .. +0x15  (opaque)
    u8    mucLayerValue;            // +0x16
    u8    mPad17;                   // +0x17  (opaque)
    u8    mucChannelCount;          // +0x18
    u8    mPad19[3];                // +0x19 .. +0x1B  (opaque)
    u32   muHeaderWord;             // +0x1C
    u8   *mpBitPointer;             // +0x20
    u8    mPad24[8];                // +0x24 .. +0x2B  (opaque)
    u32   muBitBuffer;              // +0x2C
    s32   miBitsAvailable;          // +0x30
    u8    mPad34[4];                // +0x34 .. +0x37  (opaque)
    u8    mucPolySynthPhase[2];     // +0x38 .. +0x39  (per-channel; asm indexes 0x38(this + channel))
    u8    mucIsMpeg25;              // +0x3A
    u8    mucIsLsf;                 // +0x3B
    u8    mucSampleRateIndex2;      // +0x3C
    u8    mPad3D;                   // +0x3D  (opaque)
    u8    mucVersionLowBit;         // +0x3E
    u8    mucProtectionBit;         // +0x3F
    u8    mucBitRateIndex;          // +0x40
    u8    mucSampleRateIndex;       // +0x41
    u8    mucPaddingBit;            // +0x42
    u8    mucMode;                  // +0x43
    u8    mucModeExtension;         // +0x44
    u8    mucCopyright;             // +0x45
    u8    mucOriginal;              // +0x46
    u8    mPad47;                   // +0x47  (opaque)
    void *mpPolySynthHistory;       // +0x48
    void *mpPolySynthWork;          // +0x4C
};

} // namespace core
} // namespace audio
} // namespace rw
