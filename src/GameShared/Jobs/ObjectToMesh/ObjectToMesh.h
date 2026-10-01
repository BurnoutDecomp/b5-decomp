#pragma once

#include "types.hpp"
#include "SDKs/EATech/eajobs/job_types.h"

namespace CgsGraphics
{
class DispatchPacketInterpreter;
class DispatchList;
struct DispatchObjectContext;
struct DispatchCommand;
}

// ARTIST823F5748 constructs these fields; 823F5670 copies one aligned job record.
// Declaration names from GameShared/Jobs/ObjectToMesh/ObjectToMesh.h DWARF.
// Native pointers retain the same 128-byte job-data allocation on x64.
struct alignas(128) ObjectToMeshJobInfo
{
    u32 muListID;
    CgsGraphics::DispatchPacketInterpreter* mpDispatchInterpreter;
    CgsGraphics::DispatchObjectContext* mpDispatchObjectContext;
    CgsGraphics::DispatchList* mpDispatchListInput;
    CgsGraphics::DispatchList* mpaDispatchListOutputArray;
    u32 muDispatchListOutputCount;
    CgsGraphics::DispatchCommand* mpDispatchBinMasterAddress;
    uintptr_t muSharedMemoryStartAddress;
    u32* mpSharedMemoryBlockNextFreeAtomic;
    u32 muSharedMemoryBlockMax;
    s32 miStartIndex;
    s32 miEndIndex;
    bool mbSoftwareBreakpointOnEntry;
};
static_assert(sizeof(ObjectToMeshJobInfo) == 128, "object-to-mesh job data alignment");

void ObjectToMeshEntry(EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param);
