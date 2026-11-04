#pragma once

#include "types.hpp"

namespace Snd9
{
    struct OutputOptions
    {
        u32 muUnknown00;
        u16 muBankCount;
        u8 mucVoiceCount;
        u8 mucUnknown07;
        u8 mucUnknown08;
        u8 mucOutputMode;
        u8 mucPacketCount;
        u8 mucDirectOutputCount;
        u8 mucAuxOutputCount;
        u8 mauUnknown0D[3];
    };

    struct SystemOptions
    {
        u8 mucMaxVoiceCount;
        u8 mauUnknown01[3];
        OutputOptions mOutput;
        u32 mauUnknown14[2];
    };

    extern SystemOptions gOptions;  // ARTIST 0x83277838
    extern s16 giVoiceCount;        // ARTIST 0x83277868
    extern u8 gucInitialized;       // ARTIST 0x83277864
    extern u32 guServiceTick;      // ARTIST 0x8327786C
    extern f32 gfTimeScale;         // ARTIST 0x83277644
}

extern "C" s32 SNDSYS_getopts(Snd9::SystemOptions* apOptions);
extern "C" s32 SNDSYS_setopts(const Snd9::SystemOptions* apOptions);
extern "C" s32 SNDSYSI_chanpubinit();
extern "C" s32 SNDVOICEI_get(s32 aiVoiceHandle);
extern "C" void SNDVOICEI_free(s32 aiVoice);
extern "C" s32 SNDDRV_getsamplechan(s32 aiVoice);
extern "C" void SNDSYS_service();
extern "C" void SNDSYSI_100hzserver();
extern "C" void SNDSYSI_variabletimerservice();
extern "C" s32 SNDSYS_restore();
