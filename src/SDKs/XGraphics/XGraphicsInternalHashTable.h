#pragma once

// ===========================================================================
// XGRAPHICS::InternalHashTable -- an open-hashing table used by the XGRAPHICS
// shader-microcode compiler in BURNOUT_X360_ARTIST.XEX. It buckets u32 keys by a
// caller-supplied hash function into a power-of-two array of InternalVector
// chains, comparing entries with a caller-supplied comparator. The VRegTable
// (XGRAPHICS::VRegTable::Create / FindOrCreate / RemoveConstant) is the attested
// consumer. This header is the canonical OWNING home for:
//
//     XGRAPHICS::InternalHashTable::Lookup @ 0x82C28BF0
//     XGRAPHICS::InternalHashTable::Insert @ 0x82C28CB0
//     XGRAPHICS::InternalHashTable::Remove @ 0x82C28D70
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm of these three methods. Every store /
// branch is asm-exact; because the reconstruction is by NAMED member (semantic
// parity, not byte-matching), the widened x64 pointers do not preserve the
// literal X360 offsets. `XGRAPHICS` is an X360 graphics-SDK boundary, so its
// identifiers are preserved verbatim per the naming convention.
//
// ATTESTED members (X360 byte offsets; every field is 4-byte-wide on X360 -- only
// the fields these three methods touch are attested):
//   +0x00 muBucketCount -- number of buckets (power of two; mask = count - 1)
//   +0x04 muCount       -- total number of live entries (bumped by Insert)
//   +0x08 mppBuckets    -- array of muBucketCount InternalVector* chains
//   +0x0C mpfnCompare   -- comparator(entry, key); returns 0 when they match
//   +0x10 mpfnHash      -- hash(key) -> bucket-selection hash
//   +0x14 mpArena       -- arena new bucket vectors are carved from
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsArena.h"          // Arena, ArenaAllocPrefixed
#include "SDKs/XGraphics/XGraphicsInternalVector.h" // InternalVector

namespace XGRAPHICS
{

class InternalHashTable
{
public:
    // Comparator returns 0 when `auEntry` matches `auKey` (strcmp-style), non-zero
    // otherwise. Hash maps a key to its bucket-selection hash.
    typedef int (*ComparePtr)(u32 auEntry, u32 auKey);
    typedef u32 (*HashPtr)(u32 auKey);

    // @ 0x82C28BF0 -- find the entry matching `auKey` in its bucket chain and
    // return it; return 0 if the bucket is empty/absent or no entry matches.
    u32 Lookup(u32 auKey);

    // @ 0x82C28CB0 -- insert `auKey`: hash to a bucket (lazily allocating the
    // bucket's InternalVector from the arena), push the key at the front of the
    // chain, rehash-grow the table if that chain now exceeds the bucket count,
    // and bump the live count. Returns a pointer to the stored slot.
    u32* Insert(u32 auKey);

    // @ 0x82C28D70 -- remove the entry matching `auKey` from its bucket chain
    // (shifting the tail down over it). No-op if the bucket is empty/absent or no
    // entry matches.
    void Remove(u32 auKey);

    // @ 0x82C28D58 caller -- rehash into a larger bucket array when a chain grows
    // past the bucket count. Body is owned by a separate InternalHashTable TU and
    // is NOT reconstructed here; declared so Insert compiles/links against it.
    u32* Grow();

    u32             muBucketCount; // +0x00 bucket count (power of two; mask = -1)
    u32             muCount;       // +0x04 total live entries
    InternalVector** mppBuckets;   // +0x08 array of muBucketCount chains
    ComparePtr      mpfnCompare;   // +0x0C entry/key comparator (0 == match)
    HashPtr         mpfnHash;      // +0x10 key hash
    Arena*          mpArena;       // +0x14 arena new bucket chains are carved from
};

} // namespace XGRAPHICS
