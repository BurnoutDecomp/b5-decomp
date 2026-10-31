#include "GameShared/Jobs/Relocator/RelocatorJob.h"
#include <xtl.h>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/Jobs/CgsJobAssertBuffer.h"

RelocatorJob gaRelocatorJobs[KI_NUM_RELOCATOR_JOBS];

// ARTIST 82AD2BF8. The overlap arm advances only the source: each chunk
// overwrites the same destination, as the original 82AD2C9C..B0 does.
void RelocatorJob::Execute(void* lpvJobData, u32 luJobId)
{
    CgsMemory::RelocatorJobData* lpData = static_cast<CgsMemory::RelocatorJobData*>(lpvJobData);
    mpJobData = lpData;
    if (lpData->mpTestParams != 0)
    {
        ExecuteTestHijacks(lpData->mpTestParams, luJobId);
        return;
    }

    for (s32 liOp = 0; liOp < lpData->miNumOps; ++liOp)
    {
        const CgsMemory::RelocateOp& lrOp = lpData->mpOps[liOp];
        char* lpcSource = static_cast<char*>(lrOp.mpSource);
        const u32 luSource = static_cast<u32>(reinterpret_cast<uintptr_t>(lpcSource));
        const u32 luDest = static_cast<u32>(reinterpret_cast<uintptr_t>(lrOp.mpDest));
        const u32 luSize = lrOp.muSize;
        if (static_cast<s32>(luDest + luSize - 1u) <= static_cast<s32>(luSource) ||
            static_cast<s32>(luDest) >= static_cast<s32>(luSource + luSize - 1u))
        {
            XMemCpy(lrOp.mpDest, lpcSource, luSize);
        }
        else
        {
            s32 liRemaining = static_cast<s32>(luSize);
            while (liRemaining > 0)
            {
                s32 liChunk = lpData->miBounceBufferSize;
                if (liRemaining <= liChunk)
                    liChunk = liRemaining;
                XMemCpy(lpData->mpBounceBuffer, lpcSource, static_cast<u32>(liChunk));
                XMemCpy(lrOp.mpDest, lpData->mpBounceBuffer, static_cast<u32>(liChunk));
                liRemaining -= liChunk;
                lpcSource += liChunk;
            }
        }
    }
}

// ARTIST 82AD29F8: stream commands through the worker's real aligned buffers.
void RelocatorJob::ExecuteTestHijacks(CgsMemory::RelocationTestParams* lpParams, u32 luJobId)
{
    mTestParams = *lpParams;
    mCommandReader.Construct(mTestParams.mpCommandPoster);
    mResultPoster.Construct(mTestParams.mpResultReader);
    s32 liBatches = 0;
    s32 liCount;
    while (mCommandReader.ReadCommands(mCommands, static_cast<s32>(luJobId), &liCount, 0) == 0)
    {
        for (s32 i = 0; i < liCount; ++i)
        {
            CgsMemory::RelocationTestParams::Command& command = mCommands[i];
            CgsMemory::RelocationTestParams::Result& result = mResults[i];
            result.muGetCommandUS = 0;
            result.muExecuteCommandUS = 0;
            result.muAddResultUS = 0;
            result.miNumber = command.miNumber;
            for (s32 j = 0; j < mTestParams.miCounter; ++j)
            {
                const char* lpSource = command.macString;
                char* lpDest = result.macInputSting;
                char lcValue;
                do { lcValue = *lpSource++; *lpDest++ = lcValue; } while (lcValue != 0);
                result.miJobId = static_cast<s32>(luJobId);
            }
        }
        mResultPoster.AddResults(mResults, liCount);
        if (++liBatches >= 2000)
        {
            CgsDev::StrStream lMessage(byte_83015F20, 8192);
            lMessage << "Added crazy number of results\n";
            const char* lpcFile = "D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameShared\\Jobs\\Relocator\\RelocatorJob.cpp";
            CgsDev::Assert::PrintStringed(lpcFile, 0, 252, lMessage.GetBuffer());
            __asm { mfmsr r12 }
            __asm { ori r12, r12, 0x400 }
            __asm { mtmsr r12 }
            CgsDev::Assert::PrintStringed(lpcFile, 0, 252, lMessage.GetBuffer());
            __asm { mfmsr r12 }
            __asm { ori r12, r12, 0x400 }
            __asm { mtmsr r12 }
        }
    }
    mCommandReader.Destruct();
    mResultPoster.Destruct();
}
