// =============================================================================
// GameSource/Effects/Wheel/WheelStateMachine.cpp  (X360 ARTIST)
//
// BrnEffects::WheelStateMachine -- the per-wheel tyre skid-smoke FX driver.
// Reconstructed store-for-store from the ARTIST X360 pseudocode + asm:
//
//   HandleSmokeLayer @ 0x82288E38   Update @ 0x82293EB8   FireNativeParticle @ 0x82288C30
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
// The ctor / Construct / Reset / Get/SetPreviousPosition are header-inline.
//
// ⭐⭐ THE TYRE SMOKE -- FX-CRASHVFX 2026-09-25 (C1). FireNativeParticle announced itself NOT RECONSTRUCTED
// ("BrnSimpleParticleArray is a partial layout with no particle pool") -- stale since the simple particles
// landed (2a28113a / f5018cb0) -- so no wheel ever smoked: not in a drift, not under braking, not in the crash
// slide. It is bodied now, with its terminal ParticleModule::SpawnWheelSmoke @0x82281AF0, and the two callers
// are brought to the console's arithmetic:
//   * HandleSmokeLayer's skid gate is `fcmpu ; ble` (0x82288EB0 / 0x82288EB4): a NaN skid factor RETURNS
//     (the PC's `skid <= threshold` let it through); its accumulation is ONE fmadds (0x82288ED8);
//   * its spawn loop is `fcmpu ; bge` (0x82289030 / 0x82289034): a NaN accumulator loops FOREVER, on the
//     console as here (the conductor's ruling: the PC hangs exactly where the console would; the only addition
//     is an always-on one-shot log line, see the loop);
//   * Update's wheel speed is the console's |v|^2 (vmsum3fp128) times a twice-refined vrsqrtefp with the vsel
//     zero guard (0x82293F6C..0x82293FDC), not rw::math::vpu::Magnitude's std::sqrt;
//   * Update's crash gate reads ActiveRaceCarData::mFlags BY NAME (`lhz 0x130` at 0x82293F2C is mFlags, bit 1
//     eARDFlagIsCrashing): the raw +0x130 read landed on the low half of mID on x64 (mFlags sits at +0x138 there,
//     JumpStateMachine being 0x18 bytes, not 0x14), so bit 1 of the car's id, not its crash state, gated the smoke.
// =============================================================================

#include "GameSource/Effects/Wheel/WheelStateMachine.h"
#include "GameSource/Effects/EffectsModule.h"                 // BrnEffects::CarState, EffectsModule::{RandomNumberGenerator,SurfaceList}
#include "GameSource/Effects/ParticleEffectHelper.h"          // RaceCarParticleEffectHelper
#include "GameSource/Effects/ActiveRaceCarData.h"             // ActiveRaceCarData::{GetFlags, eARDFlagIsCrashing}
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // BrnPhysics::Vehicle::{RaceCarState, WheelLite}
#include "GameShared/GameClasses/Numeric/CgsRandom.h"         // CgsNumeric::Random::RandomFloat
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"      // Attrib::Gen::surfacelist
#include "GameSource/AttribSys/Generated/classes/surface.h"          // Attrib::Gen::surface
#include "GameSource/AttribSys/Generated/classes/visualfxsurface.h"  // Attrib::Gen::visualfxsurface
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"  // Attrib::{Collection, DefaultDataArea}
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog (the NaN-accumulator line)
#include <cstdio>   // snprintf
#include <cstdlib>  // std::getenv (the [wheel-smoke] witness switch)
#include "rw/math/vpu/vector3_operation.h"                    // rw::math::vpu::{operator*, operator-}
#include "rw/math/fpu/scalar_operation.h"                     // rw::math::fpu::{Max, Clamp, Abs}

#include <cmath>                                              // std::floor / std::fma / std::sqrt

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

    inline f32 ReadF32(const u8* lpBase, u32 luOffset)
    {
        return *reinterpret_cast<const f32*>(lpBase + luOffset);
    }
    inline u32 ReadU32(const u8* lpBase, u32 luOffset)
    {
        return *reinterpret_cast<const u32*>(lpBase + luOffset);
    }

    // FireNativeParticle's size draw: `fmadds f1, f9, f13, f12` (0x82288CC4) with f13 = flt_82011C1C and
    // f12 = flt_82011C18 -- RandomFloat(0.4, 1.2) with the range the compiler folded (1.2f - 0.4f rounds to
    // 0x3F4CCCCE, one ulp above 0.8f).
    const f32 KF_WHEEL_SMOKE_SIZE_MIN   = 0.4f;          // flt_82011C18 (0x3ECCCCCD)
    const f32 KF_WHEEL_SMOKE_SIZE_RANGE = 0.80000007f;   // flt_82011C1C (0x3F4CCCCE)

    // ---- the console's vector idioms (the models EffectsModule.cpp / PropCollisions.cpp name) ----
    // vmsum3fp128: the three products summed, then rounded once. FLAG (model): the campaign's rounding rule
    // (scratch/CRASHPARITY_0922/ROUNDING_RULE.md), after xenia-canary x64_sequences.cc DOT_PRODUCT_3_V128, whose
    // float64 accumulation "closely matches Xbox 360 vmsum results". Its QNaN on an f32 overflow of a finite sum
    // is not modelled: here the operand is a wheel velocity, and |v|^2 near 1.8e19 would need |v| ~ 4e9 m/s.
    f32 Dot3(const Vector3& lrA, const Vector3& lrB)
    {
        return static_cast<f32>(static_cast<f64>(lrA.x) * lrB.x + static_cast<f64>(lrA.y) * lrB.y
                              + static_cast<f64>(lrA.z) * lrB.z);
    }

    // `vnmsubfp`: -(a * c - b), fused, a NaN keeping its sign.
    f32 Vnmsub(f32 lfA, f32 lfC, f32 lfB)
    {
        const f32 lfDifference = std::fma(lfA, lfC, -lfB);
        return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
    }

    // `vrsqrtefp` and two Newton-Raphson refinements: est * est, est * 0.5, `vnmsubfp` 1 - x est^2 (fused),
    // `vmaddfp` est + half * r (fused). FLAG (model): the hardware estimate is modelled as its correctly rounded
    // value; the two refinements then pin the result.
    f32 RefinedRsqrt(f32 lfX)
    {
        f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfX)));
        for (u32 luStep = 0; luStep < 2u; ++luStep)
        {
            const f32 lfSquared  = lfEstimate * lfEstimate;
            const f32 lfHalf     = lfEstimate * 0.5f;
            const f32 lfResidual = Vnmsub(lfX, lfSquared, 1.0f);
            lfEstimate = std::fma(lfHalf, lfResidual, lfEstimate);
        }
        return lfEstimate;
    }

    // [DIAG] BRN_WHEEL_SMOKE_DIAG -- NOT IN THE X360 BINARY. Default OFF (armed when the variable is set to anything
    // that does not start with '0', read once per process), capped. DELETE-WHEN-STABLE. The live witness of the tyre
    // smoke (FX-CRASHVFX C1): one line per HandleSmokeLayer call that spawned (48 lines), one per wheel the crash gate
    // silenced (8 lines), and a running per-car tally (below). Reads only.
    bool WheelSmokeDiagArmed()
    {
        static const bool sbArmed = []()
        {
            const char* const lpcValue = std::getenv("BRN_WHEEL_SMOKE_DIAG");
            return lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0';
        }();
        return sbArmed;
    }

    // [DIAG] the running tally, per race car (CarState::muRaceCarIndex, 0..7): particles spawned and wheel-frames the
    // crash gate silenced, cumulative, at most one line per second of effects time (and again when the time runs
    // backwards), 600 lines -- so a long run reads crash by crash after the detail lines are spent.
    struct WheelSmokeTally
    {
        u32 mauSpawned[8];
        u32 mauGated[8];
        f32 mfNextLine;
        u32 muLines;
    };

    WheelSmokeTally& Tally()
    {
        static WheelSmokeTally sTally = {};
        return sTally;
    }

    void WheelSmokeTallyLine(f32 lfTime)
    {
        WheelSmokeTally& lrTally = Tally();
        const bool lbDue = (lfTime >= lrTally.mfNextLine) || (lfTime < lrTally.mfNextLine - 2.0f);
        if (lrTally.muLines >= 600u || !lbDue)
            return;
        ++lrTally.muLines;
        lrTally.mfNextLine = lfTime + 1.0f;
        const u32* const lpuS = lrTally.mauSpawned;
        const u32* const lpuG = lrTally.mauGated;
        char lacLine[256];
        std::snprintf(lacLine, sizeof(lacLine),
                      "[wheel-smoke] tally t=%.1f spawned %u %u %u %u %u %u %u %u gated %u %u %u %u %u %u %u %u\n",
                      lfTime, lpuS[0], lpuS[1], lpuS[2], lpuS[3], lpuS[4], lpuS[5], lpuS[6], lpuS[7],
                      lpuG[0], lpuG[1], lpuG[2], lpuG[3], lpuG[4], lpuG[5], lpuG[6], lpuG[7]);
        CgsDev::Log::WriteToLog(lacLine);
    }

    // Update's wheel speed (0x82293F6C..0x82293FDC): |v|^2, times its refined rsqrt (`vmulfp128 v0, v0, v13`),
    // ZERO where |v|^2 == 0 (`vcmpeqfp` against the zero splat, then `vsel`) -- the product there is 0 * inf.
    f32 GuardedMagnitude(const Vector3& lrv)
    {
        const f32 lfLengthSquared = Dot3(lrv, lrv);
        if (lfLengthSquared == 0.0f)
            return 0.0f;
        return lfLengthSquared * RefinedRsqrt(lfLengthSquared);
    }
}

// DWARF WheelStateMachine.h:65 / :66 -- CRT-initialised .bss (the image holds zeros), their values read by running
// each init thunk:
//   unk_82FAB7E0 (thunk 0x82C4AAF8): x = z = flt_82004D04 (1.5), y = flt_82004D00 (0.6), w = 0
//   unk_82FAB810 (thunk 0x82C4AB38): x = y = z = flt_82011C18 (0.4), w = 0
const rw::math::vpu::Vector3 K_VELOCITY_SPREAD      = { 1.5f, 0.6f, 1.5f, 0.0f };
const rw::math::vpu::Vector3 K_VELOCITY_INHERITANCE = { 0.4f, 0.4f, 0.4f, 0.0f };

// =============================================================================
// HandleSmokeLayer @ 0x82288E38
//   Accumulate skid-smoke particles owed for one layer; once >= 1 spawn the
//   whole-number count, spreading their spawn time + position back across the
//   frame's timestep and biasing them backward by a randomised reverse-thrust push.
// =============================================================================
void WheelStateMachine::HandleSmokeLayer(u32 luLayer, CarState& lCarState,
                                         RaceCarParticleEffectHelper& lEffectHelper,
                                         Vector3 lWheelPos, Vector3 lWheelVel,
                                         Vector3 lWheelReverseThrustVel,
                                         f32 lfSkidFactor, f32 lfMaxWheelTravel,
                                         f32 lfSkidStartThreshold, f32 lfParticlesPerMetre,
                                         f32 lfBackwardEmissionFactor, f32 lfAngularVelocityScale,
                                         u32 leParticleType)
{
    CGS_ASSERT(luLayer < 2, "luLayer < 2");

    // Below the layer's skid-start threshold nothing accumulates: `fcmpu f31, f27 ; ble` (0x82288EB0 / 0x82288EB4)
    // is TAKEN on an unordered compare, so a NaN skid factor returns here.
    if (!(lfSkidFactor > lfSkidStartThreshold))
    {
        return;
    }

    // particles owed = (skidFactor * distanceTravelled) * particlesPerMetre + owed: `fmuls` then ONE
    // `fmadds f0, f0, f28, f13` (0x82288EB8 / 0x82288ED8).
    f32& lrfAccumulator = mfSkidSmokeAccumulators[luLayer];
    lrfAccumulator = std::fma(lfSkidFactor * lfMaxWheelTravel, lfParticlesPerMetre, lrfAccumulator);
    if (lrfAccumulator < 1.0f)   // `blt` (0x82288EE4): not taken on a NaN -- a NaN goes on into the loop
    {
        return;
    }

    // [DIAG] ALWAYS ON, ONE LINE PER PROCESS -- NOT IN THE X360 BINARY -- diagnostic only; the console loops
    // forever here (0x82289030 / 0x82289034). A NaN accumulator never leaves the spawn loop below (NaN - 1 is
    // NaN, and `bge` is taken on unordered), exactly as on the console, so the game freezes -- deliberately: the
    // PC hangs where the console would. This line, written and flushed BEFORE the loop, is the only trace such a
    // hang can leave in BrnGame.log. It reads only, costs one compare per call and changes no behaviour.
    // Upstream, the NaN can only arrive through the distance travelled, dt x max(|v|, surface speed): the
    // wheel's surface speed (mfRadius x mfRadiansPerSecond, WheelStateMachine::Update) or the step dt (CarState
    // +0x10). The skid gate above turns a NaN skid factor away, and the fsel max drops a NaN |v|.
    if (lrfAccumulator != lrfAccumulator)
    {
        static bool sbSaid = false;
        if (!sbSaid)
        {
            sbSaid = true;
            char lacLine[384];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[wheel-smoke] NaN ACCUMULATOR -- NOT IN THE X360 BINARY -- diagnostic only; the console "
                          "loops forever here (0x82289030 / 0x82289034): wheel %u layer %u skid=%.6g travel=%.6g "
                          "perMetre=%.6g; upstream source: the wheel surface speed (radius x rad/s) or dt\n",
                          mWheelIndex, luLayer, lfSkidFactor, lfMaxWheelTravel, lfParticlesPerMetre);
            CgsDev::Log::WriteToLog(lacLine);   // fputs + fflush
        }
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
    while (!(lrfAccumulator < 1.0f));   // `fcmpu f0, f31 ; bge` (0x82289030 / 0x82289034): a NaN loops, as on the console

    // [DIAG] BRN_WHEEL_SMOKE_DIAG -- NOT IN THE X360 BINARY, default OFF, 48 detail lines + the tally.
    // DELETE-WHEN-STABLE.
    if (WheelSmokeDiagArmed())
    {
        const u32 luCar = lCarState.muRaceCarIndex & 7u;
        Tally().mauSpawned[luCar] += (lfNumToSpawn < 4.0e9f) ? static_cast<u32>(lfNumToSpawn) : 0xFFFFFFFFu;
        static u32 suLines = 0;
        if (suLines < 48u)
        {
            ++suLines;
            char lacLine[256];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[wheel-smoke] t=%.3f car %u wheel %u layer %u type %u spawned %d skid=%.3f travel=%.4f "
                          "perMetre=%.3f owed=%.3f at (%.1f, %.1f, %.1f)\n",
                          lfCurrentTime, lCarState.muRaceCarIndex, mWheelIndex, luLayer, leParticleType,
                          static_cast<s32>(lfNumToSpawn), lfSkidFactor, lfMaxWheelTravel, lfParticlesPerMetre,
                          lrfAccumulator, lWheelPos.x, lWheelPos.y, lWheelPos.z);
            CgsDev::Log::WriteToLog(lacLine);
        }
        WheelSmokeTallyLine(lfCurrentTime);
    }
}

// =============================================================================
// Update @ 0x82293EB8
//   Per-frame per-wheel skid-smoke drive.
// =============================================================================
void WheelStateMachine::Update(CarState& lCarState,
                               RaceCarParticleEffectHelper& lEffectHelper)
{
    // This wheel's published sim state (112-byte WheelLite, indexed by mWheelIndex).
    const BrnPhysics::Vehicle::WheelLite& lWheel =
        lEffectHelper.RaceCarState()->maWheels[mWheelIndex];

    // No smoke unless the wheel is present, gripping and touching the road.
    if (!lWheel.mbAttached)                    return;
    if (!lWheel.mbHasTraction)                 return;
    if (!lWheel.mRoadContact.mbIsOnGround)     return;

    // No smoke while the car is crashing: `lwz r10, 4(r30) ; lhz r10, 0x130(r10) ; srwi ; clrlwi ; bne`
    // (0x82293F28..0x82293F3C) tests ActiveRaceCarData::mFlags (DWARF ActiveRaceCarData.h:230) bit 1,
    // eARDFlagIsCrashing. Read BY NAME: on x64 mFlags sits at +0x138, and +0x130 is the low half of mID.
    if ((lEffectHelper.ActiveRaceCar()->GetFlags() & ActiveRaceCarData::eARDFlagIsCrashing) != 0)
    {
        // [DIAG] BRN_WHEEL_SMOKE_DIAG -- NOT IN THE X360 BINARY, default OFF, 8 lines + the tally. DELETE-WHEN-STABLE.
        if (WheelSmokeDiagArmed())
        {
            ++Tally().mauGated[lCarState.muRaceCarIndex & 7u];
            static u32 suGateLines = 0;
            if (suGateLines < 8u)
            {
                ++suGateLines;
                char lacLine[160];
                std::snprintf(lacLine, sizeof(lacLine),
                              "[wheel-smoke] t=%.3f car %u crash gate: wheel %u silenced "
                              "(ActiveRaceCarData::mFlags=0x%04X, eARDFlagIsCrashing)\n",
                              lCarState.GetTime(), lCarState.muRaceCarIndex, mWheelIndex,
                              static_cast<u32>(lEffectHelper.ActiveRaceCar()->GetFlags()));
                CgsDev::Log::WriteToLog(lacLine);
            }
            WheelSmokeTallyLine(lCarState.GetTime());
        }
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
    const f32 lfWheelSpeed        = GuardedMagnitude(lWheelVelocity);   // 0x82293F6C..0x82293FDC
    const f32 lfWheelMaxSpeed     = rw::math::fpu::Max(lfWheelSpeed, lfWheelSurfaceSpeed);
    const f32 lfMaxWheelTravel    = lCarState.GetDt() * lfWheelMaxSpeed;   // +0x10
    if (lfWheelMaxSpeed < KF_WHEEL_VELOCITY_THRESHOLD)
    {
        return;
    }

    // A backward "reverse-thrust" velocity, ramping in below the cutoff speed and scaled
    // by the tyre-surface speed, applied along the car's forward (Z) axis (negated).
    const BrnPhysics::Vehicle::RaceCarState* lpCarState = lCarState.mpCarState;
    // 0x82294058 fsel(-x, 0.0, x) = Max(0, x) -- zero first: a NaN ramp stays NaN, -0 becomes +0.
    const f32 lfReverseThrustScale = rw::math::fpu::Max(
        0.0f, (KF_WHEEL_REVERSE_THRUST_CUTOFF_SPEED - lfWheelSpeed) * KF_WHEEL_REVERSE_THRUST_SCALE);
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
// FireNativeParticle  @0x82288C30 (DWARF WheelStateMachine.h:87) -- 129 instructions; ONE tyre-smoke
// particle (FX-CRASHVFX 2026-09-25, C1). Four draws on the effects module's ring (helper +8 -> +0x2C3C0),
// in this order, then SpawnWheelSmoke:
//   size     RandomFloat() * flt_82011C1C + flt_82011C18, ONE fmadds (0x82288C90..0x82288CC4)
//   reverse  `lwz r8, 0(r3)`: wheels 2 and 0 turn their smoke the other way (0x82288CC0..0x82288CDC)
//   spread   the Vector-slot quad ((cursor + 3) & 4) less 1.0 -- RandomVector over [0, 1): two LCG steps
//            packed into three slots, the cursor left at slot + 3 (0x82288CE0..0x82288DC8)
//   kick     RandomFloat() * lfKickUpSpeed, `fmuls` (0x82288DD4..0x82288DF0), on y only: (0, kick, 0, 0)
//   velocity ((spread - 0.5) * K_VELOCITY_SPREAD + (wheel velocity * K_VELOCITY_INHERITANCE + kick))
//            + the backward-emission bias -- `vmaddfp v11, v2, v9, v11` (A*C + B), `vsubfp v0, v13, v0`,
//            `vmaddfp v0, v0, v11, v12`, `vaddfp v2, v0, v4` (0x82288E1C..0x82288E28), each lane fused
//            where it is a vmaddfp, rounded where it is not
//   spawn    SpawnWheelSmoke(spawn position (`vmr v1, v3`), velocity, type, size, spawn time (f2 <- f1),
//            the angular-velocity scale (f3, untouched), reverse (r8)); lWheelPosition and lCarState are
//            passed and never read.
// =====================================================================================
void WheelStateMachine::FireNativeParticle(Vector3 /*lWheelPosition*/, Vector3 lWheelVelocity,
                                           u32 leParticleType, Vector3 lSpawnPosition,
                                           RaceCarParticleEffectHelper& lEffectHelper,
                                           CarState& /*lCarState*/,
                                           f32 lfSpawnTime, f32 lfKickUpSpeed,
                                           Vector3 lvBackwardEmissionBias,
                                           f32 lfAngularVelocityScale)
{
    CgsNumeric::Random& lRandom = lEffectHelper.GetEffectsModule()->RandomNumberGenerator();

    const f32 lfSize = std::fma(lRandom.RandomFloat(), KF_WHEEL_SMOKE_SIZE_RANGE, KF_WHEEL_SMOKE_SIZE_MIN);
    const bool lbReverseRotation = (mWheelIndex == 2u) || (mWheelIndex == 0u);

    Vector3 lvZero, lvOne;
    lvZero.x = 0.0f; lvZero.y = 0.0f; lvZero.z = 0.0f; lvZero.w = 0.0f;
    lvOne.x  = 1.0f; lvOne.y  = 1.0f; lvOne.z  = 1.0f; lvOne.w  = 1.0f;
    const Vector3 lvUnit = lRandom.RandomVector(lvZero, lvOne);   // (1 - 0) * t + 0 == t, all four lanes

    Vector3 lvKick;
    lvKick.x = 0.0f;
    lvKick.y = lRandom.RandomFloat() * lfKickUpSpeed;
    lvKick.z = 0.0f;
    lvKick.w = 0.0f;

    const f32 lafUnit[4]    = { lvUnit.x, lvUnit.y, lvUnit.z, lvUnit.w };
    const f32 lafWheelVel[4]= { lWheelVelocity.x, lWheelVelocity.y, lWheelVelocity.z, lWheelVelocity.w };
    const f32 lafKick[4]    = { lvKick.x, lvKick.y, lvKick.z, lvKick.w };
    const f32 lafInherit[4] = { K_VELOCITY_INHERITANCE.x, K_VELOCITY_INHERITANCE.y,
                                K_VELOCITY_INHERITANCE.z, K_VELOCITY_INHERITANCE.w };
    const f32 lafSpread[4]  = { K_VELOCITY_SPREAD.x, K_VELOCITY_SPREAD.y, K_VELOCITY_SPREAD.z, K_VELOCITY_SPREAD.w };
    const f32 lafBias[4]    = { lvBackwardEmissionBias.x, lvBackwardEmissionBias.y,
                                lvBackwardEmissionBias.z, lvBackwardEmissionBias.w };
    f32 lafVelocity[4];
    for (u32 luLane = 0; luLane < 4u; ++luLane)
    {
        const f32 lfInherited = std::fma(lafWheelVel[luLane], lafInherit[luLane], lafKick[luLane]);
        const f32 lfCentred   = lafUnit[luLane] - 0.5f;
        lafVelocity[luLane]   = std::fma(lfCentred, lafSpread[luLane], lfInherited) + lafBias[luLane];
    }
    Vector3 lvVelocity;
    lvVelocity.x = lafVelocity[0];
    lvVelocity.y = lafVelocity[1];
    lvVelocity.z = lafVelocity[2];
    lvVelocity.w = lafVelocity[3];

    lEffectHelper.ParticleModule().SpawnWheelSmoke(
        lSpawnPosition, lvVelocity, static_cast<BrnParticle::Native::ENativeParticleType>(leParticleType),
        lfSize, lfSpawnTime, lfAngularVelocityScale, lbReverseRotation);
}
} // namespace BrnEffects
