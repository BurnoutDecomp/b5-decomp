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
    // slot index against it). DWARF value + asm `& 7` cursor wrap / 8-iteration Update loops.
    static const u32 KU_MAX_GLASS_EFFECTS = 8;

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

    // 'Glass_shattering' LION effect resource path (X360 rodata aGamedbBurnout5_11).
    static const char* const KPC_GLASS_SHATTER_EFFECT =
        "gamedb://burnout5/Burnout/Effects/Glass_shattering.lef.BurnoutFXLionEffectFile?ID=507406";

    // The auto-expiry fill-in window added to the fire time (X360 flt_82001C98 == 1.0f;
    // DWARF ::-file global kfLionGlassFillInEffectTime).
    static const f32 kfLionGlassFillInEffectTime = 1.0f;

    // Forward decl for the input buffer touched by UpdateVehicleEffectPositions (reached
    // by-pointer; full layout owned by its TU, pulled in by the .cpp includes).
    namespace EffectsIO { struct InputBuffer; }

    // BrnEffects::BrnEffectsGlassManager (DWARF BrnEffectsGlassManager.h:77). Owns the fixed
    // slot array + the round-robin cursor; drives per-frame slot maintenance.
    struct BrnEffectsGlassManager
    {
        // Construct(ParticleModule*)/Destruct()/Initialise land with their own TUs
        // (DWARF-declared; not in this batch -- GROW this home, do not fork).
        void Construct(BrnParticle::ParticleModule* lpParticleModule);
        void Destruct();

        // FireGlassEffect @0x82295D50 -- claim the next slot, start the glass LION effect,
        // and (only on handle resolve) seat the slot in the vehicle's local frame.
        void FireGlassEffect(const Matrix44Affine& lEffectTransform,
                             const Matrix44Affine& lVehicleTransform,
                             EntityId lVehicleEntity,
                             f32 lfCurrentTime);

        // UpdateVehicleEffectPositions @0x8228D208 -- expire timed-out slots, then re-attach
        // live slots to the current traffic / race-car transforms.
        void UpdateVehicleEffectPositions(const EffectsIO::InputBuffer* lpInput, f32 lfCurrentTime);

        BrnParticle::ParticleModule* mpParticleModule;                  // +0x00
        u32                          muNextGlassEffect;                 // +0x04
        u8                           maPad08[0x10 - 0x08];              // +0x08 (align to 0x10)
        BrnGlassSmashEffect          maGlassEffects[KU_MAX_GLASS_EFFECTS]; // +0x10 (stride 0x50)
    };
}
