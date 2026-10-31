#include "GameShared/Jobs/RadixSort/RadixSortJob.h"
#include "GameShared/Jobs/CgsJobAssertBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "SDKs/EATech/eajobs/job_types.h"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

namespace { RadixSortJob gaRadixSortJobs[6]; }

CGS_STATIC_ASSERT(sizeof(RadixSortJob) == 0x1, "ARTIST worker array stride");

// ARTIST 82AD2020: native worker handle selects one of six contexts.
void RadixSortEntry(EA::Jobs::Param, EA::Jobs::Param lParam1,
             EA::Jobs::Param, EA::Jobs::Param)
{
    const u64 luThreadId = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
    const u64 luJobThreadIndex = (luThreadId - 0xF8000088ull) >> 2;
    if (luJobThreadIndex >= 6)
    {
        CgsDev::StrStream lMessage(byte_83015F20, sizeof(byte_83015F20));
        const u64 luReportedThreadId = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
        lMessage << "SPU Id out of range: " << luReportedThreadId << "\n";
        const char* lpcSource = "D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameShared\\Jobs\\RadixSort\\RadixSort.cpp";
        CgsDev::Assert::PrintStringed(lpcSource, 0, 59, lMessage.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
        CgsDev::Assert::PrintStringed(lpcSource, 0, 59, lMessage.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
    }
    gaRadixSortJobs[static_cast<u32>(luJobThreadIndex)].Execute(static_cast<SortInfo*>(lParam1.mpValue));
}
