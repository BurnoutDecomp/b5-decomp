#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // shared AttribSys census-free

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   Attrib::Database::Get @ 0x828021A0
//   Attrib::Database::~Database / operator delete  [scalar deleting destructor thunk] @ 0x828057A8
//
// Header-only TU companion: defines the singleton self-pointer (off_83011BC4) and the
// boot-traced static accessors. The remaining Database query API lives in
// attribdatabase.cpp (its own TU), so it is declared-only in the header.

namespace Attrib
{
    // off_83011BC4 â€” installed by the (separately reconstructed) Database constructor.
    Database* Database::sThis = nullptr;

    // ~Database @ 0x828057A8 (scalar deleting destructor thunk). The X360 thunk stores
    // the Database vtable pointer (off_820D8E2C) at this+0 and, when the low should-free
    // bit is set, runs operator delete(this, sizeof(Database)) and returns this. Database
    // holds only a non-owning reference (mPrivates), so the destructor body is empty; the
    // compiler synthesises the vtable-store + conditional operator delete shown above from
    // this trivial virtual ~Database().
    Database::~Database()
    {
    }

    // operator delete @ the deleting-destructor's free site. The X360 decrements the
    // shared AttribSys live-byte census by sizeof(Database) (=8), refreshes the peak,
    // then frees the block through the AttribSys package allocator with a NULL diagnostic
    // tag (GetAttribS()->Free(this, 8, 0)). Routed through the shared census-free helper
    // so the two census counters (dword_83011BFC / dword_83011BF8) stay defined once.
    void Database::operator delete(void* lpBlock, size_t lnBytes)
    {
        HashMapTablePolicy::FreeWithCensus(lpBlock, lnBytes, NULL);
    }

    Database& Attrib::Database::Get()
    {
        // X360: read sThis; assert + return it (the assert path still returns sThis).
        CGS_ASSERT(sThis != nullptr, "Attribute database not initialized.");
        return *sThis;
    }

    bool Database::IsInitialized()
    {
        return sThis != nullptr;
    }
}
