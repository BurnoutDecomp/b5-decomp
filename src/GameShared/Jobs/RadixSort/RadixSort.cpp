#include "GameShared/Jobs/RadixSort/RadixSortJob.h"

// ARTIST82AD2020 saves r4 as the descriptor, derives a console worker index,
// checks it against six and selects that worker's stateless RadixSortJob.
void RadixSortEntry(EA::Jobs::Param, EA::Jobs::Param lData,
                    EA::Jobs::Param, EA::Jobs::Param)
{
    // FLAG PC-platform leaf: Windows thread IDs are not console worker slots.
    // A local stateless instance preserves the worker's work without indexing
    // a six-element array with a native thread handle.
    RadixSortJob lJob;
    lJob.Execute(static_cast<SortInfo*>(lData.mpValue));
}
