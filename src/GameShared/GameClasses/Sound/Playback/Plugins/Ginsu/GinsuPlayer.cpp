// =====================================================================================
// rw::audio::core::GinsuPlayer / GinsuSynthData bodies -- the granular engine-sound
// synthesizer ("Gns0"), the heart of Burnout's car-engine audio.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// store/branch. Decode report progress/scratch_dossiers/ginsuplayer_decode_codex.md.
// See GinsuPlayer.h for the layout and the per-function console addresses.
//
// HOW IT WORKS, in one paragraph: the bound "Gnsu2" content is an engine recording swept
// across an RPM range, stored as a predictive-coded stream (19 bytes per 32 samples) plus a
// FREQUENCY table (which sample index corresponds to which engine frequency) and a CYCLE
// table (where each waveform cycle starts). Process maps the requested frequency to a
// sample position, streams from there, and every ~10 ms JUMPS to a deterministically-chosen
// nearby cycle so a short recording never audibly loops -- crossfading over 0.5 ms and
// always landing on a cycle boundary so the jump is phase-continuous. Everything is then
// resampled to the mixer's rate with a 16.16 phase accumulator.
// =====================================================================================

#include "GameShared/GameClasses/Sound/Playback/Plugins/Ginsu/GinsuPlayer.h"

#include "rw/audio/core/Mixer.h" // Mixer (the process context) + SampleBuffer
#include "rw/audio/core/Voice.h" // VoiceStageConfig (GetSize's config argument)

#include <cstdio>  // std::printf (the console's GetSamples failure diagnostic)
#include <cstring> // std::memcpy / std::memcmp / std::memset

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
    // ---- rodata, every value re-read big-endian from the decrypted XEX -----------------
    const f32 KF_ZERO = 0.0f;                 // flt_82001CC0
    const f32 KF_ONE = 1.0f;                  // flt_82001C98
    const f32 KF_TWO = 2.0f;                  // flt_82001D9C
    const f32 KF_HALF = 0.5f;                 // flt_82001DA0
    const f32 KF_CYCLES_PER_MINUTE = 120.0f;  // flt_82004A28 -- frequency -> samples/cycle
    const f32 KF_DEFAULT_FREQUENCY = 1000.0f; // flt_82009E10
    const f32 KF_MAX_INPUT_CHUNK = 64.0f;     // flt_82020A7C -- the steady-state input cap
    const f32 KF_INV_65536 = 1.0f / 65536.0f; // flt_8207ACF0 -- the 16.16 resample fraction
    const f32 KF_NO_JUMP_SECONDS = 0.01099999994f;   // flt_820AA7BC
    const f32 KF_DEFAULT_SAMPLE_RATE = 48000.0f;     // flt_820AA808
    const f32 KF_INV_2_POW_31 = 4.65661287e-10f;     // flt_820ADB88 (2^-31) -- RNG normalise
    const f32 KF_SAMPLE_SCALE = 1.0f / 32768.0f;     // flt_820ADBE8 -- decoded sample scale
    const f64 KD_JUMP_INTERVAL = 0.0099999997764825821; // dbl_820ADBF0 -- ~10 ms between jumps
    const f32 KF_OVERLAP_SECONDS = 0.00050000002374872565f; // flt_820ADBF8 -- 0.5 ms crossfade

    enum { KI_RNG_XOR = 0x1D872B41, KI_RNG_MODULUS = 0x7FFFFFFF, KI_RNG_SEED = 0x12345678 };

    // The two predictor coefficient rows (rodata off_82F2E218 / off_82F2E228). Each encoded
    // record picks one pair by the low nibble of its byte 0.
    const f32 KAF_COEF0[4] = { 0.0f, 0.9375f, 1.796875f, 1.53125f };
    const f32 KAF_COEF1[4] = { 0.0f, 0.0f, -0.8125f, -0.859375f };

    // The 256-entry nibble codebook at off_82F2E238 (0x400 bytes). Every one of its raw
    // big-endian words matches the closed form
    //     codebook[shift][nibble] = signed4(nibble) * 2^(12 - shift)   for shift 0..12
    //     codebook[shift][nibble] = 0                                   for shift 13..15
    // (verified word-for-word against the image), so it is COMPUTED here rather than
    // transcribed as 256 literals -- the recovered rule is the faithful artifact, and a
    // table of magic floats would be less checkable, not more.
    f32 CodebookEntry(unsigned auShift, unsigned auNibble)
    {
        if (auShift > 12u)
            return KF_ZERO;                                  // rows 13..15 are all zero
        const int liSigned = (auNibble < 8u) ? static_cast<int>(auNibble)
                                             : static_cast<int>(auNibble) - 16;
        f32 lfScale = 1.0f;                                  // 2^(12 - shift), exactly
        for (unsigned luBit = 0; luBit < (12u - auShift); ++luBit)
            lfScale *= 2.0f;
        return static_cast<f32>(liSigned) * lfScale;
    }

    // PPC fctiwz: truncate toward zero. CONSOLE-SEMANTICS NOTE: fctiwz saturates on
    // NaN/out-of-range where a C++ cast is undefined; over this plug-in's real domain
    // (finite rates, in-range sample indices) the two agree exactly.
    inline int TruncToInt(f32 afValue) { return static_cast<int>(afValue); }
    inline int TruncToInt(f64 adValue) { return static_cast<int>(adValue); }

    // FloorToInt @0x8268B530 -- a local helper: truncate, then step down for a negative
    // non-integral input.
    int FloorToInt(f32 afValue)
    {
        const int liTrunc = TruncToInt(afValue);
        return (afValue < KF_ZERO && static_cast<f32>(liTrunc) != afValue) ? liTrunc - 1
                                                                           : liTrunc;
    }

    int CeilToInt(f32 afValue)
    {
        const int liTrunc = TruncToInt(afValue);
        return (afValue > KF_ZERO && static_cast<f32>(liTrunc) != afValue) ? liTrunc + 1
                                                                           : liTrunc;
    }

    // The console's round-half-away-from-zero on an interpolated sample index.
    int RoundToInt(f32 afValue)
    {
        return TruncToInt(afValue + (afValue > KF_ZERO ? KF_HALF : -KF_HALF));
    }

    inline size_t AlignUp8(size_t auValue) { return (auValue + 7u) & ~static_cast<size_t>(7u); }

    // Serialized-blob readers. The blob is byte-swapped IN PLACE to native order the first
    // time it is bound (see AdjustEndianness), so after that these are native reads.
    inline s32 ReadNativeI32(const u8 *apBytes)
    {
        s32 lValue;
        std::memcpy(&lValue, apBytes, sizeof(lValue));
        return lValue;
    }
    inline f32 ReadNativeF32(const u8 *apBytes)
    {
        f32 lValue;
        std::memcpy(&lValue, apBytes, sizeof(lValue));
        return lValue;
    }
    inline u16 ReadNativeU16(const u8 *apBytes)
    {
        u16 lValue;
        std::memcpy(&lValue, apBytes, sizeof(lValue));
        return lValue;
    }
    inline void WriteNativeU16(u8 *apBytes, u16 auValue)
    {
        std::memcpy(apBytes, &auValue, sizeof(auValue));
    }
    inline void ReverseFourBytes(u8 *apBytes)
    {
        const u8 lu0 = apBytes[0], lu1 = apBytes[1], lu2 = apBytes[2], lu3 = apBytes[3];
        apBytes[0] = lu3; apBytes[1] = lu2; apBytes[2] = lu1; apBytes[3] = lu0;
    }

    // AdjustEndianness @0x8268B1D0 -- byte-reverse the header's six numeric fields and both
    // index tables IN PLACE, then set the "already done" flag at +0x06 so it happens once.
    //
    // FLAG PC-platform note: on the big-endian console this converts the file's LITTLE-endian
    // authored data to native. The tool-side authoring order is what the bytes are, so on the
    // little-endian host the same reversal would UN-swap correct data. The swap is therefore
    // performed only when the blob still carries the not-yet-adjusted marker, exactly as the
    // console gates it -- content authored for this build arrives already native and simply
    // never enters here. The body is reproduced faithfully so console-order content is still
    // handled if it appears.
    void *AdjustEndianness(void *apGinFile)
    {
        u8 *lpBytes = static_cast<u8 *>(apGinFile);
        for (size_t luOffset = 8; luOffset != KI_GINSU_HEADER_BYTES; luOffset += 4)
            ReverseFourBytes(lpBytes + luOffset);
        // segCount (+0x10) + cycleCount (+0x14) + 2 index words follow the header.
        const int liWords = ReadNativeI32(lpBytes + 0x14) + ReadNativeI32(lpBytes + 0x10) + 2;
        for (int liWord = 0; liWord < liWords; ++liWord)
            ReverseFourBytes(lpBytes + KI_GINSU_HEADER_BYTES + 4 * liWord);
        WriteNativeU16(lpBytes + 6, 1);
        return apGinFile;
    }

    bool HasGinsuMagic(const u8 *apBytes)
    {
        return std::memcmp(apBytes, "Gnsu2", 5) == 0;
    }
}

// =====================================================================================
//  GinsuSynthData
// =====================================================================================

// -------------------------------------------------------------------------------------
// GetTotalTableSize @0x8268B308 -- how many bytes of instance tail the two copied index
// tables need. NOTE the side effect: it endian-adjusts the blob if that has not happened.
// -------------------------------------------------------------------------------------
u32 GinsuSynthData::GetTotalTableSize(void *apGinFile)
{
    u8 *lpBytes = static_cast<u8 *>(apGinFile);
    if (!HasGinsuMagic(lpBytes))
        return 0;
    if (ReadNativeU16(lpBytes + 6) == 0)
        lpBytes = static_cast<u8 *>(AdjustEndianness(lpBytes));
    return 4u * static_cast<u32>(ReadNativeI32(lpBytes + 0x14)     // cycleCount
                                 + ReadNativeI32(lpBytes + 0x10)   // segCount
                                 + 2);
}

// -------------------------------------------------------------------------------------
// BindToData @0x8268B398 -- adopt a "Gnsu2" blob: copy its two index tables into the
// instance's allocation tail, point mSampleData at the encoded stream, and precompute the
// smallest cycle period (which SampleToCycle's search uses to bound its steps).
// -------------------------------------------------------------------------------------
bool GinsuSynthData::BindToData(void *apGinFile, void *apTableStorage)
{
    mMinFrequency = KF_ZERO;
    mMaxFrequency = KF_ZERO;
    mSegCount = 0;
    mCycleCount = 0;
    mSampleCount = 0;
    mSampleRate = 0;

    u8 *lpBytes = static_cast<u8 *>(apGinFile);
    if (!HasGinsuMagic(lpBytes))
        return false;
    if (ReadNativeU16(lpBytes + 6) == 0)
        lpBytes = static_cast<u8 *>(AdjustEndianness(lpBytes));

    mMinFrequency = ReadNativeF32(lpBytes + 0x08);
    mMaxFrequency = ReadNativeF32(lpBytes + 0x0C);
    mSegCount     = ReadNativeI32(lpBytes + 0x10);
    mCycleCount   = ReadNativeI32(lpBytes + 0x14);
    mSampleCount  = ReadNativeI32(lpBytes + 0x18);
    mSampleRate   = ReadNativeI32(lpBytes + 0x1C);

    const size_t luFreqBytes = 4u * static_cast<size_t>(mSegCount + 1);
    const size_t luCycleBytes = 4u * static_cast<size_t>(mCycleCount + 1);
    u8 *lpStorage = static_cast<u8 *>(apTableStorage);

    // Both tables are reached through a RELATIVE offset from this object, never a stored
    // pointer -- which is what keeps them correct once the instance widens on the host.
    mFreqOffset = static_cast<size_t>(lpStorage - reinterpret_cast<u8 *>(this));
    std::memcpy(lpStorage, lpBytes + KI_GINSU_HEADER_BYTES, luFreqBytes);
    mCycleOffset =
        static_cast<size_t>((lpStorage + luFreqBytes) - reinterpret_cast<u8 *>(this));
    std::memcpy(lpStorage + luFreqBytes,
                lpBytes + KI_GINSU_HEADER_BYTES + luFreqBytes, luCycleBytes);

    mSampleData = lpBytes + KI_GINSU_HEADER_BYTES + luFreqBytes + luCycleBytes;
    mCurrentBlock = -1;

    // The smallest adjacent cycle-table delta, seeded with the whole sample count.
    const s32 *lpCycle = CycleTable();
    int liMinPeriod = mSampleCount;
    for (int liCycle = 0; liCycle < mCycleCount; ++liCycle)
    {
        const int liPeriod = lpCycle[liCycle + 1] - lpCycle[liCycle];
        if (liPeriod < liMinPeriod)
            liMinPeriod = liPeriod;
    }
    mMinPeriod = static_cast<f32>(liMinPeriod);
    return true;
}

// -------------------------------------------------------------------------------------
// DecodeBlock @0x8268B588 -- expand one 19-byte record into 32 float samples.
//
// The record: bytes 0..3 carry two 12-bit initial samples plus the predictor and shift
// nibbles; bytes 4..18 hold 30 residual nibbles. Each output is a two-tap prediction from
// the previous two samples plus a codebook residual. The console unrolls this three ways
// over five bytes at a time; the loop below is algebraically identical and keeps the
// load/multiply/add order of the recurrence.
//
// NOTE there is NO sign extension of the two initial samples -- the recurrence runs in
// float from whatever those 12-bit values are, exactly as the console does.
// -------------------------------------------------------------------------------------
void GinsuSynthData::DecodeBlock(int aiBlock, bool abUseOldData, System * /*apSystem*/)
{
    mCurrentBlock = aiBlock;

    // The crossfade path decodes out of the look-back cache, which holds eight records
    // starting at mOldDataBlockIndex; everything else decodes from the live stream.
    const u8 *lpRecord = abUseOldData
        ? (mOldDataBlock + KI_GINSU_RECORD_BYTES * (aiBlock - mOldDataBlockIndex))
        : (mSampleData + KI_GINSU_RECORD_BYTES * aiBlock);

    mSample[0] = static_cast<f32>((static_cast<unsigned>(lpRecord[1]) << 8)
                                  | (lpRecord[0] & 0xF0u));
    mSample[1] = static_cast<f32>((static_cast<unsigned>(lpRecord[3]) << 8)
                                  | (lpRecord[2] & 0xF0u));

    const unsigned luPredictor = lpRecord[0] & 0x0Fu;
    const unsigned luShift = lpRecord[2] & 0x0Fu;
    const f32 lfCoef0 = KAF_COEF0[luPredictor & 3u];
    const f32 lfCoef1 = KAF_COEF1[luPredictor & 3u];

    int liOut = 2;
    for (int liByte = 4; liByte < KI_GINSU_RECORD_BYTES; ++liByte)
    {
        const unsigned lauCodes[2] = { static_cast<unsigned>(lpRecord[liByte] >> 4),
                                       static_cast<unsigned>(lpRecord[liByte] & 0x0Fu) };
        for (int liHalf = 0; liHalf < 2; ++liHalf)
        {
            mSample[liOut] = mSample[liOut - 1] * lfCoef0
                           + mSample[liOut - 2] * lfCoef1
                           + CodebookEntry(luShift, lauCodes[liHalf]);
            ++liOut;
        }
    }
}

// -------------------------------------------------------------------------------------
// FrequencyToSample @0x8268B7B8 -- map an engine frequency to a sample index through the
// segment table, linearly interpolating between adjacent entries.
// -------------------------------------------------------------------------------------
int GinsuSynthData::FrequencyToSample(f32 afFrequency) const
{
    if (mSegCount < 1)
        return 0;
    const s32 *lpTable = FrequencyTable();
    if (afFrequency <= mMinFrequency)
        return lpTable[0];
    if (afFrequency >= mMaxFrequency)
        return lpTable[mSegCount];

    const f32 lfPosition = (afFrequency - mMinFrequency) * static_cast<f32>(mSegCount)
                         / (mMaxFrequency - mMinFrequency);
    const int liIndex = FloorToInt(lfPosition);
    const f32 lfSample = static_cast<f32>(lpTable[liIndex])
                       + static_cast<f32>(lpTable[liIndex + 1] - lpTable[liIndex])
                             * (lfPosition - static_cast<f32>(liIndex));
    return RoundToInt(lfSample);
}

// -------------------------------------------------------------------------------------
// CycleToSample @0x8268B8F8 -- the cycle-table counterpart (no dossier; the decode
// recovered it from the raw XEX at file 0x0068E8F8).
// -------------------------------------------------------------------------------------
int GinsuSynthData::CycleToSample(f32 afCycle) const
{
    if (mCycleCount < 1)
        return 0;
    const s32 *lpTable = CycleTable();
    if (afCycle <= KF_ZERO)
        return lpTable[0];
    if (afCycle >= static_cast<f32>(mCycleCount))
        return lpTable[mCycleCount];

    const int liIndex = FloorToInt(afCycle);
    const f32 lfSample = static_cast<f32>(lpTable[liIndex])
                       + static_cast<f32>(lpTable[liIndex + 1] - lpTable[liIndex])
                             * (afCycle - static_cast<f32>(liIndex));
    return RoundToInt(lfSample);
}

// -------------------------------------------------------------------------------------
// CyclePeriod @0x8268BA20 -- the local waveform period at a (fractional) cycle position.
//
// It interpolates CENTRED periods: at an interior integer cycle i the left endpoint is the
// mean of the periods either side of i and the right endpoint the mean of the periods
// either side of i+1, so the result is smooth across cycle boundaries. The two edges
// degrade to the single outermost period.
//
// ⚠️ The left-edge arm reads T[2], so valid content must carry at least two cycles; the
// console has no defensive case for mCycleCount == 1 and neither does this.
// -------------------------------------------------------------------------------------
f32 GinsuSynthData::CyclePeriod(f32 afCycle) const
{
    const int liCount = mCycleCount;
    if (liCount < 1)
        return KF_ZERO;

    const s32 *lpTable = CycleTable();
    int liIndex = FloorToInt(afCycle);
    f32 lfLeft;
    f32 lfRight;

    if (liIndex < 1)
    {
        if (liIndex < 0) { liIndex = 0; afCycle = KF_ZERO; }
        const f32 lfP0 = static_cast<f32>(lpTable[1] - lpTable[0]);
        const f32 lfP1 = static_cast<f32>(lpTable[2] - lpTable[1]);
        lfLeft = lfP0;
        lfRight = KF_HALF * (lfP0 + lfP1);
    }
    else if (liIndex >= liCount - 1)
    {
        if (liIndex >= liCount) { liIndex = liCount - 1; afCycle = static_cast<f32>(liCount); }
        const f32 lfPrev = static_cast<f32>(lpTable[liCount - 1] - lpTable[liCount - 2]);
        const f32 lfLast = static_cast<f32>(lpTable[liCount] - lpTable[liCount - 1]);
        lfLeft = KF_HALF * (lfPrev + lfLast);
        lfRight = lfLast;
    }
    else
    {
        const f32 lfPrev = static_cast<f32>(lpTable[liIndex] - lpTable[liIndex - 1]);
        const f32 lfHere = static_cast<f32>(lpTable[liIndex + 1] - lpTable[liIndex]);
        const f32 lfNext = static_cast<f32>(lpTable[liIndex + 2] - lpTable[liIndex + 1]);
        lfLeft = KF_HALF * (lfPrev + lfHere);
        lfRight = KF_HALF * (lfHere + lfNext);
    }

    const f32 lfFraction = afCycle - static_cast<f32>(liIndex);
    return lfLeft + lfFraction * (lfRight - lfLeft);
}

// -------------------------------------------------------------------------------------
// SampleToCycle @0x8268BBF8 -- the monotone inverse of CycleToSample.
//
// An INTERPOLATION search rather than a bisection: it estimates the index from the linear
// position of the sample within the remaining span, then bounds the next interval using
// mMinPeriod (the smallest possible cycle length), which is why BindToData precomputes it.
// -------------------------------------------------------------------------------------
f32 GinsuSynthData::SampleToCycle(int aiSample) const
{
    const int liCount = mCycleCount;
    if (liCount < 1)
        return KF_ZERO;

    const s32 *lpTable = CycleTable();
    if (aiSample <= lpTable[0])
        return KF_ZERO;
    if (aiSample >= lpTable[liCount])
        return static_cast<f32>(liCount);

    int liLow = 0;
    int liHigh = liCount;
    for (;;)
    {
        const int liSpan = liHigh - liLow;
        const f32 lfEstimate = static_cast<f32>(aiSample - lpTable[liLow])
                             * static_cast<f32>(liSpan)
                             / static_cast<f32>(lpTable[liHigh] - lpTable[liLow]);
        const int liIndex = liLow + FloorToInt(lfEstimate);

        if (aiSample < lpTable[liIndex])
        {
            const int liStep =
                CeilToInt(static_cast<f32>(lpTable[liIndex] - aiSample) / mMinPeriod);
            liHigh = liIndex;
            const int liBound = liIndex - liStep;
            if (liBound > liLow)
                liLow = liBound;
        }
        else if (aiSample >= lpTable[liIndex + 1])
        {
            const int liStep =
                CeilToInt(static_cast<f32>(aiSample - lpTable[liIndex]) / mMinPeriod);
            liLow = liIndex + 1;
            const int liBound = liIndex + 1 + liStep;
            if (liBound < liHigh)
                liHigh = liBound;
        }
        else
        {
            return static_cast<f32>(liIndex)
                 + static_cast<f32>(aiSample - lpTable[liIndex])
                       / static_cast<f32>(lpTable[liIndex + 1] - lpTable[liIndex]);
        }
    }
}

// -------------------------------------------------------------------------------------
// GetSamples @0x8268BED8 -- decode `numInput` samples starting at `startSample` and deliver
// `numOutput` of them, resampling when the two counts differ.
//
// When no resampling is needed the decoded samples go straight to the output. Otherwise
// they land in the scratch buffer at index 1, with index 0 seeded from mLastInputSample so
// the interpolator has the previous call's final sample as its left neighbour -- that
// prefix is what makes consecutive chunks join without a discontinuity.
// -------------------------------------------------------------------------------------
bool GinsuSynthData::GetSamples(int aiStartSample, int aiNumInputSamples,
                                int aiNumOutputSamples, f32 *apOutput,
                                f32 *apResampleBuffer, bool abUseOldData,
                                System *apSystem)
{
    if (aiStartSample < 0 || aiStartSample + aiNumInputSamples - 1 >= mSampleCount)
    {
        // The console's own diagnostic string (rodata @0x820ADB90).
        std::printf("GinsuSynthData::GetSamples FAILED! Startsample=%d "
                    "numInputSamples=%d mSampleCount=%d\n",
                    aiStartSample, aiNumInputSamples, mSampleCount);
        return false;
    }

    const int liStartBlock = aiStartSample >> 5;   // 32 samples per block
    if (liStartBlock != mCurrentBlock)
        DecodeBlock(liStartBlock, abUseOldData, apSystem);

    const bool lbResampling = (aiNumInputSamples != aiNumOutputSamples);
    f32 *lpDecoded = apOutput;
    if (lbResampling)
    {
        apResampleBuffer[0] = mLastInputSample;   // the continuity prefix
        lpDecoded = apResampleBuffer + 1;
    }

    int liWithinBlock = aiStartSample & 31;
    f32 lfLast = KF_ZERO;
    for (int liSample = 0; liSample < aiNumInputSamples; ++liSample)
    {
        lfLast = mSample[liWithinBlock] * KF_SAMPLE_SCALE;
        lpDecoded[liSample] = lfLast;
        if (++liWithinBlock == KI_GINSU_BLOCK_SAMPLES)
        {
            DecodeBlock(mCurrentBlock + 1, abUseOldData, apSystem);
            liWithinBlock = 0;
        }
    }

    if (lbResampling)
    {
        // A signed 16.16 step, with the phase starting one step in (index 0 of the scratch
        // is the previous call's tail, so the first output already interpolates forward).
        const s32 liStep = (aiNumInputSamples << 16) / aiNumOutputSamples;
        u32 luPhase = static_cast<u32>(liStep);
        for (int liOut = 0; liOut < aiNumOutputSamples - 1; ++liOut)
        {
            const unsigned luIndex = luPhase >> 16;
            const f32 lfFraction = static_cast<f32>(luPhase & 0xFFFFu) * KF_INV_65536;
            apOutput[liOut] = apResampleBuffer[luIndex]
                            + lfFraction * (apResampleBuffer[luIndex + 1]
                                            - apResampleBuffer[luIndex]);
            luPhase += static_cast<u32>(liStep);
        }
        // The final output sample is forced to the exact last decoded input rather than
        // being interpolated, so a chunk always ends on a real sample.
        apOutput[aiNumOutputSamples - 1] = apResampleBuffer[aiNumInputSamples];
    }

    mLastInputSample = lfLast;
    return true;
}

// =====================================================================================
//  GinsuPlayer
// =====================================================================================

// off_82F2D094 -- the "GinsuPlayer" runtime descriptor, REAL (its 52 bytes were re-read
// from the XEX: 'Gns0', plugInType 0 == SOURCE stage, 1 constructor parameter,
// 5 attributes, 2 events). Metadata FLAG'd null per the descriptor-wave convention.
//
// The create thunk is the Dac/SubMix precedent: the console's first store is the vtable
// install, which on the host IS the placement construction of the derived object over the
// generic stage memory, and must happen before CreateInstance's own stores.
static int GinsuPlayerCreateInstanceThunk(GinsuPlayer *self, void *apConstructorParams)
{
    ::new (static_cast<void *>(self)) GinsuPlayer;   // *a1 = off_820AE168
    return GinsuPlayer::CreateInstance(
        self, static_cast<const GinsuPlayer::PlayParams *>(apConstructorParams));
}

static PlugInDescRunTime g_GinsuPlayerDesc = {
    "GinsuPlayer",
    reinterpret_cast<void *>(&GinsuPlayer::GetSize),             // @0x826A40B8
    reinterpret_cast<void *>(&GinsuPlayerCreateInstanceThunk),   // @0x826C3418
    reinterpret_cast<void *>(&GinsuPlayer::PreProcess),          // @0x8268C1D8
    reinterpret_cast<void *>(&GinsuPlayer::Process),             // @0x8268C1E8
    0, 0, 0, 0,
    0,
    0x476E7330u,       // 'Gns0'
    0, 1, 5, 2, 0, 0,
    0
};

char **GinsuPlayer::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_GinsuPlayerDesc);
}

// -------------------------------------------------------------------------------------
// The instance's table-storage tail. BOTH GetSize (which allocates it) and PlayHandler
// (which binds into it) must agree, so both go through this one helper.
//
// X360-LITERAL TRAP: the console spells this 0x1D0 in GetSize and `(self + 0x1D7) & ~7` in
// PlayHandler -- two encodings of align_up(CONSOLE sizeof, 8). The host object is larger
// (widened base pointers, mSampleData, mpTempStore), so reusing either literal would place
// the copied index tables INSIDE the object and let BindToData scribble its own members.
// (This is the same bug class as the Resample history-buffer fix and the phase-D probe crash.)
// -------------------------------------------------------------------------------------
size_t GinsuPlayer::TableStorageOffset()
{
    return AlignUp8(sizeof(GinsuPlayer));
}

// -------------------------------------------------------------------------------------
// GetSize @0x826A40B8 -- the instance plus the copied index tables. With no constructor
// params the console reserves a flat 4096-byte tail.
// -------------------------------------------------------------------------------------
int GinsuPlayer::GetSize(const VoiceStageConfig *config)
{
    const PlayParams *lpParams =
        static_cast<const PlayParams *>(config->mpContext);
    const size_t luTableBytes = lpParams
        ? GinsuSynthData::GetTotalTableSize(lpParams->pGinFile)
        : 4096u;                                      // X360: 0x11D0 - 0x1D0
    return static_cast<int>(TableStorageOffset() + luTableBytes);
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x826C3418 -- clear the bound-content state, publish the attribute table,
// and open at the default 1 kHz / 48 kHz. The constructor params are NOT read here (the
// blob is bound later, by the play event).
// -------------------------------------------------------------------------------------
int GinsuPlayer::CreateInstance(GinsuPlayer *self, const PlayParams * /*params*/)
{
    self->mSynthData.mLastInputSample = KF_ZERO;
    self->mSynthData.mCycleCount = 0;
    self->mSynthData.mMinFrequency = KF_ZERO;
    self->mSynthData.mMaxFrequency = KF_ZERO;
    self->mSynthData.mSegCount = 0;
    self->mSynthData.mSampleCount = 0;
    self->mSynthData.mSampleRate = 0;

    self->mAttribute[ATTRIBUTE_SETJUMPSPAN].mfValue = KF_ZERO;
    self->mAttribute[ATTRIBUTE_GETSAMPLERATE].mfValue = KF_ZERO;
    self->mAttribute[ATTRIBUTE_GETMINFREQUENCY].mfValue = KF_ZERO;
    self->mAttribute[ATTRIBUTE_GETMAXFREQUENCY].mfValue = KF_ZERO;

    self->mPlaying = 0;
    self->mSampleRate = KF_DEFAULT_SAMPLE_RATE;
    self->mPrevSampleRate = KF_DEFAULT_SAMPLE_RATE;

    self->mpAttribute = self->mAttribute;                          // stw self+0x28 -> +0x0C
    self->mAttribute[ATTRIBUTE_SETFREQUENCY].mfValue = KF_DEFAULT_FREQUENCY;
    return 1;
}

// -------------------------------------------------------------------------------------
// PreProcess @0x8268C1D8 -- stow the requested count and pull nothing from upstream (this
// is a source stage). NOTE the store is a full WORD here, unlike SndPlayer1's halfword.
// -------------------------------------------------------------------------------------
int GinsuPlayer::PreProcess(GinsuPlayer *self, Mixer * /*ctx*/, bool /*discontinuity*/,
                            int outputSamplesRequested)
{
    self->mOutputSamplesRequested = outputSamplesRequested;   // stw r6 -> +0x54
    return 0;
}

// -------------------------------------------------------------------------------------
// Event @0x826C3498 (vt[1]) -- queue a bind+play, or a stop.
//
// ⚠️ There is deliberately NO `event == 1` comparison: event 0 is play and EVERY other id
// dispatches stop. Reproduced as-is.
// -------------------------------------------------------------------------------------
int GinsuPlayer::Event(int aiEventId, void *apParam)
{
    System *lpSystem = mpSystemUseGetSystemAccessor;
    const u32 luCursor = lpSystem->muDeferredRingCursor;

    if (aiEventId == EVENT_PLAY)
    {
        PlayCommand *lpCommand =
            reinterpret_cast<PlayCommand *>(lpSystem->mpDeferredRingBase + luCursor);
        // RECORD STRIDE (X360-literal trap): console 12, host sizeof -- and PlayHandler
        // returns the same host sizeof, or the ring replay desynchronises.
        lpSystem->muDeferredRingCursor =
            luCursor + static_cast<u32>(sizeof(PlayCommand));
        lpCommand->mpHandler = &GinsuPlayer::PlayHandler;
        lpCommand->mpTarget = this;
        lpCommand->mpGinFile = static_cast<PlayParams *>(apParam)->pGinFile;
    }
    else
    {
        StopCommand *lpCommand =
            reinterpret_cast<StopCommand *>(lpSystem->mpDeferredRingBase + luCursor);
        lpSystem->muDeferredRingCursor =
            luCursor + static_cast<u32>(sizeof(StopCommand));   // console 8
        lpCommand->mpHandler = &GinsuPlayer::StopHandler;
        lpCommand->mpTarget = this;
    }
    return 0;
}

// -------------------------------------------------------------------------------------
// StopHandler @0x8268B1B8 -- clear the playing flag. The return is the ring-cursor advance.
// -------------------------------------------------------------------------------------
int GinsuPlayer::StopHandler(void *apCommand)
{
    StopCommand *lpCommand = static_cast<StopCommand *>(apCommand);
    lpCommand->mpTarget->mPlaying = 0;
    return static_cast<int>(sizeof(StopCommand));   // X360: li r3, 8
}

// -------------------------------------------------------------------------------------
// PlayHandler @0x826A4100 -- bind the blob and start.
//
// It marks itself playing FIRST, binds the content into the instance tail, publishes the
// bound rate/range as readback attributes, and seeds the jump scheduler.
// -------------------------------------------------------------------------------------
int GinsuPlayer::PlayHandler(void *apCommand)
{
    PlayCommand *lpCommand = static_cast<PlayCommand *>(apCommand);
    GinsuPlayer *lpSelf = lpCommand->mpTarget;

    lpSelf->mPlaying = 1;
    lpSelf->mpGinFile = lpCommand->mpGinFile;

    // Called for its ENDIAN-ADJUST side effect; the size result is discarded here.
    (void)GinsuSynthData::GetTotalTableSize(lpSelf->mpGinFile);

    u8 *lpTableStorage = reinterpret_cast<u8 *>(lpSelf) + TableStorageOffset();
    (void)lpSelf->mSynthData.BindToData(lpSelf->mpGinFile, lpTableStorage);

    const int liRate = lpSelf->mSynthData.mSampleRate;
    lpSelf->mPrevSampleRate = KF_DEFAULT_SAMPLE_RATE;
    lpSelf->mSampleRate = static_cast<f32>(liRate);

    if (liRate == 0)
    {
        // ⚠️ FAITHFUL CONSOLE BEHAVIOUR, and it is a console BUG: malformed content returns
        // ZERO, but the caller uses this return as the ring-cursor advance -- so the replay
        // would re-run this same record forever. Reproduced rather than "fixed"; the guard
        // belongs upstream, in whatever validates the content before it is played.
        return 0;
    }

    lpSelf->mAttribute[ATTRIBUTE_GETSAMPLERATE].mfValue = lpSelf->mSampleRate;
    lpSelf->mAttribute[ATTRIBUTE_GETMINFREQUENCY].mfValue = lpSelf->mSynthData.mMinFrequency;
    lpSelf->mAttribute[ATTRIBUTE_GETMAXFREQUENCY].mfValue = lpSelf->mSynthData.mMaxFrequency;

    lpSelf->mSynthData.mOldDataBlockIndex = -1;
    lpSelf->mSynthData.mTempStoreBlockIndex = -1;
    lpSelf->mSynthData.mpTempStore = 0;

    lpSelf->mPlaybackPos = 0;
    lpSelf->mNoJumpSize = TruncToInt(lpSelf->mSampleRate * KF_NO_JUMP_SECONDS);
    lpSelf->mOverlapSize = TruncToInt(lpSelf->mSampleRate * KF_OVERLAP_SECONDS);
    lpSelf->mRandomSeed = static_cast<u32>(KI_RNG_SEED);
    lpSelf->mNextJumpTime = 0.0;

    std::memset(lpSelf->mSynthData.mOldDataBlock, 0, KI_GINSU_OLD_BYTES);
    lpSelf->mSynthData.mpTempStore = 0;
    return static_cast<int>(sizeof(PlayCommand));   // X360: li r3, 12
}

// -------------------------------------------------------------------------------------
// Process @0x8268C1E8 -- the granular scheduler and renderer.
//
// Buffer usage inside the destination SampleBuffer: channel 0 is the final output,
// channel 2 is crossfade staging and channel 3 is resampler scratch. (Channel 1 is not
// touched by this body.)
// -------------------------------------------------------------------------------------
int GinsuPlayer::Process(GinsuPlayer *self, Mixer *ctx, bool /*isLastInput*/)
{
    if (!self->mPlaying)
        return 0;   // BUFFERSTATUS_UNAVAILABLE

    // Rate-change handshake: spend one frame adopting the new rate, publishing zero samples.
    if (self->mSampleRate != self->mPrevSampleRate)
    {
        ctx->mNumSamples = 0;
        ctx->mbChannelCount = self->mOutputChannels;
        ctx->mfSampleRate = self->mSampleRate;
        self->mPrevSampleRate = self->mSampleRate;
        return 1;
    }

    // The jump window width, clamped on its UPPER side only (the console has no lower clamp).
    f32 lfJumpSpan = self->mAttribute[ATTRIBUTE_SETJUMPSPAN].mfValue;
    const f32 lfMaxSpan = static_cast<f32>(self->mSynthData.mCycleCount - 1);
    if (lfJumpSpan > lfMaxSpan)
        lfJumpSpan = lfMaxSpan;

    ctx->mNumSamples = static_cast<u32>(self->mOutputSamplesRequested);
    ctx->mbChannelCount = self->mOutputChannels;
    ctx->mfSampleRate = self->mSampleRate;

    SampleBuffer *lpDst = ctx->mpDstBuffer;
    f32 *lpOutput = lpDst->mpSamples;
    const unsigned luStride = lpDst->muStride;
    f32 *lpCrossfade = lpOutput + 2 * luStride;
    f32 *lpScratch = lpOutput + 3 * luStride;

    int liOverlap = TruncToInt(self->mSampleRate * KF_OVERLAP_SECONDS);
    int liProduced = 0;

    // ---- the periodic cycle jump ---------------------------------------------------
    if (ctx->mdStreamTime >= self->mNextJumpTime
        && self->mOutputSamplesRequested >= liOverlap)
    {
        const f32 lfCurrentCycle = self->mSynthData.SampleToCycle(self->mPlaybackPos);
        const int liTargetSample = self->mSynthData.FrequencyToSample(
            self->mAttribute[ATTRIBUTE_SETFREQUENCY].mfValue);
        const f32 lfTargetCycle = self->mSynthData.SampleToCycle(liTargetSample);

        // Centre a jumpSpan-wide window on the target cycle and clamp it into the table.
        f32 lfWindowStart = lfTargetCycle - lfJumpSpan / KF_TWO;
        if (lfWindowStart < KF_ZERO)
        {
            lfWindowStart = KF_ZERO;
        }
        else
        {
            const f32 lfHigh =
                static_cast<f32>(self->mSynthData.mCycleCount) - lfJumpSpan - KF_TWO;
            if (lfWindowStart > lfHigh)
                lfWindowStart = lfHigh;
        }

        // The xorshift-style RNG. NOTE the choice uses the OLD seed, not the advanced one.
        const u32 luOldSeed = self->mRandomSeed;
        const u32 luX = luOldSeed ^ static_cast<u32>(KI_RNG_XOR);
        const u32 luY = luX ^ (luX >> 5);
        self->mRandomSeed = (luY << 27) ^ luY ^ luX;
        const u32 luResidue = luOldSeed % static_cast<u32>(KI_RNG_MODULUS);
        const f32 lfRandomCycle = lfWindowStart
            + (static_cast<f32>(luResidue) * lfJumpSpan) * KF_INV_2_POW_31;

        int liCandidate = self->mSynthData.CycleToSample(lfRandomCycle);

        // Phase-safe selection: a jump must land on the SAME phase of the waveform, so a
        // backward choice is re-derived as a whole number of cycles back, and a forward
        // choice is only taken if it clears the current cycle's period.
        if (liCandidate < self->mPlaybackPos)
        {
            const int liCyclesBack = TruncToInt(lfCurrentCycle - lfRandomCycle);
            liCandidate = (liCyclesBack > 0)
                ? self->mSynthData.CycleToSample(lfCurrentCycle
                                                 - static_cast<f32>(liCyclesBack))
                : self->mPlaybackPos;
        }
        else
        {
            const int liOnePeriod =
                TruncToInt(self->mSynthData.CyclePeriod(lfCurrentCycle));
            if (liCandidate > self->mPlaybackPos + liOnePeriod)
            {
                const int liCyclesForward = TruncToInt(lfRandomCycle - lfCurrentCycle);
                liCandidate = self->mSynthData.CycleToSample(
                    lfCurrentCycle + static_cast<f32>(liCyclesForward));
            }
            else
            {
                liCandidate = self->mPlaybackPos;   // too close to be worth jumping
            }
        }

        self->mNextJumpTime += KD_JUMP_INTERVAL;

        if (liCandidate != self->mPlaybackPos)
        {
            // Without a valid look-back cache there is nothing to fade FROM.
            if (self->mSynthData.mOldDataBlockIndex == -1)
                liOverlap = 0;

            // Output samples per source cycle at the requested frequency.
            const f32 lfSamplesPerCycle = (self->mSampleRate * KF_CYCLES_PER_MINUTE)
                / self->mAttribute[ATTRIBUTE_SETFREQUENCY].mfValue;
            const f32 lfOutputPerInput = KF_ONE / lfSamplesPerCycle;

            // Fade OUT the old position.
            const int liOldInput = TruncToInt(
                self->mSynthData.CyclePeriod(lfCurrentCycle)
                * static_cast<f32>(liOverlap) * lfOutputPerInput);
            (void)self->mSynthData.GetSamples(self->mPlaybackPos, liOldInput, liOverlap,
                                              lpCrossfade, lpScratch, true,
                                              self->mpSystemUseGetSystemAccessor);
            for (int liSample = 0; liSample < liOverlap; ++liSample)
            {
                lpOutput[liSample] =
                    (KF_ONE - static_cast<f32>(liSample) / static_cast<f32>(liOverlap))
                    * lpCrossfade[liSample];
            }

            // Fade IN the new position.
            self->mPlaybackPos = liCandidate;
            const f32 lfNewCycle = self->mSynthData.SampleToCycle(liCandidate);
            const int liNewInput = TruncToInt(
                self->mSynthData.CyclePeriod(lfNewCycle)
                * static_cast<f32>(liOverlap) * lfOutputPerInput);
            (void)self->mSynthData.GetSamples(liCandidate, liNewInput, liOverlap,
                                              lpCrossfade, lpScratch, false,
                                              self->mpSystemUseGetSystemAccessor);
            for (int liSample = 0; liSample < liOverlap; ++liSample)
            {
                lpOutput[liSample] +=
                    (static_cast<f32>(liSample) / static_cast<f32>(liOverlap))
                    * lpCrossfade[liSample];
            }

            liProduced = liOverlap;
            self->mOutputSamplesRequested -= liOverlap;
            self->mPlaybackPos += liNewInput;
        }
    }

    // ---- steady-state rendering ------------------------------------------------------
    while (self->mOutputSamplesRequested > 0)
    {
        int liOutCount = self->mOutputSamplesRequested;
        const f32 lfCycle = self->mSynthData.SampleToCycle(self->mPlaybackPos);
        const f32 lfPeriod = self->mSynthData.CyclePeriod(lfCycle);
        const f32 lfSamplesPerCycle = (self->mSampleRate * KF_CYCLES_PER_MINUTE)
            / self->mAttribute[ATTRIBUTE_SETFREQUENCY].mfValue;

        int liInCount =
            TruncToInt((static_cast<f32>(liOutCount) / lfSamplesPerCycle) * lfPeriod);
        if (liInCount > static_cast<int>(KF_MAX_INPUT_CHUNK))
        {
            // Cap the source chunk and recompute how much output that yields, so a long
            // request is served in bounded pieces.
            liInCount = static_cast<int>(KF_MAX_INPUT_CHUNK);
            liOutCount = TruncToInt((lfSamplesPerCycle / lfPeriod) * KF_MAX_INPUT_CHUNK);
        }

        (void)self->mSynthData.GetSamples(self->mPlaybackPos, liInCount, liOutCount,
                                          lpOutput + liProduced, lpScratch, false,
                                          self->mpSystemUseGetSystemAccessor);
        liProduced += liOutCount;
        self->mPlaybackPos += liInCount;
        self->mOutputSamplesRequested -= liOutCount;
    }

    // Refresh the look-back cache so the NEXT jump has something to fade out of.
    const int liBlock = self->mPlaybackPos >> 5;
    self->mSynthData.mOldDataBlockIndex = liBlock;
    std::memcpy(self->mSynthData.mOldDataBlock,
                self->mSynthData.mSampleData + KI_GINSU_RECORD_BYTES * liBlock,
                KI_GINSU_OLD_BYTES);

    SampleBuffer *lpTemp = ctx->mpSrcBuffer;
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = lpTemp;
    return 1;   // BUFFERSTATUS_AVAILABLE
}

} // namespace core
} // namespace audio
} // namespace rw
