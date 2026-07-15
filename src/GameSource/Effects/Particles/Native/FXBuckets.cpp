// ============================================================================
// GameSource/Effects/Particles/Native/FXBuckets.cpp
//
// BrnParticle::FXBucketManager::Construct -- carve the supplied byte budget into
// fixed-size (KU_FXBUCKET_SIZE = 8192) particle buckets and thread them into an
// intrusive doubly-linked free list.
//
// Reconstructed store-for-store from the X360 ARTIST asm @ 0x8227B178 (32-bit ABI;
// stw = 4-byte stores). r31 = this (a1), r30 = luTotalSize (a3), r4 = lpHeapMalloc (a2):
//
//   r3 = HeapMalloc::Malloc(a2, a3, 128)   ; 128-byte-aligned block of luTotalSize bytes
//   stw r3, 4(this)                        ; mpData = block
//   if (!block) { CGS_ASSERT("mpData != NULL", ...FXBuckets.cpp, 76) }
//   r11 = mpData                           ; r11 = first bucket
//   r7  = luTotalSize >> 13                ; muNumBuckets = totalSize / 8192
//   stw r11, 0(this)                       ; mpFreeList = first bucket
//   stw r6(=0), 0(r11)                     ; firstBucket->mpPreviousBucket = NULL
//   if (numBuckets > 1) {                  ; ble cr6 skips when <= 1
//     r9 = 0x2000 (running stride)
//     r10 = numBuckets - 1                 ; loop count (then pre-decremented)
//     do {
//       --r10
//       r8 = 0x2000 + mpData               ; next bucket = block + stride
//       r9 += 0x2000
//       stw r8, 4(r11)                     ; cur->mpNextBucket = next
//       stw r11, 0(r8)                     ; next->mpPreviousBucket = cur
//       r11 = cur->mpNextBucket            ; advance
//     } while (r10 != 0)
//   }
//   stw r6(=0), 4(r11)                     ; lastBucket->mpNextBucket = NULL
//   stw r7, 8(this)                        ; muNumFreeBuckets = numBuckets
//   stw r7, 0xC(this)                      ; muNumBuckets     = numBuckets
//
// X360 pointers are 32-bit; on the host the link words widen, so the absolute 0x2000
// stride is reproduced as a typed byte step over the block (each bucket is
// KU_FXBUCKET_SIZE bytes). The list SHAPE and the prev/next wiring SEQUENCE are exact.
// ============================================================================

#include "GameSource/Effects/Particles/Native/FXBuckets.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"  // BrnDebris  -> FXBucket<BrnDebris,32>
#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"// BrnSpark   -> FXBucket<BrnSpark,4>
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h" // CgsMemory::HeapMalloc::Malloc
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace BrnParticle
{
    // ------------------------------------------------------------------------
    // Explicit instantiations of the FXBucketManager::AllocateBucket<TBucket> member
    // (body inline in FXBuckets.h). These are the two allocator TUs the X360 build emits:
    //   AllocateBucket<FXBucket<BrnDebris,32>> @ 0x8228E3F0
    //       -- caller: BrnParticle::Native::BrnDebrisArray::GetNewDebris
    //   AllocateBucket<FXBucket<BrnSpark,4>>   @ 0x8228E470
    //       -- caller: BrnParticle::Native::SparkArray::SparkBank::GetNewSpark
    // ------------------------------------------------------------------------
    template BrnParticle::FXBucket<BrnParticle::Native::BrnDebris, 32>*
        FXBucketManager::AllocateBucket<BrnParticle::FXBucket<BrnParticle::Native::BrnDebris, 32> >();
    template BrnParticle::FXBucket<BrnParticle::Native::BrnSpark, 4>*
        FXBucketManager::AllocateBucket<BrnParticle::FXBucket<BrnParticle::Native::BrnSpark, 4> >();

    void FXBucketManager::Construct(CgsMemory::HeapMalloc* lpHeapMalloc, u32 luTotalSize)
    {
        // 128-byte-aligned block holding the whole bucket budget.
        mpData = static_cast<u8*>(lpHeapMalloc->Malloc(static_cast<s32>(luTotalSize), 128));
        CGS_ASSERT(mpData != NULL, "mpData != NULL");

        FXBucketBase* lpCurrentBucket = reinterpret_cast<FXBucketBase*>(mpData); // r11 = first bucket
        const u32 luNumBuckets = luTotalSize >> 13; // totalSize / 8192

        mpFreeList = lpCurrentBucket;             // mpFreeList = first bucket
        lpCurrentBucket->mpPreviousBucket = NULL; // first->prev = NULL

        if (luNumBuckets > 1)
        {
            u32 luStride = KU_FXBUCKET_SIZE; // r9 = 0x2000
            u32 luRemaining = luNumBuckets - 1;
            do
            {
                --luRemaining;
                FXBucketBase* lpNextBucket = reinterpret_cast<FXBucketBase*>(mpData + luStride);
                luStride += KU_FXBUCKET_SIZE;
                lpCurrentBucket->mpNextBucket = lpNextBucket; // cur->next = next
                lpNextBucket->mpPreviousBucket = lpCurrentBucket; // next->prev = cur
                lpCurrentBucket = lpCurrentBucket->mpNextBucket;  // advance
            }
            while (luRemaining);
        }

        lpCurrentBucket->mpNextBucket = NULL; // last->next = NULL
        muNumFreeBuckets = luNumBuckets;
        muNumBuckets = luNumBuckets;
    }
}
