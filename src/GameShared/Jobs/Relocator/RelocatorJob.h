#pragma once

#include "types.hpp"
#include "GameShared/Jobs/Relocator/CgsRelocator.h"   // CgsMemory::RelocatorJobData / RelocateOp

// RelocatorJob - the body of the copy job CgsMemory::Relocator submits. One instance per
// job thread lives in gaRelocatorJobs; the entry point picks its own and hands it the job
// data block. Execute walks the op list and performs each copy, bouncing through the
// params' staging buffer whenever a source and destination overlap.
//
// The alternate body the muTestHijacks flag selects (a data-stream command/result pump over
// the object's own 3200-byte scratch area) has no producer in this build - Relocator::Execute
// clears that flag on every pass - so only the copy body is reconstructed here. The scratch
// area is kept as explicit padding so the object keeps its shape.

// The job-thread context array bound is the console's.
const s32 KI_NUM_RELOCATOR_JOBS = 6;

class RelocatorJob
{
public:
    // Copy every op in the attached data block.
    void Execute(void* lpvJobData);

    CgsMemory::RelocatorJobData* mpJobData;      // +0x0000 the block the entry point handed us
    u8                           mPad_0004[3196]; // +0x0004 the hijack body's command/result scratch
};

extern RelocatorJob gaRelocatorJobs[KI_NUM_RELOCATOR_JOBS];
