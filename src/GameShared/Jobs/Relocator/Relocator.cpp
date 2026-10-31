#include "GameShared/Jobs/Relocator/RelocatorJob.h"
#include "GameShared/Jobs/CgsJobAssertBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "SDKs/EATech/eajobs/job_types.h"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"


CGS_STATIC_ASSERT(sizeof(RelocatorJob) == 0xC80, "ARTIST worker array stride");

// ARTIST 82AD28C0: native worker handle selects one of six contexts.
void RelocatorEntry(EA::Jobs::Param, EA::Jobs::Param lParam1,
             EA::Jobs::Param, EA::Jobs::Param)
{
    const u64 luThreadId = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
    const u64 luJobThreadIndex = (luThreadId - 0xF8000088ull) >> 2;
    if (luJobThreadIndex >= KI_NUM_RELOCATOR_JOBS)
    {
        CgsDev::StrStream lMessage(byte_83015F20, sizeof(byte_83015F20));
        const u64 luReportedThreadId = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
        lMessage << "SPU Id out of range: " << luReportedThreadId << "\n";
        const char* lpcSource = "D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameShared\\Jobs\\Relocator\\Relocator.cpp";
        CgsDev::Assert::PrintStringed(lpcSource, 0, 55, lMessage.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
        CgsDev::Assert::PrintStringed(lpcSource, 0, 55, lMessage.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
    }
    gaRelocatorJobs[static_cast<u32>(luJobThreadIndex)].Execute(lParam1.mpValue, static_cast<u32>(luJobThreadIndex));
}
