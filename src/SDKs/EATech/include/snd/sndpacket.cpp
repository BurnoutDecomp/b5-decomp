#include "SDKs/EATech/include/snd/sndpacket.h"

#include "SDKs/EATech/include/snd/sndhal.h"
#include "SDKs/EATech/include/snd/sndsys.h"
#include "rw/audio/core/PlugIn.h"
#include <cstddef>

using rw::audio::core::System;
using namespace Snd9;
using namespace Snd9::Packet;

extern "C" System* off_83271928;

namespace Snd9
{
namespace Packet
{
    struct Slot
    {
        u32 muOrdinal;
        volatile u32 muSamplesAndFlags;
        u8* mapChannelSamples[6];
    };

    struct Buffer
    {
        s32 miVoiceHandle;
        u32 muSubmitOrdinal;
        u32 muReleaseOrdinal;
        u32 muReadOrdinal;
        u32 mauChannelOrdinal[6];
        s16 maiReadSlot[6];
        s16 maiReadySlots[6];
        s16 miCapacity;
        s8 mcReleaseChannel;
        u8 mucPad43;
        s16 miReleaseSlot;
        s16 miWriteSlot;
        u32 muReadyFrames;
        u32 muInUseFrames;
        PacketReleased mpPacketReleased;
        FramesReleased mpFramesReleased;
        void* mpContext;
        u16 muSampleRate;
        u8 mucChannels;
        u8 mucFormat;
        void* mapChannelAllocations[6];

        Slot& GetSlot(s32 aiSlot)
        {
            return reinterpret_cast<Slot*>(this + 1)[aiSlot];
        }
    };

    struct Callback
    {
        u16 muPacketReleased;
        u16 muHandle;
        union
        {
            u8* mpSamples;
            s32 miFrames;
        };
    };

    static Buffer* gapBuffers[32];    // ARTIST 0x83277C14
    static Callback gaCallbacks[96];  // ARTIST 0x83277914
    static s32 giCallbackCount;       // ARTIST 0x83277910
}
}

CGS_STATIC_ASSERT((sizeof(Slot) == 0x20), "ARTIST packet ring slot");
CGS_STATIC_ASSERT((sizeof(Buffer) == 0x78), "ARTIST packet buffer header");
CGS_STATIC_ASSERT((sizeof(Callback) == 8), "ARTIST queued packet callback");
CGS_STATIC_ASSERT((offsetof(Buffer, maiReadSlot) == 0x28), "ARTIST read indices");
CGS_STATIC_ASSERT((offsetof(Buffer, maiReadySlots) == 0x34), "ARTIST ready packet counts");
CGS_STATIC_ASSERT((offsetof(Buffer, miCapacity) == 0x40), "ARTIST ring capacity");
CGS_STATIC_ASSERT((offsetof(Buffer, mcReleaseChannel) == 0x42), "ARTIST callback channel");
CGS_STATIC_ASSERT((offsetof(Buffer, mpPacketReleased) == 0x50), "ARTIST release callback");
CGS_STATIC_ASSERT((offsetof(Buffer, mucChannels) == 0x5E), "ARTIST packet channel count");
CGS_STATIC_ASSERT((offsetof(Submission, mapChannelSamples) == 0xC), "ARTIST submitted channel samples");

// ARTIST 0x82B7E290. The caller supplies the header and trailing ring slots.
extern "C" s32 SNDPKTPLAY_create(PacketReleased apPacketReleased, FramesReleased apFramesReleased,
                                 void* apContext, Buffer* apBuffer, u32 auBufferBytes)
{
    System::Lock(off_83271928);
    const s32 liLimit = gOptions.mOutput.mucPacketCount;
    for (s32 i = 0; i < liLimit; ++i)
    {
        if (gapBuffers[i])
            continue;

        apBuffer->mpPacketReleased = apPacketReleased;
        apBuffer->mpFramesReleased = apFramesReleased;
        apBuffer->mpContext = apContext;
        apBuffer->miCapacity = (auBufferBytes - sizeof(Buffer) - sizeof(Slot)) / sizeof(Slot);
        apBuffer->miVoiceHandle = -1;
        gapBuffers[i] = apBuffer;
        System::Unlock(off_83271928);
        return i;
    }
    System::Unlock(off_83271928);
    return -9;
}

// ARTIST 0x82B74068.
extern "C" s32 SNDPKTPLAY_submit(s32 aiHandle, const Submission* apSubmission)
{
    Buffer& lrBuffer = *gapBuffers[aiHandle];
    for (s32 i = 0; i < lrBuffer.mucChannels; ++i)
    {
        if (lrBuffer.maiReadySlots[i] >= lrBuffer.miCapacity - 1)
            return -13;
    }

    Slot& lrSlot = lrBuffer.GetSlot(lrBuffer.miWriteSlot);
    lrSlot.muSamplesAndFlags = (apSubmission->muSamplesAndFlags & ~1u) | (lrSlot.muSamplesAndFlags & 1u);
    lrSlot.muSamplesAndFlags = (lrSlot.muSamplesAndFlags & ~1u) | (apSubmission->muSamplesAndFlags & 1u);
    lrSlot.muOrdinal = lrBuffer.muSubmitOrdinal;
    for (s32 i = 0; i < lrBuffer.mucChannels; ++i)
    {
        lrSlot.mapChannelSamples[i] = apSubmission->mapChannelSamples[i];
        ++lrBuffer.maiReadySlots[i];
    }
    lrBuffer.muReadyFrames += apSubmission->muSamplesAndFlags >> 1;
    const u32 luOrdinal = lrBuffer.muSubmitOrdinal++;
    if (++lrBuffer.miWriteSlot >= lrBuffer.miCapacity)
        lrBuffer.miWriteSlot = 0;
    return luOrdinal;
}

// ARTIST 0x82B73F90.
extern "C" s32 SNDPKTPLAYI_voicetopackethandle(s32 aiVoice)
{
    const s32 liLimit = gOptions.mOutput.mucPacketCount;
    for (s32 i = 0; i < liLimit; ++i)
    {
        if (gapBuffers[i] && SNDVOICEI_get(gapBuffers[i]->miVoiceHandle) == aiVoice)
            return i;
    }
    return -1;
}

s32 Snd9::Packet::FindVoiceHandle(s32 aiVoice)
{
    const s32 liParent = Hal::gpChannels[aiVoice].miParentVoice;
    return SNDPKTPLAYI_voicetopackethandle(liParent == -1 ? aiVoice : liParent);
}

// ARTIST 0x82B74190. Packet release and frame accounting use separate ring cursors.
extern "C" u8* SNDPKTPLAYI_get(s32 aiHandle, s32 aiChannel, s32* apFrames, s32* apContinuation)
{
    Buffer& lrBuffer = *gapBuffers[aiHandle];
    bool lbRelease = true;
    for (s32 i = 0; i < lrBuffer.mucChannels; ++i)
    {
        if (lrBuffer.mauChannelOrdinal[i] <= lrBuffer.muReleaseOrdinal)
            lbRelease = false;
    }
    if (lbRelease)
    {
        if (lrBuffer.mpPacketReleased)
        {
            Callback& lrCallback = gaCallbacks[giCallbackCount];
            lrCallback.muPacketReleased = 1;
            lrCallback.muHandle = aiHandle;
            lrCallback.mpSamples = lrBuffer.GetSlot(lrBuffer.miReleaseSlot).mapChannelSamples[0];
            ++giCallbackCount;
        }
        ++lrBuffer.miReleaseSlot;
        ++lrBuffer.muReleaseOrdinal;
        if (lrBuffer.miReleaseSlot >= lrBuffer.miCapacity)
            lrBuffer.miReleaseSlot = 0;
    }

    if (!lrBuffer.maiReadySlots[aiChannel])
        return 0;

    Slot& lrSlot = lrBuffer.GetSlot(lrBuffer.maiReadSlot[aiChannel]);
    *apFrames = static_cast<s32>(lrSlot.muSamplesAndFlags) >> 1;
    *apContinuation = -static_cast<s32>(lrSlot.muSamplesAndFlags & 1);
    ++lrBuffer.maiReadSlot[aiChannel];
    ++lrBuffer.mauChannelOrdinal[aiChannel];
    bool lbRead = true;
    s32 i = 0;
    do
    {
        lbRead = lrBuffer.mauChannelOrdinal[i] > lrBuffer.muReadOrdinal;
        ++i;
    } while (lbRead && i < lrBuffer.mucChannels);
    if (lbRead)
    {
        ++lrBuffer.muReadOrdinal;
        lrBuffer.muReadyFrames -= static_cast<s32>(lrSlot.muSamplesAndFlags) >> 1;
        lrBuffer.muInUseFrames += static_cast<s32>(lrSlot.muSamplesAndFlags) >> 1;
    }
    if (lrBuffer.maiReadSlot[aiChannel] >= lrBuffer.miCapacity)
        lrBuffer.maiReadSlot[aiChannel] = 0;
    --lrBuffer.maiReadySlots[aiChannel];

    u8* lpSamples = lrSlot.mapChannelSamples[aiChannel];
    return lpSamples ? lpSamples : reinterpret_cast<u8*>(~uintptr_t(0));
}

// ARTIST 0x82B74388.
extern "C" void SNDPKTPLAYI_freeframes(s32 aiHandle, s32 aiChannel, s32 aiFrames)
{
    Buffer& lrBuffer = *gapBuffers[aiHandle];
    if (aiChannel == lrBuffer.mcReleaseChannel)
    {
        lrBuffer.muInUseFrames -= aiFrames;
        if (lrBuffer.mpFramesReleased)
        {
            Callback& lrCallback = gaCallbacks[giCallbackCount];
            lrCallback.muPacketReleased = 0;
            lrCallback.muHandle = aiHandle;
            lrCallback.miFrames = aiFrames;
            ++giCallbackCount;
        }
    }
}

// ARTIST 0x82B743F8.
extern "C" void SNDPKTPLAYI_flushcallbackdata()
{
    for (s32 i = 0; i < giCallbackCount; ++i)
    {
        const Callback& lrCallback = gaCallbacks[i];
        const Buffer& lrBuffer = *gapBuffers[lrCallback.muHandle];
        if (lrCallback.muPacketReleased)
            lrBuffer.mpPacketReleased(lrCallback.mpSamples, lrBuffer.mpContext);
        else
            lrBuffer.mpFramesReleased(lrCallback.muHandle, lrCallback.miFrames, lrBuffer.mpContext);
    }
    giCallbackCount = 0;
}
