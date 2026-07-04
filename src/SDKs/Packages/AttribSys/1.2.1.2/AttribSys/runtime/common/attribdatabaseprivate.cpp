#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabaseprivate.h"

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // shared AttribSys census-free

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::DatabasePrivate::`scalar deleting destructor' @ 0x8280CA40
//
// The X360 thunk is MSVC's scalar deleting destructor for the vtbl'd DatabasePrivate
// registry impl (the private object behind Attrib::Database; DWARF: struct
// Attrib::DatabasePrivate : public Database, sizeof == 172 / 0xAC). It runs the real
// virtual ~DatabasePrivate() (bl @ 0x8280CA5C, a separate AttribSys TU), then -- when the
// low should-free bit of the deleting flag is set -- decrements the shared AttribSys
// live-byte census by 172, refreshes the peak, NULL-checks the block (cmplwi r30,0 / beq
// at the X360 site), and only then frees the 172-byte object through the AttribSys package
// allocator with a NULL diagnostic tag, returning this. Because THIS site null-guards the
// free (unlike the sibling Database @ 0x828057A8 / ExportPolicy sites, which free
// unconditionally via FreeWithCensus), the free is routed through the null-guarded
// FreeWithCensusIf helper; the census decrement itself is unconditional inside the
// should-free branch. The compiler synthesises the deleting thunk from the virtual
// ~DatabasePrivate() plus the class operator delete bodied below.
namespace Attrib
{
    // ~DatabasePrivate @ 0x8280CA5C (inner dtor, called by the deleting thunk). Its real
    // body -- releasing the class registry / garbage queues this object owns -- lives in
    // its own AttribSys TU (todo) and is not part of this batch; bodied empty here so the
    // deleting-destructor thunk this batch reconstructs has a virtual dtor to synthesise
    // from without fabricating the member cleanup.
    DatabasePrivate::~DatabasePrivate()
    {
    }

    // operator delete @ the deleting-destructor's free site. Decrements the shared
    // AttribSys live-byte census by sizeof(DatabasePrivate) (=172), refreshes the peak,
    // then -- only when the block is non-NULL -- frees it through the AttribSys package
    // allocator with a NULL diagnostic tag (GetAttribS()->Free(this, 172, 0)). Routed
    // through the shared null-guarded census-free helper (matching the cmplwi r30,0 / beq
    // the X360 emits before the free) so the two census counters (dword_83011BFC /
    // dword_83011BF8) stay defined exactly once.
    void DatabasePrivate::operator delete(void* lpBlock, size_t lnBytes)
    {
        HashMapTablePolicy::FreeWithCensusIf(lpBlock, lnBytes, NULL);
    }
}
