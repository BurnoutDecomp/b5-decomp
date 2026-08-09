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
// RECONCILIATION (C05 road-noise): a prior surface-grip/friction group (C05) had added a standalone
// `f32 mfRoadNoise` immediately after mRoadContact as an admitted flagged placeholder (its note says
// the real destination lanes "overlap the suspension/integration blobs the full Wheel TU owns").
// That standalone member would shift every real member off its console offset. With the full DWARF
// layout now known, the road-noise accumulator is reconciled onto a real register lane (the
// force-variable register, where the UpdateRoadNoise stvx128 lands) and the C05 Add/GetRoadNoise
// accessor API is preserved verbatim -- no caller existed outside this header, so no break. The
// 0xE0 layout is now correct.
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus, Vector4, VecFloat, CollisionTag
#include "types.hpp"          // f32, s8, u8, u16

namespace BrnPhysics
{
namespace Vehicle
{
    // ----- ADDITIVE GROW (C04 group): a MINIMAL OWNING SLICE of the per-car base attribs block
    //       the attrib-driven Prepare{Front,Rear}Tire scatter through. The full
    //       physicsvehiclebaseattribs (the DWARF arg type of PrepareFrontTire/PrepareRearTire) is
    //       owned by the VehicleAttribs TU and has no committed header; the two attrib-driven
    //       scatter routines only consume it as a source-of-scalars through a rodata permute
    //       table, so it is carried opaquely here. When VehicleAttribs gets a real header this
    //       slice should be REPLACED by an include of the committed type.
    struct PhysicsVehicleBaseAttribs
    {
        // The block is read only as a source of packed scalars by the tire-scatter; its full field
        // map belongs to the VehicleAttribs TU. FLAG (offsets NOT pinned): the X360 scatter does
        // NOT lvx128 this block at +0x00/+0x10 -- it dereferences a sub-pointer (lwz +4) then reads
        // SCALAR floats deep inside it (lfs +0xE0/+0xE4/+0xE8/+0xEC ...). These two registers are a
        // MINIMAL OPAQUE placeholder so the inert scatter routines have a typed source argument;
        // the lane offsets here are provisional, not console-pinned, and MUST be reconciled when the
        // real physicsvehiclebaseattribs type lands. Inert today (the permute table is un-homed).
        Vector4 mvTireScalarsLong;   // provisional placeholder lane (offset NOT pinned)
        Vector4 mvTireScalarsLat;    // provisional placeholder lane (offset NOT pinned)
    };

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
            // three curves + the packed register via the rodata permute table unk_8327F140. They
            // differ only in their source numbers, not logic. The attrib-driven pair take the
            // per-car base-attribs block; the Default/AI/DonutAI presets source un-homed .rdata
            // tuning constants. FLAG (rodata): the permute table + preset constants are un-homed
            // .rdata absent from the exports -> faithful-but-inert (the scatter shape is exact, the
            // laid-out numbers stay 0). NEVER fabricated.
            void PrepareDefaultFrontTire();
            void PrepareDefaultRearTire();
            void PrepareFrontTire(const PhysicsVehicleBaseAttribs& lrAttribs);
            void PrepareRearTire(const PhysicsVehicleBaseAttribs& lrAttribs);
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

        // PRE-EXISTING accessor -- UNTOUCHED. Returns by const-ref (the committed RoadContact-only
        // consumers depend on this signature; the DWARF spells it by-value but that is not changed
        // here to avoid breaking the committed callers).
        const RoadContact& GetRoadContact() const { return mRoadContact; }

        // C05 road-noise API (PRESERVED). Reconciled onto a real register lane: UpdateRoadNoise's
        // stvx128 lands in the wheel's leading SIMD region; the force-variable register's w lane is
        // the flagged home so the standalone mfRoadNoise no longer shifts the layout. No external
        // caller existed. FLAG: exact lane within the SIMD region is partial.
        void AddRoadNoise(f32 lfNoise) { mForceVariables.w += lfNoise; }
        f32  GetRoadNoise() const { return mForceVariables.w; }

        // ----- Members: RoadContact is PRE-EXISTING (leading member). The rest is ADDITIVE GROW,
        //       pinned BY NAME + SEQUENCE per the DWARF (stride 0xE0). -----

        RoadContact mRoadContact;                       // +0x00

        // Five 16-byte SIMD "variable" registers (console VectorIntrinsic blobs). The PC type
        // vocabulary has no VectorIntrinsic union; each is one 16-byte 4-lane register (Vector4),
        // matching the console layout/offset exactly.
        Vector4 mIntegrationVariables;                  // +0x30
        Vector4 mSlipVariables;                         // +0x40
        Vector4 mForceVariables;                        // +0x50  (w lane: C05 road-noise accumulator, flagged)
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
