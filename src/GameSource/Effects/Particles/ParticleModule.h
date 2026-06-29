#ifndef GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H
#define GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44Affine

// ============================================================================
// GameSource/Effects/Particles/ParticleModule.h
//
// BrnParticle::ParticleModule and its per-playing-effect record BrnParticle::
// LionEffect. Shape recovered from the DecFIGS DWARF
// (GameSource/Effects/Particles/ParticleModule.h) and gated on the ARTIST ledger;
// the slice modelled here is exactly what the effects debug component needs to
// resolve and tweak playing junkyard effects:
//
//   * LionEffect - a playing effect slot. Verified offsets from the ARTIST asm
//     (GetLionEffect 0x82278380 + the BrnEffectsDebugComponent junkyard funcs):
//       +0x00 muHandle, +0x10 mTransform (the affine drawn by DrawAxis; its
//       translation row is read at the +0x30/+0x34/+0x38 X/Y/Z), +0x64 muFlags.
//     The intervening fields (hashed name / definition ptr / blend factor /
//     velocity / world index / death time) are modelled as explicit padding -
//     they are not touched by this debug TU. sizeof == 0x70 (the array stride the
//     asm uses: 112 * (handle & KU_HANDLE_INDEX_MASK)).
//
//   * ParticleModule::GetLionEffect - resolves a handle to its slot, returning
//     NULL when the slot's stored handle no longer matches (slot recycled). The
//     full ParticleModule is a large module with its own TU; only this accessor
//     (the one the debug component calls) is declared here. GROW this header
//     additively when ParticleModule's own TU lands; do NOT fork these types.
// ============================================================================

namespace BrnParticle
{
    // A single playing LION (particle) effect slot. DWARF home ParticleModule.h:87.
    struct LionEffect
    {
        // Handle layout / per-effect flags (DWARF ParticleModule.h:90-100,184-187).
        static const u32 KU_HANDLE_INVALID    = 0xFFFFFFFFu;
        static const u32 KU_HANDLE_INDEX_MASK = 127u;   // handle & this -> slot index

        // muFlags bits (DWARF "ePPEFlag*").
        static const u16 EPPE_FLAG_IN_USE           = 1;
        static const u16 EPPE_FLAG_ENABLED          = 2;
        static const u16 EPPE_FLAG_CHANGED          = 4;
        static const u16 EPPE_FLAG_CREATE           = 8;
        static const u16 EPPE_FLAG_KILL             = 16;
        static const u16 EPPE_FLAG_OVERRIDE_VELOCITY= 32;

        const rw::math::vpu::Matrix44Affine& GetTransform() const { return mTransform; }

        u32 muHandle;                              // +0x00 - the handle this slot holds
        u8  mPad04[0x08];                          // +0x04 - hashed name / definition ptr
        f32 mfStateBlend;                          // +0x0C - state blend factor (BoostStateMachine
                                                   //         SetBlendValue stores here; +0x53FC)
        rw::math::vpu::Matrix44Affine mTransform;  // +0x10 - the effect's world transform
        u8  mPad50[0x0C];                          // +0x50 - velocity / death time
        u32 muWorldIndex;                          // +0x5C - world index (SetWorldIndex stores
                                                   //         here; slot +0x5C / module +0x544C)
        u8  mPad60[0x04];                          // +0x60
        u16 muFlags;                               // +0x64 - ePPEFlag* bitmask
        u8  mPad66[0x0A];                          // +0x66 - pad to the 0x70 array stride
    };

    // The particle / LION effects module. Only the slice the effects debug component
    // reaches is declared; the rest of the (large) module lives in its own TU.
    class ParticleModule
    {
    public:
        // X360 0x82278380. Resolve a handle to its playing-effect slot, or NULL when the
        // slot has been recycled (its stored handle no longer equals luHandle).
        LionEffect* GetLionEffect(u32 luHandle);

        // X360 0x822867E0. Start the named LION effect (the caller precomputes the name
        // hash via ParticleDescription::HashString) at the given world index, returning the
        // new playing-effect handle. Own-TU body; declared here for the boost/jump machines.
        u32 StartLionEffect(u32 luNameHash, const char* lpcEffectName, u32 luWorldIndex);

        // X360 0x8228A238. Stop a playing LION effect, given its resolved slot pointer
        // (asserts the slot is non-NULL and in use). Own-TU body.
        void StopLionEffect(LionEffect* lpEffect);
    };
}

#endif // GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H
