// =====================================================================================
// rw::audio::core::CpuLoadBalancer bodies -- the audio-core CPU-load governor.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every offset, store width and branch. No Feb-2007 source, no DecFIGS
// DWARF, and no ProStreet08 rwaudio PDB entry exist for these bodies. See CpuLoadBalancer.h
// for the byte layout and the per-function X360 addresses.
//   Init                    @0x82B6C6F0
//   Reset                   @0x82B67638
//   Balance                 @0x82B6EF00
//   FindLowestPriorityVoice @0x82B67658
// =====================================================================================

#include "rw/audio/core/CpuLoadBalancer.h"
#include "rw/audio/core/PlugIn.h" // rw::audio::core::System (voice array + CPU-load fields)
#include "rw/audio/core/Voice.h"  // rw::audio::core::Voice / VoiceActiveNode

namespace rw
{
namespace audio
{
namespace core
{

// Free-running CPU cycle counter (rw::audio::core::GetCpuCycle @todo). Its body lives in a
// platform timing TU that is not yet reconstructed; only the declaration is needed for the
// per-TU compile gate. Returns a monotonically increasing cycle count (r3). Profiler.cpp /
// TimerManager.cpp declare the same free function.
u32 GetCpuCycle();

// Numeric rodata immediates from the balancer's asm.
static const f32 KF_ONE_THIRD           = 0.33333334f;    // flt_820065E0 -- 1/3 averaging weight
static const f32 KF_MAX_LOAD            = 100.0f;         // flt_8214AED0 -- load ceiling / priority floor
static const f32 KF_VOICE_COST_SCALE    = 64.0f;         // flt_8214AED8 -- per-voice cost multiplier
static const f32 KF_BUDGET_SCALE        = 170666.67f;    // flt_8214B280 -- load% -> cycle-budget scale
static const f32 KF_ZERO                = 0.0f;          // flt_82001CC0
static const f32 KF_CPU_CLOCK_RATE      = 3200000000.0f; // flt_820AA80C -- 3.2 GHz (Init seed)
static const f32 KF_FLT_MAX             = 3.4028235e38f; // flt_8204F664 -- FindLowest best-seed

// -------------------------------------------------------------------------------------
// Init @0x82B6C6F0 -- bind the balancer to its owning System, clear the frame accumulator
// and moving-average state, and publish the CPU clock rate into the System.
// -------------------------------------------------------------------------------------
CpuLoadBalancer *CpuLoadBalancer::Init(CpuLoadBalancer *self, System *system)
{
    self->mpSystem = system;         // +0x00
    self->muFrameCycles = 0;         // +0x04
    self->mfAverageLoad = KF_ZERO;   // +0x10
    self->mafHistory[0] = KF_ZERO;   // +0x14
    self->mafHistory[1] = KF_ZERO;   // +0x18
    system->mfCpuClockRate = KF_CPU_CLOCK_RATE; // System +0x10C8
    return self;
}

// -------------------------------------------------------------------------------------
// Reset @0x82B67638 -- clear the frame accumulator + moving-average state (mpSystem intact).
// -------------------------------------------------------------------------------------
CpuLoadBalancer *CpuLoadBalancer::Reset(CpuLoadBalancer *self)
{
    self->muFrameCycles = 0;       // +0x04
    self->mfAverageLoad = KF_ZERO; // +0x10
    self->mafHistory[0] = KF_ZERO; // +0x14
    self->mafHistory[1] = KF_ZERO; // +0x18
    return self;
}

// -------------------------------------------------------------------------------------
// Balance @0x82B6EF00 -- the per-frame governor step.
//
// Fold the CPU-cycle span since the previous stamp into the frame accumulator, roll it into
// a three-sample moving average (overwriting one of the two history slots each call, chosen
// by the alternating index), then -- while the System still reports CPU headroom -- project
// the total active-voice cost against the CPU budget and cull if it overruns. Finally fold
// this routine's own overhead back into the accumulator and publish the balance duration.
// Returns the final CPU-cycle snapshot.
// -------------------------------------------------------------------------------------
u32 CpuLoadBalancer::Balance(CpuLoadBalancer *self)
{
    u32 luStartCycle = GetCpuCycle();

    // Close the frame's useful span: (now + accumulator) - last stamp.
    u32 luNow = GetCpuCycle();
    u32 luFrameCycles = luNow + self->muFrameCycles - self->muLastStamp;
    self->muFrameCycles = luFrameCycles;

    // Roll into the moving average, then overwrite the current history slot with this frame.
    f32 lfSample = static_cast<f32>(luFrameCycles);
    self->mfAverageLoad =
        (self->mafHistory[1] + self->mafHistory[0] + lfSample) * KF_ONE_THIRD;
    self->mafHistory[self->muHistoryIndex] = lfSample;
    self->muHistoryIndex = (self->muHistoryIndex == 0) ? 1 : 0;
    self->muFrameCycles = 0;

    // Re-anchor the span for the culling work below.
    u32 luStamp = GetCpuCycle();
    System *system = self->mpSystem;
    self->muLastStamp = luStamp;

    if (system->mfCpuLoadPercent < KF_MAX_LOAD)
    {
        f32 lfBudget = system->mfCpuLoadPercent * KF_BUDGET_SCALE;
        f32 lfProjected = self->mfAverageLoad * KF_VOICE_COST_SCALE;

        u32 luVoiceCount = system->muActiveVoiceCount;
        if (luVoiceCount != 0)
        {
            VoiceActiveNode *node = system->mppVoiceListNodes;
            do
            {
                --luVoiceCount;
                lfProjected = node->mpVoice->mfAverageCpuTicks * KF_VOICE_COST_SCALE + lfProjected;
                ++node;
            } while (luVoiceCount != 0);
        }

        if (lfProjected - lfBudget > KF_ZERO)
            CullVoices(self);
    }

    // Fold the culling overhead back in, then publish this frame's total balance duration.
    u32 luEndSpan = GetCpuCycle();
    self->muFrameCycles = luEndSpan + self->muFrameCycles - self->muLastStamp;

    u32 luFinal = GetCpuCycle();
    self->mpSystem->muBalanceCycles = luFinal - luStartCycle;
    return luFinal;
}

// -------------------------------------------------------------------------------------
// FindLowestPriorityVoice @0x82B67658 -- pick the culling victim.
//
// Walk the System's active-voice array, skipping already-expelled voices. Track the voice
// with the lowest mfPriority; a voice created this very frame (its create stamp == the
// current frame counter minus one) is treated as priority-class index 0, and ties on
// mfPriority are broken by the lower priority-class index. Returns the selected voice, or 0
// when none qualifies or the lowest priority is at/above the 100.0 floor.
// -------------------------------------------------------------------------------------
Voice *CpuLoadBalancer::FindLowestPriorityVoice(CpuLoadBalancer *self)
{
    System *system = self->mpSystem;
    u32 luCount = system->muActiveVoiceCount;
    if (luCount == 0)
        return 0;

    u32 luFreshStamp = system->muFrameCounter - 1;
    Voice *lpBest = 0;
    u32 luBestIndex = 0xFFFFFFFFu;
    f32 lfBestPriority = KF_FLT_MAX;

    VoiceActiveNode *node = system->mppVoiceListNodes;
    do
    {
        Voice *voice = node->mpVoice;
        if (voice->mucState != 2)
        {
            u32 luIndex = voice->muCreateStamp;
            if (luIndex == luFreshStamp)
                luIndex = 0;

            f32 lfPriority = voice->mfPriority;
            if (lfPriority < lfBestPriority)
            {
                lfBestPriority = lfPriority;
                lpBest = voice;
                luBestIndex = luIndex;
            }
            else if (lfPriority == lfBestPriority && luIndex < luBestIndex)
            {
                lpBest = voice;
                luBestIndex = luIndex;
            }
        }
        ++node;
    } while (--luCount != 0);

    if (lfBestPriority < KF_MAX_LOAD)
        return lpBest;
    return 0;
}

// -------------------------------------------------------------------------------------
// CullVoices @0x82B6E410 -- expel lowest-priority voices until the frame cost fits the
// CPU budget (the ledgered "separate TU"; called only from Balance's overrun gate).
// FLAG honest stub (phase-D Dac slice 2026-08-28, mounted for the Balance link): the
// @0x82B6E410 dossier is NOT exported yet (targeted re-export pending), and the gate
// is UNREACHABLE today -- zero active voices project zero cost, so Balance never
// overruns. Declines (0 voices culled); body it with the phase-E voice slices.
// -------------------------------------------------------------------------------------
u32 CpuLoadBalancer::CullVoices(CpuLoadBalancer * /*self*/)
{
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
