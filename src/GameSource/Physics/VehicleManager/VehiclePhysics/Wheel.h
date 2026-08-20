#pragma once

// BrnPhysics::Vehicle::Wheel -- reconstruction.
//
// ORIGINALLY this header reconstructed ONLY the nested RoadContact struct (which WheelLite /
// RaceCarState embed by value). The "C04_wheels_tire_gripcurve" group GROWS it into the full
// Wheel layout: the five 16-byte SIMD variable blobs, the position/velocity/direction vectors,
// the tire-attribs pointer + the four trailing flag bytes, AND the two nested tuning types
// TireGripCurve / TireAttribs. The member NAMES + ORDER are authoritative from
// references/DecFIGS/dwarfdump/GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h; the
// absolute per-wheel stride is 0xE0 (offsets ASM-confirmed against the bodied funcs of this group).
//
// The pre-existing RoadContact + GetRoadContact() are UNTOUCHED (no reorder/retype/redefine);
// everything else is ADDITIVE. mRoadContact remains the leading member so the RoadContact-only
// consumers that already exist keep their offsets.
//
// ROAD-NOISE DESTINATION (Breaker UpdateRoadNoise @0x825F6AE0..0x825F6B84): the update loads
// mRoadContact.mNormal at wheel+0x10 and mRoadContact.mPosition at wheel+0x00, computes
// position + normal * noise, and stores the complete vector back to wheel+0x00. DecFIGS names the
// corresponding header inline `SetRoadContactPosition(Vector3)` (Wheel.h:351). Road noise is thus
// a stochastic displacement of the contact point, not a separate accumulator or a force-register
// lane. The full Wheel layout remains the DWARF/asm-confirmed 0xE0 bytes.
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, Vector4, VecFloat, CollisionTag
#include "types.hpp"          // f32, s8, u8, u16

// Forward decl: the generated AttribSys base-attribs wrapper (full type in
// GameSource/AttribSys/Generated/classes/physicsvehiclebaseattribs.h).
namespace Attrib { namespace Gen { class physicsvehiclebaseattribs; } }

namespace BrnPhysics
{
namespace Vehicle
{
    // ⭐ 2026-08-09 (attribs-setup wave): the `PhysicsVehicleBaseAttribs` MINIMAL OWNING SLICE
    // that stood here is RETIRED, exactly per its own contract ("MUST be reconciled when the
    // real physicsvehiclebaseattribs type lands") -- the real generated wrapper is committed at
    // GameSource/AttribSys/Generated/classes/physicsvehiclebaseattribs.h. The tire scatters take
    // it by const-ref (fwd-declared; the X360 reads the record via the wrapper's data pointer,
    // `lwz +4`). ⭐ 2026-08-09 (attribs-data wave): the scatters are REAL -- the permute table
    // 0x8327F140 is homed (static-init writer bank @0x82C74000) and all eight bodies are
    // image-emulated; see the Wheel.cpp banner.
    // Forward decl only -- the full generated header is pulled by the TUs that construct one.

    // Wheel: full layout. The RoadContact (and its accessor) are the pre-existing committed slice;
    // the rest is ADDITIVE GROW (C04 group), pinned BY NAME + SEQUENCE per the DWARF (stride 0xE0).
    struct Wheel
    {
        // DWARF Wheel.h:56 -- the inertia mode SetInertiaType selects (driven / locked while braked /
        // burnout). Reproduced for the shared vocabulary.
        enum EWheelInertiaType
        {
            eWheelInertiaTypeDriven         = 0,
            eWheelInertiaTypeLocked         = 1,
            eWheelInertiaTypeDriven_Burnout = 2,
        };

        // One wheel/road line-test contact result (position, surface normal, distance,
        // collision tag, on/near-ground flags). Embedded by value in WheelLite. PRE-EXISTING --
        // do not reorder/retype.
        struct RoadContact
        {
            Vector3      mPosition;
            Vector3      mNormal;
            f32          mfLineDistanceToRoad;
            CollisionTag mCollisionTag;
            bool         mbIsOnGround;
            bool         mbWasOnGroundLastUpdate;
            bool         mbIsCloseToGround;
            bool         mbLineTestIsValid;

            // Owned by the Wheel TU -- declare only (no body) to avoid an ODR clash.
            void operator=(const RoadContact&);
        };

        // ----- ADDITIVE GROW (C04 group): the tire grip-curve model (DWARF Wheel.h:87 / :152) -----

        // One Pacejka-like slip->coefficient curve. The four lanes of maGripVariables are
        // {peakSlipRatio (g0), floorSlipRatio (g1), peakCoefficient (g2), fallCoefficient (g3)}
        // (confirmed by the Get{Peak,Floor}SlipRatio / Get{Peak,Fall}Coefficient accessors).
        struct TireGripCurve
        {
            // @0x825B82B0: the slip->coefficient evaluator. Split slip S into |S| and sign(S),
            // evaluate a sign-symmetric rise-then-fall piecewise curve delimited by peakSlipRatio/
            // floorSlipRatio (ramp toward peakCoefficient, fall toward fallCoefficient, plateau
            // beyond), and re-apply sign(S). Bodied in Wheel.cpp.
            VecFloat GetCoefficient(VecFloat lvfSlip) const;

            // Declared-only accessors (owned by their own future TUs; no body here).
            VecFloat GetPeakSlipRatio() const;
            VecFloat GetFloorSlipRatio() const;
            VecFloat GetPeakCoefficient() const;
            VecFloat GetFallCoefficient() const;
            void     Construct();
            void     Prepare(f32 lfPeakSlipRatio, f32 lfFloorSlipRatio,
                             f32 lfPeakCoefficient, f32 lfFallCoefficient);

            // +0x00 (DWARF Wheel.h:148). {peakSlipRatio, floorSlipRatio, peakCoefficient, fallCoefficient}.
            Vector4 maGripVariables;
        };

        // The full per-tire tuning: three grip curves (long / lat / dedicated drift-lat) + a packed
        // {staticFrictionCo, dynamicFrictionCo, adhesiveLimit, longForceBias} register. DWARF
        // Wheel.h:152; offsets confirmed by the eight Prepare*Tire scatter routines (curve0 @+0x00,
        // curve1 @+0x10, curve2 @+0x20, packed @+0x30 -- the stvx128 targets r3 / r3+16 / r3+32 / r3+48).
        struct TireAttribs
        {
            // ----- C04 group: the eight tire-preset scatter routines (bodies in Wheel.cpp) -----
            // Each is a pure VMX permute-scatter that lays a handful of scalar inputs across the
            // three curves + the packed register via the lane-insert permute table @0x8327F140.
            // They differ only in their source numbers, not logic. The attrib-driven pair take
            // the per-car base-attribs record; the Default/AI/DonutAI presets lay out .rdata
            // tuning constants. ⭐ 2026-08-09 (attribs-data wave): table homed + all eight bodies
            // REAL (image-emulated, values exact -- see the Wheel.cpp banner). ⚠️ The DonutAI
            // pair deliberately PRESERVES maPackedVariables.w (15 vperms, not 16).
            void PrepareDefaultFrontTire();
            void PrepareDefaultRearTire();
            void PrepareFrontTire(const Attrib::Gen::physicsvehiclebaseattribs& lrAttribs);
            void PrepareRearTire(const Attrib::Gen::physicsvehiclebaseattribs& lrAttribs);
            void PrepareFrontTireForAI();
            void PrepareRearTireForAI();
            void PrepareFrontTireForDonutAI();
            void PrepareRearTireForDonutAI();

            // Declared-only accessors (the curve refs are trivial and inlined; the scalar getters
            // are owned elsewhere -- declare only).
            const TireGripCurve& GetLongGripCurve() const     { return mLongGripCurve; }
            const TireGripCurve& GetLatGripCurve() const      { return mLatGripCurve; }
            const TireGripCurve& GetDriftLatGripCurve() const { return mDriftLatGripCurve; }
            VecFloat GetStaticFrictionCo() const;
            VecFloat GetDynamicFrictionCo() const;
            VecFloat GetAdhesiveLimit() const;
            VecFloat GetLongForceBias() const;
            void Construct();

            TireGripCurve mLongGripCurve;      // +0x00
            TireGripCurve mLatGripCurve;       // +0x10
            TireGripCurve mDriftLatGripCurve;  // +0x20
            Vector4       maPackedVariables;   // +0x30 {staticFrictionCo,dynamicFrictionCo,adhesiveLimit,longForceBias}
        };

        // ----- ADDITIVE GROW (C04 group): the bodied Wheel instance methods -----

        // @0x825D6E88: zero the running state (variable blobs / position / velocity / direction
        // registers) and clear the tire-attribs pointer + count/traction/state flags. Bodied in
        // Wheel.cpp.
        void Clear();

        // @0x825D7190: reset the wheel to a road-contact position -- Clear-like zeroing plus a
        // normalize of the supplied vector (vrsqrtefp+Newton) scaled by an un-homed rodata factor,
        // stored into the integration register, then SetPosition(). Bodied in Wheel.cpp.
        // FLAG (rodata): the scale factor unk_82FB8AB0 is un-homed -> flagged-inert.
        void Reset(Vector3 lvPosition);

        // @0x825BDA10: validate + commit the wheel position (the X360 asserts the position is
        // finite, debug-only, then stores the supplied vector at +0x80 mPosition). Bodied in
        // Wheel.cpp. DWARF Wheel.h:372 spells it SetPosition(Vector3).
        void SetPosition(Vector3 lNewPos);

        // @0x825D6C08: stamp this frame's road-contact result (on-ground / close-to-ground flags,
        // position, normal, two collision-tag halfwords, line distance) and derive mbHasTraction
        // from the contact normal's up component vs a 0.5 threshold. Bodied in Wheel.cpp.
        void SetRoadContact(bool lbIsOnGround, bool lbIsCloseToGround,
                            Vector3 lPosition, Vector3 lNormal,
                            u16 lu16TagHi, u16 lu16TagLo, f32 lfLineDistanceToRoad);

        // @0x825FEBE8: prepare the wheel from a streamed position + per-wheel scalars + tire attribs
        // (Clear, set mpTireAttribs, scatter the scalars into their SIMD lanes, then SetPosition).
        // Bodied in Wheel.cpp.
        // ⭐ LANE MAP CORRECTED against the raw asm (seat wave 2026-08-05) -- the old "inferred from
        // the SwitchAttribs sibling" scatter was wrong on every lane. Proven map (asm 0x825FEC48..
        // 0x825FED10, caller SimpleVehiclePhysics::SetAttributes asm 0x826027F4..0x82602840):
        //   f1 -> mSlipVariables.w                 = THE WHEEL RADIUS (the analytic seat's input;
        //                                            caller passes lpafWheelRadii[i])
        //   f2 -> mIntegrationVariables.w           (caller passes the .data scalar flt_82FB8BB0)
        //   f3 -> mSuspensionAndInertiaVariables.y = lStreamedPosition.y + f3  (travel up bound)
        //   f4 -> mSuspensionAndInertiaVariables.x = lStreamedPosition.y - f4  (travel down bound)
        //   v1 -> mStreamedPositionPlusTwistAmount.xyz (the w lane is PRESERVED, not written)
        bool Prepare(Vector3 lStreamedPosition, f32 lfWheelRadius, f32 lfIntegrationSeed,
                     f32 lfSuspensionTravelUp, f32 lfSuspensionTravelDown,
                     const TireAttribs* lpTireAttribs);

        // @0x825D6D38: re-point the tire attribs + re-derive the suspension lanes WITHOUT
        // clearing the running state (the in-place sibling of Prepare). Bodied in Wheel.cpp.
        // ⭐⭐ RE-VERIFIED against its own asm 2026-08-09 (attribs-setup wave), exactly as the
        // old FLAG here demanded -- the old parameter roles WERE the disproven inference. The
        // real scatter mirrors Prepare's proven map (f1 = radius -> mSlipVariables.w,
        // f2 = integration seed -> mIntegrationVariables.w, f3/f4 = travel up/down ->
        // mSuspensionAndInertiaVariables .y/.x), and v1 is a POSITION DELTA subtracted from
        // the running +0x80/+0x90 registers, not an absolute position (the sole caller,
        // SimpleVehiclePhysics::SwitchAttribs, passes (oldCOM - newCOM) + dHeight * yhat).
        void SwitchAttribs(Vector3 lPositionDelta, f32 lfWheelRadius, f32 lfIntegrationSeed,
                           f32 lfSuspensionTravelUp, f32 lfSuspensionTravelDown,
                           const TireAttribs* lpTireAttribs);

        // @0x825D7008: integrate the accumulated wheel torque into the wheel's angular velocity,
        // then resolve it against the brakes (the per-wheel spin/brake/lock-up step UpdateWheels
        // runs per wheel). Bodied in Wheel.cpp.
        //
        // ⭐⭐ SIGNATURE CONFORMED 2026-08-07 (wheel-cluster wave). The previous 3-arg
        // (maxAngVel, bool, bool) form was a SLICE ARTIFACT: the callee asm CONSUMES v1..v5
        // (`vmulfp128 v31,v9,v1` dt; `vmulfp128 v7,v3,v4` brakeFactor*capacityScale;
        // `vmulfp128 v5,v5,v3` decelScale*brakeFactor; the clamp vs v2), and the DWARF spells
        // Wheel.h:332 `UpdateVelocity(VecFloat, VecFloat, VecFloat, VecFloat, VecFloat, bool,
        // bool)`. UpdateWheels' register map names the lanes: v1 = dt, v2 = maxAngVel (engine
        // rev limit, negative in reverse), v3 = the axle brake factor, v4 = unk_82FB9E10 ==
        // 100.0 (the brake-capacity scale), v5 = unk_82FB9380 == 1000.0 (the brake-decel
        // scale), r4 = (gas < 0.1), r5 = (reversing || gear 0).
        // ⭐ The old FLAG called unk_82FB9CF0/8BC0/8B40/8327F240 "un-homed / absent from the
        // exports": FALSE (another absence banner down). All four are static-init'd BSS with
        // rdata-attested writers -- 9000.0 / 100.0 / 500.0 / the shared FALSE|TRUE vsel mask
        // pair (see Wheel.cpp).
        void UpdateVelocity(VecFloat lvfTimeStep, VecFloat lvfMaxAngularVelocity,
                            VecFloat lvfBrakeFactor, VecFloat lvfBrakeCapacityScale,
                            VecFloat lvfBrakeDecelScale, bool lbGasReleased, bool lbInReverse);

        // @0x825D6F68: the wheel-SPIN reaction to the tyre's longitudinal contact force -- the
        // second half of the tyre model's torque couple. VehiclePhysics::HandleWheelPairFriction
        // calls it once per wheel with
        //     lvfWheelTorque   = -longitudinalForce * radius   (the reaction on the wheel)
        //     lvfRoadLongSpeed =  the road-relative longitudinal contact speed
        //     lvfTimeStep      =  dt
        // and it integrates the torque into mIntegrationVariables.x (the wheel's angular velocity),
        // never overshooting the free-rolling speed (roadLongSpeed / radius), then zeroes a locked
        // wheel. Bodied in Wheel.cpp.
        //
        // ⭐ 2026-08-12 (tyre-force wave). Its own .ida-exports JSON is a HOLE -- the address and
        // the name came out of HandleWheelPairFriction's `xrefs_from`, exactly the documented
        // hole-recovery route -- and the BODY did not need the hole closed at all: the X360
        // compiler INLINED this call for the pair's first wheel (0x825FBE6C..0x825FBF00) and only
        // emitted the real `bl` for the second, so a store-for-store copy of the callee sits inside
        // the caller this wave was already reading. Recovered from that inlined copy; the shape is
        // cross-witnessed by the BPR twin sub_B90DB0's call site (torque, longSpeed, dt).
        void ApplyFrictionReaction(VecFloat lvfWheelTorque, VecFloat lvfRoadLongSpeed,
                                   VecFloat lvfTimeStep);

        // PRE-EXISTING accessor -- UNTOUCHED. Returns by const-ref (the committed RoadContact-only
        // consumers depend on this signature; the DWARF spells it by-value but that is not changed
        // here to avoid breaking the committed callers).
        const RoadContact& GetRoadContact() const { return mRoadContact; }

        // ⭐ ADDED 2026-08-19 (wave Q6 / the jointed lean+tilt prop response). The wheel's
        // road-relative LONGITUDINAL contact speed -- lane .x of mSpeedAndMassOnWheelVariables,
        // broadcast into all four lanes.
        //
        // SIGNATURE IS DWARF-AUTHORITATIVE: references/DecFIGS/dwarfdump/GameSource/Physics/
        // VehicleManager/VehiclePhysics/Wheel.h:457 reads `VecFloat GetRoadLongSpeed() const;`
        // (source line Wheel.h:557), directly above its GetRoadLatSpeed sibling at :460/:561.
        //
        // WHICH LANE IS NOT INFERRED -- three independent attestations agree:
        //   1. the X360 consumer PropManager::HandleContactWithLeanProp @0x8260FEA8 does
        //      `addi r11, r26, 0x360` / `lvx128 v0, r0, r11` / `vspltw v12, v0, 0`, and
        //      0x360 == maWheels(+0x130) + 2*0xE0 + 0x70 == maWheels[eRearLeftWheel]
        //      .mSpeedAndMassOnWheelVariables; the `vspltw ...,0` is lane .x broadcast.
        //      HandleContactWithTiltProp repeats it verbatim at 0x82610A48/0x82610A8C.
        //   2. this tree's own WRITER, VehiclePhysics.cpp:735-737, seats
        //      `mSpeedAndMassOnWheelVariables.x = lrIn.mfLongSpeed`.
        //   3. this tree's own READER, BrnVehicleOutputInterface_UpdateRaceCarState.cpp:356,
        //      publishes `mfRoadLongSpeed = lrWheel.mSpeedAndMassOnWheelVariables.x`.
        //
        // Header-inline like the console (Wheel.h:557 is an SDK-style accessor the X360
        // compiler folds -- it has no address of its own). Spelled as an explicit four-lane
        // brace init rather than vpu::Splat so this widely-included header pulls in no new
        // dependency; VecFloat/Vector4 already arrive through BrnCommonTypes.h above.
        VecFloat GetRoadLongSpeed() const
        {
            const f32 lfRoadLongSpeed = mSpeedAndMassOnWheelVariables.x;
            return VecFloat{ lfRoadLongSpeed, lfRoadLongSpeed, lfRoadLongSpeed, lfRoadLongSpeed };
        }

        // DecFIGS Wheel.h:351. Breaker UpdateRoadNoise inlines this setter: its final stvx128 at
        // 0x825F6B84 targets wheel+0x00 after forming mPosition + mNormal * noise.
        void SetRoadContactPosition(Vector3 lPosition) { mRoadContact.mPosition = lPosition; }

        // ----- Members: RoadContact is PRE-EXISTING (leading member). The rest is ADDITIVE GROW,
        //       pinned BY NAME + SEQUENCE per the DWARF (stride 0xE0). -----

        RoadContact mRoadContact;                       // +0x00

        // Five 16-byte SIMD "variable" registers (console VectorIntrinsic blobs). The PC type
        // vocabulary has no VectorIntrinsic union; each is one 16-byte 4-lane register (Vector4),
        // matching the console layout/offset exactly.
        Vector4 mIntegrationVariables;                  // +0x30
        Vector4 mSlipVariables;                         // +0x40
        Vector4 mForceVariables;                        // +0x50
        Vector4 mSuspensionAndInertiaVariables;         // +0x60
        Vector4 mSpeedAndMassOnWheelVariables;          // +0x70

        Vector3     mPosition;                           // +0x80
        Vector3Plus mStreamedPositionPlusTwistAmount;    // +0x90 (xyz = streamed pos, w = twist amount)
        Vector3     mBodyPointVelocity;                  // +0xA0
        Vector3     mWheelLongDirection;                 // +0xB0
        Vector3     mWheelLatDirection;                  // +0xC0

        const TireAttribs* mpTireAttribs;                // +0xD0
        s8   mi8NumContacts;                             // +0xD4
        bool mbBrokenAdhesiveLimit;                      // +0xD5
        bool mbHasTraction;                              // +0xD6
        u8   mu8State;                                   // +0xD7
    };
}
}
