#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h
//
// cVector -- the Lion (eauk_common) 4-lane float vector, in the home the DecFIGS DWARF
// gives it and that three sibling Lion headers already named as "the real cVector home"
// while each carrying its own private copy:
//
//   ParticleBucket.h:67   ParticleBehaviour.h:75   ParticleLocator.h:53
//
// All three copies were token-for-token identical, and each carried the warning that a
// fourth copy -- or one that drifted -- would be an ODR fork that links silently. This
// header retires the fork rather than adding to it: cParticleRender (ParticleRender.h)
// needs cVector for mCamPos/mCamDir, and a fourth private copy would have made
// `#include ParticleBucketManager.h` + `#include ParticleRender.h` in one TU a hard
// redefinition -- which is precisely the wall cParticleRender::EmitterRender is parked on.
//
// LAYOUT AUTHORITY: the X360 ARTIST asm. The stride is 16 bytes wherever a cVector array
// is indexed (cParticleBucket::AllocateParticle @0x82908750 forms &mpVectors[count] with
// `slwi r9,r9,4`), and the 16-byte ALIGNMENT is an asm fact too -- it is what puts
// cParticleBehaviour::mAABBMin at +0x4A0 in Lerp @0x8290B1F8, i.e. what makes the record's
// attested 1216-byte size come out right (see the "sizeof is short by N -- check ALIGNMENT
// first" note in ParticleBehaviour.h).
//
// HONEST PLACEHOLDER STATUS IS UNCHANGED: this is still only as much of eauk_common's
// vector as the reconstructed Lion bodies touch (four named lanes at the attested stride
// and alignment). Grow it additively here -- never re-fork it into a consumer header.
// ============================================================================

#include "types.hpp"

struct alignas(16) cVector
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};
