#define CGS_ARTIST_BRNTRAFFICJOB_CPP_FILE "..\\..\\..\\GameSource\\Jobs/Traffic/BrnTrafficJob.cpp"
#define CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\jobs\\traffic\\TrafficCommon.h"
#include "GameSource/Jobs/Traffic/BrnTrafficJob.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "SDKs/EATech/eajobs/entry_point.h"                   // EA::Jobs::EntryPoint
#include "SDKs/EATech/eajobs/job_scheduler.h"                 // EA::Jobs::JobScheduler::AddJobs
#include "SDKs/EATech/eajobs/job_types.h"                     // EA::Jobs::JOB_ENVIRONMENT_LOCAL
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/Jobs/CgsJobAssertBuffer.h"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

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

    // The fixed descriptor slot SetData is handed (0x100 literal @0x82752D84). Host pointer
    // widening grows TrafficJobData, so pin that it still fits.
    const u32 KU_JOB_DESCRIPTOR_BYTES = 256;
    CGS_STATIC_ASSERT((sizeof(TrafficJobData) == KU_JOB_DESCRIPTOR_BYTES),
                  "TrafficJobData must own the entire submitted 256-byte slot");
}

// X360 unk_831B9C80.
TrafficJob gaTrafficJobs[KU_MAX_TRAFFIC_JOB_WORKERS];

CGS_STATIC_ASSERT((sizeof(TrafficJob) == 0x500), "ARTIST traffic worker stride");
CGS_STATIC_ASSERT((offsetof(TrafficJob, mUpdateVehiclesJob) == 0x80), "ARTIST update worker offset");
CGS_STATIC_ASSERT((offsetof(TrafficJob, mau8Workspace) == 0x300), "ARTIST worker workspace offset");

// ARTIST 0x829172E0.
void TrafficJobEntry(EA::Jobs::Param, EA::Jobs::Param lJobData,
                     EA::Jobs::Param, EA::Jobs::Param)
{
    const u64 luThreadId = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
    const u64 luWorkerIndex = (luThreadId - 0xF8000088ull) >> 2;
    if (luWorkerIndex >= KU_MAX_TRAFFIC_JOB_WORKERS)
    {
        CgsDev::StrStream lMessage(CgsCore::gacDebugPrintStringBuffer, sizeof(CgsCore::gacDebugPrintStringBuffer));
        const u64 luReportedThreadId = static_cast<u64>(
            reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));
        lMessage << "SPU Id out of range: " << luReportedThreadId << "\n";
        const char* lpcSource =
            "D:\\P4\\B5_MAIN\\Burnout\\MAIN\\Code\\GameSource\\Jobs\\Traffic\\Traffic.cpp";
        CgsDev::Assert::PrintStringed(lpcSource, 0, 57, lMessage.GetBuffer());
        CGS_JOB_ASSERT_BREAK();
        CgsDev::Assert::PrintStringed(lpcSource, 0, 57, lMessage.GetBuffer());
        CGS_JOB_ASSERT_BREAK();
    }
    gaTrafficJobs[static_cast<u32>(luWorkerIndex)].Execute(
        static_cast<JobParams*>(lJobData.mpValue));
}

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
    CGS_ASSERT_AT(mbRunningJob, "mbRunningJob", CGS_ARTIST_BRNTRAFFICJOB_CPP_FILE, 101); // DWARF :99
    mJob.WaitOn(0, 0, -1);
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
    CGS_ASSERT_AT(lpParams, "lpParams", CGS_ARTIST_BRNTRAFFICJOB_CPP_FILE, 69);
    CGS_ASSERT_AT(!mbRunningJob, "!mbRunningJob", CGS_ARTIST_BRNTRAFFICJOB_CPP_FILE, 70);

    // SetOutputs(&mNewPhysicalRequests) -- inlined. The X360 asserts the output list
    // pointer (the member address it is about to store) is non-null; &mNewPhysicalRequests
    // is a member address so this is a degenerate always-true guard preserved as-is.
    PhysicalRequestInfoList* lpOutNewPhysicalRequests = &mNewPhysicalRequests;
    CGS_ASSERT_AT(lpOutNewPhysicalRequests, "lpOutNewPhysicalRequests", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 285);
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

    CgsSystem::JobManager()->AddJobs(&mJob, 1);
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

// ARTIST 0x82917420, with the workspace passed at 0x82917498.
void TrafficJob::ExecuteUpdateVehicles(JobParams* lpParams)
{
    CGS_ASSERT(lpParams, "lpParams");

    mUpdateVehiclesJob.Execute(&lpParams->mUpdateVehicles, mau8Workspace);
}

}
