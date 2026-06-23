#pragma once

// ============================================================================
// GameSource/Effects/Particles/BrnParticleDescription.h
//
// BrnParticle::ParticleDescription -- the named-effect description record the
// particle subsystem looks up by name. Only the static name-hashing helper that the
// effects code uses to key these descriptions is reconstructed in this pass; the full
// description record (its parameter set / behaviour references) is not yet homed.
//
// LAYOUT AUTHORITY: HashString @ 0x82276FF8 is a static function (no `this`) -- it takes
// only the string pointer -- so it imposes no member layout on ParticleDescription. GROW
// this class additively (real members + accessors) as further ParticleDescription TUs
// land; do not turn HashString non-static.
// ============================================================================

#include "types.hpp"

namespace BrnParticle
{
    class ParticleDescription
    {
    public:
        // BrnParticle::ParticleDescription::HashString @ 0x82276FF8. Case-insensitive
        // FNV-1a hash of a NUL-terminated effect name; the effects code precomputes the
        // name key with this before looking a description up. Callers (X360 xrefs)
        // include BrnEffects::EffectsModule and BrnEffects::BrnEffectsGlassManager.
        static u32 HashString( const char* lpcName );
    };
}
