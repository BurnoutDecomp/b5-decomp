#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucketManager.h
//
// cParticleBucketManager -- the Lion (eauk_lion) particle runtime's pool owner. It
// allocates one fixed array of cParticleBucket (the free/used lists are threaded
// through each bucket's mpManagerNext) plus a shared side-data pool that particle
// buckets carve per-particle cMatrix / cVector storage out of, tracked by a single
// occupancy bitmap.
//
// LAYOUT AUTHORITY: member set/order/types are from the DecFIGS DWARF
// (ParticleBucketManager.h:31, lines 88..97), with every offset the runtime indexes
// verified against the X360 ARTIST asm for the pooled TU
// (AppInit @0x82912E00, AppDeInit @0x82909600, BucketAlloc @0x82913050,
//  Free @0x8290F378, VectorBucketAlloc @0x8290CDD8, AllocateBucket @0x829145A0):
//
//   mBucketCount              @0x00 (a1[0])  -- number of pooled buckets
//   mMatrixBucketCount        @0x04 (a1[1])  -- matrix-bucket capacity of the side pool
//   mVectorBucketCount        @0x08 (a1[2])  -- == 4 * mMatrixBucketCount (256B vs 1024B)
//   mVectorChunkCount         @0x0C (a1[3])  -- 32-bit words in the occupancy bitmap
//   mpVectorBucketActiveBits  @0x10 (a1[4])  -- the occupancy bitmap
//   mpBuckets                 @0x14 (a1[5])  -- the bucket array
//   mpFree                    @0x18 (a1[6])  -- head of the free list (mpManagerNext)
//   mpUsed                    @0x1C (a1[7])  -- head of the used list (mpManagerNext)
//   mpMatrices                @0x20 (a1[8])  -- base of the shared side-data pool
//   mpAllocator               @0x24 (a1[9])  -- the tagged allocator
//
// X360 pointers are 32-bit; on the 64-bit host they widen, so the ABSOLUTE byte
// offsets above do NOT hold on the host. Members are pinned BY NAME and SEQUENCE; the
// load-bearing facts reproduced store-for-store are the indexing patterns (bucket
// stride == sizeof(cParticleBucket); vector-bucket stride 256; matrix-bucket stride
// 1024; the bitmap word/bit derivation).
//
// NOTE on mpMatrices (a1[8]): the DWARF names this the "matrices" pool, but the X360
// VectorBucketAlloc carves 256-byte vector buckets out of the SAME pointer that AppInit
// sizes for matrix buckets (mMatrixBucketCount * 1024). One matrix bucket == four vector
// buckets, so mpVectorBucketActiveBits doubles as the matrix occupancy map (a matrix
// bucket clears a nibble; a vector bucket clears one bit). mpMatrices is therefore the
// single shared side-data pool base; the "matrices" name is kept from the DWARF.
// ============================================================================

#include "types.hpp"

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucket.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

// cVector / cMatrix / cTime come from ParticleBucket.h (the bucket's side-array element
// types and its birth-time stamp).

struct cParticleBucketManager
{
public:
    // Per-particle side-array element counts and the derived pool strides. cVector and
    // cMatrix are format-stable float records (no pointers), so these host sizes equal
    // the X360 ones -- 16 vectors * 16 bytes = 256, 16 matrices * 64 bytes = 1024.
    static const u32 KU_PARTICLES_PER_BUCKET = 16;

    // App lifetime: allocate the bucket array + side pool + occupancy bitmap through the
    // tagged allocator, thread the free list, and clear the used list. X360 @0x82912E00.
    void AppInit(EA::Allocator::ITaggedAllocator* apAllocator,
                 u32 auBucketCount, u32 auMatrixBucketCount);

    // Free the three pool arrays back to the allocator. X360 @0x82909600.
    void AppDeInit();

    // Reserve a bucket (from the free list, else by evicting the best used bucket) and
    // attach the side arrays its descriptor's bucket type requires. X360 @0x829145A0.
    cParticleBucket* AllocateBucket(u32 auCount, const cTime& arTime,
                                    cParticleDescriptor::BucketType aeType);

    // Recycle apBucket: release any side arrays, detach it from the owning emitter, move
    // it from the used list back to the free list. X360 @0x8290F378.
    void Free(cParticleBucket* apBucket);

    // Claim a free vector bucket (256-byte slot) from the shared side pool, marking its
    // bit in mpVectorBucketActiveBits. Returns null when full. X360 @0x8290CDD8.
    cVector* VectorBucketAlloc();

    // Matrix-bucket twin of VectorBucketAlloc (1024-byte slot). Body lives in a sibling
    // TU; declared here because AllocateBucket calls it for the heavyweight bucket type.
    cMatrix* MatrixBucketAlloc();

private:
    // Reserve a raw bucket from the free/used lists (no side arrays), preferring the free
    // list and otherwise evicting the used bucket whose birth time is furthest from
    // arTime (or any idle bucket found first). X360 @0x82913050.
    cParticleBucket* BucketAlloc(u32 auCount, const cTime& arTime);

    // Unlink apBucket from the singly-linked (mpManagerNext) list headed by arpHead. A
    // no-op if apBucket is not on the list. Extracted from the list surgery the X360
    // compiler inlined into BucketAlloc / Free.
    void ListRemove(cParticleBucket*& arpHead, cParticleBucket* apBucket);

    // ----- members (DWARF order; offsets verified against the X360 asm) -----
    u32 mBucketCount;                                   // 0x00  ParticleBucketManager.h:88
    u32 mMatrixBucketCount;                             // 0x04  ParticleBucketManager.h:89
    u32 mVectorBucketCount;                             // 0x08  ParticleBucketManager.h:90
    u32 mVectorChunkCount;                              // 0x0C  ParticleBucketManager.h:91
    u32* mpVectorBucketActiveBits;                      // 0x10  ParticleBucketManager.h:92
    cParticleBucket* mpBuckets;                         // 0x14  ParticleBucketManager.h:93
    cParticleBucket* mpFree;                            // 0x18  ParticleBucketManager.h:94
    cParticleBucket* mpUsed;                            // 0x1C  ParticleBucketManager.h:95
    cMatrix* mpMatrices;                                // 0x20  ParticleBucketManager.h:96
    EA::Allocator::ITaggedAllocator* mpAllocator;       // 0x24  ParticleBucketManager.h:97
};
