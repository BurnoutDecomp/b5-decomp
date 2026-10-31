#pragma once

#include "types.hpp"
#include "GameShared/Jobs/Relocator/CgsRelocator.h"

const s32 KI_NUM_RELOCATOR_JOBS = 6;

class RelocatorJob
{
public:
    // ARTIST 82AD2BF8: r5 is the job index supplied by RelocatorEntry.
    void Execute(void* lpvJobData, u32 luJobId);
    // ARTIST 82AD29F8: genuine alternate stream-test path; definition pending
    // recovery of the worker's test-state members.
    void ExecuteTestHijacks(CgsMemory::RelocationTestParams* lpParams, u32 luJobId);

    CgsMemory::RelocatorJobData* mpJobData;
    u8 mPad_0004[3196]; // Existing opaque worker state; its typed recovery remains pending.
};

extern RelocatorJob gaRelocatorJobs[KI_NUM_RELOCATOR_JOBS];
