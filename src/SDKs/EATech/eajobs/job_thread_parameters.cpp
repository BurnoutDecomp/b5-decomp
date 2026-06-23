#include "SDKs/EATech/eajobs/job_thread_parameters.h"

#include "SDKs/EATech/eathread/BrnEAThreadX360.h" // EA::Thread::ThreadParameters

// ============================================================================
// SDKs/EATech/eajobs/job_thread_parameters.cpp
//
// EA::Jobs::JobThreadParameters::JobThreadParameters @ 0x828D6090, reconstructed
// store-for-store from the X360 .XEX.
//
// The X360 ctor builds a stack-local EA::Thread::ThreadParameters (its default
// ctor sets mnStackSize/mnPriority/mnProcessor) and copies those three fields
// (temp+4 / temp+8 / temp+0xC) into stackSize/priority/processor, then writes the
// job-thread defaults: affinity = JOB_AFFINITY_ANY (li r7,0x3F), ptrToName =
// "Job Manager - Job Thread", environment = JOB_ENVIRONMENT_LOCAL (the final
// `stw r6,0(r31)` with r6 == 0, store order last).
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Jobs
{
    // @ 0x828D6090
    JobThreadParameters::JobThreadParameters()
    {
        // Default thread parameters supply the stack-size / priority / processor
        // baselines (the ctor reads temp+4, temp+8, temp+0xC).
        EA::Thread::ThreadParameters lThreadParameters;

        stackSize = static_cast<int>(lThreadParameters.mnStackSize); // stw r10,4
        priority  = lThreadParameters.mnPriority;                    // stw r9,8
        processor = lThreadParameters.mnProcessor;                   // stw r8,0xC

        affinity  = JOB_AFFINITY_ANY;                                // li r7,0x3F; stw 0x10
        ptrToName = "Job Manager - Job Thread";                      // stw r11,0x14

        environment = JOB_ENVIRONMENT_LOCAL;                         // stw r6(=0),0
    }
}
}
