#include "SDKs/EATech/include/snd/sndservice.h"
#include "SDKs/EATech/include/snd/sndsys.h"
#include "SDKs/EATech/include/snd/sndhal.h"
#include <cstddef>

extern "C" s32 iSNDcalcpitch(s32 aiVoice);
extern "C" s32 SNDPLATFORM_setpitch(s32 aiVoice);
extern "C" s32 iSNDcalcvol(s32 aiVoice);
extern "C" s32 SNDstop(s32 aiVoiceHandle);

namespace Snd9
{
    VariableTimerNode* gpVariableTimerHead;
    ServiceCallback gapfn100HzService[6];
    ServiceCallback gapfnService[10];
}
CGS_STATIC_ASSERT((sizeof(Snd9::VariableTimerNode) == 16), "ARTIST variable timer node");
CGS_STATIC_ASSERT((offsetof(Snd9::VariableTimerNode, mpCallback) == 8), "ARTIST timer callback");
CGS_STATIC_ASSERT((offsetof(Snd9::VariableTimerNode, mpUser) == 12), "ARTIST timer user");

// ARTIST82B74610: cache next before calling; a callback may unlink its own node.
extern "C" void SNDSYSI_variabletimerservice()
{
    Snd9::VariableTimerNode* lpNode = Snd9::gpVariableTimerHead;
    while (lpNode)
    {
        Snd9::VariableTimerNode* lpNext = lpNode->mpNext;
        lpNode->mpCallback(lpNode->mpUser);
        lpNode = lpNext;
    }
}

// ARTIST82B74940: the signed count is re-read after each callback.
extern "C" void SNDSYS_service()
{
    if (Snd9::gucInitialized)
    {
        for (s32 i = 0; i < static_cast<s8>(Snd9::gucServiceFlag66); ++i)
            Snd9::gapfnService[i]();
    }
}

// ARTIST82B80098: advance the service callbacks and active voice modulation.
extern "C" void SNDSYSI_100hzserver()
{
    using namespace Snd9;
    ++guServiceTick;
    for (s32 i = 0; i < static_cast<s8>(gucServiceFlag65); ++i)
        gapfn100HzService[i]();

    for (s32 i = 0; i < giVoiceCount; ++i)
    {
        Hal::Channel& lrChannel = Hal::gpChannels[i];
        if (lrChannel.mucVoiceState != 1)
            continue;
        if (lrChannel.miVoiceHandle >= 0 && lrChannel.mpcPitchPattern)
        {
            lrChannel.mucPitchPatternIndex = static_cast<u8>(lrChannel.mucPitchPatternIndex + 1);
            if (lrChannel.mucPitchPatternIndex >= lrChannel.mucPitchPatternLength)
                lrChannel.mucPitchPatternIndex = 0;
            lrChannel.muCachedPitch = 0;
            iSNDcalcpitch(i);
            SNDPLATFORM_setpitch(i);
        }
        bool lbUpdateVolume = false;
        if (lrChannel.mpcVolumePattern)
        {
            lbUpdateVolume = true;
            lrChannel.mucVolumePatternIndex = static_cast<u8>(lrChannel.mucVolumePatternIndex + 1);
            if (lrChannel.mucVolumePatternIndex >= lrChannel.mucVolumePatternLength)
                lrChannel.mucVolumePatternIndex = 0;
        }
        if (lrChannel.mfFadeStep != 0.0f)
        {
            const f32 lfStep = lrChannel.mfFadeStep;
            lrChannel.mfFadeCurrent += lfStep;
            lbUpdateVolume = true;
            if (lfStep < 0.0f ? !(lrChannel.mfFadeCurrent > lrChannel.mfFadeTarget)
                              : !(lrChannel.mfFadeCurrent < lrChannel.mfFadeTarget))
            {
                lrChannel.mfFadeStep = 0.0f;
                lrChannel.mfFadeCurrent = lrChannel.mfFadeTarget;
            }
            if (lrChannel.mfFadeCurrent < 0.0f)
            {
                SNDstop(lrChannel.miVoiceHandle);
                continue;
            }
        }
        lrChannel.miEnvelopeRemaining = static_cast<s32>(static_cast<u32>(lrChannel.miEnvelopeRemaining) - 1u);
        if (lrChannel.miEnvelopeStep)
        {
            lbUpdateVolume = true;
            lrChannel.miEnvelopeCurrent = static_cast<s32>(static_cast<u32>(lrChannel.miEnvelopeCurrent) + static_cast<u32>(lrChannel.miEnvelopeStep));
        }
        if (!lrChannel.miEnvelopeRemaining)
        {
            lrChannel.mcEnvelopeIndex = static_cast<s8>(static_cast<u8>(lrChannel.mcEnvelopeIndex) + 1u);
            if (lrChannel.mcEnvelopeIndex >= lrChannel.mcEnvelopeCount)
            {
                SNDstop(lrChannel.miVoiceHandle);
                continue;
            }
            const Hal::EnvelopeSegment& lrSegment = lrChannel.mpEnvelope[lrChannel.mcEnvelopeIndex];
            lrChannel.miEnvelopeRemaining = lrSegment.miDuration < 0 ? 0x7FFFFFFF : lrSegment.miDuration;
            const s32 liDifference = static_cast<s32>((static_cast<u32>(lrSegment.miTarget) << 16) - static_cast<u32>(lrChannel.miEnvelopeCurrent));
            lrChannel.miEnvelopeStep = liDifference / lrChannel.miEnvelopeRemaining;
        }
        if (lbUpdateVolume)
            Hal::SetVol(iSNDcalcvol(i));
    }
}
