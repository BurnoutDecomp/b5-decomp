#pragma once

// =====================================================================================
// rw::audio::core::HELPER_CMpegBase
//
// EARenderWare "rwaudio" MPEG-audio decoder base -- the EALayer3 codec family's variant.
// This is a DISTINCT class from the sibling rw::audio::core::CMpegBase (CMpegBase.h). The
// "HELPER_" prefix is NOT an IDA disambiguation: the NFS ProStreet 08 Milestone PDB
// (X360, Oct-2007, same-era rwaudiocore -- the designated rw::audio::core type ground
// truth) carries the class as `rw::audio::core::HELPER_CMpegBase` in its own mangled
// symbols, i.e. the vendor source itself embeds the EALayer3 helper library's MPEG base
// under this macro-renamed identity beside the plain CMpegBase. It is the base the two
// concrete EALayer3 decoders derive from and call into:
//     rw::audio::core::HELPER_CEALayer3        (calls Open / OpenSynth / GetBits / PolySynth)
//     rw::audio::core::HELPER_CMpegLayer3Base  (its ~dtor calls Close)
//
// It owns:
//   * the MPEG frame-header fields decoded from a 32-bit sync word (version / layer /
//     bitrate / sample-rate / mode / flags) plus the derived per-frame sample rate,
//     bitrate, frame size and samples-per-frame (ProcessHeader / GetHeader / OpenLayer);
//   * a big-endian, MSB-first bit reader over the encoded byte stream (GetBits, reached
//     through the +0x18 byte cursor / +0x24 accumulator / +0x28 bit count);
//   * open / close / seek lifecycle over a caller-supplied byte buffer (Open / Close /
//     Seek);
//   * the poly-phase synthesis history (OpenSynth allocates one [2][288]-float
//     double-bank block per channel through the shared rwaudio System allocator; the
//     per-channel rotating column phase lives in mucBandOffset[]; PolySynth runs the
//     32-point synthesis DCT + windowing; the destructor frees the block via Close).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this
// TU. Declaration SHAPE (virtual-ness, access, signatures) and member names are
// corroborated by the ProStreet PDB (`llvm-pdbutil pretty -classes
// -include-types="HELPER_CMpegBase"`), gated on the X360 image: ARTIST's vtable and
// ledger decide what exists here.
//
// POLYMORPHISM NOTE (measured, not inferred). The X360 vtable at 0x8215A898 -- dumped
// from the image and reinstalled by the deleting destructor @0x82B92100 and by
// ~HELPER_CMpegLayer3Base @0x82B92E34 -- has exactly FIVE slots:
//     slot 0 (+0x00)  `vector deleting destructor'  @0x82B92100
//     slot 1 (+0x04)  ProcessHeader                 @0x82B86150  (GetHeader dispatches here)
//     slot 2 (+0x08)  Open                          @0x82B92168
//     slot 3 (+0x0C)  Seek                          @0x82B860A8
//     slot 4 (+0x10)  OpenLayer                     @0x82B863F8  (Open dispatches here)
// The ProStreet PDB manglings agree: ??1 / ProcessHeader / Open / Seek are `UAA`
// (public virtual) and OpenLayer is `MAA` (protected virtual) -- everything else in the
// class is non-virtual. (ProStreet's Open takes (void*, int); the ARTIST asm takes only
// the buffer pointer -- version drift, ARTIST wins.)
//
// LAYOUT NOTE (X360 byte offsets, from the asm; names cross-checked against the
// ProStreet PDB layout, sizeof = 0x48 there). The compile target is x64 PC, so the
// pointer members widen; the fixed header offsets below describe the X360 image and are
// reached only by named member -- never by hardcoded offset. PDB names in [brackets]
// where this header keeps an earlier hand-derived name.
//   +0x00  <vptr>                    (5-slot vtable, see above)
//   +0x04  muSampleRate              [Real_mSampFreq] (Hz; divisor in the frame-size calc)
//   +0x08  muSampleRateCopy          [mSampFreq] (the working copy derived code may rescale)
//   +0x0C  miBitRate                 [mBitRate] (kbps table value for this frame)
//   +0x10  miFrameSize               [cFrameSize] (encoded frame size in bytes, less 4)
//   +0x14  muFrameSamples            [mFrameSamples] (u16 samples/frame; OpenLayer writes
//                                     1152, or 576 when LSF)
//   +0x16  mucLayerValue             [mLayer] (4 - layer id: 1 => Layer I, 2 => II, 3 => III)
//   +0x18  mpBitPointer              [mBufPtr] (byte cursor for GetBits / GetHeader)
//   +0x1C  mpDataStart               [mBufPtrBase] (start of the buffer given to Open / Seek)
//   +0x20  mpFrameStart              [mBufPtrNext] (cursor snapshot taken after a
//                                     successful Open -- the next frame's start)
//   +0x24  muBitBuffer               [mShiftReg] (left-justified bit accumulator)
//   +0x28  miBitsAvailable           [mShiftRegBits] (valid bits currently in muBitBuffer)
//   +0x2C  mucBandOffset[2]          [mBandOffset] (PER-CHANNEL rotating poly-synth history
//                                     column phase: PolySynth loads/stores
//                                     0x2C(this + channel); OpenLayer resets both to 1)
//   +0x2E  mucIsMpeg25               [mMPEG25] (MPEG 2.5 flag)
//   +0x2F  mucIsLsf                  [mLSF] (low-sampling-frequency: MPEG 2 or 2.5)
//   +0x30  mucSampleRateIndex2       [sfreq] (secondary sample-rate table index)
//   +0x31  mucMaxGranules            [max_gr] (written by derived Layer-III code, not here)
//   +0x32  mucVersionLowBit          [mVersion] (version-id low bit)
//   +0x33  mucProtectionBit          [mErrorProt] (CRC-protection bit, header bit 16)
//   +0x34  mucBitRateIndex           [mBitRateIdx] (raw 4-bit bitrate index)
//   +0x35  mucSampleRateIndex        [mSampFreqIdx] (combined 0..8 sample-rate table index)
//   +0x36  mucPaddingBit             [mPadding] (frame padding flag)
//   +0x37  mucMode                   [mMode] (channel mode)
//   +0x38  mucModeExtension          [mModeExt] (mode extension)
//   +0x39  mucCopyright              [mCopyright] (copyright flag)
//   +0x3A  mucOriginal               [mOriginal] (original/home flag)
//   +0x3B  mucIsOpen                 [mOpened] (decoder-open flag; gates Close / dtor)
//   +0x3C  mucChannelCount           [mChannels] (1 mono / 2 stereo)
//   +0x40  muHeaderWord              [mHeader] (pre-read header word, used when
//                                     mpBitPointer is null)
//   +0x44  mpPolySynthHistoryF       (poly-synth history: one f32[2][288] double-bank
//                                     block per channel, or null. In the PDB this is a
//                                     union { float(*)[2][288] mpPolySynthHistoryF;
//                                     short(*)[2][288] mpPolySynthHistoryS; } -- the X360
//                                     build allocates the float form, tag
//                                     "PolySynthHistoryF" @0x8215672C.)
// =====================================================================================

#include "types.hpp" // u8, u16, u32, s32, f32

namespace rw
{
namespace audio
{
namespace core
{

// HELPER_MPEGuse_MMX (@0x8327A300 in the X360 .data, PDB-named) -- the mpg123-heritage
// "use MMX synth" config flag shared by this class (OpenSynth skips the history
// allocation when set) and HELPER_CEALayer3 (DecodeMono / DecodeStereo consult it).
// NOTHING in the X360 image ever writes it (its only xrefs are those three reads), so it
// is permanently 0 on this platform -- a PC-era leftover kept for parity. Defined in
// HELPER_CMpegBase.cpp.
extern s32 HELPER_MPEGuse_MMX;

// -------------------------------------------------------------------------------------
// HELPER_CMpegBase -- see the file header for the byte layout and the vtable map.
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
    // samples-per-frame count (384 / 1152 / 576), or -1 if the header is invalid.
    // GetHeader dispatches through this slot.
    virtual s32 ProcessHeader(u32 uHeader);

    // vtable slot 2 (+0x08) @0x82B92168 -- open the decoder over the caller's byte
    // buffer: (re-)close, arm the cursor, read the first header, allocate the synth, run
    // the OpenLayer hook and reset the bit reader. Returns 0 on success, -1 on failure.
    virtual s32 Open(u8 *pData);

    // vtable slot 3 (+0x0C) @0x82B860A8 -- point the bit reader at `pData` (cursor,
    // data-start and frame-start all reset to it) and clear the accumulator/bit count.
    // (The X360 ABI leaves `this` in r3 at return; the PDB mangling `UAAXPAX` says the
    // C++ return type is void.)
    virtual void Seek(u8 *pData);

    // vtable slot 4 (+0x10) @0x82B863F8 -- the per-layer open hook Open dispatches to
    // (result ignored there). The base implementation records the frame's sample count
    // (1152, or 576 when LSF) in muFrameSamples and resets both per-channel poly-synth
    // band offsets to 1; returns 0. ProStreet PDB mangling `MAAHXZ`: protected virtual
    // s32 (kept public here -- this header models the whole class public, see .cpp note).
    virtual s32 OpenLayer();

    // @0x82B86430 -- close the decoder: clear the pre-read header, drop the open flag and
    // free the poly-synth history through the shared rwaudio System allocator. Returns 0.
    s32 Close();

    // @0x82B85FE0 -- read `iNumBits` bits (MSB-first) from the byte stream, refilling the
    // accumulator a byte at a time; returns the bits right-justified.
    u32 GetBits(s32 iNumBits);

    // @0x82B86378 -- read the next 32-bit frame header (big-endian from mpBitPointer, or
    // the pre-stored muHeaderWord when the cursor is null) and process it (virtual
    // dispatch through slot 1). Returns 0 on a valid header, -1 on a malformed one.
    s32 GetHeader();

    // @0x82B860C8 -- allocate the poly-synth history (2304 bytes = f32[2][288] per
    // channel) through the shared rwaudio System allocator, store it at
    // mpPolySynthHistoryF and zero it, unless HELPER_MPEGuse_MMX is set. Returns 0 on
    // success, -1 on allocation failure.
    s32 OpenSynth();

    // @0x82B87080 -- poly-phase synthesis filter stage for one 32-sample subband line of
    // one channel: rotate this channel's band offset (mucBandOffset[iChannel] =
    // (offset - 1) & 0xF), run the 32-point synthesis DCT (the file-static Dct32 helper,
    // sub_82B86488 in the XEX -- called ONLY from here) into the channel's two rotating
    // history banks, then window 32 PCM output samples from the history rows against the
    // 544-entry synthesis window table. `pInSamples` is one 32-float subband line;
    // `pOutSamples` receives 32 samples. Signature from the ProStreet PDB mangling
    // `IAAXHPAM0@Z` (void, (int, float*, float*)), matching the ARTIST asm register use
    // (r4 channel / r5 out / r6 in; r3 dead at return).
    void PolySynth(s32 iChannel, f32 *pOutSamples, f32 *pInSamples);

    // The MPEG sample-rate table (word_82156718 in the X360 rodata, 0x82156718; a static
    // class member in the ProStreet PDB: `?sSampleRateTable@HELPER_CMpegBase@core@audio@
    // rw@@1QBGB`). Indexed by mucSampleRateIndex (0..8); also read by
    // HELPER_CEALayer3::ProcessHeader. Defined in HELPER_CMpegBase.cpp from the image
    // dump.
    static const u16 sSampleRateTable[9];

    // ---- fixed header (X360 offsets + PDB names in the file-header comment) ----
    u32   muSampleRate;             // +0x04 [Real_mSampFreq]
    u32   muSampleRateCopy;         // +0x08 [mSampFreq]
    s32   miBitRate;                // +0x0C [mBitRate]
    s32   miFrameSize;              // +0x10 [cFrameSize]
    u16   muFrameSamples;           // +0x14 [mFrameSamples] (1152 / 576; OpenLayer)
    u8    mucLayerValue;            // +0x16 [mLayer]
    u8    mPad17;                   // +0x17 (PDB: padding)
    u8   *mpBitPointer;             // +0x18 [mBufPtr]
    u8   *mpDataStart;              // +0x1C [mBufPtrBase]
    u8   *mpFrameStart;             // +0x20 [mBufPtrNext]
    u32   muBitBuffer;              // +0x24 [mShiftReg]
    s32   miBitsAvailable;          // +0x28 [mShiftRegBits]
    u8    mucBandOffset[2];         // +0x2C [mBandOffset] (per-channel synth phase)
    u8    mucIsMpeg25;              // +0x2E [mMPEG25]
    u8    mucIsLsf;                 // +0x2F [mLSF]
    u8    mucSampleRateIndex2;      // +0x30 [sfreq]
    u8    mucMaxGranules;           // +0x31 [max_gr] (derived Layer-III code writes it)
    u8    mucVersionLowBit;         // +0x32 [mVersion]
    u8    mucProtectionBit;         // +0x33 [mErrorProt]
    u8    mucBitRateIndex;          // +0x34 [mBitRateIdx]
    u8    mucSampleRateIndex;       // +0x35 [mSampFreqIdx]
    u8    mucPaddingBit;            // +0x36 [mPadding]
    u8    mucMode;                  // +0x37 [mMode]
    u8    mucModeExtension;         // +0x38 [mModeExt]
    u8    mucCopyright;             // +0x39 [mCopyright]
    u8    mucOriginal;              // +0x3A [mOriginal]
    u8    mucIsOpen;                // +0x3B [mOpened]
    u8    mucChannelCount;          // +0x3C [mChannels]
    u8    mPad3D[3];                // +0x3D .. +0x3F (PDB: padding)
    u32   muHeaderWord;             // +0x40 [mHeader]
    f32 (*mpPolySynthHistoryF)[2][288]; // +0x44 (one [bank][elem] block per channel;
                                        //  PDB unions this with a s16 variant)
};

} // namespace core
} // namespace audio
} // namespace rw
