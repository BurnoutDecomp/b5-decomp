#include "GameShared/Jobs/PolygonSoupTester/PolygonSoupTesterJob.h"
#include "GameShared/Jobs/CgsJobAssertBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "SDKs/EATech/eajobs/job_types.h"
#include "SDKs/EATech/eajobs/job_thread_id.h"


CGS_STATIC_ASSERT(sizeof(PolygonSoupTesterJob) == 0x19040, "ARTIST worker array stride");

// ARTIST 829157B8: native worker handle selects one of six contexts.
void PolygonSoupTesterEntry(EA::Jobs::Param, EA::Jobs::Param lParam1,
             EA::Jobs::Param, EA::Jobs::Param)
{
    const u64 luThreadId = EA::Jobs::GetJobThreadId();
    const u64 luJobThreadIndex = (luThreadId - 0xF8000088ull) / 4;
    if (luJobThreadIndex >= KI_NUM_POLYGON_SOUP_TESTER_JOBS)
    {
        CgsDev::StrStream lMessage(CgsCore::gacDebugPrintStringBuffer, sizeof(CgsCore::gacDebugPrintStringBuffer));
        const u64 luReportedThreadId = EA::Jobs::GetJobThreadId();
        lMessage << "SPU Id out of range: " << luReportedThreadId << "\n";
        const char* lpcSource = "D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameShared\\Jobs\\PolygonSoupTester\\PolygonSoupTester.cpp";
        CGS_JOB_ASSERT_LOG(lpcSource, 55, lMessage.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
        CGS_JOB_ASSERT_LOG(lpcSource, 55, lMessage.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
    }
    gaPolygonSoupTesterJobs[static_cast<u32>(luJobThreadIndex)].Execute(lParam1.mpValue);
}
