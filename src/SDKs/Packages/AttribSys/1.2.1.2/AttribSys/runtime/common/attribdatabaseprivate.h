#pragma once

// AttribSys runtime -- Attrib::DatabasePrivate, the private implementation object behind
// the process attribute Attrib::Database (the class registry / garbage-collection engine).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). DWARF
// (references/DecFIGS/dwarfdump/.../common/attribdatabase.cpp:265) attests
//   `struct Attrib::DatabasePrivate : public Database`   (sizeof == 172 / 0xAC)
// with named members mClasses / mNumCompiledTypes / mCompiledTypes / mTypes /
// mGarbageCollections / mGarbageClasses. Their offsets/types are not attested by this
// batch's only body (the deleting destructor, which touches NO members), so -- following
// the committed sibling convention (attribclassprivate.h deliberately does NOT model its
// DWARF base because the base type is incomplete/uncompilable, and reaches members by byte
// offset) -- the interior is modelled as one opaque 172-byte span after the vptr. The DWARF
// base + member names are recorded here for the future member-touching TUs; do NOT fork
// this type when they land, GROW it (replace the opaque span with the named members).
//
// The vtbl'd DatabasePrivate is the private object Attrib::Database (attribsys.h) holds at
// its +4 mPrivates slot.
#include "types.hpp"
#include <cstddef>   // size_t

namespace Attrib
{
    struct DatabasePrivate
    {
        // @ 0x8280CA5C -- the real virtual destructor (releases the class registry / garbage
        // queues this object owns). Its body lives in its own AttribSys TU (todo); declared
        // here so the compiler can synthesise the scalar deleting-destructor thunk
        // (X360 @ 0x8280CA40) from it. Overriding a virtual gives the class its vptr @+0x00.
        virtual ~DatabasePrivate();

        // The class operator delete the deleting-destructor thunk's free site routes through
        // (frees the 172-byte object via the AttribSys package allocator, null-guarded, with
        // the shared live-byte census decremented). Defined in attribdatabaseprivate.cpp.
        static void operator delete(void* lpBlock, size_t lnBytes);

        // Opaque interior sized to the attested 172-byte X360 stride (member offsets/types
        // unattested by this batch; reached by name once their TUs land). No offset is
        // asserted across it; the sole body here touches none of it.
        unsigned char maOpaque[172];
    };
}
