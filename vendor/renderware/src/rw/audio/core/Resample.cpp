// =====================================================================================
// rw::audio::core::Resample bodies -- the sample-rate-conversion audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. The vendor resample.h (references/Feb-2007/.../
// rwaudiocore/2.11.00/include/rw/audio/core/plugins/resample.h) supplies the authoritative
// member/constant names and matches the layout member-for-member.
//   CreateInstance         @0x82BA37F0 -- store-for-store
//   GetOutputSamples       @0x82B9A8D0 -- branch-for-branch
//   GetPlugInDescRunTime   @0x82B9A850 -- returns the registered "Resample" descriptor
//   GetSize                @0x82B9F3D8 -- 24 * numChannels + header (host header, see below)
//   LinearInterpolate      @0x82B9A918 -- store-for-store (phase E; RAW-decoded from the
//                                         XEX at file_off 0xB9D918 -- the IDA export has no
//                                         dossier for it, so the 190 instructions were
//                                         disassembled directly and walked lane by lane)
//   PreProcess             @0x82B9AC10 -- store-for-store / branch-for-branch
//   Process                @0x82B9F3E8 -- branch-for-branch (phase E callback wave)
//   SetResampleIncrement   @0x82B9A860 -- store-for-store
//   `vector deleting destructor' @0x82BA1C68 -- reinstalls base vtable, conditional free
// See Resample.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/Resample.h"
#include "rw/audio/core/PlugIn.h"   // PlugInDescRunTime (the real descriptor record) + System
#include "rw/audio/core/Mixer.h"    // the process context + StackAllocator (the scratch carve)

#include <cstring>  // std::memset / std::memcpy (the X360 XMemSet / _blkmov)
#include <cstdint>  // uintptr_t

namespace rw
{
namespace audio
{
namespace core
{

// off_8217F504 -- the Resample v-table installed at construction. off_820AA810 -- the base
// PlugIn v-table the deleting destructor reinstalls before any free. These are opaque data
// symbols in the XEX (no exported contents); modelled as honest placeholder storage so the
// bodies below link without fabricating their contents.
static void *const KRS_ResampleVTable = nullptr;    // off_8217F504
static void *const KRS_BasePlugInVTable = nullptr;  // off_820AA810

// flt_821565B0 -- the fraction scale LinearInterpolate multiplies the 16-bit phase by.
// ⭐ IT IS NOT 1/65536. The XEX word at file_off 0x1595B0 is 37 7F FC 9C == 1.5258e-05f,
// the shortest decimal literal near 1/65536 (which would be 0x37800000 == 1.52587890625e-05).
// The original source therefore wrote a ROUNDED DECIMAL constant, and the interpolation
// phase is very slightly compressed as a result (by a factor of 0.9999483). The exact bit
// pattern is reproduced here rather than the mathematically "correct" reciprocal, because
// the console's output is what parity is measured against.
static const f32 KF_PHASE_SCALE = 1.5258e-05f;      // flt_821565B0 (0x377FFC9C)

// The dispatch thunk for the record's PreProcess/Process slots is unnecessary -- both PC
// bodies are already the dispatched static shape.

// off_82F8F8E0 -- the "Resample" runtime descriptor, REAL (descriptor-record wave; record
// dump progress/scratch_dossiers/plugindesc_layout_codex.md). One of only three registered
// plug-ins with a LIVE PreProcess slot. Metadata FLAG'd null per the descriptor-wave
// convention (no committed consumer reads them).
static PlugInDescRunTime g_ResampleDesc = {
    "Resample",
    reinterpret_cast<void *>(&Resample::GetSize),         // @0x82B9F3D8
    reinterpret_cast<void *>(&Resample::CreateInstance),  // @0x82BA37F0
    reinterpret_cast<void *>(&Resample::PreProcess),      // @0x82B9AC10
    reinterpret_cast<void *>(&Resample::Process),         // @0x82B9F3E8
    0, 0, 0, 0,
    0,
    0x52737030u,       // 'Rsp0'
    2, 0, 1, 0, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9A850 -- return the address of the registered descriptor
// record (its label is the string "Resample").
// -------------------------------------------------------------------------------------
char **Resample::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_ResampleDesc); // &off_82F8F8E0
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B9F3D8 -- the per-instance allocation size: the fixed header plus a
// 24-byte-per-channel history buffer. The channel count is the low byte of the stage
// config's +0x08 field (the same byte Voice passes as the CreateInstance flag).
//
// X360-LITERAL TRAP: the console's `80` is ITS OWN sizeof(Resample); the host header is
// larger (widened base-view pointers), so the host must add its own aligned header size --
// the SAME expression CreateInstance uses to place the buffer, so the allocation and the
// placement can never disagree.
// -------------------------------------------------------------------------------------
int Resample::GetSize(const VoiceStageConfig *config)
{
    return static_cast<int>(KU_RESAMPLE_HISTORY_BYTES_PER_CHANNEL
                                * static_cast<u8>(config->mFlagAndField8)
                            + ResampleHistoryOffset());   // X360: 24 * channels + 80
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA37F0 -- placement-init a Resample over `self`.
//   if (self) self->mBase.mpVTable = off_8217F504;
//   self->mBase.mpAttributes(+0x0C) = &self->mfPitch(+0x28);
//   zero the 24*numChannels history buffer at the first 8-aligned offset past the header;
//   record that offset at +0x44; seed the rate/ratio/accumulator/latency fields.
//   return 1;
// -------------------------------------------------------------------------------------
int Resample::CreateInstance(Resample *self)
{
    if (self)
        self->mBase.mpVTable = KRS_ResampleVTable; // off_8217F504

    // Point the base attribute-table slot at the live pitch attribute (self+0x28) so the
    // base SetAttribute writes land on ATTRIBUTE_SETPITCH.
    self->mBase.mpAttributes = &self->mAttribute[0];

    // History buffer: 24 bytes per channel, placed at the first 8-aligned address past the
    // fixed header.
    //
    // HOST FIX (phase E 2026-08-28): the console forms this as `((this + 0x57) & ~7) - this`
    // == 0x50, which is align_up(ITS OWN sizeof, 8). Transliterating the 0x57 literal was a
    // latent host bug -- the widened base view makes sizeof(Resample) exceed 0x50, so the
    // history buffer would have been placed INSIDE the object and the interpolation kernel
    // would have scribbled the instance's own members (the same failure mode as the phase-D
    // probe crash). ResampleHistoryOffset() is the host's align_up(sizeof(Resample), 8), and
    // GetSize allocates against the very same expression.
    const u32 historyBytes =
        KU_RESAMPLE_HISTORY_BYTES_PER_CHANNEL * self->mBase.mbChannelCount;
    const u32 bufferOffset = ResampleHistoryOffset();
    self->mHistoryBufferOffset = static_cast<u16>(bufferOffset);
    std::memset(reinterpret_cast<char *>(self) + bufferOffset, 0, historyBytes);

    self->mPreviousSampleRate = 48000.0f;  // flt_820AA808 @ +0x38
    self->mIncr16_16 = 0;                  // @ +0x3C
    self->mAcc16_16 = 0;                   // @ +0x40
    self->mCurrentHistorySamples = 0;      // @ +0x48
    self->mAttribute[ATTRIBUTE_SETPITCH].mfValue = 1.0f; // flt_82001C98 @ +0x28
    self->mRequestedResampleRatio = -1.0f; // flt_820037C8 @ +0x34
    self->mTargetHistorySamples = 2;       // @ +0x49 (the 2 interpolation taps)
    return 1;
}

// -------------------------------------------------------------------------------------
// SetResampleIncrement @0x82B9A860 -- convert the resample ratio to a 16.16 fixed-point
// increment (round-to-nearest), clamp it (and the stored ratio) to 4x, and cache the raw
// ratio for change detection.
//   inc = round(ratio * 65536);
//   if (inc > 0x40000) { inc = 0x40000; self->mfResampleRatio = 4.0; }
//   else               { self->mfResampleRatio = ratio; }
//   self->mfLastRatio = ratio; self->muIncrementFixed = inc;
// -------------------------------------------------------------------------------------
Resample *Resample::SetResampleIncrement(Resample *self, f32 ratio)
{
    f32 scaled = ratio * 65536.0f;                                   // flt_8217F368
    scaled = (scaled < 0.0f) ? (scaled - 0.5f) : (scaled + 0.5f);    // round to nearest
    int increment = static_cast<int>(scaled);                        // fctiwz (truncate)

    // 0x40000 == KU_RESAMPLE_UNITY * KU_MAX_RESAMPLE_RATIO (4x in 16.16).
    if (increment > (KU_RESAMPLE_UNITY * KU_MAX_RESAMPLE_RATIO))
    {
        increment = KU_RESAMPLE_UNITY * KU_MAX_RESAMPLE_RATIO;
        self->mActualResampleRatio = static_cast<f32>(KU_MAX_RESAMPLE_RATIO); // flt_82004EF4 @ +0x30
    }
    else
    {
        self->mActualResampleRatio = ratio;  // @ +0x30
    }
    // The REQUESTED (unclamped) ratio is what PreProcess change-detects against, so a
    // pitch pinned above 4x recomputes once and then compares equal.
    self->mRequestedResampleRatio = ratio;              // @ +0x34
    self->mIncr16_16 = static_cast<u32>(increment);     // @ +0x3C
    return self;
}

// -------------------------------------------------------------------------------------
// PreProcess @0x82B9AC10 -- recompute the increment if the (outputRate/inputRate)*pitch
// ratio has changed, work out this block's produced sample count, latch the requested
// count, and scale the context's downstream gain by the clamped ratio.
// -------------------------------------------------------------------------------------
int Resample::PreProcess(Resample *self, AudioProcessContext *ctx, int /*a3*/,
                         int outputSamples)
{
    const f32 outputRate = ctx->mpFormat->mfSampleRate;             // *(ctx+0x30018)->+0x0C
    const f32 ratio = (self->mPreviousSampleRate / outputRate)
                    * self->mAttribute[ATTRIBUTE_SETPITCH].mfValue;
    if (self->mRequestedResampleRatio != ratio)
        SetResampleIncrement(self, ratio);

    // How many INPUT samples this stage must pull to yield `outputSamples`: the phase the
    // accumulator will reach, less what history already holds, plus the kernel's taps.
    const u32 phase = self->mIncr16_16 * static_cast<u32>(outputSamples) + self->mAcc16_16;
    int result = static_cast<int>(phase >> 16)
               - self->mCurrentHistorySamples + self->mTargetHistorySamples;
    if (result < 0)
        result = 0;

    self->mOutputSamplesRequested = static_cast<u16>(outputSamples);       // sth r6 @ +0x46
    ctx->mfResampleGain = self->mActualResampleRatio * ctx->mfResampleGain; // ctx+0x30028 *= +0x30
    return result;
}

// -------------------------------------------------------------------------------------
// GetOutputSamples @0x82B9A8D0 -- given `inputSamples` available, how many output samples
// this stage yields. Returns 0 if the input can't cover the kernel latency, 0x2000 while
// the increment is still unset, else floor(((usable<<16) - fraction - 1) / increment).
// -------------------------------------------------------------------------------------
u32 Resample::GetOutputSamples(Resample *self, int inputSamples)
{
    const int usable = inputSamples - self->mTargetHistorySamples + 1;
    if (usable <= 0)
        return 0;

    const u32 increment = self->mIncr16_16;
    if (increment == 0)
        return KU_MAX_OUTPUTSIZE;   // 0x2000 -- the vendor MAX_OUTPUTSIZE

    const u32 fraction = self->mAcc16_16;
    // asm `twllei increment, 0` is the compiler's div-by-zero trap; already guarded above.
    return ((static_cast<u32>(usable) << 16) - fraction - 1u) / increment;
}

// -------------------------------------------------------------------------------------
// LinearInterpolate @0x82B9A918 -- the 2-tap interpolation kernel. RAW-DECODED from the
// decrypted XEX (file_off 0xB9D918); the IDA export has no dossier for this address, so all
// 190 instructions were disassembled and walked directly.
//
// The console body is 8x unrolled: it derives eight independent phases from the entering
// accumulator (frac + j*inc for j in 0..7), runs the 8-sample block while at least eight
// outputs remain, then a 1-at-a-time tail. Both loops are the identical lerp, so the host
// expresses the kernel ONCE -- the unrolling is a scheduling detail with no observable
// effect, and reproducing it here would only obscure the arithmetic. What IS observable and
// IS reproduced: the exact 16.16 phase arithmetic (u32 wrap included), the tap pair, the
// scale constant's exact bit pattern, and both out-parameter writes.
//
// Register contract at Process's call site (0x82B9F51C..0x82B9F54C), matching the vendor
// signature: r3=this, r4=frames, r5=pSrc, r6=pDst, r7=pAccWhole, r8=pAccFrac, r9=inc.
// -------------------------------------------------------------------------------------
void Resample::LinearInterpolate(u32 frames, const f32 *pSrc, f32 *pDst,
                                 u32 *pAccWhole, u32 *pAccFrac, u32 inc)
{
    // `lwz r10, 0(pAccWhole)` -- the running count of whole input samples consumed.
    u32 luWhole = *pAccWhole;
    // `lhz r11, 0(pAccFrac)` -- a HALFWORD load of the big-endian high half, i.e. the
    // caller's `accumulator << 16` read back as the 16-bit fraction (see the tail store).
    u32 luFrac = *pAccFrac >> 16;

    for (u32 luSample = 0; luSample < frames; ++luSample)
    {
        // The two interpolation taps at the current whole position.
        const f32 lfS0 = pSrc[luWhole];
        const f32 lfS1 = pSrc[luWhole + 1];

        // The fraction is converted through fcfid (int64 -> double) + frsp, then scaled.
        // KF_PHASE_SCALE is the console's rounded 1.5258e-5f, NOT 1/65536 -- see its note.
        const f32 lfT = static_cast<f32>(static_cast<s64>(luFrac)) * KF_PHASE_SCALE;

        pDst[luSample] = (lfS1 - lfS0) * lfT + lfS0;   // fsubs + fmadds

        // Advance the 16.16 phase. The console adds into a 32-bit register and splits with
        // srwi 16 / clrlwi 16, so the carry into the whole count is the top 16 bits and the
        // fraction keeps the low 16 -- u32 arithmetic reproduces it exactly.
        const u32 luPhase = luFrac + inc;
        luWhole += (luPhase >> 16);
        luFrac = luPhase & 0xFFFFu;
    }

    // `slwi r11,r11,16; stw -> 0(pAccFrac)` and `stw r10 -> 0(pAccWhole)`. The caller
    // shifts the fraction back down by 16 when it parks it in mAcc16_16.
    *pAccFrac = luFrac << 16;
    *pAccWhole = luWhole;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9F3E8 -- resample the frame to the output rate. r5 (discontinuity) is never
// read; both paths return BUFFERSTATUS_AVAILABLE (1).
//
// The FORMAT HANDSHAKE comes first: if the rate the upstream source published differs from
// the cached one, this frame is spent adopting it (so the next PreProcess computes a fresh
// increment) and NOTHING else happens -- no carve, no interpolation, no count change, and
// critically NO buffer swap, so the frame passes through untouched.
//
// Otherwise it carves scratch from the System stack allocator, prefixes each channel's
// carried-over history in front of that channel's incoming samples, interpolates into the
// destination, copies the unconsumed tail back to history, publishes the produced count,
// swaps, republishes the output rate, and releases the scratch.
// -------------------------------------------------------------------------------------
int Resample::Process(Resample *self, AudioProcessContext *ctx, bool /*discontinuity*/)
{
    // ---- the format handshake (exact compare; a NaN incoming rate takes this path) ------
    if (!(self->mPreviousSampleRate == ctx->mfSampleRate))
    {
        self->mPreviousSampleRate = ctx->mfSampleRate;          // stfs +0x38
        ctx->mfSampleRate = ctx->mpFormat->mfSampleRate;        // republish the OUTPUT rate
        return 1;
    }

    const u32 luInputFrames = ctx->mNumSamples;                 // lwz ctx+0x30020
    const u8  lu8History = self->mCurrentHistorySamples;        // lbz +0x48
    const u32 luTotalInput = luInputFrames + lu8History;

    // ---- carve the interpolation scratch from the System stack allocator ----------------
    // The console reserves ((4*inputFrames + 0x18) rounded up to 128) bytes by LOWERING the
    // allocator's top pointer, and restores the saved top on the way out. The 0x18 headroom
    // is MAX_RESAMPLE_HISTORY floats -- the prefix this frame may prepend.
    // `lwz r11,4(self)` -> the System, then `lwz r11,0(r11)` -> its StackAllocator (the
    // console's System::mpObjectTable slot 0), then `lwz r18,0xC(r11)` -> the live top.
    System *lpSystem = static_cast<System *>(self->mBase.mpSystem);
    StackAllocator *lpAllocator = *reinterpret_cast<StackAllocator **>(lpSystem);
    u8 *lpSavedTop = lpAllocator->mpTop;
    const u32 luReservation =
        (4u * luInputFrames + 0x18u + 0x7Fu) & ~static_cast<u32>(0x7Fu);
    u8 *lpScratchBytes = lpSavedTop - luReservation;
    f32 *lpScratch = reinterpret_cast<f32 *>(lpScratchBytes);
    lpAllocator->mpTop = lpScratchBytes;

    // How many output samples the available input yields, capped at what PreProcess asked
    // for (the clamp is downward only).
    u32 luOutputFrames = GetOutputSamples(self, static_cast<int>(luTotalInput));
    if (luOutputFrames > self->mOutputSamplesRequested)
        luOutputFrames = self->mOutputSamplesRequested;

    SampleBuffer *lpSrc = ctx->mpSrcBuffer;
    SampleBuffer *lpDst = ctx->mpDstBuffer;
    f32 *lpHistory = self->GetHistoryBuffer();

    u32 luResidual = 0;
    u32 luAccumulator = self->mAcc16_16 << 16;   // the kernel's packed fraction form

    for (u32 luChannel = 0; luChannel < self->mBase.mbChannelCount; ++luChannel)
    {
        // Each channel's history cursor advances by MAX_RESAMPLE_HISTORY floats (0x18).
        f32 *lpChannelHistory = lpHistory + KU_MAX_RESAMPLE_HISTORY * luChannel;

        // The carried-over samples go in FRONT of this frame's input, so the kernel sees one
        // continuous stream across the frame boundary.
        const u32 luHistoryThisChannel = self->mCurrentHistorySamples;
        if (luHistoryThisChannel != 0)
        {
            std::memcpy(lpScratch, lpChannelHistory,
                        sizeof(f32) * luHistoryThisChannel);          // _blkmov
        }
        std::memcpy(lpScratch + luHistoryThisChannel,
                    lpSrc->mpSamples + lpSrc->muStride * luChannel,
                    sizeof(f32) * luInputFrames);                     // XMemCpy

        f32 *lpDstChannel = lpDst->mpSamples + lpDst->muStride * luChannel;

        // Every channel restarts from the SAME entering accumulator; the kernel's two
        // out-params are re-read per channel so the last channel's values are the ones the
        // instance keeps (all channels consume identically, so they agree).
        u32 luWholeConsumed = 0;
        luAccumulator = self->mAcc16_16 << 16;
        self->LinearInterpolate(luOutputFrames, lpScratch, lpDstChannel,
                                &luWholeConsumed, &luAccumulator, self->mIncr16_16);

        // Whatever the kernel did not consume becomes this channel's history for next frame.
        luResidual = luTotalInput - luWholeConsumed;
        if (luResidual != 0)
        {
            std::memcpy(lpChannelHistory, lpScratch + luWholeConsumed,
                        sizeof(f32) * luResidual);                    // _blkmov
        }
    }

    self->mCurrentHistorySamples = static_cast<u8>(luResidual);  // stb +0x48
    self->mAcc16_16 = luAccumulator >> 16;                        // stw +0x40

    ctx->mNumSamples = luOutputFrames;                            // stw ctx+0x30020
    SampleBuffer *lpTemp = ctx->mpSrcBuffer;                      // the ping-pong
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = lpTemp;
    ctx->mfSampleRate = ctx->mpFormat->mfSampleRate;              // now at the output rate

    lpAllocator->mpTop = lpSavedTop;                              // release the scratch
    return 1;                                                     // BUFFERSTATUS_AVAILABLE
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82BA1C68
//   self->mBase.mpVTable = off_820AA810; // reinstall the base PlugIn vtable
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Resample::VectorDeletingDestructor(Resample *self, char flags)
{
    self->mBase.mpVTable = KRS_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
