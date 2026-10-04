#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "SDKs/EATech/eajobs/job_types.h"

// DecFIGS RadixSort.h:50: this three-member record is already recovered in the
// dispatcher. Alias its native representation rather than fork the key lists.
using SortJobInputOutput = CgsGraphics::DispatchList::SortJobInfo;

// ARTIST823F5EA0 writes the two addresses, the narrowed count and the prepared
// list record. Each descriptor occupies a128-byte job-data slot in the renderer.
struct alignas(128) SortInfo
{
    u64 mu64KeyInAddress = 0;
    u64 mu64KeyOutAddress = 0;
    u16 mu16Count = 0;
    SortJobInputOutput mInputOutputInfo = {};
};
static_assert(sizeof(SortInfo) == 128, "native sort descriptor must fit its job-data slot");

void RadixSortEntry(EA::Jobs::Param, EA::Jobs::Param,
                    EA::Jobs::Param, EA::Jobs::Param);
