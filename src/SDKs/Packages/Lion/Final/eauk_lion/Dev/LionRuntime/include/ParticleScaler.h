#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleScaler.h
//
// cParticleScaler -- the third of the Lion (eauk_lion) per-effect binding sub-objects,
// beside cParticleLocator (a transform) and cParticleTrigger (a run/stop edge). It is the
// smallest of the three: ONE float, the effect's scale.
//
// LAYOUT AUTHORITY, two independent sources agreeing:
//   * DecFIGS DWARF (ParticleScaler.h:26-39) declares `private: FP32 mScale;` and the
//     methods Init / Update(FP32, const cTime&) / GetScale() const.
//   * cParticleSystem::AppInit @0x82913810 sizes gLionScalerAllocator's items at 4 BYTES
//     (against 176 for a locator and 16 for a trigger), and cLionFX::ScalerUpdate
//     @0x82908878 is three instructions: null check, `stfs f1, 0(r3)`, return.
//
// All f32, no pointers, so the console layout is the host layout.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/ext-include/GameStructs/cTime.h"

// DWARF ParticleScaler.h:26.
struct cParticleScaler
{
    // DWARF ParticleScaler.h:28. RECOVERED 2026-09-03 from the export-set hole it was
    // inlined into: cLionFX::ScalerRegister @0x8290AC68 does `lfs f0, 0x1C98(r11)` /
    // `stfs f0, 0(r3)`, and 0x82001C98 reads 3F800000 == 1.0f. So the initial scale is ONE,
    // not zero -- 1.0 is the identity of the multiply cParticleEmitter::Blend applies it in,
    // which is why an effect whose scaler is never driven plays its behaviour stack unscaled
    // instead of collapsed. (This header previously declared Init without defining it,
    // precisely so a caller would fail at link rather than against a guessed body.)
    void Init() { mScale = 1.0f; }

    // DWARF ParticleScaler.h:30. INLINED at its only reachable call site: cLionFX::
    // ScalerUpdate @0x82908878 stores the float straight through the pointer and never
    // touches the time stamp. De-inlined back to the owning method, which is where the
    // DWARF says the source put it -- and the time parameter is kept because the DWARF
    // declares it, and the fact that the console ignores it is recorded here rather than
    // left for a later reader to rediscover.
    void Update(f32 afScale, const cTime& /*arTime*/) { mScale = afScale; }

    // DWARF ParticleScaler.h:36.
    f32 GetScale() const { return mScale; }

private:
    f32 mScale;   // +0x00 -- the whole record
};

static_assert(sizeof(cParticleScaler) == 4,
              "cParticleScaler is one float (cParticleSystem::AppInit @0x82913810 sizes the "
              "scaler pool's items at 4 bytes)");
