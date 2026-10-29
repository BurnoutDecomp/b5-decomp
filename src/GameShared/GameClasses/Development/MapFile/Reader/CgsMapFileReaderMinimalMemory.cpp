#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReaderMinimalMemory.h"
#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <cstring>

namespace CgsDev
{
namespace MapFile
{
    // ARTIST827E6028/2C inlines exactly the derived vptr and async=true stores.
    // The remaining fields retain their incoming storage until Prepare.
    MinimalMemoryReader::MinimalMemoryReader()
        : mbAsycronous(true)
    {
    }

    void MinimalMemoryReader::SetAsyncronousMode(bool lbAsynchronous)
    {
        mbAsycronous = lbAsynchronous;
    }

    // ARTIST828271C8: header reads and gap subtraction retain native uint32
    // arithmetic. A failed open leaves the ten cache heads empty and finished.
    void MinimalMemoryReader::Prepare(const char* lpcMapFileName, StackUnpickBase* lpCallstack)
    {
        Reader::Prepare(lpcMapFileName, lpCallstack);
        mbFinished = true;
        for (s32 liResult = 0; liResult < KI_MAX_STACK_RESULTS; ++liResult)
            maacStackNames[liResult][0] = '\0';

        pFileHandle = std::fopen(lpcMapFileName, "rb");
        if (!pFileHandle)
            return;

        const u32 luHeaderRead = static_cast<u32>(std::fread(maReadBuffer, 1, 16, pFileHandle));
        const u32* const lpHeader = reinterpret_cast<const u32*>(maReadBuffer);
        if (lpHeader[3] != E_VERSION_CURRENT)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "map file is not the correct version. make sure you have the latest tools / code",
                "..\\..\\..\\GameShared\\GameClasses\\Development/MapFile/CgsMapFile.h", 144);
            CgsDev::Assert::EndAssert();
        }
        static_cast<StackUnpick*>(mpCallstack)->ComputeAdjustedStack(lpHeader[2]);
        const u32 luRecordsOffset = lpHeader[1];
        if (luRecordsOffset != luHeaderRead)
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

    // ARTIST8281AEE8: a finished reader performs no further file operation.
    void MinimalMemoryReader::Update()
    {
        if (!mbFinished)
            ReadRecords();
    }

    // ARTIST8281A5A8. Close sees the finished flag already raised; its callback
    // may change reader state before pFileHandle is cleared. Each record resolves
    // only the first matching unresolved frame, among the original ten slots.
    void MinimalMemoryReader::ReadRecords()
    {
        const u32 luBytesRead = static_cast<u32>(std::fread(maReadBuffer, 1, KI_BUFFER_SIZE, pFileHandle));
        const u32 luNumRecords = luBytesRead / static_cast<u32>(sizeof(Record));
        if (luBytesRead != KI_BUFFER_SIZE)
        {
            mbFinished = true;
            std::fclose(pFileHandle);
            pFileHandle = 0;
        }

        for (u32 luRecord = 0; luRecord < luNumRecords; ++luRecord)
        {
            const Record* const lpRecord = reinterpret_cast<const Record*>(maReadBuffer + luRecord * sizeof(Record));
            const s32 liStackCount = mpCallstack->GetNumStackAddresses();
            const u32 luNumFrames = static_cast<u32>(liStackCount <= KI_MAX_STACK_RESULTS ? liStackCount : KI_MAX_STACK_RESULTS);
            for (u32 luFrame = 0; luFrame < luNumFrames; ++luFrame)
            {
                if (maacStackNames[luFrame][0])
                    continue;
                const TargetPtr lFrameAddress = static_cast<TargetPtr>(mpCallstack->GetStackAddress(static_cast<s32>(luFrame)));
                if (lFrameAddress >= lpRecord->mAddress && lFrameAddress < lpRecord->mAddress + lpRecord->muSize)
                {
                    const u32 luLength = static_cast<u32>(std::strlen(lpRecord->macName));
                    if (luLength >= static_cast<u32>(KI_NAME_LENGTH))
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
                        lStream << "String " << lpRecord->macName << " is too long. Buffer size = "
                                << static_cast<u32>(KI_NAME_LENGTH) << ", string length = " << luLength << "\n";
                        CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                            "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 65);
                        CgsDev::Assert::EndAssert();
                    }
                    // The original continues its complete NUL-terminated copy
                    // after the length assertion; it does not truncate the name.
                    const char* lpcSource = lpRecord->macName;
                    char* lpcDestination = maacStackNames[luFrame];
                    do
                    {
                        *lpcDestination++ = *lpcSource;
                    }
                    while (*lpcSource++);
                    break;
                }
            }
        }
    }

    // ARTIST82820C70. The lower-bound assertion continues to the native indexed
    // read; the upper bound and empty-cache cases return null.
    const char* MinimalMemoryReader::GetStackEntryName(s32 liIndex)
    {
        if (liIndex < 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("liIndex >= 0",
                "..\\..\\..\\GameShared\\GameClasses\\Development/MapFile/Reader/CgsMapFileReaderMinimalMemory.cpp", 135);
            CgsDev::Assert::EndAssert();
        }
        if (liIndex >= KI_MAX_STACK_RESULTS || maacStackNames[liIndex][0] == '\0')
            return 0;
        return maacStackNames[liIndex];
    }
}
}
