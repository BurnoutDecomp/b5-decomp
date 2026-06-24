#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/edit/attribedittable.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"   // GetAttribSysAllocator
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // AttribSysPackageAllocator::Malloc

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::EditTable::operator new @ 0x82805A80
//
// The X360 body asserts sbHasLinearAllocator (the AttribSys memory manager has been
// Prepare'd; CgsAttribSysMemoryManager.h:211) and then calls
// AttribSysPackageAllocator::Malloc(&dword_83011B94, lnBytes, 0) -- i.e. it Mallocs the
// requested bytes from the static AttribSys package allocator (&dword_83011B94 ==
// AttribSysMemoryManager::sAttribSysAllocator) with flags 0. GetAttribSysAllocator()
// runs that exact sbHasLinearAllocator assert and returns &sAttribSysAllocator, so the
// allocator lookup is routed through it (member-by-name; the asm reads the private
// static directly).
namespace Attrib
{
    void* EditTable::operator new(size_t lnBytes)
    {
        CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
            CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();
        return lpAllocator->Malloc(lnBytes, 0);
    }
}
