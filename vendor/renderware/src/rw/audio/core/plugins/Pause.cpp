// =====================================================================================
// rw::audio::core::Pause bodies -- the "pause / fade-gate" audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. The vendor pause.h (references/Feb-2007/.../
// rwaudiocore/2.11.00/include/rw/audio/core/plugins/pause.h) supplies the authoritative
// enum/constant names and matches the layout member-for-member.
//   CreateInstance            @0x82BA36B8 -- store-for-store
//   GetPlugInDescRunTime      @0x82B9A130 -- returns the registered "Pause" descriptor
//   GetSize                   @0x82B982D0 -- console 0x40; host sizeof (the stage carve)
//   PreProcess                @0x82B9A140 -- store-for-store / branch-for-branch
//   Process                   @0x82B9A218 -- branch-for-branch (phase E callback wave)
//   `scalar deleting destructor' @0x82BA1B48 -- reinstalls base vtable, conditional free
// See plugins/Pause.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/plugins/Pause.h"
#include "rw/audio/core/Mixer.h"   // the process context (src/dst slots, mNumSamples)

#include <cstring> // std::memset (the X360 XMemSet)

namespace rw
{
namespace audio
{
namespace core
{

// flt_82001CC0 == 0.0f, flt_82001C98 == 1.0f (the two compare/init immediates).
static const f32 KF_ZERO = 0.0f;
static const f32 KF_ONE = 1.0f;
// flt_820037C8 == -1.0f -- forms the fade-DOWN numerator (-mGain) in Process.
static const f32 KF_MINUS_ONE = -1.0f;

// off_8217F4C4 -- the Pause v-table installed at construction. off_820AA810 -- the base
// PlugIn v-table the scalar deleting destructor reinstalls before any free. These are
// opaque data symbols in the XEX (no exported contents); modelled as honest placeholder
// storage so the bodies below link without fabricating their contents.
static void *const KPP_PauseVTable = nullptr;        // off_8217F4C4
static void *const KPP_BasePlugInVTable = nullptr;   // off_820AA810

// off_82F8F510 -- the "Pause" runtime descriptor, REAL (descriptor-record wave; record
// dump progress/scratch_dossiers/plugindesc_layout_codex.md). One of only three registered
// plug-ins with a LIVE PreProcess slot. Metadata FLAG'd null per the descriptor-wave
// convention (no committed consumer reads them).
static PlugInDescRunTime g_PauseDesc = {
    "Pause",
    reinterpret_cast<void *>(&Pause::GetSize),         // @0x82B982D0
    reinterpret_cast<void *>(&Pause::CreateInstance),  // @0x82BA36B8
    reinterpret_cast<void *>(&Pause::PreProcess),      // @0x82B9A140
    reinterpret_cast<void *>(&Pause::Process),         // @0x82B9A218
    0, 0, 0, 0,
    0,
    0x50617530u,       // 'Pau0'
    3, 0, 1, 0, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetSize @0x82B982D0 -- the plug-in instance footprint.
// -------------------------------------------------------------------------------------
int Pause::GetSize()
{
    // X360-LITERAL TRAP (the stage-carve audit): the console immediate under-allocates the
    // widened host object -- GetSize is the stage factory's allocation stride, so return
    // host sizeof (the RawPuller2/Send/Rechannel precedent).
    return static_cast<int>(sizeof(Pause));   // X360: li r3, 0x40 (64)
}

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9A130 -- return the address of the registered descriptor
// record (the run-time-type entry the factory consults; its label is the string "Pause").
// -------------------------------------------------------------------------------------
char **Pause::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_PauseDesc); // &off_82F8F510
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA36B8 -- placement-init a Pause over `self`.
//   if (self) self->mpVTable = off_8217F4C4;
//   self->mpAttribute(+0x0C) = &self->mAttribute[0](+0x28);
//   self->mAttribute[0].mfValue = 0.0;  self->mGain = 1.0;
//   self->mPauseState = 2;      self->mDiscontinuity = 0;
//   return 1;
// -------------------------------------------------------------------------------------
int Pause::CreateInstance(Pause *self)
{
    if (self)
        self->mBase.mpVTable = KPP_PauseVTable; // off_8217F4C4

    // *(self+0x0C) = self+0x28 : point the base attribute-table slot at mAttribute[0].
    // (Now a by-name store through the shared base view; it used to be a raw offset into
    // an opaque byte blob.)
    self->mBase.mpAttributes = &self->mAttribute[0];

    self->mAttribute[ATTRIBUTE_SETPAUSECONTROL].mfValue = KF_ZERO; // flt_82001CC0 @ +0x28
    self->mPauseState = STATE_UNPAUSED; // li r8,2; stb @ +0x37
    self->mGain = KF_ONE;               // flt_82001C98 @ +0x30
    self->mDiscontinuity = 0;           // li r9,0; stb @ +0x38
    return 1;
}

// -------------------------------------------------------------------------------------
// PreProcess @0x82B9A140 -- decide this block's audible length from the pause amount.
//
// Stores the requested length, then (if forced, or already active) latches the ramp
// state from mfPauseAmount: ==1.0 -> fully open (state 0, length 0 here); ==0.0 ->
// muted (state 2, length 0). With mfPauseAmount==0.0 the block is fully gated: a fresh
// (state 0 or 1) gate passes the full 0x40-sample ramp, otherwise nothing. With
// mfPauseAmount!=0.0 the block passes min(rampLength, requested) for an active ramp
// (states 1..3) and zero once muted (state>=4) or fully open (state 0).
// Branch-for-branch with the asm.
// -------------------------------------------------------------------------------------
int Pause::PreProcess(Pause *self, int /*a2*/, char force, int length)
{
    self->mOutputSamplesRequested = static_cast<u16>(length); // sth r6 @ +0x34

    // if (force != 0) OR (mDiscontinuity != 0): latch the ramp state from the pause amount.
    if ((force & 0xFF) != 0 || self->mDiscontinuity != 0)
    {
        self->mDiscontinuity = 1; // stb r10(1) @ +0x38
        f32 amount = self->mAttribute[ATTRIBUTE_SETPAUSECONTROL].mfValue;
        if (amount == KF_ONE) // fcmpu f0,f13(1.0) -- PAUSECONTROL_PAUSED
        {
            self->mSamplesRemainingUntilStateChange = 0; // li r10,0; stb @ +0x36
            self->mPauseState = STATE_PAUSED;             // stb r10(0) @ +0x37
        }
        else if (amount == KF_ZERO) // fcmpu f0,f12(0.0) -- PAUSECONTROL_UNPAUSED
        {
            self->mSamplesRemainingUntilStateChange = 0; // li r9,0; stb @ +0x36
            self->mPauseState = STATE_UNPAUSED;           // li r10,2; stb @ +0x37
        }
    }

    f32 amount = self->mAttribute[ATTRIBUTE_SETPAUSECONTROL].mfValue;
    u8 state = self->mPauseState;

    if (amount == KF_ZERO) // fully gated path
    {
        if (state == STATE_PAUSED || state == STATE_PAUSING)
            self->mSamplesRemainingUntilStateChange = KU_PAUSE_RAMP_SAMPLES; // stb @ +0x36
        return length;                // mr r3,r6
    }

    // amount != 0.0
    if (state < STATE_PAUSING)
        return 0;       // STATE_PAUSED: nothing this block
    if (state >= 4)
        return 0;       // out-of-range/muted: nothing
    if (state != STATE_PAUSING)  // STATE_UNPAUSED/UNPAUSING: (re)arm the ramp length
        self->mSamplesRemainingUntilStateChange = KU_PAUSE_RAMP_SAMPLES;

    int out = self->mSamplesRemainingUntilStateChange;
    if (out > length)   // clamp to the requested length
        out = length;
    self->mOutputSamplesRequested = static_cast<u16>(out); // sth r10 @ +0x34
    return out;                              // mr r3,r10
}

// -------------------------------------------------------------------------------------
// Process @0x82B9A218 -- gate the block through, ramping the gain when the pause control
// crosses. r5 (discontinuity) is NEVER read; every exit returns BUFFERSTATUS_AVAILABLE (1).
//
// Three shapes:
//   * the two EXACT terminal fast paths (fully paused / fully unpaused), which deliberately
//     do NOT swap the buffer slots -- see below;
//   * the ramp path, which fades every channel from mGain toward its target over the
//     transition, copies any remaining tail unchanged, and DOES swap.
// NaN pause control follows the "pausing" arm (fcmpu leaves EQ clear when unordered, so
// every equality test falls through).
// -------------------------------------------------------------------------------------
int Pause::Process(Pause *self, AudioProcessContext *ctx, bool /*discontinuity*/)
{
    SampleBuffer *lpSrc = ctx->mpSrcBuffer;   // lwz ctx+0x3000C
    SampleBuffer *lpDst = ctx->mpDstBuffer;   // lwz ctx+0x30010
    const f32 lfControl = self->mAttribute[ATTRIBUTE_SETPAUSECONTROL].mfValue; // lfs +0x28

    // ---- fast path A: fully PAUSED and settled -> publish silence, do NOT swap ----------
    if (lfControl == KF_ONE && self->mPauseState == STATE_PAUSED)
    {
        // The requested count is published as the frame count, then each channel of the
        // SOURCE buffer is zeroed. Zeroing the SOURCE (not the destination) with no swap is
        // what makes this correct: the src slot stays the published buffer, so downstream
        // stages read the silence in place.
        const u32 luSamples = self->mOutputSamplesRequested;   // lhz +0x34
        ctx->mNumSamples = luSamples;                          // stwx -> ctx+0x30020
        for (u32 luChannel = 0; luChannel < self->mBase.mbChannelCount; ++luChannel)
        {
            std::memset(lpSrc->mpSamples + lpSrc->muStride * luChannel, 0,
                        sizeof(f32) * luSamples);              // XMemSet @0x82926FD0
        }
        return 1;
    }

    // Every non-fast-path-A route clears the discontinuity byte before going on.
    self->mDiscontinuity = 0;                                  // stb 0 -> +0x38

    // ---- fast path B: fully UNPAUSED and settled -> pass through, do NOT swap -----------
    // (Nothing is written at all: the src slot already holds the audio the next stage wants.)
    if (lfControl == KF_ZERO && self->mPauseState == STATE_UNPAUSED)
        return 1;

    // ---- the ramp path -----------------------------------------------------------------
    const u8  lu8OldRemaining = self->mSamplesRemainingUntilStateChange; // lbz +0x36
    const u32 luFrameSamples = ctx->mNumSamples;                          // lwz ctx+0x30020
    u32 luRampCount = lu8OldRemaining;
    u8  lu8NewState;
    f32 lfNumerator;

    if (lfControl == KF_ZERO)
    {
        // UNPAUSING: fade UP toward 1.0.
        lu8NewState = STATE_UNPAUSED;
        if (lu8OldRemaining > luFrameSamples)
        {
            luRampCount = luFrameSamples;      // this frame only covers part of it
            lu8NewState = STATE_UNPAUSING;
        }
        lfNumerator = KF_ONE - self->mGain;    // fsubs
    }
    else
    {
        // PAUSING (including a NaN control): fade DOWN toward 0.0.
        lu8NewState = STATE_PAUSED;
        if (lu8OldRemaining > luFrameSamples)
        {
            luRampCount = luFrameSamples;
            lu8NewState = STATE_PAUSING;
        }
        lfNumerator = self->mGain * KF_MINUS_ONE; // fmuls by flt_820037C8
    }

    // The per-sample step divides by the ORIGINAL transition length, NOT by the clamped
    // per-frame ramp count -- so a transition split across frames keeps one constant slope.
    const f32 lfStep = lfNumerator / static_cast<f32>(static_cast<s32>(lu8OldRemaining));
    const u32 luTailCount = luFrameSamples - luRampCount;

    // f0 carries the running gain out of the loops; with ZERO channels the console never
    // enters them and the value left in f0 is the preloaded 2.0f, which the state-1/3 store
    // below then parks in mGain. That literal is reproduced rather than "fixed".
    f32 lfGain = 2.0f;   // flt_82001D9C, preloaded into f0 before the channel-count test

    for (u32 luChannel = 0; luChannel < self->mBase.mbChannelCount; ++luChannel)
    {
        const u16 lu16SrcStride = lpSrc->muStride;
        const u16 lu16DstStride = lpDst->muStride;
        const f32 *lpIn = lpSrc->mpSamples + lu16SrcStride * luChannel;
        f32 *lpOut = lpDst->mpSamples + lu16DstStride * luChannel;

        // Every channel restarts from the SAME member gain and the same step.
        lfGain = self->mGain;
        for (u32 luSample = 0; luSample < luRampCount; ++luSample)
        {
            *lpOut++ = *lpIn++ * lfGain;
            lfGain += lfStep;
        }
        // The remaining samples are copied through UNCHANGED -- literal console behaviour
        // even on a block that has already faded fully to paused.
        for (u32 luSample = 0; luSample < luTailCount; ++luSample)
            *lpOut++ = *lpIn++;
    }

    self->mSamplesRemainingUntilStateChange =
        static_cast<u8>(lu8OldRemaining - luRampCount);   // stb +0x36
    self->mPauseState = lu8NewState;                       // stb +0x37

    if (lu8NewState == STATE_UNPAUSED)
        self->mGain = KF_ONE;        // settled open
    else if (lu8NewState == STATE_PAUSED)
        self->mGain = KF_ZERO;       // settled shut
    else
        self->mGain = lfGain;        // mid-transition: the post-ramp running gain

    // Only the ramp path ping-pongs the slots (the destination it just wrote becomes src).
    SampleBuffer *lpTemp = ctx->mpSrcBuffer;
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = lpTemp;
    return 1;                        // BUFFERSTATUS_AVAILABLE
}

// -------------------------------------------------------------------------------------
// `scalar deleting destructor' @0x82BA1B48
//   self->mpVTable = off_820AA810; // reinstall base PlugIn vtable
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Pause::ScalarDeletingDestructor(Pause *self, char flags)
{
    self->mBase.mpVTable = KPP_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
