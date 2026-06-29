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

class cParticleDescriptor;

class cParticleEmitter
{
public:
    // The descriptor this emitter is playing (console +0x1F8). LionParticleRender::Render
    // switches on its render mode to pick the draw shape.
    const cParticleDescriptor* GetDescriptor() const { return mpDescriptor; }

private:
    // Opaque leading simulation state preceding mpDescriptor at console +0x1F8 (504):
    // mBucketsUsed / parent matrices / nuclei / seeds / timing (wiki layout). Reserved here.
    u8 maReserved0[0x1F8];

    cParticleDescriptor* mpDescriptor;   // console +0x1F8 (504)
};
