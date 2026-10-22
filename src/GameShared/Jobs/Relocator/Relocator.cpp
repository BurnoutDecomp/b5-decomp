// RelocatorEntry - the EAJobs entry point selected by the original copy job.
// ARTIST uses a six-context global array indexed by its console EAThread ids.
// The PC identity leaf below preserves one typed context per executing native
// thread; the canonical array/type declarations remain in their existing owner.

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

    // FLAG PC-platform leaf: ARTIST 82AD28D0..29E8 resolves a private
    // RelocatorJob context from the current EAThread id, using the console's
    // six-id numbering. Windows thread ids do not have that numbering. Each
    // native thread owns the same typed context through TLS, so concurrently
    // submitted jobs never share the former hard-coded context 0.
    static thread_local RelocatorJob sThreadJob;
    sThreadJob.Execute(laParam1.mpValue);
}
