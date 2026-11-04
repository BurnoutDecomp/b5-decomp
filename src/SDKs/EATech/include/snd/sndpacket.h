#pragma once

#include "types.hpp"

namespace Snd9
{
namespace Packet
{
    typedef void (*PacketReleased)(u8* apSamples, void* apContext);
    typedef void (*FramesReleased)(s32 aiHandle, s32 aiFrames, void* apContext);

    struct Submission
    {
        u32 muUnknown00;
        u32 muSamplesAndFlags;
        u32 muUnknown08;
        u8* mapChannelSamples[6];
    };

    struct Buffer;

    s32 FindVoiceHandle(s32 aiVoice);
}
}

extern "C" s32 SNDPKTPLAY_create(Snd9::Packet::PacketReleased apPacketReleased,
                                 Snd9::Packet::FramesReleased apFramesReleased, void* apContext,
                                 Snd9::Packet::Buffer* apBuffer, u32 auBufferBytes);
extern "C" s32 SNDPKTPLAY_submit(s32 aiHandle, const Snd9::Packet::Submission* apSubmission);
extern "C" s32 SNDPKTPLAYI_voicetopackethandle(s32 aiVoice);
extern "C" u8* SNDPKTPLAYI_get(s32 aiHandle, s32 aiChannel, s32* apFrames, s32* apContinuation);
extern "C" void SNDPKTPLAYI_freeframes(s32 aiHandle, s32 aiChannel, s32 aiFrames);
extern "C" void SNDPKTPLAYI_flushcallbackdata();
