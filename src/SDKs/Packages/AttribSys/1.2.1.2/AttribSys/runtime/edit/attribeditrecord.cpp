#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/edit/attribeditrecord.h"

#include <cstddef> // offsetof

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"    // GetAttribSysAllocator (sbHasLinearAllocator assert)
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // AttribSysPackageAllocator::FreeSized

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::EditRecord::~EditRecord @ 0x8280DE58
namespace Attrib
{
    // @ 0x8280DE58 -- tear down one edit record.
    //
    // The X360 body:
    //   1. if mpUndoState (+0x20) != 0 AND mnKind (+0x00, u16) == 7: RevertReplace().
    //   2. if mpBuffer (+0x1C) != 0: release the owned buffer back to the AttribSys
    //      package allocator. The asm asserts sbHasLinearAllocator
    //      (CgsAttribSysMemoryManager.h:211 -- the manager has been Prepare'd), reads
    //      mnBufferSize (+0x18) and mpBuffer (+0x1C), asserts the package allocator is
    //      live (mbHasAllocator, CgsAttribSysPackageAllocator.h:324), calls
    //      HeapMalloc::Free(sAttribSysAllocator.mpHeapAllocator, mpBuffer), and adds
    //      mnBufferSize to sAttribSysAllocator.miFreeTotal (dword_83011BA4).
    //
    // The asm reaches the static sAttribSysAllocator by fixed address (dword_83011B94)
    // and inlines its size-accounting free. Modelled here through the committed named
    // members: GetAttribSysAllocator() performs the sbHasLinearAllocator assert and
    // hands back &sAttribSysAllocator (matching the sibling EditTable::operator new
    // convention), and FreeSized() performs the mbHasAllocator assert + heap free +
    // miFreeTotal accounting (the free-side counterpart of the Malloc(size) that grew
    // miAllocTotal). See AttribSysPackageAllocator::FreeSized.
    EditRecord::~EditRecord()
    {
        if (mpUndoState != NULL && mnKind == KU_EDITKIND_REPLACE)
        {
            RevertReplace();
        }

        if (mpBuffer != NULL)
        {
            CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
                CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();

            lpAllocator->FreeSized(mpBuffer, mnBufferSize);
        }
    }

    // Never emitted; pins the pointer-invariant X360 member offsets.
    void EditRecord::_AssertLayout()
    {
        // Pointer-invariant facts only: mnKind and mnBufferSize both precede the
        // first pointer, so they hold on the X360 (4-byte ptr) and the LLP64 gate
        // (8-byte ptr) alike. mpBuffer/mpUndoState offsets shift with pointer width
        // and alignment, so they are not asserted here.
        static_assert(offsetof(EditRecord, mnKind)       == 0x00, "mnKind @0x00");
        static_assert(offsetof(EditRecord, mnBufferSize) == 0x18, "mnBufferSize @0x18");
    }
}
