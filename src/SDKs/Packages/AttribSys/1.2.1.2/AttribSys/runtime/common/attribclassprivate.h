#pragma once

// AttribSys runtime -- Attrib::ClassPrivate, the private implementation object behind
// an attribute Attrib::Class.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2) + the DecFIGS DWARF
// (attribprivate.h:297 `struct Attrib::ClassPrivate : public Class`, members :344-351).
// The three ledger-attested bodies live in attribclassprivate.cpp:
//   Attrib::ClassPrivate::ClassPrivate  @ 0x8280EC80  (ClassLoadData&, Vault*)
//   Attrib::ClassPrivate::Release       @ 0x8280C370
//   Attrib::ClassPrivate::~ClassPrivate @ 0x8280F4D8
//
// X360 layout (ctor stores): Class base {mKey u64 @+0, mpPrivates @+8}, then
// mLayoutTable @+16 (the 12-byte X360 Attrib::HashMap), mCollections @+28 (the
// 12-byte VecHashMap header), mLayoutSize u16 @+40, mNumDefinitions u16 @+42,
// mpDefinitions @+44, mpSource @+48, mStaticData @+52 (X360 sizeof == 56, the
// ClassExportPolicy::Initialize Alloc). This is the x64 semantic-parity model:
// named members, widened pointers (the earlier raw-byte-offset body writes are
// retired -- on x64 they overlapped once pointers widened).

#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribhashmap.h"  // Attrib::HashMap (mLayoutTable)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"            // Attrib::CollectionHashMap (mCollections)

namespace Attrib
{
    class Vault;   // AttribSys resource vault (owns the loaded class data; +16 = live refcount).
    struct DatabasePrivate;

    // The serialised Attrib::Definition record (DWARF attribsys.h:336, members
    // :575-581; 24-byte stride). Verified field-for-field against the schema bin
    // (sorted u64 keys; mType == hash64 of the EA::Reflection/Attrib type name;
    // offsets bounded by the class layout size; alignment stored as log2).
    struct Definition
    {
        u64 mKey;         // +0x00  attribute key (hash64; lower_bound compare @0x82808E68)
        u64 mType;        // +0x08  type key (hash64 of the type name)
        u16 mOffset;      // +0x10  offset in the class layout / static block
        u16 mSize;        // +0x12  element byte size
        u16 mMaxCount;    // +0x14  element count (1 for scalars)
        u8  mFlags;       // +0x16  bit1 laid-out / bit3 ? / bit4 static / bit0 ?
        u8  mAlignment;   // +0x17  log2 alignment
    };

    // The serialised ClassLoadData record (DWARF attribprivate.h:194, members
    // :196-204; 40 serialised bytes -- the `>= 0x28` guard in
    // ClassExportPolicy::Initialize @0x8280EE38). mDefinitions/mStaticData are
    // 4-byte pointer SLOTS the vault PtrN fixups resolve into the schema bin
    // (the committed serialised-resource PointerFromU32 convention).
    struct ClassLoadData
    {
        u64 mClass;              // +0x00  class key (hash64 of the class name)
        u32 mCollectionReserve;  // +0x08  collection-table Rebuild seed
        u32 mNumDefinitions;     // +0x0C
        u32 mDefinitions;        // +0x10  Definition[] slot (fixed up)
        u32 mStaticSize;         // +0x14
        u32 mStaticData;         // +0x18  static block slot (fixed up)
        u32 mLayoutSize;         // +0x1C
        u16 mLayoutKeyShift;     // +0x20  layout HashMap key shift
        u16 mLayoutCount;        // +0x22  layout HashMap seed count
        u32 mPad;                // +0x24  (40-byte serialised stride)

        const Definition* GetDefinitions() const
        {
            return reinterpret_cast<const Definition*>(
                static_cast<uintptr_t>(mDefinitions));
        }
        const void* GetStaticData() const
        {
            return reinterpret_cast<const void*>(static_cast<uintptr_t>(mStaticData));
        }
    };

    // Attrib::ClassStaticDesc (DWARF attribprivate.h:139) -- one generated
    // static-class descriptor {key, the generated static struct, its size}.
    // GetStatic searches the generated table (codegen); the PC generated tables
    // are not yet recovered, so the seam returns NULL (no schema class carries
    // static data -- the schema PtrN has no mStaticData fixups -- so the copy
    // step is a no-op on the real data either way).
    struct ClassStaticDesc
    {
        ::Attribute::Key mKey;     // attribprivate.h:140
        void*            mStruct;  // attribprivate.h:141
        unsigned int     mSize;    // attribprivate.h:142

        static const ClassStaticDesc* GetTable(unsigned int& lruCount);  // attribprivate.h:143
        static const ClassStaticDesc* GetStatic(::Attribute::Key luKey); // attribprivate.h:144
    };

    // Read the process attribute database's private impl (the X360 reads
    // off_83011BC4 then its +4 mPrivates inline, asserting the database is
    // initialized first). Body: attribsys.cpp (sThis is private to Database).
    DatabasePrivate* GetDatabasePrivate();

    // DatabasePrivate::QueueForDelete<Class> @0x8280BF78 -- defer a class for
    // garbage collection on the database's class garbage ring. Body in its own
    // TU (the list-node allocate seam); declared here for Release.
    void* DatabasePrivate_QueueClassForDelete(void* lpClass, void* lpGarbageList);

    // Attrib::Class (DWARF: the {key, privates} handle; the ClassPrivate ctor
    // copies the 8-byte serialised base then re-points mpPrivates at itself).
    // The full Class API is inlined away on the X360 spine; the committed
    // collection-table wrappers live in vechashmap.h's Attrib::Class.
    //
    // Attrib::ClassPrivate : public Class (DWARF attribprivate.h:297).
    class ClassPrivate
    {
    public:
        ClassPrivate(const ClassLoadData& lrLoad, Vault* lpSource); // @ 0x8280EC80
        ~ClassPrivate();                                            // @ 0x8280F4D8
        int Release();                                              // @ 0x8280C370

        // In-place construction over an Attrib::Alloc'd block
        // (ClassExportPolicy::Initialize @0x8280EE38).
        static void* operator new(size_t, void* lpPlacement) { return lpPlacement; }
        static void  operator delete(void*, void*) {}

        // ---- Class base (DWARF attribsys.h Class; X360 +0/+8) ----
        u64           mKey;          // +0x00  class key (u64 hash; the ctor ld/std's it)
        ClassPrivate* mpPrivates;    // +0x08  self back-reference

        // ---- ClassPrivate members (DWARF attribprivate.h:344-351) ----
        HashMap           mLayoutTable;     // X360 +16  (layout attribute table)
        CollectionHashMap mCollections;     // X360 +28  (key -> Collection*)
        u16               mLayoutSize;      // X360 +40
        u16               mNumDefinitions;  // X360 +42
        const Definition* mDefinitions;     // X360 +44  (points into the schema bin)
        Vault*            mSource;          // X360 +48
        void*             mStaticData;      // X360 +52  (DWARF :351; untouched by the ctor)
    };
}
