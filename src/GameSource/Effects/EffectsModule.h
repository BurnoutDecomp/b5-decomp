#ifndef GAMESOURCE_EFFECTS_EFFECTSMODULE_H
#define GAMESOURCE_EFFECTS_EFFECTSMODULE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                 // Vector3 (CarState's camera position)
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"  // CgsModule::ModuleSingleBuffered (base)
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
    // Forward decl: the replay effects serialiser CarState carries a pointer to
    // (the effects state machines round-trip their locators through it on OnTick).
    // Full type lives in GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h.
}
namespace BrnReplays { class EffectsSerialiser; }
namespace BrnPhysics { namespace Vehicle { struct RaceCarState; } }
// GetNextAcquireResourceResponse return/param (pointer-only here; full type in
// GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h, pulled by the .cpp).
namespace CgsResource { namespace Events { struct AcquireResourceResponse; } }
// The wheel skid-smoke FX draws its randomiser + contact-surface schema straight from
// the module (WheelStateMachine 0x82288E38 / 0x82293EB8: module +0x2C3C0 Random,
// +0x2D3C8 surfacelist). Forward-only here; the accessors are declared on EffectsModule.
namespace CgsNumeric { class Random; }
namespace Attrib { namespace Gen { class surfacelist; } }

namespace BrnEffects
{
    // ------------------------------------------------------------------------
    // BrnEffects::CarState -- the per-frame car snapshot the effects state
    // machines (boost / jump / wheel) consult to decide their transitions.
    //
    // DWARF home GameSource/Effects/EffectsModule.h:68 names a member SET
    // (mEffectsModuleParams, mpBoostInfo, mpCarState, mfSpeedMPH,
    // mfExhaustPopIntensity, the mbExhaustPopThisFrame / mbCrashing / mbJumping /
    // mbEngineRunning flags, IsBoosting()) for the FIGS build. The X360 ARTIST
    // build's CarState is WIDER -- it also carries the race-car index, the replay
    // effects serialiser pointer, the boost type and the boost flag inline -- so
    // the layout below is PINNED BY THE ARTIST ASM OFFSETS (verified from OnTick
    // 0x82298BE0, OnDetermineNextState 0x82288230 and OnChangeState 0x8229E3D8);
    // unknown gaps are explicit padding and members are accessed by name only.
    // ------------------------------------------------------------------------
    struct CarState
    {
        // +0x00: the active race-car index (0..7) passed to the replay serialiser's
        // Get/SetBoostLocators in OnTick.
        u32 muRaceCarIndex;               // +0x00
        u8  mPad04[0x0C];                 // +0x04  (frame time/camera params -- unused here)

        // +0x10: the per-frame exhaust-smoke blend ramp delta added while the
        // ExhaustSmokeStopping state fades the smoke out (asm case 3:
        // mfExhaustSmokeBlend += this). The wheel skid-smoke FX reads the SAME word as
        // the frame time step (WheelStateMachine::Update 0x82293EB8 / HandleSmokeLayer
        // 0x82288E38 read lCarState+0x10 as dt), exposed via the anonymous-union alias
        // mfFrameTimeStep -- same offset/size, additive, no boost/jump breakage.
        union
        {
            f32 mfExhaustSmokeBlendDelta; // +0x10  (boost)
            f32 mfFrameTimeStep;          // +0x10  (wheel FX per-frame dt)
        };
        f32 mfCurrentTime;                // +0x14  (wheel FX skid-smoke spawn-time base)
        u8  mPad18[0x18];                 // +0x18  (params tail + mpBoostInfo/mpCarState ptrs)

        // +0x30: the replay effects serialiser this frame's locators round-trip
        // through (its leading word is the serialiser EMode: 1/2/3 recording,
        // 4/5/6 playing).
        BrnReplays::EffectsSerialiser* mpEffectsSerialiser;  // +0x30

        // +0x34: whether the car is currently boosting (drives Boosting/BoostStopping).
        bool mbIsBoosting;                // +0x34
        u8   mPad35[0x03];                // +0x35  (alignment to the boost-type word)

        // +0x38: the active boost type (BrnWorld::EBoostType; -1 none, 0..2 valid).
        // Asserted >= 0 and < 3 by OnChangeState before indexing BOOST_EFFECTS.
        s32  meBoostType;                 // +0x38

        // +0x3C: the boost-transition progress fed to the smoothstep remap that
        // produces the exhaust-smoke blend while Boosting (asm case 1).
        f32  mfBoostTransitionProgress;   // +0x3C

        // +0x40: the full race-car physics snapshot (wheels + linear velocity),
        // read by ExhaustSmokeOn (asm case 2) to derive the exhaust-smoke intensity.
        const BrnPhysics::Vehicle::RaceCarState* mpCarState;  // +0x40

        // +0x44: the car speed (m/s) the exhaust-pop logic compares against
        // KF_EXHAUST_MAX_VEHICLE_SPEED (13.4112).
        f32  mfSpeedMS;                   // +0x44
        f32  mfExhaustPopRechargeBlend;   // +0x48  (recharge blend, asm OnChangeState case 5)

        // +0x4C/+0x4D/+0x4F: the crash / jump / engine-running flags.
        bool mbCrashing;                  // +0x4C
        bool mbJumping;                   // +0x4D
        u8   mPad4E[0x01];                // +0x4E
        bool mbEngineRunning;             // +0x4F
    };

    class EffectsModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        static const u32 KU_MAX_JUNKYARD_VFX = 10;   // DWARF EffectsModule.h:633

        // X360 ARTIST @ 0x827E35E0. Constructs the module's base, the embedded
        // ParticleModule at X360 byte +0xA80, 8 debris generators, debris-cluster
        // slots, debrisparams, and surfacelist. See EffectsModule.cpp for the body.
        EffectsModule();

        // The embedded particle module (DWARF EffectsModule.h:308).
        BrnParticle::ParticleModule& ParticleModule();

        // The module-embedded pseudo-random generator (X360 byte +0x2C3C0). DWARF
        // EffectsModule::RandomNumberGenerator; the wheel skid-smoke FX draws its
        // backward-emission jitter from it. Body in EffectsModule.cpp.
        CgsNumeric::Random& RandomNumberGenerator();

        // The module-embedded contact-surface schema instance (X360 byte +0x2D3C8),
        // whose Surfaces[] array the wheel FX indexes by surface id to fetch the skid-
        // smoke visual-FX attributes. Body in EffectsModule.cpp.
        Attrib::Gen::surfacelist& SurfaceList();

        // One of the KU_MAX_JUNKYARD_VFX junkyard effect handles (maJunkyardEffectHandles[]).
        u32 GetJunkyardEffectHandle(u32 luIndex) const;

        // True while a junkyard VFX edit session is live; the debug editor resets when false.
        bool IsJunkyardVfxActive() const;

    private:
        // X360 0x8227F098. Iterate the module's resource-acquire reply queue (mReceiverQueue,
        // an EventReceiverQueue<2048,16> embedded at X360 byte +0x244). Body in EffectsModule.cpp.
        const CgsResource::Events::AcquireResourceResponse*
        GetNextAcquireResourceResponse(const CgsResource::Events::AcquireResourceResponse* lpPrevious);

        // X360 0x822926C8. Drive the convoy slip-stream LION effect (muSlipStreamEffectHandle at
        // X360 byte +0x2F54C). Body in EffectsModule.cpp.
        void HandleConvoySlipStream(f32 lfBlend, u32 luUnused,
                                    const rw::math::vpu::Matrix44Affine& lrTransform);

        // FLAG: fully opaque body. The X360 EffectsModule is ~185 KB; sub-objects are
        // constructed by the ctor at X360 absolute byte offsets via placement new.
        // When individual sub-type layouts land, fold them into named typed members.
        // Bound grown to 0x2F550 (193872) to cover muSlipStreamEffectHandle at X360 +0x2F54C
        // (193868) + 4 (HandleConvoySlipStream stores through this+0x2F54C, beyond the prior
        // 185400 surfacelist bound).
        u8 mOpaqueBody[0x2F550];
    };
}

#endif // GAMESOURCE_EFFECTS_EFFECTSMODULE_H
