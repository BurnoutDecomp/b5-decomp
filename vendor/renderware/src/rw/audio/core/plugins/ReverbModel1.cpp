// =====================================================================================
// rw::audio::core::ReverbModel1 bodies -- the "ReverbModel1" reverb plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 rwaudio PDB entry exist for this type. See plugins/ReverbModel1.h for the
// byte-exact layout and the per-function X360 addresses.
//
// The reverb chains the ReverbFilters.h building blocks over DelayLine ring buffers: six
// feedback comb sections plus up to three all-pass sections, reconfigured whenever the
// reverb-time / room-size / damping attributes or the sample rate change (RwacTimerClient
// -> ConfigureModel). ConfigureModel recomputes the section distances / delays / gains and
// re-sizes the delay lines; UpdateLatencyAndDecay derives the reverb latency the mixer must
// pre-roll from the largest comb/all-pass delay and the loop gains (Schroeder RT60 form).
//
// FOUR functions in this TU are left for a later pass and are only DECLARED here (bodies
// omitted, not stubbed):
//   * CalculateCombDelays  -- reads the undecoded rwaudio rodata delay-quantisation table
//                             flt_82170638[1652] (not in the dossier); its exact values are
//                             load-bearing, so the body is BLOCKED rather than guessed.
//   * CalculateG1Values    -- reads the undecoded rwaudio rodata coefficient tables
//                             flt_82170540[] / unk_82170564[] (not in the dossier); BLOCKED.
//   * Process              -- installs an unresolved all-pass reset function pointer (the
//                             disassembly's "STUB" symbol) into each all-pass section; the
//                             pointer identity is un-homed, so BLOCKED rather than fabricated.
//   * Dtor (~ReverbModel1) -- makes an unresolved call ("STUB") to tear down the embedded
//                             TimerHandle; the callee is un-homed, so BLOCKED.
// The bodied functions below reference the four only through their declarations, so this TU
// compiles under the per-TU gate.
// =====================================================================================

#include "rw/audio/core/plugins/ReverbModel1.h"
#include "rw/audio/core/PlugIn.h"        // rw::audio::core::System (mTimerManager @+0x60)
#include "rw/audio/core/TimerManager.h"  // rw::audio::core::TimerManager::AddTimer

#include <cmath>  // log10
#include <new>    // placement new (DelayLine sub-object construction)

namespace rw
{
namespace audio
{
namespace core
{

// The shared rwaudio System singleton (off_83271928). Its object is defined/owned by the
// System TU; ReleaseEvent removes the timer through it.
extern "C" System *off_83271928;

// off_8217F59C -- the ReverbModel1 v-table installed at construction. off_820AA810 -- the
// base PlugIn v-table the destructor reinstalls before any free. off_82F8FA14 -- the
// registered run-time descriptor record (its label is the string "ReverbModel1"). These are
// opaque data symbols in the XEX (no exported contents); modelled as honest placeholder
// storage so the bodies below link without fabricating their contents.
static void *const KRV_ReverbModel1VTable = nullptr; // off_8217F59C
static char       *KRV_ReverbModel1Desc = nullptr;   // off_82F8FA14 (the "ReverbModel1" record)

// The full mixer frame is processed per Process call; the reverb targets a 48 kHz internal
// rate (flt_820AA808) and its section maths use these fixed immediates.
static const f32 KF_ZERO = 0.0f;                 // flt_82001CC0
static const f32 KF_INTERNAL_RATE = 48000.0f;    // flt_820AA808
static const f32 KF_COMB_MIX = 0.16666667f;      // flt_8206C7DC (1/6 comb sum)
static const f32 KF_DECAY_FLOOR = 0.366f;        // flt_8217F380 (G2 decay-time floor)

// -------------------------------------------------------------------------------------
// GetSize @0x82B9ADA8 -- the plug-in instance footprint.
// -------------------------------------------------------------------------------------
int ReverbModel1::GetSize()
{
    return 1088; // li r3, 0x440
}

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9AD98 -- return the address of the registered descriptor
// record (its label is the string "ReverbModel1").
// -------------------------------------------------------------------------------------
char **ReverbModel1::GetPlugInDescRunTime()
{
    return &KRV_ReverbModel1Desc; // &off_82F8FA14
}

// -------------------------------------------------------------------------------------
// GetPpuTicksEvent @0x82B9E9D8 -- the CPU cycles the last timer callback took (the timer's
// recorded tick count). lwz r3, 0x14C(r3) == mTimer.mCpuTicks (mTimer @+0x13C, +0x10).
// -------------------------------------------------------------------------------------
u32 ReverbModel1::GetPpuTicksEvent(ReverbModel1 *self)
{
    return self->mTimer.mCpuTicks;
}

// -------------------------------------------------------------------------------------
// ReverbModel1::ReverbModel1 @0x82B9E938 -- install the v-table then default-construct every
// embedded sub-filter / delay-line / timer, in the asm's order: the 3 all-pass sections, the
// 3 all-pass delay lines, the timer, the 6 comb sections, the 6 comb delay lines.
// -------------------------------------------------------------------------------------
ReverbModel1 *ReverbModel1::ReverbModel1_ctor(ReverbModel1 *self)
{
    self->mBase.mpVTable = KRV_ReverbModel1VTable; // stw off_8217F59C @ +0x00

    for (int i = 0; i < 3; ++i)
        AllPassFilter::AllPassFilter_ctor(&self->mAllPass[i]);
    for (int i = 0; i < 3; ++i)
        new (&self->mAllPassDelay[i]) DelayLine();

    TimerHandle::TimerHandle_ctor(&self->mTimer);

    for (int i = 0; i < 6; ++i)
        CombFilter::CombFilter_ctor(&self->mComb[i]);
    for (int i = 0; i < 6; ++i)
        new (&self->mCombDelay[i]) DelayLine();

    return self;
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82B9EA48 -- run the destructor, then conditionally free.
//   Dtor(self); if (flags & 1) operator delete(self); return self;
// -------------------------------------------------------------------------------------
void *ReverbModel1::VectorDeletingDestructor(ReverbModel1 *self, char flags)
{
    Dtor(self);
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// -------------------------------------------------------------------------------------
// CalculateCombDistances @0x82B9AE30 -- map the room-size attribute (clamped to [2, 83.3])
// to the six comb reflection distances. The nearest wall is 0.8*room; the farthest is
// 1.5*that (capped at 100, which pins the room to its max and the near tap to 66.67), and
// the middle four taps are linearly spaced across the [near, far] span in 0.2 steps.
// -------------------------------------------------------------------------------------
int ReverbModel1::CalculateCombDistances(ReverbModel1 *self, f32 *pRoomSize,
                                         f32 *pCombDistances)
{
    (void)self; // `this` is passed (r3) but unused by the body

    if (*pRoomSize > 83.300003f)
        *pRoomSize = 83.300003f;
    else if (*pRoomSize < 2.0f)
        *pRoomSize = 2.0f;

    f32 nearDist = *pRoomSize * 0.80000001f;         // v5
    f32 farDist  = (*pRoomSize * 0.80000001f) * 1.5f; // v6
    if (farDist > 100.0f)
    {
        *pRoomSize = 83.333328f;
        farDist  = 100.0f;
        nearDist = 66.666664f;
    }

    pCombDistances[5] = farDist;
    pCombDistances[0] = nearDist;

    const f32 step = (farDist - nearDist) * 0.2f;    // v8
    const f32 d1 = step + nearDist;                  // v9
    pCombDistances[1] = d1;
    pCombDistances[2] = d1 + step;
    const f32 d3 = (d1 + step) + step;           // v10
    pCombDistances[3] = d3;
    pCombDistances[4] = d3 + step;
    return 1;
}

// -------------------------------------------------------------------------------------
// CalculateG2Values @0x82B9B1F8 -- the six comb damping gains. With the reverb time clamped
// to at least 0.366, each damping gain is (1 - combG1[i]) * (1 - 0.366/reverbTime).
// -------------------------------------------------------------------------------------
int ReverbModel1::CalculateG2Values(ReverbModel1 *self, f32 *pCombG2)
{
    if (self->mfDecayTime < KF_DECAY_FLOOR)
        self->mfDecayTime = KF_DECAY_FLOOR;

    const f32 damp = 1.0f - (KF_DECAY_FLOOR / self->mfDecayTime); // v5
    for (int i = 0; i < 6; ++i)
        pCombG2[i] = (1.0f - self->mfCombG1[i]) * damp;
    return 1;
}

// -------------------------------------------------------------------------------------
// ConfigureModel @0x82BA3A10 -- (re)build the reverb network. Depending on which attribute
// changed since the last configure, recompute the comb distances / delays / gains, re-tune
// and re-size every comb section, then (unless only a live-parameter tweak) rebuild the
// all-pass network from the reverb mode, and finally latch the applied state and refresh the
// latency. Returns 0 if any delay-line resize fails, else 1.
// -------------------------------------------------------------------------------------
int ReverbModel1::ConfigureModel(ReverbModel1 *self, System *pSystem)
{
    (void)pSystem; // passed (r4 = mBase.mpSystem) but unused by the body

    bool recomputedRoom = false; // v2 -- the room size changed (comb geometry rebuilt)
    bool recomputedRate = false; // v3 -- the sample rate changed (delays rebuilt)

    if (self->mfRoomSize != self->mfLastRoomSize)
    {
        CalculateCombDistances(self, &self->mfRoomSize, self->mfCombDistances);
        CalculateCombDelays(self, KF_INTERNAL_RATE, self->mfCombDistances, self->miCombDelays);
        CalculateG1Values(self, self->mfCombG1, KF_INTERNAL_RATE);
        CalculateG2Values(self, self->mfCombG2);
        recomputedRoom = true;
    }
    else if (self->mfLastSampleRate != KF_INTERNAL_RATE)
    {
        CalculateCombDelays(self, KF_INTERNAL_RATE, self->mfCombDistances, self->miCombDelays);
        CalculateG1Values(self, self->mfCombG1, KF_INTERNAL_RATE);
        CalculateG2Values(self, self->mfCombG2);
        recomputedRate = true;
    }
    else
    {
        if (self->mfDamping != self->mfLastDamping)
            CalculateG1Values(self, self->mfCombG1, KF_INTERNAL_RATE);
        CalculateG2Values(self, self->mfCombG2);
    }

    // Re-tune every comb section and, when its target delay changed, re-size + re-prime it.
    for (int i = 0; i < 6; ++i)
    {
        CombFilter::SetGains(&self->mComb[i], -self->mfCombG1[i], -self->mfCombG2[i],
                             -self->mfCombG1[i], KF_COMB_MIX);
        if (self->mCombDelay[i].miReadPosition != self->miCombDelays[i] + 1)
        {
            if (!self->mCombDelay[i].Resize(self->miCombDelays[i] + 3))
                return 0;
            self->mCombDelay[i].Reset(self->miCombDelays[i] + 1);
        }
    }

    if (self->mbAllPassConfigured && !recomputedRate)
    {
        // Network already sized: on a geometry change just re-prime the existing lines.
        if (recomputedRoom)
        {
            for (int i = 0; i < self->mbAllPassCount; ++i)
                self->mAllPassDelay[i].Reset(self->miAllPassDelaySamples[i]);
        }
    }
    else
    {
        // Size the all-pass network from the reverb mode (1 / 2|4 / other).
        f32 gain0; // v17 -- section-0 gain, shared as the base of the gain array
        const u8 mode = self->mBase.mbChannelCount;
        if (mode == 1)
        {
            self->mbAllPassCount = 1;
            self->miAllPassDelaySamples[0] = 288;
            gain0 = 0.7f;
        }
        else
        {
            u8  count;  // v18
            f32 gain1;  // v19 -- section-1 gain
            if (mode == 2 || mode == 4)
            {
                count = 2;
                gain0 = 0.63f;
                gain1 = 0.77777779f;
                self->miAllPassDelaySamples[1] = 259;
            }
            else
            {
                count = 3;
                gain0 = 0.63f;
                self->miAllPassDelaySamples[1] = 288;
                self->miAllPassDelaySamples[2] = 259;
                gain1 = 0.7f;
                self->mfAllPassGain[2] = 0.77777779f;
            }
            self->mfAllPassGain[1] = gain1;
            self->mbAllPassCount = count;
            self->miAllPassDelaySamples[0] = 320;
        }
        self->mfAllPassGain[0] = gain0;

        for (int i = 0; i < self->mbAllPassCount; ++i)
        {
            AllPassFilter::SetGains(&self->mAllPass[i], self->mfAllPassGain[i],
                                    self->mfAllPassMixGain);
            if (!self->mAllPassDelay[i].Resize(self->miAllPassDelaySamples[i] + 2))
                return 0;
            self->mAllPassDelay[i].Reset(self->miAllPassDelaySamples[i]);
        }
        self->mbAllPassConfigured = 1;
    }

    // Latch the applied attribute snapshot and refresh the reverb latency.
    self->mfLastDecayTime  = self->mfDecayTime;
    self->mfLastRoomSize   = self->mfRoomSize;
    self->mfLastDamping    = self->mfDamping;
    self->mfLastSampleRate = KF_INTERNAL_RATE;
    UpdateLatencyAndDecay(self);
    return 1;
}

// -------------------------------------------------------------------------------------
// UpdateLatencyAndDecay @0x82B9F6A8 -- recompute the reverb's reported latency (the number
// of samples the mixer must pre-roll) from the longest comb / all-pass delay and the largest
// loop gain, via the Schroeder RT60 form  d - d*10/log10(g), and fold the delta into the
// upstream voice's latency accumulator (voice +0x28).
// -------------------------------------------------------------------------------------
void ReverbModel1::UpdateLatencyAndDecay(ReverbModel1 *self)
{
    // Comb contribution: the largest comb delay (miCombDelays is ascending) and comb-0 gain.
    const f32 combDelay = static_cast<f32>(self->miCombDelays[5]);
    const f32 combLatency = combDelay - static_cast<f32>(
        (combDelay * 10.0f) / static_cast<f32>(log10(self->mfCombG2[0])));

    // All-pass contribution: the largest section gain and delay length.
    f32 maxGain = 0.0f;
    for (int i = 0; i < self->mbAllPassCount; ++i)
        if (self->mfAllPassGain[i] > maxGain)
            maxGain = self->mfAllPassGain[i];

    s32 maxApDelay = 0;
    for (int i = 0; i < self->mbAllPassCount; ++i)
        if (self->miAllPassDelaySamples[i] > maxApDelay)
            maxApDelay = self->miAllPassDelaySamples[i];

    const f32 apDelay = static_cast<f32>(maxApDelay);
    const f32 newLatency = (apDelay - static_cast<f32>(
        (apDelay * 10.0f) / static_cast<f32>(log10(maxGain)))) + combLatency;

    // Fold the change in reported latency into the upstream voice's accumulator (+0x28), then
    // latch it. (Raw +0x28 access into the voice mirrors the LowPassIir2::CreateInstance
    // idiom -- mBase.mpInput is the upstream input handle, its +0x28 the latency word.)
    f32 *pVoiceLatency = reinterpret_cast<f32 *>(
        reinterpret_cast<char *>(self->mBase.mpInput) + 0x28);
    *pVoiceLatency += (newLatency - self->mBase.mfAttrib1);
    self->mBase.mfAttrib1 = newLatency;
}

// -------------------------------------------------------------------------------------
// RwacTimerClient @0x82BA5720 -- the per-frame timer callback. While the reverb is active it
// reconfigures the network whenever a cached attribute (reverb time / room / damping) or the
// sample rate diverges from the applied snapshot; on the frame the reverb time drops to zero
// it flushes every comb / all-pass delay line and clears the comb loop state (a clean tail-
// out). (Registered via a function-pointer cast; its return value is discarded by the
// scheduler, so ConfigureModel's status is forwarded only when it runs.)
// -------------------------------------------------------------------------------------
int ReverbModel1::RwacTimerClient(ReverbModel1 *self)
{
    if (self->mfDecayTime > 0.0f)
    {
        if (self->mfDecayTime != self->mfLastDecayTime
            || self->mfRoomSize != self->mfLastRoomSize
            || self->mfDamping != self->mfLastDamping
            || self->mfLastSampleRate != KF_INTERNAL_RATE)
        {
            return ConfigureModel(self, reinterpret_cast<System *>(self->mBase.mpSystem));
        }
    }
    else if (self->mfLastDecayTime > 0.0f)
    {
        for (int i = 0; i < 6; ++i)
        {
            self->mCombDelay[i].Reset(self->miCombDelays[i] + 1);
            CombFilter::CombFilterResetFunc(&self->mComb[i]);
        }
        for (int i = 0; i < self->mbAllPassCount; ++i)
            self->mAllPassDelay[i].Reset(self->miAllPassDelaySamples[i]);
    }
    return 1;
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82B9ADB0 -- tear the reverb down: release every comb / all-pass delay line
// back to the allocator, and (if it was registered) remove the reconfigure timer through the
// shared System singleton. (The X360 return is the last cleanup call's register value, which
// callers discard.)
// -------------------------------------------------------------------------------------
int ReverbModel1::ReleaseEvent(ReverbModel1 *self)
{
    for (int i = 0; i < 6; ++i)
        self->mCombDelay[i].Release();

    for (int i = 0; i < self->mbAllPassCount; ++i)
        self->mAllPassDelay[i].Release();

    if (self->mbTimerAdded)
        System::RemoveTimer(off_83271928, &self->mTimer);

    return 0;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA67F8 -- placement-init a ReverbModel1 over `self`.
//
// Initialize<ReverbModel1>(self, 0x28) constructs the plug-in and bases its attribute table
// at self+0x28 (mfDecayTime). The templated PlugIn::Initialize helper lives in another TU;
// its locally-observable effect (construct + attribute base) is reproduced inline here, as
// the Gain / Iir2* shapes do. FLAGGED: the real Initialize<T> body (and the base
// mpSystem/mpVoice wiring it performs from the voice/factory) lives in the PlugIn TU.
//
// Then the reverb sets its own defaults, sizes each comb / all-pass delay line from the
// section's read-length (miMode), computes the shared all-pass mix gain, and registers the
// per-frame reconfigure timer. Returns 0 if the timer registration fails, else 1.
// -------------------------------------------------------------------------------------
int ReverbModel1::CreateInstance(ReverbModel1 *self)
{
    ReverbModel1_ctor(self);
    self->mBase.mpAttributes = &self->mfDecayTime; // attribute table base @ self+0x28

    self->mbTimerAdded = 0;

    // The all-pass section count follows the reverb mode (1 / 2|4 / other).
    const u8 mode = self->mBase.mbChannelCount;
    u8 allPassCount;
    if (mode == 1)
        allPassCount = 1;
    else if (mode == 2 || mode == 4)
        allPassCount = 2;
    else
        allPassCount = 3;
    self->mbAllPassCount = allPassCount;

    // Attribute / cache defaults.
    self->mfDecayTime      = KF_ZERO;   // reverb off until an attribute drives it
    self->mfLastDecayTime  = KF_ZERO;
    self->mfLastSampleRate = KF_ZERO;
    self->mfRoomSize       = 15.0f;     // flt_820047C4
    self->mfDamping        = 1.0f;      // flt_82001C98
    self->mfLastDamping    = 1.0f;

    // Size each delay line from its section's read-length descriptor (miMode).
    for (int i = 0; i < 6; ++i)
        self->mCombDelay[i].Init(1, 0, self->mComb[i].miMode);
    for (int i = 0; i < allPassCount; ++i)
        self->mAllPassDelay[i].Init(1, 0, self->mAllPass[i].miMode);

    // Shared all-pass mix gain = 2 / (mode>4 ? mode-1 : mode).
    const s32 modeS = self->mBase.mbChannelCount;
    const f32 denom = (modeS > 4) ? static_cast<f32>(modeS - 1) : static_cast<f32>(modeS);
    self->mbAllPassConfigured = 0;
    self->mfAllPassMixGain = 2.0f / denom; // flt_82001D9C == 2.0

    // Register the per-frame reconfigure timer on the System's TimerManager (collection 1).
    System *pSystem = reinterpret_cast<System *>(self->mBase.mpSystem);
    if (TimerManager::AddTimer(
            &pSystem->mTimerManager, &self->mTimer,
            reinterpret_cast<TimerManager::TimerCallback>(&ReverbModel1::RwacTimerClient),
            self, "ReverbModel1", 1, 1))
    {
        return 0;
    }

    self->mbTimerAdded = 1;
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
