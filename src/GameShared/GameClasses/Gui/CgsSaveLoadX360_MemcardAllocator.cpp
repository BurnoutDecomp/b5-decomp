#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
namespace CgsGui
{
MemcardAllocator::~MemcardAllocator() {}
// 827DBB08: flags/name are unused; HeapMalloc always gets alignment 4.
void* MemcardAllocator::Alloc(size_t nSize, const char*, unsigned int)
{
    void* pBlock = mpHeap->Malloc(static_cast<s32>(nSize), KI_DEFAULT_ALIGNMENT);
    CGS_ASSERT(pBlock != 0, "CgsMemory::HeapMalloc::Malloc failed.");
    return pBlock;
}
// 827DBBB0: mr r6,r7; tail-dispatch plain Alloc at +8. Alignment is forwarded
// into the plain overload's flags argument; no alignment work occurs here.
void* MemcardAllocator::Alloc(size_t nSize, const char* pName, unsigned int,
                             unsigned int nAlignment, unsigned int)
{
    return Alloc(nSize, pName, nAlignment);
}
void MemcardAllocator::Free(void* pBlock, size_t) // 827DBBC8
{
    mpHeap->Free(pBlock);
}
}
