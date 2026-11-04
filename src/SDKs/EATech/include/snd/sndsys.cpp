#include "SDKs/EATech/include/snd/sndsys.h"

#include "SDKs/EATech/include/snd/sndhal.h"
#include "SDKs/EATech/include/snd/sndmix.h"
#include "rw/audio/core/PlugIn.h"
#include <cstddef>
#include <cstring>

using rw::audio::core::System;
using namespace Snd9;

extern "C" System* off_83271928;

namespace Snd9
{
    SystemOptions gOptions;
    s16 giVoiceCount;
    u8 gucInitialized;
    f32 gfTimeScale;
}

static OutputOptions gSavedOutputOptions; // ARTIST 0x83277854
static s32 giOptionsStatus;               // ARTIST 0x83277CB4
static s32 giOptionsInitialized;          // ARTIST 0x83277CB8

CGS_STATIC_ASSERT((sizeof(SystemOptions) == 0x1C), "ARTIST Snd9 options");
CGS_STATIC_ASSERT((offsetof(SystemOptions, mOutput) == 4), "ARTIST output options");
CGS_STATIC_ASSERT((offsetof(SystemOptions, mauUnknown14) == 0x14), "ARTIST trailing options");
CGS_STATIC_ASSERT((sizeof(OutputOptions) == 0x10), "ARTIST output options size");
CGS_STATIC_ASSERT((offsetof(OutputOptions, mucVoiceCount) == 6), "ARTIST requested voice count");
CGS_STATIC_ASSERT((offsetof(OutputOptions, mucPacketCount) == 0xA), "ARTIST packet handle limit");

// ARTIST 0x82B74780.
extern "C" s32 SNDSYS_getopts(SystemOptions* apOptions)
{
    if (!giOptionsInitialized)
    {
        gOptions.mucMaxVoiceCount = 0xFF;
        giOptionsStatus = 0;
        gOptions.mOutput.muBankCount = 16;
        gOptions.mOutput.mucPacketCount = 16;
        gOptions.mOutput.mucUnknown07 = 1;
        gOptions.mOutput.mucVoiceCount = 32;
        gOptions.mOutput.mucOutputMode = 25;
        gOptions.mOutput.mucDirectOutputCount = 5;
        gOptions.mOutput.mucAuxOutputCount = 1;
        giOptionsInitialized = 1;
        gSavedOutputOptions = gOptions.mOutput;
    }
    *apOptions = gOptions;
    return giOptionsStatus;
}

// ARTIST 0x82B7D2E0.
extern "C" s32 SNDPLATFORM_outputset()
{
    const u8 lucMode = gOptions.mOutput.mucOutputMode;
    Mix::gucOutputMode = lucMode;
    if (gucInitialized)
    {
        gOptions.mOutput = gSavedOutputOptions;
        gOptions.mOutput.mucOutputMode = lucMode;
    }
    return 0;
}

// ARTIST 0x82B7E410.
extern "C" s32 SNDSYS_setopts(const SystemOptions* apOptions)
{
    gOptions.mOutput = apOptions->mOutput;
    gOptions.mauUnknown14[0] = apOptions->mauUnknown14[0];
    gOptions.mauUnknown14[1] = apOptions->mauUnknown14[1];
    if (gOptions.mOutput.mucPacketCount > 32)
        gOptions.mOutput.mucPacketCount = 32;
    if (gOptions.mOutput.mucVoiceCount > gOptions.mucMaxVoiceCount)
        gOptions.mOutput.mucVoiceCount = gOptions.mucMaxVoiceCount;
    giVoiceCount = gOptions.mOutput.mucVoiceCount;

    SNDPLATFORM_outputset();
    gfTimeScale = 10.0f; // ARTIST 0x82004A20
    gSavedOutputOptions = gOptions.mOutput;
    return 0;
}

// ARTIST 0x82B74848. The caller holds the audio system lock.
extern "C" s32 SNDSYSI_chanpubinit()
{
    const u32 luBytes = (sizeof(Hal::Channel) + sizeof(f32) * Hal::giNumOutputs) * giVoiceCount;
    Hal::gpChannels = static_cast<Hal::Channel*>(
        System::Alloc(off_83271928, luBytes, "Common Voice State", 16, 0));
    std::memset(Hal::gpChannels, 0, luBytes);

    f32* lpGains = reinterpret_cast<f32*>(Hal::gpChannels + giVoiceCount);
    for (s32 i = 0; i < giVoiceCount; ++i)
    {
        Hal::gpChannels[i].mpSourceGains = lpGains;
        for (s32 j = 0; j < Hal::giNumOutputs; ++j)
            lpGains[j] = 1.0f; // ARTIST 0x82001C98
        lpGains += Hal::giNumOutputs;
    }
    return 0;
}

// ARTIST 0x82B72FA8.
extern "C" s32 SNDVOICEI_get(s32 aiVoiceHandle)
{
    if (aiVoiceHandle < 0)
        return -8;

    const s32 liVoice = static_cast<u8>(aiVoiceHandle);
    if (liVoice >= giVoiceCount)
        return -8;

    const Hal::Channel& lrChannel = Hal::gpChannels[liVoice];
    return lrChannel.mucVoiceState && lrChannel.miVoiceHandle == aiVoiceHandle ? liVoice : -8;
}

// ARTIST 0x82B733E0.
extern "C" s32 SNDDRV_getsamplechan(s32 aiVoice)
{
    const s32 liParent = Hal::gpChannels[aiVoice].miParentVoice;
    if (liParent != -1)
    {
        const Hal::Channel& lrPrimary = Hal::gpChannels[liParent];
        for (s32 i = 1; i < 6; ++i)
        {
            if (lrPrimary.maiSampleVoices[i] == aiVoice)
                return i;
        }
    }
    return 0;
}
