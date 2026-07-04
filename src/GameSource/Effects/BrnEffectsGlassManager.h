#pragma once

// ============================================================================
// GameSource/Effects/BrnEffectsGlassManager.h
//
// BrnEffects::BrnGlassSmashEffect -- one glass-smash VFX slot. The glass manager owns a
// fixed array of KU_MAX_GLASS_EFFECTS of these; each tracks the LION particle effect
// playing for a shattered windscreen, the vehicle it belongs to, and when it ends.
//
// Layout / member names from the DecFIGS DWARF, gated against the X360 ARTIST binary
// (BrnGlassSmashEffect::Reset @0x8228F298):
//   +0x00  mLocalTransform    (Matrix44Affine, 64B)  -- the slot's local placement matrix
//   +0x40  mVehicleID         (EntityId {u32 muValue}) -- owning vehicle handle
//   +0x44  mGlassEffectHandle (u32)                   -- the LION effect handle (or invalid)
//   +0x48  mfEffectEndTime    (f32)                   -- game time the effect stops at
//   +0x4C  mbEffectActive     (bool)                  -- slot in use
// Members are reached BY NAME; the X360 offsets above are documentary. Only Reset is in
// this batch; the rest of the slot/manager surface lands with its own TUs (GROW this home
// then, do NOT fork it).
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                          // Matrix44Affine, EntityId {u32 muValue}

namespace BrnParticle { struct ParticleModule; }     // reached by-pointer by Reset

namespace BrnEffects
{
    // The number of concurrent glass-smash VFX slots the manager owns (Reset asserts the
    // slot index against it).
    static const u32 KU_MAX_GLASS_EFFECTS = 16;

    struct BrnGlassSmashEffect
    {
        // Reset @0x8228F298 -- stop any LION effect this slot owns and clear it to idle.
        void Reset(u32 luEffectInstance, BrnParticle::ParticleModule* lpParticleModule);

        Matrix44Affine mLocalTransform;      // +0x00
        EntityId       mVehicleID;           // +0x40
        u32            mGlassEffectHandle;   // +0x44
        f32            mfEffectEndTime;      // +0x48
        bool           mbEffectActive;       // +0x4C
    };
}
