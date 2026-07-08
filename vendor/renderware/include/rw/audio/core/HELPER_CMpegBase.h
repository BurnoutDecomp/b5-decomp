#pragma once

// =====================================================================================
// rw::audio::core::HELPER_CMpegBase
//
// EARenderWare "rwaudio" MPEG-audio decoder base -- the EALayer3 codec family's variant.
// This is a DISTINCT class from the sibling rw::audio::core::CMpegBase (CMpegBase.h): IDA
// disambiguated the two with the "HELPER_" prefix because they share method names but are
// separate implementations with separate addresses and, crucially, a DIFFERENT byte
// layout (this class's bit cursor lives at +0x18 / accumulator +0x24 / bit count +0x28,
// whereas the sibling uses +0x20 / +0x2C / +0x30). It is the base the two concrete
// EALayer3 decoders derive from and call into:
//     rw::audio::core::HELPER_CEALayer3        (calls Open / OpenSynth / GetBits / PolySynth)
//     rw::audio::core::HELPER_CMpegLayer3Base  (its ~dtor calls Close)
//
// It owns:
//   * the MPEG frame-header fields decoded from a 32-bit sync word (version / layer /
//     bitrate / sample-rate / mode / flags) plus the derived per-frame sample rate,
//     bitrate and frame size (ProcessHeader / GetHeader);
//   * a big-endian, MSB-first bit reader over the encoded byte stream (GetBits, reached
//     through the +0x18 byte cursor / +0x24 accumulator / +0x28 bit count);
//   * open / close / seek lifecycle over a caller-supplied byte buffer (Open / Close /
//     Seek);
//   * the poly-phase synthesis history buffer (OpenSynth allocates it through the shared
//     rwaudio System allocator; the destructor frees it via Close).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this
// TU.
//
// POLYMORPHISM NOTE. Unlike the sibling CMpegBase home (which models a manual vtable
// pointer because none of its bodies dispatch across classes), this class has GENUINE
// cross-class virtual dispatch in the asm and so is modelled as a real polymorphic C++
// base: GetHeader calls vtable slot +0x04 (ProcessHeader) and Open calls vtable slot
// +0x10 (the derived-decoder open hook, OnOpen). The compiler-emitted vector deleting
// destructor at 0x82B92100 is the thunk for the virtual destructor below (it reinstalls
// the base vtable, calls Close when open, and conditionally deletes). The two un-modelled
// virtuals between ProcessHeader (slot 1) and OnOpen (slot 4) are other EALayer3 decoder
// virtuals not in this TU; they are declared as pure placeholders solely so ProcessHeader
// and OnOpen land at their asm-observed vtable slots.
//
// LAYOUT NOTE (X360 byte offsets, from the asm). The compile target is x64 PC, so the
// pointer members widen; the fixed header offsets below describe the X360 image and are
// reached only by named member -- never by hardcoded offset. Gaps the reconstructed
// methods never touch are preserved as padding.
//   +0x00  <vptr>                    (polymorphic base; deleting dtor reinstalls off_8215A898)
//   +0x04  muSampleRate              (Hz; the divisor in the frame-size computation)
//   +0x08  muSampleRateCopy          (duplicate written alongside muSampleRate)
//   +0x0C  miBitRate                 (kbps table value for this frame)
//   +0x10  miFrameSize               (encoded frame size in bytes, less 4)
//   +0x16  mucLayerValue             (4 - layer id: 1 => Layer I, 2 => II, 3 => III)
//   +0x18  mpBitPointer              (byte cursor for GetBits / GetHeader)
//   +0x1C  mpDataStart               (start of the buffer passed to Open / Seek)
//   +0x20  mpFrameStart              (cursor snapshot taken after a successful Open)
//   +0x24  muBitBuffer               (left-justified bit accumulator)
//   +0x28  miBitsAvailable           (valid bits currently in muBitBuffer)
//   +0x2C  mucPolySynthPhase         (rotating poly-synth history phase; PolySynth only)
//   +0x2E  mucIsMpeg25               (MPEG 2.5 flag)
//   +0x2F  mucIsLsf                  (low-sampling-frequency: MPEG 2 or 2.5)
//   +0x30  mucSampleRateIndex2       (secondary sample-rate table index)
//   +0x32  mucVersionLowBit          (version-id low bit)
//   +0x33  mucProtectionBit          (CRC-protection-absent bit)
//   +0x34  mucBitRateIndex           (raw 4-bit bitrate index)
//   +0x35  mucSampleRateIndex        (combined 0..8 sample-rate table index)
//   +0x36  mucPaddingBit             (frame padding flag)
//   +0x37  mucMode                   (channel mode)
//   +0x38  mucModeExtension          (mode extension)
//   +0x39  mucCopyright              (copyright flag)
//   +0x3A  mucOriginal               (original/home flag)
//   +0x3B  mucIsOpen                 (decoder-open flag; gates Close / dtor)
//   +0x3C  mucChannelCount           (1 mono / 2 stereo)
//   +0x40  muHeaderWord              (pre-read header word, used when mpBitPointer is null)
//   +0x44  mpPolySynthHistory        (poly-synth history block, or null)
// =====================================================================================

#include "types.hpp" // u8, u16, u32, s32

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// HELPER_CMpegBase -- see the file header for the byte layout and the polymorphism note.
// -------------------------------------------------------------------------------------
class HELPER_CMpegBase
{
public:
    // vtable slot 0 -- the compiler emits the vector deleting destructor thunk
    // @0x82B92100 from this virtual destructor: it reinstalls the base vtable, calls
    // Close() when the decoder is open, and (when the thunk's flags bit 0 is set) deletes.
    virtual ~HELPER_CMpegBase();

    // vtable slot 1 (+0x04) @0x82B86150 -- decode a 32-bit MPEG sync word into the header
    // fields and derive the sample rate, bitrate and frame size. Returns the
    // samples-per-frame count (384 / 1152 / 576), or -1 if the header is invalid. Virtual:
    // GetHeader dispatches through vtable+0x04, so derived decoders may override it.
    virtual s32 ProcessHeader(u32 uHeader);

    // vtable slots 2 (+0x08) and 3 (+0x0C) -- un-modelled EALayer3 decoder virtuals not in
    // this TU. Declared as pure placeholders ONLY so ProcessHeader / OnOpen occupy their
    // asm-observed vtable slots; never called from this TU.
    virtual void ReservedSlot2() = 0;
    virtual void ReservedSlot3() = 0;

    // vtable slot 4 (+0x10) -- the derived-decoder open hook. Open dispatches to it (this
    // only, no args, return ignored) once the header is read and the synth is allocated.
    // Pure: the base has no implementation; only the concrete EALayer3 decoders provide it.
    virtual void OnOpen() = 0;

    // @0x82B92168 -- open the decoder over the caller's byte buffer: (re-)close, arm the
    // cursor, read the first header, allocate the synth, run the open hook and reset the
    // bit reader. Returns 0 on success, -1 on any failure.
    s32 Open(u8 *pData);

    // @0x82B86430 -- close the decoder: clear the pre-read header, drop the open flag and
    // free the poly-synth history through the shared rwaudio System allocator. Returns 0.
    s32 Close();

    // @0x82B860A8 -- point the bit reader at `pData` and reset the accumulator/bit count.
    void Seek(u8 *pData);

    // @0x82B85FE0 -- read `iNumBits` bits (MSB-first) from the byte stream, refilling the
    // accumulator a byte at a time; returns the bits right-justified.
    u32 GetBits(s32 iNumBits);

    // @0x82B86378 -- read the next 32-bit frame header (big-endian from mpBitPointer, or
    // the pre-stored muHeaderWord when the cursor is null) and process it. Returns 0 on a
    // valid header, -1 on a malformed one.
    s32 GetHeader();

    // @0x82B860C8 -- allocate the poly-synth history block (2304 bytes per channel) through
    // the shared rwaudio System allocator, store it at mpPolySynthHistory and zero it,
    // unless the global poly-synth-disabled flag is set. Returns 0 on success, -1 on
    // allocation failure.
    s32 OpenSynth();

    // @0x82B87080 -- poly-phase synthesis filter stage. BLOCKED here (needs the un-recovered
    // synthesis-window coefficient rodata unk_82156740 / unk_82156780 and the un-homed
    // matrixing helper sub_82B86488). Declared so callers/derived codecs can bind it; the
    // body is left to its own TU -- an honest gap.
    s32 PolySynth();

    // ---- fixed header (X360 offsets in the file-header comment) ----
    u32   muSampleRate;             // +0x04
    u32   muSampleRateCopy;         // +0x08
    s32   miBitRate;                // +0x0C
    s32   miFrameSize;              // +0x10
    u8    mPad14[2];                // +0x14 .. +0x15  (opaque)
    u8    mucLayerValue;            // +0x16
    u8    mPad17;                   // +0x17  (opaque)
    u8   *mpBitPointer;             // +0x18
    u8   *mpDataStart;              // +0x1C
    u8   *mpFrameStart;             // +0x20
    u32   muBitBuffer;              // +0x24
    s32   miBitsAvailable;          // +0x28
    u8    mucPolySynthPhase;        // +0x2C
    u8    mPad2D;                   // +0x2D  (opaque)
    u8    mucIsMpeg25;              // +0x2E
    u8    mucIsLsf;                 // +0x2F
    u8    mucSampleRateIndex2;      // +0x30
    u8    mPad31;                   // +0x31  (opaque)
    u8    mucVersionLowBit;         // +0x32
    u8    mucProtectionBit;         // +0x33
    u8    mucBitRateIndex;          // +0x34
    u8    mucSampleRateIndex;       // +0x35
    u8    mucPaddingBit;            // +0x36
    u8    mucMode;                  // +0x37
    u8    mucModeExtension;         // +0x38
    u8    mucCopyright;             // +0x39
    u8    mucOriginal;              // +0x3A
    u8    mucIsOpen;                // +0x3B
    u8    mucChannelCount;          // +0x3C
    u8    mPad3D[3];                // +0x3D .. +0x3F  (opaque)
    u32   muHeaderWord;             // +0x40
    void *mpPolySynthHistory;       // +0x44
};

} // namespace core
} // namespace audio
} // namespace rw
