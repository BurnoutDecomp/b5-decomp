#pragma once

// BrnPhysics::Vehicle::RaceCarPhysics -- the player/AI race-car physics body (a VehiclePhysics
// subclass that adds showtime, bounce-boost, target-assist and crash state).
//
// MINIMAL OWNING SLICE for the METHODS. The full RaceCarPhysics carries ~100 methods owned by
// separate future TUs and only a few are declared here.
//
// ⭐⭐ THE DATA IS NO LONGER A SLICE (2026-08-03). All SIXTEEN of this class's own members are now
// declared, in DWARF order, at their X360 seats -- see the big block above the private section.
// The block is width- and alignment-identical on x64, and its derivation closes independently on
// sizeof == 5216, the asm-literal per-car stride in VehicleManager::Construct. That closure is what
// makes it a derivation rather than an accumulation of plausible single fields.
//
// LAYOUT NOTES (from the asm):
//   * The class as a whole is NOT byte-pinned: the ~0x13F0 bytes of ExternalPhysicsBody /
//     SimpleVehiclePhysics / VehiclePhysics state that precede the own block are not reproduced as
//     padding here (project rule -- parity by NAMED MEMBERS). What IS pinned, and compiled, is
//     every own member's offset RELATIVE to the first of them; see _AssertOwnBlockLayout.
//   * GetHeightAboveRoad iterates the four driven wheels (VehiclePhysics::maWheels, console
//     stride 0xE0) reading each wheel's road-contact result and the vehicle up axis -- accessed
//     here BY NAME through the VehiclePhysics accessors, not by absolute offset.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, EntityId
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"   // base + Wheel
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::{Dot, Subtract}
#include "rw/physics/inertia.h"              // rw::physics::Inertia -- Prepare's by-value 7th parameter
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
        // ⭐ VehicleManager owns the eight-car array (`RaceCarPhysics maRaceCarVehicles[8]`,
        // BrnVehicleManager.h) and the X360 build reaches these members BARE, off an absolute
        // per-car offset, from VehicleManager::SetRaceCarCrashing @0x82634C90 and
        // VehicleManager::ApplyPlayerStats @0x8259BFE8 -- no accessor, no assert. Routing those
        // reads through getters would add asserts the console does not fire there, so the console
        // access is reproduced by friendship instead. Same precedent as PhysicalTrafficManager,
        // which is a friend of this class's owner for exactly the same reason.
        friend class VehicleManager;

    public:
        // ⭐ Construct @0x8263BF10..0x8263BF38 -- INLINED into the eight-car loop of
        // VehicleManager::Construct @0x8263B7C8, which is the only place it is issued. It is a
        // real function and not a hand-rolled sequence of pokes: the PS3 DecFIGS build calls it
        // OUT OF LINE (RaceCarPhysics::Construct @0x6EB3D4) and its body there is exactly this
        // one -- `bl VehiclePhysics::Construct` then the same six writes, at the uniform Δ = −16
        // that separates the two builds in this region:
        //     PS3 @0x6EB3D4  bl   VehiclePhysics::Construct
        //         @0x6EB404  stvx v13(0), this, 0x13E0   <- X360 +0x13F0  mPropCollisionImpulseSum
        //                    stfs 0.0f, 0x13F8(this)     <- X360 +0x1408  mfBeachedTime
        //                    stfs 0.0f, 0x13F0(this)     <- X360 +0x1400  mfTimeSinceTookDownPlayer
        //                    stb  0,    0x13FD(this)     <- X360 +0x140D  mbUsingAftertouch
        //                    stb  0,    0x13FC(this)     <- X360 +0x140C  mbPlayerCarInShowtime
        //                    <lvx/vperm/stvx at this+0x1060>  <- X360 +0x1070, the Z-lane insert
        // ⚠️ SEVEN writes, not six: the X360 loop body ALSO re-zeroes lane .z of the +0x1070
        // register (`lvx128 v0,r0,r11 ; vrlimi128 v0,v127,2,0 ; stvx128 v0,r0,r11` with
        // r11 == record + 0x1070), a lane VehiclePhysics::Construct has already cleared. The
        // redundancy is in both builds; it is reproduced rather than optimised away.
        // ⛔ NOTE WHAT IS *NOT* HERE. Nine of this class's sixteen own members are left untouched
        // (mfSlamSteering, mu8StrengthStat, mInitialCrashVel/AngVel, mfCrashTimer, mbAISlowMo,
        // mbWroteIntoRWInSlowMo, mbDeformedBeyondDriveTimeLimitsInCrash, mCrashNormal,
        // mEntityCausingCrash, mbDebugShowTargetAssist). That is the console's behaviour, not an
        // omission: Prepare and the crash paths seed them. Do not "complete" it.
        void Construct();

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

        // ==========================================================================================
        // ⭐⭐ THE CREATE LEG'S VCALL TARGET -- @0x82639CB8, X360 vtable slot +0x30.
        //
        // Declared here for the FIRST TIME (2026-08-11, create-drain wave). Every previous wave
        // recorded this as "an export hole with no declaration anywhere in this tree" and planned to
        // lift it from the image. It did not have to be lifted: the X360 hole is covered by the PS3
        // export set, which carries the full mangled name with parameter names --
        //     PS3 0x73648C  _ZN10BrnPhysics7Vehicle14RaceCarPhysics7PrepareE
        //         N2rw4math3vpu14Matrix44AffineE NS4_7Vector3E S6_ S6_ S6_
        //         RKN12CgsGeometric14AxisAlignedBoxE NS2_7physics7InertiaE
        //         PNS0_14VehicleAttribsE PKS6_ PKf h        (89 insns)
        // i.e. VehiclePhysics::Prepare's nine parameters plus `rw::physics::Inertia lInertia` after
        // the AABB and `u8 lu8StrengthStat` at the tail.
        //
        // ⭐ AND THE X360 CALL SITE CONFIRMS ALL ELEVEN, register for register -- read from
        // VehicleManager::ProcessCreateEvents @0x826171D8..0x8261724C:
        //     r3 = &maRaceCarVehicles[idx]   (mulli r11,r27,0x1460 ; addi r3,r11,0x740)
        //     r4 = &var_8C0                  -> lOnRoadTransform
        //     v1/v2/v3/v4                    -> lLinearVelocity / lAngularVelocity /
        //                                       lHandlingBodyOffset / lHalfExtent.
        //                                       ⭐ v3 is v125, the COM vector the create body's
        //                                       four-wheel loop accumulates.
        //     r5 = &var_760                  -> lAABB -- the buffer StreamedDeformationSpec::
        //                                       GetBoundingBox filled one instruction earlier.
        //     r6..r10 + stack@0x98           -> lInertia BY VALUE: six doublewords == 48 bytes ==
        //                                       three 16-byte rows, the output of the create body's
        //                                       ~200-insn VMX Newton-Raphson inertia block.
        //     stack@0xA4/0xAC/0xB4           -> lpAttribs / lpaWheelPositions / lpafWheelRadii
        //     stack@0xBF (a byte)            -> lu8StrengthStat  (the event's miCarStrengthStat)
        //     lwz r11,0(r3) ; lwz r11,0x30(r11) ; mtctr ; bctrl
        //
        // ⚠️ Reached BY NAME, and `virtual` -- see the VIRTUAL note at the end of this banner.
        // The console dispatches through slot +0x30, but the only console call site
        // (ProcessCreateEvents) makes the vcall on maRaceCarVehicles[idx] -- static type
        // RaceCarPhysics -- so a by-name call on the exact type reproduces the dispatch exactly,
        // and this tree's rule is to reach members BY NAME and never by console slot (the host
        // vtable's numbering is the compiler's, not the console's; see the
        // [[vtable-slot0-Create-shim]] precedent). NOTE this is NOT an override of
        // VehiclePhysics::Prepare (nine params) -- it is a distinct eleven-param function that
        // FORWARDS into it. It therefore introduces its OWN slot rather than replacing one.
        // ⚠️ The old "rw::physics::Inertia has NO type in this tree" line is STALE and deleted:
        // vendor/renderware/include/rw/physics/inertia.h has owned the record since task #141
        // (DWARF-named members, X360-attested offsets, 48-byte array stride pinned).
        //
        // ==========================================================================================
        // ⭐⭐ LANDED 2026-08-11 (create-drain wave). The 42-instruction stream was recovered by a
        // TARGETED IDA EXPORT over BURNOUT_X360_ARTIST.XEX.i64 (the b53e2523 technique, re-run by
        // the conductor on a DB copy: the per-function JSON hole is real, but the DATABASE carries
        // the bytes -- the function was materialised at 0x82639CB8..0x82639D5B and decompiled
        // cleanly). The park block that stood here (prepare-chain wave, same day) is retired per
        // its own DELETE-WHEN; its three findings -- the 42-insn extent, the five-entry callee set
        // (VehiclePhysics::Prepare @0x82637C80, SetTransformFromPositionOnRoad @0x825D1C00, the
        // Begin/Fire/EndAssert trio), both callees real -- are all CONFIRMED by the recovered
        // stream, instruction for instruction. The full decode is in the body's banner
        // (RaceCarPhysics.cpp).
        //
        // What the stream settled -- the park's four open questions:
        //   * ORDER: VehiclePhysics::Prepare FIRST (bl @0x82639CD8, return captured in r28), then
        //     the strength assert, the strength store, SetTransformFromPositionOnRoad, then a
        //     seven-member zeroing tail.
        //   * ASSERT: "lu8StrengthStat < KU8_MAX_STRENGTH", RaceCarPhysics.cpp line 0xC0 == 192;
        //     the guard is `cmplwi r11, 0xB ; blt` -- fires at >= 11, so KU8_MAX_STRENGTH == 11,
        //     asm-attested (constant defined below).
        //   * lInertia IS NEVER READ on X360. The body reloads the three trailing pointers from
        //     its own incoming stack slots (a48/a50/a52 -> r6/r7/r8) for the VehiclePhysics::
        //     Prepare call, and the inertia's r6..r10 + stack doubleword are simply dropped. The
        //     "where is lInertia stored" intuition came from the PS3's 89-insn body; on X360 the
        //     inertia's consumer is elsewhere on the create path (the drain hands it to the
        //     rigid-body request). The parameter STAYS in the signature -- the call site marshals
        //     it -- and is deliberately unnamed in the definition.
        //   * RETURN: the callee's bool, verbatim (mr r28,r3 ... mr r3,r28), named
        //     lbPreparedVehiclePhysics after the pseudocode's own register role.
        //
        // ==========================================================================================
        // ⭐⭐⭐ INDEPENDENTLY CONFIRMED BY A SECOND, DIFFERENT DERIVATION (2026-08-11, the other
        // create-drain wave, merged here). That wave could not get the X360 bytes and instead
        // derived the body STRUCTURALLY from the PS3 DecFIGS twin -- **PS3 0x73648C, 89 insns**,
        // symbolled and decompiled -- then mapped its stores onto this class through the UNIFORM
        // Δ = +16 that this header's own member table already establishes for the region (attested
        // three ways: IsPlayerVehicleActuallyInShowtime 0x13FC/0x140C, IsUsingAftertouch
        // 0x13FD/0x140D, Construct's Z-lane insert 0x1060/0x1070). The two derivations were done
        // from different binaries, on different days, and they land on the SAME body:
        //     PS3 0x13FE -> +0x140E  mu8StrengthStat            X360 `stb r29, 0x140E`      ✓
        //     PS3 0x13F4 -> +0x1404  mfSlamSteering             X360 `stfs f0, 0x1404`      ✓
        //     PS3 0x134F -> +0x135F  mbIsWedgedInWorld          X360 `stb 0, 0x135F`        ✓
        //     PS3 0x13F8 -> +0x1408  mfBeachedTime              X360 `stfs f0, 0x1408`      ✓
        //     PS3 0x1425 -> +0x1435  mbWroteIntoRWInSlowMo      X360 `stb 0, 0x1435`        ✓
        //     PS3 0x13E0 -> +0x13F0  mPropCollisionImpulseSum   X360 `stvx128 v0, 0x13F0`   ✓
        //     PS3 0x1424 -> +0x1434  mbAISlowMo                 X360 `stb 0, 0x1434`        ✓
        //     PS3 0x1426 -> +0x1436  mbDeformedBeyond...InCrash X360 `stb 0, 0x1436`        ✓
        // Same forward-first ordering, same seven-member zeroing tail, same verbatim return of the
        // callee's bool. Only the STORE ORDER inside the tail differs, which is compiler scheduling.
        // ⭐ AND THE ASSERT BOUND CONVERGES TOO, from two different encodings: X360 is
        // `cmplwi r11, 0xB ; blt` (fires at >= 11, i.e. asserts `< 11`) and PS3 is
        // `cmplwi cr7, 0xA ; ble` (skips at <= 10, i.e. asserts `<= 10`). For an unsigned byte those
        // are the SAME predicate, so KU8_MAX_STRENGTH == 11 is doubly attested. The X360 stream is
        // what the body reproduces (rung 1); the PS3 is the corroboration, not the source.
        // ⚠️ Where the two ever disagree, the X360 wins and this note is the audit trail.
        //
        // ⭐⭐ VIRTUAL -- ADOPTED FROM THE PS3-DERIVED WAVE. The console reaches this through vtable
        // slot +0x30 of a statically known `RaceCarPhysics*` (`lwz r11,0(r3) ; lwz r11,0x30(r11) ;
        // bctrl`), which only happens if the function is virtual. It is declared virtual HERE
        // rather than in VehiclePhysics because the arities differ (11 vs 9), so it introduces its
        // OWN slot; the host compiler assigns the number and nothing anywhere reaches it by index
        // (⛔ per [[vtable-slot0-Create-shim-bug]], no console slot number is spelled in code).
        // ⚠️ IT COSTS NOTHING IN LAYOUT: SimpleVehiclePhysics already introduces the virtuals for
        // this hierarchy, so the vptr exists either way and `sizeof(RaceCarPhysics) == 5216` (the
        // console's 0x1460 stride) is unchanged -- which BrnVehicleManager_layout_check.cpp asserts.
        // ⚠️ OVERLOAD-HIDING CHECKED: declaring any `Prepare` here hides the base's nine-parameter
        // `VehiclePhysics::Prepare` (that was already true before `virtual`). Swept the tree -- the
        // only calls to the nine-parameter form are explicitly qualified (`VehiclePhysics::Prepare`
        // in this class's own body and in TrafficPhysics::PreparePhysical, which is a SIBLING
        // subclass, not a RaceCarPhysics). No call site invokes the 9-param form ON a RaceCarPhysics
        // object, so no `using VehiclePhysics::Prepare;` is needed. Add one if that ever changes.
        // ==========================================================================================
        static const u8 KU8_MAX_STRENGTH = 11;   // asm-attested: Prepare's `cmplwi 0xB` guard
                                                 // (PS3 twin's `cmplwi 0xA ; ble` agrees)

        virtual bool Prepare(Matrix44Affine lOnRoadTransform, Vector3 lLinearVelocity,
                             Vector3 lAngularVelocity, Vector3 lHandlingBodyOffset,
                             Vector3 lHalfExtent, const AxisAlignedBox& lrAABB,
                             rw::physics::Inertia lInertia, VehicleAttribs* lpAttribs,
                             const Vector3* lpaWheelPositions, const f32* lpafWheelRadii,
                             u8 lu8StrengthStat);   // @0x82639CB8 (41)

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
        // ⭐ THE TIMESTEP IS lrTimeStep.x -- RECOVERED FROM THE ASM 2026-08-01 (physics wave 1).
        // The entry prologue saves BOTH incoming vector registers (`vmr128 v126, v2` and
        // `vmr128 v127, v1` at 0x8264160C/0x82641614) and the very first thing the CRASHING
        // branch does is spill v2 and read LANE 0 as a scalar float:
        //     addi r11, r1, var_80 ; stvx128 v126, r0, r11
        //     lfs  f13, var_80(r1)              ; == v2.x
        //     lfs  f0, 0x1430(r31) ; fadds f0, f0, f13 ; stfs f0, 0x1430(r31)
        // i.e. `mfCrashTimer += v2.x`.
        // ⚠️ CORRECTED 2026-08-03: this note used to say "the engine-only branch" and
        // "mfSlamSteerEnvelope". The branch is gated on `lbz r11,0x710(r31)` == mbCrashing, and
        // +0x1430 is mfCrashTimer -- see the ⛔ block over the member declarations below. It also
        // matters WHICH vector each timer uses: mfCrashTimer integrates v2 (the REAL timestep, so
        // the slow-motion window is real-time-bounded) while mfTimeSinceTookDownPlayer @+0x1400 and
        // mfBeachedTime @+0x1408 integrate v1 (the SIM timestep, which the slow-mo path scales).
        // Both vectors are restored verbatim
        // (`vmr128 v2,v126 ; vmr128 v1,v127` @0x8264185C) immediately before the chained
        // VehiclePhysics::Update, so they are pass-through arguments, not scratch.
        // ⚠️ FLAG: only the REGISTER assignment is recovered, not the console's declaration
        // POSITION for the two vector parameters -- vector args do not consume GPR slots on the
        // X360 ABI, so their place in the C++ parameter list cannot be read off the asm. They are
        // appended here. (Hex-Rays renders this function as 7 all-integer arguments and drops
        // both vectors outright -- the twelfth instance of that failure in this project.)
        // ⭐⭐ SIGNATURE CONFORMED 2026-08-06 (UpdateVehiclePhysics wave) -- the DWARF's own
        // declaration order and the manager call site @0x82645A10..5C recover the four
        // placeholder args: a2 = the camera matrix, a5 = player-aftertouch-additive,
        // a6 = (meShowtimeBehaviour == 2), a7 = the manager's Random. See VehiclePhysics.h.
        void Update(const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                    const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                    bool lbPlayerAftertouchForceAdditive, bool lbShowtimeAllowed,
                    CgsNumeric::Random& lrRandom,
                    Vector3 lrPassThroughV1, Vector3 lrTimeStep);

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
        // ⭐⭐ WIDENED 2026-08-09 (crash/shunt wave) to the 5-arg DWARF virtual form so it
        // OVERRIDES VehiclePhysics's image-attested slot +0x28 (the RaceCarPhysics vtable
        // @0x820D1034 carries this @0x8262EBE8 there). The old 4-arg form dropped the VecFloat
        // dt the asm saves at entry -- a base-pointer dispatch would have reached the empty
        // traffic default instead of this body (the hollow-shell class of defect).
        void UpdateAftertouch(const BrnPlayerDriverControls* lpControls,
                              const Matrix44Affine* lpCameraMatrix,
                              VecFloat lvfTimeStep,
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

        // @0x825FFAE8: record a wheel/surface traction point. Chains register-transparently to
        // the base SimpleVehiclePhysics::AddTractionPoint, then -- once mfBeachedTime has
        // passed the 0.5s window -- promotes a close-to-ground wheel to on-ground.
        // ⭐ RE-SIGNATURED 2026-08-07 (orchestrator wave) to the console's 4-arg form: the asm
        // passes r4/r5/v1/v2 through untouched and the PS3 mangled name agrees
        // (_ZN...16AddTractionPointENS0_19EVehicleDrivenWheelEN2rw4math3vpu7Vector3ES6_j).
        // The old 2-arg spelling (s32, u32) existed nowhere in the image.
        void AddTractionPoint(EVehicleDrivenWheel leWheel, Vector3 lvPosition, Vector3 lvNormal,
                              u32 lu32CollisionTag);

        // ----- ADDITIVE GROW (stunt-offences group): seven declare-only race-car stunt-state
        //       accessors BrnPhysics::StuntOffencesManager reads by name (drift / convoy /
        //       tailgating). Bodied by the owning RaceCarPhysics/SimpleVehiclePhysics TU later.
        //       Only the console offsets are asm-proven; FLAG: IsConsideredAirborne and the
        //       Stunt* names are proposed-by-role. The stunt code MUST go through these (host
        //       vptr is 8 bytes -- raw console offsets would be wrong). -----
        //
        // ⛔⛔ DO NOT BODY THESE OFF THE ROLE NAMES. Measured 2026-08-03 (task #110): these seven
        // are the ONLY thing keeping BrnStuntOffencesManager.cpp out of the build (7 of the 10
        // LNK2019 a first mount produced), so the next wave will be tempted to knock them out as
        // one-line member returns. FOUR of the seven have an offset whose committed meaning in the
        // base classes CONTRADICTS the role in the comment -- the "address right, meaning wrong"
        // class this tree keeps paying for. Each conflict is between two already-committed,
        // separately asm-cited comments, so ONE of the two is wrong and neither wins by default:
        //
        //   +0x109C  GetDriftActiveTime   vs  VehiclePhysics.h:1447 -- mPreviousControls sits at
        //            +0x1090 and is the WHOLE 0x48-byte BrnPlayerDriverControls, so +0x109C is
        //            +0x0C INSIDE last frame's controls, not a free timer slot.
        //   +0x1010  GetDriftLateralSpeed ("lane 2")  vs  VehiclePhysics.h:1095 + VehiclePhysics.cpp
        //            :2099/:2462, which pin that register's lanes as .x=1/TimeToReachTargetDriftSlip,
        //            .y=StartSlip, .z=TimeDrifting, .w=BrakeScale. Lane 2 is a TIME, not a speed.
        //   +0x135B  IsHandbrakeHeld      vs  VehiclePhysics.h:1183 -- mbAllWheelsHaveTraction,
        //            pinned by `lbz r11,0x135B(r3)` and gated by VehiclePhysics_layout_check.cpp.
        //   +0x1340  GetStuntWorldPosition vs VehiclePhysics.h:1197 -- mNormLinearVelocityMag, the
        //            cached NORMALIZED LINEAR VELOCITY (xyz) + magnitude (.w), written by
        //            UpdateLinearVelocityMagnitude and likewise gated by the layout check. A unit
        //            vector is not a world position. (Note the sibling GetStuntForwardAxis reads
        //            the SAME +0x1340 and calls it a normalized velocity -- they cannot both hold.)
        //
        // The remaining three are consistent with the committed map: IsConsideredAirborne/+0x1350
        // == mbHasAir; GetStuntForwardAxis/+0x1340 == the normalized velocity; and
        // GetStuntReferenceVelocity/+0x6C0 == SimpleVehiclePhysics::mfSpeedMPH -- a SPLATTED SPEED,
        // which is why every call site reads only `.x >= 30.0f`; the "Velocity" in its name is
        // misleading but its use is not.
        // ⇒ Re-derive all four from the StuntOffencesManager asm that produced the offsets before
        // writing a single body. A wrong body here compiles, links, and silently feeds the whole
        // stunt/takedown scoring system the wrong field.
        f32     GetDriftActiveTime() const;        // +0x109C  ⛔ conflicts with mPreviousControls
        f32     GetDriftLateralSpeed() const;      // +0x1010 lane 2  ⛔ conflicts with TimeDrifting
        bool    IsHandbrakeHeld() const;           // +0x135B  ⛔ conflicts with mbAllWheelsHaveTraction
        bool    IsConsideredAirborne() const;      // +0x1350: mbHasAir (consistent)
        Vector3 GetStuntReferenceVelocity() const; // +0x6C0: mfSpeedMPH, splatted speed (consistent)
        Vector3 GetStuntWorldPosition() const;     // +0x1340  ⛔ conflicts with mNormLinearVelocityMag
        Vector3 GetStuntForwardAxis() const;       // normalized +0x1340 velocity = cone forward axis

    public:
        // Never called. Bodied in RaceCarPhysics_layout_check.cpp (a MOUNTED TU) and nothing but
        // static_asserts -- see the ⭐⭐ block below for why it has to be a compiled gate and why it
        // has to live in a separate TU. Static so it can see the private block through offsetof.
        static void _AssertOwnBlockLayout();

        // ⭐ ADDED 2026-08-11 (physics->output publish wave). DWARF RaceCarPhysics.h:351, and
        // X360-ATTESTED by VehicleOutputInterface::UpdateRaceCarState @0x825ECC40
        // (`lbz r11,0x1436(r30) ; cntlzw ; extrwi r11,r11,1,26 ; stb r11,0x44C(r31)` -- the byte,
        // negated, seeds RaceCarState::mbIsDriveable before the per-wheel AND-fold). The member it
        // returns is declared in the private block below with this accessor already named in its
        // comment; only the declaration was missing.
        bool GetDeformedBeyondDriveTimeLimitsInCrash() const
        {
            return mbDeformedBeyondDriveTimeLimitsInCrash;
        }

        // ⭐ ADDED 2026-08-11 (physics->output publish wave). DWARF RaceCarPhysics.h:363 -- the
        // "stuck on its belly for too long" query the member comment at mfBeachedTime already names.
        // X360-ATTESTED by VehicleManager::WriteOutVehicleStats @0x8263F6B0:
        //     lfs f0, 0x1408(r30) ; fcmpu cr6, f0, f31 ; bgt -> r11 = 1
        // with f31 == flt_82004270 == 3.0f (Hex-Rays renders the same compare as `> 3.0`). The
        // console emits it INLINE at that one site, which is why no out-of-line symbol exists.
        // (3.0f == flt_82004270 -- the seconds of continuous beaching after which the car is
        // force-reset; spelled at the one site the console compares it at.)
        bool IsBeached() const { return mfBeachedTime > 3.0f; }

    private:
        // =========================================================================================
        // ⭐⭐ THE RaceCarPhysics OWN-MEMBER BLOCK -- RECOVERED IN FULL 2026-08-03.
        //
        // WHAT CHANGED. This section used to hold FIVE members in an order that did not reproduce
        // the console's (mbPlayerCarInShowtime was declared FIRST while living at +0x140C, i.e.
        // AFTER mPropCollisionImpulseSum at +0x13F0), plus one member -- `mfSlamSteerEnvelope`
        // @+0x1430 -- whose ROLE was wrong (see the ⛔ below). It now holds all SIXTEEN members the
        // DecFIGS DWARF attests for this class, in DWARF DECLARATION ORDER, each at its X360 seat.
        //
        // WHY THIS IS THE BLOCKER THE VehicleManager BRIEF NAMES. BrnVehicleManager.h carries
        // `RaceCarVehicleRecord maRaceCarVehicles[8]` -- a byte-pinned 5216-byte stand-in for this
        // very class -- and cannot be swapped for the real type while the real type does not
        // DECLARE the fields the record names. TWO of the record's ten named fields live in this
        // block and are settled by it (+5120 -> mfTimeSinceTookDownPlayer, +5200 ->
        // mEntityCausingCrash); a third (+1808) is SimpleVehiclePhysics::mbCrashing. The other
        // seven are VehiclePhysics / SimpleVehiclePhysics members and are a separate wave. The
        // full fold-in map lives over RaceCarVehicleRecord in BrnVehicleManager.h.
        //
        // ⭐ WHY THE OFFSETS ARE MEANINGFUL ON x64 AT ALL. Every member below is a fixed-width
        // scalar or a 16-byte `alignas(16)` Vector3/EntityId -- no pointer, no vptr, no
        // ResourceHandle. So this block is WIDTH- AND ALIGNMENT-IDENTICAL on X360 and on host x64,
        // and every INTERNAL offset difference is exactly zero. The class as a whole is NOT
        // byte-pinned (the ~0x13F0 bytes of base state ahead of it are not reproduced as padding,
        // per project rule), so what _AssertOwnBlockLayout pins is each member's offset RELATIVE TO
        // mPropCollisionImpulseSum, written as `<X360 offset> - <X360 base>` with BOTH terms
        // literal. Never as `sizeof(T) - K`, which would make the assert self-fulfilling.
        //
        // ⭐⭐ THE INDEPENDENT CLOSURE -- this is what makes the block a derivation and not a guess.
        // The DWARF supplies the member ORDER; the X360 asm supplies the OFFSETS (table below);
        // neither knows about the other. Run them together and the last data byte lands at
        // +0x1454 (a bool), so sizeof rounds up to the 16-byte alignment the four Vector3s force:
        //       0x1455 -> 0x1460 == 5216
        // and 5216 is the asm-literal per-car stride in VehicleManager::Construct @0x8263B7C8
        // (`addi r29, r29, 0x1460`) -- a different function, in a different class, pinned by an
        // earlier wave that knew nothing about this member list. The two meet with ZERO SLACK: no
        // member can be added, dropped, widened or re-ordered inside this block without breaking
        // the stride. `_AssertOwnBlockLayout` pins that closure too.
        // ⚠️ ONE THING THE CLOSURE CANNOT SEE, measured by tamper test rather than assumed: a
        // SEVENTEENTH member of <= 11 bytes appended after mbDebugShowTargetAssist fits inside the
        // alignment pad and changes neither sizeof nor any offset, so no assert C++ can express
        // would catch it. What guards that case is that the DWARF list above is exhaustive and is
        // quoted verbatim -- a 17th member would have to be invented.
        //
        // PROVENANCE, member by member. X360 first-hand unless marked; PS3 = the DecFIGS internal
        // build, which is a second symbolled witness at a UNIFORM Δ = −16 in this region
        // (IsPlayerVehicleActuallyInShowtime reads 0x13FC there vs 0x140C here; IsUsingAftertouch
        // 0x13FD vs 0x140D; RaceCarPhysics::Construct's Z-lane insert 0x1060 vs 0x1070). PS3 is
        // used for IDENTITY ONLY -- no PS3 number is committed as an offset anywhere below.
        //
        //   +0x13F0  mPropCollisionImpulseSum   PS3 Construct @0x6EB404 `stvx v13,this,0x13E0`;
        //                                       X360 ApplyPropCollisionImpulseSum `addi r31,this,0x13F0`
        //   +0x1400  mfTimeSinceTookDownPlayer  X360 Update @0x826418E0..F8 `lfs 0x1400 ; fadds v1.x ;
        //                                       stfs 0x1400` -- a per-frame timer INTEGRATION
        //   +0x1404  mfSlamSteering             X360 Update (11 accesses); VehicleManager::
        //                                       CalculateSlamData @0x825C7678 `lfs f0,0x1404(r26)`
        //   +0x1408  mfBeachedTime              X360 Update @0x826419C8..D8 (`+= dt` only when slow AND
        //                                       not airborne AND wheels ungrounded AND NOT crashing);
        //                                       AddTractionPoint @0x825FFB0C reads it
        //   +0x140C  mbPlayerCarInShowtime      X360 IsPlayerVehicleActuallyInShowtime @0x827E42B0
        //   +0x140D  mbUsingAftertouch          X360 IsUsingAftertouch @0x825B8C88
        //   +0x140E  mu8StrengthStat            X360 PhysicalTrafficVehicle::OnChecked @0x8261E3F0
        //                                       `lbz r11,0x140E(r28)`; PS3 Prepare @0x736554 stores it
        //                                       from a parameter IDA names `lu8StrengthStat` (that name
        //                                       is IDA reading the PS3 build's own DWARF)
        //   +0x1410  mInitialCrashVel           X360 SetCrashing @0x825B8AD0 `stvx128 v0,r31,0x1410`
        //                                       loaded from this+0x50 (mLinearVelocity)
        //   +0x1420  mInitialCrashAngVel        X360 SetCrashing @0x825B8ACC `stvx128 v13,r31,0x1420`
        //                                       loaded from this+0x60 (angular velocity)
        //   +0x1430  mfCrashTimer               X360 SetCrashing @0x825B8AC8 `stfs flt_82001CC0,0x1430`
        //   +0x1434  mbAISlowMo                 X360 Update @0x82641698/9C/F4
        //   +0x1435  mbWroteIntoRWInSlowMo      PS3 Prepare @0x7365C4; PS3 VehicleManager::
        //                                       GetUpdatedVehicleBodies reads 0x1424 then writes 0x1425
        //   +0x1436  mbDeformedBeyondDrive...   X360 DeformableObject::UpdateDeformedBBox @0x825E0EBC
        //                                       `stb r11,0x1436(r10)` (IDA names the PS3 source register
        //                                       `lbDeformedPastLimit`); VehicleOutputInterface::
        //                                       UpdateRaceCarState @0x825ECC40 reads it
        //   +0x1440  mCrashNormal               X360 GetNormalCausingCrash @0x825B3978
        //                                       `lvx128 v0,r30,0x1440`
        //   +0x1450  mEntityCausingCrash        X360 VehicleManager::SetRaceCarCrashing @0x82635478
        //                                       `stw r26,0x1450(r30)`; ProcessContactSpies @0x82646DC0
        //   +0x1454  mbDebugShowTargetAssist    X360 UpdateTargetAssist @0x826202F4 `lbz r11,0x1454(r28)`
        //
        // ⛔ ONE COMMITTED ROLE WAS WRONG, and it is the exact shape this project keeps hitting:
        // ADDRESS RIGHT, MEANING WRONG. The member at +0x1430 was committed as `mfSlamSteerEnvelope`,
        // "a short-lived slam/steer envelope scalar Update also drives on the engine-only path". The
        // address is right and everything else about it is not. Re-pulled X360 Update @0x826415E8:
        //     0x82641624  lbz   r11, 0x710(r31)          <- mbCrashing, NOT an "engine-only" flag
        //     0x82641638  beq   -> not-crashing branch
        //     0x82641640  lfs   f0, 0x1430(r31)          <- crashing: timer += the REAL timestep (v2.x)
        //     0x82641664  stfs  f0, 0x1430(r31)
        //     0x8264168C  <compare the timer against unk_82FB83B0>   == KVF_AI_CRASH_PAUSE_TIME
        //     0x82641698  stb   r30(0), 0x1434(r31)      <- expired -> mbAISlowMo = false
        //     0x8264169C  lbz   r11, 0x1434(r31)         <- still in slow-mo ->
        //     0x826416D8  vmulfp128 v127, v0, v1                scale the SIM timestep by
        //                                                       1/unk_82FB8880 == KVF_AI_CRASH_SLOWMO_FACTOR
        //     0x826416F4  stb   r30(0), 0x1434(r31)      <- not crashing: mbAISlowMo = false
        //     0x826416FC  stfs  <flt_820037C8>, 0x1430(r31)      and the timer is re-seeded
        // i.e. +0x1430 is the AI CRASH TIMER that drives the AI crash slow-motion, which is precisely
        // the DWARF's `mfCrashTimer` (:408) + `mbAISlowMo` (:409), and precisely what the two
        // constants BrnPhysics::Vehicle::KVF_AI_CRASH_PAUSE_TIME / KVF_AI_CRASH_SLOWMO_FACTOR
        // (DWARF RaceCarPhysics.h:39/:40) exist for. It has nothing to do with slam steering.
        // =========================================================================================

        // @+0x13F0 (DWARF :393). The accumulated prop-collision impulse summed across the frame,
        // flushed by ApplyPropCollisionImpulseSum. Read/written as a single VMX register.
        Vector3 mPropCollisionImpulseSum;

        // @+0x1400 (DWARF :395). Time since this car last took the player down -- integrated by
        // Update every frame and reset to 0 by the takedown path (VehicleManager::SetRaceCarCrashing
        // zeroes it in the victim==player case). Read by HasRecentlyTakendownPlayer against
        // KF_POST_PLAYER_TD_INVULNERABILITY_TIME (DWARF RaceCarPhysics.h:34/:518/:532).
        // ⭐ This is the field BrnVehicleManager.h's RaceCarVehicleRecord carried as
        // `mfRecoveryTimer` at in-record +5120.
        f32 mfTimeSinceTookDownPlayer;

        // @+0x1404 (DWARF :396). The stick-driven extra-steer scalar Update maintains. Decays
        // 0.95/frame, deadzone 0.1, clamp +/-10; consumed by VehicleManager::CalculateSlamData.
        f32 mfSlamSteering;

        // @+0x1408 (DWARF :397). How long this car has been "beached" -- integrated by Update only
        // while the car is slow, is not considered airborne, has its wheels off the ground and is
        // not crashing; the whole point is the IsBeached() query (DWARF RaceCarPhysics.h:363).
        f32 mfBeachedTime;

        // @+0x140C (DWARF :399). Read by IsPlayerVehicleActuallyInShowtime / IsCrashingNormally.
        bool mbPlayerCarInShowtime;

        // @+0x140D (DWARF :400). Latched each frame by Update; read by IsUsingAftertouch.
        bool mbUsingAftertouch;

        // @+0x140E (DWARF :401). The car's "strength" stat byte, seeded by Prepare from its
        // parameter and read back by GetStrengthStat (DWARF :355) -- PhysicalTrafficVehicle::
        // OnChecked reads it directly off the race car to weight traffic interest.
        u8 mu8StrengthStat;

        // @+0x1410 / +0x1420 (DWARF :406 / :407). The linear and angular velocity snapshot taken the
        // instant a crash begins: SetCrashing copies this+0x50 and this+0x60 into them verbatim.
        Vector3 mInitialCrashVel;
        Vector3 mInitialCrashAngVel;

        // @+0x1430 (DWARF :408). The AI crash timer -- see the ⛔ block above. Re-seeded whenever the
        // car is not crashing, integrated by the REAL (unscaled) timestep while it is, and compared
        // against KVF_AI_CRASH_PAUSE_TIME to end the AI crash slow-motion.
        f32 mfCrashTimer;

        // @+0x1434 (DWARF :409). While set, Update scales the simulation timestep by
        // KVF_AI_CRASH_SLOWMO_FACTOR (the AI crash slow-motion). Read by IsInAICrashSlowMo (:292).
        bool mbAISlowMo;

        // @+0x1435 (DWARF :410). Latched by VehicleManager::GetUpdatedVehicleBodies once a slow-mo
        // frame's state has been written back into RenderWare, so it is written at most once per
        // slow-mo episode. Accessors SetWrittenIntoRWInSlowMo / GetWrittenIntoRWInSlowMo (:296/:300).
        bool mbWroteIntoRWInSlowMo;

        // @+0x1436 (DWARF :412). Set by the deformation pass when this car's crush has passed the
        // limit past which it can still be driven; read back by VehicleOutputInterface::
        // UpdateRaceCarState. Accessors Set/GetDeformedBeyondDriveTimeLimitsInCrash (:347/:351).
        bool mbDeformedBeyondDriveTimeLimitsInCrash;

        // @+0x1440 (DWARF :414). The collision normal that caused the current crash, snapshotted
        // when the crash begins; read out by GetNormalCausingCrash (which asserts mbCrashing first).
        Vector3 mCrashNormal;

        // @+0x1450 (DWARF :415). The entity that caused the current crash. Written by
        // VehicleManager::SetRaceCarCrashing together with mCrashNormal (the pair is the console's
        // SetCrashEntityIdAndNormal, DWARF :590) and read back by GetEntityCausingCrash (:331).
        // ⭐ This is the field BrnVehicleManager.h's RaceCarVehicleRecord carried as
        // `mStampedEntityId` at in-record +5200.
        EntityId mEntityCausingCrash;

        // @+0x1454 (DWARF :417). Debug-draw gate consulted by UpdateTargetAssist. LAST DATA BYTE OF
        // THE CLASS -- the 11 bytes after it are the alignment pad that closes sizeof at 5216.
        bool mbDebugShowTargetAssist;
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
        s16   muBounceChainCount;      // +0x06  word_82FB8486 (signed: asm reads via extsh; DWARF int16_t)
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

        // ---- +0x4C : the last 4 bytes of the scalar block before the target-position array ----
        u8    maReserved4C[0x50 - 0x4C];   // +0x4C..+0x50

        // ⭐ NAMED 2026-08-11 (prepare-chain wave). +0x50..+0xD0 == unk_82FB84D0, the per-candidate
        // target-assist POSITION array, parallel to maTargetIds below (8 slots, 16-byte stride).
        // It used to be buried in a `maReserved4C[0x84]` blob with a note saying UpdateTargetAssist
        // reaches it "via a raw offset rather than a named member". It is not a blob: VehicleManager
        // ::UpdateDrivers @0x82642C68 (⚠ corrected 2026-08-11: this used to cite @0x82642E40, which
        // is the address of the `addi` INSIDE the body that forms this very `+ 0x50`, not the
        // function's entry point) passes `lbBounceBoosting + 0x50` as the FIRST argument of
        // VehicleDriverInputInterface::GetTargetAssistParams, whose committed signature types that
        // parameter `Vector3* lpOutTargetAssistPositions` -- and the next two arguments are
        // +0xD0 == maTargetIds and +0xF0 == &miNumTargets, both already named here. One call site
        // types all three seats at once. 8 * 16 == 0x80 closes the run exactly on maTargetIds.
        Vector3 maTargetPositions[8];      // +0x50..+0xD0  (unk_82FB84D0)

        // ---- +0xD0..+0x110 : target-assist candidate list (filled by a game-side TU) ----
        // ⚠️ RE-TYPED 2026-08-11 (prepare-chain wave), s32 -> EntityId (both 4 bytes, no layout
        // change): VehicleManager::UpdateDrivers passes this exact seat as the SECOND argument of
        // VehicleDriverInputInterface::GetTargetAssistParams, whose committed signature types it
        // `EntityId* lpOutTargetAssistIDs`. They are entity ids, not bare ints.
        EntityId maTargetIds[8];       // +0xD0  dword_82FB8550 (per-candidate id, 8 slots)
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
    // never-called _AssertPlayerParamsLayout(). ⭐ The per-candidate target POSITIONS at +0x50
    // (unk_82FB84D0, 16B stride) are NO LONGER inside the reserved blob -- they are the named
    // maTargetPositions[8] above, typed by VehicleManager::UpdateDrivers' GetTargetAssistParams
    // call site. UpdateTargetAssist's raw-offset read of the same run should be re-pointed at the
    // named array when that body is next touched.

    // The single process-wide showtime singleton (X360 msPlayerParams; base = lbBounceBoosting).
    extern PlayerParameters msPlayerParams;
}
}
