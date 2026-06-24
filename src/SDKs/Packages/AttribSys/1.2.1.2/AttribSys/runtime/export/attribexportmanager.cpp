#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/export/attribexportmanager.h"

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // shared AttribSys census-free

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::ClassExportPolicy::`vector deleting destructor'      @ 0x82808018
//   Attrib::CollectionExportPolicy::`vector deleting destructor' @ 0x82808098
//   Attrib::DatabaseExportPolicy::`scalar deleting destructor'   @ 0x82807F98
//
// Each X360 thunk is MSVC's deleting destructor for a vtable'd export-policy object
// that owns no heap members: it stores the shared policy vtable pointer (off_820D8E08)
// at this+0 and, when the low should-free flag bit is set, frees the 4-byte object
// through the AttribSys package allocator with the live-byte census update, then
// returns this. The free site reads GetAttribS() (== AttribSysMemoryManager::
// GetAttribSysAllocator()) and calls Free(this, 4, 0) (sub_82802138 with a NULL tag),
// decrementing dword_83011BFC and refreshing dword_83011BF8 first. That census-and-free
// is the shared HashMapTablePolicy::FreeWithCensus helper.
//
// The destructor bodies are empty (no owned members); the compiler synthesises the
// vtable-store + conditional operator delete shown above. All three policies share one
// vtable address on the X360 (off_820D8E08): the linker folded their identical vtables.

namespace Attrib
{
    // Shared policy operator delete: census-account and free a 4-byte policy object with a
    // NULL diagnostic tag, mirroring the GetAttribS()->Free(this, size, 0) at each site.
    void ExportPolicy::operator delete(void* lpBlock, size_t lnBytes)
    {
        HashMapTablePolicy::FreeWithCensus(lpBlock, lnBytes, NULL);
    }

    ExportPolicy::~ExportPolicy()
    {
    }

    ClassExportPolicy::~ClassExportPolicy()
    {
    }

    CollectionExportPolicy::~CollectionExportPolicy()
    {
    }

    DatabaseExportPolicy::~DatabaseExportPolicy()
    {
    }
}
