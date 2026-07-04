#ifndef GAMESOURCE_EFFECTS_PROPS_PROPCOLLISIONS_H
#define GAMESOURCE_EFFECTS_PROPS_PROPCOLLISIONS_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                 // Vector3, Matrix44Affine (rw::math::vpu)

// ============================================================================
// GameSource/Effects/Props/PropCollisions.h
//
// BrnEffects::VFXRuntimeMaterialLef -- the runtime helper that fires the "LION
// effect" (particle) VFX from a prop's material locators when the prop is struck.
// Reconstructed from the DWARF (GameSource/Effects/Props/PropCollisions.cpp) gated
// on the ARTIST ledger:
//   VFXRuntimeMaterialLef::CreateEffect     @ 0x822936F0
//   VFXRuntimeMaterialLef::TriggerLocators  @ 0x82299168
//
// A five-slot round-robin of playing LION-effect handles is kept in the file-scope
// statics mNextEffect / maEffectHandles.
// ============================================================================

namespace BrnParticle
{
    class ParticleModule;
    struct LionEffect;

    // DWARF VFXPropsResourceType.h:48 -- one baked VFX locator on a prop material:
    // a local-space anchor position, a precomputed effect-name hash, and the raw name
    // string. 0x50-byte stride (X360 TriggerLocators indexes lpLocatorArray at *0x50).
    struct VFXLocator
    {
        Vector3 mPosition;      // +0x00  local-space anchor
        u32     mHashedName;    // +0x10  precomputed effect-name hash
        char    macName[60];    // +0x14  raw locator name (.. +0x50)

        u32 GetHash() const { return mHashedName; }
    };
}

namespace BrnPhysics { namespace Vehicle { struct RaceCarState; } }
namespace CgsNumeric { class Random; }

namespace BrnEffects
{
    // The runtime striking-VFX helper for prop material locators (Lion-effect flavour).
    class VFXRuntimeMaterialLef
    {
    public:
        // The round-robin depth of the playing-effect handle ring (X360 mod-5 spine).
        static const u32 kuSizeOfEffectsArray = 5;

        // TriggerLocators @ 0x82299168 -- fire the LION effect on each locator of the
        // struck prop, seeding each effect's world transform (velocity-aligned basis
        // through the prop transform), override velocity, and a random state-blend.
        void TriggerLocators(BrnParticle::ParticleModule& lParticleModule,
                             f32 lfCurrentTimeStep,
                             f32 lfCurrentTime,
                             const rw::math::vpu::Matrix44Affine& lPropTransform,
                             const BrnParticle::VFXLocator* lpLocatorArray,
                             u32 lNumLocators,
                             const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState,
                             CgsNumeric::Random& lRandom);

    private:
        // CreateEffect @ 0x822936F0 -- round-robin one of the five VFX slots: stop the
        // slot if it is playing, start a fresh LION effect under the given name hash,
        // record the handle, return the resolved playing slot.
        BrnParticle::LionEffect* CreateEffect(BrnParticle::ParticleModule& lParticleModule,
                                              u32 lHashName);

        // File-scope statics (X360 dword_82FAB698 / dword_82FAB624[5]).
        static u32 mNextEffect;
        static u32 maEffectHandles[kuSizeOfEffectsArray];
    };
}

#endif // GAMESOURCE_EFFECTS_PROPS_PROPCOLLISIONS_H
