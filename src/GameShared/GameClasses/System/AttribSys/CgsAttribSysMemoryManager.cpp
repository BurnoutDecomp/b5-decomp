#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

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
}
