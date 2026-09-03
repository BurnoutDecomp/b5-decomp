#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucket.h
//
// cParticleBucket -- the Lion (eauk_lion) particle runtime "bucket": a small,
// fixed-capacity (16) pool of live particle slots owned by a cParticleEmitter.
// Buckets are chained two ways (mpManagerNext through the manager free/used list,
// mpEmitterNext through the owning emitter's bucket list) and carry the parallel
// arrays the simulation walks per-particle: the nucleus array (mParticles), and
// the optional per-particle matrix / vector side arrays (mpMatrices / mpVectors).
//
// LAYOUT AUTHORITY: member set, order and types are from the DecFIGS DWARF
// (ParticleBucket.h:33 / lines 219..241). The offsets the runtime indexes are
// verified against the X360 asm for AllocateParticle @ 0x82908750:
//
//   mnNextParticlePositionToFill  @0x50  (a1[20]) -- live count / next free slot
//   mActiveBits                   @0x54  (a1[21]) -- 16-bit "slot active" mask
//   mParticles[0]                 @0x60         -- nucleus array, stride 0xE0
//   mpMatrices                    @0xE60 (a1[920]) -- &mParticles[16] tail
//   mpVectors                     @0xE64 (a1[921])
//   sizeof(sParticleNucleus)      == 0xE0 (224)   (mulli r10,r10,0xE0 + addi 0x60)
//   per-particle cVector stride   == 0x10 (16)    (slwi r9,r9,4 over mpVectors)
//   per-particle cMatrix stride   == 0x40 (64)    (slwi r10,r10,6 over mpMatrices)
//
// X360 pointers are 32-bit; on the 64-bit host they widen, so the ABSOLUTE byte
// offsets above do NOT hold on the host. Members are therefore pinned BY NAME and
// SEQUENCE only (never by an absolute-offset static_assert). The load-bearing
// facts reproduced store-for-store are the *indexing pattern*: count in
// mnNextParticlePositionToFill, the active-slot bit set in mActiveBits, the
// nucleus array indexed by count, and the two optional side arrays indexed off
// their base pointers.
//
// ⭐ THE PLACEHOLDER LIST IN THIS BANNER WAS ITSELF GOING STALE, so it is restated as of
// 2026-09-03 rather than left to describe a header that no longer exists. FOUR of the five
// types this header used to fork now come from their real homes and are pulled in by
// #include above: cVector (eauk_common/Maths/Vector.h), cParticleRandomSeed
// (ParticleRandomSeed.h), cMatrix (eauk_common/Maths/Matrix.h) and sParticleNucleus
// (sParticle.h). cParticleEmitter stays an opaque forward declaration (a pointer member
// only), which is not a fork.
// NOTHING IN THIS HEADER IS A FORK ANY MORE: cTime was the last one, and it is homed at
// ext-include/GameStructs/cTime.h as of the same day (it had been modelled here as an
// opaque EIGHT bytes against the console's four -- see that header's banner).
// ⛔ THE OLD NOTE SAYING THIS HEADER "does NOT include ParticleBehaviour.h" TO AVOID AN
// ODR CLASH IS RETIRED WITH THE FORKS IT DESCRIBED -- both headers now name the same
// cVector -- but the include is still not added, because nothing here needs it.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"   // cVector -- the real eauk_common home (fork retired 2026-09-03)

// Real home for the per-bucket RNG seed snapshot (mRandomSeed). Its 0x34-byte size
// exactly fills the DWARF-attested gap between mRandomSeed@0x1C and the next member
// mnNextParticlePositionToFill@0x50 -- so this is the type's owning header, not a fork.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRandomSeed.h"

// Real homes for the two types this record's arrays are made of (both were HONEST
// PLACEHOLDERS in this header until 2026-09-03; see their banners).
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h"   // cMatrix (mpMatrices)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/sParticle.h"  // sParticleNucleus
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/ext-include/GameStructs/cTime.h"   // cTime -- the real home (fork retired 2026-09-03)

class cParticleEmitter;         // owning emitter (chained list head) -- opaque here
struct cParticleBucketManager;  // pool owner (ParticleBucketManager.h) -- befriended below

// cVector now comes from its real home, eauk_common/Maths/Vector.h (included above).
// The private copy that used to live here -- and its "keep the three copies token-for-
// token identical" warning -- are retired: the home exists, so there is one definition.

// cMatrix now comes from its real home, eauk_common/Maths/Matrix.h (included above).
// The private `struct cMatrix { f32 m[16]; }` that used to sit here was one of THREE
// forks of the type (ParticleLocator.h had a second, ParticleRender.h a third), and two
// of them were a hard redefinition of each other -- see the Matrix.h banner.

// cTime now comes from its real home, ext-include/GameStructs/cTime.h (included above).
// The `struct cTime { u64 muTicks; }` that used to sit here was EIGHT bytes against the
// console's four, and it was a silent ODR fork of ParticleTrigger.h's four-byte
// `struct cTime { s32 mi32Ticks; }` -- whose own banner had already reasoned to the right
// width from cParticleTrigger::Update @0x82908808 while this one stayed 8. See the cTime.h
// banner for the four independent attestations that settle it at one S32.

// cParticleRandomSeed (mRandomSeed) now comes from its real home,
// ParticleRandomSeed.h (included above); the former opaque placeholder is retired.

// sParticleNucleus now comes from its real home, sParticle.h (included above): fourteen
// named 16-byte lanes from the DecFIGS DWARF (sParticle.h:22), whose sum is the same 0xE0
// stride the opaque placeholder that used to sit here was built around. The 0x60 array
// base + 0xE0 stride still place the array tail (mpMatrices) at the asm-verified a1[920].

// ParticleBucket.h:33
struct cParticleBucket
{
    // The bucket manager owns the free/used pools and performs the list surgery,
    // per-particle side-array (matrix/vector) allocation and the birth-time eviction
    // over these members (AppInit / BucketAlloc / Free / AllocateBucket). It reads and
    // writes them by name, so it is a friend rather than duplicating accessors.
    friend struct cParticleBucketManager;

public:
    // Maximum live particles per bucket (the `cmplwi r10,0x10` cap on the count).
    static const u32 KU_MAX_PARTICLES = 16;

    // ParticleBucket.h:147. Reserve the next free slot. On success records the new
    // slot index in arSlot, hands back the nucleus pointer and the (optional) per-
    // particle vector / matrix pointers, and bumps the live count. Returns false
    // when the bucket is already full. Bodied store-for-store from the X360
    // AllocateParticle @ 0x82908750.
    //
    // NOTE on the parameter order: the DWARF declares
    //   AllocateParticle(U32&, sParticleNucleus**, cVector**, cMatrix**)
    // and the X360 register order is r3=this, r4=&index, r5=&nucleus, r6=&vector,
    // r7=&matrix. The IDA pseudocode renders the last two as anonymous a4/a5, but
    // the asm proves r6 takes the mpVectors-derived pointer (16B stride) and r7 the
    // mpMatrices-derived pointer (64B stride) -- matching the DWARF cVector** then
    // cMatrix** order exactly.
    bool AllocateParticle(u32& arSlot,
                          sParticleNucleus** appNucleus,
                          cVector** appVector,
                          cMatrix** appMatrix);

    // ParticleBucket.h:81 -- produce the world transform for ONE particle in this bucket,
    // from whichever of the three sources this bucket was allocated with (per-particle matrix
    // / per-particle position / the emitter's locator). X360 @0x8290F188. RECONSTRUCTED
    // (ParticleBucket.cpp).
    void GetpMatrix(u32 auIndex, cMatrix* apMatrix, const cTime& arTime);

    // ParticleBucket.h:84 -- "no free slot left". cParticleEmitter::ParticleInsert
    // @0x829133F8 spells it `cmplwi r11, 0x10 ; beq`: an EQUALITY against the capacity, not
    // a >=. Kept as the console asks it.
    bool IsFull() const { return mnNextParticlePositionToFill == KU_MAX_PARTICLES; }

    // ParticleBucket.h:105 -- record when this bucket's youngest particle was born.
    // cParticleEmitter::ParticleInsert @0x82913454 (`stw r11, 0xC(r31)`) is the writer; the
    // bucket manager's eviction pass is the reader.
    void SetLatestBirthTime(const cTime& arTime) { mLatestBirthTime = arTime; }

private:
    // ----- members (DWARF order; offsets verified against the X360 asm) -----
    cParticleBucket*    mpManagerNext;                // 0x00  ParticleBucket.h:219
    cParticleEmitter*   mpEmitter;                    // 0x04  ParticleBucket.h:222
    cParticleBucket*    mpEmitterNext;                // 0x08  ParticleBucket.h:225
    cTime               mLatestBirthTime;             // 0x0C  ParticleBucket.h:229
    cParticleRandomSeed mRandomSeed;                  // 0x1C  ParticleBucket.h:232
    u32                 mnNextParticlePositionToFill; // 0x50  ParticleBucket.h:235
    u32                 mActiveBits;                  // 0x54  ParticleBucket.h:236
    sParticleNucleus    mParticles[KU_MAX_PARTICLES]; // 0x60  ParticleBucket.h:239
    cMatrix*            mpMatrices;                    // 0xE60 ParticleBucket.h:240
    cVector*            mpVectors;                     // 0xE64 ParticleBucket.h:241
};
