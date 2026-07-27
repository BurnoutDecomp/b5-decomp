#pragma once

// AttribSys runtime -- Attrib::DatabasePrivate, the private implementation object behind
// the process attribute Attrib::Database (the class registry / garbage-collection engine).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). DWARF
// (references/DecFIGS/dwarfdump/.../common/attribdatabase.cpp:265) attests
//   `struct Attrib::DatabasePrivate : public Database`   (X360 sizeof == 172 / 0xAC)
// with the named members below (attribdatabase.cpp:97-104):
//   mClasses            ClassTable (VecHashMap<Key,Class,TablePolicy,false,16u>)
//   mNumCompiledTypes   unsigned int
//   mCompiledTypes      TypeDescPtrVec (eastl::vector<const TypeDesc*>)
//   mTypes              TypeTable (eastl::set<TypeDesc>)
//   mGarbageCollections CollectionList (eastl::list<const Collection*>)
//   mGarbageClasses     ClassList (eastl::list<const Class*>)
// The ctor @0x8280C598 (from a serialised DatabaseLoadData) attests the member
// order/offsets on the X360: Database base {vptr@0, mPrivates@+4=self},
// mClasses@+8 (12B), mNumCompiledTypes@+20, mCompiledTypes@+24 (12B),
// mTypes@+36, garbage ring sentinels @+108/+140.
//
// This is the x64 semantic-parity layout (named members, widened pointers) per
// the reconstruction rules; the earlier opaque-172-byte-span model is retired.

#include "types.hpp"
#include <cstddef>   // size_t

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"          // Attrib::Database base + TypeDesc fwd
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"          // VecHashMap_Attrib_Class_TablePolicy_0_16
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.h" // AttribListBase (garbage rings)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"     // Attrib::TypeDesc (the corrected u64-key row)

namespace Attrib
{
    class ITypeHandler;
    class Vault;

    // ------------------------------------------------------------------------
    // Attrib::ClassTable (DWARF attribdatabase.cpp:16) -- the class registry:
    // the committed VecHashMap<Key,Class,TablePolicy,false,16u> instantiation
    // plus the reserving ctor the DatabasePrivate ctor runs (ClassTable(n) =
    // zero the header, then RebuildTable(n) when n != 0 -- X360 @0x8280C5C4).
    // ------------------------------------------------------------------------
    struct ClassTable : public ::VecHashMap_Attrib_Class_TablePolicy_0_16
    {
        explicit ClassTable(unsigned int luReserve);
    };

    // ------------------------------------------------------------------------
    // Attrib::TypeDescPtrVec (DWARF attribdatabase.cpp:18) -- the by-index type
    // table: eastl::vector<const TypeDesc*, AttribSysPackageAllocator>. The
    // three-pointer control block is the committed AttribVectorBase shape; the
    // grow path is the committed AttribVectorReserve/Allocate/Free helper set.
    // ------------------------------------------------------------------------
    struct TypeDescPtrVec
    {
        const TypeDesc** mpBegin;        // +0
        const TypeDesc** mpEnd;          // +4 (x64 widened)
        const TypeDesc** mpCapacityEnd;  // +8

        unsigned int Size() const { return static_cast<unsigned int>(mpEnd - mpBegin); }

        // push_back (the X360 inlines the vector push at the two ctor sites;
        // grow via the committed AttribVector helpers) + the ctor's up-front
        // reserve (AttribSysPackageAllocator>::reser @0x8280C258). Bodies:
        // attribdatabase.cpp.
        void PushBack(const TypeDesc* lpDesc);
        void Reserve(unsigned int luCapacity);
    };

    // ------------------------------------------------------------------------
    // Attrib::TypeTable (DWARF attribdatabase.cpp:17) -- the keyed type
    // registry: eastl::set<TypeDesc, less<TypeDesc>, AttribSysPackageAllocator>.
    //
    // Reconstructed as the library-container seam (the committed AttribList/
    // AttribVector convention): node-stable keyed insert/find with nodes from
    // the AttribSys package allocator, matching the eastl rbtree's observable
    // behaviour (unique keys, stable value addresses, key-ordered walk). The
    // red-black REBALANCING of the eastl instantiation (X360 insert
    // @0x8280BCB8 + node-create @0x8280AB98) is a lookup-performance detail
    // and is deliberately not reproduced; behaviour (contents, stability,
    // ordering) is identical.
    // ------------------------------------------------------------------------
    class TypeTable
    {
    public:
        struct SetNode
        {
            SetNode* mpLeft;
            SetNode* mpRight;
            TypeDesc mValue;
        };

        // Construct empty (the X360 ctor zero-seeds the set anchor).
        void Construct();

        // Keyed unique insert; returns the STABLE stored TypeDesc (the existing
        // one when the key is already present, matching eastl::set::insert).
        TypeDesc* Insert(const TypeDesc& lrDesc);

        // Keyed lookup; NULL when absent (Database::GetTypeDesc's search).
        const TypeDesc* Find(u64 luType) const;

    private:
        SetNode*     mpRoot;
        unsigned int muSize;
    };

    // ------------------------------------------------------------------------
    // The serialised DatabaseLoadData (DWARF attribprivate.h:209, members
    // :211-214) -- the schema's database export payload (the first DatN record;
    // 16-byte head + the u32 type-size table). mTypenames is a 4-byte pointer
    // SLOT the vault PtrN fixup resolves to the typename strings in the schema
    // bin (the committed serialised-resource PointerFromU32 convention).
    // ------------------------------------------------------------------------
    struct DatabaseLoadData
    {
        u32 mNumClasses;      // attribprivate.h:211 (class-table reserve)
        u32 mDefaultDataSize; // attribprivate.h:212 (DefaultDataArea floor)
        u32 mNumTypes;        // attribprivate.h:213
        u32 mTypenames;       // attribprivate.h:214 (const char* slot, fixed up)

        // attribprivate.h:217 -- the type byte sizes follow the head directly.
        const u32* GetTypeSizes() const
        {
            return reinterpret_cast<const u32*>(this + 1);
        }
        const char* GetTypenames() const
        {
            return reinterpret_cast<const char*>(static_cast<uintptr_t>(mTypenames));
        }
    };

    // ------------------------------------------------------------------------
    // Attrib::DatabasePrivate (DWARF attribdatabase.cpp:35).
    // ------------------------------------------------------------------------
    struct DatabasePrivate : public Database
    {
        ClassTable     mClasses;             // attribdatabase.cpp:97  (X360 +8)
        unsigned int   mNumCompiledTypes;    // attribdatabase.cpp:99  (X360 +20)
        TypeDescPtrVec mCompiledTypes;       // attribdatabase.cpp:100 (X360 +24)
        TypeTable      mTypes;               // attribdatabase.cpp:101 (X360 +36)
        AttribListBase mGarbageCollections;  // attribdatabase.cpp:103 (X360 +108 ring)
        AttribListBase mGarbageClasses;      // attribdatabase.cpp:104 (X360 +140 ring)

        // @ 0x8280C598 -- build the registry from the schema's serialised
        // DatabaseLoadData: reserve the class table, seed the type registry with
        // the NULL type then one TypeDesc per schema typename (key = the hash64
        // of the name, size from the load data's size table, handler from the
        // generated TypeDesc::Lookup), publish each into the by-index vector,
        // and prime the default data area. Body: attribdatabase.cpp.
        explicit DatabasePrivate(const DatabaseLoadData& lrLoad);

        // @ 0x8280CA5C -- the real virtual destructor (releases the registry).
        // Deferred body (its own teardown TU); declared so the deleting thunk
        // (@0x8280CA40, attribdatabaseprivate.cpp) can synthesise.
        virtual ~DatabasePrivate();

        // The class operator delete the deleting-destructor thunk's free site
        // routes through. Defined in attribdatabaseprivate.cpp.
        static void operator delete(void* lpBlock, size_t lnBytes);
        // Placement form (DatabaseExportPolicy::Initialize constructs into an
        // Attrib::Alloc'd block); no-op release mirror.
        static void* operator new(size_t, void* lpPlacement) { return lpPlacement; }
        static void operator delete(void*, void*) {}
    };
}
