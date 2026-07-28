// =====================================================================================
// rw::audio::core::SinePlayer -- the sine-tone generator source plug-in bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative for every
// store and branch. See plugins/SinePlayer.h for the byte-exact layout and the per-function
// X360 addresses. The synthesis maths is scalar (fdivs/fmuls/fadds/fsubs + libc sin); the
// rodata immediates are ordinary constants (2pi, 1.0), reproduced below.
//
// The two vtable data symbols the constructor / deleting destructor install are opaque
// pointers in the XEX (off_8217F544 = the SinePlayer v-table, off_820AA810 = the shared base
// PlugIn v-table reinstalled on teardown). They are modelled as honest null placeholders so
// the bodies link without fabricating their contents -- they are installed/reinstalled only,
// never dereferenced here.
// =====================================================================================

#include "rw/audio/core/plugins/SinePlayer.h"

#include <cmath> // std::sin (the X360 `sin` libc call)

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
// rodata immediates (grounded by the asm's flt_* loads; loaded single-precision via lfs).
const f32 KF_TWO_PI = 6.2831855f; // flt_82001C94 (2pi)
const f32 KF_ONE    = 1.0f;       // flt_82001C98
const f32 KF_ZERO   = 0.0f;       // flt_82001CC0

// off_8217F544 -- the SinePlayer run-time v-table installed by CreateInstance.
// off_820AA810 -- the base PlugIn v-table the deleting destructor reinstalls (shared with
// Gain / RawPuller2 / Pan2D1 / ReverbModel1). Opaque data symbols in the XEX; modelled as
// honest placeholders (installed/reinstalled only, never dereferenced here).
void *const KVTABLE_SINEPLAYER = nullptr; // off_8217F544
void *const KVTABLE_PLUGIN_BASE = nullptr; // off_820AA810
} // namespace

// ---------------------------------------------------------------------------
// CreateInstance @0x82BA3FB8 -- placement-construct. Install the vtable (only when self is
// non-null, per the asm's guard), clear the phase, point the base attribute pointer at the
// frequency attribute, clear the frequency, and clear the playing flag. Returns 1.
// ---------------------------------------------------------------------------
int SinePlayer::CreateInstance(SinePlayer *self)
{
    if (self)
        self->mBase.mpVTable = KVTABLE_SINEPLAYER; // off_8217F544
    self->mfPhase = KF_ZERO;                         // +0x38
    self->mBase.mpAttributes = &self->mfFrequency;   // +0x0C -> &(+0x30)
    self->mfFrequency = KF_ZERO;                      // +0x30
    self->mbPlaying = 0;                              // +0x3C
    return 1;
}

// ---------------------------------------------------------------------------
// GetSize @0x82B98308 -- the plug-in instance footprint the factory allocates.
//   li r3, 0x48
// This is the CONSOLE footprint (72), NOT host sizeof(SinePlayer) (96 on x64 -- the five
// PlugInBaseView pointers widened). The constant is reproduced verbatim to match the committed
// sibling plug-ins (RawPuller2::GetSize 56, Delay::GetSize 184, Limiter1 168, Pan2D 240,
// ReverbModel1 1088); switching this family to host sizeof is a tree-wide decision, not a
// per-TU one. Unlike the command-ring handlers below, this value is not a cursor stride.
// NOTE: Compressor1::GetSize has already moved to host sizeof (its console 192 under-allocates
// the 216-byte host object by 24 bytes), so the family is no longer uniform -- the sweep of the
// remaining plug-ins is a filed follow-up, not evidence that the console literal is correct.
// ---------------------------------------------------------------------------
int SinePlayer::GetSize()
{
    return 72; // li r3, 0x48 -- X360 instance footprint, not host sizeof(SinePlayer)
}

// ---------------------------------------------------------------------------
// PlayHandler @0x82B9B900 -- replay of a queued Play command. Latch the command's start
// time into the target's mdStartTime (a double) and raise the playing flag.
//   target = cmd->mpTarget; target->mdStartTime = cmd->mdStartTime; target->mbPlaying = 1;
//   return the command record size (the ring advance).
// ---------------------------------------------------------------------------
int SinePlayer::PlayHandler(void *lpCommand)
{
    SinePlayerPlayCommand *cmd = reinterpret_cast<SinePlayerPlayCommand *>(lpCommand);
    SinePlayer *self = cmd->mpTarget;      // *(a1 + 4)
    self->mdStartTime = cmd->mdStartTime;  // stfd f0, 0x28  (double from *(a1 + 8))
    self->mbPlaying = 1;                    // stb 1, 0x3C
    // RECORD STRIDE (X360-literal trap): the asm's `li r3,0x10` IS the console
    // sizeof(SinePlayerPlayCommand) ({int(*)(void*), SinePlayer*, f64} = 4+4+8). The ring
    // consumer System::ExecuteCommands advances lpCursor by exactly this return, so it must
    // be the HOST sizeof (24 on x64) -- returning the literal 16 would leave the cursor 8
    // bytes short and the next iteration would read a SystemCommandHeader straddling the
    // middle of this record. Any future SinePlayer producer (an EventEvent) must advance the
    // cursor by the same sizeof, never by 16.
    return static_cast<int>(sizeof(SinePlayerPlayCommand)); // X360: li r3,0x10 @0x82B9B90C
}

// ---------------------------------------------------------------------------
// StopHandler @0x82B9B920 -- replay of a queued Stop command. Clear the target's playing
// flag.
//   cmd->mpTarget->mbPlaying = 0; return the command record size (the ring advance).
// ---------------------------------------------------------------------------
int SinePlayer::StopHandler(void *lpCommand)
{
    SinePlayerStopCommand *cmd = reinterpret_cast<SinePlayerStopCommand *>(lpCommand);
    cmd->mpTarget->mbPlaying = 0; // stb 0, 0x3C
    // RECORD STRIDE (X360-literal trap): the asm's `li r3,8` IS the console
    // sizeof(SinePlayerStopCommand) ({int(*)(void*), SinePlayer*} = 4+4). System::
    // ExecuteCommands advances the ring cursor by this return, so it must be the HOST sizeof
    // (16 on x64) -- an 8-byte advance over a 16-byte record would land the cursor on this
    // record's mpTarget and call the SinePlayer pointer as the next command handler. Any
    // future producer must advance by the same sizeof, never by 8.
    return static_cast<int>(sizeof(SinePlayerStopCommand)); // X360: li r3,8 @0x82B9B928
}

// ---------------------------------------------------------------------------
// PreProcess @0x82B9B938 -- stow the requested sample count (the 4th argument). Returns 0.
//   self->mSamplesRequested = a4;
// ---------------------------------------------------------------------------
int SinePlayer::PreProcess(SinePlayer *self, int /*a2*/, int /*a3*/, int a4)
{
    self->mSamplesRequested = a4; // stw r6, 0x40
    return 0;
}

// ---------------------------------------------------------------------------
// Process @0x82B9B948 -- per-frame entry. First publish this frame's descriptor into the
// context (sample count / channel count / sample rate). If not playing, return 0. Otherwise
// synthesise one output sample per requested sample: samples whose stream time precedes the
// scheduled start time are written as silence, the rest carry the running phase's sine
// (written to every channel). The phase accumulator advances by (rate/sampleRate)*freq*2pi
// per sample and wraps at 2pi. Finally ping-pong the context's src/dst buffer slots.
//   r3 == self, r4 == ctx.
// ---------------------------------------------------------------------------
int SinePlayer::Process(SinePlayer *self, AudioProcessContext *ctx)
{
    AudioFormat        *pFormat = ctx->mpFormat;   // v7 = *(ctx + 0x30018)
    AudioChannelBuffer *pDst    = ctx->mpDstBuffer; // v8 = *(ctx + 0x30010)

    // Publish this frame's descriptor into the context (before the playing gate).
    ctx->mNumSamples    = static_cast<u32>(self->mSamplesRequested); // +0x30020
    ctx->mbChannelCount = self->mBase.mbChannelCount;                // +0x3002C
    ctx->mfSampleRate   = pFormat->mfSampleRate;                     // +0x30024 = *(v7 + 0x0C)

    if (!self->mbPlaying) // +0x3C
        return 0;

    const int channelCount = self->mBase.mbChannelCount; // +0x21
    const int frameCount   = self->mSamplesRequested;     // +0x40

    // Per-sample phase increment: (rateScale / sampleRate) * frequency * 2pi. The rate scale
    // is the context's downstream gain slot (mfResampleGain @+0x30028), read as-is.
    const f32 phaseInc = static_cast<f32>(
        ((ctx->mfResampleGain / pFormat->mfSampleRate) * self->mfFrequency) * KF_TWO_PI);

    if (frameCount > 0)
    {
        for (int sample = 0; sample < frameCount; ++sample)
        {
            // Stream time of this output sample.
            const f32 invRate    = KF_ONE / pFormat->mfSampleRate;               // fdivs (single)
            const f64 sampleTime = static_cast<f64>(invRate * static_cast<f32>(sample))
                                   + ctx->mdStreamTime;                          // + *(ctx + 0x30000)

            if (sampleTime < self->mdStartTime) // +0x28
            {
                // Not started yet: write silence to every channel.
                for (int ch = 0; ch < channelCount; ++ch)
                {
                    const int idx = pDst->muStride * ch + sample;
                    pDst->mpSamples[idx] = KF_ZERO;
                }
            }
            else
            {
                // Emit the current phase's sine into every channel (recomputed per channel,
                // as the compiler did not hoist the libc sin call).
                for (int ch = 0; ch < channelCount; ++ch)
                {
                    const int idx = pDst->muStride * ch + sample;
                    pDst->mpSamples[idx] = static_cast<f32>(std::sin(static_cast<f64>(self->mfPhase)));
                }
            }

            // Advance and wrap the phase accumulator.
            const f32 nextPhase = self->mfPhase + phaseInc; // fadds
            self->mfPhase = nextPhase;
            if (nextPhase >= KF_TWO_PI)
                self->mfPhase = nextPhase - KF_TWO_PI; // fsubs
        }
    }

    // Ping-pong the src/dst buffer slots for the next stage (src <- old dst, dst <- old src).
    AudioChannelBuffer *pOldDst = ctx->mpDstBuffer; // r10 = *(ctx + 0x30010)
    AudioChannelBuffer *pOldSrc = ctx->mpSrcBuffer; // r9  = *(ctx + 0x3000C)
    ctx->mpSrcBuffer = pOldDst;                     // *(ctx + 0x3000C) = r10
    ctx->mpDstBuffer = pOldSrc;                     // *(ctx + 0x30010) = r9
    return 1;
}

// ---------------------------------------------------------------------------
// `vector deleting destructor' @0x82BA1DA0 -- reinstall the base PlugIn v-table, then
// conditionally free. (~SinePlayer is trivial and folded away -- the asm performs no member
// teardown, only the vtable reinstall.)
//   self->mBase.mpVTable = off_820AA810;
//   if (flags & 1) operator delete(self);
//   return self;
// ---------------------------------------------------------------------------
void *SinePlayer::VectorDeletingDestructor(SinePlayer *self, char flags)
{
    self->mBase.mpVTable = KVTABLE_PLUGIN_BASE; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
