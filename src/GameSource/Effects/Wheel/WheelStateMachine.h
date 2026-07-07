#ifndef GAMESOURCE_EFFECTS_WHEEL_WHEELSTATEMACHINE_H
#define GAMESOURCE_EFFECTS_WHEEL_WHEELSTATEMACHINE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (mvPreviousPosition + the per-particle position/velocity args)

// ============================================================================
// GameSource/Effects/Wheel/WheelStateMachine.h
//
// BrnEffects::WheelStateMachine -- the per-wheel effects helper that drives a
// race car's tyre skid-smoke / surface-contact particle effects. One instance is
// owned per driven wheel by the effects module; EffectsModule::HandleWheels calls
// Update() on each every frame.
//
// SOURCE-OF-TRUTH: behaviour + member OFFSETS verified against the ARTIST X360 asm
// (HandleSmokeLayer 0x82288E38, Update 0x82293EB8); declaration SHAPE (method set,
// member names/types, the SKID_FACTOR_THRESHOLD / K_VELOCITY_* statics) from the
// DecFIGS DWARF (GameSource/Effects/Wheel/WheelStateMachine.h), gated on the ARTIST
// ledger.
//
// LAYOUT (verified by Update's mWheelIndex read @+0x00 and HandleSmokeLayer's
// accumulator store @ this + (luLayer+2)*4 == +0x08 + luLayer*4):
//   +0x00  mWheelIndex                 (BrnPhysics::Vehicle::EVehicleDrivenWheel; index 0..3)
//   +0x04  mfSurfaceParticleAccumulator
//   +0x08  mfSkidSmokeAccumulators[2]  (one accumulator per skid-smoke layer)
//   +0x10  mvPreviousPosition          (Vector3)
//   sizeof == 0x20.
//
// NOTE: mWheelIndex / the Construct() argument are typed u32 here (byte-identical to
// the DWARF's BrnPhysics::Vehicle::EVehicleDrivenWheel, which is an int-sized enum)
// to keep this owning header standalone; the code only ever uses the value as a
// maWheels[] index. Other WheelStateMachine TUs (ctor / Construct / Reset /
// Get/SetPreviousPosition / FireNativeParticle) EXTEND this header -- do not fork it.
// ============================================================================

namespace BrnEffects
{
    // Forward declarations of the types the wheel state machine is handed by reference.
    class RaceCarParticleEffectHelper;
    struct CarState;

    // DWARF WheelStateMachine.h:28. The minimum skid factor before skid smoke spawns
    // (namespace-scope const; consumed by FireNativeParticle's own TU). Declared here
    // for the shared vocabulary; defined with that TU.
    extern const f32 SKID_FACTOR_THRESHOLD;

    // ------------------------------------------------------------------------
    // BrnEffects::WheelStateMachine
    // ------------------------------------------------------------------------
    struct WheelStateMachine
    {
    public:
        // ---- construction / lifetime (bodies own their own TUs) -------------
        WheelStateMachine();
        void Construct(u32 luWheelIndex);   // DWARF: Construct(BrnPhysics::Vehicle::EVehicleDrivenWheel)
        void Reset();

        // Update @ 0x82293EB8. Per-frame: derive this wheel's road speed / skid
        // factors / reverse-thrust velocity, resolve the contact surface's visual-FX
        // attributes, and drive the (up to two) skid-smoke particle layers.
        void Update(const CarState& lCarState, const RaceCarParticleEffectHelper& lEffectHelper);

        // The previous-frame contact position (bodies own their own TU).
        Vector3& GetPreviousPosition();
        void     SetPreviousPosition(Vector3 lPosition);

    private:
        // FireNativeParticle (DWARF WheelStateMachine.h:87). Spawns one native
        // skid-smoke particle. Body lives in its own WheelStateMachine TU -- declared
        // only here (HandleSmokeLayer calls it in its spawn loop). leParticleType is
        // the SimpleParticleBatch::ENativeParticleType enum word (u32).
        void FireNativeParticle(Vector3 lWheelPosition, Vector3 lWheelVelocity,
                                u32 leParticleType, Vector3 lSpawnPos,
                                const RaceCarParticleEffectHelper& lEffectHelper,
                                const CarState& lCarState,
                                f32 lfSpawnTime, f32 lfKickUpSpeed,
                                Vector3 lvBackwardEmissionBias, f32 lfAngularVelocityScale);

        // HandleSmokeLayer @ 0x82288E38. Accumulate skid-smoke "particles owed" for one
        // layer and, once >= 1, spawn the whole-number count back-dating each particle
        // across the frame's timestep.
        void HandleSmokeLayer(u32 luLayer, const CarState& lCarState,
                              const RaceCarParticleEffectHelper& lEffectHelper,
                              Vector3 lWheelPos, Vector3 lWheelVel,
                              Vector3 lWheelReverseThrustVel,
                              f32 lfSkidFactor, f32 lfMaxWheelTravel,
                              f32 lfSkidStartThreshold, f32 lfParticlesPerMetre,
                              f32 lfBackwardEmissionFactor, f32 lfAngularVelocityScale,
                              u32 leParticleType);

        // ---- data members (offsets relative to the WheelStateMachine object) --
        u32     mWheelIndex;                   // +0x00  (EVehicleDrivenWheel; maWheels index)
        f32     mfSurfaceParticleAccumulator;  // +0x04
        f32     mfSkidSmokeAccumulators[2];    // +0x08  (per skid-smoke layer)
        Vector3 mvPreviousPosition;            // +0x10
    };

    // DWARF WheelStateMachine.h:65/66. The per-particle velocity spread / inherited-
    // velocity constants FireNativeParticle draws on. Declared here for the shared
    // vocabulary; defined with FireNativeParticle's TU.
    extern const rw::math::vpu::Vector3 K_VELOCITY_SPREAD;
    extern const rw::math::vpu::Vector3 K_VELOCITY_INHERITANCE;
}

#endif // GAMESOURCE_EFFECTS_WHEEL_WHEELSTATEMACHINE_H
