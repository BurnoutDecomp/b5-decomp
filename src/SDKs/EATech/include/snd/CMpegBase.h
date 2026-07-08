#ifndef SDKS_EATECH_SND_CMPEGBASE_H
#define SDKS_EATECH_SND_CMPEGBASE_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/CMpegBase.h
//
// EATech "Snd" MPEG-audio decoder base. The shared MPEG-1/2/2.5 Layer I/II/III
// frame machinery that the concrete Snd EALayer3 decoder (Snd::CEALayer3)
// derives from: it routes every bitstream read through GetBits, decodes each
// frame header (GetHeader / ProcessHeader), and owns the poly-phase synthesis
// history block (OpenSynth allocates it; the destructor's Close tail frees it).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative
// for every offset, width and side-effect. No Feb-2007 source and no DecFIGS
// DWARF exist for this TU/source path. This is a DISTINCT class from the
// rwaudio rw::audio::core::CMpegBase home (that one has mpBitPointer @ +0x20 and
// the header word @ +0x1C; this Snd class has the byte cursor @ +0x24 and the
// header word @ +0x4C -- a genuinely different layout, so it is not forked from
// or merged with the rwaudio home).
//
// LAYOUT NOTE (X360 byte offsets, from the asm). The compile target is x64 PC, so
// the vptr and the byte-stream / history pointers widen; the fixed offsets below
// describe the X360 image and are reached only by named member -- never by a
// hardcoded offset. Gaps the reconstructed methods never touch are preserved as
// opaque padding.
//   +0x00  vptr
//   +0x04  muSampleRate          (Hz; the divisor in the frame-size computation)
//   +0x08  muSampleRateCopy      (duplicate written alongside muSampleRate)
//   +0x0C  miDecodeSize          (PCM output bytes/frame: 768/2304/1152 * channels)
//   +0x10  miBitRate             (bitrate table value for this frame)
//   +0x14  miFrameSize           (encoded frame size in bytes, less 4)
//   +0x18  mucLayerValue         (4 - layer id: 1 => Layer I, 2 => II, 3 => III)
//   +0x24  mpBitPointer          (byte cursor for GetBits / GetHeader)
//   +0x28  mpStreamStart         (saved copy of the stream start pointer)
//   +0x2C  mpFrameStart          (saved copy of the current-frame data pointer)
//   +0x30  muBitBuffer           (left-justified bit accumulator)
//   +0x34  miBitsAvailable       (valid bits currently in muBitBuffer)
//   +0x38  mucSynthPhase0         (rotating poly-synth history phase, channel 0)
//   +0x39  mucSynthPhase1         (rotating poly-synth history phase, channel 1)
//   +0x3A  mucIsMpeg25           (MPEG 2.5 flag)
//   +0x3B  mucIsLsf              (low-sampling-frequency: MPEG 2 or 2.5)
//   +0x3C  mucSampleRateIndex2   (secondary sample-rate table index)
//   +0x3E  mucVersionLowBit      (version-id low bit)
//   +0x3F  mucProtectionBit      (CRC-protection-absent bit)
//   +0x40  mucBitRateIndex       (raw 4-bit bitrate index)
//   +0x41  mucSampleRateIndex    (combined 0..8 sample-rate table index)
//   +0x42  mucPaddingBit         (frame padding flag)
//   +0x43  mucMode               (channel mode)
//   +0x44  mucModeExtension      (mode extension)
//   +0x45  mucCopyright          (copyright flag)
//   +0x46  mucOriginal           (original/home flag)
//   +0x47  mucIsOpen             (a stream is currently open)
//   +0x48  mucChannelCount       (1 mono / 2 stereo)
//   +0x4C  muHeaderWord          (pre-read header word, used when mpBitPointer null)
//   +0x50  mpPolySynthHistory    (poly-synth history block, or null)
// ============================================================================

namespace Snd
{

// ----------------------------------------------------------------------------
// CMpegBase -- see the file header for the byte layout. Polymorphic base; the
// derived Snd::CEALayer3 overrides ProcessHeader / OpenLayer / Close. The four
// virtuals reconstructed here occupy X360 vtable byte-offsets +0x04 (ProcessHeader),
// +0x0C (Close) and +0x14 (OpenLayer) plus the deleting-destructor slot; the
// intervening slots belong to sibling virtuals outside this TU and are not
// modelled (we do not byte-match the vtable -- semantic dispatch is preserved).
// ----------------------------------------------------------------------------
class CMpegBase
{
public:
    // @0x82B7E688 -- scalar deleting destructor: reinstall the CMpegBase vtable
    // (compiler-emitted), call Close() when a stream is open, and (on the deleting
    // variant) operator delete `this`. Modelled as the real virtual destructor so
    // MSVC emits exactly that vtable-store + conditional Close + operator-delete.
    virtual ~CMpegBase();

    // @0x82B74A28 (X360 vtable +0x04) -- decode a 32-bit MPEG sync word into the
    // header fields and derive the sample rate, bitrate and frame size. Returns 0
    // on a valid header, -1 if the sync word or bitrate index is invalid.
    virtual s32 ProcessHeader(u32 auHeader);

    // Close the current stream. Body lives in its own TU (not this one); declared
    // virtual here so Open / the destructor dispatch through the vtable exactly as
    // the X360 does (vtable +0x0C).
    virtual void Close();

    // @0x82B74C98 (X360 vtable +0x14) -- size the per-frame PCM output buffer from
    // the layer id and channel count and prime the poly-synth phase counters.
    // Always returns 0.
    virtual s32 OpenLayer();

    // @0x82B7E6F0 -- open a byte stream: reset if already open, read+process the
    // first header, allocate the synth history and size the output. Returns 0 on
    // success, -1 on a null stream or a malformed first header.
    s32 Open(u8* apData);

    // @0x82B74C18 -- read the next 32-bit frame header (big-endian from the byte
    // cursor, or the pre-stored muHeaderWord when the cursor is null) and process
    // it (virtual ProcessHeader). Returns 0 on a valid header, -1 otherwise.
    s32 GetHeader();

    // @0x82B73378 -- read `aiNumBits` bits MSB-first from the byte stream, refilling
    // the accumulator a byte at a time; returns the bits right-justified.
    u32 GetBits(s32 aiNumBits);

    // @0x82B749C0 -- allocate the poly-synth history block (2304 bytes per channel)
    // through the shared Snd allocator hook, store it at mpPolySynthHistory and zero
    // it. Skipped entirely when the global synth-alloc guard is set. Returns the
    // buffer (or `this` when skipped) -- the X360 leaves it in r3; callers ignore it.
    void* OpenSynth();

    // @0x82B749A0 -- reposition the bit reader onto `apPos` and clear the bit
    // accumulator. Returns `this`.
    CMpegBase* Seek(u8* apPos);

    // @0x82B771D8 -- poly-phase synthesis filter stage. BLOCKED here: needs the
    // un-recovered synthesis-window coefficient rodata (unk_8214B6C0 / unk_8214B700)
    // and the un-homed matrixing helper sub_82B765E0, so its history addressing
    // cannot be grounded. Declared only so callers/derived codecs bind to it; the
    // body lives in its own TU.
    s32 PolySynth();

    // ---- fixed header (X360 offsets in the file-header comment) ----
    // vptr occupies +0x00 (compiler-owned).
    u32   muSampleRate;             // +0x04
    u32   muSampleRateCopy;         // +0x08
    s32   miDecodeSize;             // +0x0C
    s32   miBitRate;                // +0x10
    s32   miFrameSize;              // +0x14
    u8    mucLayerValue;            // +0x18
    u8    mPad19[11];               // +0x19 .. +0x23  (opaque)
    u8*   mpBitPointer;             // +0x24
    u8*   mpStreamStart;            // +0x28
    u8*   mpFrameStart;             // +0x2C
    u32   muBitBuffer;              // +0x30
    s32   miBitsAvailable;          // +0x34
    u8    mucSynthPhase0;           // +0x38
    u8    mucSynthPhase1;           // +0x39
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
    u8    mucIsOpen;                // +0x47
    u8    mucChannelCount;          // +0x48
    u8    mPad49[3];                // +0x49 .. +0x4B  (opaque)
    u32   muHeaderWord;             // +0x4C
    void* mpPolySynthHistory;       // +0x50
};

} // namespace Snd

#endif // SDKS_EATECH_SND_CMPEGBASE_H
