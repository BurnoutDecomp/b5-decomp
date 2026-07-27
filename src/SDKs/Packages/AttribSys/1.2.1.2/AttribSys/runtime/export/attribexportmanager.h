#pragma once

#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h" // Attrib::Vault, TypeID, ExportID
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/export/attribiexportpolicy.h" // Attrib::IExportPolicy

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
    // concrete policies below share its layout; the differentiating export behaviour
    // is virtual. Derives the full IExportPolicy interface (the DWARF base of all
    // three concrete policies) so the ExportManager's IExportPolicy* table can hold
    // them and the Vault's slot-numbered dispatch lands on the right methods.
    class ExportPolicy : public IExportPolicy
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

    // Whole-Database export policy. Deleting-destructor @ 0x82807F98; vtable
    // off_820D9630 (dumped from the ARTIST .i64):
    //   IsExported = the ICF-folded `return true` @0x82C296C8; Initialize
    //   @0x8280CAC8 builds the DatabasePrivate; AnyReferences = the folded
    //   `return false` @0x827E2F38; IsReferenced @0x82807EC0; PrepareToClean/
    //   Clean/PrepareToDeinitialize = the folded empty body @0x8284CB38;
    //   Deinitialize @0x8280F5D0.
    class DatabaseExportPolicy : public ExportPolicy
    {
    public:
        virtual bool IsExported(const TypeID& lrType);                              // @ 0x82C296C8 (fold)
        virtual void Initialize(Vault& lrVault, const TypeID& lrType,
                                const ExportID& lrExport, void* lpData,
                                unsigned int luSize);                               // @ 0x8280CAC8
        virtual bool AnyReferences(const Vault& lrVault);                           // @ 0x827E2F38 (fold)
        virtual bool IsReferenced(const Vault& lrVault, const TypeID& lrType,
                                  const ExportID& lrExport);                        // @ 0x82807EC0 (own TU)
        virtual void PrepareToClean(Vault& lrVault);                                // @ 0x8284CB38 (fold)
        virtual void Clean(Vault& lrVault, const TypeID& lrType,
                           const ExportID& lrExport);                               // @ 0x8284CB38 (fold)
        virtual void PrepareToDeinitialize(Vault& lrVault);                         // @ 0x8284CB38 (fold)
        virtual void Deinitialize(Vault& lrVault, const TypeID& lrType,
                                  const ExportID& lrExport);                        // @ 0x8280F5D0 (own TU)
        virtual ~DatabaseExportPolicy();
    };

    // -----------------------------------------------------------------------
    // Attrib::ExportManager -- a Database's registry of export policies. When a
    // Database is sealed it registers one policy per attribute granularity (whole
    // Database / Class / Collection) into a fixed, TypeID-sorted table; a Vault then
    // looks a policy up by TypeID (eastl::lower_bound over that table) as it exports,
    // initializes, or deinitializes its assets.
    //
    // The three-field layout is X360-attested from AddExportPolicy (0x82803138),
    // which reads the table base, the reserved capacity, and the live count and
    // appends one row: mpPolicies@+0 / mMaxPolicies@+4 / mNumPolicies@+8 on the X360
    // (pointer widened here for the PC target). Export @ 0x8280A988 confirms the same
    // base@+0 / count@+8 pair when it binary-searches the table.
    // -----------------------------------------------------------------------
    class ExportManager
    {
    public:
        // One (TypeID -> policy) row of the sorted policy table. Members from the
        // DecFIGS DWARF (attribloadandgo.cpp:29/30). The table is kept sorted on
        // mType so a Vault can binary-search it by attribute type.
        struct ExportPolicyPair
        {
            TypeID         mType;    // attribute type this policy serialises
            IExportPolicy* mPolicy;  // the policy object (owned by the Database)

            bool operator<(const ExportPolicyPair& lrOther) const;
        };

        // Reserve a fixed policy table of luReserve rows (Database::GetExportPolicies
        // fills it). X360 0x82803110 region; body in its own TU.
        explicit ExportManager(unsigned int luReserve);

        // Append one policy row (X360 0x82803138). Asserts the reserved table still
        // has room, then stores {luType, lpPolicy} at the next free slot and bumps
        // the live count.
        void AddExportPolicy(TypeID luType, IExportPolicy* lpPolicy);

        // Deinitialize every registered policy against lrVault (X360 0x828031A8).
        // Body lives in the AttribSys load-and-go TU (attribloadandgo.cpp).
        void PrepareToDeinitialize(const Vault& lrVault);

        // TypeID -> policy lookups over the sorted table (DWARF attribloadandgo.h:
        // 109/112). The X360 inlines the eastl::lower_bound instantiation
        // (@0x82808FE8) at every Vault call site (ctor export counting /
        // Initialize dispatch / Export kind resolution); the DWARF-declared
        // member accessors are that same search. GetExportPolicyIndex returns
        // mNumPolicies when the type has no policy (the Vault sites test the
        // result against the live count). Bodies: attribloadandgo.cpp.
        IExportPolicy* GetExportPolicy(TypeID luType) const;
        unsigned int   GetExportPolicyIndex(TypeID luType) const;

        // Live-table accessors the Vault dispatch loops read (mpPolicies/mNumPolicies
        // by name; the X360 reads the raw +0/+8 fields inline).
        unsigned int            GetNumPolicies() const  { return mNumPolicies; }
        const ExportPolicyPair& GetPair(unsigned int luIndex) const { return mpPolicies[luIndex]; }

        // Sort the pair table ascending by TypeID (Database::GetExportPolicies
        // @0x8280DC70 runs the eastl::quick_sort<ExportPolicyPair*> instantiation
        // after registering the three granularity policies). Body:
        // attribloadandgo.cpp.
        void SortPolicies();

    private:
        ExportPolicyPair* mpPolicies;    // X360 +0 : reserved, TypeID-sorted table
        unsigned int      mMaxPolicies;  // X360 +4 : reserved capacity
        unsigned int      mNumPolicies;  // X360 +8 : live entry count
    };
}
