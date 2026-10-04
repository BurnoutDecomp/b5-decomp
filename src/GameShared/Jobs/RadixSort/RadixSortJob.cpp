#include "GameShared/Jobs/RadixSort/RadixSortJob.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <algorithm>
#include <cstdint>

// ARTIST82AD2818..82AD28B8. The name is historical:82AD28B0 calls
// std::_Sort<u64*,int>, over the complete unsigned records, in place.
void RadixSortJob::Execute(SortInfo* lpData)
{
    CGS_ASSERT(lpData->mu64KeyInAddress == lpData->mu64KeyOutAddress,
               "key_in == key_out");
    u64* lpaKeys = reinterpret_cast<u64*>(static_cast<uintptr_t>(lpData->mu64KeyInAddress));
    // Avoid null-pointer arithmetic for an empty prepared list. Sorting zero or
    // one record has no side effects in the original std::_Sort implementation.
    if (lpData->mu16Count > 1)
        std::sort(lpaKeys, lpaKeys + lpData->mu16Count);
}
