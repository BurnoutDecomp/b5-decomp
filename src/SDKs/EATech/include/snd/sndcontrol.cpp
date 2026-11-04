#include "SDKs/EATech/include/snd/sndcontrol.h"

#include "SDKs/EATech/include/snd/sndhal.h"
#include "SDKs/EATech/include/snd/sndmix.h"
#include "SDKs/EATech/include/snd/sndsys.h"

namespace
{
    using Snd9::Hal::Channel;
    using Snd9::Hal::gpChannels;

    // ARTIST 0x82F87EA8. Negative fractions index the same table from its end.
    const u8 kauDetuneFraction[256] =
    {
        0x00, 0x01, 0x02, 0x02, 0x03, 0x04, 0x04, 0x05, 0x06, 0x07, 0x07, 0x08, 0x09, 0x09, 0x0A, 0x0B,
        0x0C, 0x0C, 0x0D, 0x0E, 0x0E, 0x0F, 0x10, 0x11, 0x11, 0x12, 0x13, 0x14, 0x14, 0x15, 0x16, 0x17,
        0x17, 0x18, 0x19, 0x1A, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1E, 0x1F, 0x20, 0x21, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x25, 0x26, 0x27, 0x28, 0x29, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2D, 0x2E, 0x2F, 0x30,
        0x31, 0x32, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3D,
        0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4B,
        0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x56, 0x57, 0x58, 0x59, 0x5A,
        0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
        0x6B, 0x6C, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77, 0x78, 0x79, 0x7A,
        0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A,
        0x8B, 0x8C, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x9A, 0x9B, 0x9C,
        0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAB, 0xAC, 0xAD, 0xAE,
        0xAF, 0xB0, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBF, 0xC0, 0xC1,
        0xC2, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xCA, 0xCB, 0xCC, 0xCD, 0xCF, 0xD0, 0xD1, 0xD2, 0xD4, 0xD5,
        0xD6, 0xD8, 0xD9, 0xDA, 0xDB, 0xDD, 0xDE, 0xDF, 0xE1, 0xE2, 0xE3, 0xE5, 0xE6, 0xE7, 0xE8, 0xEA,
        0xEB, 0xEC, 0xEE, 0xEF, 0xF0, 0xF2, 0xF3, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB, 0xFD, 0xFE, 0xFF
    };

    // ARTIST 0x82B731A0.
    s32 DetuneToLinear(s32 aiCents)
    {
        u32 luScale = 4096;
        while (aiCents >= 1200)
        {
            aiCents -= 1200;
            luScale <<= 1;
        }
        while (aiCents <= -1200)
        {
            aiCents += 1200;
            luScale >>= 1;
        }

        s32 liIndex = (13981 * aiCents) >> 16;
        if (liIndex < -255)
            liIndex = -255;
        if (liIndex < 0)
            return static_cast<s32>((256 + kauDetuneFraction[256 + liIndex]) * luScale) >> 9;
        return static_cast<s32>((256 + kauDetuneFraction[liIndex]) * luScale) >> 8;
    }
}

// ARTIST 0x82B73240.
extern "C" s32 iSNDcalcpitch(s32 aiVoice)
{
    Channel& lrChannel = gpChannels[aiVoice];
    s32 liResult = aiVoice;
    if (lrChannel.muCachedPitch == 0)
    {
        s32 liCents = lrChannel.miBasePitchCents;
        if (lrChannel.mpcPitchPattern)
        {
            const s32 liPattern = lrChannel.mpcPitchPattern[lrChannel.mucPitchPatternIndex] - 64;
            liCents += (liPattern * lrChannel.miPitchPatternDepth) >> 6;
        }
        liResult = DetuneToLinear(liCents);
        lrChannel.muCachedPitch = static_cast<u16>(liResult);
    }

    const u32 luProduct = static_cast<u32>(lrChannel.muPitchMultiplier) * lrChannel.muCachedPitch;
    lrChannel.muComputedPitch = static_cast<u16>(static_cast<s32>(luProduct) >> 12);
    return liResult;
}

// ARTIST 0x82B7D410.
extern "C" s32 SNDPLATFORM_setpitch(s32 aiVoice)
{
    const Channel& lrChannel = gpChannels[aiVoice];
    const u32 luRelativeRate = (static_cast<u32>(lrChannel.muSampleRate) << 16) / 48000;
    const u32 luPitch = (luRelativeRate * lrChannel.muComputedPitch) >> 12;
    for (s32 i = 0; i < lrChannel.mucSampleVoiceCount; ++i)
        MIX_setpitch(lrChannel.maiSampleVoices[i], luPitch);
    return 0;
}

// ARTIST 0x82B73CE8.
extern "C" s32 iSNDcalcvol(s32 aiVoice)
{
    Channel& lrChannel = gpChannels[aiVoice];
    const f32 lfEnvelope = static_cast<f32>(lrChannel.miEnvelopeCurrent >> 16);
    const f32 lfPacketVolume = static_cast<f32>(static_cast<s8>(lrChannel.mucPacketFlags));
    const f32 lfBase = ((lfEnvelope * lfPacketVolume) * lrChannel.mfFadeCurrent) *
                       0.00006200012285262346f; // 0x82156414
    lrChannel.mfVolume = lfBase;
    if (lrChannel.mpcVolumePattern)
    {
        const f32 lfPattern = static_cast<f32>(lrChannel.mpcVolumePattern[lrChannel.mucVolumePatternIndex]);
        lrChannel.mfVolume = (lfPattern * lfBase) * 0.007874015718698502f; // 0x82156410
    }
    if (lrChannel.mpcVolumeRemap)
    {
        const f32 lfScaled = lrChannel.mfVolume * 127.0f;
        const f32 lfRounded = lfScaled < 0.0f ? lfScaled - 0.5f : lfScaled + 0.5f;
        lrChannel.mfVolume = static_cast<f32>(lrChannel.mpcVolumeRemap[static_cast<s32>(lfRounded)]);
    }
    return aiVoice;
}

// ARTIST 0x82B73EF8.
extern "C" s32 iSNDpatchkey(s32 aiVoice, s32* apCursor)
{
    const u8 lucGroup = gpChannels[aiVoice].mucVoiceGroup;
    if (lucGroup)
    {
        while (++*apCursor < Snd9::giVoiceCount)
        {
            const Channel& lrChannel = gpChannels[*apCursor];
            if (lrChannel.mucVoiceGroup == lucGroup && lrChannel.mucVoiceState == 1 &&
                lrChannel.miVoiceHandle >= 0)
                return 1;
        }
    }
    else if (*apCursor < 0)
    {
        *apCursor = aiVoice;
        return 1;
    }
    return 0;
}

// ARTIST 0x82B7D518. Stop every mixer leg before releasing any voice record.
extern "C" s32 SNDPLATFORM_stop(s32 aiVoice)
{
    const Channel& lrChannel = gpChannels[aiVoice];
    for (s32 i = 0; i < lrChannel.mucSampleVoiceCount; ++i)
        MIX_stop(lrChannel.maiSampleVoices[i]);
    for (s32 i = 0; i < lrChannel.mucSampleVoiceCount; ++i)
        SNDVOICEI_free(lrChannel.maiSampleVoices[i]);
    return 0;
}

// ARTIST 0x82B7E3B0.
extern "C" s32 SNDstop(s32 aiVoiceHandle)
{
    const s32 liVoice = SNDVOICEI_get(aiVoiceHandle);
    if (liVoice >= 0)
    {
        s32 liCursor = -1;
        while (iSNDpatchkey(liVoice, &liCursor))
            SNDPLATFORM_stop(liCursor);
    }
    return liVoice;
}
