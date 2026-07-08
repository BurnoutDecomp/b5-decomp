#pragma once

#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h" // Attrib::Vault, TypeID, ExportID

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

    // The eight-method IExportPolicy interface each concrete policy overrides, in DWARF
    // vtable order (DecFIGS attribdatabase.cpp:182 ClassExportPolicy / :297
    // CollectionExportPolicy). Declared on the concrete classes (not hoisted onto the
    // minimally-modelled IExportPolicy/ExportPolicy base above, whose reconstruction is
    // deliberately vptr-only) so the per-policy vtable carries the right slots. Bodies land
    // in the policies' own AttribSys TUs; the two/three trivial "should never happen" asserts
    // this batch grounds are defined in attribdatabase.cpp (their X360 primary_file).

    // Per-Class export policy. Vtable off_820D8E08; deleting-destructor @ 0x82808018.
    class ClassExportPolicy : public ExportPolicy
    {
    public:
        virtual bool IsExported(const TypeID& lrType);                                // own TU
        virtual void Initialize(Vault& lrVault, const TypeID& lrType,
                                const ExportID& lrExport, void* lpData,
                                unsigned int luSize);                                 // own TU
        virtual bool AnyReferences(const Vault& lrVault);                             // @ 0x8280B2F0 (own TU)
        virtual bool IsReferenced(const Vault& lrVault, const TypeID& lrType,
                                  const ExportID& lrExport);                          // own TU
        virtual void PrepareToClean(Vault& lrVault);                                  // @ 0x8280B450 (own TU)
        virtual void Clean(Vault& lrVault, const TypeID& lrType,
                           const ExportID& lrExport);                                 // @ 0x82805620
        virtual void PrepareToDeinitialize(Vault& lrVault);                           // own TU
        virtual void Deinitialize(Vault& lrVault, const TypeID& lrType,
                                  const ExportID& lrExport);                          // @ 0x82805660
        virtual ~ClassExportPolicy();
    };

    // Per-Collection export policy. Vtable off_820D8E08; deleting-destructor @ 0x82808098.
    class CollectionExportPolicy : public ExportPolicy
    {
    public:
        virtual bool IsExported(const TypeID& lrType);                               // own TU
        virtual void Initialize(Vault& lrVault, const TypeID& lrType,
                                const ExportID& lrExport, void* lpData,
                                unsigned int luSize);                                // own TU
        virtual bool AnyReferences(const Vault& lrVault);                            // @ 0x8280B728 (own TU)
        virtual bool IsReferenced(const Vault& lrVault, const TypeID& lrType,
                                  const ExportID& lrExport);                         // @ 0x828056A0
        virtual void PrepareToClean(Vault& lrVault);                                 // @ 0x8280B9F8 (own TU)
        virtual void Clean(Vault& lrVault, const TypeID& lrType,
                           const ExportID& lrExport);                                // @ 0x828056E0
        virtual void PrepareToDeinitialize(Vault& lrVault);                          // @ 0x8280CD80 (own TU)
        virtual void Deinitialize(Vault& lrVault, const TypeID& lrType,
                                  const ExportID& lrExport);                         // @ 0x82805720
        virtual ~CollectionExportPolicy();
    };

    // Whole-Database export policy. Vtable off_820D8E08; deleting-destructor @ 0x82807F98.
    class DatabaseExportPolicy : public ExportPolicy
    {
    public:
        virtual ~DatabaseExportPolicy();
    };
}
