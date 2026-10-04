#include "GameSource/Jobs/Traffic/BrnTrafficJob.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "SDKs/EATech/eajobs/entry_point.h"                   // EA::Jobs::EntryPoint
#include "SDKs/EATech/eajobs/job_scheduler.h"                 // EA::Jobs::JobScheduler::AddJobs
#include "SDKs/EATech/eajobs/job_types.h"                     // EA::Jobs::JOB_ENVIRONMENT_LOCAL
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "pc/gcm/renderengine/MeshJobOwnerWaitPCLeaf.h"


#include <cstring>   // std::memcpy (models the X360 memcpy intrinsic)
#include <cstdlib>

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
    // FLAG PC-platform leaf: enable only after native parity/timing validation.
    // The old traffic witness writes shared camera state; retain its ordered
    // owner execution when that diagnostic is requested.
    bool UseNativeTrafficJobsPC()
    {
        static const bool sbEnabled = [] {
            const char* lpcValue = std::getenv("BRN_TRAFFIC_JOBS");
            return lpcValue && lpcValue[0] == '1'
                && !std::getenv("BRN_TRAFFIC_DIAG")
                && !std::getenv("BRN_WORLD_CAMTRAFFIC");
        }();
        return sbEnabled;
    }

    // The fixed descriptor slot SetData is handed (0x100 literal @0x82752D84). Host pointer
    // widening grows TrafficJobData, so pin that it still fits.
    const u32 KU_JOB_DESCRIPTOR_BYTES = 256;
    static_assert(sizeof(TrafficJobData) == KU_JOB_DESCRIPTOR_BYTES,
                  "TrafficJobData must own the entire submitted 256-byte slot");
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

// ARTIST82752DC8..82752E34, recovered directly from the image: assert running,
// wait on the embedded job with sleep=-1, then clear the running flag.
void TrafficJobStub::WaitOn()
{
    CGS_ASSERT(mbRunningJob, "mbRunningJob");
    // FLAG PC-platform leaf: this join runs on the frame/window owner. Service
    // dependent native window messages and worker assertions until completion.
    renderengine::MeshJobOwnerWaitPC lOwnerWait;
    mJob.WaitOn(&renderengine::MeshJobOwnerWaitPC::Poll, &lOwnerWait);
    mbRunningJob = false;
}

// DWARF BrnTrafficJob.h:71.
PhysicalRequestInfoList* TrafficJobStub::GetNewPhysicalRequests()
{
    CGS_ASSERT(!mbRunningJob, "!mbRunningJob");
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

    if (UseNativeTrafficJobsPC())
    {
        CgsSystem::JobManager()->AddJobs(&mJob, 1);
    }

    else
    {
        TrafficJobEntry(EA::Jobs::Param(), EA::Jobs::Param(static_cast<void*>(&mJobData)),
                        EA::Jobs::Param(), EA::Jobs::Param());
    }
}

// X360 @0x829172E0. The console derives the worker id from EA::Thread::GetThreadId (an SPU
// slot on PS3), asserts it is in range ("SPU Id out of range: ", Traffic.cpp:57) and runs the
// matching TrafficJob over the params carried in the SECOND job argument.
void TrafficJobEntry(EA::Jobs::Param,
                     EA::Jobs::Param lData,
                     EA::Jobs::Param,
                     EA::Jobs::Param)
{
    // FLAG PC-platform leaf: native thread IDs are not the console's six worker
    // indices, and Param0 is not a worker ID. Each host thread keeps the original
    // worker state separately; Initialise resets it for every submitted slice.
    static thread_local TrafficJob slWorker;
    slWorker.Execute(static_cast<JobParams*>(lData.mpValue));
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
