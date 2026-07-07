#pragma once

// vechashmap.h -- AttribSys's open-addressing, vector-backed hash map (AttribSys 1.2.1.2).
//
// VecHashMap<TKey, TValue, TPolicy, bMultiKey, GrowN> is a flat Node[] table probed
// linearly from ((u32)key % tableSize). A bucket is FREE/INVALID iff its value pointer
// points at the node itself (the X360 self-pointer sentinel). Each home bucket caches
// the length of the probe run that hashes to it (muMax); the table-wide maximum
// (muWorstCollision) drives a grow-and-rehash once a run exceeds the GrowN threshold.
//
// Recovered from BURNOUT_X360_ARTIST.XEX (CgsAttribSysUnity TU). VecHashMap has no
// committed generic template body in this tree -- only the two live instantiations
// appear as real X360 functions (the rest of the template API is inlined away at the
// call sites). Both instantiations are therefore homed here as concrete classes that
// share this one header, mirroring the sibling attribinstance.h / attribute.h X360
// reconstructions:
//   VecHashMap<Attrib::Key, Attrib::Class,      Attrib::Class::TablePolicy, false, 16u>
//       -> VecHashMap_Attrib_Class_TablePolicy_0_16  (single-key, rehash threshold 16)
//   VecHashMap<Attrib::Key, Attrib::Collection, Attrib::Class::TablePolicy, true,  96u>
//       -> Attrib::CollectionHashMap                 (multi-key, rehash threshold 96)
//
// Container layout (DWARF vechashmap.h:527-531; all 16-bit counters):
//   mTable          @ 0    Node*
//   mTableSize      @ 4    u16
//   mNumEntries     @ 6    u16
//   mFixedAlloc     @ 8    u16
//   mWorstCollision @ 0xA  u16
//
// Node layout (DWARF vechashmap.h:367-370; X360 stride = 16, proven by slwi ,4):
//   mKey  @ 0    u64      (the X360 build loads/stores the key with ld/std and compares
//                          with cmpld, so the on-disk key is a full 8-byte doubleword,
//                          even though Attrib::Key typedefs to u32 on this spine)
//   mPtr  @ 8    TValue*  (value pointer; == &node marks an empty slot)
//   mMax  @ 0xC  u32      (max probe length of the chain homed at this slot)
//
//   Add                @ 0x8280AAE8 / 0x828088D0
//   Clear              @ 0x8280DBA8
//   CopyFromOldTable   @ 0x82808D30 / 0x828066F0
//   Find               @ 0x828064D0
//   FindIndex          @ 0x828063C0 / 0x82806210
//   GetKeyAtIndex      @ 0x82807430
//   InternalAdd        @ 0x82808BB0 / 0x82806570
//   RebuildTable       @ 0x82806928 / 0x82806828
//   UpdateSearchLength @ 0x82806D60 / 0x82806A28

#include <cstddef>   // offsetof
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h" // Attrib::Collection, Attrib::Key

namespace Attrib
{
    class Class;

    // TablePolicy::Free -- releases a bucket array previously handed out by the policy's
    // Allocate. IDA names the free-standing thunk Attrib::TableFreeFunc; body lives in its
    // own TU (routes through the AttribSys package allocator + hash-map byte census).
    void* TableFreeFunc(void* lpBlock, size_t liSize);

    // The per-TU freed-byte census the inlined TablePolicy::Free bumps on release
    // (X360 dword_83011BA4). Homed in vechashmap.cpp so it is defined exactly once; the
    // freeOld path of CollectionHashMap::CopyFromOldTable adds 16 * oldSize to it.
    extern u32 gTablePolicyFreedBytes;

    // The compiler-synthesised scalar deleting destructor of Attrib::Collection
    // (X360 Attrib::Collection::_scalar_deleting_destructor_): runs ~Collection then hands
    // the object back to the collection allocator. Own ledger TU; trap stub in this TU.
    // Clear calls it on every live collection it evicts.
    void* Collection_ScalarDeletingDtor(Collection* lpCollection, int liDeleteFlag);
}

// -----------------------------------------------------------------------------
// VecHashMap<Attrib::Key, Attrib::Class, Attrib::Class::TablePolicy, false, 16u>
// -----------------------------------------------------------------------------
// Single-key class-registry table: maps a 64-bit attribute key -> Attrib::Class*.
// Kept in the vendor vechashmap home so all VecHashMap instantiations share one header.
class VecHashMap_Attrib_Class_TablePolicy_0_16
{
public:
    // The rehash threshold (the 16u template argument): a worst-case probe run longer
    // than this triggers a grow-and-rehash.
    static const u32 KU_REHASH_THRESHOLD = 16u;

    // vechashmap.h:344 -- one bucket. FREE iff mpPtr == &node (self-pointer sentinel).
    // 16-byte stride (X360-attested): std key @+0, stw ptr @+8, stw max @+0xC.
    struct Node
    {
        u64            mKey;  // +0x00  attribute key (64-bit in this instantiation)
        Attrib::Class* mpPtr; // +0x08  mapped Class* / self-pointer when free
        u32            muMax; // +0x0C  cached probe-run length for this home bucket

        // A bucket is valid (live) iff its ptr does NOT point at itself.
        bool IsValid() const
        {
            return mpPtr != reinterpret_cast<Attrib::Class*>(const_cast<Node*>(this));
        }
        Attrib::Class* Get() const { return IsValid() ? mpPtr : NULL; }
        u64            Key() const { return IsValid() ? mKey : 0; }
        u32            MaxSearch() const { return muMax; }
    };

    bool           Add(u64 luKey, Attrib::Class* lpValue);                 // 0x8280AAE8
    unsigned int   FindIndex(u64 luKey) const;                            // 0x828063C0
    Attrib::Class* Find(u64 luKey) const;                                 // 0x828064D0
    u64            GetKeyAtIndex(u32 luIndex) const;                      // 0x82807430

    // In-range AND non-sentinel test the asserting index accessors guard on.
    bool ValidIndex(u32 luIndex) const
    {
        return luIndex < muTableSize && mpTable[luIndex].IsValid();
    }

private:
    bool         InternalAdd(u64 luKey, Attrib::Class* lpValue);         // 0x82808BB0
    unsigned int UpdateSearchLength(unsigned int luOldIndex,
                                    unsigned int luFreeIndex);          // 0x82806D60
    void         CopyFromOldTable(Node* lpOldTable,
                                  unsigned int luOldSize, bool lbFree);  // 0x82808D30
    void         RebuildTable(unsigned int luNewCount);                 // 0x82806928

    Node* mpTable;          // +0x00  the flat bucket array
    u16   muTableSize;      // +0x04  number of buckets
    u16   muNumEntries;     // +0x06  live entries
    u16   muFixedAlloc;     // +0x08  non-zero => array is a fixed (non-growable) allocation
    u16   muWorstCollision; // +0x0A  longest probe run in the table
};

// Pin the pointer-free layout facts (X360-attested field offsets). The total node is
// 16 bytes on the X360 (32-bit mpPtr, slwi ...,4 stride); on a 64-bit host the pointer
// widens, so we pin the offsets that survive the width change rather than sizeof, exactly
// as the sibling Attrib::Node / RefSpec reconstructions do.
static_assert(offsetof(VecHashMap_Attrib_Class_TablePolicy_0_16::Node, mKey) == 0,
              "VecHashMap Node mKey must sit at +0 (X360 std/ld doubleword)");
static_assert(offsetof(VecHashMap_Attrib_Class_TablePolicy_0_16::Node, mpPtr) == 8,
              "VecHashMap Node mpPtr must sit at +8 (X360 self-pointer sentinel slot)");

namespace Attrib
{
    // -------------------------------------------------------------------------
    // VecHashMap<Attrib::Key, Attrib::Collection, Attrib::Class::TablePolicy, true, 96u>
    // -------------------------------------------------------------------------
    // The class-collection bucket-array table for one Attrib::Class (ClassPrivate::
    // mCollections). Multi-key; maps a 64-bit collection key -> Attrib::Collection*.
    class CollectionHashMap
    {
    public:
        // The template worst-collision rebuild threshold (the 96u instantiation param).
        static const u16 KU_WORST_COLLISION_LIMIT = 96;

        // One open-addressing bucket. 16 bytes; stride confirmed by the X360 asm.
        struct Node
        {
            u64         mKey;   // +0x00 -- bucket key (std/ld, 8 bytes)
            Collection* mPtr;   // +0x08 -- collection; == &self when the bucket is empty
            u32         mMax;   // +0x0C -- longest probe run that homes on this bucket

            // A bucket is empty iff its ptr points at itself (the X360 self-sentinel).
            bool IsEmpty() const
            {
                return reinterpret_cast<const void*>(mPtr) ==
                       reinterpret_cast<const void*>(this);
            }
            // Live iff not empty; Key()/Get() mirror the X360 sentinel-guarded reads.
            bool IsValid() const { return !IsEmpty(); }
            Collection* Get() const { return IsValid() ? mPtr : NULL; }
            u64 Key() const { return IsValid() ? mKey : 0; }
        };

        // X360 0x828088D0 -- public add: InternalAdd, then grow-and-rebuild if the worst
        // probe run now exceeds 96.
        bool Add(u64 luKey, Collection* lpPtr);

        // X360 0x82806210 -- open-addressing lookup; returns the bucket index or
        // mTableSize when absent.
        u32 FindIndex(u64 luKey) const;

        // X360 0x82806828 -- (re)allocate the bucket array and re-home every live entry,
        // growing by one bucket per pass until the worst run drops to 96 or below.
        // luNewSize == 0 is the release path.
        void RebuildTable(u32 luNewSize);

        // X360 0x828066F0 -- clear this (freshly-allocated) table to all-empty, then
        // re-insert every live entry of the old table; optionally free the old array.
        void CopyFromOldTable(Node* lpOldTable, u32 luOldSize, bool lbFreeOld);

        // X360 0x8280DBA8 -- destroy every live collection the table holds, release the
        // bucket array unless it is a fixed allocation, then zero the counts.
        void Clear();

        // Find(key) -- resolve the slot via FindIndex and return the stored collection,
        // or NULL when the key is absent. Real X360 symbol in its own ledger TU
        // (declaration-only here); Attrib::Class::GetCollectionWithDefault calls it.
        Collection* Find(u64 luKey) const;

        // X360 0x82808980 -- RemoveIndex(index): vacate the bucket at luIndex (running
        // UpdateSearchLength to preserve the probe-run invariant) and return the
        // collection that was removed, or NULL when the index does not name a live
        // bucket. Own ledger TU (declaration-only here); Attrib::Class::RemoveCollection
        // calls it.
        Collection* RemoveIndex(u32 luIndex);

    private:
        // X360 0x82806570 -- open-addressing insert of (luKey -> lpPtr) with linear probing.
        bool InternalAdd(u64 luKey, Collection* lpPtr);

        // X360 0x82806A28 -- table-invariant maintenance after a remove vacates a slot.
        u32 UpdateSearchLength(u32 luFreedSlot, u32 luHomeSlot);

        // Attrib::Class::TablePolicy::GrowRequest -- the next-larger bucket count
        // ((20 * n) / 16 + 3) & ~3, or 1 when n == 0. Declared here; bodied in
        // vechashmap.cpp with the TablePolicy sizing helper.
        u32 GrowRequest(u32 luCurrentEntries) const;

        Node* mTable;          // +0x00
        u16   mTableSize;      // +0x04
        u16   mNumEntries;     // +0x06
        u16   mFixedAlloc;     // +0x08
        u16   mWorstCollision; // +0x0A
    };

    // Pin the pointer-free layout facts (X360-attested field offsets); see the note on
    // the <0,16> Node above -- the X360 node is 16 bytes (32-bit mPtr), so we pin the
    // width-stable offsets rather than the host sizeof.
    static_assert(offsetof(CollectionHashMap::Node, mKey) == 0,
                  "CollectionHashMap Node mKey must sit at +0 (X360 std/ld doubleword)");
    static_assert(offsetof(CollectionHashMap::Node, mPtr) == 8,
                  "CollectionHashMap Node mPtr must sit at +8 (X360 self-pointer sentinel slot)");

    // -------------------------------------------------------------------------
    // Attrib::Class -- the two collection-table wrappers off the X360 spine.
    // -------------------------------------------------------------------------
    // A Class is the {key, privates} handle onto an Attrib::ClassPrivate. The two
    // ledger functions homed here both drive the class's collection table
    // (ClassPrivate::mCollections == the VecHashMap<...Collection...,true,96u>
    // instantiation, reached at mpPrivates+0x1C). Only these two members are attested
    // as real X360 functions off this spine; the rest of the Class API is inlined away
    // at its call sites, so -- like the sibling attribinstance.h reconstruction -- this
    // is the minimal layout the recovered bodies need rather than the full SDK class.
    //
    // Layout by X360 byte offset (attribclassprivate.h agrees: mKey@+0, mpPrivates@+8):
    //   +0x00  mKey        : Attrib::Key  (class key)
    //   +0x08  mpPrivates  : ClassPrivate*  (owns mCollections @ +0x1C)
    class Class
    {
    public:
        // @ 0x82807DD0 -- look up the collection stored under luKey; when the key is
        // absent, fall back to the class's default collection (the on-spine literal
        // default key). Returns NULL only when neither is present.
        Collection* GetCollectionWithDefault(u64 luKey) const;

        // @ 0x8280ADD8 -- remove lpCollection (located by its own 64-bit key at
        // collection+0x10) from this class's collection table. Returns true iff a live
        // bucket was actually vacated.
        bool RemoveCollection(Collection* lpCollection);

    private:
        // The class's default-collection key, materialised as a literal immediate in the X360 asm
        // (0x82807DF8-E10): r4 low = ori(lis 0x2D7D, 0x2152) = 0x2D7D2152; then insrdi r4,r10,32,0
        // inserts r10's low dword 0xD7EDBD36 into r4's HIGH half -> 0xD7EDBD36_2D7D2152.
        static const u64 KU_DEFAULT_COLLECTION_KEY = 0xD7EDBD362D7D2152ull;

        // ClassPrivate::mCollections lives at mpPrivates+0x1C (X360 lwz 8(this) + addi 0x1C).
        CollectionHashMap* GetCollectionTable() const
        {
            return reinterpret_cast<CollectionHashMap*>(
                reinterpret_cast<u8*>(mpPrivates) + 0x1C);
        }

        Key   mKey;        // +0x00  class key
        u32   muPad0;      // +0x04
        void* mpPrivates;  // +0x08  ClassPrivate* (owns mCollections @ +0x1C)
    };
}
