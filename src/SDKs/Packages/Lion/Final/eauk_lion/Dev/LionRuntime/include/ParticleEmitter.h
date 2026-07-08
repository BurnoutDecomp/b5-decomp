#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h
//
// cParticleEmitter -- a live Lion (eauk_lion) particle emitter instance: the runtime state
// for one playing effect (parent transform, simulation seeds, timing) plus the descriptor /
// bucket / behaviour pointers that drive it.
//
// LAYOUT AUTHORITY: the member offsets are from the burnout.wiki "Particle Description"
// cParticleEmitter table, cross-checked against the X360 ARTIST asm for the one field
// LionParticleRender::Render reads -- mpDescriptor @ console +0x1F8 (lwz r11,0x1F8(emitter)).
// Only that pointer is needed here, so the preceding 0x1F8 bytes of simulation state are an
// explicit reserved span (named members for the rest grow additively when an emitter TU is
// reconstructed). X360 pointers are 32-bit; on the host they widen, so the leading span is a
// console fact, not a host layout assertion. The descriptor is accessed BY NAME via the
// inline GetDescriptor() accessor.
// ============================================================================

#include "types.hpp"

class  cParticleDescriptor;
struct cParticleBucket;
struct cTime;   // monotonic game-time stamp (ParticleBucket.h placeholder home) -- ref only here

class cParticleEmitter
{
public:
    // The descriptor this emitter is playing (console +0x1F8). LionParticleRender::Render
    // switches on its render mode to pick the draw shape.
    const cParticleDescriptor* GetDescriptor() const { return mpDescriptor; }

    // Detach apBucket from this emitter's bucket list. Called by
    // cParticleBucketManager::Free @ 0x8290F378 as it recycles a bucket back to the pool
    // (X360: r3 = emitter, r4 = bucket). Body lives in the cParticleEmitter TU; declared
    // here so the pool manager can compile against the real signature.
    void BucketRemove(cParticleBucket* apBucket);

    // ---- emitter lifecycle / pool-threading surface used by cParticleEmitterManager ----
    // Bodies live in the cParticleEmitter TU (not yet reconstructed); declared here so the
    // manager (AppInit / Register / RegisterSubEmitter / Update) compiles against the real
    // DWARF signatures (ParticleEmitter.h:40/79/100/179 + GetNextEmitter@:60). The X360
    // manager folds SetActiveFlag / SetNext / GetNextEmitter inline; they are re-outlined
    // here as the calls the original source made.

    // Bring the emitter to its initial state for apDescriptor (nullptr while pooling). :40
    void Init(cParticleDescriptor* apDescriptor);

    // Advance one frame; returns non-zero while still alive (0 -> manager unregisters it). :179
    u32 Update(const cTime& arTime);

    // Set the "active" flag word (bit 0 == active). :100
    void SetActiveFlag(u32 aFlag);

    // Manager free/used list link (emitter console +0x204). SetNext writes it; :79
    void SetNext(cParticleEmitter* apNext);
    // GetNextEmitter returns it by reference. :60
    cParticleEmitter*& GetNextEmitter();

private:
    // Opaque leading simulation state preceding mpDescriptor at console +0x1F8 (504):
    // mBucketsUsed / parent matrices / nuclei / seeds / timing (wiki layout). Reserved here.
    u8 maReserved0[0x1F8];

    cParticleDescriptor* mpDescriptor;   // console +0x1F8 (504)
};
