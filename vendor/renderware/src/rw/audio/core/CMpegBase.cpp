// =====================================================================================
// rw::audio::core::CMpegBase bodies -- the shared rwaudio MPEG-audio decoder base.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every offset, width and side-effect. No Feb-2007 source and no DecFIGS
// DWARF exist for this TU. See CMpegBase.h for the byte-exact layout and Decoder.h /
// PlugIn.h for the shared rwaudio System allocator.
//   AllocateSynth              @0x82B8BFF0
//   GetBits                    @0x82B8AAE8
//   GetHeader                  @0x82B8F3F0
//   ProcessHeader              @0x82B8F128
//   `vector deleting destructor'@0x82B94FB0
//   PolySynth                  @0x82B8EE50 -- BLOCKED, NOT reconstructed here (see below);
//                                only declared in CMpegBase.h.
//
// PolySynth needs the un-recovered poly-phase synthesis-window coefficient rodata
// (unk_82159D70 / unk_82159DB0) and the un-homed matrixing helper sub_82B8E258, and its
// history-buffer addressing (the +0x38 rotating phase into the 2304-byte-per-channel block
// at +0x4C) cannot be grounded without them. Reconstructing it would mean fabricating that
// float table and the helper's contract, so it is left to its own TU -- an honest gap.
//
// The `_savefpr_17` / `_restfpr_17` and the `twllei` / `twlgei` in the pseudocode are the
// compiler's FP register save/restore helpers and its divide-by-zero / overflow trap
// instrumentation around `divw` -- not source-level calls -- so they are dropped.
// =====================================================================================

#include "rw/audio/core/CMpegBase.h"
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
// reached from Decoder.cpp / Collection.cpp.
extern "C" System *off_83271928;

// off_8215A8E8 -- the CMpegBase v-table reinstalled by the deleting destructor before any
// free. An opaque data symbol in the XEX (its function-pointer contents are not exported);
// modelled as an honest placeholder so the body links without fabricating the table.
static void *const KMPG_VTable = nullptr; // &off_8215A8E8

// The MPEG frame-header lookup tables (off_8215A66C / off_8215A680 in the X360 rodata).
// ProcessHeader only *indexes* them -- their contents (the standard MPEG sample-rate Hz and
// bitrate-kbps values) live in the audio rodata TU, so they are referenced as externs and
// never fabricated here, exactly as Xas1Dec references its off_82F8A544 descriptor. The
// sample-rate table is read with `lhzx` (unsigned u16); the bitrate table with `lhax`
// (signed s16).
extern "C" const u16 word_8215A66C[]; // sample-rate table (Hz), indexed by mucSampleRateIndex
extern "C" const s16 word_8215A680[]; // bitrate table (kbps), indexed by the derived slot

// XMemSet @ the X360 build = byte-wise memset; reproduced with std::memset.
inline void XMemSet(void *dst, int value, u32 bytes)
{
    std::memset(dst, value, bytes);
}
} // namespace

// -------------------------------------------------------------------------------------
// AllocateSynth @0x82B8BFF0
// Allocate the poly-phase synthesis history block (2304 bytes per channel) through the
// shared rwaudio System allocator, store it at mpPolySynthHistory and zero it. Returns 0
// on success, -1 when the allocation fails. (The pointer is stored unconditionally, before
// the null check, matching the asm.)
// -------------------------------------------------------------------------------------
s32 CMpegBase::AllocateSynth(s32 iChannelCount)
{
    const u32 uSize = 2304u * static_cast<u32>(iChannelCount);

    void *pHistory = System::Alloc(off_83271928, uSize, "PolySynthHistoryF", 16, 0);
    mpPolySynthHistory = pHistory;

    if (!pHistory)
        return -1;

    XMemSet(pHistory, 0, uSize);
    return 0;
}

// -------------------------------------------------------------------------------------
// GetBits @0x82B8AAE8
// Read `iNumBits` bits MSB-first from the byte stream. The accumulator (muBitBuffer) is
// left-justified: bytes are shifted into the top as they are consumed, and the result is
// taken from the high `iNumBits` bits, then the accumulator is shifted up by that many.
// -------------------------------------------------------------------------------------
u32 CMpegBase::GetBits(s32 iNumBits)
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
// GetHeader @0x82B8F3F0
// Fetch the next 32-bit frame header -- big-endian from the byte cursor when it is set,
// otherwise the pre-stored muHeaderWord -- and process it. Returns 0 on a valid header,
// -1 on a malformed one.
// -------------------------------------------------------------------------------------
s32 CMpegBase::GetHeader()
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
// ProcessHeader @0x82B8F128
// Decode a 32-bit MPEG sync word into the header fields, then derive the sample rate,
// bitrate and frame size for the frame. Returns the samples-per-frame count (384 for
// Layer I, 1152 for Layer II / III MPEG-1, 576 for Layer III LSF), or -1 on an invalid
// header. (`mucLayerValue` holds 4 - the layer id, so 1 => Layer I .. 3 => Layer III.)
// -------------------------------------------------------------------------------------
s32 CMpegBase::ProcessHeader(u32 uHeader)
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

    const u32 uSampleRate = word_8215A66C[mucSampleRateIndex];
    muSampleRate = uSampleRate;
    muSampleRateCopy = uSampleRate;

    // "free format" bitrate index (0) is not supported here.
    if (ucBitRateIndex == 0)
        return -1;

    const u8 ucLsf = mucIsLsf;
    const s32 iBitRateSlot =
        48 * ucLsf + 16 * ucLayerValue - 16 + ucBitRateIndex;
    const s32 iBitRate = word_8215A680[iBitRateSlot];
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

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82B94FB0
// Reinstall the CMpegBase v-table, free the poly-synth history block through the shared
// rwaudio System allocator, and -- when bit 0 of `flags` is set -- delete `self`.
// -------------------------------------------------------------------------------------
void *CMpegBase::VectorDeletingDestructor(CMpegBase *self, char flags)
{
    void *pHistory = self->mpPolySynthHistory;
    self->mpVTable = KMPG_VTable; // &off_8215A8E8

    if (pHistory)
        System::Free(off_83271928, pHistory, 0);

    if ((flags & 1) != 0)
        ::operator delete(self);

    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
