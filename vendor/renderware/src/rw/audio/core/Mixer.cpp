// =====================================================================================
// rw::audio::core::Mixer -- member-function bodies.
//
// EARenderWare "rwaudio" mix executive. Reconstructed from BURNOUT_X360_ARTIST.XEX;
// the PowerPC asm is authoritative for every store. No Feb-2007 leak source and no
// DecFIGS DWARF exist for this type; member names are the ProStreet08Milestone.pdb
// rw::audio::core::Mixer names (layout proof: rw/audio/core/Mixer.h header note).
//   Mixer::Mixer (placement ctor)         @0x82B6D880
//   Mixer::Execute                        @0x82B6D900
//   Mixer::ProcessInputPlugIns            @0x82B6A048 (FLAG honest stub below)
//   Mixer::HandleBufferStatusUnavailable  @0x82B69F78 (FLAG honest stub below)
// =====================================================================================

#include "rw/audio/core/Mixer.h"
#include "rw/audio/core/PlugIn.h" // System (the full layout) + PlugIn::mCpuTicks
#include "rw/audio/core/Voice.h"  // Voice / VoiceActiveNode / VoiceStageData

#include <cstdint> // uintptr_t (the console's clrrwi alignment at host width)

namespace rw
{
namespace audio
{
namespace core
{

// The System singleton pointer (X360 off_83271928; defined in System.cpp, published by
// System::CreateInstance). Mixer::Mixer reads it to link the StackAllocator.
extern "C" System *off_83271928;

// The PPC timebase read (System.cpp owns the one definition; this TU keeps a local
// declaration, the Profiler.cpp / CpuLoadBalancer.cpp convention -- link-merged).
u32 GetCpuCycle();

namespace
{
    // The three static master SampleBuffer descriptors (X360 unk_83271940, stride
    // 0x114). Execute re-seeds and re-publishes all three every audio frame;
    // descriptor [0] is the master output the Dac interleaves.
    SampleBuffer sSampleBuffers[Mixer::KU_NUM_REGIONS];

    // The static mixer scratch arena (X360 unk_83271D00..0x83277500 == 0x5800 bytes;
    // the .bss block the console's clrrwi bounds arithmetic carves). Mixer::Mixer
    // seeds the StackAllocator's limits over it.
    u8 sMixerScratchArena[0x5800];

    // The console's `clrrwi rN, rN, 7` -- round an arena address down to 128 -- on the
    // host's own pointer width (the same original bounds arithmetic, not offset access).
    u8 *AlignDown128(u8 *apAddress)
    {
        return reinterpret_cast<u8 *>(
            reinterpret_cast<std::uintptr_t>(apAddress) & ~static_cast<std::uintptr_t>(0x7F));
    }

    // The stage process callback shape Execute dispatches out of the copied
    // VoiceStageData::mpProcess slot: (plugin, mixer, alreadyProcessedThisFrame).
    // Cast at the call site -- the console's own generic-dispatch site (bctrl
    // @0x82B6DA9C).
    typedef int (*VoiceStageProcessFn)(PlugIn *, Mixer *, int);

    // flt_820065E0 -- the 3-sample moving-average fold (1/3).
    const f32 KF_ONE_THIRD = 0.33333334f;
}

// -------------------------------------------------------------------------------------
// Mixer::Mixer @0x82B6D880 -- placement ctor over the Dac-allocated 0x30080 block: zero
// the four header stat fields, then seed the static StackAllocator (the 4-slot record
// System::mpObjectTable points at) over the static scratch arena and link the System
// singleton into it. The sample regions and the remaining header fields are NOT
// ctor-touched (Dac::CreateInstance links mpSystem; Execute writes the rest) --
// decode-attested.
// -------------------------------------------------------------------------------------
Mixer *Mixer::Mixer_ctor(Mixer *self)
{
    self->muMixerCpuCycles = 0;   // stwx 0 -> +0x3001C
    self->mbChannelCount = 0;     // stbx 0 -> +0x3002C
    self->mNumSamples = 0;        // stwx 0 -> +0x30020
    self->mfSampleRate = 0.0f;    // stfsx flt_82001CC0 -> +0x30024

    // table = (*off_83271928)->mpObjectTable; table->mpSystem = the singleton. The
    // arena bounds are the console's own clrrwi(.,7) alignment arithmetic at host
    // width: upper/top = align-down of the arena END, lower = align-down of base+0x7F
    // (== align-UP of the unaligned base). (The asm's `cmplwi 0x80` guard on the end
    // address is constant-true on the console image and folds away.)
    System *lpSystem = off_83271928;
    StackAllocator *lpStack = static_cast<StackAllocator *>(lpSystem->mpObjectTable);
    lpStack->mpSystem = lpSystem;
    u8 *lpUpper = AlignDown128(sMixerScratchArena + sizeof(sMixerScratchArena));
    lpStack->mpUpperLimit = lpUpper;   // stw -> slot [1]
    lpStack->mpTop = lpUpper;          // stw -> slot [3] (the live top starts at upper)
    lpStack->mpLowerLimit = AlignDown128(sMixerScratchArena + 0x7F); // slot [2]
    return self;
}

// -------------------------------------------------------------------------------------
// Mixer::Execute @0x82B6D900 -- the per-audio-frame mix walk (called only by Dac::Mix
// @0x82B96E80). Register-level decode in the .ida-exports dossier; the shape:
//   1. stamp the frame-start cycle; publish the params (+0x30018);
//   2. re-seed the three SampleBuffer descriptors over the sample regions and
//      re-publish them at mpSampleBuffer[0..2];
//   3. per active voice (params->mpVoiceListNodes x muNumVoices, both re-read each
//      lap): reset mfTotalPitch to 1.0, run the source stages (ProcessInputPlugIns)
//      unless the voice has none (mcSourceStageIndex == -1 starts the walk at stage 0
//      with status 1), then each remaining stage's process callback; a stage that
//      declines falls back to HandleBufferStatusUnavailable, and a hard decline stops
//      the voice's walk. A full pass latches mucFlag45 = mucNumStages. Stage-0 success
//      clears the voice's decay accumulator (mfParam2C). Per-stage and per-voice CPU
//      cycles land in PlugIn::mCpuTicks and the voice's 3-sample stat block.
//   4. stamp muMixerCpuCycles; return the LAST status (0 with zero voices -- the
//      Dac then emits silence, the faithful idle frame).
// -------------------------------------------------------------------------------------
int Mixer::Execute(Mixer *self, MixerExecuteParams *apParams)
{
    u32 luFrameStartCycle = GetCpuCycle();
    self->mpFormat = apParams;   // stw r31 -> +0x30018 (the execute-params / format slot)

    // The descriptor re-seed loop (0x82B6D958..0x82B6D994): all three regions, every
    // frame -- system, region base, stride 256, capacity 64, the +0x0C word cleared --
    // and the src/dst/aux slots re-pointed in order (resetting the stage ping-pong).
    SampleBuffer **lppSlots[KU_NUM_REGIONS] =
        { &self->mpSrcBuffer, &self->mpDstBuffer, &self->mpAuxBuffer };
    for (u32 luRegion = 0; luRegion < KU_NUM_REGIONS; ++luRegion)
    {
        SampleBuffer *lpBuffer = &sSampleBuffers[luRegion];
        lpBuffer->mpSystem = self->mpSystem;         // lwz +0x30008 ; stw -> +0x00
        lpBuffer->mpSamples = self->mBuffer[luRegion]; // the region base -> +0x04
        lpBuffer->muUnk0C = 0;                        // sth 0 -> +0x0C
        lpBuffer->muStride = KU_FRAME_SIZE;           // sth 0x100 -> +0x0E
        lpBuffer->mu8ChannelCapacity = KU_CHANNEL_CAPACITY; // stb 0x40 -> +0x10
        *lppSlots[luRegion] = lpBuffer;               // stw -> +0x3000C + 4*region
    }

    int liStatus = 0;
    u32 luVoiceIndex = 0;
    // The count and the node table are RE-READ from the params each lap (lhz 0x10 /
    // lwz +8 inside the loop) -- reproduced.
    while (luVoiceIndex < apParams->muNumVoices)
    {
        VoiceActiveNode *lpNode = apParams->mpVoiceListNodes + luVoiceIndex;
        u32 luVoiceStartCycle = GetCpuCycle();
        Voice *lpVoice = lpNode->mpVoice;
        VoiceStageData *lpStageData = lpVoice->mpStageData; // lwz +0x18
        self->mfResampleGain = 1.0f;                        // stfs flt_82001C98 -> +0x30028

        int liStage;
        if (lpVoice->mcSourceStageIndex == -1)   // lbz +0x46 ; cmplwi 0xFF
        {
            liStage = 0;
            liStatus = 1;
        }
        else
        {
            self->mdStreamTime = apParams->mfSystemTime;     // lfd 0(params) -> +0x30000
            liStatus = ProcessInputPlugIns(self, lpStageData, lpNode, lpVoice);
            liStage = lpVoice->mcSourceStageIndex + 1;
        }
        self->mdStreamTime = apParams->mfSystemTime;         // the second refresh (both paths)

        if (liStatus == 1)
        {
            if (liStage >= lpVoice->mucNumStages)
            {
                lpVoice->mucFlag45 = lpVoice->mucNumStages;  // the LABEL_17 latch
            }
            else
            {
                do
                {
                    u32 luStageStartCycle = GetCpuCycle();
                    PlugIn *lpPlugIn = lpVoice->mpPlugIns[liStage]; // lwzx 4*(stage+0x13)
                    VoiceStageProcessFn lpfnProcess = reinterpret_cast<VoiceStageProcessFn>(
                        lpStageData[liStage].mpProcess);            // lwz 12*stage+4
                    liStatus = lpfnProcess(lpPlugIn, self,
                                           liStage > lpVoice->mucFlag45 ? 1 : 0);
                    if (liStatus)
                    {
                        if (liStage == 0)
                            lpVoice->mfParam2C = 0.0f;   // stage-0 success clears the decay accumulator
                    }
                    else
                    {
                        liStatus = HandleBufferStatusUnavailable(self, lpVoice, lpPlugIn,
                                                                 KU_FRAME_SIZE);
                        if (!liStatus)
                            break;   // hard decline -- stop this voice's walk
                    }
                    ++liStage;
                    lpPlugIn->mCpuTicks = GetCpuCycle() - luStageStartCycle; // stw -> +0x1C
                }
                while (liStage < lpVoice->mucNumStages);

                if (liStatus == 1)
                    lpVoice->mucFlag45 = lpVoice->mucNumStages;
            }
        }

        // Per-voice CPU stats: latch the raw cycles, fold the 3-sample moving average
        // (last two history samples + this frame, * 1/3), store this frame into the
        // toggled history slot, flip the toggle.
        u32 luVoiceCycles = GetCpuCycle() - luVoiceStartCycle;
        f32 lfSum = lpVoice->mafCpuTickHistory[1] + lpVoice->mafCpuTickHistory[0];
        f32 lfCycles = static_cast<f32>(luVoiceCycles);
        lpVoice->miLastFrameCpuTicks = static_cast<s32>(luVoiceCycles);  // stw -> +0x3C
        lpVoice->mfAverageCpuTicks = (lfSum + lfCycles) * KF_ONE_THIRD;  // stfs -> +0x00
        lpVoice->mafCpuTickHistory[lpVoice->muCpuHistoryIndex] = lfCycles; // stfsx 4*(idx+1)
        lpVoice->muCpuHistoryIndex = (lpVoice->muCpuHistoryIndex == 0) ? 1u : 0u; // cntlzw fold
        ++luVoiceIndex;
    }

    self->muMixerCpuCycles = GetCpuCycle() - luFrameStartCycle; // stwx -> +0x3001C
    return liStatus;
}

// -------------------------------------------------------------------------------------
// Mixer::ProcessInputPlugIns @0x82B6A048 -- run a voice's source/input stages (the
// decoder pull + stage-buffer binding through the StackAllocator).
// FLAG honest stub: the register-level decode is in flight
// (progress/scratch_dossiers/mixer_voicepath_decode_codex.md). UNREACHABLE today --
// Execute only calls it for a voice with a source stage, and no voice exists until the
// phase-E staging lights the first ones. DECLINES (status 0) so a premature voice
// mixes silence instead of an invented frame.
// -------------------------------------------------------------------------------------
int Mixer::ProcessInputPlugIns(Mixer * /*self*/, VoiceStageData * /*apStageData*/,
                               VoiceActiveNode * /*apNode*/, Voice * /*apVoice*/)
{
    return 0;
}

// -------------------------------------------------------------------------------------
// Mixer::HandleBufferStatusUnavailable @0x82B69F78 -- a stage reported no buffer: bind
// the fallback silence buffer for the downstream stages.
// FLAG honest stub: same in-flight decode, same unreachable-until-voices reasoning.
// DECLINES (0) -- Execute then ends that voice's walk for the frame.
// -------------------------------------------------------------------------------------
int Mixer::HandleBufferStatusUnavailable(Mixer * /*self*/, Voice * /*apVoice*/,
                                         PlugIn * /*apPlugIn*/, int /*aiNumSamples*/)
{
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
