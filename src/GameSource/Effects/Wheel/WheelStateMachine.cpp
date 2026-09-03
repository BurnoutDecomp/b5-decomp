// =============================================================================
// GameSource/Effects/Wheel/WheelStateMachine.cpp  (X360 ARTIST)
//
// BrnEffects::WheelStateMachine -- the per-wheel tyre skid-smoke FX driver.
// Reconstructed store-for-store from the ARTIST X360 pseudocode + asm:
//
//   HandleSmokeLayer @ 0x82288E38   Update @ 0x82293EB8
//
// Update() runs once per wheel per frame: it early-outs unless the wheel is
// attached, has traction, is on the ground and effects are enabled; derives the
// wheel's road speed / long+lat skid factors / a backward "reverse-thrust"
// velocity; resolves the contact surface's visualfxsurface attributes (indexed by
// the road-contact surface id); and drives the two skid-smoke layers through
// HandleSmokeLayer. HandleSmokeLayer accumulates fractional particles owed for a
// layer and, once >= 1, spawns the whole-number count, back-dating each particle's
// spawn time + position across the frame's timestep.
//
// The two functions listed here are the ONLY ones bodied in this TU; the rest of
// WheelStateMachine (ctor / Construct / Reset / Get/SetPreviousPosition /
// FireNativeParticle) live in their own TUs and are declared-only in the header.
// =============================================================================

#include "GameSource/Effects/Wheel/WheelStateMachine.h"
#include "GameSource/Effects/EffectsModule.h"                 // BrnEffects::CarState, EffectsModule::{RandomNumberGenerator,SurfaceList}
#include "GameSource/Effects/ParticleEffectHelper.h"          // RaceCarParticleEffectHelper
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // BrnPhysics::Vehicle::{RaceCarState, WheelLite}
#include "GameShared/GameClasses/Numeric/CgsRandom.h"         // CgsNumeric::Random::RandomFloat
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"      // Attrib::Gen::surfacelist
#include "GameSource/AttribSys/Generated/classes/surface.h"          // Attrib::Gen::surface
#include "GameSource/AttribSys/Generated/classes/visualfxsurface.h"  // Attrib::Gen::visualfxsurface
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"  // Attrib::{Collection, DefaultDataArea}
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the NOT-RECONSTRUCTED announcement
#include <cstdio>   // snprintf
#include "rw/math/vpu/vector3_operation.h"                    // rw::math::vpu::{Magnitude, operator*, operator-}
#include "rw/math/fpu/scalar_operation.h"                     // rw::math::fpu::{Max, Clamp, Abs}

#include <cmath>                                              // std::floor

namespace BrnEffects
{

namespace
{
    // ---- rodata constants recovered from the X360 build (inline in the Hex-Rays) ----

    // The minimum wheel speed (m/s) below which the wheel emits no skid smoke this
    // frame (rodata flt_82001C98 == 1.0f).
    const f32 KF_WHEEL_VELOCITY_THRESHOLD = 1.0f;

    // The lateral road speed -> layer-2 skid-factor scale (rodata flt_82008714 == 0.025;
    // Hex-Rays: `-(fabs(mfRoadLatSpeed) * 0.025)`). |latSpeed| * this, clamped to [0,1].
    const f32 KF_LATERAL_SKID_SPEED_SCALE = 0.025f;

    // The reverse-thrust ramp: below KF_WHEEL_REVERSE_THRUST_CUTOFF_SPEED the wheel
    // gets a backward velocity that fades to zero at the cutoff. The scale is exactly
    // 1 / cutoff (Hex-Rays: `-((6.7041669 - wheelSpeed) * 0.14916097)`, rodata
    // flt_82013278 / flt_82013274).
    const f32 KF_WHEEL_REVERSE_THRUST_CUTOFF_SPEED = 6.7041669f;
    const f32 KF_WHEEL_REVERSE_THRUST_SCALE        = 0.14916097f;

    // The contact-surface id packed into the road-contact collision tag (asm:
    // (tag.halfword@+2 >> 4) & 0x3F). 6 bits.
    const u32 KU_SURFACE_ID_SHIFT = 4;
    const u32 KU_SURFACE_ID_MASK  = 0x3F;

    // The visualfxsurface attribute-data byte offsets the two skid-smoke layers read
    // (attested by the WheelStateMachine::Update HandleSmokeLayer arg loads). Layer 0
    // at +0x20.., layer 1 at +0x34.., the two enable flags at +0x4C / +0x4D.
    const u32 KU_VFX_LAYER0_SKID_START_THRESHOLD = 0x20;
    const u32 KU_VFX_LAYER0_PARTICLE_TYPE        = 0x24;
    const u32 KU_VFX_LAYER0_PARTICLES_PER_METRE  = 0x28;
    const u32 KU_VFX_LAYER0_BACKWARD_EMISSION    = 0x2C;
    const u32 KU_VFX_LAYER0_ANGULAR_VEL_SCALE    = 0x30;
    const u32 KU_VFX_LAYER1_SKID_START_THRESHOLD = 0x34;
    const u32 KU_VFX_LAYER1_PARTICLE_TYPE        = 0x38;
    const u32 KU_VFX_LAYER1_PARTICLES_PER_METRE  = 0x3C;
    const u32 KU_VFX_LAYER1_BACKWARD_EMISSION    = 0x40;
    const u32 KU_VFX_LAYER1_ANGULAR_VEL_SCALE    = 0x44;
    const u32 KU_VFX_SKID_SMOKE_ENABLED          = 0x4C;
    const u32 KU_VFX_SKID_SMOKE_2_ENABLED        = 0x4D;

    // ⭐⭐ RENAMED + RETYPED 2026-09-03. What sits 16 bytes into the surface's attribute data
    // is an Attrib::RefSpec -- the ref to that surface's visualfxsurface collection -- not a
    // sub-collection. The asm at 0x82293EB8 is
    //     sub_8227FB58(v37, AttributePointer, 0);                      // surface(const RefSpec&)
    //     Attrib::Gen::visualfxsurface(&v33, LODWORD(v37[1]) + 16, 0); // visualfxsurface(const RefSpec&)
    // and BOTH of those generated ctors enter Attrib::Instance through sub_8280A248, whose
    // first instruction pair is `mr r3,r4 / bl Attrib__RefSpec__GetCollection`. Handing either
    // one an Attrib::Collection* made Instance::Instance read mpData/mpSource at +0x28/+0x30
    // and bump muRefCount at +0x08 -- past the end of a 24-byte RefSpec -- and GetClass() then
    // loaded a garbage class pointer. See visualfxsurface.h for the whole chain.
    const u32 KU_VFX_SURFACE_REF_OFFSET = 16;

    // The visualfxsurface reference embedded in a surface's layout block.
    const Attrib::RefSpec& VfxSurfaceRef(const void* lpSurfaceLayout)
    {
        return *reinterpret_cast<const Attrib::RefSpec*>(
            reinterpret_cast<const u8*>(lpSurfaceLayout) + KU_VFX_SURFACE_REF_OFFSET);
    }

    // The ActiveRaceCarData flag halfword (X360 byte +0x130); bit 1 disables the wheel FX.
    const u32 KU_ACTIVE_RACE_CAR_FLAGS_OFFSET = 0x130;

    inline f32 ReadF32(const u8* lpBase, u32 luOffset)
    {
        return *reinterpret_cast<const f32*>(lpBase + luOffset);
    }
    inline u32 ReadU32(const u8* lpBase, u32 luOffset)
    {
        return *reinterpret_cast<const u32*>(lpBase + luOffset);
    }
}

// =============================================================================
// HandleSmokeLayer @ 0x82288E38
//   Accumulate skid-smoke particles owed for one layer; once >= 1 spawn the
//   whole-number count, spreading their spawn time + position back across the
//   frame's timestep and biasing them backward by a randomised reverse-thrust push.
// =============================================================================
void WheelStateMachine::HandleSmokeLayer(u32 luLayer, const CarState& lCarState,
                                         const RaceCarParticleEffectHelper& lEffectHelper,
                                         Vector3 lWheelPos, Vector3 lWheelVel,
                                         Vector3 lWheelReverseThrustVel,
                                         f32 lfSkidFactor, f32 lfMaxWheelTravel,
                                         f32 lfSkidStartThreshold, f32 lfParticlesPerMetre,
                                         f32 lfBackwardEmissionFactor, f32 lfAngularVelocityScale,
                                         u32 leParticleType)
{
    CGS_ASSERT(luLayer < 2, "luLayer < 2");

    // Below the layer's skid-start threshold nothing accumulates.
    if (lfSkidFactor <= lfSkidStartThreshold)
    {
        return;
    }

    // particles owed += skidFactor * distanceTravelled * particlesPerMetre.
    f32& lrfAccumulator = mfSkidSmokeAccumulators[luLayer];
    lrfAccumulator += (lfSkidFactor * lfMaxWheelTravel) * lfParticlesPerMetre;
    if (lrfAccumulator < 1.0f)
    {
        return;
    }

    const f32 lfTimeStep    = lCarState.GetDt();           // CarState +0x10 (EffectsModuleParams::mDt)
    const f32 lfCurrentTime = lCarState.GetTime();         // CarState +0x14 (EffectsModuleParams::mTime)
    const f32 lfNumToSpawn  = std::floor(lrfAccumulator);

    // One randomised backward-emission bias, shared by every particle spawned this call.
    CgsNumeric::Random& lRandom = lEffectHelper.GetEffectsModule()->RandomNumberGenerator();
    const f32 lfBackwardsEmissionFactorRandomised = lRandom.RandomFloat() * lfBackwardEmissionFactor;
    const Vector3 lvBackwardEmissionBias = lWheelReverseThrustVel * lfBackwardsEmissionFactorRandomised;

    // Spread the whole-number spawn count evenly across the frame's timestep, walking
    // the spawn position back along the wheel's velocity and the spawn time back from now.
    const f32 lfSpawnTimeStep = lfTimeStep / lfNumToSpawn;
    const Vector3 lSpawnStep   = lWheelVel * lfSpawnTimeStep;

    Vector3 lSpawnPos  = lWheelPos;
    f32     lfSpawnTime = lfCurrentTime;
    do
    {
        const f32 KF_KICKUP_SPEED = 0.2f;   // flt_8200DD40
        FireNativeParticle(lWheelPos, lWheelVel, leParticleType, lSpawnPos,
                           lEffectHelper, lCarState, lfSpawnTime, KF_KICKUP_SPEED,
                           lvBackwardEmissionBias, lfAngularVelocityScale);
        lrfAccumulator -= 1.0f;
        lSpawnPos    = lSpawnPos - lSpawnStep;
        lfSpawnTime -= lfSpawnTimeStep;
    }
    while (lrfAccumulator >= 1.0f);
}

// =============================================================================
// Update @ 0x82293EB8
//   Per-frame per-wheel skid-smoke drive.
// =============================================================================
void WheelStateMachine::Update(const CarState& lCarState,
                               const RaceCarParticleEffectHelper& lEffectHelper)
{
    // This wheel's published sim state (112-byte WheelLite, indexed by mWheelIndex).
    const BrnPhysics::Vehicle::WheelLite& lWheel =
        lEffectHelper.RaceCarState()->maWheels[mWheelIndex];

    // No smoke unless the wheel is present, gripping and touching the road.
    if (!lWheel.mbAttached)                    return;
    if (!lWheel.mbHasTraction)                 return;
    if (!lWheel.mRoadContact.mbIsOnGround)     return;

    // Effects-disable flag (ActiveRaceCarData +0x130 bit 1).
    const u16 lu16ActiveFlags = *reinterpret_cast<const u16*>(
        reinterpret_cast<const u8*>(lEffectHelper.ActiveRaceCar()) + KU_ACTIVE_RACE_CAR_FLAGS_OFFSET);
    if (((lu16ActiveFlags >> 1) & 1u) != 0)
    {
        return;
    }

    const Vector3 lWheelPos      = lWheel.mRoadContact.mPosition;
    const Vector3 lWheelVelocity = lWheel.mVelocity;

    // The two per-layer skid factors: layer 0 is the wheel's own skid factor; layer 1 is
    // its lateral (sideways-scrub) road speed mapped through a scale + clamp.
    const f32 lfLongitudalSkidFactor = lWheel.mfSkidFactor;
    const f32 lfLatitudalSkidFactor =
        rw::math::fpu::Clamp(rw::math::fpu::Abs(lWheel.mfRoadLatSpeed) * KF_LATERAL_SKID_SPEED_SCALE,
                             0.0f, 1.0f);

    // The wheel's road speed = max(|velocity|, tyre-surface speed). The distance the
    // wheel travels this frame drives the skid-smoke accumulation.
    const f32 lfWheelSurfaceSpeed = lWheel.mfRadius * lWheel.mfRadiansPerSecond;
    const f32 lfWheelSpeed        = rw::math::vpu::Magnitude(lWheelVelocity);
    const f32 lfWheelMaxSpeed     = rw::math::fpu::Max(lfWheelSpeed, lfWheelSurfaceSpeed);
    const f32 lfMaxWheelTravel    = lCarState.GetDt() * lfWheelMaxSpeed;   // +0x10
    if (lfWheelMaxSpeed < KF_WHEEL_VELOCITY_THRESHOLD)
    {
        return;
    }

    // A backward "reverse-thrust" velocity, ramping in below the cutoff speed and scaled
    // by the tyre-surface speed, applied along the car's forward (Z) axis (negated).
    const BrnPhysics::Vehicle::RaceCarState* lpCarState = lCarState.mpCarState;
    const f32 lfReverseThrustScale = rw::math::fpu::Max(
        (KF_WHEEL_REVERSE_THRUST_CUTOFF_SPEED - lfWheelSpeed) * KF_WHEEL_REVERSE_THRUST_SCALE, 0.0f);
    const Vector3 lWheelReverseThrustVelocity =
        lpCarState->mTransform.zAxis * -(lfReverseThrustScale * lfWheelSurfaceSpeed);

    // Resolve the contact surface's visual-FX attributes: index the module's surfacelist
    // by the road-contact surface id, wrap the resolved element in a surface instance,
    // then follow the visualfxsurface REF embedded in that surface's layout (16 bytes in).
    const u32 luSurfaceID =
        (lWheel.mRoadContact.mCollisionTag.muValue >> KU_SURFACE_ID_SHIFT) & KU_SURFACE_ID_MASK;

    Attrib::Gen::surfacelist& lSurfaceList = lEffectHelper.GetEffectsModule()->SurfaceList();
    void* lpSurfaceRef = lSurfaceList.Surfaces(luSurfaceID);
    if (!lpSurfaceRef)
    {
        lpSurfaceRef = Attrib::DefaultDataArea(0x18u);
    }

    // sub_8227FB58 is the surface ctor's RefSpec overload -- Surfaces() hands back an
    // Attrib::RefSpec element of the "Surfaces" array, never a collection.
    Attrib::Gen::surface lSurface(*static_cast<const Attrib::RefSpec*>(lpSurfaceRef), NULL);
    Attrib::Gen::visualfxsurface lVfxSurface(VfxSurfaceRef(lSurface.GetAttributeData()), NULL);

    const u8* lpVfx = reinterpret_cast<const u8*>(lVfxSurface.GetAttributeData());

    // Layer 0: the wheel's straight-line skid smoke.
    if (lpVfx[KU_VFX_SKID_SMOKE_ENABLED])
    {
        HandleSmokeLayer(0, lCarState, lEffectHelper,
                         lWheelPos, lWheelVelocity, lWheelReverseThrustVelocity,
                         lfLongitudalSkidFactor, lfMaxWheelTravel,
                         ReadF32(lpVfx, KU_VFX_LAYER0_SKID_START_THRESHOLD),
                         ReadF32(lpVfx, KU_VFX_LAYER0_PARTICLES_PER_METRE),
                         ReadF32(lpVfx, KU_VFX_LAYER0_BACKWARD_EMISSION),
                         ReadF32(lpVfx, KU_VFX_LAYER0_ANGULAR_VEL_SCALE),
                         ReadU32(lpVfx, KU_VFX_LAYER0_PARTICLE_TYPE));
    }

    // Layer 1: the lateral-scrub skid smoke.
    if (lpVfx[KU_VFX_SKID_SMOKE_2_ENABLED])
    {
        HandleSmokeLayer(1, lCarState, lEffectHelper,
                         lWheelPos, lWheelVelocity, lWheelReverseThrustVelocity,
                         lfLatitudalSkidFactor, lfMaxWheelTravel,
                         ReadF32(lpVfx, KU_VFX_LAYER1_SKID_START_THRESHOLD),
                         ReadF32(lpVfx, KU_VFX_LAYER1_PARTICLES_PER_METRE),
                         ReadF32(lpVfx, KU_VFX_LAYER1_BACKWARD_EMISSION),
                         ReadF32(lpVfx, KU_VFX_LAYER1_ANGULAR_VEL_SCALE),
                         ReadU32(lpVfx, KU_VFX_LAYER1_PARTICLE_TYPE));
    }
}


    // =====================================================================================
    // FireNativeParticle  @0x82288C30 (DWARF WheelStateMachine.h:87)
    //
    // ⛔ NOT RECONSTRUCTED, announced not faked. The body builds one randomised spawn vector
    // from the wheel's position/velocity and the module's LCG ring, and its terminal call is
    //     BrnParticle::ParticleModule::SpawnWheelSmoke(arrayId, size, spawnPosition)
    // which pushes into ParticleModule::maSimpleParticles -- and BrnSimpleParticleArray is an
    // honest PARTIAL in this tree (only AcquireTexture's mbDirty + mpTextureNameRef are
    // modelled; there is no particle pool to push into and no Construct that would give it
    // one). Transcribing the arithmetic would end in a call with no destination.
    //
    // This is SKID SMOKE, not the skid MARK: HandleWheels @0x82296C80 lays the tyre mark
    // through TrailSystem::AddTrailSegment on a completely separate limb, and that limb is
    // live. Reachable (WheelStateMachine::Update calls this once the per-layer particle
    // accumulator passes 1.0), hence a loud no-op rather than a trap.
    // =====================================================================================
    void WheelStateMachine::FireNativeParticle(Vector3 /*lWheelPosition*/, Vector3 /*lWheelVelocity*/,
                                               u32 /*leParticleType*/, Vector3 /*lSpawnPosition*/,
                                               const RaceCarParticleEffectHelper& /*lEffectHelper*/,
                                               const CarState& /*lCarState*/,
                                               f32 /*lfSpawnTime*/, f32 /*lfKickUpSpeed*/,
                                               Vector3 /*lvBackwardEmissionBias*/,
                                               f32 /*lfAngularVelocityScale*/)
    {
        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            CgsDev::Log::WriteToLog(
                "[effects] NOT RECONSTRUCTED: WheelStateMachine::FireNativeParticle @0x82288C30 "
                "(its terminal ParticleModule::SpawnWheelSmoke pushes into maSimpleParticles, and "
                "BrnSimpleParticleArray is a partial layout with no particle pool). Skid SMOKE only "
                "-- the tyre MARK goes through HandleWheels -> TrailSystem::AddTrailSegment.\n");
        }
    }
} // namespace BrnEffects
