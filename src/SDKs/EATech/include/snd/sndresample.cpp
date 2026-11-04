#include "SDKs/EATech/include/snd/sndfilter.h"
#include "SDKs/EATech/include/snd/sndhal.h"
#include "rw/audio/core/PlugIn.h"

using rw::audio::core::System;
extern "C" System* off_83271928;

namespace Snd9
{
namespace Mix
{
    typedef void (*ResampleKernel)(s32, const f32*, f32*, u32*, u32*, u32, u32);

    struct ResampleFilter
    {
        FilterState mHeader;
        s32 miPitch;                   // +0x1C, 16.16 source samples per output
        u32 muPhase;                   // +0x20, fractional phase at block entry
        ResampleKernel mpKernel;       // +0x24
        u16 muHasHistory;              // +0x28
        s16 miHistoryOffset;           // +0x2A, one preceding sample for cubic
        f32 mafHistory[4];             // +0x2C
    };

    CGS_STATIC_ASSERT((sizeof(ResampleFilter) == 0x3C), "ARTIST resampler state");
    CGS_STATIC_ASSERT((offsetof(ResampleFilter, miPitch) == 0x1C), "ARTIST pitch step");
    CGS_STATIC_ASSERT((offsetof(ResampleFilter, muPhase) == 0x20), "ARTIST phase");
    CGS_STATIC_ASSERT((offsetof(ResampleFilter, mpKernel) == 0x24), "ARTIST resample kernel");
    CGS_STATIC_ASSERT((offsetof(ResampleFilter, mafHistory) == 0x2C), "ARTIST resample history");

    // ARTIST 0x821565B0. This rounded coefficient is shared by both kernels.
    const f32 KF_PHASE_TO_FLOAT = 0.000015257999621098861f;

    // ARTIST 0x82B7A9B0.
    static void ResampleLinear(s32 aiCount, const f32* apSource, f32* apDest,
                               u32* apPosition, u32* apFraction, u32 auPitchWhole, u32 auPitchFraction)
    {
        const u32 luPitch = (auPitchWhole << 16) | (auPitchFraction >> 16);
        u32 luFraction = *apFraction >> 16;
        u32 luPosition = *apPosition;
        for (s32 i = 0; i < aiCount; ++i)
        {
            const f32 lfPhase = static_cast<f32>(luFraction) * KF_PHASE_TO_FLOAT;
            const f32* lpPair = apSource + luPosition;
            const f32 lfDelta = lpPair[1] - lpPair[0];
            apDest[i] = lfDelta * lfPhase + lpPair[0];

            const u32 luAdvanced = luFraction + luPitch;
            luPosition += luAdvanced >> 16;
            luFraction = luAdvanced & 0xFFFF;
        }
        *apPosition = luPosition;
        *apFraction = luFraction << 16;
    }

    // ARTIST 0x82B7A898. Evaluate the Catmull-Rom coefficients in the native order.
    static void ResampleCubic(s32 aiCount, const f32* apSource, f32* apDest,
                              u32* apPosition, u32* apFraction, u32 auPitchWhole, u32 auPitchFraction)
    {
        const u32 luPitch = (auPitchWhole << 16) | (auPitchFraction >> 16);
        u32 luFraction = *apFraction >> 16;
        u32 luPosition = *apPosition;
        for (s32 i = 0; i < aiCount; ++i)
        {
            const f32* lpSamples = apSource + luPosition;
            const f32 lfPrevious = lpSamples[-1];
            const f32 lfCurrent = lpSamples[0];
            const f32 lfNext = lpSamples[1];
            const f32 lfFollowing = lpSamples[2];
            const f32 lfDelta = lfCurrent - lfNext;
            const f32 lfSlope = (lfNext - lfPrevious) * 0.5f;
            const f32 lfQuadraticHigh = lfNext * 2.0f + lfPrevious;
            const f32 lfQuadraticLow = lfCurrent * 5.0f + lfFollowing;
            const f32 lfQuadratic = lfQuadraticHigh - lfQuadraticLow * 0.5f;
            const f32 lfCubicBase = lfDelta * 3.0f - lfPrevious;
            const f32 lfCubic = (lfCubicBase + lfFollowing) * 0.5f;

            const f32 lfPhase = static_cast<f32>(luFraction) * KF_PHASE_TO_FLOAT;
            const f32 lfPhase2 = lfPhase * lfPhase;
            const f32 lfPhase3 = lfPhase2 * lfPhase;
            const f32 lfQuadraticTerm = lfQuadratic * lfPhase2;
            const f32 lfCurve = lfCubic * lfPhase3 + lfQuadraticTerm;
            const f32 lfInterpolated = lfSlope * lfPhase + lfCurve;
            apDest[i] = lfInterpolated + lfCurrent;

            const u32 luAdvanced = luFraction + luPitch;
            luPosition += luAdvanced >> 16;
            luFraction = luAdvanced & 0xFFFF;
        }
        *apPosition = luPosition;
        *apFraction = luFraction << 16;
    }

    static void Remember(ResampleFilter& arFilter, const f32* apSamples)
    {
        for (s32 i = 0; i < 4; ++i)
            arFilter.mafHistory[i] = apSamples[i];
    }

    static void ClearHistory(ResampleFilter& arFilter)
    {
        if (!arFilter.muHasHistory)
            arFilter.muHasHistory = 1;
        for (s32 i = 0; i < 4; ++i)
            arFilter.mafHistory[i] = 0.0f;
    }

    // ARTIST 0x82B7B010.
    static s32 ProcessResample(FilterState* apState, s32 aiCount, f32* apScratch, f32* apDest, s32)
    {
        ResampleFilter& lrFilter = *reinterpret_cast<ResampleFilter*>(apState);
        s32 liRequested = aiCount;
        s32 liResult = aiCount;
        if (lrFilter.miPitch == 0x10000)
        {
            if (lrFilter.muHasHistory)
            {
                for (s32 i = lrFilter.miHistoryOffset; i < 4; ++i)
                    *apDest++ = lrFilter.mafHistory[i];
            }
            else
            {
                liRequested += 4 - lrFilter.miHistoryOffset;
            }

            FilterState* lpNext = lrFilter.mHeader.mpNext;
            if (lpNext)
                liResult = lpNext->mpProcess(lpNext, liRequested, apScratch, apDest, lrFilter.mHeader.mucFlags);
            if (liResult <= 0)
            {
                ClearHistory(lrFilter);
                return liResult;
            }

            const s32 liTail = aiCount - (lrFilter.muHasHistory ? 4 : lrFilter.miHistoryOffset);
            if (!lrFilter.muHasHistory)
                lrFilter.muHasHistory = 1;
            Remember(lrFilter, apDest + liTail);
            return aiCount;
        }

        const u32 luPitch = lrFilter.miPitch;
        liRequested = (luPitch * static_cast<u32>(aiCount) + lrFilter.muPhase) >> 16;
        f32* lpInput = apScratch;
        if (lrFilter.muHasHistory)
        {
            for (s32 i = 0; i < 4; ++i)
                *lpInput++ = lrFilter.mafHistory[i];
        }
        else
        {
            liRequested += 4 - lrFilter.miHistoryOffset;
            for (s32 i = 0; i < lrFilter.miHistoryOffset; ++i)
                *lpInput++ = 0.0f;
        }

        FilterState* lpNext = lrFilter.mHeader.mpNext;
        if (liRequested > 0 && lpNext)
        {
            liResult = lpNext->mpProcess(lpNext, liRequested, apDest, lpInput, lrFilter.mHeader.mucFlags);
            if (liResult <= 0)
            {
                ClearHistory(lrFilter);
                return liResult;
            }
        }

        if (lrFilter.muHasHistory)
            lpInput += lrFilter.miHistoryOffset - 4;
        else
            lrFilter.muHasHistory = 1;
        u32 luPosition = 0;
        u32 luFraction = lrFilter.muPhase << 16;
        lrFilter.mpKernel(aiCount, lpInput, apDest, &luPosition, &luFraction, luPitch >> 16, luPitch << 16);
        lrFilter.muPhase = (luPosition << 16) | (luFraction >> 16);
        Remember(lrFilter, lpInput + (static_cast<s32>(lrFilter.muPhase >> 16) - lrFilter.miHistoryOffset));
        lrFilter.muPhase &= 0xFFFF;
        return aiCount;
    }

    // ARTIST 0x82B7B2E8. Its mode byte in r4 is unused by the implementation.
    static void InitResample(ResampleFilter* apFilter, bool abCubic)
    {
        apFilter->mHeader.mpRestore = 0;
        apFilter->mHeader.mpProcess = ProcessResample;
        apFilter->muPhase = 0;
        apFilter->muHasHistory = 0;
        apFilter->miHistoryOffset = abCubic ? 1 : 0;
        apFilter->mpKernel = abCubic ? ResampleCubic : ResampleLinear;
        for (s32 i = 0; i < 4; ++i)
            apFilter->mafHistory[i] = 0.0f;
    }
}
}

using namespace Snd9::Mix;

// ARTIST 0x82B7AA58. Equal priorities insert before the current node.
extern "C" void SFILTER_add(FilterState** appHead, FilterState* apFilter)
{
    FilterState* lpNext = *appHead;
    FilterState* lpPrevious = 0;
    while (lpNext && lpNext->muPriority < apFilter->muPriority)
    {
        lpPrevious = lpNext;
        lpNext = lpNext->mpNext;
    }

    apFilter->mpNext = lpNext;
    if (lpNext)
        lpNext->mpPrevious = apFilter;
    if (lpPrevious)
    {
        lpPrevious->mpNext = apFilter;
        apFilter->mpPrevious = lpPrevious;
    }
    else
    {
        *appHead = apFilter;
    }
}

// ARTIST 0x82B7BED0.
extern "C" void MIX_setpitch(s32 aiVoice, s32 aiPitch)
{
    Snd9::Hal::MixChannel& lrChannel = Snd9::Hal::gpMixChannels[aiVoice];
    if (aiPitch > 0x40000)
        aiPitch = 0x40000;
    if (!lrChannel.mpResample)
    {
        lrChannel.mpResample = static_cast<ResampleFilter*>(
            System::Alloc(off_83271928, sizeof(ResampleFilter), "Resample Filter State", 16, 0));
        lrChannel.mpResample->mHeader.mpRestore = 0;
        lrChannel.mpResample->mHeader.muPriority = 160;
        lrChannel.mpResample->mHeader.mucFlags = 0;
        InitResample(lrChannel.mpResample, gucOutputMode >= 50);
        SFILTER_add(&lrChannel.mpFilter, &lrChannel.mpResample->mHeader);
    }
    lrChannel.mpResample->miPitch = aiPitch;
}
