#include "SDKs/EATech/include/snd/sndfilter.h"

#include "SDKs/EATech/include/snd/CEAXABLKDecf.h"
#include "SDKs/EATech/include/snd/CMTBLKDecf.h"
#include "SDKs/EATech/include/snd/sndpacket.h"
#include "SDKs/EATech/include/snd/sndsys.h"
#include <cstring>

namespace Snd9
{
namespace Mix
{
    CGS_STATIC_ASSERT((sizeof(FilterState) == 0x1C), "ARTIST filter header");
    CGS_STATIC_ASSERT((offsetof(FilterState, mpNext) == 8), "ARTIST next filter");
    CGS_STATIC_ASSERT((offsetof(FilterState, mpPrevious) == 0x10), "ARTIST previous filter");
    CGS_STATIC_ASSERT((offsetof(FilterState, muPriority) == 0x18), "ARTIST filter priority");
    CGS_STATIC_ASSERT((sizeof(VoiceStartParams) == 0x28), "ARTIST voice start parameters");
    CGS_STATIC_ASSERT((offsetof(VoiceStartParams, mpPosition) == 0x24), "ARTIST position callback");

    struct PcmFilter
    {
        FilterState mHeader;
        const s16* mpSource;
        u32 muCursor;
        u32 muSampleCount;
    };

    struct LoopPcmFilter
    {
        FilterState mHeader;
        const s16* mpSource;
        u32 muCursor;
        u32 muLoopStart;
        u32 muLoopEnd;
    };

    struct MicroTalkFilter
    {
        FilterState mHeader;
        u32 muSampleCount;
        u32 muCursor;
        s32 miDecodeMode;
        Snd::CMTBLKDecf* mpDecoder;
    };

    struct LoopMicroTalkFilter
    {
        FilterState mHeader;
        Snd::CMTBLKDecf* mpDecoder;
        u32 muCursor;
        u32 muLoopStart;
        u32 muLoopEnd;
        u8* mpLoopSource;
    };

    struct XaFilter
    {
        FilterState mHeader;
        Snd::CEAXABLKDecf* mpDecoder;
        const u8* mpSource;
        s32 miSampleCount;
        u32 muCursor;
    };

    struct LoopXaFilter
    {
        FilterState mHeader;
        Snd::CEAXABLKDecf* mpDecoder;
        u32 muCursor;
        s32 miLoopStart;
        s32 miLoopEnd;
        const u8* mpLoopSource;
        s32 miDecodeMode;
        const u8* mpFallbackSource;
        u32 muFallbackSampleCount;
        u32 mauFallbackCursor[2];
        u32 muFallbackLoopStart;
        u32 muFallbackLoopEnd;
        u8 mauUnknown4C[8];
    };

    struct PacketPcmFilter
    {
        FilterState mHeader;
        const s16* mpSource;
        s32 miFrameCount;
        s32 miCursor;
        s32 miHandle;
        s32 miDecoded;
        u8 mucChannel;
        u8 mauPad31[3];
    };

    struct PacketXaFilter
    {
        FilterState mHeader;
        Snd::CEAXABLKDecf* mpDecoder;
        const u8* mpSource;
        s32 miFrameCount;
        s32 miCursor;
        s32 miDecodeMode;
        s32 miHandle;
        s32 miDecoded;
        u8 mucChannel;
        u8 mauPad39[3];
    };

    struct PacketMicroTalkFilter
    {
        FilterState mHeader;
        u8* mpSource;
        s32 miFrameCount;
        s32 miHandle;
        s32 miDecoded;
        u16 muCursor;
        u8 mauPad2E[2];
        s32 miDecodeMode;
        Snd::CMTBLKDecf* mpDecoder;
        u8 mucChannel;
        u8 mauPad39[3];
    };

    CGS_STATIC_ASSERT((sizeof(PacketPcmFilter) == 0x34), "ARTIST packet PCM filter");
    CGS_STATIC_ASSERT((sizeof(PacketXaFilter) == 0x3C), "ARTIST packet XA filter");
    CGS_STATIC_ASSERT((sizeof(PacketMicroTalkFilter) == 0x3C), "ARTIST packet MicroTalk filter");
    CGS_STATIC_ASSERT((offsetof(PacketMicroTalkFilter, muCursor) == 0x2C), "ARTIST packet MicroTalk cursor");
    CGS_STATIC_ASSERT((offsetof(PacketMicroTalkFilter, mpDecoder) == 0x34), "ARTIST packet MicroTalk decoder");

    CGS_STATIC_ASSERT((sizeof(XaFilter) == 0x2C), "ARTIST XA filter");
    CGS_STATIC_ASSERT((sizeof(LoopXaFilter) == 0x54), "ARTIST looped XA filter");
    CGS_STATIC_ASSERT((offsetof(XaFilter, muCursor) == 0x28), "ARTIST XA cursor");
    CGS_STATIC_ASSERT((offsetof(LoopXaFilter, mpFallbackSource) == 0x34), "ARTIST looped XA alternate state");

    CGS_STATIC_ASSERT((sizeof(PcmFilter) == 0x28), "ARTIST PCM filter");
    CGS_STATIC_ASSERT((sizeof(LoopPcmFilter) == 0x2C), "ARTIST looped PCM filter");
    CGS_STATIC_ASSERT((sizeof(MicroTalkFilter) == 0x2C), "ARTIST MicroTalk filter");
    CGS_STATIC_ASSERT((sizeof(LoopMicroTalkFilter) == 0x30), "ARTIST looped MicroTalk filter");
    CGS_STATIC_ASSERT((offsetof(MicroTalkFilter, muCursor) == 0x20), "ARTIST MicroTalk cursor");
    CGS_STATIC_ASSERT((offsetof(Snd::CMTBLKDecf, miMode) == 0xD64), "ARTIST MicroTalk mode");
    CGS_STATIC_ASSERT((sizeof(Snd::CMTBLKDecf) == 0xD68), "ARTIST MicroTalk decoder");

    // The PCM and MicroTalk cursor readers fold with AptCIH::GetAnimationInst
    // at 0x82B7B358. Each reads the filter-specific cursor at +0x20.
    template <class Filter>
    static u32 ReadCursor(FilterState* apState)
    {
        return reinterpret_cast<Filter*>(apState)->muCursor;
    }

    // ARTIST 0x82B7C808.
    static s32 ProcessPcm(FilterState* apState, s32 aiCount, f32*, f32* apDest, s32)
    {
        PcmFilter& lrFilter = *reinterpret_cast<PcmFilter*>(apState);
        const u32 luCursor = lrFilter.muCursor;
        const u32 luSampleCount = lrFilter.muSampleCount;
        if (luCursor >= luSampleCount)
            return -1;

        lrFilter.muCursor = luCursor + aiCount;
        if (lrFilter.muCursor < luSampleCount)
        {
            gpfnDecodeSamples(aiCount, lrFilter.mpSource + luCursor, apDest);
        }
        else
        {
            const s32 liAvailable = luSampleCount - luCursor;
            gpfnDecodeSamples(liAvailable, lrFilter.mpSource + luCursor, apDest);
            std::memset(apDest + liAvailable, 0, (aiCount - liAvailable) * sizeof(f32));
        }
        return 1;
    }

    // ARTIST 0x82B7C8F0.
    static s32 ProcessLoopPcm(FilterState* apState, s32 aiCount, f32*, f32* apDest, s32)
    {
        LoopPcmFilter& lrFilter = *reinterpret_cast<LoopPcmFilter*>(apState);
        while (aiCount > 0)
        {
            s32 liCount = lrFilter.muLoopEnd - lrFilter.muCursor + 1;
            if (aiCount < liCount)
                liCount = aiCount;
            gpfnDecodeSamples(liCount, lrFilter.mpSource + lrFilter.muCursor, apDest);
            apDest += liCount;
            aiCount -= liCount;
            lrFilter.muCursor += liCount;
            if (lrFilter.muCursor > lrFilter.muLoopEnd)
                lrFilter.muCursor = lrFilter.muLoopStart;
        }
        return 1;
    }

    static Snd::CMTBLKDecf* NewMicroTalkDecoder()
    {
        Snd::CMTBLKDecf* lpDecoder = static_cast<Snd::CMTBLKDecf*>(
            Snd::gpfnSndAlloc(sizeof(Snd::CMTBLKDecf)));
        if (lpDecoder)
        {
            lpDecoder->miWindowCount = 0;
            lpDecoder->miBlockRemaining = 0;
            lpDecoder->miCapacity = 0;
            lpDecoder->miHeaderA = 0;
            lpDecoder->miHeaderB = 0;
            lpDecoder->miInitFlag = 0;
            lpDecoder->miFrameFlag = 0;
            lpDecoder->miMode = 1;
        }
        return lpDecoder;
    }

    template <class Filter>
    static void RestoreMicroTalk(FilterState* apState)
    {
        Snd::CMTBLKDecf* lpDecoder = reinterpret_cast<Filter*>(apState)->mpDecoder;
        if (lpDecoder)
            Snd::gpfnSndFree(lpDecoder);
    }

    // ARTIST 0x82B85480.
    static s32 ProcessMicroTalk(FilterState* apState, s32 aiCount, f32*, f32* apDest, s32)
    {
        MicroTalkFilter& lrFilter = *reinterpret_cast<MicroTalkFilter*>(apState);
        if (lrFilter.muCursor >= lrFilter.muSampleCount)
            return -1;

        const s32 liDecoded = lrFilter.mpDecoder->Decode(&apDest, aiCount);
        lrFilter.muCursor += liDecoded;
        if (liDecoded < aiCount)
            std::memset(apDest + liDecoded, 0, (aiCount - liDecoded) * sizeof(f32));
        return 1;
    }

    // ARTIST 0x82B855D8.
    static s32 ProcessLoopMicroTalk(FilterState* apState, s32 aiCount, f32*, f32* apDest, s32)
    {
        LoopMicroTalkFilter& lrFilter = *reinterpret_cast<LoopMicroTalkFilter*>(apState);
        while (aiCount > 0)
        {
            const s32 liDecoded = lrFilter.mpDecoder->Decode(&apDest, aiCount);
            aiCount -= liDecoded;
            apDest += liDecoded;
            lrFilter.muCursor += liDecoded;
            if (liDecoded < aiCount)
            {
                lrFilter.muCursor = lrFilter.muLoopStart;
                lrFilter.mpDecoder->mReader.muAccum = 0;
                lrFilter.mpDecoder->miInitFlag = 1;
                lrFilter.mpDecoder->miWindowCount = 0;
                const s32 liLoopCount = lrFilter.muLoopEnd - lrFilter.muLoopStart + 1;
                lrFilter.mpDecoder->Feed(lrFilter.mpLoopSource, 4 * liLoopCount, liLoopCount);
            }
        }
        return 1;
    }

    static Snd::CEAXABLKDecf* NewXaDecoder()
    {
        Snd::CEAXABLKDecf* lpDecoder = static_cast<Snd::CEAXABLKDecf*>(
            Snd::gpfnSndAlloc(sizeof(Snd::CEAXABLKDecf)));
        if (lpDecoder)
        {
            lpDecoder->miCapacity = 0;
            lpDecoder->miRemaining = 0;
            lpDecoder->miBufferedCount = 0;
            lpDecoder->mBlock.mfHist0 = 0.0f;
            lpDecoder->mBlock.mfHist1 = 0.0f;
        }
        return lpDecoder;
    }

    template <class Filter>
    static void RestoreXa(FilterState* apState)
    {
        Snd::CEAXABLKDecf* lpDecoder = reinterpret_cast<Filter*>(apState)->mpDecoder;
        if (lpDecoder)
            Snd::gpfnSndFree(lpDecoder);
    }

    static void FeedXa(Snd::CEAXABLKDecf* apDecoder, const u8* apSource, s32 aiCount)
    {
        if (apSource && apDecoder->miRemaining == 0)
        {
            apDecoder->miRemaining = aiCount;
            apDecoder->miCapacity = 4 * aiCount;
            apDecoder->mBlock.mpSource = apSource;
        }
    }

    // ARTIST 0x82B81178.
    static s32 ProcessXa(FilterState* apState, s32 aiCount, f32*, f32* apDest, s32)
    {
        XaFilter& lrFilter = *reinterpret_cast<XaFilter*>(apState);
        if (static_cast<s32>(lrFilter.muCursor) >= lrFilter.miSampleCount)
            return -1;

        const s32 liDecoded = lrFilter.mpDecoder->Decode(&apDest, aiCount);
        lrFilter.muCursor += liDecoded;
        if (liDecoded < aiCount)
        {
            std::memset(apDest + liDecoded, 0, (aiCount - liDecoded) * sizeof(f32));
            return aiCount;
        }
        return liDecoded;
    }

    // ARTIST 0x82B812D0.
    static s32 ProcessLoopXa(FilterState* apState, s32 aiCount, f32*, f32* apDest, s32)
    {
        LoopXaFilter& lrFilter = *reinterpret_cast<LoopXaFilter*>(apState);
        s32 liProduced = 0;
        while (aiCount > 0)
        {
            const s32 liDecoded = lrFilter.mpDecoder->Decode(&apDest, aiCount);
            aiCount -= liDecoded;
            liProduced += liDecoded;
            apDest += liDecoded;
            lrFilter.muCursor += liDecoded;
            if (liDecoded < aiCount)
            {
                lrFilter.muCursor = lrFilter.miLoopStart;
                Snd::CEAXABLKDecf* lpDecoder = lrFilter.mpDecoder;
                const s32 liLoopCount = lrFilter.miLoopEnd - lrFilter.miLoopStart + 1;
                if (lrFilter.mpLoopSource && lpDecoder->miRemaining == 0)
                {
                    lpDecoder->miCapacity = 4 * liLoopCount;
                    lpDecoder->mBlock.mpSource = lrFilter.mpLoopSource;
                    lpDecoder->miRemaining = liLoopCount;
                }
            }
        }
        return liProduced;
    }

    template <class Filter>
    static void ReleaseDecodedFrames(Filter& arFilter)
    {
        if (arFilter.miDecoded)
        {
            SNDPKTPLAYI_freeframes(arFilter.miHandle, arFilter.mucChannel, arFilter.miDecoded);
            arFilter.miDecoded = 0;
        }
    }

    // ARTIST 0x82B7C9F8.
    static s32 ProcessPacketPcm(FilterState* apState, s32 aiCount, f32*, f32* apDest, s32)
    {
        PacketPcmFilter& lrFilter = *reinterpret_cast<PacketPcmFilter*>(apState);
        ReleaseDecodedFrames(lrFilter);
        while (aiCount > 0)
        {
            if (lrFilter.miCursor >= lrFilter.miFrameCount)
            {
                s32 liFrames;
                s32 liContinuation;
                lrFilter.mpSource = reinterpret_cast<const s16*>(
                    SNDPKTPLAYI_get(lrFilter.miHandle, lrFilter.mucChannel, &liFrames, &liContinuation));
                if (!lrFilter.mpSource)
                {
                    if (lrFilter.miDecoded)
                        std::memset(apDest, 0, aiCount * sizeof(f32));
                    break;
                }
                lrFilter.miCursor = 0;
                lrFilter.miFrameCount = liFrames;
            }

            s32 liCount = lrFilter.miFrameCount - lrFilter.miCursor;
            if (aiCount < liCount)
                liCount = aiCount;
            gpfnDecodeSamples(liCount, lrFilter.mpSource + lrFilter.miCursor, apDest);
            lrFilter.miCursor += liCount;
            lrFilter.miDecoded += liCount;
            aiCount -= liCount;
            apDest += liCount;
        }
        return lrFilter.miDecoded;
    }

    // ARTIST 0x82B814A0.
    static s32 ProcessPacketXa(FilterState* apState, s32 aiCount, f32*, f32* apDest, s32)
    {
        PacketXaFilter& lrFilter = *reinterpret_cast<PacketXaFilter*>(apState);
        s32 liProduced = 0;
        ReleaseDecodedFrames(lrFilter);
        while (aiCount > 0)
        {
            const s32 liDecoded = lrFilter.mpDecoder->Decode(&apDest, aiCount);
            lrFilter.miDecoded += liDecoded;
            lrFilter.miCursor += liDecoded;
            liProduced += liDecoded;
            apDest += liDecoded;
            aiCount -= liDecoded;
            if (aiCount > 0)
            {
                s32 liContinuation;
                lrFilter.mpSource = SNDPKTPLAYI_get(lrFilter.miHandle, lrFilter.mucChannel,
                                                   &lrFilter.miFrameCount, &liContinuation);
                if (!lrFilter.mpSource)
                {
                    if (lrFilter.miDecoded)
                        std::memset(apDest, 0, aiCount * sizeof(f32));
                    break;
                }
                lrFilter.miCursor = 0;
                FeedXa(lrFilter.mpDecoder, lrFilter.mpSource, lrFilter.miFrameCount);
            }
        }
        return liProduced;
    }

    // ARTIST 0x82B85760.
    static s32 ProcessPacketMicroTalk(FilterState* apState, s32 aiCount, f32*, f32* apDest, s32)
    {
        PacketMicroTalkFilter& lrFilter = *reinterpret_cast<PacketMicroTalkFilter*>(apState);
        s32 liProduced = 0;
        ReleaseDecodedFrames(lrFilter);
        while (aiCount > 0)
        {
            const s32 liDecoded = lrFilter.mpDecoder->Decode(&apDest, aiCount);
            lrFilter.miDecoded += liDecoded;
            lrFilter.muCursor += liDecoded;
            liProduced += liDecoded;
            apDest += liDecoded;
            aiCount -= liDecoded;
            if (aiCount > 0)
            {
                s32 liContinuation;
                lrFilter.mpSource = SNDPKTPLAYI_get(lrFilter.miHandle, lrFilter.mucChannel,
                                                   &lrFilter.miFrameCount, &liContinuation);
                if (!lrFilter.mpSource)
                {
                    if (lrFilter.miDecoded)
                        std::memset(apDest, 0, aiCount * sizeof(f32));
                    lrFilter.miFrameCount = 0;
                    break;
                }
                lrFilter.muCursor = 0;
                lrFilter.mpDecoder->miInitFlag = liContinuation ? 0 : 1;
                if (!liContinuation)
                    lrFilter.mpDecoder->miWindowCount = 0;
                lrFilter.mpDecoder->miMode = 1;
                lrFilter.mpDecoder->Feed(lrFilter.mpSource + 1, 4 * lrFilter.miFrameCount,
                                         lrFilter.miFrameCount);
            }
        }
        return liProduced;
    }
}
}

using namespace Snd9::Mix;

// ARTIST 0x82B7C8B8.
extern "C" void SFILTER_unpackfinit(FilterState* apState, VoiceStartParams* apParams)
{
    PcmFilter& lrFilter = *reinterpret_cast<PcmFilter*>(apState);
    lrFilter.mHeader.mpProcess = ProcessPcm;
    lrFilter.muCursor = 0;
    lrFilter.mpSource = reinterpret_cast<const s16*>(apParams->mpSource);
    lrFilter.muSampleCount = apParams->muSampleCount;
    apParams->mpPosition = ReadCursor<PcmFilter>;
    lrFilter.mHeader.mpRestore = 0;
}

// ARTIST 0x82B7C998.
extern "C" void SFILTER_unpacklfinit(FilterState* apState, VoiceStartParams* apParams)
{
    LoopPcmFilter& lrFilter = *reinterpret_cast<LoopPcmFilter*>(apState);
    lrFilter.mHeader.mpProcess = ProcessLoopPcm;
    lrFilter.muCursor = 0;
    lrFilter.mpSource = reinterpret_cast<const s16*>(apParams->mpSource);
    lrFilter.muLoopStart = apParams->muLoopStart;
    lrFilter.muLoopEnd = apParams->muLoopEnd;
    apParams->mpPosition = ReadCursor<LoopPcmFilter>;
}

// ARTIST 0x82B85500.
extern "C" void SFILTER_unpackmtfinit(FilterState* apState, VoiceStartParams* apParams)
{
    MicroTalkFilter& lrFilter = *reinterpret_cast<MicroTalkFilter*>(apState);
    lrFilter.mHeader.mpProcess = ProcessMicroTalk;
    lrFilter.muCursor = 0;
    lrFilter.muSampleCount = apParams->muSampleCount;
    lrFilter.miDecodeMode = apParams->miDecodeMode;
    apParams->mpPosition = ReadCursor<MicroTalkFilter>;
    lrFilter.mHeader.mpRestore = RestoreMicroTalk<MicroTalkFilter>;
    lrFilter.mpDecoder = NewMicroTalkDecoder();
    lrFilter.mpDecoder->miInitFlag = 1;
    lrFilter.mpDecoder->miMode = 1;
    lrFilter.mpDecoder->Feed(apParams->mpSource, 4 * lrFilter.muSampleCount, lrFilter.muSampleCount);
}

// ARTIST 0x82B85688.
extern "C" void SFILTER_unpackmtlfinit(FilterState* apState, VoiceStartParams* apParams)
{
    LoopMicroTalkFilter& lrFilter = *reinterpret_cast<LoopMicroTalkFilter*>(apState);
    lrFilter.mHeader.mpProcess = ProcessLoopMicroTalk;
    lrFilter.mHeader.mpRestore = RestoreMicroTalk<LoopMicroTalkFilter>;
    lrFilter.muCursor = 0;
    lrFilter.mpLoopSource = apParams->mpLoopSource;
    lrFilter.muLoopStart = apParams->muLoopStart;
    lrFilter.muLoopEnd = apParams->muLoopEnd;
    lrFilter.mpDecoder = NewMicroTalkDecoder();
    lrFilter.mpDecoder->miInitFlag = 1;
    lrFilter.mpDecoder->Feed(apParams->mpSource, 4 * lrFilter.muLoopStart, lrFilter.muLoopStart);
    apParams->mpPosition = ReadCursor<LoopMicroTalkFilter>;
}

// ARTIST 0x82B81200.
extern "C" void SFILTER_unpackxafinit(FilterState* apState, VoiceStartParams* apParams)
{
    XaFilter& lrFilter = *reinterpret_cast<XaFilter*>(apState);
    lrFilter.mHeader.mpProcess = ProcessXa;
    lrFilter.mHeader.mpRestore = RestoreXa<XaFilter>;
    lrFilter.mpSource = apParams->mpSource;
    lrFilter.muCursor = 0;
    lrFilter.miSampleCount = apParams->muSampleCount;
    lrFilter.mpDecoder = NewXaDecoder();
    apParams->mpPosition = ReadCursor<XaFilter>;  // ARTIST 0x82AD5F20
    FeedXa(lrFilter.mpDecoder, lrFilter.mpSource, lrFilter.miSampleCount);
}

// ARTIST 0x82B81388.
extern "C" void SFILTER_unpackxalfinit(FilterState* apState, VoiceStartParams* apParams)
{
    LoopXaFilter& lrFilter = *reinterpret_cast<LoopXaFilter*>(apState);
    lrFilter.mHeader.mpProcess = ProcessLoopXa;
    lrFilter.mHeader.mpRestore = RestoreXa<LoopXaFilter>;
    lrFilter.muCursor = 0;
    lrFilter.mpLoopSource = apParams->mpLoopSource;
    lrFilter.miDecodeMode = apParams->miDecodeMode;
    lrFilter.miLoopStart = apParams->muLoopStart;
    lrFilter.miLoopEnd = apParams->muLoopEnd;
    lrFilter.mpDecoder = NewXaDecoder();
    apParams->mpPosition = ReadCursor<LoopXaFilter>;
    if (lrFilter.miDecodeMode > 1)
    {
        FeedXa(lrFilter.mpDecoder, apParams->mpSource, lrFilter.miLoopStart);
    }
    else
    {
        lrFilter.mpFallbackSource = apParams->mpSource;
        lrFilter.mauFallbackCursor[0] = 0;
        lrFilter.mauFallbackCursor[1] = 0;
        lrFilter.muFallbackSampleCount = apParams->muSampleCount;
        lrFilter.muFallbackLoopStart = apParams->muLoopStart;
        lrFilter.muFallbackLoopEnd = apParams->muLoopEnd;
    }
}

// ARTIST 0x82B7CB08.
extern "C" void SFILTER_unpackpfinit(FilterState* apState, VoiceStartParams* apParams)
{
    PacketPcmFilter& lrFilter = *reinterpret_cast<PacketPcmFilter*>(apState);
    lrFilter.mHeader.mpProcess = ProcessPacketPcm;
    lrFilter.miHandle = Snd9::Packet::FindVoiceHandle(apParams->miVoice);
    lrFilter.mucChannel = SNDDRV_getsamplechan(apParams->miVoice);
    lrFilter.mpSource = 0;
    lrFilter.miFrameCount = -1;
    lrFilter.miCursor = 0;
    lrFilter.miDecoded = 0;
}

// ARTIST 0x82B815B8.
extern "C" void SFILTER_unpackxapfinit(FilterState* apState, VoiceStartParams* apParams)
{
    PacketXaFilter& lrFilter = *reinterpret_cast<PacketXaFilter*>(apState);
    lrFilter.mHeader.mpProcess = ProcessPacketXa;
    lrFilter.mHeader.mpRestore = RestoreXa<PacketXaFilter>;
    lrFilter.miHandle = Snd9::Packet::FindVoiceHandle(apParams->miVoice);
    lrFilter.mucChannel = SNDDRV_getsamplechan(apParams->miVoice);
    lrFilter.mpSource = 0;
    lrFilter.miDecodeMode = apParams->miDecodeMode;
    lrFilter.miCursor = 0;
    lrFilter.miDecoded = 0;
    lrFilter.miFrameCount = apParams->muSampleCount;
    lrFilter.mpDecoder = NewXaDecoder();
}

// ARTIST 0x82B858C0.
extern "C" void SFILTER_unpackmtpfinit(FilterState* apState, VoiceStartParams* apParams)
{
    PacketMicroTalkFilter& lrFilter = *reinterpret_cast<PacketMicroTalkFilter*>(apState);
    lrFilter.mHeader.mpProcess = ProcessPacketMicroTalk;
    lrFilter.miHandle = Snd9::Packet::FindVoiceHandle(apParams->miVoice);
    lrFilter.mucChannel = SNDDRV_getsamplechan(apParams->miVoice);
    lrFilter.mpSource = 0;
    lrFilter.muCursor = 0;
    lrFilter.miDecodeMode = apParams->miDecodeMode;
    lrFilter.mHeader.mpRestore = RestoreMicroTalk<PacketMicroTalkFilter>;
    lrFilter.miFrameCount = apParams->muSampleCount;
    lrFilter.mpDecoder = NewMicroTalkDecoder();
    lrFilter.miDecoded = 0;
}
