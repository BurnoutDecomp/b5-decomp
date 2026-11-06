#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugCollections.h"

#include "rw/rwcore_structs.h"

// ARTIST 0x82819228: allocate lane zero through the supplied resource allocator,
// with 16-byte alignment and no allocation name. The allocation tag is unused.
void* operator new[](size_t size, rw::IResourceAllocator* lpAllocator, CgsDev::Internal::AllocationType /*leAllocation*/)
{
    return lpAllocator->AllocateMemoryResource(static_cast<u32>(size), 16u, 0);
}
