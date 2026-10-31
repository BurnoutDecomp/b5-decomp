#pragma once

#include "types.hpp"
#include "GameShared/Jobs/Relocator/CgsRelocator.h"
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandReader.h"
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultPoster.h"

const s32 KI_NUM_RELOCATOR_JOBS = 6;

class RelocatorJob
{
public:
    void Execute(void* lpvJobData, u32 luJobId);
    void ExecuteTestHijacks(CgsMemory::RelocationTestParams* lpParams, u32 luJobId);

    CgsMemory::RelocatorJobData* mpJobData;                 // +0x000
    // ARTIST leaves these existing bytes untyped; no field or alignment is claimed.
    u8 mUnrecoveredContext04[12];                          // +0x004..+0x00F
    CgsMemory::RelocationTestParams mTestParams;            // +0x010
    CgsMemory::DataStreamCommandReader mCommandReader;      // +0x080
    CgsMemory::DataStreamResultPoster mResultPoster;        // +0x100
    // Bounded storage representation of ARTIST +0x180..+0x480 and +0x480..+0xC80.
    // The original source array bounds are not independently established.
    CgsMemory::RelocationTestParams::Command mCommands[16]; // +0x180
    CgsMemory::RelocationTestParams::Result mResults[16];   // +0x480
};

CGS_STATIC_ASSERT(offsetof(RelocatorJob, mpJobData) == 0x000, "ARTIST relocator data pointer");
CGS_STATIC_ASSERT(offsetof(RelocatorJob, mTestParams) == 0x010, "ARTIST relocator test params");
CGS_STATIC_ASSERT(offsetof(RelocatorJob, mCommandReader) == 0x080, "ARTIST relocator command reader");
CGS_STATIC_ASSERT(offsetof(RelocatorJob, mResultPoster) == 0x100, "ARTIST relocator result poster");
CGS_STATIC_ASSERT(offsetof(RelocatorJob, mCommands) == 0x180, "ARTIST relocator commands");
CGS_STATIC_ASSERT(offsetof(RelocatorJob, mResults) == 0x480, "ARTIST relocator results");
CGS_STATIC_ASSERT(sizeof(RelocatorJob) == 0xC80, "ARTIST relocator worker stride");

extern RelocatorJob gaRelocatorJobs[KI_NUM_RELOCATOR_JOBS];
