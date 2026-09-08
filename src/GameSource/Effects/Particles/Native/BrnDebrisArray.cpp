#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cmath>                                      // std::floor (de-inlined Floor/IntFloor)

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
    // The five debris parameter presets are DECLARED in BrnDebrisArray.h and DEFINED in
    // BrnDebrisRenderer.cpp, recovered from the console image. Construct indexes the table
    // by EDebrisArrayID.

    namespace
    {
        // Push a bucket onto the FRONT of the intrusive used list. GetNewDebris's two relink
        // arms emit this identical sequence twice over (the compiler inlined the helper at
        // both sites); de-inlined here. The head's own mpPreviousBucket is carried across
        // rather than assumed null -- that is what the console does.
        void LinkBucketAtFront( BrnDebrisArray::DebrisBucket*& lrpHead,
                                BrnDebrisArray::DebrisBucket*  lpBucket )
        {
            lpBucket->mpNextBucket = lrpHead;

            if ( lrpHead )
            {
                lpBucket->mpPreviousBucket = lrpHead->mpPreviousBucket;
                if ( lrpHead->mpPreviousBucket )
                    lrpHead->mpPreviousBucket->mpNextBucket = lpBucket;
                lrpHead->mpPreviousBucket = lpBucket;
            }
            else
            {
                lpBucket->mpPreviousBucket = nullptr;
            }

            lrpHead = lpBucket;
        }
    }

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

    // ------------------------------------------------------------------------
    // BrnDebrisArray::GetNewDebris
    //
    // Claim the next free debris slot. Three arms, in the console's own order:
    //   (a) the tail bucket still has room  -> use it;
    //   (b) the array is still under its particle budget -> pull a fresh bucket off the
    //       pool free list and push it onto the FRONT of the used list, ++muNumBuckets;
    //   (c) the budget is spent -> walk to the OLDEST bucket (the used list's tail),
    //       unlink it, re-push it at the front and rewind its write cursor. Note what the
    //       console does NOT do here: it neither Clear()s the recycled bucket nor touches
    //       muNumBuckets / mu16NumberOfParticlesInBucket, so the recycled bucket keeps its
    //       live count and simply overwrites its own oldest particles.
    // Returns NULL only when the pool itself is exhausted (arm (b) with no free bucket, or
    // arm (c) with no used bucket at all).
    //
    // The "still has room" test and the budget test are both against the bucket's OWN
    // capacity -- the FXBucket<BrnDebris,32> element count, NOT the template's `32`. That
    // capacity is derived from the 8192-byte bucket and sizeof(BrnDebris) and comes out at
    // 96, which is exactly the constant the console tests the write cursor against and the
    // factor it multiplies muNumBuckets by. The static_assert below re-derives it on the host,
    // so a change to BrnDebris's layout cannot silently move the budget.
    // ------------------------------------------------------------------------
    BrnDebris* BrnDebrisArray::GetNewDebris(f32 lfBirthTime)
    {
        static_assert( DebrisBucket::KuMaxNumParticles == 96,
                       "the debris bucket holds 96 particles (the console's own immediate)" );

        DebrisBucket* lpBucket = mpBuckets;

        if ( !lpBucket ||
             ( lpBucket->mu16NextPositionInBucket >= DebrisBucket::KuMaxNumParticles ) )
        {
            const u32 luParticleCapacity = DebrisBucket::KuMaxNumParticles * muNumBuckets;

            if ( luParticleCapacity < static_cast<u32>( mpParams->mnNumParticles ) )
            {
                // (b) Room in the budget: take a fresh bucket from the pool.
                DebrisBucket* lpNewBucket =
                    mpBucketManager->AllocateBucket<DebrisBucket>();
                if ( !lpNewBucket )
                    return nullptr;

                LinkBucketAtFront( mpBuckets, lpNewBucket );
                ++muNumBuckets;
                lpBucket = lpNewBucket;
            }
            else
            {
                // (c) Budget spent: recycle the oldest bucket, which is the list's tail.
                if ( !lpBucket )
                    return nullptr;

                DebrisBucket* lpOldestBucket = lpBucket;
                while ( lpOldestBucket->mpNextBucket )
                    lpOldestBucket = static_cast<DebrisBucket*>( lpOldestBucket->mpNextBucket );

                if ( lpOldestBucket->mpPreviousBucket )
                    lpOldestBucket->mpPreviousBucket->mpNextBucket = lpOldestBucket->mpNextBucket;
                if ( lpOldestBucket->mpNextBucket )
                    lpOldestBucket->mpNextBucket->mpPreviousBucket = lpOldestBucket->mpPreviousBucket;

                lpOldestBucket->mpNextBucket     = nullptr;
                lpOldestBucket->mpPreviousBucket = nullptr;

                LinkBucketAtFront( mpBuckets, lpOldestBucket );

                // Rewind the write cursor only: the live count is deliberately left alone.
                lpOldestBucket->mu16NextPositionInBucket = 0;
                lpBucket = lpOldestBucket;
            }
        }

        return lpBucket->GetNewParticle( lfBirthTime );
    }

    // ------------------------------------------------------------------------
    // BrnDebrisArray::SpawnDebris @ 0x82294DC8
    //
    // The X360 body passes the four vector arguments in v1..v4 and the three scalars in f1..f3,
    // forwards the spawn time to GetNewDebris, and (when a slot is available) writes the packed
    // Vector3Plus lanes through the SIMD scratch-swap idiom the compiler emitted for
    // SetVector3()/SetPlus(). Reproduced here as the equivalent per-lane assignments:
    //   mPositionPlusRotVel = (lPos,             w = lrRotationalVelocity)
    //   mAxisPlusAngle      = (lRotationAxis,    w = mPositionPlusRotVel.w)   [SetPlus<VectorAxisW>]
    //   mVelocityPlusScale  = (lLinearVelocity,  w = lrSizeScale)
    //   mDiffuseColour      = lDiffuseColour * (df, df, df, 1)   df = mpParams->mfDiffuseFactor
    // The bounce count hashes the two scalar spawn params into a seed, takes the fractional part
    // of its square, maps that onto the bounce range and floors it to an integer.
    // ------------------------------------------------------------------------
    void BrnDebrisArray::SpawnDebris(rw::math::vpu::Vector3 lPos,
                                     rw::math::vpu::Vector3 lLinearVelocity,
                                     rw::math::vpu::Vector3 lRotationAxis,
                                     rw::math::vpu::Vector4 lDiffuseColour,
                                     f32 lrRotationalVelocity,
                                     f32 lrSizeScale,
                                     f32 lrSpawnTime)
    {
        BrnDebris* lpNewDebris = GetNewDebris( lrSpawnTime );
        if ( !lpNewDebris )
            return;

        // Position (xyz) + rotational velocity (w).
        lpNewDebris->mPositionPlusRotVel.SetVector3( lPos );
        lpNewDebris->mPositionPlusRotVel.SetPlus( lrRotationalVelocity );

        // Rotation axis (xyz); the angle (w) lane is seeded from the rotational velocity just
        // written -- X360 SetPlus<VectorAxisW>( mPositionPlusRotVel ).
        lpNewDebris->mAxisPlusAngle.SetVector3( lRotationAxis );
        lpNewDebris->mAxisPlusAngle.SetPlus( lpNewDebris->mPositionPlusRotVel.GetPlus() );

        // Linear velocity (xyz) + size scale (w).
        lpNewDebris->mVelocityPlusScale.SetVector3( lLinearVelocity );
        lpNewDebris->mVelocityPlusScale.SetPlus( lrSizeScale );

        // Diffuse colour tinted by the array's diffuse factor: rgb *= mfDiffuseFactor, a *= 1.
        const f32 lfDiffuseFactor = mpParams->mfDiffuseFactor;
        lpNewDebris->mDiffuseColour.x = lDiffuseColour.x * lfDiffuseFactor;
        lpNewDebris->mDiffuseColour.y = lDiffuseColour.y * lfDiffuseFactor;
        lpNewDebris->mDiffuseColour.z = lDiffuseColour.z * lfDiffuseFactor;
        lpNewDebris->mDiffuseColour.w = lDiffuseColour.w * 1.0f;

        // Pseudo-random bounce count: seed = (rotVel + 2.4679999)*(sizeScale + 1.357); take the
        // fractional part of seed^2, scale onto the bounce range (span 3.0, min 2.5), and floor.
        const f32 lfSeed        = ( lrRotationalVelocity + 2.4679999f ) * ( lrSizeScale + 1.357f );
        const f32 lfSeedSquared = lfSeed * lfSeed;
        const f32 lfFraction    = lfSeedSquared - static_cast<f32>( std::floor( lfSeedSquared ) );
        const f32 lfBounceCount = lfFraction * 3.0f + 2.5f;

        lpNewDebris->muBounceCount =
            static_cast<u8>( static_cast<s32>( std::floor( lfBounceCount ) ) );
    }
}
}
