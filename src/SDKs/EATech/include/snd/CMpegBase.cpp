// ============================================================================
// SDKs/EATech/include/snd/CMpegBase.cpp
//
// Out-of-line bodies for the Snd MPEG-audio decoder base declared in CMpegBase.h.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for
// every offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF
// exist for this TU.
//
//   ~CMpegBase       @0x82B7E688  (scalar deleting destructor)
//   ProcessHeader    @0x82B74A28
//   OpenLayer        @0x82B74C98
//   Open             @0x82B7E6F0
//   GetHeader        @0x82B74C18
//   GetBits          @0x82B73378
//   OpenSynth        @0x82B749C0
//   Seek             @0x82B749A0
//   PolySynth        @0x82B771D8 -- RECONSTRUCTED in CMpegBase_wO_01.cpp (wave O),
//       together with the file-static matrixing helper and the two rodata blocks.
//       DO NOT add a second definition here.
//       The wave-N blocker is CRACKED (wave O): the synthesis-window rodata
//       (unk_8214B6C0, 544 f32) and the Dct32 coefficients (flt_8214BF40, 31 f32)
//       are DUMPED FROM THE IMAGE (never filled in from ISO/IEC 11172-3 Table B.3 --
//       the image is assumed to hold a rounded/scaled variant, as a sibling wave
//       proved for the XDK-rounded BT.601 coefficients) in
//       scratchpad/waveO/SndCMpegBase.spec.md, and the
//       "un-homed" matrixing helper sub_82B765E0 HOMES HERE as a file-static (its
//       only caller is PolySynth; the X360 compiler even interprocedurally kept
//       r6-r9 live across the call, proving TU-local codegen). Both rodata blocks
//       are byte-identical to the rw::audio::core copies, and both functions are
//       instruction-identical to the wave-M interpreter-verified twins
//       (sub_82B8E258 / 0x82B8EE50) except for ONE load: this class reads
//       mpPolySynthHistory (+0x50) where the twin reads mpPolySynthWork (+0x4C).
//   Close            @0x82B74CF0 -- vtable slot +0x0C, but NOT one of this TU's 9
//       ledger functions (absent from identity.json -- identity gap; spec section
//       6 carries its dumped asm). RECONSTRUCTED in CMpegBase_wO_01.cpp (wave O),
//       so this TU now ships 10 bodies against 9 ledger rows.
//
// The `twllei` / `twlgei` in the ProcessHeader asm are the compiler's divide-by-
// zero / overflow trap instrumentation around `divw`, not source-level calls, so
// they are dropped.
// ============================================================================

#include "SDKs/EATech/include/snd/CMpegBase.h"

#include <cstring> // std::memset (the X360 XMemSet)

namespace Snd
{

namespace
{
// dword_83277C9C -- global "skip synth allocation" guard. When non-zero OpenSynth
// is a no-op. Owned by the Snd allocator/config TU; referenced here as an extern
// (never fabricated), exactly as the sibling decoders reference their rodata.
extern "C" u32 dword_83277C9C;

// dword_83277830 -- the shared Snd allocator hook (a function pointer). Takes a
// byte count and returns the block. Also owned elsewhere; referenced by extern.
extern "C" void* (*dword_83277830)(u32 auSize);

// The MPEG frame-header lookup tables (X360 rodata). ProcessHeader only *indexes*
// them -- their contents (the standard MPEG sample-rate Hz and bitrate values) live
// in the audio rodata TU, so they are referenced as externs and never fabricated
// here. The sample-rate table is read with `lhzx` (unsigned u16); the bitrate table
// with `lhax` (signed s16).
extern "C" const u16 word_8214B6A8[]; // sample-rate table (Hz), indexed by mucSampleRateIndex
extern "C" const s16 word_8214B5E8[]; // bitrate table, indexed by the derived slot
} // namespace

// ----------------------------------------------------------------------------
// ~CMpegBase @0x82B7E688
// Close the stream if one is open. The vtable reinstall and the conditional
// operator-delete of the "scalar deleting destructor" thunk are compiler-emitted;
// the Close() call here dispatches statically to CMpegBase::Close (a virtual call
// in a destructor resolves to the current class), matching the direct X360 call.
// ----------------------------------------------------------------------------
CMpegBase::~CMpegBase()
{
    if (mucIsOpen)
        Close();
}

// ----------------------------------------------------------------------------
// GetBits @0x82B73378
// Read `aiNumBits` bits MSB-first from the byte stream. muBitBuffer is left-
// justified: bytes are shifted into the top as they are consumed, the result is
// taken from the high `aiNumBits` bits, then the accumulator is shifted up.
// ----------------------------------------------------------------------------
u32 CMpegBase::GetBits(s32 aiNumBits)
{
    // HOST DIVERGENCE GUARD (wave O, mirrors the wave-M fix in the rw::audio::core
    // twins): for aiNumBits == 0 the X360 tail is `srw r3, r11, r10` with r10 = 32,
    // and PPC srw takes a 6-bit shift count, so the console returns 0 (and leaves
    // the accumulator state untouched, as the guard does). On x64 `luBits >> 32` is
    // UB and MSVC masks the count to 0, returning the raw accumulator instead.
    // aiNumBits == 0 is reachable in real MP3 data (a scalefactor slen of 0).
    if (aiNumBits == 0)
        return 0;

    while (miBitsAvailable < aiNumBits)
    {
        muBitBuffer |= static_cast<u32>(*mpBitPointer) << (24 - miBitsAvailable);
        ++mpBitPointer;
        miBitsAvailable += 8;
    }

    const u32 luBits = muBitBuffer;
    miBitsAvailable -= aiNumBits;
    muBitBuffer = luBits << aiNumBits;
    return luBits >> (32 - aiNumBits);
}

// ----------------------------------------------------------------------------
// GetHeader @0x82B74C18
// Fetch the next 32-bit frame header -- big-endian from the byte cursor when it is
// set, otherwise the pre-stored muHeaderWord -- and process it (virtual dispatch).
// Returns 0 on a valid header, -1 on a malformed one.
// ----------------------------------------------------------------------------
s32 CMpegBase::GetHeader()
{
    u32 luHeader;
    if (mpBitPointer)
    {
        luHeader = (static_cast<u32>(mpBitPointer[0]) << 24)
                 | (static_cast<u32>(mpBitPointer[1]) << 16)
                 | (static_cast<u32>(mpBitPointer[2]) << 8)
                 |  static_cast<u32>(mpBitPointer[3]);
    }
    else
    {
        luHeader = muHeaderWord;
    }

    return (ProcessHeader(luHeader) == -1) ? -1 : 0;
}

// ----------------------------------------------------------------------------
// ProcessHeader @0x82B74A28 (virtual)
// Decode a 32-bit MPEG sync word into the header fields, then derive the sample
// rate, bitrate and frame size. Returns 0 on a valid header, -1 when the 11-bit
// frame sync is absent or the bitrate index is "free format" (0). `mucLayerValue`
// holds 4 - the layer id, so 1 => Layer I .. 3 => Layer III.
// ----------------------------------------------------------------------------
s32 CMpegBase::ProcessHeader(u32 auHeader)
{
    // 11-bit frame sync must be all ones.
    if ((auHeader & 0xFFE00000u) != 0xFFE00000u)
        return -1;

    const u8 lucLayerValue = static_cast<u8>(4 - ((auHeader >> 17) & 3));
    const u8 lucVersionLow = static_cast<u8>((auHeader >> 19) & 1);
    const u8 lucBitRateIndex = static_cast<u8>((auHeader >> 12) & 0xF);
    const u8 lucPadding = static_cast<u8>((auHeader >> 9) & 1);

    mucVersionLowBit = lucVersionLow;
    mucBitRateIndex = lucBitRateIndex;
    mucLayerValue = lucLayerValue;
    mucPaddingBit = lucPadding;
    mucMode = static_cast<u8>((auHeader >> 6) & 3);
    mucProtectionBit = static_cast<u8>((auHeader >> 16) & 1);
    mucModeExtension = static_cast<u8>((auHeader >> 4) & 3);
    mucCopyright = static_cast<u8>((auHeader >> 3) & 1);
    mucOriginal = static_cast<u8>((auHeader >> 2) & 1);

    // MPEG version bit (20) -> MPEG-2.5 / low-sampling-frequency flags.
    bool lbLsf;
    if ((auHeader & 0x100000u) != 0)
    {
        mucIsMpeg25 = 0;
        lbLsf = (lucVersionLow == 0);
    }
    else
    {
        mucIsMpeg25 = 1;
        lbLsf = true;
    }
    mucIsLsf = lbLsf ? 1 : 0;

    // Sample-rate table indices (combined MPEG-1 / 2 / 2.5 space).
    const u8 lucSampleRateRaw = static_cast<u8>((auHeader >> 10) & 3);
    if (mucIsMpeg25)
    {
        mucSampleRateIndex = static_cast<u8>(lucSampleRateRaw + 6);
        mucSampleRateIndex2 = static_cast<u8>(lucSampleRateRaw + 6);
    }
    else
    {
        const u8 lucLsf = mucIsLsf;
        mucSampleRateIndex = static_cast<u8>(3 * lucLsf + lucSampleRateRaw);
        mucSampleRateIndex2 = static_cast<u8>((lucLsf == 0 ? 0 : 3) + lucSampleRateRaw);
    }

    // Mono when the channel mode == 3, stereo otherwise.
    mucChannelCount = static_cast<u8>((mucMode == 3) ? 1 : 2);

    const u32 luSampleRate = word_8214B6A8[mucSampleRateIndex];
    muSampleRate = luSampleRate;
    muSampleRateCopy = luSampleRate;

    // "free format" bitrate index (0) is not supported here.
    if (lucBitRateIndex == 0)
        return -1;

    const u8 lucLsf = mucIsLsf;
    const s32 liBitRateSlot = 48 * lucLsf + 16 * lucLayerValue - 16 + lucBitRateIndex;
    const s32 liBitRate = word_8214B5E8[liBitRateSlot];
    miBitRate = liBitRate;

    if (lucLayerValue == 1) // Layer I
    {
        miFrameSize = 4 * (12000 * liBitRate / static_cast<s32>(muSampleRate) + lucPadding);
    }
    else // Layer II / III
    {
        const s32 liFrameSize = 144000 * liBitRate / static_cast<s32>(muSampleRate);
        miFrameSize = liFrameSize;

        if (lucLayerValue == 3 && lbLsf) // Layer III LSF
            miFrameSize = liFrameSize >> 1;

        if (lucPadding)
            ++miFrameSize;
    }

    miFrameSize -= 4;
    return 0;
}

// ----------------------------------------------------------------------------
// OpenLayer @0x82B74C98 (virtual)
// Size the per-frame PCM output buffer from the layer id and channel count and
// prime the two poly-synth phase counters. Layer I => 768 bytes/channel; Layer
// II / III => 2304 bytes/channel, halved for Layer III LSF. Always returns 0.
// ----------------------------------------------------------------------------
s32 CMpegBase::OpenLayer()
{
    if (mucLayerValue == 1)
    {
        miDecodeSize = 768 * mucChannelCount;
    }
    else
    {
        miDecodeSize = 2304 * mucChannelCount;
        if (mucLayerValue == 3 && mucIsLsf)
            miDecodeSize = (2304 * mucChannelCount) >> 1;
    }

    mucSynthPhase0 = 1;
    mucSynthPhase1 = 1;
    return 0;
}

// ----------------------------------------------------------------------------
// OpenSynth @0x82B749C0
// Allocate the poly-phase synthesis history block (2304 bytes per channel) through
// the shared Snd allocator hook, store it at mpPolySynthHistory and zero it. When
// the global synth-alloc guard is set the whole thing is skipped. The X360 leaves
// the buffer (or `this`, on the skip path) in r3; callers ignore it.
// ----------------------------------------------------------------------------
void* CMpegBase::OpenSynth()
{
    if (dword_83277C9C)
        return this;

    const u32 luSize = 2304u * mucChannelCount;
    void* lpHistory = dword_83277830(luSize);
    mpPolySynthHistory = lpHistory;
    return std::memset(lpHistory, 0, 2304u * mucChannelCount);
}

// ----------------------------------------------------------------------------
// Open @0x82B7E6F0
// Open a byte stream. If one is already open, Close it first (virtual). Latch the
// stream pointer, read+process the first header, allocate the synth history, size
// the output (virtual OpenLayer) and reset the bit reader. Returns 0 on success,
// -1 on a null stream or a malformed first header.
// ----------------------------------------------------------------------------
s32 CMpegBase::Open(u8* apData)
{
    if (mucIsOpen)
        Close();

    mpBitPointer = apData;
    mpStreamStart = apData;
    mucIsOpen = 1;

    if (!apData || GetHeader())
        return -1;

    OpenSynth();
    OpenLayer();

    mpFrameStart = mpBitPointer;
    muBitBuffer = 0;
    miBitsAvailable = 0;
    return 0;
}

// ----------------------------------------------------------------------------
// Seek @0x82B749A0
// Reposition the bit reader onto `apPos` (cursor + both saved copies) and clear
// the bit accumulator. Returns `this`.
// ----------------------------------------------------------------------------
CMpegBase* CMpegBase::Seek(u8* apPos)
{
    mpBitPointer = apPos;
    mpStreamStart = apPos;
    mpFrameStart = apPos;
    muBitBuffer = 0;
    miBitsAvailable = 0;
    return this;
}

} // namespace Snd
