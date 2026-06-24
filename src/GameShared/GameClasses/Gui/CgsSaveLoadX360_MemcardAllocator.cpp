#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h" // CgsMemory::HeapMalloc

// CgsGui::MemcardAllocator member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU (class:CgsGui::MemcardAllocator) bodies the three X360-emitted functions:
//
//   Alloc                       @ 0x827DBB08
//   Free                        @ 0x827DBBC8
//   `vector deleting destructor' @ 0x827DBC00
//
// X360 store-for-store notes:
//   - Alloc: `lwz r3, 4(this)` loads mpHeap, `lwz r3,4(r3)`->no; r3<-mpHeap, r4<-luSize,
//     `li r5,4`(alignment) then `bl HeapMalloc::Malloc`. Asserts the result non-null with
//     "CgsMemory::HeapMalloc::Malloc failed." and returns it.
//   - Free: `lwz r3, 4(this)` loads mpHeap, then `b HeapMalloc::Free` -- a tail call forwarding
//     the block pointer (r4) to mpHeap->Free.
//   - The vector deleting destructor writes the vtable pointer (off_8200F5B4) into *this then,
//     when the low bit of the flag is set, calls ::operator delete(this). The compiler-generated
//     scalar/vector deleting destructor is emitted by the host compiler from the virtual ~dtor;
//     ~MemcardAllocator itself has no X360 body work (no members to release -- mpHeap is owned
//     elsewhere), so it is a trivial out-of-line definition that anchors the vtable.
//   - The X360-baked CgsSaveLoadX360.h file/line (the Malloc-failed assert cites line 649) is
//     discarded per project convention; the assert string is X360 rodata, reproduced verbatim.

namespace CgsGui
{
    // Out-of-line destructor: anchors the vtable (the X360 vector-deleting-destructor stores the
    // vtable pointer at +0 and conditionally frees the object; the host compiler synthesises the
    // deleting-destructor thunk from this virtual ~dtor). No owned resources to release.
    MemcardAllocator::~MemcardAllocator()
    {
    }

    // X360 0x827DBB08.
    void* MemcardAllocator::Alloc(s32 luSize)
    {
        void* lpBlock = mpHeap->Malloc(luSize, KI_DEFAULT_ALIGNMENT);
        CGS_ASSERT(lpBlock != 0, "CgsMemory::HeapMalloc::Malloc failed.");
        return lpBlock;
    }

    // X360 0x827DBBC8 (tail call into mpHeap->Free).
    void MemcardAllocator::Free(void* lpBlock)
    {
        mpHeap->Free(lpBlock);
    }
}
