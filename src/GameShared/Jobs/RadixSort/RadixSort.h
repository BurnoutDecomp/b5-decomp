#pragma once

#include "types.hpp"
#include "SDKs/EATech/eajobs/job_types.h"

// The native sort job and dispatch list share these packed records.
struct SortKey
{
    u64 mu64Whole;

    static const u64 KU_MASK_OFFSET = 0xFFFFFull;
    static const u64 KU_SHIFT_KEY = 20;
};

struct SortKeyBlock
{
    SortKey* mpKeyArray;
    u32 muKeyCount;
    u32 muKeyCapacity;
    SortKeyBlock* mpNextBlock;
};

struct SortJobInputOutput
{
    SortKeyBlock* mpKeyBlockListHeader;
    SortKey* mpSortedPointerArray;
    u32 muSortedPointerCount;
};

CGS_STATIC_ASSERT(sizeof(SortKey) == 8, "native sort record");
CGS_STATIC_ASSERT(sizeof(SortKeyBlock) == 16, "native sort chain header");
CGS_STATIC_ASSERT(sizeof(SortJobInputOutput) == 12, "native sort job input");

// ARTIST823F5EA0 writes the two addresses, the narrowed count and the prepared
// list record. Each descriptor occupies a128-byte job-data slot in the renderer.
struct CGS_ALIGN(128) SortInfo
{
    u64 mu64KeyInAddress;
    u64 mu64KeyOutAddress;
    u16 mu16Count;
    SortJobInputOutput mInputOutputInfo;
};
CGS_STATIC_ASSERT((sizeof(SortInfo) == 128), "native sort descriptor must fit its job-data slot");

void RadixSortEntry(EA::Jobs::Param, EA::Jobs::Param,
                    EA::Jobs::Param, EA::Jobs::Param);
