// Native Snd9 startup and platform speaker-pan allocation.
// ARTIST 0x82B7D338, 0x82B7D3C0 and 0x82B85D18.
#include "types.hpp"
#include "SDKs/EATech/include/snd/sndsys.h"
#include "SDKs/EATech/include/snd/sndmix.h"
#include "SDKs/EATech/include/snd/sndhal.h"
#include "SDKs/EATech/include/snd/snd_panning_x360.h"
#include "rw/audio/core/PlugIn.h"
#include <cstring>
using rw::audio::core::System;
extern "C" System* off_83271928;
// Native worker callback, owned by the independent Snd9 voice lane.
extern "C" void SNDVOICEI_free(s32 channel);
namespace Snd9 {
    u32 guServiceTick;                 // 0x8327786C
    u32 gauRandomSeeds[6];             // 0x832776E8
    void* gpBankList;                  // 0x832778C4
    f32 gfSystemScale = 1.0f;          // 0x82F883CC
    u8 gucStartupFlag6A;               // 0x8327786A
    u8 gucServiceFlag65;               // 0x83277865
    u8 gucServiceFlag66;               // 0x83277866
    void (*gpfnRestoreB0)();            // 0x832778B0
    void (*gpfnRestoreB4)();            // 0x832778B4
    void (*gpfnRestoreB8)(s32);         // 0x832778B8
    void (*gpfnRestoreBC)();            // 0x832778BC
}
extern "C" s32 SNDPLATFORM_init()
{
    System::Lock(off_83271928);
    const u32 bytes=24u*static_cast<u32>(static_cast<u16>(Snd9::giVoiceCount));
    Snd9::Hal::gpChannelPan=static_cast<Snd9::Hal::SpeakerVol*>(
        System::Alloc(off_83271928,bytes,"Platform voices",16,0));
    std::memset(Snd9::Hal::gpChannelPan,0,bytes);
    System::Unlock(off_83271928);
    return 0;
}
extern "C" s32 SNDPLATFORM_restore()
{
    System::Lock(off_83271928);
    System::Free(off_83271928,Snd9::Hal::gpChannelPan,0);
    System::Unlock(off_83271928);
    return 0;
}
extern "C" s32 SNDSYSI_init()
{
    using namespace Snd9;
    if (gucInitialized) return 0;
    if (!giVoiceCount)
    {
        const s32 result=SNDSYS_getopts(&gOptions);
        if (result<0) return result;
        SNDSYS_setopts(&gOptions);
    }
    static const u32 kInitialSeeds[6]={0xf22d0e56u,0x883126e9u,0xc624dd2fu,
                                       0x0702c49cu,0x9e353f7du,0x6fdf3b64u};
    for (s32 i=0;i<6;++i) gauRandomSeeds[i]=kInitialSeeds[i]+gOptions.mOutput.muUnknown00;
    gucInitialized=1;
    System::Lock(off_83271928);
    SNDSYSI_chanpubinit();
    const u32 bankBytes=static_cast<u32>(gOptions.mOutput.muBankCount)*8u;
    gpBankList=System::Alloc(off_83271928,bankBytes,"Bank List Array",16,0);
    std::memset(gpBankList,0,bankBytes);
    System::Unlock(off_83271928);
    guServiceTick=0; gfSystemScale=1.0f;
    gucStartupFlag6A=0; gucServiceFlag65=0; gucServiceFlag66=0;
    const s32 status=SNDPLATFORM_init();
    if (status<0) { SNDPLATFORM_restore(); gucInitialized=0; return status; }
    Mix::CreateParams params;
    params.miSampleRate=48000;
    params.mpVoiceFree=&SNDVOICEI_free;
    params.mucVoiceCount=gOptions.mOutput.mucVoiceCount;
    params.mucOutputCount=static_cast<u8>(gOptions.mOutput.mucDirectOutputCount+gOptions.mOutput.mucAuxOutputCount);
    params.mucFlags=0;
    MIX_create(&params);
    Hal::gucNumSpeakers=static_cast<u8>(gOptions.mOutput.mucDirectOutputCount+gOptions.mOutput.mucAuxOutputCount);
    SNDI_spkrconfig();
    SNDI_precalcaztospkrvol();
    return 0;
}

// ARTIST 0x82B7E4F8. The original does not clear freed pointers/callbacks.
extern "C" s32 SNDSYS_restore()
{
    using namespace Snd9;
    if (gucInitialized != 1) return -14;
    if (gpfnRestoreB0) gpfnRestoreB0();
    if (gpfnRestoreB4) gpfnRestoreB4();
    if (gpfnRestoreBC) gpfnRestoreBC();
    SNDI_freespkrtable();
    if (gpfnRestoreB8) gpfnRestoreB8(-1);
    MIX_destroy();
    SNDPLATFORM_restore();
    System::Lock(off_83271928);
    System::Free(off_83271928,Hal::gpChannels,0);
    System::Free(off_83271928,gpBankList,0);
    System::Unlock(off_83271928);
    gucInitialized=0;
    return 0;
}
