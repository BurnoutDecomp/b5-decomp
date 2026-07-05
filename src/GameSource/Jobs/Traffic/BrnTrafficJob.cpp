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
    mJob.SetData(&mJobData, 256);
    mJob.mEntryPoint.SetName("Traffic");

    mbRunningJob = true;
    gJobManager.AddJobs(&mJob, 1);
}

}
