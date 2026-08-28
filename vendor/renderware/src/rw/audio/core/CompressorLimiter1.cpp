// =====================================================================================
// rw::audio::core::CompressorLimiter1 bodies.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. No Feb-2007 leak source and no DecFIGS DWARF exist.
//   ClearBuffer @0x82B671F0 -- store-for-store
//   Configure   @0x82B67188 -- store-for-store
//   Process     @0x82B64DB0 -- the VMX128 DSP kernel, decoded and bodied 2026-08-28
//                              (phase E; ONE marked deviation -- see its header comment)
// See CompressorLimiter1.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/CompressorLimiter1.h"
#include "rw/audio/core/Mixer.h"  // Mixer (the process context) + SampleBuffer

#include <cstring> // std::memset (the X360 XMemSet) / std::memcpy (the gain-word compare)
#include <cmath>   // std::fma / std::fabs / std::exp2 / std::log2 (the recurrence + curve)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// ClearBuffer @0x82B671F0 -- XMemSet(self, 0, 0x30): zero the 48-byte envelope scratch.
// -------------------------------------------------------------------------------------
void *CompressorLimiter1::ClearBuffer(CompressorLimiter1 *self)
{
    return std::memset(self, 0, 0x30);
}

// -------------------------------------------------------------------------------------
// Configure @0x82B67188   (FLAG: rwaudio PDB reconcile 2026-06-27 -- member names)
//   self->mThresholdOn        = thresholdOn;                 // stfs f1 @ +0x30
//   self->mThresholdOff       = thresholdOff;                // stfs f2 @ +0x34
//   self->mCompExponent       = compExponent;                // stfs f3 @ +0x38
//   self->mAttackSamples      = attackSamples;               // stw  r7 @ +0x3C
//   self->mReleaseSamples     = releaseSamples;              // stw  r8 @ +0x40
//   self->mGroupChannels      = (groupChannels != 0);        // cntlzw/extrwi/xori @ +0x4C
//   self->mCompExponentStepOn = compExponent / (f32)attack;  // fcfid/frsp/fdivs @ +0x44
//   self->mCompExponentStepOff= compExponent / (f32)release; // fcfid/frsp/fdivs @ +0x48
// (store ORDER per the asm: +0x30, +0x34, +0x38, +0x3C, +0x40, +0x4C, then +0x44, +0x48 --
// the two derived steps come last, not in ascending offset order.)
//
// The +0x4C byte is the asm's `cntlzw(groupChannels&0xFF); extrwi bit26; xori 1`, i.e. it
// is set exactly when the low byte is nonzero. The attack/release divisors are
// sign-extended-to-i64 then converted to f32 (fcfid+frsp) before the divide, matching the
// asm's stack round-trip. r3 is never written, so the machine return is the incoming self.
// -------------------------------------------------------------------------------------
CompressorLimiter1 *CompressorLimiter1::Configure(CompressorLimiter1 *self, f32 thresholdOn,
                                                  f32 thresholdOff, f32 compExponent,
                                                  s32 attackSamples, s32 releaseSamples,
                                                  s32 groupChannels)
{
    self->mThresholdOn = thresholdOn;
    self->mThresholdOff = thresholdOff;
    self->mCompExponent = compExponent;
    self->mAttackSamples = attackSamples;
    self->mReleaseSamples = releaseSamples;
    self->mGroupChannels = static_cast<u8>((groupChannels & 0xFF) != 0 ? 1 : 0);
    self->mCompExponentStepOn = compExponent / static_cast<f32>(static_cast<s64>(attackSamples));
    self->mCompExponentStepOff = compExponent / static_cast<f32>(static_cast<s64>(releaseSamples));
    return self;
}

// =====================================================================================
// Process @0x82B64DB0 -- the per-block envelope-follower + gain-curve kernel.
//
// DECODED AND BODIED 2026-08-28 (phase E). The console body is 2,294 instructions of
// hand-written VMX128 working four samples per lane-group; the full instruction-level walk,
// the lane-mask audit and the rodata re-reads are in
// progress/scratch_dossiers/compressorlimiter1_process_vmx_decode_codex.md. It is written
// here as the scalar equivalent because the vectorisation is a scheduling detail -- every
// lane runs the identical per-sample recurrence in increasing memory order, so the sample
// ORDER, the state, and the arithmetic are all preserved by the scalar form. What is NOT a
// scheduling detail and IS reproduced exactly: the 8-chunks-of-32 structure (the history
// commit and the threshold-reciprocal rounding happen per chunk), the fixed 256-sample
// frame, the destination-as-gain-scratch protocol, the reverse channel order of the linked
// application loop, and the unconditional slot swap.
//
// ⭐ THE ONE DEVIATION, marked because it is physically impossible on PC: the gain curve is
// evaluated on the console by the VMX ESTIMATE pair `vexptefp(vlogefp(|B|) * E)`. Those are
// hardware estimate instructions; the ISA publishes error BOUNDS, not the algorithm, and
// Xenon's tables are not in the image. std::exp2/std::log2 below is therefore a
// mathematically better but NOT bit-identical substitute. The architectural worst case for
// vexptefp alone is about +0.53/-0.56 dB of gain error, so the audible difference is a
// slightly different level contour WHILE the compressor is actively pulling gain down --
// never a timing, routing, channel-count or buffer-publication difference. Everything else
// in this body is exact.
//
// Buffer protocol (load-bearing, and the reason NEITHER caller swaps): source and
// destination must be DISTINCT. The kernel first fills the destination with per-sample GAIN
// values, then overwrites that same destination with source*gain audio, then swaps the
// slots itself so the freshly written destination becomes the published source. It never
// touches mNumSamples, mbChannelCount, mfSampleRate or mpFormat, and it swaps even when the
// channel count is zero.
// =====================================================================================
namespace
{
    // The envelope recurrence's three rodata coefficients, re-read big-endian from the
    // decrypted XEX (file_off = 0x3000 + vaddr - 0x82000000):
    const f32 KF_ENVELOPE_FEEDBACK = 0.997f;      // flt_8214AF04 (0x3F7F3B64)
    const f32 KF_ENVELOPE_INPUT    = 0.003000021f;// flt_8214B10C (0x3B449C00)
    const f32 KF_ENVELOPE_BIAS     = 1.0e-18f;    // flt_8214B108 (0x219392EF) -- keeps the
                                                  //   envelope off exact zero so the
                                                  //   logarithm stays on its normal path
    enum { KI_FRAME_SAMPLES = 256, KI_CHUNK_SAMPLES = 32, KI_CHUNKS = 8 };

    // The console reduces the linked gain curve with vcmpgtuw -- an UNSIGNED comparison of
    // the raw float WORDS, not a floating min. For the positive finite gains this kernel
    // produces the two agree, but the bit form is what the asm does, so it is what is
    // written here.
    inline bool GainBitsGreater(f32 afLhs, f32 afRhs)
    {
        u32 luLhs;
        u32 luRhs;
        std::memcpy(&luLhs, &afLhs, sizeof(luLhs));
        std::memcpy(&luRhs, &afRhs, sizeof(luRhs));
        return luLhs > luRhs;
    }

    // One sample of the envelope + exponent state machine, returning that sample's gain.
    // The rounding order is the console's: a separately rounded fmuls of the old level,
    // then a FUSED fmadds of the input term, then a plain fadds of the bias.
    f32 UpdateOneSample(CompressorLimiter1::History &arHistory,
                        const CompressorLimiter1 &akrCoefficients,
                        f32 afThresholdReciprocal, f32 afInput)
    {
        const f32 lfFeedbackTerm = arHistory.lpfDelay1 * KF_ENVELOPE_FEEDBACK;   // fmuls
        f32 lfLevel = std::fma(std::fabs(afInput), KF_ENVELOPE_INPUT, lfFeedbackTerm); // fmadds
        lfLevel = lfLevel + KF_ENVELOPE_BIAS;                                    // fadds

        // Attack steps the exponent toward the (negative) target and simply STOPS stepping
        // once it has passed it -- there is no clamp to the target. Release steps back
        // toward zero and DOES clamp a would-cross-zero step to exactly 0.
        const f32 lfExponent = arHistory.compExponentCurrent;
        const f32 lfAttack =
            (lfExponent - akrCoefficients.mCompExponent >= 0.0f)
                ? lfExponent + akrCoefficients.mCompExponentStepOn
                : lfExponent;
        const f32 lfRelease =
            (akrCoefficients.mCompExponentStepOff - lfExponent >= 0.0f)
                ? lfExponent - akrCoefficients.mCompExponentStepOff
                : 0.0f;

        // The two threshold tests are hysteresis: compress at/above mThresholdOn, release
        // at/below mThresholdOff, HOLD the current curve between them. The asm applies the
        // off-select first and the on-select last, so the order below is load-bearing.
        f32 lfNextExponent = lfExponent;
        if (akrCoefficients.mThresholdOff - lfLevel >= 0.0f)
            lfNextExponent = lfRelease;
        if (lfLevel - akrCoefficients.mThresholdOn >= 0.0f)
            lfNextExponent = lfAttack;

        arHistory.lpfDelay1 = lfLevel;
        arHistory.compExponentCurrent = lfNextExponent;

        // gain = (level / thresholdOn) ^ exponent. On the console this is the estimate pair
        // vexptefp(vlogefp(|B|) * E) -- see the marked deviation in the header comment. The
        // console's surrounding mask network handles negative/zero bases and integral
        // exponents; the configured domain here always has a positive, nonzero base (the
        // bias guarantees it) and an exponent in [mCompExponent, 0], so the normal path is
        // the only reachable one and the edge machinery is documented rather than emulated.
        const f32 lfNormalizedLevel = lfLevel * afThresholdReciprocal;
        return std::exp2(lfNextExponent * std::log2(lfNormalizedLevel));
    }
}

int CompressorLimiter1::Process(CompressorLimiter1 *self, AudioProcessContext *ctx,
                                u8 channelCount)
{
    SampleBuffer *lpSrcDesc = ctx->mpSrcBuffer;
    SampleBuffer *lpDstDesc = ctx->mpDstBuffer;

    // The linked curve lives in destination channel 0 while it is being built.
    f32 *lpSharedGain = lpDstDesc ? lpDstDesc->mpSamples : 0;

    // ---- pass 1: build the gain curve(s) ------------------------------------------------
    for (u32 luChannel = 0; luChannel < channelCount; ++luChannel)
    {
        const f32 *lpSrc = lpSrcDesc->mpSamples + lpSrcDesc->muStride * luChannel;
        f32 *lpChannelDst = lpDstDesc->mpSamples + lpDstDesc->muStride * luChannel;

        // Eight chunks of 32: the console reloads the history and re-rounds the threshold
        // reciprocal per chunk, and commits the history back at the end of each one.
        for (u32 luChunk = 0; luChunk < KI_CHUNKS; ++luChunk)
        {
            History lHistory = self->mChannelHistory[luChannel];
            const f32 lfThresholdReciprocal = 1.0f / self->mThresholdOn;
            const u32 luFirst = luChunk * KI_CHUNK_SAMPLES;

            for (u32 luLane = 0; luLane < KI_CHUNK_SAMPLES; ++luLane)
            {
                const u32 luSample = luFirst + luLane;
                const f32 lfGain = UpdateOneSample(lHistory, *self, lfThresholdReciprocal,
                                                   lpSrc[luSample]);

                if (!self->mGroupChannels)
                {
                    lpChannelDst[luSample] = lfGain;   // per-channel curve
                }
                else if (luChannel == 0 || GainBitsGreater(lpSharedGain[luSample], lfGain))
                {
                    // LINKED does NOT share the envelope STATE -- every channel keeps its
                    // own mChannelHistory[c] and derives its own curve from its own samples.
                    // What is shared is only the APPLIED gain: the strongest attenuation
                    // across channels wins, with channel 0 seeding every lane.
                    lpSharedGain[luSample] = lfGain;
                }
            }
            self->mChannelHistory[luChannel] = lHistory;
        }
    }

    // ---- pass 2: apply the curve, overwriting the gain scratch with audio ---------------
    if (!self->mGroupChannels)
    {
        for (u32 luChannel = 0; luChannel < channelCount; ++luChannel)
        {
            const f32 *lpSrc = lpSrcDesc->mpSamples + lpSrcDesc->muStride * luChannel;
            f32 *lpDst = lpDstDesc->mpSamples + lpDstDesc->muStride * luChannel;
            for (u32 luSample = 0; luSample < KI_FRAME_SAMPLES; ++luSample)
                lpDst[luSample] = lpSrc[luSample] * lpDst[luSample]; // dst held the gain
        }
    }
    else
    {
        // REVERSE channel order, and that is load-bearing: channel 0's destination IS the
        // shared curve, so it must be consumed last or the higher channels would read audio
        // where they expect gains.
        for (u32 luRemaining = channelCount; luRemaining != 0; --luRemaining)
        {
            const u32 luChannel = luRemaining - 1;
            const f32 *lpSrc = lpSrcDesc->mpSamples + lpSrcDesc->muStride * luChannel;
            f32 *lpDst = lpDstDesc->mpSamples + lpDstDesc->muStride * luChannel;
            for (u32 luSample = 0; luSample < KI_FRAME_SAMPLES; ++luSample)
                lpDst[luSample] = lpSrc[luSample] * lpSharedGain[luSample];
        }
    }

    // The swap is UNCONDITIONAL -- it happens even for a zero channel count, which is why
    // the zero-channel early-out above still falls through to here.
    SampleBuffer *lpTemp = ctx->mpSrcBuffer;
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = lpTemp;

    // The console forms no return value; both callers discard it.
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
