// =====================================================================================
// rw::audio::core::HELPER_CMpegBase bodies -- the EALayer3 codec family's MPEG-audio
// decoder base (distinct from the sibling rw::audio::core::CMpegBase; see the header).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every offset, width and side-effect. No Feb-2007 source and no DecFIGS
// DWARF exist for this TU. See HELPER_CMpegBase.h for the byte-exact layout and PlugIn.h
// for the shared rwaudio System allocator.
//   Open                       @0x82B92168
//   Close                      @0x82B86430
//   Seek                       @0x82B860A8
//   GetBits                    @0x82B85FE0
//   GetHeader                  @0x82B86378
//   OpenSynth                  @0x82B860C8
//   ProcessHeader              @0x82B86150
//   ~HELPER_CMpegBase          (vector deleting destructor @0x82B92100)
//   PolySynth                  @0x82B87080 -- BLOCKED, NOT reconstructed here (see below);
//                                only declared in HELPER_CMpegBase.h.
//
// PolySynth needs the un-recovered poly-phase synthesis-window coefficient rodata
// (unk_82156740 / unk_82156780) and the un-homed matrixing helper sub_82B86488, and its
// history-buffer addressing (the +0x2C rotating phase into the 2304-byte-per-channel block
// at +0x44) cannot be grounded without them. Reconstructing it would mean fabricating that
// float table and the helper's contract, so it is left to its own TU -- an honest gap.
//
// The `_savefpr_17` / `_restfpr_17` and the `twllei` / `twlgei` seen in the pseudocode of
// the sibling functions are the compiler's FP register save/restore helpers and its
// divide-by-zero / overflow trap instrumentation around `divw` -- not source-level calls --
// so they are dropped.
// =====================================================================================

#include "rw/audio/core/HELPER_CMpegBase.h"
#include "rw/audio/core/PlugIn.h" // rw::audio::core::System (Alloc / Free)

#include <cstring> // std::memset (the X360 XMemSet)

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
// The shared rwaudio System singleton (off_83271928). Its object is defined/owned by the
// System allocator TU; here it is the target for System::Alloc / System::Free. Same global
// reached from CMpegBase.cpp / Decoder.cpp.
extern "C" System *off_83271928;

// off_8327A300 -- a global "poly-synth disabled / externally provided" flag. When non-zero
// OpenSynth skips its per-decoder history allocation entirely. Read-only in this TU; owned
// elsewhere, so referenced as an honest extern rather than fabricated.
extern "C" int dword_8327A300;

// The MPEG frame-header lookup tables (word_82156718 / word_82156658 in the X360 rodata).
// ProcessHeader only *indexes* them -- their contents (the standard MPEG sample-rate Hz and
// bitrate-kbps values) live in the audio rodata TU, so they are referenced as externs and
// never fabricated here. The sample-rate table is read with `lhzx` (unsigned u16); the
// bitrate table with `lhax` (signed s16).
extern "C" const u16 word_82156718[]; // sample-rate table (Hz), indexed by mucSampleRateIndex
extern "C" const s16 word_82156658[]; // bitrate table (kbps), indexed by the derived slot

// XMemSet @ the X360 build = byte-wise memset; reproduced with std::memset.
inline void XMemSet(void *dst, int value, u32 bytes)
{
    std::memset(dst, value, bytes);
}
} // namespace

// -------------------------------------------------------------------------------------
// ~HELPER_CMpegBase (vector deleting destructor @0x82B92100)
// The compiler emits the vector deleting destructor thunk from this virtual destructor:
// on entry the vptr is reinstalled to the base vtable (off_8215A898), the body then calls
// Close() when the decoder is still open, and the thunk conditionally frees `this`.
// -------------------------------------------------------------------------------------
HELPER_CMpegBase::~HELPER_CMpegBase()
{
    if (mucIsOpen)
        Close();
}

// -------------------------------------------------------------------------------------
// Open @0x82B92168
// (Re-)open the decoder over the caller's encoded byte buffer. Any prior session is closed
// first; the cursor and its reset copy are armed and the open flag is set before the empty
// -buffer guard, matching the asm. On success the open hook runs and the bit reader is
// reset to the frame start.
// -------------------------------------------------------------------------------------
s32 HELPER_CMpegBase::Open(u8 *pData)
{
    if (mucIsOpen)
        Close();

    mpBitPointer = pData;
    mpDataStart = pData;
    mucIsOpen = 1;

    if (!pData || GetHeader() != 0 || OpenSynth() < 0)
        return -1;

    OnOpen(); // vtable slot +0x10

    mpFrameStart = mpBitPointer;
    muBitBuffer = 0;
    miBitsAvailable = 0;
    return 0;
}

// -------------------------------------------------------------------------------------
// Close @0x82B86430
// Tear down an open session: read the poly-synth history pointer, clear the pre-read header
// word and the open flag, then free the history block (when present) through the shared
// rwaudio System allocator. A no-op (returns 0) when already closed.
// -------------------------------------------------------------------------------------
s32 HELPER_CMpegBase::Close()
{
    if (mucIsOpen)
    {
        void *pHistory = mpPolySynthHistory;
        muHeaderWord = 0;
        mucIsOpen = 0;

        if (pHistory)
            System::Free(off_83271928, pHistory, 0);
    }
    return 0;
}

// -------------------------------------------------------------------------------------
// Seek @0x82B860A8
// Re-point the bit reader at `pData` (cursor, data-start and frame-start all reset to it)
// and clear the accumulator and bit count.
// -------------------------------------------------------------------------------------
void HELPER_CMpegBase::Seek(u8 *pData)
{
    mpBitPointer = pData;
    mpDataStart = pData;
    mpFrameStart = pData;
    muBitBuffer = 0;
    miBitsAvailable = 0;
}

// -------------------------------------------------------------------------------------
// GetBits @0x82B85FE0
// Read `iNumBits` bits MSB-first from the byte stream. The accumulator (muBitBuffer) is
// left-justified: bytes are shifted into the top as they are consumed, and the result is
// taken from the high `iNumBits` bits, then the accumulator is shifted up by that many.
// -------------------------------------------------------------------------------------
u32 HELPER_CMpegBase::GetBits(s32 iNumBits)
{
    while (miBitsAvailable < iNumBits)
    {
        muBitBuffer |= static_cast<u32>(*mpBitPointer) << (24 - miBitsAvailable);
        ++mpBitPointer;
        miBitsAvailable += 8;
    }

    const u32 uBits = muBitBuffer;
    miBitsAvailable -= iNumBits;
    muBitBuffer = uBits << iNumBits;
    return uBits >> (32 - iNumBits);
}

// -------------------------------------------------------------------------------------
// GetHeader @0x82B86378
// Fetch the next 32-bit frame header -- big-endian from the byte cursor when it is set,
// otherwise the pre-stored muHeaderWord -- and process it (virtual: dispatched through
// vtable+0x04). Returns 0 on a valid header, -1 on a malformed one.
// -------------------------------------------------------------------------------------
s32 HELPER_CMpegBase::GetHeader()
{
    u32 uHeader;
    if (mpBitPointer)
    {
        uHeader = (static_cast<u32>(mpBitPointer[0]) << 24)
                | (static_cast<u32>(mpBitPointer[1]) << 16)
                | (static_cast<u32>(mpBitPointer[2]) << 8)
                |  static_cast<u32>(mpBitPointer[3]);
    }
    else
    {
        uHeader = muHeaderWord;
    }

    return (ProcessHeader(uHeader) == -1) ? -1 : 0;
}

// -------------------------------------------------------------------------------------
// OpenSynth @0x82B860C8
// Allocate the poly-phase synthesis history block (2304 bytes per channel) through the
// shared rwaudio System allocator, store it at mpPolySynthHistory and zero it -- unless the
// global poly-synth-disabled flag is set, in which case nothing is allocated. Returns 0 on
// success, -1 when the allocation fails. (The pointer is stored unconditionally, before the
// null check, matching the asm.)
// -------------------------------------------------------------------------------------
s32 HELPER_CMpegBase::OpenSynth()
{
    if (!dword_8327A300)
    {
        const u32 uSize = 2304u * mucChannelCount;

        void *pHistory = System::Alloc(off_83271928, uSize, "PolySynthHistoryF", 16, 0);
        mpPolySynthHistory = pHistory;

        if (!pHistory)
            return -1;

        XMemSet(pHistory, 0, uSize);
    }
    return 0;
}

// -------------------------------------------------------------------------------------
// ProcessHeader @0x82B86150
// Decode a 32-bit MPEG sync word into the header fields, then derive the sample rate,
// bitrate and frame size for the frame. Returns the samples-per-frame count (384 for
// Layer I, 1152 for Layer II / III MPEG-1, 576 for Layer III LSF), or -1 on an invalid
// header. (`mucLayerValue` holds 4 - the layer id, so 1 => Layer I .. 3 => Layer III.)
// -------------------------------------------------------------------------------------
s32 HELPER_CMpegBase::ProcessHeader(u32 uHeader)
{
    // 11-bit frame sync must be all ones.
    if ((uHeader & 0xFFE00000u) != 0xFFE00000u)
        return -1;

    const u8 ucLayerValue = static_cast<u8>(4 - ((uHeader >> 17) & 3));
    const u8 ucVersionLow = static_cast<u8>((uHeader >> 19) & 1);
    const u8 ucBitRateIndex = static_cast<u8>((uHeader >> 12) & 0xF);
    const u8 ucPadding = static_cast<u8>((uHeader >> 9) & 1);

    mucProtectionBit = static_cast<u8>((uHeader >> 16) & 1);
    mucVersionLowBit = ucVersionLow;
    mucLayerValue = ucLayerValue;
    mucBitRateIndex = ucBitRateIndex;
    mucPaddingBit = ucPadding;
    mucMode = static_cast<u8>((uHeader >> 6) & 3);
    mucModeExtension = static_cast<u8>((uHeader >> 4) & 3);
    mucCopyright = static_cast<u8>((uHeader >> 3) & 1);
    mucOriginal = static_cast<u8>((uHeader >> 2) & 1);

    // Reserved layer id, or reserved bitrate index => invalid.
    if (ucLayerValue == 4 || ucBitRateIndex == 0xF)
        return -1;

    // MPEG version -> MPEG-2.5 / low-sampling-frequency flags (version bit 20).
    bool bLsf;
    if ((uHeader & 0x100000u) != 0)
    {
        mucIsMpeg25 = 0;
        bLsf = (ucVersionLow == 0);
    }
    else
    {
        mucIsMpeg25 = 1;
        bLsf = true;
    }
    mucIsLsf = bLsf ? 1 : 0;

    // Sample-rate table index (combined MPEG-1 / 2 / 2.5 space).
    const u8 ucSampleRateRaw = static_cast<u8>((uHeader >> 10) & 3);
    if (mucIsMpeg25)
    {
        mucSampleRateIndex = static_cast<u8>(ucSampleRateRaw + 6);
    }
    else
    {
        const u8 ucLsf = mucIsLsf;
        mucSampleRateIndex = static_cast<u8>(3 * ucLsf + ucSampleRateRaw);
        mucSampleRateIndex2 = static_cast<u8>((ucLsf == 0 ? 3 : 0) + ucSampleRateRaw);
    }

    // Mono when channel mode == 3, stereo otherwise.
    mucChannelCount = static_cast<u8>((mucMode == 3) ? 1 : 2);

    const u32 uSampleRate = word_82156718[mucSampleRateIndex];
    muSampleRate = uSampleRate;
    muSampleRateCopy = uSampleRate;

    // "free format" bitrate index (0) is not supported here.
    if (ucBitRateIndex == 0)
        return -1;

    const u8 ucLsf = mucIsLsf;
    const s32 iBitRateSlot =
        48 * ucLsf + 16 * ucLayerValue - 16 + ucBitRateIndex;
    const s32 iBitRate = word_82156658[iBitRateSlot];
    miBitRate = iBitRate;

    s32 iResult;
    if (ucLayerValue == 1) // Layer I
    {
        miFrameSize =
            4 * (12000 * iBitRate / static_cast<s32>(muSampleRate) + ucPadding);
        iResult = 384;
    }
    else // Layer II / III
    {
        const s32 iFrameSize = 144000 * iBitRate / static_cast<s32>(muSampleRate);
        miFrameSize = iFrameSize;
        iResult = 1152;

        if (ucLayerValue == 3 && bLsf) // Layer III LSF
        {
            iResult = 576;
            miFrameSize = iFrameSize >> 1;
        }

        if (ucPadding)
            ++miFrameSize;
    }

    miFrameSize -= 4;
    return iResult;
}

} // namespace core
} // namespace audio
} // namespace rw
