#pragma once
#include "types.hpp"

// DecFIGS StreamDistribute.h:42-58; ARTIST loader job-data base0x2B000,
// target-list pointer+8 and next member at+128. Original DMA payload storage.
struct StreamDistributeTargetEntry
{
    u32 muOffset;
    void* mpTarget;
    u32 muSize;
    u32 muPad;
};

struct __declspec(align(128)) StreamDistributeData
{
    void* mpBaseDataStream;
    u32 muTotalStreamSize;
    StreamDistributeTargetEntry* mpTargets;
    u32 muTargetCount;
    char macPad[112];
};
