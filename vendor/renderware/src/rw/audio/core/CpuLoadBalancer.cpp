// =====================================================================================
// rw::audio::core::CpuLoadBalancer bodies -- the audio-core CPU-load governor.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every offset, store width and branch. The Xbox 360 audio PDB
// identifies the nested timing objects and the member APIs.
//   Init                    @0x82B6C6F0
//   Reset                   @0x82B67638
//   Balance                 @0x82B6EF00
//   FindLowestPriorityVoice @0x82B67658
// =====================================================================================

// ARTIST built the governor for size, like the other rw::audio::core members: CullVoices tests
// FindLowestPriorityVoice's result with the record-form cr0 compare (0x82B6E450 `cmplwi r3,0`).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
#include "rw/audio/core/CpuLoadBalancer.h"
#include "rw/audio/core/PlugIn.h" // rw::audio::core::System (voice array + CPU-load fields)
#include "rw/audio/core/Voice.h"  // rw::audio::core::Voice / VoiceActiveNode
#include <cfloat>

namespace rw
{
namespace audio
{
namespace core
{

// Shared platform cycle counter at 0x82B64558.
u32 GetCpuCycle();

// Numeric rodata immediates from the balancer's asm.
static const f32 KF_MAX_LOAD            = 100.0f;         // flt_8214AED0 -- load ceiling / priority floor
static const f32 KF_VOICE_COST_SCALE    = 64.0f;         // flt_8214AED8 -- per-voice cost multiplier
static const f32 KF_BUDGET_SCALE        = 170666.67f;    // flt_8214B280 -- load% -> cycle-budget scale
static const f32 KF_CPU_CLOCK_RATE      = 3200000000.0f; // flt_820AA80C -- 3.2 GHz (Init seed)
static const f32 KF_FLT_MAX             = FLT_MAX; // ARTIST flt_8204F664: 0x7F7FFFFF

// -------------------------------------------------------------------------------------
// Init @0x82B6C6F0 -- bind the balancer to its owning System, clear the frame accumulator
// and moving-average state, and publish the CPU clock rate into the System.
// -------------------------------------------------------------------------------------
void CpuLoadBalancer::Init(System *system)
{
    mpSystem = system;
    Reset();
    mpSystem->mfCpuClockRate = KF_CPU_CLOCK_RATE;
}

// -------------------------------------------------------------------------------------
// Reset @0x82B67638 -- clear the frame accumulator + moving-average state (mpSystem intact).
// -------------------------------------------------------------------------------------
void CpuLoadBalancer::Reset()
{
    mOverheadCounter.Reset();
    mOverheadAverager.OverRide(0.0f);
}

// -------------------------------------------------------------------------------------
// Balance @0x82B6EF00 -- the per-frame governor step.
//
// Fold the CPU-cycle span since the previous stamp into the frame accumulator, roll it into
// a three-sample moving average (overwriting one of the two history slots each call, chosen
// by the alternating index), then -- while the System still reports CPU headroom -- project
// the total active-voice cost against the CPU budget and cull if it overruns. Finally fold
// this routine's own overhead back into the accumulator and publish the balance duration.
// -------------------------------------------------------------------------------------
void CpuLoadBalancer::Balance()
{
    u32 luStartCycle = GetCpuCycle();

    // Close the frame's useful span: (now + accumulator) - last stamp.
    u32 luNow = GetCpuCycle();
    u32 luFrameCycles = luNow + mOverheadCounter.mCpuCycles - mOverheadCounter.mCpuCycleStart;
    mOverheadCounter.mCpuCycles = luFrameCycles;

    // Roll into the moving average, then overwrite the current history slot with this frame.
    mOverheadAverager.Average(static_cast<f32>(luFrameCycles));
    mOverheadCounter.Reset();

    // Re-anchor the span for the culling work below.
    u32 luStamp = GetCpuCycle();
    System *system = mpSystem;
    mOverheadCounter.mCpuCycleStart = luStamp;

    if (system->mfCpuLoadPercent < KF_MAX_LOAD)
    {
        f32 lfBudget = system->mfCpuLoadPercent * KF_BUDGET_SCALE;
        f32 lfProjected = mOverheadAverager.mAverage * KF_VOICE_COST_SCALE;

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

        if (lfProjected - lfBudget > 0.0f)
            CullVoices(lfProjected - lfBudget);
    }

    // Fold the culling overhead back in, then publish this frame's total balance duration.
    u32 luEndSpan = GetCpuCycle();
    mOverheadCounter.mCpuCycles = luEndSpan + mOverheadCounter.mCpuCycles - mOverheadCounter.mCpuCycleStart;

    u32 luFinal = GetCpuCycle();
    mpSystem->muBalanceCycles = luFinal - luStartCycle;
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
Voice *CpuLoadBalancer::FindLowestPriorityVoice()
{
    System *system = mpSystem;
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
// CPU budget. Recovered from the raw ARTIST image, 0x82B6E410..0x82B6E490:
// f1 carries the excess cost, each victim subtracts its average cost * 64,
// and ExpelImmediate receives reason 2. Balance @0x82B6EFFC supplies f1.
// -------------------------------------------------------------------------------------
void CpuLoadBalancer::CullVoices(f32 afExcessCycles)
{
    while (afExcessCycles > 0.0f)
    {
        Voice* lpVoice = FindLowestPriorityVoice();
        if (!lpVoice)
            break;
        afExcessCycles -= lpVoice->mfAverageCpuTicks * KF_VOICE_COST_SCALE;
        Voice::ExpelImmediate(lpVoice, 2);
    }
}

} // namespace core
} // namespace audio
} // namespace rw
