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
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT (FXBucket template bodies)

namespace CgsMemory { class HeapMalloc; }

namespace BrnParticle
{
    // Each particle bucket is exactly 8192 bytes (KU_FXBUCKET_SIZE); the manager
    // carves the HeapMalloc block into this many fixed-stride buckets.
    const u32 KU_FXBUCKET_SIZE = 8192;

    // FXBuckets.h:58 (DWARF) -- the "never yet born" birth-time sentinel every particle
    // slot is seeded with by FXBucket::Clear. == 315360000.0f (10 * 365 * 24 * 3600 s).
    const f32 KF_NUMBER_OF_SECONDS_IN_TEN_YEARS = 315360000.0f;

    // The intrusive doubly-linked-list node every FXBucket starts with. DWARF-attested
    // (FXBuckets.h:73): the two link words plus the shared per-bucket bookkeeping the
    // FXBucket template's Clear/GetNewParticle touch. The manager's list management uses
    // only the first two link words.
    struct FXBucketBase
    {
        FXBucketBase* mpPreviousBucket;             // FXBuckets.h:85 -- node[0] @ +0x00
        FXBucketBase* mpNextBucket;                 // FXBuckets.h:86 -- node[1] @ +0x04
        f32           mfFinalParticleBirthTime;     // FXBuckets.h:88 -- @ +0x08
        u16           mu16NumberOfParticlesInBucket;// FXBuckets.h:89 -- @ +0x0C
        u16           mu16NextPositionInBucket;     // FXBuckets.h:90 -- @ +0x0E
    };

    // ------------------------------------------------------------------------
    // FXBuckets.h:109 (DWARF) -- FXBucket<TParticleType, N>
    //
    // A single fixed-capacity ring of particles laid out inside one 8192-byte bucket:
    // the FXBucketBase node, then a KuMaxNumParticles-entry birth-time table, then the
    // KuMaxNumParticles-entry particle array. KuMaxNumParticles is derived so the whole
    // bucket (base + table + array) fits inside KU_FXBUCKET_SIZE.
    //
    // The Clear() and GetNewParticle() bodies below are reconstructed store-for-store
    // from the X360 asm of the FXBucket<BrnSpark,4> instantiation (Clear @ 0x822869C8,
    // GetNewParticle @ 0x82285460). They are generic over TParticleType/N; the BrnSpark
    // element stride (48 bytes = sizeof(alignas(16) BrnSpark)) and table base (+0x280)
    // match the attested displacements exactly.
    // ------------------------------------------------------------------------
    template< class TParticleType, u32 N >
    struct FXBucket : public FXBucketBase
    {
        // FXBuckets.h:311/313/316/320 (DWARF) -- capacity derivation.
        static const u32 KuBucketManagementOverhead   = 16;
        static const u32 KuTotalParticleSizeInBytes   = sizeof(TParticleType) + sizeof(f32);
        static const u32 KuMaxNumParticlesUnAligned   =
            (KU_FXBUCKET_SIZE - KuBucketManagementOverhead) / KuTotalParticleSizeInBytes;
        static const u32 KuMaxNumParticles            = KuMaxNumParticlesUnAligned & ~1u;

        f32           maParticleBirthTimes[KuMaxNumParticles]; // FXBuckets.h:326 -- @ +0x10
        TParticleType maParticleData[KuMaxNumParticles];       // FXBuckets.h:329 -- @ +0x280

        // FXBuckets.h:286 -- reset the bucket: seed every birth-time slot to the ten-year
        // sentinel and zero the live/next counters and final-birth-time accumulator.
        // X360 Clear @ 0x822869C8.
        void Clear()
        {
            for ( u32 luIndex = 0; luIndex < KuMaxNumParticles; ++luIndex )
            {
                maParticleBirthTimes[luIndex] = KF_NUMBER_OF_SECONDS_IN_TEN_YEARS;
            }
            mu16NumberOfParticlesInBucket = 0;
            mu16NextPositionInBucket      = 0;
            mfFinalParticleBirthTime      = 0.0f;
        }

        // FXBuckets.h:176 -- claim the next particle slot: record its birth time, advance
        // the next-position and (clamped) live count, fold the birth time into the running
        // final-birth-time minimum, and return the slot's particle. X360 GetNewParticle
        // @ 0x82285460. NB: the running minimum uses fsel(final - birthTime), i.e.
        // final = (final - birthTime >= 0) ? final : birthTime.
        TParticleType* GetNewParticle( f32 lfBirthTime )
        {
            CGS_ASSERT( mu16NextPositionInBucket < KuMaxNumParticles,
                        "mu16NextPositionInBucket < KuMaxNumParticles" );

            const u16 lu16NewParticlePosition = mu16NextPositionInBucket;
            u32       luNewParticleCount      = static_cast<u16>( mu16NumberOfParticlesInBucket + 1 );

            mu16NextPositionInBucket = lu16NewParticlePosition + 1;

            u16 lu16NewCount = static_cast<u16>( luNewParticleCount );
            if ( luNewParticleCount >= KuMaxNumParticles )
                lu16NewCount = static_cast<u16>( KuMaxNumParticles );
            mu16NumberOfParticlesInBucket = lu16NewCount;

            maParticleBirthTimes[lu16NewParticlePosition] = lfBirthTime;

            const f32 lfFinal = mfFinalParticleBirthTime;
            mfFinalParticleBirthTime =
                ( ( lfFinal - lfBirthTime ) >= 0.0f ) ? lfFinal : lfBirthTime;

            TParticleType* lpNewParticle = &maParticleData[lu16NewParticlePosition];
            CGS_ASSERT( ( reinterpret_cast<u8*>( lpNewParticle ) + sizeof( TParticleType ) )
                            <= ( reinterpret_cast<u8*>( this ) + KU_FXBUCKET_SIZE ),
                        "( reinterpret_cast<uint8_t*>( &maParticleData[lu16NewParticlePosition] ) + sizeof(_TParticleType) ) <= ( reinterpret_cast<uint8_t*>( this ) + KU_FXBUCKET_SIZE )" );
            return lpNewParticle;
        }
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

        // X360 FXBucketManager::AllocateBucket<TBucket> @ 0x8228E3F0 (FXBucket<BrnDebris,32>)
        // and @ 0x8228E470 (FXBucket<BrnSpark,4>): pop the head bucket off the intrusive free
        // list, unlink it, Clear() its particle payload, drop the free count and hand it back.
        // Returns NULL when the pool is exhausted. TBucket is the concrete FXBucket<T,N>; the
        // asm dispatches to that instantiation's Clear(). Reconstructed store-for-store:
        //
        //   v2 = mpFreeList; if (!mpFreeList) return 0;      ; bail on empty pool
        //   v3 = mpFreeList;                                 ; v3 == v2 (the popped head)
        //   mpFreeList = v2->mpNextBucket;                   ; advance the head (*a1 = v2[1])
        //   v2->mpNextBucket = 0;                            ; null the popped bucket's next
        //   if (mpFreeList) mpFreeList->mpPreviousBucket = 0;; new head has no prev (**a1 = 0)
        //   ((TBucket*)v2)->Clear();                         ; reset the payload
        //   --muNumFreeBuckets;                              ; --a1[2]
        //   return v3;
        template< class TBucket >
        TBucket* AllocateBucket()
        {
            FXBucketBase* lpBucket = mpFreeList;
            if ( !mpFreeList )
                return NULL;

            FXBucketBase* lpAllocatedBucket = mpFreeList;
            mpFreeList = lpBucket->mpNextBucket;
            lpBucket->mpNextBucket = NULL;
            if ( mpFreeList )
                mpFreeList->mpPreviousBucket = NULL;

            static_cast<TBucket*>( lpBucket )->Clear();
            --muNumFreeBuckets;
            return static_cast<TBucket*>( lpAllocatedBucket );
        }
    };
}
