#pragma once

// AttribSys runtime -- Attrib::ClassPrivate, the private implementation object behind
// an attribute Attrib::Class.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). ClassPrivate (DWARF:
// derived from Class) owns a layout HashMap (@+0x10), a collection table (@+0x1C, an
// Attrib::CollectionHashMap == the VecHashMap<...Collection...,true,96u> instantiation),
// the layout/definition metadata (@+0x28..+0x2C) and a back-reference to the source
// Vault (@+0x30). The three ledger-attested bodies live in attribclassprivate.cpp:
//   Attrib::ClassPrivate::ClassPrivate  @ 0x8280EC80  (ClassLoadData&, Vault*)
//   Attrib::ClassPrivate::Release       @ 0x8280C370
//   Attrib::ClassPrivate::~ClassPrivate @ 0x8280F4D8
//
// The sub-tables (the layout HashMap / the collection VecHashMap / the DatabasePrivate
// class registry) are owned by their own AttribSys TUs and reached through recovered byte
// offsets, mirroring the sibling attribinstance.cpp / attribute.cpp reconstructions. The
// cross-TU helpers below are free-function trap stubs here (matching the committed
// AttribSys convention); their real bodies live in separate todo TUs.
#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"

namespace Attrib
{
    class Vault;   // AttribSys resource vault (owns the loaded class data; +0x10 = live-class count).

    // The load descriptor a ClassPrivate is built from (Attrib::ClassLoadData). Only the
    // fields the ctor reads are named. A Class sub-object (the 8-byte {key,privates} base)
    // lives at +0x00 and is copied wholesale into the new ClassPrivate.
    struct ClassLoadData
    {
        u8    maClassBase[8];      // +0x00 : Class base (mKey + mpPrivates), copied to this+0
        u32   muNumDefinitions;    // +0x08 : gates the collection-table Rebuild seed
        u32   muNumDefinitions2;   // +0x0C : stored as ClassPrivate mNumDefinitions (u16)
        void* mpDefinitions;       // +0x10 : Definition[] base (24-byte records)
        u8    maPad0[0x08];        // +0x14
        u32   muLayoutSize;        // +0x1C : stored as ClassPrivate mLayoutSize (u16)
        u16   mu16LayoutKeyShift;  // +0x20 : HashMap key-shift
        u16   mu16LayoutCount;     // +0x22 : HashMap initial count
    };

    // AttribSys layout HashMap. Only the byte offsets the bodies touch are modelled by the
    // ClassPrivate bodies (buckets@+0x00, capacity key-shift u16@+0x04, live count u16@+0x06,
    // collection refcount u16@+0x08). Construct/Add/Release are their own AttribSys TUs
    // (free-function trap stubs below; a member function may not share the class name, so the
    // constructor/adder/releaser are free helpers so this batch compiles standalone).
    void HashMap_Construct(void* lpHashMap, unsigned int luCount, u8 lu8KeyShift, u8 lu8Dynamic);
    void HashMap_Add(void* lpHashMap, u64 luKey, u64 luValue, u16 lu16Type,
                     int liArg4, unsigned int luFlags, char lcArg6, int liArg7);
    int  HashMap_Release(void* lpHashMap);

    // Class-registry table helpers (Attrib::Class::TablePolicy_0_16_ in the X360 mangling)
    // and the collection-table clear helper (CollectionHashMap::Clear). Own TUs; trap stubs
    // here.
    void         ClassTable_Add(u16* lpTable, u32 luKey, void* lpClass);
    unsigned int ClassTable_Find(u16* lpTable, u32 luKey);
    void         ClassTable_EraseAt(u16* lpTable, unsigned int luIndex);   // sub_82808A98
    void         CollectionTable_Rebu();                                   // TablePolicy_1_96_::Rebu seed
    void         CollectionTable_Clear(void* lpTable);                     // CollectionHashMap::Clear

    // DatabasePrivate::QueueForDelete<Class> -- defers a class for garbage collection.
    void* DatabasePrivate_QueueClassForDelete(void* lpClass, void* lpGarbageList);

    // The Vault scalar-deleting destructor (Attrib::Vault::~Vault, deleting form).
    void Vault_Destroy(void* lpVault, char lcDeleting);

    // Read the process attribute database's private impl pointer (the X360 off_83011BC4
    // singleton followed to +4 = mPrivates). Asserts the database is initialized, exactly
    // as the X360 emits inline at each of these sites. Own TU (Database::sThis is private
    // to attribsys.h); trap stub here.
    void* GetDatabasePrivate();

    // Attrib::ClassPrivate. DWARF derives it from Attrib::Class, but Class is only
    // forward-declared in attribsys.h (incomplete here), so -- like the sibling
    // Attrib::Instance / Attrib::Attribute reconstructions -- the bodies reach every
    // member by its X360 byte offset rather than through a (currently uncompilable) base.
    // Layout by X360 byte offset:
    //   +0x00  mKey            : u32  (Class base key)
    //   +0x08  mpSelf          : ClassPrivate* (Class::mpPrivates self back-ref)
    //   +0x10  mLayoutTable    : HashMap (buckets@+0x10, shift u16 @+0x14, count u16 @+0x16,
    //                             collection refcount u16 @+0x18)
    //   +0x1C  mCollections    : Attrib::CollectionHashMap table (header zero-inited)
    //   +0x28  mLayoutSize     : u16
    //   +0x2A  mNumDefinitions : u16
    //   +0x2C  mpDefinitions   : Definition* (24-byte records)
    //   +0x30  mpSource        : Vault*
    class ClassPrivate
    {
    public:
        ClassPrivate(const ClassLoadData& lrLoad, Vault* lpSource); // @ 0x8280EC80
        ~ClassPrivate();                                            // @ 0x8280F4D8
        int Release();                                              // @ 0x8280C370
    };
}
