#include "GameShared/Jobs/Relocator/CgsRelocator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // Start/StopMonitor

#include "SDKs/EATech/eajobs/job_types.h"   // EA::Jobs::Param / JOB_ENVIRONMENT_LOCAL

// CgsMemory::Relocator. The copy body itself lives in the job TUs beside this one
// (RelocatorEntry -> RelocatorJob::Execute).

// The job entry point, declared at global scope the way the other job dispatchers in this
// tree declare theirs (the entry is homed in Relocator.cpp).
void RelocatorEntry(EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param);

namespace CgsMemory
{
    // The three CPU monitor handles this engine times itself with. The resource module's
    // perfmon registration block seeds them ("Relocator Exec" / "Relocator GPU" /
    // "Relocator Update"); until it does they hold the unregistered sentinel and
    // Start/StopMonitor ignore them. -1 is the console's initialiser, NOT zero - zero is a
    // valid monitor handle.
    static s32 giRelocatorExecMonitor   = -1;
    static s32 giRelocatorGpuMonitor    = -1;
    static s32 giRelocatorUpdateMonitor = -1;

    // Reset both running latches.
    Relocator* Relocator::Construct()
    {
        mbRunningExternally = false;
        mbRunning           = false;
        return this;
    }

    // Latch the caller's op list into the job data block and submit the copy job. An empty
    // list submits nothing (and leaves the engine idle), so a caller may call unconditionally.
    void Relocator::Execute(RelocationParams* lpParams)
    {
        CGS_ASSERT(!mbRunningExternally && !mbRunning, "Already running\n");

        CgsDev::PerfMonCpu::StartMonitor(giRelocatorExecMonitor);
        CgsDev::PerfMonCpu::StartMonitor(giRelocatorGpuMonitor);

        // The snapshot: the job reads this block, not the caller's params.
        mJobData.mpOps              = lpParams->mpOps;
        mJobData.miNumOps           = lpParams->miNumOps;
        mJobData.muTestHijacks      = 0;
        mJobData.mpBounceBuffer     = lpParams->mpBounceBuffer;
        mJobData.miBounceBufferSize = lpParams->miBounceBufferSize;

        const s32 liNumOps = lpParams->miNumOps;

        CgsDev::PerfMonCpu::StopMonitor(giRelocatorGpuMonitor);

        if (liNumOps > 0)
        {
            mJob.Clear();
            mJob.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                         reinterpret_cast<const void*>(&RelocatorEntry), 0);
            mJob.SetData(&mJobData, sizeof(mJobData));
            mJob.SetName("Relocator");

            mbRunning = true;

            // ---- FLAG PC-platform leaf: THE DISPATCH, AND ONLY THE DISPATCH ----
            // The console submits the job to the global job scheduler, which does not exist
            // on this build, so the job body runs here instead. Everything either side of
            // this call - the job wiring, the running latch, the reaping in Update - is the
            // console's. Same precedent as the collision-generator dispatchers.
            RelocatorEntry(EA::Jobs::Param(),
                           EA::Jobs::Param(static_cast<void*>(&mJobData)),
                           EA::Jobs::Param(),
                           EA::Jobs::Param());
        }

        CgsDev::PerfMonCpu::StopMonitor(giRelocatorExecMonitor);
    }

    // Poll the copy job and report whether the engine has gone idle.
    bool Relocator::Update(bool lbClearExternalLatch, bool lbBlock)
    {
        CgsDev::PerfMonCpu::StartMonitor(giRelocatorUpdateMonitor);

        if (lbClearExternalLatch)
        {
            mbRunningExternally = false;
        }

        if (mbRunning)
        {
            if (mJob.IsDone())
            {
                mbRunning = false;
            }
            else if (lbBlock)
            {
                mJob.WaitOn();
                mbRunning = false;
            }
            // Otherwise the job is still going and the latch stays set.
        }

        CgsDev::PerfMonCpu::StopMonitor(giRelocatorUpdateMonitor);

        return !mbRunningExternally && !mbRunning;
    }
}
