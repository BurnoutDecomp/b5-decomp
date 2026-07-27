#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"  // LinearMalloc::SetAlignment (Prepare)

namespace CgsAttribSys
{
// ---- static storage --------------------------------------------------------
// These are the manager's file-scope statics from the DWARF home. Only the two
// the getter touches are load-bearing for this TU (sbHasLinearAllocator gates the
// access; sAttribSysAllocator is the returned instance == X360 &dword_83011B94);
// the others are defined here so the home links once Prepare()/the other accessors
// are bodied in their own TUs.
CgsMemory::LinearMalloc*  AttribSysMemoryManager::spLinearAllocator   = NULL;
bool                      AttribSysMemoryManager::sbHasLinearAllocator = false;
AttribSysPackageAllocator AttribSysMemoryManager::sAttribSysAllocator;
AttribSysPackageAllocator AttribSysMemoryManager::sGameTalkAllocator;
AttribSysPackageAllocator AttribSysMemoryManager::sEaStlAllocator;

// @ 0x821F02A0 -- hand the AttribSys package allocator to the EA AttribSys vendor
// library. Asserts the manager has been Prepare'd (sbHasLinearAllocator) then
// returns &sAttribSysAllocator. (X360 returns &dword_83011B94, the static
// sAttribSysAllocator instance; the assert string/file/line match the original
// CgsAttribSysMemoryManager.h:211 "sbHasLinearAllocator".)
AttribSysPackageAllocator* AttribSysMemoryManager::GetAttribSysAllocator()
{
    CGS_ASSERT(sbHasLinearAllocator, "sbHasLinearAllocator");

    return &sAttribSysAllocator;
}

// Reset the manager's static state (called from AttribSysModule::Construct @0x8280AD50;
// the X360 inlines the two stores -- no standalone symbol).
void AttribSysMemoryManager::Construct()
{
    spLinearAllocator    = NULL;
    sbHasLinearAllocator = false;
}

// @ 0x828043B8 -- adopt the engine's linear allocator + the three package heaps: pin the
// linear allocator's alignment to 16, raise the prepared flag, then Construct + Prepare
// the three package allocators (AttribSys align 16, GameTalk align 4, EASTL align 4 --
// the KI_*_ALLOC_ALIGNMENT constants). The X360 inlines each PackageAllocator::Construct
// as the field-reset store runs before the Prepare calls.
bool AttribSysMemoryManager::Prepare(CgsMemory::LinearMalloc* lpLinearAllocator,
                                     CgsMemory::HeapMalloc* lpAttribSysHeap,
                                     CgsMemory::HeapMalloc* lpGameTalkHeap,
                                     CgsMemory::HeapMalloc* lpEaStlHeap)
{
    CGS_ASSERT(!sbHasLinearAllocator, "!sbHasLinearAllocator");                  // .cpp:139
    CGS_ASSERT(lpLinearAllocator != NULL, "lpLinearAllocator != NULL");          // .cpp:142
    CGS_ASSERT(lpAttribSysHeap != NULL, "lpAttribSysHeap != NULL");              // .cpp:143
    CGS_ASSERT(lpGameTalkHeap != NULL, "lpGameTalkHeap != NULL");                // .cpp:144
    CGS_ASSERT(lpEaStlHeap != NULL, "lpEaStlHeap != NULL");                      // .cpp:145

    spLinearAllocator = lpLinearAllocator;
    lpLinearAllocator->SetAlignment(KI_LINEAR_ALLOC_ALIGNMENT);
    sbHasLinearAllocator = true;

    sAttribSysAllocator.Construct(AttribSysPackageAllocator::E_PACKAGE_ATTRIBSYS);
    sGameTalkAllocator.Construct(AttribSysPackageAllocator::E_PACKAGE_GAMETALK);
    sEaStlAllocator.Construct(AttribSysPackageAllocator::E_PACKAGE_EASTL);

    sAttribSysAllocator.Prepare(lpAttribSysHeap, KI_ATTRIBSYS_ALLOC_ALIGNMENT);
    sGameTalkAllocator.Prepare(lpGameTalkHeap, KI_GAMETALK_ALLOC_ALIGNMENT);
    sEaStlAllocator.Prepare(lpEaStlHeap, KI_EASTL_ALLOC_ALIGNMENT);
    return true;
}

// The prepared-flag getter (the EASTL deallocate adapter asserts through it).
bool AttribSysMemoryManager::HasMemoryBuffer()
{
    return sbHasLinearAllocator;
}
}
