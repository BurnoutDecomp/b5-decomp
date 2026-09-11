// RelocatorEntry - the EA::Jobs entry point CgsMemory::Relocator wires its copy job to.
// Same shape as the sibling job entries in this tree: spill the four params, resolve this job
// thread's private context out of the six-entry array, and hand it the job data block (the
// second param).

#include "GameShared/Jobs/Relocator/RelocatorJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include "SDKs/EATech/eajobs/job_types.h"            // EA::Jobs::Param

void RelocatorEntry(EA::Jobs::Param laParam0,
                    EA::Jobs::Param laParam1,
                    EA::Jobs::Param laParam2,
                    EA::Jobs::Param laParam3)
{
    (void)laParam0;
    (void)laParam2;
    (void)laParam3;

    // ---- FLAG PC-platform leaf: the job-thread index ----
    // The console derives it from the hardware thread id; there is no such id on this host
    // and this build runs the job body on one thread, so the index is the first context.
    // Only that one computation is replaced - the bounds check, the six-entry context array
    // and the Execute call are the console's. Same precedent as the sibling job entries.
    const s32 liJobThreadIndex = 0;

    CGS_ASSERT(liJobThreadIndex < KI_NUM_RELOCATOR_JOBS, "SPU Id out of range: ");

    gaRelocatorJobs[liJobThreadIndex].Execute(laParam1.mpValue);
}
