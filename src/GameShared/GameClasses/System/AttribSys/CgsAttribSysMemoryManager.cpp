#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"  // LinearMalloc::SetAlignment (Prepare)
#define CGS_ARTIST_CGSATTRIBSYSMEMORYMANAGER_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\attribsys\\CgsAttribSysMemoryManager.cpp"
namespace CgsAttribSys
{
// ---- static storage --------------------------------------------------------
// These are the manager's file-scope statics from the DWARF home. Only the two
// the getter touches are load-bearing for this TU (sbHasLinearAllocator gates the
// access; sAttribSysAllocator is the returned instance == X360 &dword_83011B94);
// Construct clears the same pointer/flag that Prepare adopts; ARTIST stores the
// linear pointer at 0x83011BAC and the flag at 0x83011BB0.
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
    CGS_ASSERT_AT(sbHasLinearAllocator, "sbHasLinearAllocator",
        "..\\..\\..\\GameShared\\GameClasses\\System/AttribSys/CgsAttribSysMemoryManager.h",
        211 /* ARTIST 0x821F02C4 */);

    return &sAttribSysAllocator;
}

// The GameTalk package uses the same prepared-state guard.
AttribSysPackageAllocator* AttribSysMemoryManager::GetGameTalkAllocator()
{
    CGS_ASSERT(sbHasLinearAllocator, "sbHasLinearAllocator");

    return &sGameTalkAllocator;
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
    CGS_ASSERT_AT(!sbHasLinearAllocator, "!sbHasLinearAllocator", CGS_ARTIST_CGSATTRIBSYSMEMORYMANAGER_FILE, 139);                  // .cpp:139
    CGS_ASSERT_AT(lpLinearAllocator != NULL, "lpLinearAllocator != NULL", CGS_ARTIST_CGSATTRIBSYSMEMORYMANAGER_FILE, 142);          // .cpp:142
    CGS_ASSERT_AT(lpAttribSysHeap != NULL, "lpAttribSysHeap != NULL", CGS_ARTIST_CGSATTRIBSYSMEMORYMANAGER_FILE, 143);              // .cpp:143
    CGS_ASSERT_AT(lpGameTalkHeap != NULL, "lpGameTalkHeap != NULL", CGS_ARTIST_CGSATTRIBSYSMEMORYMANAGER_FILE, 144);                // .cpp:144
    CGS_ASSERT_AT(lpEaStlHeap != NULL, "lpEaStlHeap != NULL", CGS_ARTIST_CGSATTRIBSYSMEMORYMANAGER_FILE, 145);                      // .cpp:145

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
