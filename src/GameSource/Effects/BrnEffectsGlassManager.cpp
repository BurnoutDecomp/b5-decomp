// =============================================================================
// GameSource/Effects/BrnEffectsGlassManager.cpp
//
// BrnEffects::BrnGlassSmashEffect -- one glass-smash VFX slot.
// =============================================================================
#include "GameSource/Effects/BrnEffectsGlassManager.h"
#include "GameSource/Effects/Particles/ParticleModule.h"          // BrnParticle::ParticleModule / LionEffect
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT

namespace BrnEffects
{
    // X360 read of the raw .data word at dword_82CDAFA0, stored into mVehicleID@+0x40
    // on Reset. Its committed name/value are UNATTESTED (no source, no DWARF, no other
    // committed use of 0x82CDAFA0). Modelled as an extern u32 so the store is byte-
    // faithful without fabricating a value; NAME the global properly and drop this
    // placeholder once its home lands. Almost certainly the invalid-entity sentinel
    // (0xFFFFFFFF) but that is NOT proven here, so it is left as an external read.
    extern const u32 guResetVehicleId_82CDAFA0;                    // dword_82CDAFA0

    // Reset @ 0x8228F298
    //   Stop any LION effect this slot still owns, then clear the slot to idle:
    //   identity local transform, invalid effect handle, no end-time, inactive,
    //   and the module's reset vehicle id.
    //
    //   luEffectInstance is the manager slot index (asserted < KU_MAX_GLASS_EFFECTS);
    //   it is validated but otherwise unused -- Reset operates on `this`.
    void BrnGlassSmashEffect::Reset(u32 luEffectInstance, BrnParticle::ParticleModule* lpParticleModule)
    {
        CGS_ASSERT(luEffectInstance < KU_MAX_GLASS_EFFECTS,
                   "luEffectInstance < KU_MAX_GLASS_EFFECTS");

        // Resolve the slot; GetLionEffect returns NULL unless the stored handle still
        // matches (the X360 build inlines this resolve + its own 'luArrayIndex <
        // KU_MAX_PLAYING_EFFECTS' assert here). Stop it only if ours.
        BrnParticle::LionEffect* lpLionEffect = lpParticleModule->GetLionEffect(mGlassEffectHandle);
        if (lpLionEffect != NULL)
        {
            lpParticleModule->StopLionEffect(lpLionEffect);
        }

        mLocalTransform.SetIdentity();
        mGlassEffectHandle    = BrnParticle::LionEffect::KU_HANDLE_INVALID;  // -1
        mfEffectEndTime       = 0.0f;
        mbEffectActive        = false;
        mVehicleID.muValue    = guResetVehicleId_82CDAFA0;                   // dword_82CDAFA0
    }
}
