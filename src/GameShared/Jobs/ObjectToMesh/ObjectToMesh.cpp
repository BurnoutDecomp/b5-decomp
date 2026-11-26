#include "GameShared/Jobs/ObjectToMesh/ObjectToMeshJob.h"
#include "GameShared/Jobs/CgsJobAssertBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

namespace { ObjectToMeshJob gaObjectToMeshJobs[6]; }
CGS_STATIC_ASSERT(sizeof(ObjectToMeshJob) == 4, "ARTIST object-to-mesh worker stride");

// ARTIST 82926940: the scheduler's second Param is the job data in r4.
void ObjectToMeshEntry(EA::Jobs::Param, EA::Jobs::Param param,
                       EA::Jobs::Param, EA::Jobs::Param)
{
    const u64 id = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
    const u64 index = (id - 0xF8000088ull) >> 2;
    if (index >= 6)
    {
        CgsDev::StrStream message(CgsCore::gacDebugPrintStringBuffer, sizeof(CgsCore::gacDebugPrintStringBuffer));
        const u64 reported = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
        message << "SPU Id out of range: " << reported << "\n";
        const char* const file = "D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameShared\\Jobs\\ObjectToMesh\\ObjectToMesh.cpp";
        CGS_JOB_ASSERT_LOG(file, 59, message.GetBuffer());
        CGS_JOB_ASSERT_BREAK();
        CGS_JOB_ASSERT_LOG(file, 59, message.GetBuffer());
        CGS_JOB_ASSERT_BREAK();
    }
    gaObjectToMeshJobs[static_cast<u32>(index)].Execute(static_cast<ObjectToMeshJobInfo*>(param.mpValue));
}
