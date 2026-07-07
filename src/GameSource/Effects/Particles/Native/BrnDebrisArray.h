#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnDebrisArray.h
//
// BrnParticle::Native::BrnDebrisArray -- the platform ("Native") debris-bucket array
// owned by the particle module. Live debris buckets hang off an intrusive doubly-linked
// "used" list threaded through the FXBucketBase node of each bucket; freed buckets are
// pushed back onto the shared FXBucketManager free list.
//
// LAYOUT AUTHORITY: DecFIGS DWARF (BrnDebrisRenderer.h:115) names the members and their
// order; the X360 ARTIST asm pins the byte displacements this build touches:
//   Construct           @ 0x8227A3D0  (stores mpParams/mpBucketManager, zeroes list)
//   FreeExpiredBuckets  @ 0x82281CE0  (walks used list, recycles expired buckets)
//   ClearAllBuckets     @ 0x8227E3D0  (drains the whole used list)
//
//   this[0] @ +0x00 -- mpParams        : const BrnDebrisArrayParams*  (&_gaDebrisArrayParams[type])
//   this[1] @ +0x04 -- mpBuckets       : head of the used (live) bucket list
//   this[2] @ +0x08 -- mpBucketManager : shared bucket pool / free list owner
//   this[3] @ +0x0C -- muNumBuckets    : live bucket count
//   then     mTexture / mMeshCollection : the acquired render resources (not touched by
//                                         the three list-management bodies reconstructed here)
//
//   Each bucket is a BrnParticle::FXBucket<BrnDebris,32>; the list management touches only
//   its FXBucketBase head (mpPreviousBucket @ +0x00, mpNextBucket @ +0x04,
//   mfFinalParticleBirthTime @ +0x08). The free list lives on the FXBucketManager
//   (mpFreeList @ +0x00, muNumFreeBuckets @ +0x08).
//
// X360 pointers are 32-bit; on the 64-bit host they widen so the absolute byte offsets
// above do NOT hold. The load-bearing facts reproduced are the member SET/ORDER and the
// intrusive unlink-then-push SEQUENCE. Members are pinned BY NAME. GROW additively.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"                                   // Vector3 / Vector4 / Vector3Plus
#include "GameSource/Effects/Particles/Native/FXBuckets.h"       // FXBucket / FXBucketManager / FXBucketBase
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::SafeResourceHandle

namespace renderengine { class Texture; }

namespace BrnParticle
{
    class BrnVFXMeshCollection;   // resource type held by mMeshCollection

namespace Native
{
    // BrnDebrisRenderer.h:67 (DWARF) -- selects one of the five debris parameter presets.
    enum EDebrisArrayID
    {
        eDebrisArray_Coloured   = 0,
        eDebrisArray_Shiny      = 1,
        eDebrisArray_Dark       = 2,
        eDebrisArray_HighDetail = 3,
        eDebrisArray_Glass      = 4,
        eDebrisArray_Max        = 5,
    };

    // BrnDebrisRenderer.h:79 (DWARF) -- one debris particle. Three Vector3Plus registers
    // pack a vec3 + a scalar in the w ("plus") lane, followed by the diffuse colour and a
    // bounce counter. Offsets confirmed by SpawnDebris @ 0x82294DC8 (mDiffuseColour @ +0x30,
    // mVelocityPlusScale @ +0x20, mPositionPlusRotVel @ +0x10, muBounceCount @ +0x40).
    struct BrnDebris
    {
        rw::math::vpu::Vector3Plus mAxisPlusAngle;       // +0x00 -- rotation axis (xyz) + angle (w)
        rw::math::vpu::Vector3Plus mPositionPlusRotVel;  // +0x10 -- position (xyz) + rotational velocity (w)
        rw::math::vpu::Vector3Plus mVelocityPlusScale;   // +0x20 -- linear velocity (xyz) + size scale (w)
        rw::math::vpu::Vector4     mDiffuseColour;       // +0x30
        u8                         muBounceCount;        // +0x40
    };

    // BrnDebrisRenderer.h:92 (DWARF) -- per-array immutable parameters. sizeof == 0x50 (80)
    // is pinned by Construct's stride (a3*5<<4 == a3*80). The Vector4/Vector3 members are
    // 16-aligned, which forces the 8-byte pad after mfAmbientFactor so mColour lands at +0x20.
    struct BrnDebrisArrayParams
    {
        const char*            mpMeshCollectionName; // +0x00
        s32                    mnNumParticles;       // +0x04
        f32                    mfSpecularPower;      // +0x08
        f32                    mfSpecularIntensity;  // +0x0C
        f32                    mfDiffuseFactor;      // +0x10  (SpawnDebris reads *(params+16))
        f32                    mfAmbientFactor;      // +0x14
        rw::math::vpu::Vector4 mColour;              // +0x20
        rw::math::vpu::Vector3 mvBounciness;         // +0x30
        f32                    mfFadeInTime;         // +0x40
        f32                    mfDragResistance;     // +0x44
    };

    class BrnDebrisArray
    {
    public:
        // BrnParticle::Native::BrnDebrisArray::DebrisBucket -- one fixed-capacity bucket of
        // debris particles (DWARF BrnDebrisRenderer.h:111). The intrusive list links live in
        // its FXBucketBase head.
        typedef BrnParticle::FXBucket<BrnDebris, 32> DebrisBucket;

        // BrnParticle::Native::BrnDebrisArray::Construct @ 0x8227A3D0. Point the array at its
        // parameter preset (&_gaDebrisArrayParams[leDebrisType]), latch the shared bucket
        // manager, and start with an empty used list. Caller: ParticleModule::Prepare.
        void Construct(FXBucketManager* lpBucketManager, EDebrisArrayID leDebrisType);

        // BrnParticle::Native::BrnDebrisArray::FreeExpiredBuckets @ 0x82281CE0. Walk the used
        // list and recycle every bucket whose final particle birth time has fallen past the
        // cut-off (currentTime - maxLifetime; maxLifetime is 10s at 30Hz, 2s otherwise).
        // Caller: ParticleModule::BeginSimulateDebris.
        void FreeExpiredBuckets(f32 lfCurrentTime, bool lbIsRunningAt30Hz);

        // BrnParticle::Native::BrnDebrisArray::ClearAllBuckets @ 0x8227E3D0. Drain every live
        // bucket back onto the pool free list. Caller: ParticleModule::ProcessEventQueue.
        void ClearAllBuckets();

        // BrnParticle::Native::BrnDebrisArray::SpawnDebris @ 0x82294DC8. Claim a fresh debris
        // particle (GetNewDebris) and fill it in: position + rotational velocity, rotation axis +
        // angle, linear velocity + size scale, diffuse colour tinted by the array's diffuse
        // factor, and a pseudo-randomly seeded bounce count. No-op if the pool is exhausted.
        // Callers: ParticleModule::{HandleFireDebrisBurstEvent, ProcessEventQueue}.
        void SpawnDebris(rw::math::vpu::Vector3 lPos,
                         rw::math::vpu::Vector3 lLinearVelocity,
                         rw::math::vpu::Vector3 lRotationAxis,
                         rw::math::vpu::Vector4 lDiffuseColour,
                         f32 lrRotationalVelocity,
                         f32 lrSizeScale,
                         f32 lrSpawnTime);

    private:
        // BrnParticle::Native::BrnDebrisArray::GetNewDebris (BrnDebrisRenderer.cpp:250). Claim the
        // next free debris slot from the tail bucket, threading in a fresh bucket from the pool
        // when the current one fills up. Reconstructed in a later pass; declared here so
        // SpawnDebris can dispatch to it.
        BrnDebris* GetNewDebris(f32 lfBirthTime);

        const BrnDebrisArrayParams*                            mpParams;        // this[0] @ +0x00
        DebrisBucket*                                          mpBuckets;       // this[1] @ +0x04
        FXBucketManager*                                       mpBucketManager; // this[2] @ +0x08
        u32                                                    muNumBuckets;    // this[3] @ +0x0C
        CgsResource::SafeResourceHandle<renderengine::Texture> mTexture;        // acquired texture
        CgsResource::SafeResourceHandle<BrnVFXMeshCollection>  mMeshCollection; // acquired mesh collection
    };
}
}
