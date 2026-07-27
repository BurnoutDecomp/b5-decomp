#pragma once

// AttribSys runtime â€” Attrib::Database, the process-wide attribute database singleton.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). An earlier SDK
// revision used a 64-bit Key; the X360 spine is 32-bit, so this header reuses the
// committed `typedef u32 Key` (Attrib::Key / Attribute::Key) rather than re-forking it.
//
// Database is a vtbl'd object holding a single reference to its DatabasePrivate impl
// (mPrivates) and a private static self-pointer (sThis). Only the boot-traced slice is
// reconstructed here: the static accessor Get() and IsInitialized(); the rich query API
// (GetClass / AddType / CollectGarbage / ...) is declared-only because those bodies live
// in attribdatabase.cpp (their own TU). Out-of-line bodies + the sThis definition live in
// the attribsys.cpp companion.
#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"

namespace Attrib
{
    // X360: 32-bit class/attribute key. Reuse the committed runtime typedef instead of
    // re-forking the older 64-bit Key. (Attribute::Key is the generated-code spelling;
    // the heavy query API below is typed in those Attribute:: aliases, grown in AttributeKey.h.)
    typedef u32 Key;

    // Forward declarations for the heavy query API (owned by attribdatabase.cpp / SDK).
    class Class;
    struct TypeDesc;   // class-key MUST be struct (attribarray.h defines struct TypeDesc; MSVC mangles U/V from the key)
    class ExportManager;
    class DatabasePrivate;

    // The process-wide attribute database. A vtbl'd handle onto a DatabasePrivate impl,
    // with a private static self-pointer installed at construction.
    class Database
    {
    public:
        // Get @ 0x828021A0 â€” returns the installed database; asserts if uninitialised.
        static Database& Get();
        // IsInitialized @ attribsys.h:650 â€” true once a Database has been constructed.
        static bool IsInitialized();

        // --- declared-only query API (bodies in attribdatabase.cpp) ---
        // static on PC: the body never touches `this` and the X360 RegisterSchema
        // site calls it BEFORE the database singleton exists (@0x8280E4A0 calls
        // @0x8280DC70 with no valid this) -- semantic parity keeps it callable
        // pre-database without the Get() assert.
        static ExportManager& GetExportPolicies();
        void           ReleaseExportPolicies();
        // NOTE: fully qualify the top-level ::Attribute namespace here. Inside namespace
        // Attrib an unqualified `Attribute::` binds to the Attrib::Attribute cursor class
        // whenever attribute.h is also visible in the TU (as in attribute.cpp), which broke
        // these declarations. ::Attribute is AttributeKey.h's Key/Type namespace.
        Class*           GetClass(::Attribute::Key key) const;
        unsigned int     GetNumClasses() const;
        ::Attribute::Key GetFirstClass() const;
        ::Attribute::Key GetNextClass(::Attribute::Key key) const;
        unsigned int     GetNumIndexedTypes() const;
        const TypeDesc&  GetIndexedTypeDesc(u16 index) const;
        // @ 0x82808118 -- the keyed type-registry lookup; takes the 64-bit type
        // key (hash64 of the type name) and falls back to the NULL type row when
        // the key is unregistered. Body: attribdatabase.cpp.
        const TypeDesc&  GetTypeDesc(u64 type) const;
        ::Attribute::Type AddType(const char* name, unsigned int bytes);
        void             CollectGarbage();
        void             DumpContents(::Attribute::Key key) const;

    private:
        // The concrete registry impl derives Database and self-installs as its
        // own mPrivates (the X360 ctor @0x8280C598 stores this at +4); the
        // GetDatabasePrivate() helper (attribclassprivate.h seam) reads the
        // private singleton the same way the X360 reads off_83011BC4->+4 inline.
        friend struct DatabasePrivate;
        friend DatabasePrivate* GetDatabasePrivate();
        // DatabaseExportPolicy::Initialize (@0x8280CAC8) installs the freshly
        // built registry as the singleton (off_83011BC4 store).
        friend class DatabaseExportPolicy;

        explicit Database(DatabasePrivate& privates) : mPrivates(privates) {}
        Database(const Database& src);
        virtual ~Database();
        const Database& operator=(const Database& rhs);

        void* operator new(size_t bytes);
        void  operator delete(void* ptr, size_t bytes);
        void* operator new(size_t, void* ptr);
        void  operator delete(void*, void*);

        // +0: vptr (Database is virtual; ~Database is virtual in the X360 layout).
        const DatabasePrivate& mPrivates;   // attribsys.h:695

        // The singleton self-pointer (off_83011BC4 on X360). Defined in attribsys.cpp.
        static Database* sThis;              // attribdatabase.cpp:112
    };
}
