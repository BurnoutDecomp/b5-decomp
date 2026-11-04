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
        CgsDev::StrStream message(byte_83015F20, sizeof(byte_83015F20));
        const u64 reported = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
        message << "SPU Id out of range: " << reported << "\n";
        const char* const file = "D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameShared\\Jobs\\ObjectToMesh\\ObjectToMesh.cpp";
        CgsDev::Assert::PrintStringed(file, 0, 59, message.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
        CgsDev::Assert::PrintStringed(file, 0, 59, message.GetBuffer());
        __asm { mfmsr r12 }
        __asm { ori r12, r12, 0x400 }
        __asm { mtmsr r12 }
    }
    gaObjectToMeshJobs[static_cast<u32>(index)].Execute(static_cast<ObjectToMeshJobInfo*>(param.mpValue));
}
