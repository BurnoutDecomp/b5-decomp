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
// HONEST PLACEHOLDERS (flagged for proper homing -- see ParticleBucket.cpp
// dep_flags): cVector, cMatrix, cTime, sParticleNucleus and cParticleEmitter are
// vendor math / Lion runtime types not yet homed in the project. (cParticleRandomSeed
// is now homed -- ParticleRandomSeed.h -- and pulled in by include, not forked here.) They are modelled here only as far as this TU's runtime touches them
// (correct element STRIDE for the indexed arrays; opaque-but-named otherwise).
// NOTE: a sibling Lion home (ParticleBehaviour.h) also defines HONEST-PLACEHOLDER
// cVector/cColour8. To avoid a cross-header ODR clash this header does NOT include
// ParticleBehaviour.h; if the two are ever pulled into one TU, both placeholders
// must be replaced by the single real cVector home (grow additively there).
// ============================================================================

#include "types.hpp"

// Real home for the per-bucket RNG seed snapshot (mRandomSeed). Its 0x34-byte size
// exactly fills the DWARF-attested gap between mRandomSeed@0x1C and the next member
// mnNextParticlePositionToFill@0x50 -- so this is the type's owning header, not a fork.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRandomSeed.h"

class cParticleEmitter;         // owning emitter (chained list head) -- opaque here
struct cParticleBucketManager;  // pool owner (ParticleBucketManager.h) -- befriended below

// ----------------------------------------------------------------------------
// HONEST PLACEHOLDER: 4-lane vector. AllocateParticle only computes the address
// &mpVectors[count]; the X360 stride is 16 bytes (slwi r9,r9,4), so a 4xf32 lane
// struct is the faithful element size. Replace with the real cVector home.
// ----------------------------------------------------------------------------
struct cVector
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};

// ----------------------------------------------------------------------------
// HONEST PLACEHOLDER: 4x4 row-major matrix. AllocateParticle only computes the
// address &mpMatrices[count]; the X360 stride is 64 bytes (slwi r10,r10,6), so a
// 16xf32 element is the faithful size. Replace with the real cMatrix home.
// ----------------------------------------------------------------------------
struct cMatrix
{
    f32 m[16];
};

// ----------------------------------------------------------------------------
// HONEST PLACEHOLDER: monotonic game time stamp (mLatestBirthTime). Not touched
// by this TU's bodied function; modelled opaque (8 bytes) for shape only.
// ----------------------------------------------------------------------------
struct cTime
{
    u64 muTicks;
};

// cParticleRandomSeed (mRandomSeed) now comes from its real home,
// ParticleRandomSeed.h (included above); the former opaque placeholder is retired.

// ----------------------------------------------------------------------------
// HONEST PLACEHOLDER: one particle "nucleus" (the per-particle simulation state).
// AllocateParticle returns &mParticles[count]; the X360 element stride is 0xE0
// (224) bytes (mulli r10,r10,0xE0). The internals are not touched by this TU, so
// the slot is modelled as an opaque 224-byte blob with the correct STRIDE. The
// 0x60 array base + 0xE0 stride together place the array tail (mpMatrices) at the
// asm-verified a1[920]. Replace with the real sParticleNucleus home.
// ----------------------------------------------------------------------------
struct sParticleNucleus
{
    u8 mau8Opaque[0xE0];
};

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
