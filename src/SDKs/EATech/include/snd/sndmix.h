#pragma once

#include "types.hpp"
#include <cstddef>

namespace Snd
{
    extern void* (*gpfnSndAlloc)(u32 auSize);               // ARTIST 0x83277830
    extern void (*gpfnSndFree)(void* apBlock);              // ARTIST 0x83277834
    extern void* (*gpfnSndCopy)(void* apDest, const void* apSource, size_t auBytes);
}

namespace Snd9
{
namespace Mix
{
    struct FilterState;
    struct VoiceStartParams;
    struct FxAllocation
    {
        FxAllocation* mpNext;
    };

    typedef void (*VoiceFree)(s32 aiChannel);
    typedef void (*FilterInit)(FilterState* apFilter, VoiceStartParams* apParams);
    typedef void (*MixSamples)(s32 aiCount, f32 afGain, const f32* apSource, f32* apDest);
    typedef void (*DecodeSamples)(s32 aiCount, const s16* apSource, f32* apDest);

    struct CreateParams
    {
        s32 miSampleRate;
        VoiceFree mpVoiceFree;
        u8 mucVoiceCount;
        u8 mucOutputCount;
        u8 mucFlags;
    };

    extern u8 gucOutputMode;                   // ARTIST 0x83277724
    extern CreateParams gCreateParams;         // ARTIST 0x83277718
    extern f32** gppFxMixBuffer;                  // ARTIST 0x83277608
    extern s32* gpiFxMixEnabled;               // ARTIST 0x8327772C
    extern f32* gpfFxMixTail;              // ARTIST 0x83277818
    extern FxAllocation** gppFxAllocations;     // ARTIST 0x8327761C
    extern s32 giFxGlobalsCreated;              // ARTIST 0x83277CA0
    extern FilterInit gapfnFilterInit[18];      // ARTIST 0x83277730
    extern u32 gauFilterBytes[18];              // ARTIST 0x83277778
    extern void* gapPingPongAllocation[2];      // ARTIST 0x832777C0
    extern f32* gapPingPongBuffer[2];           // ARTIST 0x832777C8
    extern void* gapOutputAllocation[6];        // ARTIST 0x832777D0
    extern f32* gapOutputBuffer[6];             // ARTIST 0x832777E8
    extern MixSamples gpfnMixSamples;          // ARTIST 0x83277820
    extern DecodeSamples gpfnDecodeSamples;    // ARTIST 0x83277824
}
}

extern "C" void* SNDI_New(u32 auSize);
extern "C" void SNDI_Delete(void* apBlock);
extern "C" void MIX_create(const Snd9::Mix::CreateParams* apParams);
extern "C" void MIX_destroy();
extern "C" void MIX_audio(f32* const* appChannels, s32 aiSampleCount);
extern "C" void MIX_setpitch(s32 aiVoice, s32 aiPitch);
extern "C" void MIX_stop(s32 aiVoice);
