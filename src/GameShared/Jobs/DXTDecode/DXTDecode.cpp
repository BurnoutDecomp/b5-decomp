// The DXT decode job's entry point.

#include "GameShared/Jobs/DXTDecode/DXTDecode.h"
#include "GameShared/Jobs/DXTDecode/DXTDecodeJob.h"   // DXTDecodeJob, gaDXTDecodeJobs

#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT

// Spill the four params, resolve this job thread's context out of the six-entry array and run
// it over the job data block (the second param).
void DXTDecodeEntry(EA::Jobs::Param laParam0,
                    EA::Jobs::Param laParam1,
                    EA::Jobs::Param laParam2,
                    EA::Jobs::Param laParam3)
{
    (void)laParam0;
    (void)laParam2;
    (void)laParam3;

    // ---- [PC platform layer] the job-thread index ----
    // The console derives it from the hardware thread id. This build runs the job body on the
    // thread that dispatches it (see NetworkTextureDXTCompress::Update), so the index is the
    // first context. Only that one computation is replaced; the bounds check, the six-entry
    // context array and the Execute call are the console's. Same precedent as RelocatorEntry
    // and TrafficJobEntry.
    const s32 liJobThreadIndex = 0;

    CGS_ASSERT(liJobThreadIndex < KI_NUM_DXT_DECODE_JOBS, "SPU Id out of range: ");

    gaDXTDecodeJobs[liJobThreadIndex].Execute(static_cast<const DXTDecodeData*>(laParam1.mpValue));
}
