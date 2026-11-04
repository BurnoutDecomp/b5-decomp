#include "SDKs/EATech/include/snd/sndmix.h"

#include "SDKs/EATech/include/snd/sndhal.h"
#include "rw/audio/core/PlugIn.h"
#include <cstddef>
#include <cstring>

using rw::audio::core::System;
using namespace Snd9::Mix;

extern "C" System* off_83271928;

namespace Snd
{
    void* (*gpfnSndAlloc)(u32);
    void (*gpfnSndFree)(void*);

    // ARTIST 0x82F883DC initially points at memcpy, then at the folded copy thunk.
    void* (*gpfnSndCopy)(void*, const void*, size_t) = &std::memcpy;
}

namespace Snd9
{
namespace Mix
{
    u8 gucOutputMode;
    CreateParams gCreateParams;
    f32** gppFxMixBuffer;
    s32* gpiFxMixEnabled;
    f32* gpfFxMixTail;
    FxAllocation** gppFxAllocations;
    s32 giFxGlobalsCreated;
    FilterInit gapfnFilterInit[18];
    u32 gauFilterBytes[18];
    void* gapPingPongAllocation[2];
    f32* gapPingPongBuffer[2];
    void* gapOutputAllocation[6];
    f32* gapOutputBuffer[6];
    MixSamples gpfnMixSamples;
    DecodeSamples gpfnDecodeSamples;
}
}

extern "C"
{
    void SFILTER_unpackfinit(FilterState*, VoiceStartParams*);
    void SFILTER_unpacklfinit(FilterState*, VoiceStartParams*);
    void SFILTER_unpackpfinit(FilterState*, VoiceStartParams*);
    void SFILTER_unpackxafinit(FilterState*, VoiceStartParams*);
    void SFILTER_unpackxalfinit(FilterState*, VoiceStartParams*);
    void SFILTER_unpackxapfinit(FilterState*, VoiceStartParams*);
    void SFILTER_unpackmtfinit(FilterState*, VoiceStartParams*);
    void SFILTER_unpackmtlfinit(FilterState*, VoiceStartParams*);
    void SFILTER_unpackmtpfinit(FilterState*, VoiceStartParams*);
}

CGS_STATIC_ASSERT((sizeof(CreateParams) == 0xC), "ARTIST mixer creation block");
CGS_STATIC_ASSERT((offsetof(CreateParams, mpVoiceFree) == 4), "ARTIST voice callback");
CGS_STATIC_ASSERT((offsetof(CreateParams, mucVoiceCount) == 8), "ARTIST mixer voice count");
CGS_STATIC_ASSERT((offsetof(CreateParams, mucOutputCount) == 9), "ARTIST mixer output count");
CGS_STATIC_ASSERT((offsetof(CreateParams, mucFlags) == 10), "ARTIST mixer creation flags");

// ARTIST 0x82B7BAB0 and 0x82B7BAD0.
extern "C" void* SNDI_New(u32 auSize)
{
    return System::Alloc(off_83271928, auSize, "New Dummy ID", 16, 0);
}

extern "C" void SNDI_Delete(void* apBlock)
{
    System::Free(off_83271928, apBlock, 0);
}

// ARTIST 0x82B6FC80 is also used as the RSA signature copy thunk.
static void* CopySamples(void* apDest, const void* apSource, size_t auBytes)
{
    return std::memcpy(apDest, apSource, auBytes);
}

// ARTIST 0x82B7B360. The first allocation packs buffer pointers, mix flags and tail samples.
extern "C" void MIX_createFxglobals()
{
    const u32 luFxBytes = (sizeof(f32*) + sizeof(s32) + sizeof(f32)) * Snd9::Hal::giNumOutputs;
    gppFxMixBuffer = static_cast<f32**>(
        System::Alloc(off_83271928, luFxBytes, "Global FX mems", 16, 0));
    gpiFxMixEnabled = reinterpret_cast<s32*>(gppFxMixBuffer + Snd9::Hal::giNumOutputs);
    gpfFxMixTail = reinterpret_cast<f32*>(gpiFxMixEnabled + Snd9::Hal::giNumOutputs);
    std::memset(gppFxMixBuffer, 0, luFxBytes);

    gppFxAllocations = static_cast<FxAllocation**>(
        System::Alloc(off_83271928, sizeof(FxAllocation*) * Snd9::Hal::giNumOutputs,
                      "Global FX mems", 16, 0));
    std::memset(gppFxAllocations, 0, sizeof(FxAllocation*) * Snd9::Hal::giNumOutputs);
    giFxGlobalsCreated = 1;
}

// ARTIST 0x82B7B418. Read each link before releasing its node.
extern "C" void MIX_destroyFxglobals()
{
    if (!giFxGlobalsCreated)
        return;

    System::Free(off_83271928, gppFxMixBuffer, 0);
    for (s32 i = 0; i < Snd9::Hal::giNumOutputs; ++i)
    {
        FxAllocation* lpAllocation = gppFxAllocations[i];
        while (lpAllocation)
        {
            FxAllocation* lpNext = lpAllocation->mpNext;
            System::Free(off_83271928, lpAllocation, 0);
            lpAllocation = lpNext;
        }
    }
    System::Free(off_83271928, gppFxAllocations, 0);
    giFxGlobalsCreated = 0;
}

// ARTIST 0x82B7B4D8. The XDK emits the single-precision fused multiply-add.
extern "C" void mixc(s32 aiCount, f32 afGain, const f32* apSource, f32* apDest)
{
    for (s32 i = 0; i < aiCount; ++i)
        apDest[i] = apSource[i] * afGain + apDest[i];
}

// ARTIST 0x82B7CB98. Keep the unscaled signed PCM amplitude.
extern "C" void decode16x87(s32 aiCount, const s16* apSource, f32* apDest)
{
    for (s32 i = 0; i < aiCount; ++i)
        apDest[i] = static_cast<f32>(apSource[i]);
}

// ARTIST 0x82B859B8.
extern "C" void MIXI_initunpackmt()
{
    gapfnFilterInit[6] = SFILTER_unpackmtfinit;
    gauFilterBytes[6] = 0x2C;
    gapfnFilterInit[8] = SFILTER_unpackmtpfinit;
    gauFilterBytes[8] = 0x3C;
    gapfnFilterInit[7] = SFILTER_unpackmtlfinit;
    gauFilterBytes[7] = 0x30;
    gapfnFilterInit[9] = SFILTER_unpackmtfinit;
    gauFilterBytes[9] = 0x2C;
    gapfnFilterInit[11] = SFILTER_unpackmtpfinit;
    gauFilterBytes[11] = 0x3C;
    gapfnFilterInit[10] = SFILTER_unpackmtlfinit;
    gauFilterBytes[10] = 0x30;
}

// ARTIST 0x82B85A40.
extern "C" void MIX_create(const CreateParams* apParams)
{
    Snd::gpfnSndAlloc = SNDI_New;
    Snd::gpfnSndFree = SNDI_Delete;
    Snd::gpfnSndCopy = CopySamples;
    std::memcpy(&gCreateParams, apParams, sizeof(gCreateParams));

    System::Lock(off_83271928);
    MIX_createFxglobals();
    for (s32 i = 0; i < Snd9::Hal::giNumOutputs; ++i)
        gpiFxMixEnabled[i] = 0;

    for (s32 i = 0; i < 2; ++i)
    {
        void* lpAllocation = System::Alloc(off_83271928, 0x20BC, "Mixer Ping Pong Temp Buf", 16, 0);
        gapPingPongAllocation[i] = lpAllocation;
        // Leave the native 8-byte lead-in before aligning to 64 bytes.
        gapPingPongBuffer[i] = reinterpret_cast<f32*>(
            (reinterpret_cast<uintptr_t>(lpAllocation) + 0x47) & ~uintptr_t(0x3F));
    }
    for (s32 i = 0; i < gCreateParams.mucOutputCount; ++i)
    {
        void* lpAllocation = System::Alloc(off_83271928, 0x840, "Mixer Output Buffer", 16, 0);
        gapOutputAllocation[i] = lpAllocation;
        gapOutputBuffer[i] = reinterpret_cast<f32*>(
            (reinterpret_cast<uintptr_t>(lpAllocation) + 0x3F) & ~uintptr_t(0x3F));
    }
    if (gCreateParams.mucVoiceCount)
    {
        const u32 luBytesPerVoice = sizeof(Snd9::Hal::MixChannel) +
                                    2 * sizeof(f32) * Snd9::Hal::giNumOutputs;
        Snd9::Hal::gpMixChannels = static_cast<Snd9::Hal::MixChannel*>(
            System::Alloc(off_83271928, luBytesPerVoice * gCreateParams.mucVoiceCount,
                          "Mixer Voice State", 16, 0));
        std::memset(Snd9::Hal::gpMixChannels, 0,
                    (sizeof(Snd9::Hal::MixChannel) + 2 * sizeof(f32) * Snd9::Hal::giNumOutputs) *
                    gCreateParams.mucVoiceCount);
        f32* lpGains = reinterpret_cast<f32*>(Snd9::Hal::gpMixChannels + gCreateParams.mucVoiceCount);
        for (s32 i = 0; i < gCreateParams.mucVoiceCount; ++i)
        {
            Snd9::Hal::gpMixChannels[i].mpPreviousOutputGains = lpGains;
            lpGains += Snd9::Hal::giNumOutputs;
            Snd9::Hal::gpMixChannels[i].mpOutputBuffer = lpGains;
            lpGains += Snd9::Hal::giNumOutputs;
        }
    }
    System::Unlock(off_83271928);

    gapfnFilterInit[0] = SFILTER_unpackfinit;
    gauFilterBytes[0] = 0x28;
    gapfnFilterInit[1] = SFILTER_unpacklfinit;
    gauFilterBytes[1] = 0x2C;
    gapfnFilterInit[2] = SFILTER_unpackpfinit;
    gauFilterBytes[2] = 0x34;
    gapfnFilterInit[3] = SFILTER_unpackxafinit;
    gauFilterBytes[3] = 0x2C;
    gauFilterBytes[4] = 0x54;
    gapfnFilterInit[4] = SFILTER_unpackxalfinit;
    gapfnFilterInit[5] = SFILTER_unpackxapfinit;
    gauFilterBytes[5] = 0x3C;
    MIXI_initunpackmt();
    gpfnDecodeSamples = decode16x87;
    gpfnMixSamples = mixc;

    for (s32 i = 0; i < gCreateParams.mucOutputCount; ++i)
        std::memset(gapOutputBuffer[i], 0, 0x800);
}

// ARTIST 0x82B7BAE8. The aligned views are never handed to the allocator.
extern "C" void MIX_destroy()
{
    System::Lock(off_83271928);
    MIX_destroyFxglobals();
    if (Snd9::Hal::gpMixChannels)
    {
        System::Free(off_83271928, Snd9::Hal::gpMixChannels, 0);
        Snd9::Hal::gpMixChannels = 0;
    }
    for (s32 i = 0; i < 2; ++i)
    {
        if (gapPingPongAllocation[i])
        {
            System::Free(off_83271928, gapPingPongAllocation[i], 0);
            gapPingPongAllocation[i] = 0;
        }
    }
    for (s32 i = 0; i < gCreateParams.mucOutputCount; ++i)
    {
        if (gapOutputAllocation[i])
        {
            System::Free(off_83271928, gapOutputAllocation[i], 0);
            gapOutputAllocation[i] = 0;
        }
    }
    System::Unlock(off_83271928);
}
