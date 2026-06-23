#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"

// Out-of-line body for BrnParticle::Native::BrnDebrisArray::ClearAllBuckets.
//
// Reconstructed store-for-store from the X360 ARTIST asm at 0x8227E3D0. While the used
// list is non-empty, repeatedly take its head bucket, unlink it from the used list
// (fixing up its neighbours' prev/next), then push it onto the bucket pool's free list
// (becoming the new free head, with its prev cleared) and bump the free count. Once the
// used list is empty, zero the live bucket count. The empty-list fast path just zeroes
// the count.

namespace BrnParticle
{
namespace Native
{
    void BrnDebrisArray::ClearAllBuckets()
    {
        if ( mpUsedHead )
        {
            do
            {
                DebrisBucket*     lpBucket = mpUsedHead;
                DebrisBucketPool* lpPool   = mpBucketPool;

                // Advance the used-list head past the bucket being removed.
                mpUsedHead = lpBucket->mpNext;

                // Unlink lpBucket from the used list.
                if ( lpBucket->mpPrev )
                    lpBucket->mpPrev->mpNext = lpBucket->mpNext;
                if ( lpBucket->mpNext )
                    lpBucket->mpNext->mpPrev = lpBucket->mpPrev;

                // Push lpBucket onto the front of the pool's free list.
                lpBucket->mpNext = lpPool->mpFreeHead;
                if ( lpPool->mpFreeHead )
                    lpPool->mpFreeHead->mpPrev = lpBucket;
                lpPool->mpFreeHead = lpBucket;
                lpBucket->mpPrev = nullptr;
                ++lpPool->muFreeCount;
            }
            while ( mpUsedHead );
        }

        muUsedCount = 0;
    }
}
}
