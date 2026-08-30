// =====================================================================================
// rw::audio::core::Mixer -- member-function bodies.
//
// EARenderWare "rwaudio" mix executive. Reconstructed from BURNOUT_X360_ARTIST.XEX;
// the PowerPC asm is authoritative for every store. No Feb-2007 leak source and no
// DecFIGS DWARF exist for this type; member names are the ProStreet08Milestone.pdb
// rw::audio::core::Mixer names (layout proof: rw/audio/core/Mixer.h header note).
//   Mixer::Mixer (placement ctor)         @0x82B6D880
//   Mixer::Execute                        @0x82B6D900
//   Mixer::ProcessInputPlugIns            @0x82B6A048 (REAL -- register decode:
//                                          mixer_voicepath_decode_codex.md section 1)
//   Mixer::HandleBufferStatusUnavailable  @0x82B69F78 (REAL -- section 2)
// =====================================================================================

#include "rw/audio/core/Mixer.h"
#include "rw/audio/core/PlugIn.h" // System (the full layout) + PlugIn::mCpuTicks
#include "rw/audio/core/Voice.h"  // Voice / VoiceActiveNode / VoiceStageData

#include <cstdint> // uintptr_t (the console's clrrwi alignment at host width)
#include <cstring> // memcpy / memset (the console XMemCpy / XMemSet)

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
// Mixer::ProcessInputPlugIns @0x82B6A048 -- run a voice's source/input stages: chunked
// pull (pre-process backwards from the source stage cascading the requested count,
// process forwards from stage 0), each good chunk appended into the aux region until a
// full 256-sample frame is assembled, then published into the dst slot and the src/dst
// pair ping-ponged. Register-level decode:
// progress/scratch_dossiers/mixer_voicepath_decode_codex.md section 1 (the terminate/
// publish tail re-verified from the raw XEX words in-session: EVERY termination path
// forces produced=256 and falls into the unconditional publish block).
// The incoming node (r5) is decode-attested UNUSED. The caller guarantees a valid
// source-stage index (Execute rejects the 0xFF sentinel first).
// -------------------------------------------------------------------------------------
int Mixer::ProcessInputPlugIns(Mixer *self, VoiceStageData *apStageData,
                               VoiceActiveNode * /*apNode -- r5, unused (attested)*/,
                               Voice *apVoice)
{
    // The pre-process callback shape from the copied VoiceStageData::mpPreProcess slot:
    // (plugin, mixer, alreadyProcessedThisFrame, requestedCount) -> the count handed to
    // the next lower stage. Cast at the call site (bctrl @0x82B6A124).
    typedef int (*VoiceStagePreProcessFn)(PlugIn *, Mixer *, int, int);

    int liProduced = 0;             // r22 -- samples assembled into the aux region
    int liStatus = 0;               // r20 -- the running/returned stage status
    u8 lu8LastGoodChannels = 0;     // r21 -- channel count of the last appended chunk
    f32 lfSavedSampleRate = 0.0f;   // f31 -- the remembered +0x30024
    bool lbFirstChunk = true;       // first-outer-chunk gate (the mCpuTicks clear)

    while (liProduced < KU_FRAME_SIZE)   // the 0x82B6A350 backedge
    {
        int liCount = KU_FRAME_SIZE - liProduced;   // remaining
        self->mfResampleGain = 1.0f;                // stfs -> +0x30028 every iteration

        // --- pre-process: source stage DOWN through stage 0; the returned count
        // cascades (capped at 256, not lower-clamped) ---
        const int liSourceStage = apVoice->mcSourceStageIndex;   // lbz +0x46
        int liEnteringStageZero = liCount;   // r24 -- the argument of the LAST call
        for (int liStage = liSourceStage; liStage >= 0; --liStage)
        {
            PlugIn *lpPlugIn = apVoice->mpPlugIns[liStage];
            liEnteringStageZero = liCount;   // r24 updated immediately before the call
            u32 luStartCycle = GetCpuCycle();
            VoiceStagePreProcessFn lpfnPreProcess = reinterpret_cast<VoiceStagePreProcessFn>(
                apStageData[liStage].mpPreProcess);
            liCount = lpfnPreProcess(lpPlugIn, self,
                                     liStage > apVoice->mucFlag45 ? 1 : 0, liCount);
            if (liCount > KU_FRAME_SIZE)
                liCount = KU_FRAME_SIZE;
            if (lbFirstChunk)
                lpPlugIn->mCpuTicks = 0;     // cleared once, first chunk only
            lpPlugIn->mCpuTicks += GetCpuCycle() - luStartCycle;
        }

        // --- process: stage 0 UP through the source stage ---
        for (int liStage = 0; liStage <= liSourceStage; ++liStage)
        {
            u32 luStartCycle = GetCpuCycle();
            PlugIn *lpPlugIn = apVoice->mpPlugIns[liStage];
            VoiceStageProcessFn lpfnProcess = reinterpret_cast<VoiceStageProcessFn>(
                apStageData[liStage].mpProcess);
            liStatus = lpfnProcess(lpPlugIn, self,
                                   liStage > apVoice->mucFlag45 ? 1 : 0);
            if (liStatus)
            {
                if (liStage == 0)
                    apVoice->mfParam2C = 0.0f;   // stage-0 SUCCESS clears the decay accumulator
            }
            else
            {
                // THIS site passes the count that entered stage zero (r24) -- unlike
                // Execute's later-stage site, which passes the literal 256.
                liStatus = HandleBufferStatusUnavailable(self, apVoice, lpPlugIn,
                                                         liEnteringStageZero);
                if (!liStatus)
                {
                    lpPlugIn->mCpuTicks += GetCpuCycle() - luStartCycle;
                    break;   // hard decline -- leave the stage loop
                }
            }
            lpPlugIn->mCpuTicks += GetCpuCycle() - luStartCycle;
        }

        if (liStatus == 1)
        {
            // --- append the chunk: src slot channel starts -> aux at [produced] ---
            u8 lu8Channels = self->mbChannelCount;   // lbz +0x3002C
            int liChunkSamples = self->mNumSamples;  // lwz +0x30020
            if (liChunkSamples != 0)
            {
                SampleBuffer *lpSrc = self->mpSrcBuffer;
                SampleBuffer *lpAccum = self->mpAuxBuffer;
                for (u8 lu8Ch = 0; lu8Ch < lu8Channels; ++lu8Ch)
                {
                    std::memcpy(lpAccum->mpSamples + lpAccum->muStride * lu8Ch + liProduced,
                                lpSrc->mpSamples + lpSrc->muStride * lu8Ch,
                                sizeof(f32) * liChunkSamples);
                }
                lu8LastGoodChannels = lu8Channels;
                lfSavedSampleRate = self->mfSampleRate;   // remember +0x30024
                // Advance the frame clock by chunk/outputRate (the f32 division the
                // asm performs -- fcfid/frsp then fdivs against params +0x0C).
                self->mdStreamTime = self->mdStreamTime
                    + static_cast<f32>(liChunkSamples) / self->mpFormat->mfSampleRate;
                liProduced += liChunkSamples;
            }
            // (A status-1 chunk with mNumSamples == 0 does not advance -- the console
            // relies on the callback contract; reproduced.)
        }
        else
        {
            // --- terminate: pad the partial frame (if any chunk landed), else bail ---
            if (liProduced != 0)
            {
                SampleBuffer *lpAccum = self->mpAuxBuffer;
                if (lu8LastGoodChannels != 0)
                {
                    for (u8 lu8Ch = 0; lu8Ch < lu8LastGoodChannels; ++lu8Ch)
                    {
                        std::memset(lpAccum->mpSamples + lpAccum->muStride * lu8Ch + liProduced,
                                    0, sizeof(f32) * (KU_FRAME_SIZE - liProduced));
                    }
                }
                self->mfSampleRate = lfSavedSampleRate;      // restore +0x30024
                liStatus = 1;                                // coerce (li r20, 1)
                self->mbChannelCount = lu8LastGoodChannels;  // stbx -> +0x3002C
            }
            liProduced = KU_FRAME_SIZE;   // li r22, 0x100 -- force completion (both arms)
        }
        lbFirstChunk = false;
    }

    // --- the unconditional publish block (0x82B6A358): aux -> the dst slot per the
    // CURRENT channel count (a zero count skips the copy only), the full-frame count
    // published, the src/dst pair ping-ponged, the (possibly coerced) status returned.
    {
        u8 lu8Channels = self->mbChannelCount;
        SampleBuffer *lpAccum = self->mpAuxBuffer;
        SampleBuffer *lpDst = self->mpDstBuffer;
        for (u8 lu8Ch = 0; lu8Ch < lu8Channels; ++lu8Ch)
        {
            std::memcpy(lpDst->mpSamples + lpDst->muStride * lu8Ch,
                        lpAccum->mpSamples + lpAccum->muStride * lu8Ch,
                        sizeof(f32) * KU_FRAME_SIZE);   // li r5, 0x400
        }
        self->mNumSamples = KU_FRAME_SIZE;   // stwx 0x100 -> +0x30020
        SampleBuffer *lpOldSrc = self->mpSrcBuffer;
        self->mpSrcBuffer = self->mpDstBuffer;
        self->mpDstBuffer = lpOldSrc;
    }
    return liStatus;
}

// -------------------------------------------------------------------------------------
// Mixer::HandleBufferStatusUnavailable @0x82B69F78 -- a stage reported no buffer: ring
// the voice's decay tail out as silence. Clamp the decay target up to at least the
// fade start, and while the accumulator has not reached it: advance the accumulator by
// the sample count, zero the src slot's channels (the PLUG-IN's output channel count)
// and report the silence chunk as available (1). Once the tail is rung out: clear
// mucFlag45 and decline for good (0). Register-level decode:
// mixer_voicepath_decode_codex.md section 2.
// -------------------------------------------------------------------------------------
int Mixer::HandleBufferStatusUnavailable(Mixer *self, Voice *apVoice,
                                         PlugIn *apPlugIn, int aiNumSamples)
{
    // fadeEnd = max(fadeEnd, fadeStart) -- the literal clamp.
    if (apVoice->mfFadeEnd < apVoice->mfFadeStart)
        apVoice->mfFadeEnd = apVoice->mfFadeStart;

    if (apVoice->mfParam2C >= apVoice->mfFadeEnd)
    {
        apVoice->mucFlag45 = 0;
        return 0;   // the tail is rung out -- decline
    }

    apVoice->mfParam2C += static_cast<f32>(aiNumSamples);   // fcfid/frsp then fadds

    // Zero the src slot's channels from sample 0 (the PLUG-IN's output channel count
    // bounds the loop; the mixer's own +0x3002C is NOT written here).
    SampleBuffer *lpSrc = self->mpSrcBuffer;
    for (u8 lu8Ch = 0; lu8Ch < apPlugIn->mOutputChannels; ++lu8Ch)
    {
        std::memset(lpSrc->mpSamples + lpSrc->muStride * lu8Ch, 0,
                    sizeof(f32) * aiNumSamples);
    }
    self->mNumSamples = aiNumSamples;   // stwx -> +0x30020
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
