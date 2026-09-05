// =============================================================================
// GameSource/Effects/Boost/BoostStateMachine.cpp  (X360 ARTIST)
//
// BrnEffects::BoostStateMachine -- the effects state machine that drives a race
// car's boost / exhaust-smoke / exhaust-pop particle effects. Reconstructed for
// SEMANTIC PARITY from the ARTIST X360 pseudocode + asm:
//
//   OnConstruct          @ 0x82280620   OnChangeState        @ 0x8229E3D8
//   OnDetermineNextState @ 0x82288230   OnTick               @ 0x82298BE0
//   Reset                @ 0x82298B20   SetWorldIndex        @ 0x82280578
//   SetBlendValue        @ 0x82280660   SetEnabled           @ 0x82280710
//   StartEffects         @ 0x8229B920   StopEffects          @ 0x822990C0
//
// The Set*/Start/Stop helpers all walk the live tagged effects, resolve each tag
// to its playing-LION slot via ParticleModule::GetLionEffect (inlined in the X360
// build as `module[handle & 0x7F]` with a stored-handle match), and poke the slot.
// =============================================================================

#include "GameSource/Effects/Boost/BoostStateMachine.h"
#include "GameSource/Effects/EffectsStateMachine.h"
#include "GameSource/Effects/EffectsModule.h"                 // BrnEffects::CarState
#include "GameSource/Effects/ParticleEffectHelper.h"          // ParticleEffectHelper / RaceCarParticleEffectHelper
#include "GameSource/Effects/Particles/ParticleModule.h"      // BrnParticle::ParticleModule, LionEffect
#include "GameSource/Effects/Particles/BrnParticleDescription.h"  // BrnParticle::ParticleDescription::HashString
#include "GameSource/Effects/Curves.h"                        // BrnEffects::Curves::SmoothStep
#include "GameSource/Effects/BrnEffectsDebugComponent.h"      // BrnEffects::EffectsDebugComponent
#include "GameSource/Replays/Serialisers/BrnReplayEffectsSerialiser.h"  // EffectsSerialiser(StaticLayout)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // BrnPhysics::Vehicle::RaceCarState (wheels + transform)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnVehicleLocatorData.h"  // VehicleLocatorData / Output / ETagPointType
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the [boost*] witnesses
#include <cmath>                                              // sqrtf
#include <cstdio>                                             // snprintf (witnesses)

namespace BrnEffects
{

// =============================================================================
// File-scope rodata recovered from the X360 build.
// =============================================================================

namespace
{
    // The vehicle speed (m/s) above which the exhaust-pop transition is suppressed
    // (rodata flt_8200DD34 == 13.4112, ~= 30 mph). DWARF KF_EXHAUST_MAX_VEHICLE_SPEED.
    const f32 KF_EXHAUST_MAX_VEHICLE_SPEED = 13.4112f;

    // The default boost-transition smoothstep input range (rodata flt_8200DD24 /
    // flt_8200DD28) used when the debug component is NOT overriding it.
    const f32 KF_BOOST_TRANSITION_MIN_DEFAULT = 3.0f;
    const f32 KF_BOOST_TRANSITION_MAX_DEFAULT = 6.0f;

    // The exhaust-smoke intensity scale applied to the max of (vehicle speed, wheel
    // surface speeds) in the ExhaustSmokeOn state (rodata flt_820119C8).
    const f32 KF_EXHAUST_SMOKE_SPEED_SCALE = 0.074564546f;

    // EXHAUST_EFFECT (DWARF BoostStateMachine.cpp:27, rodata off_82CDAE48). The
    // exhaust-smoke LION effect started while in ExhaustSmokeOn.
    const char* const KAC_EXHAUST_EFFECT =
        "gamedb://burnout5/Burnout/Effects/ExhaustSmoke.lef.BurnoutFXLionEffectFile?ID=323059";

    // BOOST_EFFECTS[3] (DWARF BoostStateMachine.cpp:29, rodata off_82CDAE4C), indexed
    // by CarState::meBoostType (BrnWorld::EBoostType: 0=Danger, 1=Aggression, 2=Stunt).
    // The count is the number of boost TYPES (3), distinct from KU_MAX_BOOST_EFFECTS
    // (the 4 effect SLOTS).
    //
    // ⭐ CORRECTED 2026-09-03 (boost-exhaust wave). The three strings that stood here
    // ("BoostDanger" / "BoostAggression" / "BoostStunt") were INVENTED placeholders whose
    // own banner said the suffixes "could not be recovered from the binary". They CAN be
    // read out of the image, and they are none of those three names. The pointer array is
    // at rodata 0x82CDAE4C (OnChangeState @0x8229E3D8 indexes `off_82CDAE4C[meBoostType]`);
    // its three words and the strings they point at, read with tools/re/x360rd.py:
    //     0x82CDAE4C -> 0x8200D818  "...BoostYellow.lef...?ID=560104"
    //     0x82CDAE50 -> 0x8200D7C0  "...BoostRed.lef...?ID=559640"
    //     0x82CDAE54 -> 0x8200D768  "...BoostGreen.lef...?ID=560102"
    // (0x82CDAE48 -> 0x8200D870 is the ExhaustSmoke string above, so the array base is
    // right; the three that follow at 0x82CDAE58/5C/60 are the Cam_* effects of a
    // neighbouring table, so the array is exactly 3 long.)
    //
    // ⭐⭐ THE NAME IS THE LOOKUP KEY, SO A WRONG NAME IS A SILENT TOTAL FAILURE.
    // StartEffects hashes the string with ParticleDescription::HashString (FNV-1a,
    // case-folded) and ParticleModule::StartLionEffect @0x82289F50 matches that hash
    // against the ParticleDescriptionCollection; a miss takes the console's
    // "Couldn't locate lion effect description ... Does the Particles Bundle need
    // rebuilding ?" exit and returns KU_HANDLE_INVALID. MEASURED both ways against the
    // shipped X360 PARTICLES.BUNDLE and our ported copy:
    //     BoostYellow  0x82B1D830  present (X360 BE @2868, ported LE @2864)
    //     BoostRed     0xD07A9AFA  present (X360 BE @4916, ported LE @4912)
    //     BoostGreen   0x658A7FD9  present (X360 BE @2420, ported LE @2416)
    //     "BoostDanger" 0x7DAC27A0 ABSENT FROM BOTH BUNDLES
    // so the placeholders could never have started a boost effect on any build.
    const u32 KU_NUM_BOOST_TYPES = 3;
    const char* const KAC_BOOST_EFFECTS[KU_NUM_BOOST_TYPES] =
    {
        // EBoostType 0 == E_BOOST_TYPE_DANGER      (rodata 0x8200D818)
        "gamedb://burnout5/Burnout/Effects/BoostYellow.lef.BurnoutFXLionEffectFile?ID=560104",
        // EBoostType 1 == E_BOOST_TYPE_AGGRESSION  (rodata 0x8200D7C0)
        "gamedb://burnout5/Burnout/Effects/BoostRed.lef.BurnoutFXLionEffectFile?ID=559640",
        // EBoostType 2 == E_BOOST_TYPE_STUNT       (rodata 0x8200D768)
        "gamedb://burnout5/Burnout/Effects/BoostGreen.lef.BurnoutFXLionEffectFile?ID=560102",
    };

    // The boost-recharge effect started on entry to ExhaustPopRestart (rodata,
    // full string recovered from the binary).
    const char* const KAC_BOOST_RECHARGE_EFFECT =
        "gamedb://burnout5/Burnout/Effects/BoostRecharge.lef.BurnoutFXLionEffectFile?ID=321336";

    // The number of boost nozzle locators (E_TAGPOINT_FXBOOSTPOINT1..4).
    const u32 KU_NUM_BOOST_LOCATORS = 4;

    // ParticleModule::GetLionEffect was inlined into each of the Set*/Start/Stop loops
    // in the X360 build. The assert it carries (luArrayIndex < KU_MAX_PLAYING_EFFECTS,
    // BoostStateMachine.cpp line 0x17E == 382) cannot fire (the handle index mask is
    // 7 bits, < 128 == KU_MAX_PLAYING_EFFECTS) but is reproduced for parity.
    const u32 KU_MAX_PLAYING_EFFECTS = 128;

    inline bool BoostLocatorIsActive(BrnPhysics::Deformation::ETagPointType leTagType,
                                     u32 luBoostIndex, u32 luNumBoostTags)
    {
        // The generic locator's tag type selects which boost effect it drives, and the
        // effect is only updated if the state machine actually has that tag.
        return luBoostIndex < luNumBoostTags;
    }

    // Transform a boost locator's frame into the effect's world frame using the car's
    // world transform. The X360 build inlines this as a hand-unrolled VMX sequence
    // (vspltw the locator rows, vmulfp128 by the car X axis, vmaddfp the car Y axis +
    // translation); reconstructed here as the equivalent affine compose.
    // UNCERTAIN: the exact VMX algebra (which car-matrix rows scale vs. translate) is the
    // highest-risk part of this TU -- see the postmortem note in the dossier.
    inline Matrix44Affine TransformBoostLocator(const Matrix44Affine& lCarWorld,
                                                const Matrix44Affine& lLocatorLocal)
    {
        Matrix44Affine lWorld;
        for (int liRow = 0; liRow < 4; ++liRow)
        {
            const Vector3& lLocal = (liRow == 0) ? lLocatorLocal.xAxis
                                  : (liRow == 1) ? lLocatorLocal.yAxis
                                  : (liRow == 2) ? lLocatorLocal.zAxis
                                                 : lLocatorLocal.wAxis;
            const f32 lfTranslate = (liRow == 3) ? 1.0f : 0.0f;   // only the position row picks up the car origin
            Vector3& lOut = (liRow == 0) ? lWorld.xAxis
                          : (liRow == 1) ? lWorld.yAxis
                          : (liRow == 2) ? lWorld.zAxis
                                         : lWorld.wAxis;
            lOut.x = lLocal.x * lCarWorld.xAxis.x + lLocal.y * lCarWorld.yAxis.x
                   + lLocal.z * lCarWorld.zAxis.x + lfTranslate * lCarWorld.wAxis.x;
            lOut.y = lLocal.x * lCarWorld.xAxis.y + lLocal.y * lCarWorld.yAxis.y
                   + lLocal.z * lCarWorld.zAxis.y + lfTranslate * lCarWorld.wAxis.y;
            lOut.z = lLocal.x * lCarWorld.xAxis.z + lLocal.y * lCarWorld.yAxis.z
                   + lLocal.z * lCarWorld.zAxis.z + lfTranslate * lCarWorld.wAxis.z;
            lOut.w = 0.0f;
        }
        return lWorld;
    }
}

// =============================================================================
// OnConstruct @ 0x82280620
//   Reset the boost state machine's added state: zero the tag count / state /
//   exhaust delta, invalidate the 4 tag handles, prime mfExhaustSmokeBlend to 1.0.
// =============================================================================
void BoostStateMachine::OnConstruct()
{
    mTime              = 0.0f;          // +0x08  (base mTime / state timer)
    muNumBoostTags     = 0;             // +0x0C
    mState             = EffectsStateAllOff;  // +0x04 (base mState = 0)
    for (u32 lu = 0; lu < KU_MAX_BOOST_EFFECTS; ++lu)
    {
        mEffects[lu].muHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;  // +0x10.. = -1
    }
    mfExhaustSmokeBlend = 1.0f;         // +0x20
}

// =============================================================================
// SetBlendValue @ 0x82280660
//   Set the state blend on every live boost effect slot (and flag it changed).
// =============================================================================
void BoostStateMachine::SetBlendValue(ParticleEffectHelper& lParticleEffectHelper, f32 lfBlend)
{
    BrnParticle::ParticleModule& lModule = lParticleEffectHelper.ParticleModule();
    for (u32 lu = 0; lu < muNumBoostTags; ++lu)
    {
        CGS_ASSERT((mEffects[lu].muHandle & 0x7Fu) < KU_MAX_PLAYING_EFFECTS,
                   "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
        BrnParticle::LionEffect* lpEffect = lModule.GetLionEffect(mEffects[lu].muHandle);
        if (lpEffect != NULL)
        {
            lpEffect->mfStateBlend = lfBlend;
            lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
        }
    }
}

// =============================================================================
// SetEnabled @ 0x82280710
//   Enable (flags |= ENABLED|CHANGED) or disable (clear ENABLED, set CHANGED)
//   every live boost effect slot.
// =============================================================================
void BoostStateMachine::SetEnabled(ParticleEffectHelper& lParticleEffectHelper, bool lbEnabled)
{
    BrnParticle::ParticleModule& lModule = lParticleEffectHelper.ParticleModule();
    for (u32 lu = 0; lu < muNumBoostTags; ++lu)
    {
        CGS_ASSERT((mEffects[lu].muHandle & 0x7Fu) < KU_MAX_PLAYING_EFFECTS,
                   "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
        BrnParticle::LionEffect* lpEffect = lModule.GetLionEffect(mEffects[lu].muHandle);
        if (lpEffect != NULL)
        {
            if (lbEnabled)
            {
                lpEffect->muFlags |= (BrnParticle::LionEffect::EPPE_FLAG_ENABLED
                                    | BrnParticle::LionEffect::EPPE_FLAG_CHANGED);
            }
            else
            {
                lpEffect->muFlags = static_cast<u16>(
                    (lpEffect->muFlags & ~BrnParticle::LionEffect::EPPE_FLAG_ENABLED)
                    | BrnParticle::LionEffect::EPPE_FLAG_CHANGED);
            }
        }
    }
}

// =============================================================================
// SetWorldIndex @ 0x82280578
//   Set the world index on every live boost effect slot (and flag it changed).
// =============================================================================
void BoostStateMachine::SetWorldIndex(ParticleEffectHelper& lParticleEffectHelper, u32 luWorldIndex)
{
    BrnParticle::ParticleModule& lModule = lParticleEffectHelper.ParticleModule();
    for (u32 lu = 0; lu < muNumBoostTags; ++lu)
    {
        CGS_ASSERT((mEffects[lu].muHandle & 0x7Fu) < KU_MAX_PLAYING_EFFECTS,
                   "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
        BrnParticle::LionEffect* lpEffect = lModule.GetLionEffect(mEffects[lu].muHandle);
        if (lpEffect != NULL)
        {
            lpEffect->muWorldIndex = luWorldIndex;
            lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
        }
    }
}

// =============================================================================
// StopEffects @ 0x822990C0
//   Stop every live boost effect (resolved slot) and invalidate the tag handles.
//   Does NOT clear the tag count.
// =============================================================================
void BoostStateMachine::StopEffects(ParticleEffectHelper& lParticleEffectHelper)
{
    BrnParticle::ParticleModule& lModule = lParticleEffectHelper.ParticleModule();
    for (u32 lu = 0; lu < muNumBoostTags; ++lu)
    {
        CGS_ASSERT((mEffects[lu].muHandle & 0x7Fu) < KU_MAX_PLAYING_EFFECTS,
                   "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
        BrnParticle::LionEffect* lpEffect = lModule.GetLionEffect(mEffects[lu].muHandle);
        if (lpEffect != NULL)
        {
            lModule.StopLionEffect(lpEffect);
        }
        mEffects[lu].muHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;
    }
}

// =============================================================================
// Reset @ 0x82298B20
//   Stop every live boost effect, invalidate the tags, then clear the tag count,
//   the state and the exhaust-smoke blend.
// =============================================================================
void BoostStateMachine::Reset(ParticleEffectHelper& lParticleEffectHelper)
{
    BrnParticle::ParticleModule& lModule = lParticleEffectHelper.ParticleModule();
    for (u32 lu = 0; lu < muNumBoostTags; ++lu)
    {
        CGS_ASSERT((mEffects[lu].muHandle & 0x7Fu) < KU_MAX_PLAYING_EFFECTS,
                   "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
        BrnParticle::LionEffect* lpEffect = lModule.GetLionEffect(mEffects[lu].muHandle);
        if (lpEffect != NULL)
        {
            lModule.StopLionEffect(lpEffect);
        }
        mEffects[lu].muHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;
    }

    muNumBoostTags     = 0;       // +0x0C
    mState             = EffectsStateAllOff;  // +0x04
    mfExhaustSmokeBlend = 0.0f;   // +0x20  (rodata flt_82001CC0 == 0.0)
}

// =============================================================================
// StartEffects @ 0x8229B920
//   For each tag slot: stop any existing playing effect, then start the named LION
//   effect (keying it by ParticleDescription::HashString) at the given world index,
//   storing the new handle back into the tag.
// =============================================================================
void BoostStateMachine::StartEffects(ParticleEffectHelper& lParticleEffectHelper,
                                     const char* lpcEffectName, u32 luWorldIndex)
{
    BrnParticle::ParticleModule& lModule = lParticleEffectHelper.ParticleModule();
    for (u32 lu = 0; lu < muNumBoostTags; ++lu)
    {
        CGS_ASSERT((mEffects[lu].muHandle & 0x7Fu) < KU_MAX_PLAYING_EFFECTS,
                   "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
        BrnParticle::LionEffect* lpExisting = lModule.GetLionEffect(mEffects[lu].muHandle);
        if (lpExisting != NULL)
        {
            lModule.StopLionEffect(lpExisting);
        }
        mEffects[lu].muHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;

        const u32 luNameHash = BrnParticle::ParticleDescription::HashString(lpcEffectName);
        mEffects[lu].muHandle = lModule.StartLionEffect(luNameHash, lpcEffectName, luWorldIndex);
    }

    // ---- [boostfx] witness. NOT console behaviour: ours, bounded, log-only. ---------
    // PRINTED OUTSIDE THE LOOP, DELIBERATELY. The loop above is bounded by muNumBoostTags,
    // so with a zero tag count StartEffects is a no-op that leaves NOTHING in the log --
    // and "the boost effect did not start" then looks identical to "StartEffects was never
    // called". This line separates the two and names the count that decided it, plus the
    // handle slot 0 actually got (KU_HANDLE_INVALID == the name did not resolve).
    // DELETE-WHEN-STABLE.
    {
        static u32 suStartWitness = 0;
        const u32 KU_START_WITNESS_LIMIT = 12;
        if (suStartWitness < KU_START_WITNESS_LIMIT)
        {
            ++suStartWitness;
            char lacMsg[288];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[boostfx] #%u StartEffects(\"%s\", world=%u) muNumBoostTags=%u handle0=%08X%s\n",
                suStartWitness, lpcEffectName ? lpcEffectName : "<NULLSTRING>",
                luWorldIndex, muNumBoostTags,
                (muNumBoostTags != 0) ? mEffects[0].muHandle : 0u,
                (muNumBoostTags == 0)
                    ? "  <-- ZERO: the loop did not execute and no effect started."
                    : "");
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }
}

// =============================================================================
// OnChangeState @ 0x8229E3D8
//   Run on entry to each state: start/stop the relevant effects and seed the
//   exhaust-smoke blend / state timer.
// =============================================================================
void BoostStateMachine::OnChangeState(EffectsState leNewState,
                                      RaceCarParticleEffectHelper& lHelper,
                                      CarState& lCarState)
{
    switch (leNewState)
    {
        case EffectsStateAllOff:                 // 0
            mfExhaustSmokeBlend = 1.0f;          // +0x20 (flt_82001C98)
            StopEffects(lHelper);
            break;

        case EffectsStateBoosting:               // 1
            CGS_ASSERT(lCarState.meBoostType >= 0, "lCarState.meBoostType >= 0");
            CGS_ASSERT(lCarState.meBoostType < 3, "Boost type out of range: ");
            StartEffects(lHelper, KAC_BOOST_EFFECTS[lCarState.meBoostType], lHelper.WorldIndex());
            break;

        case EffectsStateExhaustSmokeOn:         // 2
            StartEffects(lHelper, KAC_EXHAUST_EFFECT, lHelper.WorldIndex());
            break;

        case EffectsStateExhaustSmokeStopping:   // 3
            // No entry action.
            break;

        case EffectsStateBoostStopping:          // 4
            mTime = 0.75f;                       // +0x08 (flt_82004018)
            SetEnabled(lHelper, false);
            break;

        case EffectsStateExhaustPopRestart:      // 5
        {
            StartEffects(lHelper, KAC_BOOST_RECHARGE_EFFECT, lHelper.WorldIndex());
            // blend = max(0, 1 - rechargeBlend)  (fsel f1, -(1-x), 0.0, 1-x)
            f32 lfBlend = 1.0f - lCarState.mfExhaustPopIntensity;   // CarState +0x48 (`*(a4 + 72)`)
            if (lfBlend <= 0.0f)
            {
                lfBlend = 0.0f;
            }
            SetBlendValue(lHelper, lfBlend);
            break;
        }

        case EffectsStateExhaustPop:             // 6
            mTime = 0.5f;                         // +0x08 (flt_82001DA0)
            break;

        default:
            CGS_ASSERT(false, "Unknown state in BrnEffects::BoostStateMachine");
            break;
    }
}

// =============================================================================
// OnDetermineNextState @ 0x82288230
//   The boost effects transition table. Returns the next EffectsState.
// =============================================================================
EffectsState BoostStateMachine::OnDetermineNextState(CarState& lCarState,
                                                     bool lbStateTimerExpired,
                                                     EffectsState leCurrentState,
                                                     RaceCarParticleEffectHelper& lHelper)
{
    // Early-out: once the car is fully drivable again after a crash, all boost
    // effects are forced off (asm: mpRaceCarState->mbFullyDrivableFromCrash @ +0x452).
    if (lHelper.RaceCarState()->mbFullyDrivableFromCrash)
    {
        return EffectsStateAllOff;
    }

    switch (leCurrentState)
    {
        case EffectsStateAllOff:                 // 0
            if (lCarState.mbIsBoosting)
            {
                return EffectsStateBoosting;
            }
            if (lCarState.mbExhaustPopThisFrame)   // +0x4C (`*(a2 + 76)`)
            {
                return EffectsStateExhaustPopRestart;
            }
            // Engine running, below the exhaust-pop speed cap, not jumping -> start the
            // exhaust smoke; otherwise stay put.
            if (lCarState.mbEngineRunning
                && lCarState.mfSpeedMPH < KF_EXHAUST_MAX_VEHICLE_SPEED
                && !lCarState.mbCrashing)   // +0x4D (`*(a2 + 77)`)
            {
                return EffectsStateExhaustSmokeOn;
            }
            return leCurrentState;

        case EffectsStateBoosting:               // 1
        {
            // Stay boosting while still boosting and (not jumping, or in one of the two
            // game modes where jumping keeps boost VFX: game mode 16 or 2).
            const bool lbKeepBoosting =
                (!lCarState.mbCrashing   // +0x4D (`*(a2 + 77)`)
                 || lHelper.CurrentGameMode() == 16
                 || lHelper.CurrentGameMode() == 2)
                && lCarState.mbIsBoosting;
            if (!lbKeepBoosting)
            {
                return EffectsStateBoostStopping;
            }

            // The boost-transition smoothstep maps the boost-transition progress to the
            // exhaust-smoke blend; the debug component can override the input range.
            f32 lfMin = KF_BOOST_TRANSITION_MIN_DEFAULT;
            f32 lfMax = KF_BOOST_TRANSITION_MAX_DEFAULT;
            const EffectsDebugComponent* lpDebug = lHelper.DebugComponent();
            if (lpDebug != NULL && lpDebug->IsBoostTransitionEditing())
            {
                lfMin = lpDebug->BoostTransitionMin();
                lfMax = lpDebug->BoostTransitionMax();
            }

            const Vector3 lvParams = { lfMin, ((lfMax - lfMin) * 0.5f) + lfMin, lfMax, 0.0f };
            const Vector2 lvScale  = { 0.0f, 1.0f, 0.0f, 0.0f };
            BrnEffects::Curves::SmoothStep lCurve;
            const f32 lfBlend = 1.0f - lCurve.Evaluate(lvParams, lvScale,
                                                       lCarState.mfBoostAmount);   // CarState +0x3C (`v14 = *(a2 + 60)`)
            SetBlendValue(lHelper, lfBlend);
            return leCurrentState;
        }

        case EffectsStateExhaustSmokeOn:         // 2
        {
            if (lCarState.mbIsBoosting)
            {
                return EffectsStateBoosting;
            }
            if (lCarState.mbExhaustPopThisFrame)   // +0x4C (`*(a2 + 76)`)
            {
                return EffectsStateExhaustPopRestart;
            }
            // Over the speed cap, engine off, or jumping -> begin stopping the smoke.
            if (lCarState.mfSpeedMPH >= KF_EXHAUST_MAX_VEHICLE_SPEED
                || !lCarState.mbEngineRunning
                || lCarState.mbCrashing)   // +0x4D (`*(a2 + 77)`)
            {
                return EffectsStateExhaustSmokeStopping;
            }

            // Exhaust-smoke intensity = clamp(max(|linearVelocity|, per-wheel surface
            // speed) * KF_EXHAUST_SMOKE_SPEED_SCALE, 0, 1). The X360 inlines the vector
            // magnitude (vmsum3fp128 + rsqrt refine) and the running max via fsel.
            const BrnPhysics::Vehicle::RaceCarState& lCar = *lCarState.mpCarState;
            const Vector3& lvVel = lCar.mLinearVelocity;
            f32 lfMax = sqrtf(lvVel.x * lvVel.x + lvVel.y * lvVel.y + lvVel.z * lvVel.z);
            for (int liWheel = 0; liWheel < 4; ++liWheel)
            {
                const f32 lfWheelSurfaceSpeed =
                    lCar.maWheels[liWheel].mfRadiansPerSecond * lCar.maWheels[liWheel].mfRadius;
                if (lfWheelSurfaceSpeed > lfMax)
                {
                    lfMax = lfWheelSurfaceSpeed;
                }
            }

            f32 lfIntensity = lfMax * KF_EXHAUST_SMOKE_SPEED_SCALE;
            if (lfIntensity <= 0.0f)
            {
                lfIntensity = 0.0f;
            }
            if (lfIntensity > 1.0f)
            {
                lfIntensity = 1.0f;
            }
            mfExhaustSmokeBlend = lfIntensity;   // +0x20
            SetBlendValue(lHelper, lfIntensity);
            return leCurrentState;
        }

        case EffectsStateExhaustSmokeStopping:   // 3
            if (lCarState.mbIsBoosting)
            {
                return EffectsStateBoosting;
            }
            if (lCarState.mbExhaustPopThisFrame)   // +0x4C (`*(a2 + 76)`)
            {
                return EffectsStateExhaustPopRestart;
            }
            // Fade the smoke out; once fully blended (>= 1.0) the effects are all off.
            mfExhaustSmokeBlend += lCarState.GetDt();   // `*(a1+32) = *(a2+16) + *(a1+32)` == CarState +0x10
            SetBlendValue(lHelper, mfExhaustSmokeBlend);
            if (mfExhaustSmokeBlend < 1.0f)
            {
                return leCurrentState;
            }
            return EffectsStateAllOff;

        case EffectsStateBoostStopping:          // 4
            if (lCarState.mbIsBoosting)
            {
                return EffectsStateBoosting;
            }
            // Otherwise: timer-expired -> AllOff, else stay.
            return lbStateTimerExpired ? EffectsStateAllOff : leCurrentState;

        case EffectsStateExhaustPopRestart:      // 5
            return EffectsStateExhaustPop;

        case EffectsStateExhaustPop:             // 6
            if (lCarState.mbIsBoosting)
            {
                return EffectsStateBoosting;
            }
            if (lCarState.mbExhaustPopThisFrame)   // +0x4C (`*(a2 + 76)`)
            {
                return EffectsStateExhaustPopRestart;
            }
            // (the EffectsStateBoostStopping fall-through): timer -> AllOff, else stay.
            return lbStateTimerExpired ? EffectsStateAllOff : leCurrentState;

        default:
            CGS_ASSERT(false, "Unknown state");
            return leCurrentState;
    }
}

// =============================================================================
// OnTick @ 0x82298BE0
//   Per frame: refresh the boost-effect world transforms from the car's boost
//   locators (or stop the effects with no live locator), and round-trip the boost
//   locators through the replay effects serialiser while recording / playing.
// =============================================================================
void BoostStateMachine::OnTick(CarState& lCarState, RaceCarParticleEffectHelper& lHelper)
{
    // Whether to run the locator update this frame.
    bool lbDoLocatorUpdate = false;

    // Debug override: force the state blend to the editor value, then always update.
    const EffectsDebugComponent* lpDebug = lHelper.DebugComponent();
    if (lpDebug != NULL && lpDebug->IsForceStateBlend())
    {
        SetBlendValue(lHelper, lpDebug->ForceStateBlendValue());
        lbDoLocatorUpdate = true;
    }
    else if (lHelper.VehicleLocators() != NULL)
    {
        // Live locators available -> update from them.
        lbDoLocatorUpdate = true;
    }
    else
    {
        // No live locators: only update while the replay is PLAYING back (serialiser
        // EMode 4/5/6), so the recorded locators drive the effects.
        const BrnReplays::EffectsSerialiser::EMode leMode = lCarState.mpEffectsSerialiser->GetMode();
        if (leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING_PREPARING
            || leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING
            || leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING_STALLED)
        {
            lbDoLocatorUpdate = true;
        }
    }

    if (!lbDoLocatorUpdate)
    {
        // ================================================================================
        // ⚠ FLAG PC BRING-UP STAND-IN -- NOT CONSOLE BEHAVIOUR. DELETE-WHEN the deformation
        // module publishes VehicleLocatorOutput for race cars on this build.
        //
        // WHY IT IS HERE. Every arm above needs a locator source, and on this build there is
        // none: EffectsModule's per-car lookup (EffectsModule.cpp, the [deformloc] witness)
        // searches DeformationOutputInterface::maLocatorData for this car's entity id and
        // finds nothing, so RaceCarParticleEffectHelper::VehicleLocators() is null; and the
        // replay serialiser is not playing. The console would then leave the effects where
        // they were -- but on this build they have never been anywhere, so they sit at the
        // WORLD ORIGIN and cParticleRender::Render culls every one of them by range. Measured:
        //     [lionfx] cull#1 cam=(3003.57,-2.54,-1938.61) ... emit=(0.00,0.00,0.00)
        //              distSq=12779648.0
        // with `[boosttag] ExtractTags RAN: locators=5 boostTags=2` -- so the car's spec DOES
        // carry FXBOOSTPOINT1/2 (tag-type bits 41 and 42); it is only the per-frame DEFORMED
        // locator table that never arrives.
        //
        // WHAT IT DOES, AND WHAT IT DELIBERATELY DOES NOT. It puts each live boost effect at
        // the car's own world transform -- the console's own idiom for an effect that follows
        // a car (HandleJumpAndLandingEffects @0x82288068 does exactly
        // `lpEffect->SetTransform(lrHelper.RaceCarState()->mTransform)` for the jump effect).
        // It does NOT invent the nozzle offset: the real transform is
        // TransformBoostLocator(carWorld, locator) a few lines below, and until the locator
        // exists any offset here would be a number nothing attests. So the exhaust plume rides
        // the car's origin rather than its tailpipes, and it is off by the nozzle offset ONLY.
        // The moment VehicleLocators() is non-null this arm stops running and the console path
        // takes over untouched.
        // ================================================================================
        const BrnPhysics::Vehicle::RaceCarState* lpCar = lHelper.RaceCarState();
        if (lpCar != NULL && muNumBoostTags != 0)
        {
            for (u32 luBoost = 0; luBoost < muNumBoostTags && luBoost < KU_NUM_BOOST_LOCATORS;
                 ++luBoost)
            {
                lHelper.SetEffectTransform(mEffects[luBoost].muHandle, lpCar->mTransform);
            }

            // [boostloc] the stand-in's own witness, latched on the car moving into a new
            // 8-metre cell so a stationary car costs one line. DELETE with the arm above.
            static s32 siLastCell = 0x7FFFFFFF;
            const s32 liCell = static_cast<s32>(lpCar->mTransform.wAxis.x * 0.125f)
                             ^ (static_cast<s32>(lpCar->mTransform.wAxis.z * 0.125f) << 12);
            if (liCell != siLastCell)
            {
                siLastCell = liCell;
                char lacMsg[224];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[boostloc] STAND-IN: no VehicleLocatorOutput; %u boost effect(s) placed at"
                    " the car transform (%.2f,%.2f,%.2f)\n",
                    muNumBoostTags, lpCar->mTransform.wAxis.x, lpCar->mTransform.wAxis.y,
                    lpCar->mTransform.wAxis.z);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
        return;
    }

    // The serialiser EMode decides whether we READ the recorded locators (PLAYING:
    // 4/5/6) or COMPUTE them from the vehicle this frame.
    const BrnReplays::EffectsSerialiser::EMode leMode = lCarState.mpEffectsSerialiser->GetMode();
    const bool lbPlaying =
        (leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING_PREPARING
         || leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING
         || leMode == BrnReplays::EffectsSerialiser::E_MODE_PLAYING_STALLED);

    // The per-effect world transforms and the active-effect bitmask.
    Matrix44Affine laBoostTransforms[4];
    for (u32 luSeed = 0; luSeed < 4u; ++luSeed)
        laBoostTransforms[luSeed].SetIdentity();   // [boostloc]: the witness below reads slot 0 even when no tag is active
    u8 luActiveMask = 0;

    if (lbPlaying)
    {
        // Read the recorded boost locators for this race car.
        BrnReplays::EffectsSerialiserStaticLayout* lpLayout =
            lCarState.mpEffectsSerialiser->GetStaticLayout();
        lpLayout->GetBoostLocators(lCarState.muRaceCarIndex, luActiveMask, laBoostTransforms);
    }
    else
    {
        // Compute the boost locators from the vehicle's deformation locator table.
        const BrnPhysics::Deformation::VehicleLocatorData* lpLocators =
            lHelper.VehicleLocators()->mpLocatorData;
        const BrnPhysics::Vehicle::RaceCarState* lpCar = lHelper.RaceCarState();
        const Matrix44Affine& lCarWorld = lpCar->mTransform;

        const s32 liNumLocators = lpLocators->miNumGenericLocators;
        for (s32 li = 0; li < liNumLocators; ++li)
        {
            const BrnPhysics::Deformation::ETagPointType leTag = lpLocators->maGenericLocatorTypes[li];

            // Each of the four boost nozzle tag points drives one boost effect, but only
            // if the state machine actually has that tag.
            for (u32 luBoost = 0; luBoost < KU_NUM_BOOST_LOCATORS; ++luBoost)
            {
                const BrnPhysics::Deformation::ETagPointType leBoostTag =
                    static_cast<BrnPhysics::Deformation::ETagPointType>(
                        BrnPhysics::Deformation::E_TAGPOINT_FXBOOSTPOINT1 + luBoost);
                if (leTag == leBoostTag && BoostLocatorIsActive(leTag, luBoost, muNumBoostTags))
                {
                    laBoostTransforms[luBoost] =
                        TransformBoostLocator(lCarWorld, lpLocators->maGenericLocators[li]);
                    luActiveMask |= static_cast<u8>(1u << luBoost);
                }
            }
        }
    }

    // ---- [boostloc] witness. NOT console behaviour: ours, bounded, log-only. -----------------
    // MEASURED 2026-09-05 (boost-exhaust wave): every live Lion emitter culls at the world
    // ORIGIN -- `[lionfx] cull#1 cam=(3003.57,-2.54,-1938.61) ... emit=(0.00,0.00,0.00)`. The
    // camera is right; the EFFECT is at (0,0,0). This function is the only thing that ever gives
    // a boost effect a world transform, so the question is which of its four exits it takes: no
    // locator source at all, a locator list with no FXBOOSTPOINT in it, a tag the machine has no
    // slot for, or a transform that really is the identity. Latched on the answer CHANGING, so a
    // steady state costs one line. DELETE-WHEN-STABLE.
    {
        static u32 suLastKey = 0xFFFFFFFFu;
        const u32 luKey = (static_cast<u32>(luActiveMask) << 8) | muNumBoostTags;
        if (luKey != suLastKey)
        {
            suLastKey = luKey;
            const BrnPhysics::Deformation::VehicleLocatorOutput* lpLocOut = lHelper.VehicleLocators();
            s32 liNum = -1;
            if (lpLocOut != NULL && lpLocOut->mpLocatorData != NULL)
                liNum = lpLocOut->mpLocatorData->miNumGenericLocators;
            const BrnPhysics::Vehicle::RaceCarState* lpCarDiag = lHelper.RaceCarState();
            char lacMsg[320];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[boostloc] tags=%u activeMask=%02X playing=%d locOut=%p genericLocators=%d"
                " car=(%.2f,%.2f,%.2f) fx0=(%.2f,%.2f,%.2f)\n",
                muNumBoostTags, (unsigned)luActiveMask, (int)lbPlaying,
                static_cast<const void*>(lpLocOut), (int)liNum,
                lpCarDiag ? lpCarDiag->mTransform.wAxis.x : 0.0f,
                lpCarDiag ? lpCarDiag->mTransform.wAxis.y : 0.0f,
                lpCarDiag ? lpCarDiag->mTransform.wAxis.z : 0.0f,
                laBoostTransforms[0].wAxis.x, laBoostTransforms[0].wAxis.y,
                laBoostTransforms[0].wAxis.z);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // Apply each boost effect: set its world transform when active, else stop it.
    for (u32 luBoost = 0; luBoost < KU_NUM_BOOST_LOCATORS; ++luBoost)
    {
        if ((luActiveMask & (1u << luBoost)) != 0)
        {
            lHelper.SetEffectTransform(mEffects[luBoost].muHandle, laBoostTransforms[luBoost]);
        }
        else
        {
            lHelper.StopEffect(mEffects[luBoost].muHandle);
        }
    }

    // While RECORDING (serialiser EMode 1/2/3) write the computed locators back so the
    // replay can reproduce them.
    const BrnReplays::EffectsSerialiser::EMode leModeAfter = lCarState.mpEffectsSerialiser->GetMode();
    if (leModeAfter == BrnReplays::EffectsSerialiser::E_MODE_RECORDING_PREPARING
        || leModeAfter == BrnReplays::EffectsSerialiser::E_MODE_RECORDING
        || leModeAfter == BrnReplays::EffectsSerialiser::E_MODE_RECORDING_STALLED)
    {
        BrnReplays::EffectsSerialiserStaticLayout* lpLayout =
            lCarState.mpEffectsSerialiser->GetStaticLayout();
        lpLayout->SetBoostLocators(lCarState.muRaceCarIndex, luActiveMask, laBoostTransforms);
    }
}

} // namespace BrnEffects
