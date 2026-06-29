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
        u8  mPad04[0x0C];                          // +0x04 - hashed name / definition ptr /
                                                   //         blend factor (not used here)
        rw::math::vpu::Matrix44Affine mTransform;  // +0x10 - the effect's world transform
        u8  mPad50[0x14];                          // +0x50 - velocity / world index / death time
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
    };
}

#endif // GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H
