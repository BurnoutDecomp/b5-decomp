#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/FXBuckets.h
//
// BrnParticle::FXBucketManager -- owns a single contiguous block of fixed-size
// particle buckets (KU_FXBUCKET_SIZE = 8192 bytes each) and threads them into an
// intrusive doubly-linked free list at Construct time. The particle module hands
// out free buckets and recycles them back as effects come and go.
//
// LAYOUT AUTHORITY (X360 ARTIST asm, FXBucketManager::Construct @ 0x8227B178)
// plus the DecFIGS DWARF (FXBuckets.h:363) which names the four members:
//   this[0] @ +0x00 -- mpFreeList        : head of the free-bucket list (== first bucket)
//   this[1] @ +0x04 -- mpData            : base of the bucket block (HeapMalloc result)
//   this[2] @ +0x08 -- muNumFreeBuckets  : free count, seeded to muNumBuckets
//   this[3] @ +0x0C -- muNumBuckets      : total bucket count == totalSize / 8192
//
// Each bucket begins with the FXBucketBase intrusive node:
//   node[0] @ +0x00 -- mpPreviousBucket
//   node[1] @ +0x04 -- mpNextBucket
// (the asm builds the list by walking the block at a fixed 0x2000 stride, wiring
// each node's prev/next and zero-terminating both ends).
//
// X360 pointers are 32-bit; on the 64-bit host they widen, so the absolute byte
// offsets above do NOT hold. The load-bearing facts reproduced are the member
// SET/ORDER and the Construct free-list-build SEQUENCE. Members are pinned BY NAME.
// GROW additively.
//
// HONEST PLACEHOLDER: the per-bucket FXBucket<Particle,N> payload (birth-time table +
// particle array, declared in the DWARF) is not reconstructed in this pass; only the
// FXBucketBase intrusive node and the FXBucketManager bookkeeping are modelled, which
// is the entire surface Construct touches.
// ============================================================================

#include "types.hpp"

namespace CgsMemory { class HeapMalloc; }

namespace BrnParticle
{
    // Each particle bucket is exactly 8192 bytes (KU_FXBUCKET_SIZE); the manager
    // carves the HeapMalloc block into this many fixed-stride buckets.
    const u32 KU_FXBUCKET_SIZE = 8192;

    // The intrusive doubly-linked-list node every FXBucket starts with. Only the two
    // link words the manager's list management touches are modelled; the bucket payload
    // that follows (birth-time table + particle data) is opaque here -- grow additively.
    struct FXBucketBase
    {
        FXBucketBase* mpPreviousBucket; // node[0] @ +0x00
        FXBucketBase* mpNextBucket;     // node[1] @ +0x04
        // +0x08 .. payload (mfFinalParticleBirthTime, counts, per-N particle tables)
        // omitted: not touched by FXBucketManager::Construct.
    };

    // Manages the bucket block and the free list threaded through it.
    struct FXBucketManager
    {
        FXBucketBase* mpFreeList;   // this[0] @ +0x00
        u8*           mpData;       // this[1] @ +0x04
        u32           muNumFreeBuckets; // this[2] @ +0x08
        u32           muNumBuckets;     // this[3] @ +0x0C

        // X360 FXBucketManager::Construct @ 0x8227B178: allocate the block, then thread
        // every 8192-byte bucket into the free list (prev/next wired, both ends nulled),
        // and seed the free/total counts to the bucket count.
        void Construct(CgsMemory::HeapMalloc* lpHeapMalloc, u32 luTotalSize);
    };
}
