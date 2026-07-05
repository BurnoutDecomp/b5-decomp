#pragma once

// GameSource/Jobs/Traffic/BrnTrafficJob.h
//
// BrnTraffic::TrafficJobStub -- the per-frame traffic vehicle-update job wrapper. It owns
// an EA::Jobs::Job by value, a snapshot of the job parameters (TrafficJobData), and the
// output physical-request list the worker fills; TrafficEntityModule::UpdateVehicles fills a
// JobParams block and calls Execute, which wires + submits the job to the process-wide
// scheduler. Layout / member NAMES from the DecFIGS DWARF (BrnTrafficJob.h:50), byte offsets
// pinned by Execute @0x82752CB0:
//   +0x000 mbRunningJob         (bool)                       -- h:76
//   +0x010 mJob                 (EA::Jobs::Job, 848B)        -- h:77
//   +0x380 mJobData             (TrafficJobData, 160B live in a 256B reserved slot) -- h:78
//   +0x480 mNewPhysicalRequests (PhysicalRequestInfoList)    -- h:81
// (X360 32-bit member offsets are NOT preserved on the 64-bit PC compile; the embedded
//  Job/list widen -- shape-faithful, matching the committed Job header policy.)

#include "types.hpp"
#include "SDKs/EATech/eajobs/job.h"                    // EA::Jobs::Job (embedded by value, mEntryPoint public)
#include "SDKs/EATech/eajobs/job_scheduler.h"          // EA::Jobs::JobScheduler (gJobManager)
#include "SDKs/EATech/eajobs/job_types.h"              // EA::Jobs::Param
#include "GameSource/Jobs/Traffic/TrafficCommon.h"     // BrnTraffic::JobParams / UpdateVehiclesJobParams
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMiscRuntimeClasses.h" // BrnTraffic::PhysicalRequestInfoList

namespace BrnTraffic {

// DWARF Traffic.h:28. The snapshot of the job parameter block the worker reads. On the X360
// it lives in a fixed 256-byte reserved slot (SetData is handed 0x100); the live UpdateVehicles
// arm is 160 bytes (== sizeof(UpdateVehiclesJobParams)).
struct TrafficJobData
{
    JobParams mParams;    // Traffic.h:31
};

// The traffic vehicle-update worker entry (the local job function whose address is handed to
// EntryPoint::SetCode). Its body lives in the traffic-job worker TU; declared BY NAME here to
// match the X360 `bl TrafficJobEntry`.
void TrafficJobEntry(EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param);

// X360 unk_830EA650 -- the process-wide job scheduler singleton (CgsSystem::HardwareInit brings
// it up). No committed home yet; declared extern here (link-time resolves to its owning TU).
extern EA::Jobs::JobScheduler gJobManager;

// DWARF BrnTrafficJob.h:50.
struct TrafficJobStub
{
public:
    void Construct();                                       // h:55
    void Destruct();                                        // h:59
    // @0x82752CB0 -- wire the embedded job at TrafficJobEntry over a snapshot of lpParams and
    // submit it to the scheduler.
    void Execute(JobParams* lpParams);                     // h:63
    void WaitOn();                                          // h:67
    PhysicalRequestInfoList* GetNewPhysicalRequests();     // h:71

private:
    bool                    mbRunningJob;          // +0x000  h:76
    // (padding to 16-align the embedded Job)
    EA::Jobs::Job           mJob;                  // +0x010  h:77
    TrafficJobData          mJobData;              // +0x380  h:78 (160B live / 256B slot)
    PhysicalRequestInfoList mNewPhysicalRequests;  // +0x480  h:81
};

} // namespace BrnTraffic
