// =============================================================================
// GameSource/Effects/ParticleEffectHelper.cpp  (X360 ARTIST)
//
// BrnEffects::ParticleEffectHelper -- the thin facade the effects state machines
// (boost / jump / wheel) use to mutate playing LION particle effects. Each body
// resolves a handle to its playing-LION slot via ParticleModule::GetLionEffect
// (module[handle & 0x7F] with a stored-handle match) and pokes the slot.
//
//   SetEffectTransform    @ 0x822783E8
//   SetEffectStateBlend   @ 0x82278488   (multi-handle run overload)
//   StopEffect            @ 0x8228F220
//
// Reconstructed store-for-store from the X360 asm. Slot offsets (muHandle +0x00,
// mfStateBlend +0x0C, mTransform +0x10, muFlags +0x64; maPlayingEffects base
// +0x53F0, stride 0x70) verified against ParticleModule.h. Mirrors the inlined
// siblings in BoostStateMachine.cpp (SetBlendValue / StopEffects).
// =============================================================================

#include "GameSource/Effects/ParticleEffectHelper.h"
#include "GameSource/Effects/Particles/ParticleModule.h"   // BrnParticle::ParticleModule, LionEffect
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

namespace BrnEffects
{

// =============================================================================
// SetEffectTransform @ 0x822783E8
//   Resolve one playing-effect handle to its LION slot; if it still holds that
//   handle, overwrite the slot's world transform (the full 4x4 affine, copied as
//   four VMX rows in the X360 build) and flag the slot changed.
// =============================================================================
void ParticleEffectHelper::SetEffectTransform(u32& lruHandle, const Matrix44Affine& lTransform)
{
    const u32 luHandle = lruHandle;
    CGS_ASSERT((luHandle & 0x7Fu) < BrnParticle::ParticleModule::KU_MAX_PLAYING_EFFECTS,
               "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
    BrnParticle::LionEffect* lpEffect = ParticleModule().GetLionEffect(luHandle);
    if (lpEffect != NULL)
    {
        lpEffect->mTransform = lTransform;   // slot +0x10 (module +0x5400): four VMX rows
        lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
    }
}

// =============================================================================
// SetEffectStateBlend (multi-handle overload) @ 0x82278488
//   For each handle in [lpuHandles, lpuHandles+luCount): resolve its LION slot
//   and, if the slot still holds that handle, write the state blend and flag the
//   slot changed. Empty run -> no-op.
// =============================================================================
void ParticleEffectHelper::SetEffectStateBlend(const u32* lpuHandles, u32 luCount, f32 lfBlend)
{
    BrnParticle::ParticleModule& lModule = ParticleModule();
    for (u32 lu = 0; lu < luCount; ++lu)
    {
        const u32 luHandle = lpuHandles[lu];
        CGS_ASSERT((luHandle & 0x7Fu) < BrnParticle::ParticleModule::KU_MAX_PLAYING_EFFECTS,
                   "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
        BrnParticle::LionEffect* lpEffect = lModule.GetLionEffect(luHandle);
        if (lpEffect != NULL)
        {
            lpEffect->mfStateBlend = lfBlend;
            lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
        }
    }
}

// =============================================================================
// StopEffect @ 0x8228F220
//   Resolve the handle to its LION slot; if it still holds that handle, stop the
//   playing effect through the module, then invalidate the caller's handle
//   (unconditionally, whether or not the slot was still live).
// =============================================================================
void ParticleEffectHelper::StopEffect(u32& lruHandle)
{
    BrnParticle::ParticleModule& lModule = ParticleModule();
    CGS_ASSERT((lruHandle & 0x7Fu) < BrnParticle::ParticleModule::KU_MAX_PLAYING_EFFECTS,
               "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
    BrnParticle::LionEffect* lpEffect = lModule.GetLionEffect(lruHandle);
    if (lpEffect != NULL)
    {
        lModule.StopLionEffect(lpEffect);
    }
    lruHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;
}

} // namespace BrnEffects
