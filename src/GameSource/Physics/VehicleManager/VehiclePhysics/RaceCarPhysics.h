#pragma once

// BrnPhysics::Vehicle::RaceCarPhysics -- the player/AI race-car physics body (a VehiclePhysics
// subclass that adds showtime, bounce-boost, target-assist and crash state).
//
// MINIMAL OWNING SLICE. The full RaceCarPhysics carries a large PlayerParameters block and ~100
// methods owned by separate future TUs. THIS group bodies only the three ledger funcs
// GetHeightAboveRoad @0x825B3998, IsCrashingNormally @0x827E42B8 and
// IsPlayerVehicleActuallyInShowtime @0x827E42B0; the rest of the API is NOT declared here. The
// single bool member the latter two read (mbPlayerCarInShowtime) is reconstructed BY NAME.
//
// LAYOUT NOTES (from the asm):
//   * mbPlayerCarInShowtime is read at this+0x140C by both IsPlayerVehicleActuallyInShowtime
//     (`lbz r3,0x140C(r3); blr`) and IsCrashingNormally. Per project rule the absolute console
//     offset is pinned BY NAME (the ~0x1400 bytes of base+VehiclePhysics state that precede it
//     are not reproduced as padding here).
//   * GetHeightAboveRoad iterates the four driven wheels (VehiclePhysics::maWheels, console
//     stride 0xE0) reading each wheel's road-contact result and the vehicle up axis -- accessed
//     here BY NAME through the VehiclePhysics accessors, not by absolute offset.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, EntityId
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"   // base + Wheel
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::{Dot, Subtract}
// BrnPlayerDriverControls has ONE definition -- the canonical owning home in SharedIO. (A prior
// duplicate minimal slice lived here and clashed (ODR) with that home; removed. The typed control
// accessors the showtime/aftertouch bodies call are declared on the canonical struct.)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"

namespace BrnPhysics
{
namespace Vehicle
{
    class RaceCarPhysics : public VehiclePhysics
    {
    public:
        // @0x827E42B0: trivial getter -- true while the player car is actually in showtime.
        //   asm: lbz r3,0x140C(r3); blr
        bool IsPlayerVehicleActuallyInShowtime() const
        {
            return mbPlayerCarInShowtime;
        }

        // @0x827E42B8: a crash counts as "normal" (i.e. NOT a special showtime/bounce crash)
        // unless the player car is in showtime AND the global bounce-boosting flag is set.
        //   asm: if (!mbPlayerCarInShowtime) return true;
        //        if (!<global bounce flag>) return true;
        //        return false;
        // FLAG (rodata): the global byte (byte_82FB84B2, sited next to lbBounceBoosting) is an
        // un-homed module static; carried here as an honest extern-declared global rather than a
        // fabricated value. The Vehicle-physics group does NOT home it (it belongs to a showtime/
        // bounce TU) -- declared extern so this leaf resolves it BY NAME.
        bool IsCrashingNormally() const;

        // @0x825B3998: the (signed) height of the car above the road, taken as the MINIMUM over the
        // driven wheels whose road-contact line test is valid AND that are on the ground (the
        // contact normal points up: dot(normal, up) > 0.5). For each such wheel the height is the
        // projection of the wheel position relative to the contact onto the contact normal. Wheels
        // that fail the on-ground test do not lower the running minimum. The X360 returns the
        // result broadcast across a VMX register; here a flat Vector3 with the height in every lane.
        //
        // FLAG (semantic): the asm's per-wheel reference vector (the `v1` operand of the
        // `vsubfp v12,v1,v12` height step) is not separately homed -- it is the wheel's own contact
        // position relative to the contact plane, reconstructed here as
        // dot(position - contactPoint, normal). The on-ground threshold (0.5) and the seed "max"
        // value are the values the X360 decompiler resolved for the inlined constants.
        Vector3 GetHeightAboveRoad() const;

        // ----- ADDITIVE GROW (Deformation car-car-impulse group): two bounce-state methods the
        //       car-car shunt path calls. DECLARE-ONLY -- their bodies are owned by a separate
        //       showtime/bounce RaceCarPhysics TU; ApplyCarCarImpulse only needs the declarations
        //       to compile under the per-TU `cl /c` gate. Signatures are DWARF-authoritative
        //       (references/DecFIGS/dwarfdump/.../RaceCarPhysics.h:331 / :382). -----

        // @ DWARF :382. True while this race car is currently bounce-boosting (showtime bounce).
        // The car-car bounce shaping picks a different boost-scale vector depending on this.
        bool IsBounceBoosting() const;

        // @ DWARF :331. Record that this race car just bounced off another body this frame: the
        // bounce impulse direction, whether this is the first/chained bounce, whether the impact
        // exceeded the minimum bounce stress, and the other entity's id. Declare-only.
        void SetJustBounced(Vector3 lvBounceDirection, bool lbFirstBounce, bool lbOverMinStress,
                            EntityId lOtherEntityId);

        // ----- ADDITIVE GROW (takedown-chain group): the per-vehicle crash-state latch the
        //       VehicleManager-level SetRaceCarCrashing calls (X360 RaceCarPhysics::SetCrashing
        //       @0x825B8A70). When crashing it snapshots the live velocity/orientation vectors into
        //       the crash-replay slots and zeroes the crash-blend scalar (it dispatches down the
        //       VehiclePhysics/SimpleVehiclePhysics::SetCrashing chain via vtbl+8 first). The bool is
        //       gated on PROXIMITY TO THE PLAYER CAMERA by the caller. DECLARE-ONLY -- bodied by a
        //       separate RaceCarPhysics crash-state TU. Signature DWARF-authoritative (int/char). -----
        void SetCrashing(bool lbCrash);

        // =====================================================================================
        // ADDITIVE GROW (C10 showtime / aftertouch / target-assist / bounce-boost group).
        // These methods make up the player/AI race-car superpowers layered on top of the driving
        // sim. Almost every one asserts mbPlayerCarInShowtime first and then reads/writes the
        // MODULE-STATIC showtime singleton msPlayerParams (see PlayerParameters below) -- only one
        // car can be in showtime at a time, so the showtime state is NOT per-instance. The single
        // per-instance bits these add are mbUsingAftertouch (+0x140D) and mPropCollisionImpulseSum
        // (+0x13F0), both pinned BY NAME.
        // =====================================================================================

        // @0x826415E8: the per-frame entry point. Maintains mfSlamSteering (a stick-driven extra
        // steer scalar, deadzone 0.1, decay 0.95/frame, clamp +/-10), flushes accumulated prop-hit
        // impulses via ApplyPropCollisionImpulseSum, chains to VehiclePhysics::Update (+ a follow-up
        // UpdateSteering on the engine-only path), then decays the showtime/uncapped-speed timers in
        // the singleton and latches mbUsingAftertouch. lpControls is the driver-control block.
        void Update(s32 a2, const BrnPlayerDriverControls* lpControls, bool lbApplyAftertouch,
                    s32 a5, s32 a6, s32 a7);

        // @0x825FFBD8: the showtime bounce-boost state machine, run each frame from UpdateAftertouch
        // while in showtime. Decides if the car is airborne/slow enough to bounce, runs the latch
        // (latch armed externally by SetJustBounced; chain counter muBounceChainCount), on a valid
        // bounce applies a spin AddWorldSpaceAngularImpulse + an AddAirRam boost along
        // normalize(mAimDirection + worldUp); when mfTimeUntilPush expires fires a launch pop + spin
        // and sets mbBounceBoosting; finally CapShowtimeVelocities. lfTimeStep is the frame dt
        // (passed in a VMX lane), lvAimSpin / lvLaunchSpin the spin-impulse vectors.
        void UpdateShowtimePhysics(const Vector3& lvLaunchSpin, const Vector3& lvAimSpin,
                                   f32 lfTimeStep);

        // @0x825D7940: derive the per-frame bounce deformation modifiers from the live deformation
        // state (per-sensor crush magnitudes -> a clamped bounce-strength array), and the global
        // deformation scale flt_82FB84B4 = sqrt(totalCrush) / numSensors. lpDeformationState is a
        // BrnDeformationState*.
        void UpdateShowtimeBounceModifiers(const void* lpDeformationState);

        // @0x825D7600: clamp the showtime velocity each frame -- speed to one of two caps (boosting
        // vs not) and the vertical component separately, UNLESS IsPlayerVehicleWithUncappedShowtimeSpeed
        // lets a fresh launch briefly exceed the vertical cap.
        void CapShowtimeVelocities();

        // @0x825B8BC0: the showtime player-car "strength" (damage budget), stored in the singleton.
        f32 GetShowtimePlayerCarStrength() const;

        // @0x825D7B68: true while the player car is in showtime AND not in the brief post-bounce
        // disable window (msPlayerParams.mbDisableShowtime) AND past the launch-push delay.
        bool IsPlayerVehicleInShowtime() const;

        // @0x825B8C18: true during the brief window (mfUncappedSpeedTimer > 0) when a fresh launch
        // may exceed the vertical showtime speed cap.
        bool IsPlayerVehicleWithUncappedShowtimeSpeed() const;

        // @0x826000F8: enter (or refresh) showtime. Resets the singleton, gives the car a double
        // impulse launch (overwrites mLinearVelocity with a scaled push AND fires an AddAirRam --
        // input-space 5130 rising / 3082 falling), seeds mfTimeUntilPush and a deformation/damage
        // budget. lfPlayerCarStrength is stored verbatim; lfPlayerCarDamageLimit scales the budget.
        void SetPlayerVehicleInShowtime(bool lbInShowtime, f32 lfPlayerCarStrength,
                                        f32 lfPlayerCarDamageLimit);

        // @0x825B8AF0: stash the showtime aim direction (a single VMX register) into the singleton.
        void SetShowtimeAimDirection(const Vector3& lvAimDirection);

        // @0x8262EBE8: camera-relative air-steer. Normalizes the camera matrix's X and Z axes, reads
        // stick deflection via GetAftertouchValues (yaw/pitch/scalar, + optional SIXAXIS tilt), and
        // applies (1) a world-space lateral force along camera-X, (2) a world-space roll angular
        // impulse, (3) local pitch impulses. Magnitudes differ for showtime vs normal flight, with an
        // extra IsBounceBoosting multiplier. From showtime it chains UpdateTargetAssist +
        // UpdateShowtimePhysics. Gated on the car being airborne (mbIsCrashing here means in-air-ish).
        void UpdateAftertouch(const BrnPlayerDriverControls* lpControls,
                              const Matrix44Affine* lpCameraMatrix,
                              bool lbDoForceAdditiveAftertouch, bool lbUseSixaxis);

        // @0x825B8C88: trivial getter -- true while aftertouch air-steer is active this frame.
        bool IsUsingAftertouch() const { return mbUsingAftertouch; }

        // @0x8261FF50: showtime auto-aim. Argmin over a GLOBAL candidate target list
        // (msTargetPositions / msNumTargets), score weight = (2 - alignmentDot) * (1/distance) with a
        // stickiness bonus for last frame's target, only while moving upward; lerps an aim direction
        // and, when aligned, pulls velocity toward a ballistic intercept (ComputeIdealVelocity).
        void UpdateTargetAssist(const BrnPlayerDriverControls* lpControls);

        // @0x82600558: solve a projectile arc to a target. Flattens the target-relative vector; if
        // horizontal distance >= 1.0, horizontal = dir/(2t), vertical = t^2 * 9.81, with
        // t = horizDist / (KF_IDEAL_T_BASE - lfInputSpeed); else returns the target's own velocity.
        // Writes the result to *lpResult (return is lpResult). lfInputSpeed = the car's 2D speed.
        Vector3* ComputeIdealVelocity(Vector3* lpResult, f32 lfInputSpeed) const;

        // @0x825B8B08: copy out the recent-bounce report (chain count / over-min-stress / car-bounce /
        // good-impact flags, the other entity id, and the bounce direction vector) and consume the
        // bounce latch. Returns the "bounced this frame" flag and clears it + the per-frame flags.
        bool GetRecentBounce(s32* lpChainCount, bool* lpOverMinStress, bool* lpCarBounce,
                             bool* lpGoodImpact, bool* lpExtraFlag, s32* lpOtherEntityId,
                             Vector3* lpBounceDirection);

        // @0x825B8CE0: true if the next impact should bounce-boost (the latched ShouldBounceBoost bit).
        bool ShouldBounceBoostNextImpact() const;

        // @0x825B3928: copy out the collision normal that caused the crash (asserts mbIsCrashing).
        // Writes to *lpNormal (return is lpNormal).
        Vector3* GetNormalCausingCrash(Vector3* lpNormal) const;

        // @0x82600780: flush the accumulated per-frame prop-collision impulse (mPropCollisionImpulseSum)
        // into the body, soft-clamping its magnitude (against the car's mass/speed) so props can't
        // catapult the car, then zeroing the accumulator.
        void ApplyPropCollisionImpulseSum();

        // @0x825FFAE8: record a wheel/surface traction point. Chains to the base
        // SimpleVehiclePhysics::AddTractionPoint, then -- if the showtime push timer has elapsed --
        // snapshots the wheel's road-contact record and flags it.
        void AddTractionPoint(s32 leWheel, u32 luSurfaceTag);

        // ----- ADDITIVE GROW (stunt-offences group): seven declare-only race-car stunt-state
        //       accessors BrnPhysics::StuntOffencesManager reads by name (drift / convoy /
        //       tailgating). Bodied by the owning RaceCarPhysics/SimpleVehiclePhysics TU later.
        //       Only the console offsets are asm-proven; FLAG: IsConsideredAirborne and the
        //       Stunt* names are proposed-by-role. The stunt code MUST go through these (host
        //       vptr is 8 bytes -- raw console offsets would be wrong). -----
        f32     GetDriftActiveTime() const;        // +0x109C: drift-active timer (>0 while drifting)
        f32     GetDriftLateralSpeed() const;      // +0x1010 lane 2: drift lateral (Z) speed
        bool    IsHandbrakeHeld() const;           // +0x135B: handbrake-held byte
        bool    IsConsideredAirborne() const;      // +0x1350: physics "should be airborne" gate
        Vector3 GetStuntReferenceVelocity() const; // +0x6C0: stunt reference velocity register
        Vector3 GetStuntWorldPosition() const;     // +0x1340: world position for tailgating tests
        Vector3 GetStuntForwardAxis() const;       // normalized +0x1340 velocity = cone forward axis

    private:
        bool mbPlayerCarInShowtime;   // +0x140C (pinned BY NAME)

        // ----- ADDITIVE GROW (C10): per-instance race-car state the C10 functions touch. -----

        // @+0x140D (BY NAME). Latched each frame by Update; read by IsUsingAftertouch. The asm
        // stores the aftertouch-enable bool to this+0x140D (`stb r11,0x140D(r31)`) and the getter
        // returns `lbz r3,0x140D(r3)`.
        bool mbUsingAftertouch;

        // @+0x13F0 (BY NAME). The accumulated prop-collision impulse summed across the frame, flushed
        // by ApplyPropCollisionImpulseSum (asm: `addi r31,this,0x13F0`). Read/written as a single VMX
        // register. The ~0x13F0 bytes of base+VehiclePhysics state before it are not padded here.
        Vector3 mPropCollisionImpulseSum;

        // @+0x1404 (BY NAME). The stick-driven extra-steer scalar Update maintains (mfSlamSteering;
        // the asm reads/writes `this->float1404`). Decays 0.95/frame, deadzone 0.1, clamp +/-10.
        f32 mfSlamSteering;

        // @+0x1430 (BY NAME). A short-lived slam/steer envelope scalar Update also drives on the
        // engine-only path (the asm's `this->float1430`).
        f32 mfSlamSteerEnvelope;

        // @+0x1440 (BY NAME; DWARF RaceCarPhysics.h:414). The collision normal that caused the
        // current crash, snapshotted when the crash begins; read out by GetNormalCausingCrash
        // (asm: lvx128 v0, this, 0x1440). The ~0x1440 bytes of preceding base+VehiclePhysics+
        // RaceCarPhysics state are not padded here.
        Vector3 mCrashNormal;
    };

    // =========================================================================================
    // PlayerParameters -- the MODULE-STATIC showtime singleton (X360 msPlayerParams, base symbol
    // lbBounceBoosting @0x82FB8480). Only ONE car can be in showtime, so all of showtime's mutable
    // state lives here, not on the instance. Reconstructed BY NAME from PlayerParameters::Reset
    // @0x825B89B8 (the exact store offsets) + the named globals the C10 functions reference. The
    // members are pinned at their console byte offsets; intervening bytes that no C10 function reads
    // are reproduced as named reserved fields ONLY where needed to hold a later member's offset.
    //
    // FLAG: several of these are written from un-homed .rdata seeds (flt_82F2A2xx); those numeric
    // SEEDS are placeholders (see the .cpp), but the singleton LAYOUT and the member roles are exact.
    // =========================================================================================
    struct PlayerParameters
    {
        // ---- +0x00..+0x10 : bounce report + latch scalars ----
        bool  mbBounceBoosting;        // +0x00  lbBounceBoosting
        bool  mbJustBounced;           // +0x01  byte_82FB8481 (SetJustBounced latch)
        bool  mbBouncedThisFrame;      // +0x02  byte_82FB8482 (GetRecentBounce return + consume)
        bool  mbCarBounce;             // +0x03  byte_82FB8483
        bool  mbGoodImpact;            // +0x04  byte_82FB8484
        u8    mu8Reserved05;           // +0x05  (alignment hole before the u16)
        u16   muBounceChainCount;      // +0x06  word_82FB8486
        bool  mbShouldBounceBoost;     // +0x08  byte_82FB8488
        bool  mbBounceBoostPending;    // +0x09  byte_82FB8489 (ShouldBounceBoostNextImpact)
        bool  mbSixaxisTiltApplied;    // +0x0A  byte_82FB848A
        bool  mbGoodImpactReport;      // +0x0B  byte_82FB848B (GetRecentBounce flag 5)
        s32   miOtherEntityId;         // +0x0C  dword_82FB848C (SetJustBounced a4)

        // ---- +0x10 : bounce / aim direction (one VMX register) ----
        Vector3 mBounceDirection;      // +0x10  (SetJustBounced/SetShowtimeAimDirection VMX @ +0x10)

        // ---- +0x20..+0x30 : unread interior (a 16B VMX scratch the asm does not read by name) ----
        u8    maReserved20[0x30 - 0x20];

        // ---- +0x30..+0x4C : showtime launch/timer block (Reset zeroes / seeds these) ----
        bool  mbDisableShowtime;       // +0x30  byte_82FB84B0 (IsPlayerVehicleInShowtime gate)
        bool  mbBounceWasGood;         // +0x31  byte_82FB84B1
        bool  mbLaunchActive;          // +0x32  byte_82FB84B2 (CapShowtimeVelocities gate)
        bool  mbLaunchSpin;            // +0x33  byte_82FB84B3
        f32   mfDeformationScale;      // +0x34  flt_82FB84B4 (UpdateShowtimeBounceModifiers result)
        f32   mfDamageBudget;          // +0x38  flt_82FB84B8 (= seed * lfPlayerCarDamageLimit)
        f32   mfUncappedSpeedTimer;    // +0x3C  flt_82FB84BC (>0 -> uncapped vertical speed window)
        f32   mfReserved40;            // +0x40
        f32   mfTimeUntilPush;         // +0x44  flt_82FB84C4 (launch-push delay countdown)
        f32   mfPlayerCarStrength;     // +0x48  lfShowtimePlayerCarStrength

        // ---- +0x50..+0xD0 : showtime aim/timer scratch the asm splats (not read by name here) ----
        u8    maReserved4C[0xD0 - 0x4C];   // +0x4C..+0xD0 (incl. unk_82FB84D0 target-pos list base)

        // ---- +0xD0..+0x110 : target-assist candidate list (filled by a game-side TU) ----
        s32   maTargetIds[8];          // +0xD0  dword_82FB8550 (per-candidate id, 8 slots)
        s32   miNumTargets;            // +0xF0  dword_82FB8570
        s32   miCurrentTargetId;       // +0xF4  dword_82FB8574 (Reset -> -1)
        u8    maReservedF8[0x110 - 0xF8];  // +0xF8..+0x110

        // ---- +0x110.. : bounce-sensor count ----
        u8    mu8NumBounceSensors;     // +0x110 byte_82FB8590 (Reset -> 0)

        // @0x825B89B8: zero/seed the singleton on showtime entry. Bodied in RaceCarPhysics.cpp.
        void Reset();
    };
    // The +0x00..+0x4C scalar block and the +0xD0.. target block are ONE contiguous singleton
    // (0x4C < 0xD0, no overlap). The maReserved* gaps bridge the unread interior so every named
    // member lands at its true console offset; verified by offsetof asserts in RaceCarPhysics.cpp's
    // never-called _AssertPlayerParamsLayout(). The X360 also keeps per-candidate target POSITIONS
    // at +0x50 (unk_82FB84D0, 16B stride) inside maReserved4C -- read there by UpdateTargetAssist via
    // a raw offset rather than a named member (the position array is parallel to maTargetIds).

    // The single process-wide showtime singleton (X360 msPlayerParams; base = lbBounceBoosting).
    extern PlayerParameters msPlayerParams;
}
}
