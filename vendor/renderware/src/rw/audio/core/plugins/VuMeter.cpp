// =====================================================================================
// rw::audio::core::VuMeter bodies -- the "VuMeter" per-channel level-metering plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 rwaudio PDB entry exist for this type. See plugins/VuMeter.h for the byte-exact
// layout and the per-function X360 addresses.
//
// The plug-in reuses the committed audio-core homes TimerHandle / TimerManager / System (it
// does NOT fork them). All eleven ledger functions are reconstructed here.
//
// The X360 UpdateRunningPeakandRMS is compiler-unrolled x4 with a scalar single-sample tail,
// and the TWO PATHS ARE NOT INTERCHANGEABLE: the group path folds the running peak
// (mPeakCurrent) into mPeakHold once per group (0x82BA10E8), while the scalar tail folds the
// SAMPLE MAGNITUDE (0x82BA1160). Those agree only while mPeakHold >= mPeakCurrent, an
// invariant ResetFunc deliberately breaks (it zeroes mPeakHold but leaves mPeakCurrent
// standing), and ResetFunc is reachable at any time through the deferred ResetHandler that
// EventEvent queues. An earlier reconstruction re-rolled both paths into one per-sample loop
// and claimed store-for-store equivalence -- that claim was FALSE and the divergence was
// reachable, so the chunking below reproduces the asm's two paths exactly instead.
// =====================================================================================

#include "rw/audio/core/plugins/VuMeter.h"
#include "rw/audio/core/PlugIn.h"       // rw::audio::core::System (mTimerManager @+0x60, deferred ring)
#include "rw/audio/core/TimerManager.h" // rw::audio::core::TimerManager::AddTimer

#include <cmath> // std::sqrt

namespace rw
{
namespace audio
{
namespace core
{

// off_820AA810 -- the base PlugIn v-table the deleting destructor reinstalls (shared with
// Gain / Limiter1 / Delay / ReverbModel1). The VuMeter v-table installed at construction is
// not exposed in the CreateInstance asm (PlugIn::Initialize<VuMeter> installs it inside its
// own separate TU). Both are opaque data symbols in the XEX; modelled as honest placeholder
// storage so the ctor/dtor link without fabricating their contents.
static void *const KVU_VuMeterVTable    = nullptr; // installed by PlugIn::Initialize<VuMeter>
static void *const KVU_BasePlugInVTable = nullptr; // off_820AA810

// Grounded flt_* immediates (well-known constants; from the asm's flt_* loads).
static const f32 KF_ZERO    = 0.0f;  // flt_82001CC0
static const f32 KF_NEG_ONE = -1.0f; // flt_820037C8 (abs via negate when sample <= 0)

// The default RMS window length (li r9, 0x1DF). CreateInstance seeds it; RwacTimerClient
// restores it if it was ever cleared to 0.
static const u16 KU_DEFAULT_WINDOW = 479; // 0x1DF

// The per-frame sample count Process meters (li r5, 0x100 == MIXER_FRAME_SIZE).
static const int KI_FRAME = 256;

// The deferred-command record EventEvent pushes into the System command ring and ResetHandler
// replays: { handler, target }. Matches the two words written at EventEvent's ring slot
// (*v3 = &ResetHandler; v3[1] = self). RECORD STRIDE (X360-literal trap): the console record
// is 8 bytes (4+4) because X360 pointers are 4 bytes; on the host the same two pointers make
// it 16, so both the producer's cursor advance and ResetHandler's return must be the HOST
// sizeof -- never the literal 8. See the RECORD STRIDE notes on EventEvent / ResetHandler.
namespace
{
struct VuMeterResetCommand
{
    int (*mpHandler)(void *); // +0x00 -- always &VuMeter::ResetHandler
    VuMeter *mpTarget;        // +0x04 -- the VuMeter to reset
};
} // namespace

// -------------------------------------------------------------------------------------
// GetSize @0x82B98360 -- the plug-in instance footprint (X360 constant).
//   li r3, 0x150
// -------------------------------------------------------------------------------------
int VuMeter::GetSize()
{
    // X360-LITERAL TRAP: the console immediate is this object's CONSOLE footprint, but
    // GetSize is the plug-in factory's allocation stride -- it allocates GetSize() bytes and
    // constructs the object into them. On the host the object is larger (widened vptr and
    // pointers), so returning the console value under-allocates and corrupts what follows.
    // Return host sizeof; the console immediate stays in the comment above.
    return static_cast<int>(sizeof(VuMeter));   // X360: li r3, 0x150
// 0x150
}

// -------------------------------------------------------------------------------------
// ResetFunc @0x82B9D038 -- clear the momentary state: zero all 18 display slots (rms / peak /
// hold bands) and the per-channel RMS-result / last-window-peak / hold-peak columns. (The
// rms-accumulator and running-peak columns are NOT touched here -- CreateInstance clears
// those; a mid-run reset intentionally keeps the in-flight window accumulation.)
// -------------------------------------------------------------------------------------
VuMeter *VuMeter::ResetFunc(VuMeter *self)
{
    // Loop 1: the 18 contiguous 8-byte display level slots (bands rms / peak / hold).
    for (int i = 0; i < KI_MAX_CHANNELS; ++i)
    {
        self->mRmsDisplay[i].mfLevel  = KF_ZERO;
        self->mPeakDisplay[i].mfLevel = KF_ZERO;
        self->mHoldDisplay[i].mfLevel = KF_ZERO;
    }

    // The scattered stores: the RMS-result, last-window-peak and hold-peak columns.
    for (int i = 0; i < KI_MAX_CHANNELS; ++i)
    {
        self->mRms[i]      = KF_ZERO; // +0xE8
        self->mPeak[i]     = KF_ZERO; // +0x118
        self->mPeakHold[i] = KF_ZERO; // +0x130
    }

    return self;
}

// -------------------------------------------------------------------------------------
// ResetHandler @0x82B9D130 -- the deferred-command trampoline queued by EventEvent. Reset the
// target VuMeter and return the command record's own size, which System::ExecuteCommands uses
// as the ring advance (lpCursor += header->mpHandler(lpCursor)).
//   console form: ResetFunc(*(cmd + 4)); return 8;   <- the 8 is CONSOLE sizeof, see below
// RECORD STRIDE (X360-literal trap): the asm's `li r3,8` @0x82B9D144 IS the console
// sizeof(VuMeterResetCommand) ({int(*)(void*), VuMeter*} = 4+4). On the host the two widened
// pointers make the same record 16 bytes, so returning the literal 8 would step the consumer
// into the middle of this record and misparse every following command -- the return must be
// the HOST sizeof, exactly as the committed twin System::ReleaseHandler does for the same
// immediate.
// -------------------------------------------------------------------------------------
int VuMeter::ResetHandler(void *cmd)
{
    ResetFunc(static_cast<VuMeterResetCommand *>(cmd)->mpTarget);
    return static_cast<int>(sizeof(VuMeterResetCommand)); // X360: li r3,8 @0x82B9D144
}

// -------------------------------------------------------------------------------------
// EventEvent @0x82BA0F80 -- queue a deferred ResetHandler command into the owning System's
// deferred-command ring (base @+0x20, byte cursor @+0x10B8, advanced by one record). The
// reset is thus applied on the audio thread at the next ring drain, not inline. The cursor is
// advanced BEFORE the fields are written, exactly as the asm orders the stores.
// -------------------------------------------------------------------------------------
VuMeter *VuMeter::EventEvent(VuMeter *self)
{
    System *pSystem = reinterpret_cast<System *>(self->mBase.mpSystem);

    VuMeterResetCommand *pCmd = reinterpret_cast<VuMeterResetCommand *>(
        pSystem->mpDeferredRingBase + pSystem->muDeferredRingCursor);
    // RECORD STRIDE (X360-literal trap): the asm's `addi r10,r10,8` IS the console
    // sizeof(VuMeterResetCommand) (4+4); on the host the record is 16 bytes and
    // System::ExecuteCommands replays the ring by the handler's return -- ResetHandler above
    // likewise returns sizeof(VuMeterResetCommand) -- so the enqueue stride must be the HOST
    // sizeof. Advancing by the literal 8 would let the next producer overwrite mpTarget.
    pSystem->muDeferredRingCursor +=
        static_cast<u32>(sizeof(VuMeterResetCommand)); // X360: addi r10,r10,8 @0x82BA0F98

    pCmd->mpHandler = &VuMeter::ResetHandler;
    pCmd->mpTarget  = self;
    return self;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA0EA8 -- placement-init a VuMeter over `self`.
//
// PlugIn::Initialize<VuMeter>(self, 0x40) constructs the plug-in base and bases its attribute
// table at self+0x40 (mRmsDisplay). The templated PlugIn::Initialize helper lives in another
// (separate) TU; its locally-observable effect (v-table install + attribute-base wiring) is
// reproduced inline here, exactly as the Gain / Limiter1 / Iir2* / Delay shapes do. FLAGGED:
// the real Initialize<T> also performs the base mpSystem/mpVoice wiring from the owning
// voice/factory.
//
// Then the meter seeds the default window length, clears all display + metering state, and
// registers the per-frame decay timer. Returns 1 on success, 0 if the timer registration
// fails.
// -------------------------------------------------------------------------------------
int VuMeter::CreateInstance(VuMeter *self)
{
    // PlugIn::Initialize<VuMeter>(self, 0x40) -- observable effect reproduced inline.
    self->mBase.mpVTable     = KVU_VuMeterVTable;       // installed by Initialize (symbol hidden)
    self->mBase.mpAttributes = &self->mRmsDisplay[0];   // attribute table base @ self+0x40

    self->mWindowSize       = KU_DEFAULT_WINDOW; // sth +0x148 = 479
    self->mbTimerRegistered = 0;                 // stb +0x14D

    // Loop 1: zero the 18 display level slots.
    for (int i = 0; i < KI_MAX_CHANNELS; ++i)
    {
        self->mRmsDisplay[i].mfLevel  = KF_ZERO;
        self->mPeakDisplay[i].mfLevel = KF_ZERO;
        self->mHoldDisplay[i].mfLevel = KF_ZERO;
    }

    self->mbUpdated = 0; // stb +0x14C
    ResetFunc(self);
    self->mWindowPos = 0; // sth +0x14A

    // Loop 2: zero the RMS-accumulator and running-peak columns.
    for (int i = 0; i < KI_MAX_CHANNELS; ++i)
    {
        self->mRmsAccum[i]    = KF_ZERO; // +0xD0
        self->mPeakCurrent[i] = KF_ZERO; // +0x100
    }

    // Register the per-frame decay timer on the System's embedded TimerManager (@+0x60).
    System *pSystem = reinterpret_cast<System *>(self->mBase.mpSystem);
    if (TimerManager::AddTimer(
            &pSystem->mTimerManager, &self->mTimer,
            reinterpret_cast<TimerManager::TimerCallback>(&VuMeter::RwacTimerClient),
            self, "VuMeter", 1, 1))
    {
        return 0; // AddTimer non-zero == registration failed
    }

    self->mbTimerRegistered = 1;
    return 1;
}

// -------------------------------------------------------------------------------------
// `scalar deleting destructor' @0x82B9EB58 -- destruct the embedded TimerHandle, reinstall the
// base PlugIn v-table, then conditionally free.
//
// The leading teardown call (r3 = self+0x24 = &mTimer -> "STUB") targets the image's shared
// empty thunk: ~TimerHandle is trivial (all-POD members), folded to the no-op, so the call is
// reproduced but does nothing (same resolution as Delay's destructor).
//   ~TimerHandle(&mTimer); mBase.mpVTable = off_820AA810; if (flags & 1) operator delete(self);
// -------------------------------------------------------------------------------------
void *VuMeter::ScalarDeletingDestructor(VuMeter *self, char flags)
{
    self->mTimer.~TimerHandle(); // bl STUB -- trivial/no-op ~TimerHandle

    self->mBase.mpVTable = KVU_BasePlugInVTable; // stw off_820AA810 @ +0x00

    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82B9D0A8 -- if the decay timer was registered, remove it through the
// plug-in's own System (mBase.mpSystem). (The X360 tail-calls System::RemoveTimer; its result
// is a value callers discard.)
// -------------------------------------------------------------------------------------
int VuMeter::ReleaseEvent(VuMeter *self)
{
    if (self->mbTimerRegistered == 1)
        System::RemoveTimer(reinterpret_cast<System *>(self->mBase.mpSystem), &self->mTimer);

    return 0;
}

// -------------------------------------------------------------------------------------
// RwacTimerClient @0x82B9D0C8 -- the per-frame decay timer callback. Restore the window length
// if it was cleared, then: if Process ran since the last tick (mbUpdated) just clear the flag;
// otherwise decay the momentary display (RMS + peak bands) to silence, leaving the hold band
// intact. (Registered via a function-pointer cast; its return value is discarded by the
// scheduler.)
// -------------------------------------------------------------------------------------
VuMeter *VuMeter::RwacTimerClient(VuMeter *self)
{
    if (self->mWindowSize == 0)
        self->mWindowSize = KU_DEFAULT_WINDOW;

    if (self->mbUpdated)
    {
        self->mbUpdated = 0;
    }
    else
    {
        // No frame processed this tick: drop the momentary RMS + peak display to zero (the
        // hold band is deliberately left standing).
        for (int i = 0; i < KI_MAX_CHANNELS; ++i)
        {
            self->mRmsDisplay[i].mfLevel  = KF_ZERO;
            self->mPeakDisplay[i].mfLevel = KF_ZERO;
        }
    }

    return self;
}

// -------------------------------------------------------------------------------------
// UpdateRunningPeakandRMS @0x82BA0FB0 -- accumulate this frame's per-channel peak / hold peak
// and windowed RMS. For each active channel, walk the frame's samples: track the running peak
// (max |sample|), fold into the all-time hold peak, and sum the squares; when the window
// (mWindowSize samples) fills, latch the window peak, compute RMS = sqrt(sum/window), and
// reset the accumulator / running peak / window position.
//
//   pSrc == the source channel buffer (ctx->mpSrcBuffer); numSamples == 256 (frame size).
//
// mWindowPos is reloaded per channel and stored once at the end: every channel advances the
// same number of samples, so the final position is common to all.
//
// The asm's chunking is reproduced exactly rather than re-rolled, because the two paths DIVERGE
// (see the file header):
//   take = min(mWindowSize - windowPos, samplesRemaining)   @0x82BA1004..0x82BA101C
//   n4   = take & ~3                                        @0x82BA1020 (clrrwi. r9,r9,2)
//   n4 != 0 -> n4/4 unrolled groups of four @0x82BA1044: four magnitudes, four mPeakCurrent
//              maxes, then ONE mPeakHold fold FROM mPeakCurrent @0x82BA10E8, then four
//              fmadds accumulations; windowPos += n4 up front @0x82BA1040.
//   n4 == 0 -> ONE sample @0x82BA1140: mPeakCurrent max, then the mPeakHold fold FROM THE
//              SAMPLE MAGNITUDE @0x82BA1160 (not from mPeakCurrent), one fmadds, ++windowPos.
// Both paths then share the window-boundary check @0x82BA1188. (The asm's second `r9 == 0`
// test @0x82BA1028 is dead -- clrrwi. already proved r9 non-zero -- so it is not modelled.)
// Because n4 <= the window remainder, no group ever straddles a window boundary.
// -------------------------------------------------------------------------------------
VuMeter *VuMeter::UpdateRunningPeakandRMS(VuMeter *self, AudioChannelBuffer *pSrc, int numSamples)
{
    const int numChannels = self->mBase.mbChannelCount;

    // r7: the window position, held full-register-width and truncated only by the final
    // `sth r7, 0x14A(r3)` @0x82BA11E8 (it is bounded by mWindowSize, so this never wraps).
    u32 luWindowPos = 0;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        luWindowPos = self->mWindowPos; // lhz 0x14A(r3) -- reloaded per channel

        const f32 *pCursor = pSrc->mpSamples + static_cast<u32>(pSrc->muStride) * ch; // r10
        const f32 *pEnd    = pCursor + numSamples;                                    // r31

        while (pCursor < pEnd) // cmplw r10, r31 @0x82BA11D0
        {
            const u32 luSamplesLeft = static_cast<u32>(pEnd - pCursor);            // srawi r8,r8,2
            const u32 luWindowLeft  = static_cast<u32>(self->mWindowSize) - luWindowPos; // subf r9
            const u32 luTake        = (luWindowLeft > luSamplesLeft) ? luSamplesLeft : luWindowLeft;
            const u32 luN4          = luTake & ~3u; // clrrwi. r9, r9, 2

            if (luN4 != 0)
            {
                // ---- unrolled-by-four group path @0x82BA1044 ----
                u32 luGroups = luN4 >> 2;  // ((r9 - 1) >> 2) + 1
                luWindowPos += luN4;       // addi r7, r8, r7 -- advanced BEFORE the group loop

                do
                {
                    const f32 lfS0 = pCursor[0];
                    const f32 lfS1 = pCursor[1];
                    const f32 lfS2 = pCursor[2];
                    const f32 lfS3 = pCursor[3];

                    const f32 lfM0 = (lfS0 <= KF_ZERO) ? (lfS0 * KF_NEG_ONE) : lfS0;
                    const f32 lfM1 = (lfS1 <= KF_ZERO) ? (lfS1 * KF_NEG_ONE) : lfS1;
                    const f32 lfM2 = (lfS2 <= KF_ZERO) ? (lfS2 * KF_NEG_ONE) : lfS2;
                    const f32 lfM3 = (lfS3 <= KF_ZERO) ? (lfS3 * KF_NEG_ONE) : lfS3;

                    // Four dependent max-stores into the running peak @0x82BA10A8..0x82BA10E4.
                    if (self->mPeakCurrent[ch] < lfM0)
                        self->mPeakCurrent[ch] = lfM0;
                    if (self->mPeakCurrent[ch] < lfM1)
                        self->mPeakCurrent[ch] = lfM1;
                    if (self->mPeakCurrent[ch] < lfM2)
                        self->mPeakCurrent[ch] = lfM2;
                    if (self->mPeakCurrent[ch] < lfM3)
                        self->mPeakCurrent[ch] = lfM3;

                    // ONE hold fold per group, taken from the RUNNING PEAK @0x82BA10E8.
                    if (self->mPeakHold[ch] < self->mPeakCurrent[ch])
                        self->mPeakHold[ch] = self->mPeakCurrent[ch];

                    // Four fmadds of the RAW (unsigned-magnitude-free) samples @0x82BA10FC.
                    self->mRmsAccum[ch] += lfS0 * lfS0;
                    self->mRmsAccum[ch] += lfS1 * lfS1;
                    self->mRmsAccum[ch] += lfS2 * lfS2;
                    self->mRmsAccum[ch] += lfS3 * lfS3;

                    pCursor += 4; // addi r10, r10, 0x10
                } while (--luGroups != 0);
            }
            else
            {
                // ---- scalar single-sample tail @0x82BA1140 ----
                const f32 lfSample = pCursor[0];
                const f32 lfMag    = (lfSample <= KF_ZERO) ? (lfSample * KF_NEG_ONE) : lfSample;

                if (self->mPeakCurrent[ch] < lfMag)
                    self->mPeakCurrent[ch] = lfMag;

                // NOT a typo and NOT interchangeable with the group path: this fold takes the
                // SAMPLE MAGNITUDE, not mPeakCurrent (lfs f13, 0x60(r11); fcmpu f13, f0;
                // stfs f0, 0x60(r11) @0x82BA1160..0x82BA116C, where f0 is still the magnitude).
                // After a deferred ResetFunc -- which zeroes mPeakHold but leaves mPeakCurrent
                // standing -- the two forms give different mPeakHold values, so folding
                // mPeakCurrent here would be a real behavioural divergence.
                if (self->mPeakHold[ch] < lfMag)
                    self->mPeakHold[ch] = lfMag;

                self->mRmsAccum[ch] += lfSample * lfSample;

                ++luWindowPos; // addi r7, r7, 1
                ++pCursor;     // addi r10, r10, 4
            }

            // ---- shared window-boundary check @0x82BA1188 ----
            if (luWindowPos >= self->mWindowSize)
            {
                self->mPeak[ch] = self->mPeakCurrent[ch]; // stfs 0x48(r11) == self+0x118
                luWindowPos     = 0;

                const f32 lfAccum      = self->mRmsAccum[ch];
                self->mRmsAccum[ch]    = KF_ZERO;
                self->mPeakCurrent[ch] = KF_ZERO;
                // fcfid/frsp: the window length converts through a 64-bit integer.
                self->mRms[ch] = std::sqrt(lfAccum / static_cast<f32>(self->mWindowSize));
            }
        }
    }

    self->mWindowPos = static_cast<u16>(luWindowPos); // sth r7, 0x14A(r3)
    return self;
}

// -------------------------------------------------------------------------------------
// UpdateAttributes @0x82B9D158 -- de-interleave the per-channel metering columns (RMS, last-
// window peak, hold peak) into speaker-position display slots by the active channel count:
//   mono  -> centre slot (index 1)
//   stereo-> L/R slots (indices 0, 2)
//   quad  -> indices 0, 2, 3, 4
//   5.1   -> direct 1:1 (indices 0..5)
// The hold display's slot 0 is always refreshed from hold channel 0 (the shared tail store)
// except for mono, which writes only its centre slot.
// -------------------------------------------------------------------------------------
void VuMeter::UpdateAttributes(VuMeter *self)
{
    const f32 hold0 = self->mPeakHold[0]; // loaded up front (f0)

    switch (self->mBase.mbChannelCount)
    {
    case 1: // mono -> centre position
        self->mHoldDisplay[1].mfLevel = hold0;
        self->mPeakDisplay[1].mfLevel = self->mPeak[0];
        self->mRmsDisplay[1].mfLevel  = self->mRms[0];
        return; // mono skips the shared slot-0 hold store

    case 2: // stereo -> L / R
        self->mHoldDisplay[2].mfLevel = self->mPeakHold[1];
        self->mPeakDisplay[0].mfLevel = self->mPeak[0];
        self->mPeakDisplay[2].mfLevel = self->mPeak[1];
        self->mRmsDisplay[0].mfLevel  = self->mRms[0];
        self->mRmsDisplay[2].mfLevel  = self->mRms[1];
        break;

    case 4: // quad -> positions 0, 2, 3, 4
        self->mHoldDisplay[2].mfLevel = self->mPeakHold[1];
        self->mHoldDisplay[3].mfLevel = self->mPeakHold[2];
        self->mHoldDisplay[4].mfLevel = self->mPeakHold[3];
        self->mPeakDisplay[0].mfLevel = self->mPeak[0];
        self->mPeakDisplay[2].mfLevel = self->mPeak[1];
        self->mPeakDisplay[3].mfLevel = self->mPeak[2];
        self->mPeakDisplay[4].mfLevel = self->mPeak[3];
        self->mRmsDisplay[0].mfLevel  = self->mRms[0];
        self->mRmsDisplay[2].mfLevel  = self->mRms[1];
        self->mRmsDisplay[3].mfLevel  = self->mRms[2];
        self->mRmsDisplay[4].mfLevel  = self->mRms[3];
        break;

    default: // 5.1 / 6-channel -> direct 1:1
        self->mHoldDisplay[1].mfLevel = self->mPeakHold[1];
        self->mHoldDisplay[2].mfLevel = self->mPeakHold[2];
        self->mHoldDisplay[3].mfLevel = self->mPeakHold[3];
        self->mHoldDisplay[4].mfLevel = self->mPeakHold[4];
        self->mHoldDisplay[5].mfLevel = self->mPeakHold[5];
        self->mPeakDisplay[0].mfLevel = self->mPeak[0];
        self->mPeakDisplay[1].mfLevel = self->mPeak[1];
        self->mPeakDisplay[2].mfLevel = self->mPeak[2];
        self->mPeakDisplay[3].mfLevel = self->mPeak[3];
        self->mPeakDisplay[4].mfLevel = self->mPeak[4];
        self->mPeakDisplay[5].mfLevel = self->mPeak[5];
        self->mRmsDisplay[0].mfLevel  = self->mRms[0];
        self->mRmsDisplay[1].mfLevel  = self->mRms[1];
        self->mRmsDisplay[2].mfLevel  = self->mRms[2];
        self->mRmsDisplay[3].mfLevel  = self->mRms[3];
        self->mRmsDisplay[4].mfLevel  = self->mRms[4];
        self->mRmsDisplay[5].mfLevel  = self->mRms[5];
        break;
    }

    self->mHoldDisplay[0].mfLevel = hold0; // shared slot-0 hold refresh (non-mono)
}

// -------------------------------------------------------------------------------------
// Process @0x82BA11F0 -- per-frame entry. Accumulate the frame's peaks / RMS from the source
// channel buffer, refresh the display slots, and flag that a frame was processed (so the decay
// timer keeps the meter live). Always returns 1.
// -------------------------------------------------------------------------------------
int VuMeter::Process(VuMeter *self, AudioProcessContext *ctx)
{
    UpdateAttributes(UpdateRunningPeakandRMS(self, ctx->mpSrcBuffer, KI_FRAME));
    self->mbUpdated = 1;
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
