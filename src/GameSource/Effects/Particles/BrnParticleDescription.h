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
// only the string pointer -- so it imposes no member layout on ParticleDescription.
//
// ⭐ THE RECORD, added 2026-09-03 (boost-exhaust wave). Two handlers shape it and nothing
// else does:
//   ParticleDescriptionResourceType::Serialise   @0x8267C220
//        lpDst[0] = lpSrc[0]                      -- the effect-name hash
//        lpDst[1] = (u32)lpDst + 16               -- the saved LION blob, which BinSave
//                                                    then writes at +16
//   ParticleDescriptionResourceType::DeSerialise @0x82675868
//        *(res + 4) = cLionFX::BinLoad(*(res + 4))  -- the blob pointer is replaced,
//                                                      in place, by BinLoad's return
// and the consumer confirms both: ParticleModule::StartLionEffect @0x82289F50 matches its
// argument against entry->word0 and then reads `*(entry->word1 + 0x48)`, which is
// cLionEffectDefinition::mpParticles. So word 0 is the hash and word 1 is the definition.
//
//   +0x00  u32  muNameHash      -- ParticleDescription::HashString(gdb path)
//   +0x04  u32  muDefinition    -- the .lef blob: the file holds the OFFSET 16, FixUp
//                                  rebases it to `this + 16`, DeSerialise then replaces it
//                                  with BinLoad's return
//   +0x08 .. +0x0F              -- the 16-byte header's tail (zero in every shipped
//                                  resource; Serialise never writes it -- the 16 comes
//                                  from GetSerialisedResourceDescriptor's alignment)
//   +0x10  the cLionEffectDefinition itself
//
// ⛔⛔ BOTH WORDS ARE SERIALISED 32-BIT SLOTS AND MUST STAY u32. Modelling the second as a
// host `cLionEffectDefinition*` is wrong twice over, and the first attempt at this file did
// exactly that: an 8-byte member forces 4 bytes of ALIGNMENT PADDING after muNameHash, so
// the definition slot lands at host +0x08 -- reading the zero pad word instead of the
// offset, which made cLionFX::BinLoad see a null and reject all 41 descriptions with a
// message that blamed the byte order. No compile gate can see that; only a run can. Same
// rule as every other serialised slot in this project: keep the width, convert at the
// accessor through the low-4 GB reservation.
// GROW this class additively as further ParticleDescription TUs land; do not turn
// HashString non-static.
// ============================================================================

#include "types.hpp"

struct cLionEffectDefinition;   // SDKs/.../LionRuntime/include/LionEffect.h (pointer-only)

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

        // The definition slot as a pointer. Valid only after FixUp has rebased it and
        // DeSerialise has run BinLoad over it; before FixUp the slot holds the file
        // offset 16, which is why nothing may read it earlier.
        cLionEffectDefinition* GetDefinition() const
        {
            return reinterpret_cast<cLionEffectDefinition*>(static_cast<uintptr_t>(muDefinition));
        }
        void SetDefinition(const void* lpDefinition)
        {
            muDefinition = static_cast<u32>(reinterpret_cast<uintptr_t>(lpDefinition));
        }

        u32 muNameHash;     // +0x00
        u32 muDefinition;   // +0x04  serialised slot -- see the banner
    };
}
