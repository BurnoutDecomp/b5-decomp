// Translation-unit embed check for CgsAttribSys::AttribSysPackageAllocator.
// Forces the owning header to compile standalone and exercises the public
// interface so the member offsets / signatures stay wired to their homes.
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"

namespace
{
void EmbedCheck(CgsMemory::HeapMalloc* lpHeap)
{
    CgsAttribSys::AttribSysPackageAllocator lAllocator;
    lAllocator.Prepare(lpHeap, CgsAttribSys::AttribSysPackageAllocator::KI_DEFAULT_ALIGNMENT);
    void* lpBlock = lAllocator.Malloc(64u, 0);
    lAllocator.Free(lpBlock, 64u);
}
}
