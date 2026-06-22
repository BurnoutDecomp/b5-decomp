// Compile-gate embed check for BrnResource::GameDataIO::AllocatorList.
// Instantiates the list by value and references the accessors + the GeneralAllocatorReportCB walk
// callback (newly bodied @0x82662E90), so its declaration and definition are type-checked together
// against the EA::Allocator::GeneralAllocator::BlockInfo layout it reads.

#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"
#include "ppmalloc/EAGeneralAllocator.h"   // EA::Allocator::GeneralAllocator::BlockInfo

namespace
{
    void ExerciseAllocatorList()
    {
        BrnResource::GameDataIO::AllocatorList lList;
        lList.Construct();

        (void)lList.GetRawResource(0);
        (void)lList.GetRawResourceDescriptor(0);
        (void)lList.GetLinearAllocator(0);
        (void)lList.GetHeapAllocator(0);
        (void)lList.GetRWLinearResourceAllocator(0);
        (void)lList.GetRWGeneralResourceAllocator(0);
        (void)lList.GetAudioStreamAllocator();
    }
}

void* gpAllocatorListEmbedCheck = reinterpret_cast<void*>(&ExerciseAllocatorList);
