#include "GameSource/Jobs/Traffic/BrnTrafficJob.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "SDKs/EATech/eajobs/entry_point.h"                   // EA::Jobs::EntryPoint
#include "SDKs/EATech/eajobs/job_scheduler.h"                 // EA::Jobs::JobScheduler::AddJobs
#include "SDKs/EATech/eajobs/job_types.h"                     // EA::Jobs::JOB_ENVIRONMENT_LOCAL


#include <cstring>   // std::memcpy (models the X360 memcpy intrinsic)

// GameSource/Jobs/Traffic/BrnTrafficJob.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Execute @ 0x82752CB0) + the DecFIGS DWARF.
// TrafficEntityModule::UpdateVehicles fills a BrnTraffic::JobParams (its UpdateVehicles
// arm) on the stack and passes it to Execute; Execute wires the output list, snapshots
// the params into mJobData, points the embedded EA::Jobs job at TrafficJobEntry, names
// it "Traffic", flags the stub busy and submits to the process-wide scheduler.
//
// SetOutputs is inlined here (the X360 takes &mNewPhysicalRequests, asserts that address
// non-null -- a degenerate always-true guard preserved as-is -- and stores it at the
// params' +0x90). The asserts only report (control falls through regardless), mirroring
// the committed CgsResource::DecompressionJobInterface precedent, which likewise embeds an
// EA::Jobs::Job by value and drives it by name.

namespace BrnTraffic
{
namespace
{
    // FLAG PC-platform leaf: single-threaded job dispatch. The console submits mJob to
    // EA::Jobs::JobScheduler gJobManager (X360 unk_830EA650), which CgsSystem::HardwareInit
    // brings up; that singleton has no committed home on this host, so TrafficJobEntry runs
    // inline here and WaitOn is a no-op. One worker slot is live, so the worker id is 0.
    // DELETE-WHEN gJobManager is homed and Initialize()d during boot.
    const bool KB_PC_SYNCHRONOUS_JOB_DISPATCH = true;
    const u32  KU_PC_SYNCHRONOUS_WORKER_ID    = 0;

    // The fixed descriptor slot SetData is handed (0x100 literal @0x82752D2C). Host pointer
    // widening grows TrafficJobData, so pin that it still fits.
    const u32 KU_JOB_DESCRIPTOR_BYTES = 256;
    static_assert(sizeof(TrafficJobData) <= KU_JOB_DESCRIPTOR_BYTES,
                  "TrafficJobData outgrew the console's 256-byte job descriptor slot");
}

// X360 unk_831B9C80.
TrafficJob gaTrafficJobs[KU_MAX_TRAFFIC_JOB_WORKERS];

// BrnTrafficJob.cpp:39 / DWARF BrnTrafficJob.h:55.
void TrafficJobStub::Construct()
{
    mbRunningJob = false;
    mNewPhysicalRequests.Construct();
}

// BrnTrafficJob.cpp:54 / DWARF BrnTrafficJob.h:59. Empty in the DWARF listing.
void TrafficJobStub::Destruct()
{
}

// BrnTrafficJob.cpp:99 / DWARF BrnTrafficJob.h:67. EXPORT HOLE at X360 0x82752DC8 (no
// per-function JSON), so only the shape is attested: the console blocks on mJob then drops
// mbRunningJob. Under the synchronous dispatch above the work already ran inside Execute.
void TrafficJobStub::WaitOn()
{
    mbRunningJob = false;
}

// DWARF BrnTrafficJob.h:71.
PhysicalRequestInfoList* TrafficJobStub::GetNewPhysicalRequests()
{
    return &mNewPhysicalRequests;
}

// BrnTrafficJob.cpp:67 / X360 0x82752CB0
void TrafficJobStub::Execute(JobParams* lpParams)
{
    CGS_ASSERT(lpParams, "lpParams");
    CGS_ASSERT(!mbRunningJob, "!mbRunningJob");

    // SetOutputs(&mNewPhysicalRequests) -- inlined. The X360 asserts the output list
    // pointer (the member address it is about to store) is non-null; &mNewPhysicalRequests
    // is a member address so this is a degenerate always-true guard preserved as-is.
    PhysicalRequestInfoList* lpOutNewPhysicalRequests = &mNewPhysicalRequests;
    CGS_ASSERT(lpOutNewPhysicalRequests, "lpOutNewPhysicalRequests");
    lpParams->mUpdateVehicles.mpOutNewPhysicalRequests = lpOutNewPhysicalRequests;

    // Snapshot the caller's params into our own descriptor (160 bytes ==
    // sizeof(UpdateVehiclesJobParams), the union's largest arm; the X360 memcpy stride).
    std::memcpy(&mJobData, lpParams, sizeof(UpdateVehiclesJobParams));

    // Wire and submit the job. The X360 drives the embedded job's EntryPoint directly
    // (EntryPoint::SetCode/SetName on mJob.mEntryPoint), and hands SetData the fixed
    // 256-byte descriptor slot the worker reads (0x100 literal, not sizeof(mJobData)).
    mJob.Clear();
    mJob.mEntryPoint.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                             reinterpret_cast<const void*>(&TrafficJobEntry), 0);
    mJob.SetData(&mJobData, KU_JOB_DESCRIPTOR_BYTES);
    mJob.mEntryPoint.SetName("Traffic");

    mbRunningJob = true;

    if (KB_PC_SYNCHRONOUS_JOB_DISPATCH)
    {
        // FLAG PC-platform leaf: run the worker inline instead of gJobManager.AddJobs(&mJob, 1).
        // Reason + DELETE-WHEN are on KB_PC_SYNCHRONOUS_JOB_DISPATCH above.
        TrafficJobEntry(EA::Jobs::Param(static_cast<u32>(KU_PC_SYNCHRONOUS_WORKER_ID)),
                        EA::Jobs::Param(static_cast<void*>(&mJobData)),
                        EA::Jobs::Param(),
                        EA::Jobs::Param());
        return;
    }

    gJobManager.AddJobs(&mJob, 1);
}

// X360 @0x829172E0. The console derives the worker id from EA::Thread::GetThreadId (an SPU
// slot on PS3), asserts it is in range ("SPU Id out of range: ", Traffic.cpp:57) and runs the
// matching TrafficJob over the params carried in the SECOND job argument.
void TrafficJobEntry(EA::Jobs::Param lWorkerId,
                     EA::Jobs::Param lData,
                     EA::Jobs::Param,
                     EA::Jobs::Param)
{
    // FLAG PC-platform leaf: the worker id is the job argument, not a thread-id derived SPU
    // slot. Reason + DELETE-WHEN on KB_PC_SYNCHRONOUS_JOB_DISPATCH.
    const u32 luWorkerId = lWorkerId.muValue;
    CGS_ASSERT(luWorkerId < KU_MAX_TRAFFIC_JOB_WORKERS, "SPU Id out of range: ");

    gaTrafficJobs[luWorkerId].Execute(static_cast<JobParams*>(lData.mpValue));
}

// X360 @0x829174B0 (TrafficJob.cpp:51 / :69).
void TrafficJob::Execute(JobParams* lpData)
{
    CGS_ASSERT(lpData, "lpData");

    mpData = lpData;

    if (lpData->meProcess != E_JOBPROCESS_UPDATE_VEHICLES)
    {
        CGS_ASSERT(false, "Invalid job process\n");
        return;
    }

    ExecuteUpdateVehicles(lpData);
}

// X360 @0x82917420 (TrafficJob.cpp:89). The console also passes this+0x300 in r5; neither
// UpdateVehiclesJob::Execute nor ::Initialise reads it, so it is not modelled.
void TrafficJob::ExecuteUpdateVehicles(JobParams* lpParams)
{
    CGS_ASSERT(lpParams, "lpParams");

    mUpdateVehiclesJob.Execute(&lpParams->mUpdateVehicles);
}

}
