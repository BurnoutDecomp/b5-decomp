#include "GameSource/Replays/Stream/BrnReplayReadStream.h"

#include "GameSource/Replays/BrnReplayShared.h"
#include "GameSource/Replays/Stream/BrnReplayDiskReadStream.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <cstring>

namespace BrnReplays
{
    char ReadStream::mpcIntermediateBuffer[ReadStream::KI_INTERMEDIATEBUFFERSIZE];

    // Inlined in UpdatePlaying_PreSim 82660A3C..70. Result time/flags and
    // header magic are retained; only these header fields and arrays are reset.
    FrunkReadResult::FrunkReadResult()
    {
        mHeader.miFrameNumber = 0;
        mHeader.mxFlags = 0;
        mHeader.mfFrameTime = 0.0f;
        mHeader.miNumSerialisers = 0;
        for (s32 liSerialiser = 0; liSerialiser < E_ID_COUNT; ++liSerialiser)
        {
            maiSizes[liSerialiser] = 0;
            mapBuffers[liSerialiser] = nullptr;
        }
    }

    // Inlined by ARTIST ReplayModule::Construct 82656E20: stores at module
    // +8E0/+8E4/+8F0. The other cursor/counter fields are not reset here.
    void ReadStream::Construct()
    {
        mpStreamHeader = nullptr;
        miCurrentFrunk = 0;
        mpStream = nullptr;
    }

    // ARTIST 8264CFF8. The original capacity branch leaves destination/cursor
    // untouched when the requested bytes do not fit; no extra read guard added.
    void ReadStream::Read(void* lpDest, s32 liSize)
    {
        // Original shared intermediate bytes: 0x82FBA380.
        if (KI_INTERMEDIATEBUFFERSIZE - miCurrentFrunkPos >= liSize)
        {
            std::memcpy(lpDest, mpcIntermediateBuffer + miCurrentFrunkPos, liSize);
            miCurrentFrunkPos += liSize;
        }
    }

    // ARTIST 8265C1F0. The header-buffer-size argument is not read. Preserve
    // the exact end-index expression, including the original first+count slot.
    s64 ReadStream::StartNewStream(void* lpHeaderBuffer, s32 /*liHeaderBufferSize*/,
                                   DiskReadStream* lpReadStream)
    {
        mpStreamHeader = static_cast<StreamHeader*>(lpHeaderBuffer);
        miFilePosition = 0;
        mpStream = lpReadStream;
        miCurrentFrunk = mpStreamHeader->miFirstFrunk;
        const StreamOffset& lrFirst = mpStreamHeader->mpFrameOffsets[miCurrentFrunk];
        const StreamOffset& lrEnd = mpStreamHeader->mpFrameOffsets[
            (mpStreamHeader->miFirstFrunk + mpStreamHeader->miNumFrunks) % KI_MAX_FRUNKS];
        mpStream->SetRange(lrFirst.miFileOffset, lrEnd.miFileOffset + lrEnd.miFrunkSize);
        return lrFirst.miFrameNumber;
    }

    // ARTIST 8265E7F0. The result's sizes enter as caller capacities and leave
    // as actual payload sizes. VOID/not-ready paths preserve their old contents.
    bool ReadStream::ReadCurrentFrunk(FrunkReadResult* lpInOutResult)
    {
        // 0x82FBA380 is the shared read buffer. The original assertion builder's
        // 0x82000D00/0x82000D08 vtables and 0x82F32264 capacity are represented
        // by the canonical CgsDev::StrStream and KI_MESSAGEBUFFERSIZE below.
        const StreamOffset& lrOffset = mpStreamHeader->mpFrameOffsets[miCurrentFrunk];
        lpInOutResult->mxFlags = lrOffset.mxFlags;
        lpInOutResult->mfTime = lrOffset.mfFrameTime;
        if ((lpInOutResult->mxFlags & KU_FLAG_VOID) != 0)
            return true;
        if (!mpStream->ReadBlock(lrOffset.miFileOffset, mpcIntermediateBuffer,
                                (lrOffset.miFrunkSize + 0xFFFF) & ~0xFFFF))
            return false;

        miCurrentFrunkPos = 0;
        Read(&lpInOutResult->mHeader, sizeof(FrunkHeader));
        CGS_ASSERT(lpInOutResult->mHeader.miNumSerialisers < E_ID_COUNT,
                   "Too many serialisers in header\n");
        FrunkSerialiserEntry laEntries[E_ID_COUNT];
        Read(laEntries, sizeof(FrunkSerialiserEntry) * lpInOutResult->mHeader.miNumSerialisers);
        for (s32 liEntry = 0; liEntry < lpInOutResult->mHeader.miNumSerialisers; ++liEntry)
        {
            const s32 liId = laEntries[liEntry].miId;
            const s32 liSize = laEntries[liEntry].miSize;
            if (!lpInOutResult->mapBuffers[liId])
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
                lMessage << "No buffer to store data for serialiser " << liId << " into\n";
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
            if (lpInOutResult->maiSizes[liId] < liSize)
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
                lMessage << "Buffer is not large enough for serialiser " << liId
                         << " - needs " << liSize << ", is " << lpInOutResult->maiSizes[liId] << "\n";
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }
            lpInOutResult->maiSizes[liId] = liSize;
            Read(lpInOutResult->mapBuffers[liId], liSize);
        }
        return true;
    }

    // ARTIST 8264D060: advance within the live ring, wrap to its first frunk,
    // and clear the stall count only when that first frunk is reached.
    bool ReadStream::MoveToNextFrunk()
    {
        miCurrentFrunk = (miCurrentFrunk + 1) % KI_MAX_FRUNKS;
        s32 liUnwrapped = miCurrentFrunk;
        if (liUnwrapped < mpStreamHeader->miFirstFrunk)
            liUnwrapped += KI_MAX_FRUNKS;
        if (liUnwrapped >= mpStreamHeader->miFirstFrunk + mpStreamHeader->miNumFrunks)
            miCurrentFrunk = mpStreamHeader->miFirstFrunk;
        if (miCurrentFrunk == mpStreamHeader->miFirstFrunk)
        {
            miStallCount = 0;
            return true;
        }
        return false;
    }
}
