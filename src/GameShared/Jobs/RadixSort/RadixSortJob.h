#pragma once

#include "GameShared/Jobs/RadixSort/RadixSort.h"

// ARTIST82AD2818 never reads or writes this: the per-worker object is stateless.
class RadixSortJob
{
public:
    void Execute(SortInfo* lpData);
};
