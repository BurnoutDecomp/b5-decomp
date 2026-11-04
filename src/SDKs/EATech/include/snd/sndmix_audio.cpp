// ARTIST 82B7F300: split the caller buffers into 512-sample mixer slices.
#include "SDKs/EATech/include/snd/sndmix.h"
#include <cstring>

extern "C" void MIX_audioslice(f32** appChannels, s32 aiSampleCount);

extern "C" void MIX_audio(f32* const* appChannels, s32 aiSampleCount)
{
    f32* lapChannels[6];
    const u8 lucCount = Snd9::Mix::gCreateParams.mucOutputCount;
    if (lucCount)
        std::memcpy(lapChannels, appChannels, sizeof(f32*) * lucCount);
    for (; aiSampleCount > 0; aiSampleCount -= 512)
    {
        const s32 liCount = aiSampleCount > 512 ? 512 : aiSampleCount;
        MIX_audioslice(lapChannels, liCount);
        for (s32 i = 0; i < Snd9::Mix::gCreateParams.mucOutputCount; ++i)
            lapChannels[i] += 512;
    }
}
#include "SDKs/EATech/include/snd/sndfilter.h"
#include "SDKs/EATech/include/snd/sndhal.h"
#include "rw/audio/core/PlugIn.h"

using namespace Snd9::Mix;
using Snd9::Hal::MixChannel;
using rw::audio::core::System;
extern "C" System* off_83271928;

namespace Snd9 { namespace Mix {
// ARTIST native globals: six output tails, post-mix callback, and FX state word.
f32 gafOutputTail[6];                       // 0x83277800
void (*gpfnPostMix)(s32);                   // 0x83277728
s32 giFxMixState;                           // 0x8327760C
} }

// ARTIST 82B7B510: decay the previous final sample into sixteen outputs.
extern "C" void MIXI_interpolateto0(f32* apTail,f32* apDest)
{
    static const f32 kWeight[16] = {
        0.9411764740943909f, 0.8823529481887817f, 0.8235294222831726f, 0.7647058963775635f,
        0.7058823704719543f, 0.6470588445663452f, 0.5882353186607361f, 0.5294117927551270f,
        0.4705882370471954f, 0.4117647111415863f, 0.3529411852359772f, 0.2941176593303680f,
        0.2352941185235977f, 0.1764705926179886f, 0.1176470592617989f, 0.0588235296308994f
    };
    for (s32 i=0;i<16;++i) apDest[i]=*apTail*kWeight[i]+apDest[i];
    *apTail=0.0f;
}
// ARTIST 82B7B6C0: 16-sample transition between previous and new gains.
extern "C" void MIXI_interpolatemix(f32 afOld,f32 afNew,const f32* apSource,f32* apDest)
{
    const f32 lfStep=(afNew-afOld)*0.0588235296308994f;
    for (s32 i=0;i<16;++i)
        apDest[i]=(lfStep*static_cast<f32>(i+1)+afOld)*apSource[i]+apDest[i];
}
// ARTIST 82B7B8B0: ramp the initial 16 samples and retain the new gains.
extern "C" s32 SNDMIXI_volramp(MixChannel* apVoice)
{
    FilterState* lpFilter=apVoice->mpFilter;
    const s32 liCount=lpFilter->mpProcess(lpFilter,16,gapPingPongBuffer[0],gapPingPongBuffer[1],0);
    if (liCount<=0)
    {
        for (s32 i=0;i<Snd9::Hal::giNumOutputs;++i)
            apVoice->mpPreviousOutputGains[i]=apVoice->mpOutputBuffer[i];
        for (s32 i=0;i<gCreateParams.mucOutputCount;++i)
            apVoice->mafPreviousSpeakerGain[i]=apVoice->mafSpeakerGain[i];
        return 0;
    }
    apVoice->mfLastSample=gapPingPongBuffer[1][15];
    for (s32 i=0;i<Snd9::Hal::giNumOutputs;++i)
    {
        if (!gpiFxMixEnabled[i]) continue;
        const f32 lfOld=apVoice->mpPreviousOutputGains[i], lfNew=apVoice->mpOutputBuffer[i];
        if (lfOld!=lfNew) {
            MIXI_interpolatemix(lfOld,lfNew,gapPingPongBuffer[1],gppFxMixBuffer[i]);
            apVoice->mpPreviousOutputGains[i]=apVoice->mpOutputBuffer[i];
        } else if (lfNew!=0.0f) gpfnMixSamples(16,lfNew,gapPingPongBuffer[1],gppFxMixBuffer[i]);
    }
    for (s32 i=0;i<gCreateParams.mucOutputCount;++i)
    {
        const f32 lfOld=apVoice->mafPreviousSpeakerGain[i], lfNew=apVoice->mafSpeakerGain[i];
        if (lfOld!=lfNew) {
            MIXI_interpolatemix(lfOld,lfNew,gapPingPongBuffer[1],gapOutputBuffer[i]);
            apVoice->mafPreviousSpeakerGain[i]=apVoice->mafSpeakerGain[i];
        } else if (lfNew!=0.0f) gpfnMixSamples(16,lfNew,gapPingPongBuffer[1],gapOutputBuffer[i]);
    }
    return 16;
}
// ARTIST 82B7BC58: preserve tail samples and release the actual filter chain.
extern "C" void MIX_stop(s32 aiVoice)
{
    MixChannel& lrVoice=Snd9::Hal::gpMixChannels[aiVoice];
    for (s32 i=0;i<Snd9::Hal::giNumOutputs;++i)
        gpfFxMixTail[i]=lrVoice.mpPreviousOutputGains[i]*lrVoice.mfLastSample+gpfFxMixTail[i];
    for (s32 i=0;i<gCreateParams.mucOutputCount;++i)
        gafOutputTail[i]=lrVoice.mafPreviousSpeakerGain[i]*lrVoice.mfLastSample+gafOutputTail[i];
    do {
        FilterState* lpFilter=lrVoice.mpFilter;
        if (lpFilter->mpRestore) lpFilter->mpRestore(lpFilter);
        lpFilter=lrVoice.mpFilter;
        FilterState* lpNext=lpFilter->mpNext;
        System::Free(off_83271928,lpFilter,0);
        lrVoice.mpFilter=lpNext;
    } while (lrVoice.mpFilter);
    lrVoice.mucState=0;
}
extern "C" void MIXI_stop(s32 aiVoice)
{
    MIX_stop(aiVoice);
    gCreateParams.mpVoiceFree(aiVoice);
}
// ARTIST 82B7EFC8: clear, ramp, filter, mix and scale one native slice.
extern "C" void MIX_audioslice(f32** appChannels,s32 aiSampleCount)
{
    for (s32 i=0;i<Snd9::Hal::giNumOutputs;++i)
        if (gpiFxMixEnabled[i] && gpfFxMixTail[i]!=0.0f)
            MIXI_interpolateto0(&gpfFxMixTail[i],gppFxMixBuffer[i]);
    for (s32 i=0;i<gCreateParams.mucOutputCount;++i) {
        std::memset(gapOutputBuffer[i],0,4*aiSampleCount);
        if (gafOutputTail[i]!=0.0f) MIXI_interpolateto0(&gafOutputTail[i],gapOutputBuffer[i]);
    }
    for (s32 i=0;i<gCreateParams.mucVoiceCount;++i) {
        MixChannel& lrVoice=Snd9::Hal::gpMixChannels[i];
        if (lrVoice.mucState!=2) continue;
        s32 liOffset=0;
        if (lrVoice.mucDirty) { liOffset=SNDMIXI_volramp(&lrVoice); lrVoice.mucDirty=0; }
        const s32 liRemaining=aiSampleCount-liOffset;
        if (!liRemaining) continue;
        FilterState* lpFilter=lrVoice.mpFilter;
        const s32 liProduced=lpFilter->mpProcess(lpFilter,liRemaining,gapPingPongBuffer[0],gapPingPongBuffer[1],0);
        if (liProduced<0) { MIXI_stop(i); continue; }
        if (!liProduced) continue;
        lrVoice.mfLastSample=gapPingPongBuffer[1][liProduced-1];
        for (s32 j=0;j<gCreateParams.mucOutputCount;++j)
            if (lrVoice.mafSpeakerGain[j]!=0.0f)
                gpfnMixSamples(liProduced,lrVoice.mafSpeakerGain[j],gapPingPongBuffer[1],gapOutputBuffer[j]+liOffset);
        for (s32 j=0;j<Snd9::Hal::giNumOutputs;++j)
            if (gpiFxMixEnabled[j] && lrVoice.mpOutputBuffer[j]!=0.0f) {
                gpfnMixSamples(liProduced,lrVoice.mpOutputBuffer[j],gapPingPongBuffer[1],gppFxMixBuffer[j]+liOffset);
                giFxMixState=0;
            }
    }
    if (gpfnPostMix) gpfnPostMix(aiSampleCount);
    for (s32 i=0;i<gCreateParams.mucOutputCount;++i)
        std::memcpy(appChannels[i],gapOutputBuffer[i],4*aiSampleCount);
    for (s32 i=0;i<gCreateParams.mucOutputCount;++i)
        for (s32 j=0;j<aiSampleCount;++j) appChannels[i][j]*=0.000030499999411404133f;
}
