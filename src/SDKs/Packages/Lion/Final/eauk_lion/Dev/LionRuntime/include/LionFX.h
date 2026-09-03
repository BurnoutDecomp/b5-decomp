#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h
//
// cLionFX -- the LION (eauk_lion) particle runtime's front door. DecFIGS DWARF
// LionFX.h:45 declares the class and its API; the X360 bodies are a thin facade over
// the module-scope singletons (cLionEffectManager @dword_83121D94,
// cParticleEmitterManager @dword_831238E8, cParticleRender @dword_82FACC20 ...).
//
// ⚠ THE MEMBERS ARE STATIC, and that is settled by the ASM, not by the DWARF dump.
// Every cLionFX entry point takes its first real argument in r3 with no `this`:
// BinLoad @0x82914388 is `BinLoad(blob)`, EffectCreate @0x82914CB8 forwards to
// cLionEffectManager::EffectCreate(&dword_83121D94, a1..a5) supplying the manager
// from a global rather than from `this`. (The dwarfdump cannot show the difference --
// it prints no implicit parameter either way.) Declaring them non-static would mangle
// every call site to a symbol with an extra `this` and silently fork the ODR.
//
// ONLY BinLoad IS RECONSTRUCTED IN THIS PASS. The rest of the class -- Init, DeInit,
// Update, Render, Dispatch, the locator/scaler/trigger registries, EffectCreate/
// EffectDestroy -- reaches the Lion simulation and render core, which is not landed
// (cParticleEmitter, cParticleRender, cParticleBehaviour::Lerp and friends). They are
// deliberately NOT declared here: a declaration with no definition is how a caller
// gets to fail at link instead of at the honest place, and a quiet body would be worse.
// ============================================================================

#include "types.hpp"

struct cLionEffectDefinition;   // LionEffect.h (sibling home)

// DecFIGS DWARF LionFX.h:45.
struct cLionFX
{
    // cLionFX::BinLoad @0x82914388 (DWARF LionFX.h:99). Take a saved LION effect blob,
    // check its version word, re-base the effect graph inside it and build it, then link
    // the effect into the runtime's global effect chain. Returns the blob as a
    // cLionEffectDefinition*, or NULL when the blob is null or is not a LION effect.
    static cLionEffectDefinition* BinLoad(void* apData);
};
