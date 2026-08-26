// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring_StuntTypes.cpp
// ============================================================================
// Out-of-line method BODIES for the StuntModeScoring per-category "Update*Stunts"
// sub-passes that StuntModeScoring::UpdateStunts fans out to each frame:
//   UpdateAirStunts     (X360 0x8232C640)  -- spin / barrel-roll / air / reverse-takeoff
//   UpdateDriftStunts   (X360 0x8232CAE0)  -- drift
//   UpdateBoostStunts   (X360 0x8232CC18)  -- burnout (boost chain) / boost time
//   UpdateDrivingStunts (X360 0x8232CD70)  -- reverse-driving / handbrake-turn
//
// SHAPE = DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../Scoring/BrnStuntModeScoring.cpp:395/476/488/498);
// BODY = X360 pseudocode (work show <addr> --full), reconstructed store-for-store and
// OVERRIDING the DWARF on any conflict. Members + sub-scorer helpers are reached BY NAME
// against the committed StuntModeScoring layout (BrnStuntModeScoring.h) and the committed
// RCEntityActiveRaceCarOutputInterface (BrnRaceCarEntityModuleOutputInterface.h). NO raw
// offset casts: the X360 `this`-relative byte offsets are X360-physical and do NOT line up
// with the logical (minimal-slice) member offsets, so every member is named via its
// semantic role (anchored by the X360 assert strings + the DWARF source-level locals).
//
// SELF-GATE: cl /c (compile only, no link). The four bodies declare-call several sibling
// StuntModeScoring helpers (RegisterStunt / UpdateScore / UpdateStuntRating) that are
// declare-only in the keystone header -- their out-of-line bodies land with this type's
// full TU (BrnStuntModeScoring.cpp). That is fine for the compile gate (no link).
//
// ----------------------------------------------------------------------------
// X360 sub_ -> named-accessor mapping (all on the active-race-car output interface, the
// `lpActiveRaceCarInterface` param; resolved from the DWARF accessor list per method):
//   sub_82310240(itf) -> itf->GetPlayerRaceCarState()      (RaceCarState*, DWARF :406/:467/:482/:493/:501)
//   sub_82310440(out,itf) -> itf->GetPlayerLinearVelocity() (DWARF :443; SIMD return-by-out)
//   sub_82310398(out,itf) -> itf->GetPlayerDirection()      (DWARF :469)
//   sub_823102F0(out,itf) -> itf->GetPlayerPosition()       (DWARF :452)
//   sub_823104E8(out,itf,idx) -> itf->GetCurrentInAirRotations(idx)  (DWARF :408)
//   RCEntit(itf, idx) -> itf->GetBoostOutputInfoN(idx)       (DWARF :490/:491/:492/:494)
//
// K-constants (the X360 flt_82CDB7xx rodata, the BrnStuntModeScoring.cpp anonymous-namespace
// tuning block, BrnStuntModeScoring.cpp:40-71). They have NO committed home yet (the full TU
// owns the canonical anonymous-namespace block), so they are defined file-local below.
// The names are recovered VERBATIM from the PS3 DecFIGS DWARF (BrnGameState:: globals;
// 0x1E15F0/0x1E1F60/0x1E2140/0x1E24E4) -- e.g. KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_AIR,
// _DRIFTING, _OF_BOOST, _PER_BURNOUT_CHAINED, _OF_REVERSING, _PER_HANDBRAKE_TURN.
//
// The MAGNITUDES are now RECOVERED. Each is a direct big-endian f32 read of its flt_ address
// out of the decrypted X360 ARTIST basefile (image VA - 0x82000000 == file offset), and each
// constant's flt_ address was independently cross-checked against the referencing method's own
// asm export (0x8232C640 / 0x8232CAE0 / 0x8232CC18 / 0x8232CD70 each reference the
// flt_ set attributed to it below). The per-constant comments carry the VA each value came
// from. When the full BrnStuntModeScoring.cpp is reconstructed it owns the canonical block --
// delete these and let the shared anonymous-namespace constants resolve.
//
// STILL UNRECOVERED (and still FLAGged below): the two KVF_REVERSE_TAKEOFF_* gates. They are
// VecFloat vectors built by dynamic initialisation (the asm loads them as unk_82FADAF0 /
// unk_82FADC30, both all-zero in the static image), so no static rodata read can recover them.
// ----------------------------------------------------------------------------

#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"

// The four bodies dereference the WorldStuntAction / PowerParkResultAction-adjacent
// GameStateModuleIO action family + the EStuntType / EActiveRaceCarIndex tags through the
// shared game-action home. (UpdateAirStunts/Drift/Boost/Driving themselves do not consume a
// GameAction, but the TU header is included for parity with the sibling Update* bodies and to
// keep the action types complete when this partial is folded into the full TU.)
#include "GameSource/GameState/BrnGameActions.h"

// Completes ActiveRaceCarOutputInterface (the keystone typedef ->
// BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface): the four bodies
// read the player car BY NAME through its accessors (GetPlayerRaceCarState / IsPlayerInAir /
// GetPlayerActiveRaceCarIndex / GetCurrentInAirRotations / GetPlayerLinearVelocity /
// GetPlayerPosition / GetPlayerDirection / GetBoostOutputInfoN / IsPlayerInReverseGear) and
// dereference the returned RaceCarState (BrnVehicleEvents.h, pulled in transitively) +
// BoostOutputInfo BY NAME.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

// rw::math::vpu Vector3 vocabulary (operator- / Magnitude / MagnitudeSquared / Dot / Max /
// Abs / Normalize / GetX/Y/Z / IsZero), found by ADL on Vector3. UpdateAirStunts turns the
// player's linear velocity / position / direction / in-air rotations into the scalar gates
// the spin/roll/air ratings need.
#include "rw/math/vpu/vector3_operation.h"

#include <cmath>   // std::fabs (abs-spin / abs-roll magnitudes)

namespace BrnGameState
{
    namespace
    {
        // --- StuntModeScoring tuning constants (BrnStuntModeScoring.cpp:40-71 anon ns) -------
        // Names are DWARF-attested; magnitudes are RECOVERED from the X360 rodata (see the file
        // header for the read method). Each trailing comment names the VA the value was read at.

        // Air / spin / barrel-roll rating thresholds (UpdateAirStunts).
        // NB: names recovered VERBATIM from the PS3 DecFIGS DWARF (BrnGameState:: globals;
        // 0x1E15F0 UpdateAirStunts) -- they replace the round-1 role-guessed spellings.
        const f32 KF_STUNT_ATTACK_MIN_AIR_TIME            = 0.4f;   // flt_82CDB76C : air gate + AIR-rating input
        const f32 KF_STUNT_ATTACK_GOOD_SECONDS_OF_AIR     = 0.7f;   // flt_82CDB74C : AIR "good" target
        const f32 KF_STUNT_ATTACK_AWESOME_SECONDS_OF_AIR  = 1.0f;   // flt_82CDB758 : AIR "awesome" target
        const f32 KF_STUNT_ATTACK_MIN_SPIN_DEGREES        = 45.0f;  // flt_82CDB764 : spin gate
        const f32 KF_STUNT_ATTACK_GOOD_DEGREES_SPUN       = 80.0f;  // flt_82CDB750 : spin "good" target
        const f32 KF_STUNT_ATTACK_AWESOME_DEGREES_SPUN    = 160.0f; // flt_82CDB75C : spin "awesome" target
        const f32 KF_STUNT_ATTACK_MIN_ROLL_DEGREES        = 180.0f; // flt_82CDB768 : roll gate
        const f32 KF_STUNT_ATTACK_GOOD_DEGREES_ROLLED     = 160.0f; // flt_82CDB754 : roll "good" target
        const f32 KF_STUNT_ATTACK_AWESOME_DEGREES_ROLLED  = 320.0f; // flt_82CDB760 : roll "awesome" target
        // NB (not a transcription slip): the roll GATE (180) is above the roll "good" target
        // (160), so any roll that clears the gate is already at least "good" -- barrel rolls are
        // rated GOOD at one half-revolution past the gate and AWESOME at 320 degrees. Both values
        // re-read from the image; the spin triple (45/80/160) is ordered the way you would expect.
        const f32 KF_STUNT_ATTACK_JUMP_REPETITION_RADIUS  = 4.0f;   // flt_82CDB77C : jump-repeat radius (metres)
        const f32 KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_AIR = 1000.0f;// flt_82CDB730 : air score rate (PS3 DWARF name)
        const f32 KF_STUNT_ATTACK_REVERSE_TAKEOFF_SCORE_MULTIPLIER = 2.0f; // flt_82CDB780 : reverse-takeoff bonus mult
        // (X360 0x8232C640 uses this inline flt_82CDB780 multiplier; the PS3 branch refactored
        //  it into a GetTakeOffMultiplier() method -- we keep the X360-faithful inline form.)

        // Drift (UpdateDriftStunts).
        const f32 KF_STUNT_ATTACK_MIN_DRIFT_TIME              = 1.0f;   // flt_82CDB770 : drift gate
        const f32 KF_STUNT_ATTACK_SCORE_PER_SECOND_DRIFTING  = 300.0f;  // flt_82CDB734 : drift score rate (PS3 DWARF name)

        // Boost / burnout (UpdateBoostStunts).
        const f32 KF_STUNT_ATTACK_MIN_BOOST_TIME             = 0.01f;   // flt_82CDB774 : boost-time gate
        const f32 KF_STUNT_ATTACK_SCORE_PER_BURNOUT_CHAINED  = 1000.0f; // flt_82CDB724 : burnout (chain) score rate (PS3 DWARF name)
        const f32 KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_BOOST  = 120.0f;  // flt_82CDB738 : boost score rate (PS3 DWARF name)

        // Driving / reverse / handbrake (UpdateDrivingStunts).
        // The reverse-gear cap is NEGATIVE (-25): the gate is `mfMaxSpeedMPH < -25.0f`, i.e. the
        // car must be travelling backwards faster than 25 mph, which is why the signed speed is
        // compared with `<` rather than a magnitude with `>`.
        const f32 KF_MIN_SPEED_FOR_REVERSING_STUNT             = -25.0f; // flt_82CDB7A4 : reverse-gear speed cap
        const f32 KF_HANDBRAKE_TURN_MIN_ANGLE                  = 160.0f; // flt_82CDB7A8 : handbrake-turn angle gate
        const f32 KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_REVERSING = 200.0f;// flt_82CDB740 : reverse-driving score rate (PS3 DWARF name)
        const f32 KF_STUNT_ATTACK_SCORE_PER_HANDBRAKE_TURN     = 200.0f; // flt_82CDB72C : handbrake-turn flat award (PS3 DWARF name)

        // Reverse-takeoff direction test (UpdateAirStunts; the X360 VMX block normalises the
        // player velocity, then dots it with the car forward direction and compares vs a cos
        // threshold). The DWARF spells these as the VecFloat KVF_REVERSE_TAKEOFF_* constants.
        // FLAG -- magnitudes UNRECOVERABLE by a static read, and these two ALONE still hold
        // placeholders in this file. The X360 loads them as whole vectors from unk_82FADAF0
        // (the |v| gate, 0x8232C738) and unk_82FADC30 (the cos gate, 0x8232C7C4); both are
        // all-zero in the static image, so they are written by a dynamic initialiser that has
        // no IDA export. Recover them with a dyn-init scan, not another rodata read.
        const f32 KVF_REVERSE_TAKEOFF_MIN_VELOCITY        = 0.0f;  // FLAG: unk_82FADAF0, dyn-init (min |v|)
        const f32 KVF_REVERSE_TAKEOFF_COS_MAX_ANGLE       = 0.0f;  // FLAG: unk_82FADC30, dyn-init (cos gate)

        // Landing-pass "still rotating" gate (X360 flt_82020B30 splat fed to vcmpgtfp on the
        // X lane of GetCurrentInAirRotations). NOT a KF tuning constant -- it is FLT_EPSILON
        // (1.1920929e-07), the same near-zero dead-band the sibling RwMathFPU::IsZero checks use.
        const f32 KF_STILL_ROTATING_RATE_THRESHOLD       = 1.1920929e-07f;  // flt_82020B30
    }

    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
        ActiveRaceCarOutputInterface;
    typedef BrnPhysics::Vehicle::RaceCarState RaceCarState;

    // ------------------------------------------------------------------------
    // StuntModeScoring::UpdateAirStunts  (X360 0x8232C640)
    // ------------------------------------------------------------------------
    // Rates the in-air portion of a stunt: while the player is airborne with throttle/air
    // time over the gate, awards AIR score and tracks the spin/roll the player has racked
    // up; on the takeoff edge it runs the reverse-takeoff bonus test and the jump-repetition
    // (already-jumped-here) check; on landing it commits the spin/roll ratings.
    //
    // X360 control flow (asm at 0x8232C640):
    //   lbStunting = IsPlayerInAir(player) && GetPlayerRaceCarState()->mfTimeInAir > 0.
    //   if ( lbStunting && airTime > KF_STUNT_ATTACK_MIN_AIR_TIME && RegisterStunt() )
    //   {
    //       if ( !mbWasInAirLastFrame )            // takeoff edge -- once per jump
    //       {
    //           mbWasInAirLastFrame = true;
    //           // reverse-takeoff test: normalise the player velocity, compare |v| vs the
    //           // KVF_REVERSE_TAKEOFF_MIN_VELOCITY gate, then dot(unitVel, playerDir) vs
    //           // KVF_REVERSE_TAKEOFF_COS_MAX_ANGLE -- if a reverse takeoff, set the
    //           // REVERSE_TAKEOFF bit in muStuntTypesInProgress.
    //           UpdateScore(AIR, takeoffMult*KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_AIR*KF_STUNT_ATTACK_MIN_AIR_TIME)
    //           // jump-repetition: for each Vector3 in mRecentJumpSet, if the player is within
    //           // KF_STUNT_ATTACK_JUMP_REPETITION_RADIUS of it, this jump is a repeat -> clear
    //           // the per-jump valid flag and set the repetition-error bit; else Push() it.
    //       }
    //       UpdateScore(AIR, takeoffMult*KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_AIR*airTime)   // per-frame air score
    //       UpdateStuntRating(AIR, airTime, GOOD_SECONDS_OF_AIR, AWESOME_SECONDS_OF_AIR)
    //       lbReturn = true;
    //   }
    //   else mbWasInAirLastFrame = false;
    //   // landing pass: read the accumulated in-air rotations, fold in any leftover, and if
    //   // the abs spin / abs roll cleared their gates, rate + score SPIN / BARREL_ROLL.
    //
    // The "takeoffMult" is KF_STUNT_ATTACK_REVERSE_TAKEOFF_SCORE_MULTIPLIER when the
    // REVERSE_TAKEOFF bit is set in muStuntTypesInProgress, else 1.0 (X360 ternary on bit 14).
    bool StuntModeScoring::UpdateAirStunts(f32 lfSimTimeStep,
                                           const ActiveRaceCarOutputInterface* lpActiveRaceCarInterface)
    {
        bool lbReturn = false;

        const EActiveRaceCarIndex lePlayerIndex =
            lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
        const RaceCarState* lpPlayerRC = lpActiveRaceCarInterface->GetPlayerRaceCarState();

        // lbStunting: the player is airborne and has clocked some air time this frame.
        const bool lbStunting =
            lpActiveRaceCarInterface->IsPlayerInAir() && lpPlayerRC->mfTimeInAir > 0.0f;

        const f32 lfAirTime = lpPlayerRC->mfTimeInAir;

        if (lbStunting && lfAirTime > KF_STUNT_ATTACK_MIN_AIR_TIME && RegisterStunt())
        {
            if (!mbWasInAirLastFrame)
            {
                // Takeoff edge -- runs once per jump.
                mbWasInAirLastFrame = true;

                // --- reverse-takeoff test (X360 VMX normalise + dot block) ---
                Vector3 lUnitVelocity = lpActiveRaceCarInterface->GetPlayerLinearVelocity();
                const f32 lfSpeed = rw::math::vpu::Magnitude(lUnitVelocity);
                if (lfSpeed > KVF_REVERSE_TAKEOFF_MIN_VELOCITY)
                {
                    lUnitVelocity = rw::math::vpu::Normalize(lUnitVelocity);
                    const Vector3 lPlayerDirection = lpActiveRaceCarInterface->GetPlayerDirection();
                    if (rw::math::vpu::Dot(lUnitVelocity, lPlayerDirection)
                        <= KVF_REVERSE_TAKEOFF_COS_MAX_ANGLE)
                    {
                        // Facing away from travel at takeoff -- flag a reverse takeoff.
                        muStuntTypesInProgress |= (1u << E_STUNT_TYPE_REVERSE_TAKEOFF);
                    }
                }

                // Reverse-takeoff multiplier (bit 14 of the in-progress mask).
                const f32 lfTakeOffMultiplier =
                    ((muStuntTypesInProgress >> E_STUNT_TYPE_REVERSE_TAKEOFF) & 1u) != 0u
                        ? KF_STUNT_ATTACK_REVERSE_TAKEOFF_SCORE_MULTIPLIER
                        : 1.0f;

                UpdateScore(lfTakeOffMultiplier * KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_AIR * KF_STUNT_ATTACK_MIN_AIR_TIME,
                            E_STUNT_TYPE_AIR, false);

                // --- jump-repetition check against the recent-jump ring buffer ---
                const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
                // The X360 (0x8232C640) drives the fresh-jump decision off the mbValidStunt
                // member (byte +0x2B): on a repeat it CLEARS mbValidStunt (`*(v6+43)=0`) + sets
                // the repetition bit, and the Push is gated on mbValidStunt still being set
                // (`if (*(v6+43)) Push(...)`). Both the X360 and the PS3 DecFIGS (0x1E15F0)
                // agree on the member + the gate; the round-1 local `lbFreshJump` flag was a
                // misreconstruction (it never wrote the member). Reached BY NAME.
                const s32 liJumpCount = mRecentJumpSet.GetLength();
                for (s32 liJumpSetIndex = 0; liJumpSetIndex < liJumpCount; ++liJumpSetIndex)
                {
                    const Vector3 lOffset = lPlayerPosition - mRecentJumpSet[liJumpSetIndex];
                    const f32 lfDistSq = rw::math::vpu::MagnitudeSquared(lOffset);
                    if (lfDistSq <= (KF_STUNT_ATTACK_JUMP_REPETITION_RADIUS
                                     * KF_STUNT_ATTACK_JUMP_REPETITION_RADIUS))
                    {
                        // Already jumped from here -- invalidate the stunt + flag the repetition.
                        mbValidStunt = false;                  // X360 *(v6+43) = 0
                        muStuntTypesInProgress |= 0x40000u;    // X360 *(v6+80) |= 0x40000
                    }
                }
                if (mbValidStunt)                              // X360 if (*(v6+43)) Push(...)
                {
                    mRecentJumpSet.Push(&lPlayerPosition);
                }
            }

            // Per-frame air score (scaled by the still-current air time).
            const f32 lfAirTimeNow = lpActiveRaceCarInterface->GetPlayerRaceCarState()->mfTimeInAir;
            const f32 lfTakeOffMultiplier =
                ((muStuntTypesInProgress >> E_STUNT_TYPE_REVERSE_TAKEOFF) & 1u) != 0u
                    ? KF_STUNT_ATTACK_REVERSE_TAKEOFF_SCORE_MULTIPLIER
                    : 1.0f;
            UpdateScore(lfTakeOffMultiplier * KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_AIR * lfSimTimeStep,
                        E_STUNT_TYPE_AIR, false);
            UpdateStuntRating(E_STUNT_TYPE_AIR, lfAirTimeNow,
                              KF_STUNT_ATTACK_GOOD_SECONDS_OF_AIR,
                              KF_STUNT_ATTACK_AWESOME_SECONDS_OF_AIR);
            lbReturn = true;
        }
        else
        {
            mbWasInAirLastFrame = false;
        }

        CGS_ASSERT(lePlayerIndex != -1, "Player car index hasn't been set");

        // --- landing pass: rate accumulated spin / barrel-roll ---
        // GetCurrentInAirRotations -> a Vector3 of per-axis rotation; the X360 takes the
        // per-axis max against mStuntRollInProgress (the leftover), then the abs of the spin
        // (Y) and roll (Z) lanes for the gate + rating.
        Vector3 lCurrentRotation = lpActiveRaceCarInterface->GetCurrentInAirRotations(lePlayerIndex);

        if (rw::math::vpu::GetX(lCurrentRotation) > KF_STILL_ROTATING_RATE_THRESHOLD)
        {
            // (X360 vcmpgtfp branch) -- still airborne / mid-rotation: nothing to commit yet.
            // The rotation accumulator carries over; return the air-stunt result as-is.
        }
        else
        {
            // X360 (0x8232C9C4): abs the current rotation FIRST (vandc sign-mask), THEN lane-wise
            // max against the leftover mStuntRollInProgress -- not max-then-discard-abs.
            lCurrentRotation = rw::math::vpu::Max(rw::math::vpu::Abs(lCurrentRotation), mStuntRollInProgress);
            mStuntRollInProgress = lCurrentRotation;

            const f32 lfAbsSpin = std::fabs(rw::math::vpu::GetY(lCurrentRotation));
            const f32 lfAbsRoll = std::fabs(rw::math::vpu::GetZ(lCurrentRotation));

            if (lfAbsSpin > KF_STUNT_ATTACK_MIN_SPIN_DEGREES && RegisterStunt())
            {
                UpdateStuntRating(E_STUNT_TYPE_SPIN, lfAbsSpin,
                                  KF_STUNT_ATTACK_GOOD_DEGREES_SPUN,
                                  KF_STUNT_ATTACK_AWESOME_DEGREES_SPUN);
                UpdateScore(0.0f, E_STUNT_TYPE_SPIN, false);
            }
            if (lfAbsRoll > KF_STUNT_ATTACK_MIN_ROLL_DEGREES && RegisterStunt())
            {
                UpdateStuntRating(E_STUNT_TYPE_BARREL_ROLL, lfAbsRoll,
                                  KF_STUNT_ATTACK_GOOD_DEGREES_ROLLED,
                                  KF_STUNT_ATTACK_AWESOME_DEGREES_ROLLED);
                UpdateScore(0.0f, E_STUNT_TYPE_BARREL_ROLL, false);
            }

            // X360 tail: return true if an air stunt happened OR the spin/roll bits are now set
            // in muStuntTypesInProgress (a spin/roll was scored this pass).
            const bool lbSpinOrRoll =
                (muStuntTypesInProgress & (1u << E_STUNT_TYPE_SPIN)) != 0u
                || (muStuntTypesInProgress & (1u << E_STUNT_TYPE_BARREL_ROLL)) != 0u;
            lbReturn = (lbReturn || lbSpinOrRoll);
        }

        return lbReturn;
    }

    // ------------------------------------------------------------------------
    // StuntModeScoring::UpdateDriftStunts  (X360 0x8232CAE0)
    // ------------------------------------------------------------------------
    // Rates the player drift. While a drift is ongoing the latch holds; when the drift value
    // returns to ~0 the latch clears. If the in-progress drift time cleared the gate, awards
    // DRIFT score (with a one-shot threshold award the first frame the DRIFT bit is unset).
    //
    // X360 control flow (asm at 0x8232CAE0):
    //   lpPlayerRC = GetPlayerRaceCarState();  CGS_ASSERT(lpPlayerRC, "lpPlayerRC")
    //   if ( mbInitialDriftOngoing && !IsZero(lpPlayerRC->mfInProgressDriftTime) ) return false;
    //   mbInitialDriftOngoing = false;
    //   if ( lpPlayerRC->mfInProgressDriftTime <= KF_STUNT_ATTACK_MIN_DRIFT_TIME
    //        || !RegisterStunt() ) return false;
    //   if ( (muStuntTypesInProgress & (1<<E_STUNT_TYPE_DRIFT)) == 0 )       // first drift frame
    //       UpdateScore(DRIFT, KF_STUNT_ATTACK_SCORE_PER_SECOND_DRIFTING * KF_STUNT_ATTACK_MIN_DRIFT_TIME)
    //   UpdateScore(DRIFT, KF_STUNT_ATTACK_SCORE_PER_SECOND_DRIFTING * lfSimTimeStep)
    //   return true;
    bool StuntModeScoring::UpdateDriftStunts(f32 lfSimTimeStep,
                                             const ActiveRaceCarOutputInterface* lpActiveRaceCarInterface)
    {
        const RaceCarState* lpPlayerRC = lpActiveRaceCarInterface->GetPlayerRaceCarState();
        CGS_ASSERT(lpPlayerRC != NULL, "lpPlayerRC");

        const f32 lfDriftTime = lpPlayerRC->mfInProgressDriftTime;

        if (mbInitialDriftOngoing)
        {
            // While a drift is still running (drift value not yet back to ~0), hold off.
            if (!rw::math::vpu::IsZero(Vector3{ lfDriftTime, 0.0f, 0.0f, 0.0f }))
            {
                return false;
            }
        }
        mbInitialDriftOngoing = false;

        if (lpPlayerRC->mfInProgressDriftTime <= KF_STUNT_ATTACK_MIN_DRIFT_TIME || !RegisterStunt())
        {
            return false;
        }

        if ((muStuntTypesInProgress & (1u << E_STUNT_TYPE_DRIFT)) == 0u)
        {
            // First frame this drift scores -- the threshold catch-up award.
            UpdateScore(KF_STUNT_ATTACK_SCORE_PER_SECOND_DRIFTING * KF_STUNT_ATTACK_MIN_DRIFT_TIME,
                        E_STUNT_TYPE_DRIFT, true);
        }
        UpdateScore(KF_STUNT_ATTACK_SCORE_PER_SECOND_DRIFTING * lfSimTimeStep, E_STUNT_TYPE_DRIFT, true);
        return true;
    }

    // ------------------------------------------------------------------------
    // StuntModeScoring::UpdateBoostStunts  (X360 0x8232CC18)
    // ------------------------------------------------------------------------
    // Rates the boost output: a freshly-completed boost chain awards BURNOUT score scaled by
    // the chain length; otherwise an in-air boost over the min-boost-time gate awards BOOST
    // score (with a one-shot threshold award the first frame the BOOST bit is unset).
    //
    // X360 control flow (asm at 0x8232CC18):
    //   const BoostOutputInfo* lpBoost = GetBoostOutputInfoN(GetPlayerActiveRaceCarIndex());
    //   if ( lpBoost->mbWasChainJustCompleted && RegisterStunt() )
    //   {
    //       UpdateScore(BURNOUT, lpBoost->muNumChained * KF_STUNT_ATTACK_SCORE_PER_BURNOUT_CHAINED)
    //       return true;
    //   }
    //   else if ( lpBoost->mbIsInAir
    //             && GetPlayerRaceCarState()->mfTimeBoosting > KF_STUNT_ATTACK_MIN_BOOST_TIME
    //             && RegisterStunt() )
    //   {
    //       if ( (muStuntTypesInProgress & (1<<E_STUNT_TYPE_BOOST)) == 0 )
    //           UpdateScore(BOOST, KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_BOOST * KF_STUNT_ATTACK_MIN_BOOST_TIME)
    //       UpdateScore(BOOST, KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_BOOST * lfSimTimeStep)
    //       return true;
    //   }
    //   return false;
    //
    // NOTE: the dossier's `*(... + 6)` / `*(... >> 32)` / `HIDWORD(v7)+12` are the X360
    // 64-bit-register artifacts of the SAME BoostOutputInfo* load: byte +6 == the 7th bool
    // (mbWasChainJustCompleted), +12 == muNumChained (first non-bool after the 12 bools),
    // and the in-air retest reads mbIsInAir (byte +1). Reached BY NAME here.
    bool StuntModeScoring::UpdateBoostStunts(f32 lfSimTimeStep,
                                             const ActiveRaceCarOutputInterface* lpActiveRaceCarInterface)
    {
        const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* lpBoost =
            lpActiveRaceCarInterface->GetBoostOutputInfoN(
                lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex());

        if (lpBoost->mbWasChainJustCompleted && RegisterStunt())
        {
            UpdateScore(static_cast<f32>(lpBoost->muNumChained) * KF_STUNT_ATTACK_SCORE_PER_BURNOUT_CHAINED,
                        E_STUNT_TYPE_BURNOUT, true);
            return true;
        }
        else if (lpBoost->mbIsInAir
                 && lpActiveRaceCarInterface->GetPlayerRaceCarState()->mfTimeBoosting
                        > KF_STUNT_ATTACK_MIN_BOOST_TIME
                 && RegisterStunt())
        {
            if ((muStuntTypesInProgress & (1u << E_STUNT_TYPE_BOOST)) == 0u)
            {
                UpdateScore(KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_BOOST * KF_STUNT_ATTACK_MIN_BOOST_TIME,
                            E_STUNT_TYPE_BOOST, true);
            }
            UpdateScore(KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_BOOST * lfSimTimeStep, E_STUNT_TYPE_BOOST, true);
            return true;
        }
        else
        {
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // StuntModeScoring::UpdateDrivingStunts  (X360 0x8232CD70)
    // ------------------------------------------------------------------------
    // Rates two ground-driving stunts: driving in reverse gear under the speed cap awards
    // REVERSE_DRIVING score; a handbrake turn past the min angle awards HANDBRAKE_TURN score
    // (a one-shot flat award the first frame the HANDBRAKE_TURN bit is unset).
    //
    // X360 control flow (asm at 0x8232CD70):
    //   lbReverse = GetPlayerActiveRaceCarIndex() != -1 && !<reverse-takeoff-block-flag>
    //   if ( lbReverse && GetPlayerRaceCarState()->mfMaxSpeedMPH < KF_MIN_SPEED_FOR_REVERSING_STUNT
    //        && RegisterStunt() )
    //   {
    //       UpdateScore(REVERSE_DRIVING, KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_REVERSING * lfSimTimeStep)
    //       return true;
    //   }
    //   else if ( GetPlayerRaceCarState()->mfInProgressHandbreakTurnAngle >= KF_HANDBRAKE_TURN_MIN_ANGLE
    //             && RegisterStunt() )
    //   {
    //       if ( (muStuntTypesInProgress & (1<<E_STUNT_TYPE_HANDBRAKE_TURN)) != 0 )
    //           UpdateScore(HANDBRAKE_TURN, 0.0f)                 // already counted -- no double award
    //       else
    //           UpdateScore(HANDBRAKE_TURN, KF_STUNT_ATTACK_SCORE_PER_HANDBRAKE_TURN)
    //       return true;
    //   }
    //   return false;
    //
    // X360 detail: the reverse predicate gates on the active player index being set AND a
    // per-car interface flag at the (player-indexed) RaceCarState block being clear -- reached
    // here via IsPlayerInReverseGear() (the named accessor that owns that test). The reverse
    // speed cap reads the player RaceCarState's mfMaxSpeedMPH (@972, the X360 +972 load).
    bool StuntModeScoring::UpdateDrivingStunts(f32 lfSimTimeStep,
                                               const ActiveRaceCarOutputInterface* lpActiveRaceCarInterface)
    {
        bool lbResult;

        const bool lbInReverse =
            lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() != -1
            && lpActiveRaceCarInterface->IsPlayerInReverseGear();
        const RaceCarState* lpPlayerRC = lpActiveRaceCarInterface->GetPlayerRaceCarState();

        if (lbInReverse
            && lpPlayerRC->mfMaxSpeedMPH < KF_MIN_SPEED_FOR_REVERSING_STUNT
            && RegisterStunt())
        {
            UpdateScore(KF_STUNT_ATTACK_SCORE_PER_SECOND_OF_REVERSING * lfSimTimeStep,
                        E_STUNT_TYPE_REVERSE_DRIVING, true);
            lbResult = true;
        }
        else if (lpActiveRaceCarInterface->GetPlayerRaceCarState()->mfInProgressHandbreakTurnAngle
                     >= KF_HANDBRAKE_TURN_MIN_ANGLE
                 && RegisterStunt())
        {
            if ((muStuntTypesInProgress & (1u << E_STUNT_TYPE_HANDBRAKE_TURN)) != 0u)
            {
                UpdateScore(0.0f, E_STUNT_TYPE_HANDBRAKE_TURN, true);
            }
            else
            {
                UpdateScore(KF_STUNT_ATTACK_SCORE_PER_HANDBRAKE_TURN, E_STUNT_TYPE_HANDBRAKE_TURN, true);
            }
            lbResult = true;
        }
        else
        {
            lbResult = false;
        }

        return lbResult;
    }
}
