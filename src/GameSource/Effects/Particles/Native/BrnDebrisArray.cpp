#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Out-of-line bodies for BrnParticle::Native::BrnDebrisArray, reconstructed store-for-store
// from the X360 ARTIST asm:
//   Construct           @ 0x8227A3D0
//   FreeExpiredBuckets  @ 0x82281CE0
//   ClearAllBuckets     @ 0x8227E3D0
//
// The used bucket list and the pool free list are intrusive doubly-linked lists threaded
// through each bucket's FXBucketBase head (mpPreviousBucket/mpNextBucket) and the
// FXBucketManager (mpFreeList/muNumFreeBuckets); see BrnDebrisArray.h for the layout map.

namespace BrnParticle
{
namespace Native
{
    // File-scope table of the five debris parameter presets (BrnDebrisRenderer.cpp:86,
    // base symbol off_82CDB250). Construct indexes it by EDebrisArrayID. Its contents (mesh
    // names / colours / bounciness / fade+drag) are static rodata not reconstructed in this
    // pass; declared here so Construct can take the address of an entry and read
    // mnNumParticles. Defined by the (future) full BrnDebrisRenderer TU globals recon.
    extern const BrnDebrisArrayParams _gaDebrisArrayParams[eDebrisArray_Max];

    // ------------------------------------------------------------------------
    // BrnDebrisArray::Construct @ 0x8227A3D0
    // ------------------------------------------------------------------------
    void BrnDebrisArray::Construct(FXBucketManager* lpBucketManager, EDebrisArrayID leDebrisType)
    {
        CGS_ASSERT( ( leDebrisType >= 0 ) && ( leDebrisType < eDebrisArray_Max ),
                    "( leDebrisType >= 0 ) && ( leDebrisType < eDebrisArray_Max )" );

        mpParams = &_gaDebrisArrayParams[leDebrisType];

        CGS_ASSERT( mpParams->mnNumParticles > 0, "mpParams->mnNumParticles > 0" );

        mpBucketManager = lpBucketManager;
        mpBuckets       = nullptr;
        muNumBuckets    = 0;
    }

    // ------------------------------------------------------------------------
    // BrnDebrisArray::FreeExpiredBuckets @ 0x82281CE0
    //
    // maxLifetime is 10.0s when running at 30Hz, 2.0s otherwise; a bucket expires once its
    // final particle birth time is no later than (lfCurrentTime - maxLifetime). Each expired
    // bucket is unlinked from the used list and pushed onto the front of the pool free list.
    // ------------------------------------------------------------------------
    void BrnDebrisArray::FreeExpiredBuckets(f32 lfCurrentTime, bool lbIsRunningAt30Hz)
    {
        f32 lfDebrisMaxLifetime = 10.0f;
        if ( !lbIsRunningAt30Hz )
            lfDebrisMaxLifetime = 2.0f;

        const f32 lfCutOffTime = lfCurrentTime - lfDebrisMaxLifetime;

        FXBucketBase* lpBucket = mpBuckets;
        if ( lpBucket )
        {
            do
            {
                FXBucketBase* lpNextBucket = lpBucket->mpNextBucket;

                if ( lpBucket->mfFinalParticleBirthTime <= lfCutOffTime )
                {
                    if ( lpBucket == mpBuckets )
                        mpBuckets = static_cast<DebrisBucket*>( lpNextBucket );

                    FXBucketManager* lpManager = mpBucketManager;

                    // Unlink from the used list.
                    if ( lpBucket->mpPreviousBucket )
                        lpBucket->mpPreviousBucket->mpNextBucket = lpBucket->mpNextBucket;
                    if ( lpBucket->mpNextBucket )
                        lpBucket->mpNextBucket->mpPreviousBucket = lpBucket->mpPreviousBucket;

                    // Push onto the front of the pool free list.
                    lpBucket->mpNextBucket = lpManager->mpFreeList;
                    if ( lpManager->mpFreeList )
                        lpManager->mpFreeList->mpPreviousBucket = lpBucket;
                    lpManager->mpFreeList     = lpBucket;
                    lpBucket->mpPreviousBucket = nullptr;
                    ++lpManager->muNumFreeBuckets;

                    --muNumBuckets;
                }

                lpBucket = lpNextBucket;
            }
            while ( lpBucket );
        }
    }

    // ------------------------------------------------------------------------
    // BrnDebrisArray::ClearAllBuckets @ 0x8227E3D0
    //
    // Drain the entire used list back onto the pool free list, then zero the live count.
    // ------------------------------------------------------------------------
    void BrnDebrisArray::ClearAllBuckets()
    {
        if ( mpBuckets )
        {
            do
            {
                FXBucketBase*    lpBucket  = mpBuckets;
                FXBucketManager* lpManager = mpBucketManager;

                mpBuckets = static_cast<DebrisBucket*>( lpBucket->mpNextBucket );

                if ( lpBucket->mpPreviousBucket )
                    lpBucket->mpPreviousBucket->mpNextBucket = lpBucket->mpNextBucket;
                if ( lpBucket->mpNextBucket )
                    lpBucket->mpNextBucket->mpPreviousBucket = lpBucket->mpPreviousBucket;

                lpBucket->mpNextBucket = lpManager->mpFreeList;
                if ( lpManager->mpFreeList )
                    lpManager->mpFreeList->mpPreviousBucket = lpBucket;
                lpManager->mpFreeList      = lpBucket;
                lpBucket->mpPreviousBucket = nullptr;
                ++lpManager->muNumFreeBuckets;
            }
            while ( mpBuckets );
        }

        muNumBuckets = 0;
    }
}
}
