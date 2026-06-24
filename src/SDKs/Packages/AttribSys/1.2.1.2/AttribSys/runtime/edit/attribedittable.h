#pragma once

#include "types.hpp"
#include <cstddef> // size_t

// Attrib::EditTable -- the AttribSys editor/live-link edit table.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). EditTable is the
// mutable, editor-side table AttribSys allocates while decoding a live-link edit
// message (its operator new is reached from Attrib::DecodeLiveLinkMessage @ the
// live-link path). Only the boot-traceable class allocator is reconstructed here:
//   Attrib::EditTable::operator new @ 0x82805A80
// The table's data layout and its edit/apply members live in their own (editor-only,
// non-boot) TUs, so this header declares just enough for the allocator to be homed.
namespace Attrib
{
    class EditTable
    {
    public:
        // X360 0x82805A80 -- allocate an EditTable from the AttribSys package allocator.
        // Asserts the AttribSys memory manager has been Prepare'd (sbHasLinearAllocator,
        // CgsAttribSysMemoryManager.h:211) then forwards to the AttribSys package
        // allocator's Malloc(size, 0). Routed through AttribSysMemoryManager::
        // GetAttribSysAllocator() (which performs that same sbHasLinearAllocator assert
        // and returns &sAttribSysAllocator == the X360 &dword_83011B94 the asm Mallocs from).
        static void* operator new(size_t lnBytes);
    };
}
