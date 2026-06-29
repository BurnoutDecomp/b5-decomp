#ifndef GAMESOURCE_EFFECTS_EFFECTSMODULE_H
#define GAMESOURCE_EFFECTS_EFFECTSMODULE_H

#include "types.hpp"
#include "GameSource/Effects/Particles/ParticleModule.h"   // BrnParticle::ParticleModule (accessor return)

// ============================================================================
// GameSource/Effects/EffectsModule.h
//
// BrnEffects::EffectsModule - the game's effects (VFX) module. It is a large
// CgsModule::ModuleSingleBuffered with its own TU (EffectsModule.cpp); only the
// slice the effects debug component reaches is declared here:
//
//   * ParticleModule()           - the embedded particle module (DWARF
//                                  EffectsModule.h:308). The debug component
//                                  resolves playing junkyard effects through it.
//   * GetJunkyardEffectHandle()  - reads one of the KU_MAX_JUNKYARD_VFX (=10)
//                                  junkyard effect handles (DWARF maJunkyard-
//                                  EffectHandles, EffectsModule.h:634).
//   * IsJunkyardVfxActive()      - whether the module is currently running a
//                                  junkyard VFX edit session (read by
//                                  EffectsDebugComponent::RenderWorld to know when
//                                  to reset the editor). Backed by a bool deep in
//                                  the module; its exact offset is module-internal.
//
// Shape/names recovered from the DecFIGS DWARF (GameSource/Effects/EffectsModule.h),
// gated on the ARTIST ledger. The bodies live in EffectsModule.cpp; GROW this
// header additively when that TU lands - do NOT fork the type locally.
// ============================================================================

namespace BrnEffects
{
    class EffectsModule
    {
    public:
        static const u32 KU_MAX_JUNKYARD_VFX = 10;   // DWARF EffectsModule.h:633

        // The embedded particle module (DWARF EffectsModule.h:308).
        BrnParticle::ParticleModule& ParticleModule();

        // One of the KU_MAX_JUNKYARD_VFX junkyard effect handles (maJunkyardEffectHandles[]).
        u32 GetJunkyardEffectHandle(u32 luIndex) const;

        // True while a junkyard VFX edit session is live; the debug editor resets when false.
        bool IsJunkyardVfxActive() const;
    };
}

#endif // GAMESOURCE_EFFECTS_EFFECTSMODULE_H
