#pragma once

// attribhashmap.h -- Attrib::HashMap, AttribSys's open-addressing attribute hash map
// (AttribSys 1.2.1.2).
//
// HashMap is a flat Bucket[] table probed linearly from (rotl32(key, keyShift) % capacity).
// Each home bucket caches the length of the probe run that homes to it (mu8SearchLen/mMax);
// the table-wide maximum (mu8MaxSearchLength / worst-collision high-water mark) drives the
// search-length repair bookkeeping after a removal and the grow-and-rehash on insert.
//
// Recovered from BURNOUT_X360_ARTIST.XEX. This is the sibling of the VecHashMap
// instantiations in vechashmap.h; it shares the same 16-byte-stride Node/Bucket layout and
// the same self-consistent counter block, but is the plain (non-templated) Attrib::HashMap
// that Attrib::ClassPrivate / attribute collections use for their layout tables. Only the
// byte offsets the recovered bodies touch are pinned; pointer-bearing members are widened
// for the 64-bit host so raw X360 offsets past the first pointer are NOT asserted.
//
// Container layout (X360-attested, mirrors vechashmap.h's 16-bit counter block):
//   mpBuckets          @ 0x00  Bucket*   (the flat bucket array)
//   muCapacity         @ 0x04  u16       (number of buckets == table size)
//   muCount            @ 0x06  u16       (live entries / mNumEntries)
//   muRefCount         @ 0x08  u16       (shared refcount, Release @ +8)
//   mu8MaxSearchLength @ 0x0A  u8        (table-wide worst-collision probe length)
//   mu8KeyShift        @ 0x0B  u8        (rotate-hash shift)
//
// Bucket/Node layout (16-byte stride, X360 slwi ,4):
//   mKey        @ 0x00  u64   (attribute key; free bucket reads back 0)
//   mValue      @ 0x08  (payload word: pointer / laid-out offset / inline value)
//   mTypeIndex  @ 0x0C  u16
//   mMax        @ 0x0E  u8    (this home bucket's cached probe-run length)
//   mFlags      @ 0x0F  u8    (bit7 0x80 = occupied; bit6 0x40 by-value; bit5 0x20
//                              inherited; bit4 0x10 laid-out)

#include <cstddef>   // offsetof, NULL
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace Attrib
{
    // The parent collection an inherited attribute resolves through. Opaque foreign type:
    // Remove() reaches the parent-collection back-reference at +0x18 and its data area by
    // recovered byte offsets (the collection's real layout is homed in attribinstance.h /
    // vechashmap.h); modelled here only as a forward class so the bodies link.
    class Collection;

    // Attrib::HashMap -- open-addressing attribute hash map (X360 non-templated spine).
    class HashMap
    {
    public:
        // One bucket. 16-byte stride (X360 slwi ,4). FREE iff the occupied flag (bit7 of
        // mFlags @ +0xF) is clear; a free bucket reads back key 0.
        struct Node
        {
            u64 mKey;        // +0x00  attribute key
            union
            {
                void* mpValue;   // +0x08  plain pointer payload / by-value inline word
                u32   muOffset;  // +0x08  laid-out / inherited data-area offset (low word)
            };
            u16 mTypeIndex;    // +0x0C  attribute type index
            u8  mu8SearchLen;  // +0x0E  cached probe-run length for this home bucket (aka mMax)
            u8  mFlags;        // +0x0F  bit7 occupied / bit6 by-value / bit5 inherited / bit4 laid-out

            // A bucket is occupied iff the top flag bit is set.
            bool IsOccupied() const { return (mFlags & 0x80) != 0; }
            // The stored key (0 when free), matching the X360 `flags&0x80 ? key : 0` idiom.
            u64  Key() const { return IsOccupied() ? mKey : 0; }
        };

        // Bucket is the recovered spelling the accessor bodies use for a table slot.
        typedef Node Bucket;

        // Alias for the per-home cached probe length the bodies read as mu8SearchLen.
        // (Same byte as Node::mMax @ +0xE.)

        HashMap(unsigned int luCount, u8 lu8KeyShift, u8 lu8Dynamic);       // 0x828094E8

        int          CountSearchCacheLines(u64 luKey, u8 lu8CacheLineShift) const; // 0x828048C8
        Bucket*      Find(u64 luKey) const;                                 // 0x82804838
        unsigned int FindIndex(u64 luKey) const;                           // 0x82804700
        u64          GetKeyAtIndex(u32 luIndex) const;                     // 0x82802660
        unsigned int PreFlightAdd(u64 luKey, unsigned int luStartIndex, int* lpiSteps); // 0x828027A8
        bool         Release() const;                                       // 0x82802718 (mutates mRefCount)
        void*        Remove(Node* lpNode, void* lpBase, const Collection* lpCollection, bool lbRestore); // 0x82807A78
        void         Transfer(Node& lrNode);                                // 0x828049D8
        unsigned int UpdateSearchLength(unsigned int luIndex, unsigned int luFreedIndex); // 0x82804B50

    private:
        // Grow (or first-build) the bucket array to hold luCount buckets and re-home every
        // live entry via Transfer. Own AttribSys TU (not part of this group); declared so the
        // ctor links against the real member.
        void RebuildTable(unsigned int luCount);                           // (own TU)

        Node* mpBuckets;          // +0x00  flat bucket array
        u16   muCapacity;         // +0x04  number of buckets (table size)
        u16   muCount;            // +0x06  live entries
        u16   muRefCount;         // +0x08  shared refcount
        u8    mu8MaxSearchLength; // +0x0A  table-wide worst-collision probe length
        u8    mu8KeyShift;        // +0x0B  rotate-hash shift
    };
}
