// =============================================================================
// GameSource/Effects/Jump/JumpStateMachine.cpp  (X360 ARTIST)
//
// BrnEffects::JumpStateMachine -- the per-race-car effects state machine that drives
// the jump vapour trail and the landing dust / spark / debris effects. Reconstructed
// for SEMANTIC PARITY from the ARTIST X360 pseudocode + asm:
//
//   OnDetermineNextState @ 0x8229B9F8   OnChangeState  @ 0x82299510
//   SetVapourBlend       @ 0x82288A58   OnTick         -- no export (empty base slot)
//   FireWheelSparks      @ 0x82299670   FireWheelDebris @ 0x822939D8  (both PARKED --
//                                       see their banners below)
//
// The machine is ticked once per active car per frame: EffectsModule.cpp:1835 ->
// ActiveRaceCarData::Tick -> EffectsStateMachine::Tick (EffectsStateMachine.cpp:49),
// which calls OnDetermineNextState, then OnChangeState on any transition.
//
// The states this machine uses (EffectsStateMachine.h):
//   AllOff(0) -> Jumping(7) -> JumpingMovingDown(9) -> Landed(10) ->
//   FiringSparks(11) -> LandedWaiting(12) -> JumpingFinishing(13) -> AllOff(0)
// (LandedWaiting re-arms straight back to Jumping while CarState::mbJumping is set.)
//
// SetVapourBlend @ 0x82288A58
//   Compute and apply the jump-vapour LION effect's state blend. The car's linear
//   velocity direction and world Z axis, its upward speed and the debug "Jumping"
//   menu's vapour delay/ramp times drive two smoothstep ramps that are multiplied
//   together; the product is written to the vapour effect handle-run held in the
//   ActiveRaceCarData (mJumpEffectHandle). Reconstructed store-for-store from the
//   ARTIST X360 asm; SmoothStep call convention + Vector3/Vector2 brace-init mirror
//   the committed BoostStateMachine sibling.
//
//   Gates (all must hold or the blend stays 0):
//     speed = |mLinearVelocity| > 20.0                       (fast enough)
//     mLinearVelocity.y >= speed                             (essentially moving up)
//     dot(normalize(mLinearVelocity), mTransform.zAxis) > 0  (facing into the jump)
//   Then blend = SmoothStep(dir) * SmoothStep(time); the debug "Force State Blend"
//   override replaces the computed blend when enabled.
// =============================================================================

#include "GameSource/Effects/Jump/JumpStateMachine.h"
#include "GameSource/Effects/EffectsModule.h"                 // BrnEffects::CarState, EffectsModule::ParticleModule()
#include "GameSource/Effects/ActiveRaceCarData.h"             // mJumpEffectHandle / maJumpLandingWheelEffectHandles
#include "GameSource/Effects/ParticleEffectHelper.h"          // RaceCarParticleEffectHelper
#include "GameSource/Effects/Particles/ParticleModule.h"      // BrnParticle::ParticleModule, LionEffect
#include "GameSource/Effects/Particles/BrnParticleDescription.h"  // ParticleDescription::HashString
#include "GameSource/Effects/BrnEffectsDebugComponent.h"      // EffectsDebugComponent / EffectsDebugJumping
#include "GameSource/Effects/Curves.h"                        // BrnEffects::Curves::SmoothStep
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // BrnPhysics::Vehicle::RaceCarState / WheelLite
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // the [jump] diag witness
#include <cmath>
#include <cstdio>    // snprintf
#include <cstdlib>   // getenv

namespace BrnEffects
{

// =============================================================================
// File-scope rodata recovered from the X360 build.
// =============================================================================

namespace
{
    // ---- SetVapourBlend literals (inlined at the call site; no named globals) ----
    const f32 KF_VAPOUR_MIN_SPEED = 20.0f;          // flt_820054CC (speed gate)
    const f32 KF_VAPOUR_DIR_LOW   = 0.70700002f;    // flt_82011C14 (dir smoothstep lower threshold)
    const f32 KF_VAPOUR_DIR_MID   = 0.85350001f;    // flt_82011C10 (dir smoothstep mid threshold)

    // ---- OnDetermineNextState / OnChangeState literals ----
    // The squared landing speed above which the per-wheel dust effects are started
    // (flt_82013AF8 == 196.0, i.e. |mLinearVelocity| > 14 m/s). The X360 compares the
    // vmsum3fp128 self-dot against it directly, so the compare stays in the squared domain.
    const f32 KF_LANDING_MIN_SPEED_SQ = 196.0f;

    // The accumulated fade time (seconds spent falling) above which the landing debris
    // burst fires (flt_82001C98 == 1.0).
    const f32 KF_DEBRIS_FADE_TIME = 1.0f;

    // The LandedWaiting dwell before the machine finishes the jump (flt_8200DD24 == 3.0).
    const f32 KF_LANDED_WAITING_TIME = 3.0f;

    // The three LION effect paths, read out of the ARTIST image at the pointer table
    // off_82CDAE58 / off_82CDAE5C / off_82CDAE60.
    //
    //   VAPOUR         -- off_82CDAE58, started on entry to Jumping.
    //   LANDING_DUST   -- off_82CDAE5C, the junkyard-VFX landing burst.
    //   LANDING_DUST_L -- off_82CDAE60, the ordinary in-world landing burst.
    // The selector between the last two is ParticleModule::mbIsInJunkyard.
    const char* const KPC_JUMP_VAPOUR_EFFECT =
        "gamedb://burnout5/Burnout/Effects/Cam_VaportrailF.lef.BurnoutFXLionEffectFile?ID=376725";
    const char* const KPC_LANDING_DUST_EFFECT =
        "gamedb://burnout5/Burnout/Effects/Cam_DustImpact.lef.BurnoutFXLionEffectFile?ID=382185";
    const char* const KPC_LANDING_DUST_LITE_EFFECT =
        "gamedb://burnout5/Burnout/Effects/Cam_DustImpactL.lef.BurnoutFXLionEffectFile?ID=614063";

    // -------------------------------------------------------------------------
    // [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_JUMP_DIAG), first-N witness of the jump
    // machine's transitions and of the handles ParticleModule::StartLionEffect hands
    // back. It exists because "the machine is pinned in AllOff" and "the machine runs but
    // every effect handle came back invalid" look identical on screen (nothing renders in
    // either case), and only a log can tell them apart.
    // DELETE-WHEN jump vapour / landing dust are confirmed on screen.
    // -------------------------------------------------------------------------
    bool JumpDiagEnabled()
    {
        static const bool sbJumpDiag = (getenv("BRN_JUMP_DIAG") != 0);
        return sbJumpDiag;
    }

    // The shared, budgeted lane: state transitions and the handles StartLionEffect
    // returned. Capped so a car that lands repeatedly cannot flood the log.
    bool JumpDiagTakeLine()
    {
        static s32 siLinesLeft = 64;
        if (!JumpDiagEnabled() || siLinesLeft <= 0)
        {
            return false;
        }
        --siLinesLeft;
        return true;
    }

    // The parked arms fire every frame while the machine sits in FiringSparks, so they
    // get their own ONE-SHOT latches rather than eating the transition budget above.
    bool JumpDiagTakeOnce(bool& lrbAlreadySaid)
    {
        if (!JumpDiagEnabled() || lrbAlreadySaid)
        {
            return false;
        }
        lrbAlreadySaid = true;
        return true;
    }

    void JumpDiagLine(const char* lpcFormat, s32 liA, u32 luB)
    {
        char lacMsg[224];
        std::snprintf(lacMsg, sizeof(lacMsg), lpcFormat, liA, luB);
        CgsDev::Log::WriteToLog(lacMsg);
    }

    void JumpDiagText(const char* lpcText)
    {
        CgsDev::Log::WriteToLog(lpcText);
    }
}

// @ 0x82288A58
void JumpStateMachine::SetVapourBlend(f32 lfFadeTime,
                                      RaceCarParticleEffectHelper& lHelper) const
{
    const BrnPhysics::Vehicle::RaceCarState* lpCar   = lHelper.RaceCarState();
    const EffectsDebugComponent*             lpDebug = lHelper.DebugComponent();

    f32 lfBlend = 0.0f;

    // speed = |mLinearVelocity| (X360: vmsum3fp128 + rsqrt Newton refine -> magnitude,
    // vsel-guarded so |v|^2 == 0 yields 0).
    const Vector3& lvVel = lpCar->mLinearVelocity;
    const f32 lfSpeed = sqrtf(lvVel.x * lvVel.x + lvVel.y * lvVel.y + lvVel.z * lvVel.z);

    if (lfSpeed > KF_VAPOUR_MIN_SPEED)
    {
        // Only fire while the velocity is (essentially) straight up: vel.y >= |vel|.
        if (lvVel.y >= lfSpeed)
        {
            // Direction alignment: dot(unit velocity, car world Z axis).
            const f32 lfInvSpeed = 1.0f / lfSpeed;
            const Vector3& lvForward = lpCar->mTransform.zAxis;
            const f32 lfDot = (lvVel.x * lfInvSpeed) * lvForward.x
                            + (lvVel.y * lfInvSpeed) * lvForward.y
                            + (lvVel.z * lfInvSpeed) * lvForward.z;

            if (lfDot > 0.0f)
            {
                const f32 lfVapourDelay = lpDebug->JumpParams().VapourStartDelay();   // +0x70
                const f32 lfVapourEnd   = lpDebug->JumpParams().VapourRampEndTime()   // +0x74
                                        + lfVapourDelay;

                // Ramp 1: alignment dot -> [0,1] over [0.707, 1.0] (mid 0.8535).
                const Vector3 lvDirParams = { KF_VAPOUR_DIR_LOW, KF_VAPOUR_DIR_MID, 1.0f, 0.0f };
                const Vector2 lvDirScale  = { 0.0f, 1.0f, 0.0f, 0.0f };

                // Ramp 2: fade time -> [0,0.5] over [delay, delay+ramp] (mid midpoint).
                const Vector3 lvTimeParams = { lfVapourDelay,
                                               ((lfVapourEnd - lfVapourDelay) * 0.5f) + lfVapourDelay,
                                               lfVapourEnd, 0.0f };
                const Vector2 lvTimeScale  = { 0.0f, 0.5f, 0.0f, 0.0f };

                BrnEffects::Curves::SmoothStep lCurve;
                const f32 lfDirBlend  = lCurve.Evaluate(lvDirParams, lvDirScale, lfDot);
                const f32 lfTimeBlend = lCurve.Evaluate(lvTimeParams, lvTimeScale, lfFadeTime);
                lfBlend = lfDirBlend * lfTimeBlend;
            }
        }
    }

    // Debug "Force State Blend" override.
    if (lpDebug->IsForceStateBlend())
    {
        lfBlend = lpDebug->ForceStateBlendValue();
    }

    // Apply to the single jump-vapour effect handle-run in the active-race-car data
    // (the console reaches it as mpActiveRaceCar + 0x114). One u32 handle, count == 1.
    const u32* lpuVapourHandle = &lHelper.ActiveRaceCar()->mJumpEffectHandle;
    lHelper.SetEffectStateBlend(lpuVapourHandle, 1, lfBlend);
}


// =============================================================================
// FireWheelSparks @ 0x82299670  -- PARKED, and announced rather than faked.
//
// The X360 body gates on the two REAR wheels being on the ground
// (maWheels[2].mRoadContact.mbIsOnGround && maWheels[3].mRoadContact.mbIsOnGround),
// draws two randomised parameters out of the effects module's random pool
// (EffectsModule + 0x2C400, the 0x4C957F2D LCG), lerps between the two rear wheels'
// contact points/normals by those parameters, and calls
// BrnEffects::EffectsModule::FireJumpSparks TWICE -- once per lerped contact.
//
// BLOCKER: EffectsModule::FireJumpSparks has NO declaration and NO definition anywhere
// in the tree (`tools/re/hasbody.py EffectsModule::FireJumpSparks` -> NO DEFINITION IN
// THE TREE, and EffectsModule.h does not declare it). Transcribing this body would mean
// either inventing that callee or adding a declaration to EffectsModule.h, which this
// wave does not own. The ladder therefore keeps its calls to this function -- the
// control flow is faithful -- and only the spark emission is missing.
// UNPARK WHEN EffectsModule::FireJumpSparks has a body.
// =============================================================================
void JumpStateMachine::FireWheelSparks(CarState& /*lCarState*/,
                                       RaceCarParticleEffectHelper& /*lHelper*/) const
{
    static bool sbSaid = false;
    if (JumpDiagTakeOnce(sbSaid))
    {
        JumpDiagText("[jump] FireWheelSparks @0x82299670 PARKED "
                     "(EffectsModule::FireJumpSparks has no body)\n");
    }
}

// =============================================================================
// FireWheelDebris @ 0x822939D8 -- PARKED, and announced rather than faked.
//
// The X360 body normalises the car's linear velocity, runs it through a smoothstep
// over the rodata pair flt_82CDAE64 / flt_82CDAE68 to get a burst count between
// flt_82CDAE6C and flt_82CDAE70, and then, once per burst, draws a rejection-sampled
// unit direction out of the effects module's random pool, offsets it through
// BrnEffects::Utils::Vector3Randomiser::RandomiseXYZ, and calls
// BrnParticle::ParticleModule::SpawnDebris with debris type 2.
//
// BLOCKER: ParticleModule::SpawnDebris has NO declaration and NO definition anywhere
// in the tree (`tools/re/hasbody.py ParticleModule::SpawnDebris` -> NO DEFINITION IN
// THE TREE; ParticleModule.h declares only the Native::BrnDebrisArray::SpawnDebris it
// eventually dispatches to). Six of the vectors the burst shape reads
// (unk_82FAB7C0 / unk_82FAB8C0 / unk_82FAC110 / unk_82FAC120 / unk_82FAC130 /
// unk_82FAC370) are also unnamed rodata with no attested owner. The ladder keeps its
// call -- the control flow is faithful -- and only the debris burst is missing.
// UNPARK WHEN ParticleModule::SpawnDebris has a body.
// =============================================================================
void JumpStateMachine::FireWheelDebris(CarState& /*lCarState*/,
                                       RaceCarParticleEffectHelper& /*lHelper*/) const
{
    static bool sbSaid = false;
    if (JumpDiagTakeOnce(sbSaid))
    {
        JumpDiagText("[jump] FireWheelDebris @0x822939D8 PARKED "
                     "(ParticleModule::SpawnDebris has no body)\n");
    }
}


// =============================================================================
// OnDetermineNextState @ 0x8229B9F8
//   The jump-effects transition table. Returns the next EffectsState; returning the
//   current state is the console's own "no transition" value (the base Tick's
//   `leNextState != mState` test then does nothing).
// =============================================================================
EffectsState JumpStateMachine::OnDetermineNextState(CarState& lCarState,
                                                    bool lbStateTimerExpired,
                                                    EffectsState leCurrentState,
                                                    RaceCarParticleEffectHelper& lHelper)
{
    // 0x8229BA18 / 0x8229BA24 -- the two unconditional kills, before the switch.
    // A crashing car (CarState +0x4D) or a hidden car (RaceCarState +0x452) drops the
    // whole machine straight to AllOff, whose entry action stops every jump effect.
    if (lCarState.mbCrashing || lHelper.RaceCarState()->mbIsHidden)
    {
        return EffectsStateAllOff;
    }

    EffectsState leNextState = leCurrentState;

    switch (leCurrentState)
    {
        case EffectsStateAllOff:                 // 0
            // Arm on the physics jump flag; clear the per-wheel landing latches first.
            if (lCarState.mbJumping)             // CarState +0x4E
            {
                for (u32 luWheel = 0; luWheel < ActiveRaceCarData::KU_NUM_WHEELS; ++luWheel)
                {
                    mbWheelsOnGround[luWheel] = false;   // this +0x10..+0x13
                }
                leNextState = EffectsStateJumping;
            }
            break;

        case EffectsStateJumping:                // 7
            // A pure entry state: the vapour effect is started by OnChangeState, and the
            // machine falls through to the "moving down" state on the very next tick.
            leNextState = EffectsStateJumpingMovingDown;
            break;

        case EffectsStateJumpingMovingDown:      // 9
            // Accumulate airborne time and drive the vapour trail's state blend from it.
            mFadeTime += lCarState.GetDt();      // this +0x0C += CarState +0x10 (mDt)
            SetVapourBlend(mFadeTime, lHelper);
            if (!lCarState.mbJumping)
            {
                leNextState = EffectsStateLanded;
            }
            break;

        case EffectsStateLanded:                 // 10
        {
            ActiveRaceCarData* const lpActiveRaceCar = lHelper.ActiveRaceCar();

            // The vapour trail ends the moment the car is back on the deck.
            lHelper.StopEffect(lpActiveRaceCar->mJumpEffectHandle);

            const BrnPhysics::Vehicle::RaceCarState* const lpCar = lCarState.mpCarState;

            // Landing speed gate, kept in the squared domain exactly as the console
            // (vmsum3fp128 self-dot of mLinearVelocity vs 196.0).
            const Vector3& lvVel = lpCar->mLinearVelocity;
            const f32 lfSpeedSq = lvVel.x * lvVel.x + lvVel.y * lvVel.y + lvVel.z * lvVel.z;
            const bool lbFastLanding = (lfSpeedSq > KF_LANDING_MIN_SPEED_SQ);

            // ParticleModule +0x23136. In the junkyard the landing burst uses the full
            // dust effect and the car's own orientation; in the world it uses the lite
            // effect and a basis built from the wheel's contact.
            const bool lbInJunkyard = lHelper.ParticleModule().mbIsInJunkyard;

            bool lbStartedAnyWheel = false;

            if (lbFastLanding || lbInJunkyard)
            {
                for (u32 luWheel = 0; luWheel < ActiveRaceCarData::KU_NUM_WHEELS; ++luWheel)
                {
                    const BrnPhysics::Vehicle::WheelLite& lWheel = lpCar->maWheels[luWheel];
                    if (!lWheel.mRoadContact.mbIsOnGround)   // wheel +0x28
                    {
                        continue;
                    }

                    const char* const lpcEffectName =
                        lbInJunkyard ? KPC_LANDING_DUST_EFFECT : KPC_LANDING_DUST_LITE_EFFECT;

                    u32& lruHandle = lpActiveRaceCar->maJumpLandingWheelEffectHandles[luWheel];
                    const u32 luWorldIndex = lHelper.WorldIndex();

                    lHelper.StopEffect(lruHandle);
                    lruHandle = lHelper.ParticleModule().StartLionEffect(
                                    BrnParticle::ParticleDescription::HashString(lpcEffectName),
                                    lpcEffectName,
                                    luWorldIndex);

                    if (JumpDiagTakeLine())
                    {
                        JumpDiagLine("[jump] landing dust wheel %d -> handle 0x%08X\n",
                                     static_cast<s32>(luWheel), lruHandle);
                    }

                    // The console resolves the freshly-stored handle through the EFFECTS
                    // MODULE's own particle module (helper +0x08, member +0xA80), not
                    // through the helper's module reference -- transcribed as written.
                    BrnParticle::LionEffect* const lpEffect =
                        lHelper.GetEffectsModule()->ParticleModule().GetLionEffect(lruHandle);

                    if (lpEffect != 0)
                    {
                        if (lbInJunkyard)
                        {
                            // Junkyard arm: the effect wears the car's orientation, planted
                            // at the wheel's contact point.
                            const Matrix44Affine& lCarTransform = lpCar->mTransform;
                            lpEffect->mTransform.xAxis = lCarTransform.xAxis;
                            lpEffect->mTransform.yAxis = lCarTransform.yAxis;
                            lpEffect->mTransform.zAxis = lCarTransform.zAxis;
                            lpEffect->mTransform.wAxis = lWheel.mRoadContact.mPosition;
                        }
                        else
                        {
                            // World arm: build a contact basis out of the road normal and
                            // the wheel velocity projected onto the contact plane, and hand
                            // the effect that projected velocity as an override.
                            const Vector3& lvNormal = lWheel.mRoadContact.mNormal;   // wheel +0x10
                            const Vector3& lvWheelVel = lWheel.mVelocity;            // wheel +0x30

                            // dot3 broadcast: the X360 subtracts in all four lanes, so the
                            // unused w lane rides along exactly as it does on the console.
                            const f32 lfAlongNormal = lvNormal.x * lvWheelVel.x
                                                    + lvNormal.y * lvWheelVel.y
                                                    + lvNormal.z * lvWheelVel.z;
                            const Vector3 lvTangent =
                            {
                                lvWheelVel.x - lvNormal.x * lfAlongNormal,
                                lvWheelVel.y - lvNormal.y * lfAlongNormal,
                                lvWheelVel.z - lvNormal.z * lfAlongNormal,
                                lvWheelVel.w - lvNormal.w * lfAlongNormal
                            };

                            // The particle's own velocity override (+0x50..+0x58), then
                            // OVERRIDE_VELOCITY | CHANGED (the console's `|= 0x24`).
                            lpEffect->mfVelocityX = lvTangent.x;
                            lpEffect->mfVelocityY = lvTangent.y;
                            lpEffect->mfVelocityZ = lvTangent.z;
                            lpEffect->muFlags |= (BrnParticle::LionEffect::EPPE_FLAG_OVERRIDE_VELOCITY
                                                | BrnParticle::LionEffect::EPPE_FLAG_CHANGED);

                            // Forward = normalised tangential velocity. The X360 uses a bare
                            // vrsqrtefp + two Newton refinements with NO zero guard here (unlike
                            // SetVapourBlend's vsel), so a stationary wheel yields the same
                            // infinities it does on the console.
                            const f32 lfTangentLenSq = lvTangent.x * lvTangent.x
                                                     + lvTangent.y * lvTangent.y
                                                     + lvTangent.z * lvTangent.z;
                            const f32 lfInvLen = 1.0f / sqrtf(lfTangentLenSq);
                            const Vector3 lvForward = { lvTangent.x * lfInvLen,
                                                        lvTangent.y * lfInvLen,
                                                        lvTangent.z * lfInvLen,
                                                        lvTangent.w * lfInvLen };

                            // Right = cross(normal, forward). The console builds it with the
                            // yzx-swizzle identity, whose w lane cancels to exactly 0.
                            const Vector3 lvRight =
                            {
                                lvNormal.y * lvForward.z - lvNormal.z * lvForward.y,
                                lvNormal.z * lvForward.x - lvNormal.x * lvForward.z,
                                lvNormal.x * lvForward.y - lvNormal.y * lvForward.x,
                                0.0f
                            };

                            lpEffect->mTransform.xAxis = lvRight;
                            lpEffect->mTransform.yAxis = lvNormal;
                            lpEffect->mTransform.zAxis = lvForward;
                            lpEffect->mTransform.wAxis = lWheel.mRoadContact.mPosition;
                        }

                        lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
                    }

                    mbWheelsOnGround[luWheel] = true;
                    lbStartedAnyWheel = true;
                }

                if (lbStartedAnyWheel)
                {
                    FireWheelSparks(lCarState, lHelper);
                }
            }

            // A long fall earns a debris burst, once, then the fall clock is reset.
            if (mFadeTime > KF_DEBRIS_FADE_TIME)
            {
                FireWheelDebris(lCarState, lHelper);
                mFadeTime = 0.0f;
            }

            // Move on once every wheel has touched down -- or immediately when the landing
            // was slow, because the speed gate above never armed and no wheel latch will
            // ever be set (this is the junkyard-only path out of Landed).
            if ((mbWheelsOnGround[0] && mbWheelsOnGround[1]
                 && mbWheelsOnGround[2] && mbWheelsOnGround[3])
                || !lbFastLanding)
            {
                leNextState = EffectsStateFiringSparks;
            }
            break;
        }

        case EffectsStateFiringSparks:           // 11
            // Keep spraying sparks for as long as the debug "Landing Sparks Time" timer
            // (seeded by OnChangeState) runs.
            FireWheelSparks(lCarState, lHelper);
            if (lbStateTimerExpired)
            {
                leNextState = EffectsStateLandedWaiting;
            }
            break;

        case EffectsStateLandedWaiting:          // 12
            // A new jump inside the dwell re-arms the machine immediately (the console
            // compares the CarState +0x4E byte against 1); otherwise the 3 s dwell ends
            // the jump.
            if (lCarState.mbJumping)
            {
                mTime = 0.0f;                    // base +0x08 (SetStateTimer, inlined)
                leNextState = EffectsStateJumping;
            }
            else if (lbStateTimerExpired)
            {
                leNextState = EffectsStateJumpingFinishing;
            }
            break;

        case EffectsStateJumpingFinishing:       // 13
            leNextState = EffectsStateAllOff;
            break;

        default:
            CGS_ASSERT(false, "Unknown state");   // .cpp:242
            break;
    }

    return leNextState;
}

// =============================================================================
// OnChangeState @ 0x82299510
//   Entry action for each jump state: start/stop the vapour and landing-dust LION
//   effects and seed the state timer.
// =============================================================================
void JumpStateMachine::OnChangeState(EffectsState leNewState,
                                     RaceCarParticleEffectHelper& lHelper,
                                     CarState& /*lCarState*/)
{
    if (JumpDiagTakeLine())
    {
        JumpDiagLine("[jump] state %d -> %u\n",
                     static_cast<s32>(mState), static_cast<u32>(leNewState));
    }

    ActiveRaceCarData* const lpActiveRaceCar = lHelper.ActiveRaceCar();

    switch (leNewState)
    {
        case EffectsStateAllOff:                 // 0
            // Everything off: the vapour trail and all four landing-dust runs.
            lHelper.StopEffect(lpActiveRaceCar->mJumpEffectHandle);
            for (u32 luWheel = 0; luWheel < ActiveRaceCarData::KU_NUM_WHEELS; ++luWheel)
            {
                lHelper.StopEffect(lpActiveRaceCar->maJumpLandingWheelEffectHandles[luWheel]);
            }
            break;

        case EffectsStateJumping:                // 7
        {
            // Restart the vapour trail from scratch and re-zero the fall clock, then push
            // the (still zero) blend straight into the fresh handle.
            const u32 luWorldIndex = lHelper.WorldIndex();

            lHelper.StopEffect(lpActiveRaceCar->mJumpEffectHandle);
            lpActiveRaceCar->mJumpEffectHandle = lHelper.ParticleModule().StartLionEffect(
                BrnParticle::ParticleDescription::HashString(KPC_JUMP_VAPOUR_EFFECT),
                KPC_JUMP_VAPOUR_EFFECT,
                luWorldIndex);

            if (JumpDiagTakeLine())
            {
                JumpDiagLine("[jump] vapour start %d -> handle 0x%08X\n",
                             0, lpActiveRaceCar->mJumpEffectHandle);
            }

            mFadeTime = 0.0f;                    // this +0x0C
            SetVapourBlend(mFadeTime, lHelper);  // the console passes the same f1 it just stored
            break;
        }

        case EffectsStateJumpingMovingDown:      // 9
        case EffectsStateLanded:                 // 10
        case EffectsStateJumpingFinishing:       // 13
            // No entry action.
            break;

        case EffectsStateFiringSparks:           // 11
            // Spark dwell comes from the debug "Jumping" menu (EffectsDebugComponent +0x6C).
            mTime = lHelper.DebugComponent()->JumpParams().LandingSparksTime();
            break;

        case EffectsStateLandedWaiting:          // 12
            mTime = KF_LANDED_WAITING_TIME;      // base +0x08 (flt_8200DD24 == 3.0)
            break;

        default:
            CGS_ASSERT(false, "Unknown state");   // .cpp:302
            break;
    }
}

void JumpStateMachine::OnTick(CarState& /*lCarState*/,
                              RaceCarParticleEffectHelper& /*lHelper*/)
{
    // No X360 export: the base's empty slot, ICF-folded. Empty is faithful.
}

} // namespace BrnEffects
