#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReaderMinimalMemory.h"
#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"   // StackUnpick (ComputeAdjustedStack)
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT

#include <cstring>   // strlen / strcpy

namespace CgsDev
{
namespace MapFile
{
    // The X360 ctor leaves the reader synchronous (Prepare reads the whole map up front) so a freshly
    // prepared reader has every resolvable frame named immediately.
    MinimalMemoryReader::MinimalMemoryReader()
        : mbAsycronous(false)
        , pFileHandle(nullptr)
        , mbFinished(true)
    {
        mpCallstack = nullptr;
        maacStackNames[0][0] = '\0';
    }

    void MinimalMemoryReader::SetAsyncronousMode(bool lbAsynchronous)
    {
        mbAsycronous = lbAsynchronous;
    }

    // 0x828271C8 - latch the call-stack, clear the resolved-name cache, open the map, read + version-check
    // the header, rebase the stack into the map's address space, skip to the record array, then read the
    // records (the whole map up front unless asynchronous).
    void MinimalMemoryReader::Prepare(const char* lpcMapFileName, StackUnpickBase* lpCallstack)
    {
        Reader::Prepare(lpcMapFileName, lpCallstack);   // assert + mpCallstack = lpCallstack

        mbFinished = true;
        for (s32 liResult = 0; liResult < KI_MAX_STACK_RESULTS; ++liResult)
            maacStackNames[liResult][0] = '\0';

        pFileHandle = std::fopen(lpcMapFileName, "rb");
        if (!pFileHandle)
            return;

        // The 16-byte header: muRecordCount, record-array file offset, base address, version.
        const size_t luHeaderRead = std::fread(maReadBuffer, 1, 16, pFileHandle);
        const u32*      lpHeader        = reinterpret_cast<const u32*>(maReadBuffer);
        const u32       luRecordsOffset = lpHeader[1];
        const TargetPtr lBaseAddress    = lpHeader[2];
        const u32       luVersion       = lpHeader[3];

        CGS_ASSERT(luVersion == E_VERSION_CURRENT,
                   "map file is not the correct version. make sure you have the latest tools / code");

        // Rebase the captured stack into the map's address space (ComputeAdjustedStack is the platform
        // StackUnpick's; the assert passes a StackUnpick, as the X360 passes a StackUnpickX360).
        static_cast<StackUnpick*>(mpCallstack)->ComputeAdjustedStack(lBaseAddress);

        // Skip any gap between the header and the record array.
        if (luRecordsOffset != luHeaderRead && luRecordsOffset > luHeaderRead)
            std::fread(maReadBuffer, 1, luRecordsOffset - luHeaderRead, pFileHandle);

        mbFinished = false;
        ReadRecords();
        if (!mbAsycronous && !mbFinished)
        {
            do
            {
                ReadRecords();
            }
            while (!mbFinished);
        }
    }

    // 0x8281AEE8 - advance the incremental load by one buffer (no-op once the whole map has been read).
    void MinimalMemoryReader::Update()
    {
        if (!mbFinished)
            ReadRecords();
    }

    // 0x8281A5A8 - read one KI_BUFFER_SIZE chunk of fixed-size records; for each record, find the first
    // not-yet-resolved frame whose (adjusted) address lies in [mAddress, mAddress+muSize) and cache the
    // record's name for it. A short read marks the whole map read + closes the file.
    void MinimalMemoryReader::ReadRecords()
    {
        const size_t luBytesRead   = std::fread(maReadBuffer, 1, KI_BUFFER_SIZE, pFileHandle);
        const u32    luNumRecords  = static_cast<u32>(luBytesRead) / static_cast<u32>(sizeof(Record));

        if (luBytesRead != KI_BUFFER_SIZE)
        {
            std::fclose(pFileHandle);
            mbFinished  = true;
            pFileHandle = nullptr;
        }

        const u8* lpCursor = maReadBuffer;
        for (u32 luRecord = 0; luRecord < luNumRecords; ++luRecord)
        {
            const Record* lpRecord = reinterpret_cast<const Record*>(lpCursor);

            const s32 liNumFrames = (mpCallstack->GetNumStackAddresses() < KI_MAX_STACK_RESULTS)
                                  ? mpCallstack->GetNumStackAddresses()
                                  : KI_MAX_STACK_RESULTS;

            for (s32 liFrame = 0; liFrame < liNumFrames; ++liFrame)
            {
                if (maacStackNames[liFrame][0] != '\0')
                    continue;   // already resolved

                const TargetPtr lFrameAddress =
                    (liFrame < mpCallstack->GetNumStackAddresses())
                        ? static_cast<TargetPtr>(mpCallstack->GetStackAddress(liFrame))
                        : 0;

                if (lFrameAddress >= lpRecord->mAddress &&
                    lFrameAddress < lpRecord->mAddress + lpRecord->muSize)
                {
                    CGS_ASSERT(std::strlen(lpRecord->macName) < static_cast<size_t>(KI_NAME_LENGTH),
                               "map record name is too long");
                    std::strcpy(maacStackNames[liFrame], lpRecord->macName);
                    break;   // one frame resolved per record
                }
            }

            lpCursor += sizeof(Record);
        }
    }

    // 0x82820C70 - the resolved name of frame liIndex, or null if out of range / not yet resolved.
    const char* MinimalMemoryReader::GetStackEntryName(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        if (liIndex >= KI_MAX_STACK_RESULTS || maacStackNames[liIndex][0] == '\0')
            return nullptr;
        return maacStackNames[liIndex];
    }
}
}
