#pragma once

#include "types.hpp"

// AttribSys export policies -- the per-scope serialisation policy objects the
// AttribSys ExportManager hands out (Attrib::Database::GetExportPolicies()).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). The export
// subsystem drives the editor/live-link path: when AttribSys writes a database
// back out, ExportManager selects a policy per granularity -- a whole Database,
// a single Class, or a single Collection -- and the policy decides what subset of
// attributes is serialised. Each policy is a small polymorphic object with a vtable
// and no data members of its own (X360 free size 4 == one vptr); the heavy export
// logic is virtual and lives in the policy's own out-of-line TUs.
//
// This header owns the three policy types so their single shared vtable
// (off_820D8E08) and their deleting-destructor thunks are emitted exactly once:
//   Attrib::ClassExportPolicy::`vector deleting destructor'      @ 0x82808018
//   Attrib::CollectionExportPolicy::`vector deleting destructor' @ 0x82808098
//   Attrib::DatabaseExportPolicy::`scalar deleting destructor'   @ 0x82807F98
// All three thunks store off_820D8E08 at this+0 then, on the should-free bit, run
// the AttribSys census-and-free for a 4-byte object. The destructors are trivial
// (no owned members), so the compiler synthesises those thunks from the empty
// virtual ~Policy() plus the class operator delete bodied in the companion .cpp.
namespace Attrib
{
    // Base policy: a bare polymorphic object (vptr only, X360 sizeof == 4). The three
    // concrete policies below share its layout and its vtable address; the differentiating
    // export behaviour is virtual and reconstructed in their own (non-boot) TUs.
    class ExportPolicy
    {
    public:
        virtual ~ExportPolicy();

    public:
        // Frees a policy object back to the AttribSys package allocator with the shared
        // live-byte census update (sizeof == 4 at the X360 sites). The size is taken from
        // the deleting destructor (host sizeof) and forwarded with a NULL diagnostic tag.
        // The X360 "vector deleting destructor" thunks here free a single object (no array
        // loop, free size 4), so they resolve to this scalar operator delete.
        static void operator delete(void* lpBlock, size_t lnBytes);
    };

    // Per-Class export policy. Vtable off_820D8E08; deleting-destructor @ 0x82808018.
    class ClassExportPolicy : public ExportPolicy
    {
    public:
        virtual ~ClassExportPolicy();
    };

    // Per-Collection export policy. Vtable off_820D8E08; deleting-destructor @ 0x82808098.
    class CollectionExportPolicy : public ExportPolicy
    {
    public:
        virtual ~CollectionExportPolicy();
    };

    // Whole-Database export policy. Vtable off_820D8E08; deleting-destructor @ 0x82807F98.
    class DatabaseExportPolicy : public ExportPolicy
    {
    public:
        virtual ~DatabaseExportPolicy();
    };
}
