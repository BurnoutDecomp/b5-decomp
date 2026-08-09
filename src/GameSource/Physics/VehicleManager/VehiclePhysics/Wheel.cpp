#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Wheel::Prepare's lpTireAttribs gate, Wheel.cpp:478)

#include <cmath>   // std::fabs, std::sqrt

// BrnPhysics::Vehicle::Wheel -- the C04_wheels_tire_gripcurve group's out-of-line bodies:
// the per-wheel lifecycle (Clear/Reset/SetPosition/SetRoadContact/Prepare/SwitchAttribs/
// UpdateVelocity), the tire grip-curve evaluator (TireGripCurve::GetCoefficient), and the eight
// TireAttribs::Prepare*Tire scatter routines. Reconstructed from the X360 ARTIST pseudocode+asm
// (addresses on each function) against the verified findings doc (docs/RaceCarPhysics_findings.md
// section 6) and the DecFIGS DWARF member layout. The heavy CgsDev::Assert finite-value machinery
// is ELIDED (debug-build guards, no effect on output) per the project assert convention.

namespace BrnPhysics
{
namespace Vehicle
{
    // ===========================================================================================
    //  Wheel::TireGripCurve::GetCoefficient   @0x825B82B0
    // ===========================================================================================
    // The literal slip->coefficient function. Degenerate in Hex-Rays but legible in asm: it splits
    // the input slip S into |S| and sign(S), evaluates a sign-symmetric, rise-then-fall
    // (Pacejka-like) piecewise curve over three regions delimited by peakSlipRatio (g0) and
    // floorSlipRatio (g1) -- ramp up toward peakCoefficient (g2), then fall from peak toward
    // fallCoefficient (g3), plateau beyond -- and re-applies sign(S). The asm uses vrefp+Newton
    // reciprocals for the segment slopes and vspltisw/vcfsx for the 1.0/0.0 immediates (NO rodata
    // constants in the evaluator).
    //
    // The exact per-region polynomial (the asm's vmaddfp cascade with the rational segment slopes)
    // is reproduced here as the piecewise rise/fall the asm computes:
    //   * sign s = (S >= 0) ? +1 : -1 ; a = |S|
    //   * region 1 (a <= g0):  rise from 0 toward g2 with slope g2/g0           -> coeff = g2 * a/g0
    //   * region 2 (g0 < a <= g1): fall from g2 toward g3 across [g0,g1]
    //                              -> coeff = g2 + (g3 - g2) * (a - g0)/(g1 - g0)
    //   * region 3 (a > g1):   plateau at g3
    //   * result = s * coeff
    // FLAG: the asm's two reciprocal slopes (1/g0 and 1/(g1-g0)) are formed via vrefp + one Newton
    // step; here written as exact divides (algebraically identical; the Newton refinement only
    // restores full precision to the hardware estimate). The region blend matches the vcmpgefp/vsel
    // selection chain. FIDELITY: PARTIAL on region 2. The rise (region 1 = g2*|S|/g0), the plateau
    // (region 3 = g3) and the sign(S) re-apply ARE store-exact; but the EXACT region-2 fall is NOT
    // pinned -- the asm's final vsubfp/vmulfp/vmaddfp triple (@0x825B838C..) multiplies (g3-g2) by a
    // grip lane and adds the t term with an operand grouping that differs from the clean
    // coeff = g2 + (g3-g2)*t lerp written below. The lerp is a faithful APPROXIMATION of the fall's
    // endpoints (g2 at g0, g3 at g1) but the interior curvature is not store-verified. FLAG.
    VecFloat Wheel::TireGripCurve::GetCoefficient(VecFloat lvfSlip) const
    {
        const f32 lfS  = lvfSlip.x;                 // the slip is splatted across all lanes on console
        const f32 lfG0 = maGripVariables.x;         // peakSlipRatio
        const f32 lfG1 = maGripVariables.y;         // floorSlipRatio
        const f32 lfG2 = maGripVariables.z;         // peakCoefficient
        const f32 lfG3 = maGripVariables.w;         // fallCoefficient

        const f32 lfSign = (lfS >= 0.0f) ? 1.0f : -1.0f;   // vcmpgefp vs 0 -> +1 / -1 select
        const f32 lfAbs  = std::fabs(lfS);

        f32 lfCoeff;
        if (lfAbs <= lfG0)
        {
            // rise toward the peak. Slope = g2/g0 (vrefp(g0)+Newton then *g2).
            lfCoeff = (lfG0 != 0.0f) ? (lfG2 * (lfAbs / lfG0)) : lfG2;
        }
        else if (lfAbs <= lfG1)
        {
            // fall from peak toward the floor across [g0, g1].
            const f32 lfSpan = lfG1 - lfG0;
            const f32 lfT    = (lfSpan != 0.0f) ? ((lfAbs - lfG0) / lfSpan) : 0.0f;
            lfCoeff = lfG2 + (lfG3 - lfG2) * lfT;
        }
        else
        {
            // plateau beyond the floor.
            lfCoeff = lfG3;
        }

        const f32 lfResult = lfSign * lfCoeff;
        return VecFloat{ lfResult, lfResult, lfResult, lfResult };
    }

    // ===========================================================================================
    //  Wheel::Clear   @0x825D6E88
    // ===========================================================================================
    // Zero the running-state registers (integration / slip / force / suspension+inertia, plus the
    // position / streamed-position / body-velocity / long+lat direction vectors) and clear the
    // tire-attribs pointer + the count / has-traction / state flags. NOTE the asm deliberately does
    // NOT touch mSpeedAndMassOnWheelVariables (+0x70) nor mbBrokenAdhesiveLimit (+0xD5).
    void Wheel::Clear()
    {
        mIntegrationVariables.SetZero();             // +0x30 (rlimi-zeroed all lanes)
        mSlipVariables.SetZero();                    // +0x40
        mForceVariables.SetZero();                   // +0x50
        mSuspensionAndInertiaVariables.SetZero();    // +0x60

        mPosition.SetZero();                         // +0x80 (stvx128 v0=0)
        mStreamedPositionPlusTwistAmount.SetZero();  // +0x90
        mBodyPointVelocity.SetZero();                // +0xA0
        mWheelLongDirection.SetZero();               // +0xB0 (r10=176)
        mWheelLatDirection.SetZero();                // +0xC0 (r9=192)

        mpTireAttribs  = nullptr;                    // *(result+208)=0
        mi8NumContacts = 0;                          // *(result+212)=0
        mbHasTraction  = false;                      // *(result+214)=0
        mu8State       = 0;                          // *(result+215)=0
    }

    // ===========================================================================================
    //  Wheel::SetPosition   @0x825BDA10
    // ===========================================================================================
    // Validate the new position is finite (the per-lane vcmpeqfp self-compare assert -- elided as a
    // debug-build guard) then commit it to mPosition (+0x80).
    void Wheel::SetPosition(Vector3 lNewPos)
    {
        // CgsDev::Assert( RwMathVPU::IsValid( lNewPos ) ) -- elided (debug-only finite check).
        mPosition = lNewPos;                         // stvx128 v127(arg) -> this+0x80
    }

    // ===========================================================================================
    //  Wheel::SetRoadContact   @0x825D6C08
    // ===========================================================================================
    // Stamp this frame's road-contact result and derive traction. Args (de-fastcall'd): the two
    // bool flags, the contact position + normal vectors, two collision-tag halfwords, and the line
    // distance. The asm:
    //   this+0x28 (40) = lbIsOnGround ; this+0x2A (42) = lbIsCloseToGround ; this+0x2B (43) = 1 (valid)
    //   this->mfLineDistanceToRoad (word 8 = +0x20) = line distance
    //   stvx128 position -> this+0x00 ; stvx128 normal(v127) -> this+0x10
    //   halfword 18 (+0x24) / 19 (+0x26) = the two collision-tag halfwords (the CollisionTag word)
    //   if (lbIsOnGround) mbHasTraction = (normal.Y() > 0.5) ; else mbHasTraction = 0   (this+0xD6=214)
    void Wheel::SetRoadContact(bool lbIsOnGround, bool lbIsCloseToGround,
                               Vector3 lPosition, Vector3 lNormal,
                               u16 lu16TagHi, u16 lu16TagLo, f32 lfLineDistanceToRoad)
    {
        // CgsDev::Assert( lPosition.Y() == lPosition.Y() ) -- elided (debug-only NaN check).
        mRoadContact.mfLineDistanceToRoad   = lfLineDistanceToRoad;   // +0x20
        mRoadContact.mbIsOnGround           = lbIsOnGround;           // +0x28
        mRoadContact.mbIsCloseToGround      = lbIsCloseToGround;      // +0x2A
        mRoadContact.mbLineTestIsValid      = true;                   // +0x2B = 1

        mRoadContact.mPosition = lPosition;                          // stvx128 -> +0x00
        mRoadContact.mNormal   = lNormal;                            // stvx128 -> +0x10

        // The CollisionTag word is written as its two halfwords (+0x24 / +0x26). The asm stores
        // `*(this+18)=a5 ; *(this+19)=a4` (halfword indices 18/19 = byte offsets 0x24/0x26), and
        // a4 is bound to lu16TagHi / a5 to lu16TagLo (GP-arg order), so the HIGH halfword (0x24)
        // is lu16TagLo and the LOW halfword (0x26) is lu16TagHi -- reassembled accordingly.
        mRoadContact.mCollisionTag.muValue =
            (static_cast<u32>(lu16TagLo) << 16) | static_cast<u32>(lu16TagHi);

        // Traction: only when on the ground AND the contact normal points up enough (Y > 0.5). A
        // steep wall (low normal.Y) breaks traction. 0.5 is the asm's vcfsx(1, scale 1) immediate.
        if (lbIsOnGround)
            mbHasTraction = (lNormal.y > 0.5f);
        else
            mbHasTraction = false;
    }

    // ===========================================================================================
    //  Wheel::Reset   @0x825D7190
    // ===========================================================================================
    // Reset the wheel to a road-contact position. The asm zeroes the suspension/integration lanes
    // (like Clear), then normalizes the supplied vector via vrsqrtefp + two Newton refinement steps
    // (zero-guarded by vcmpeqfp-against-zero), scales it by an un-homed rodata factor (unk_82FB8AB0)
    // and a second vrefp+Newton reciprocal, stores the result into the integration register, and
    // finishes with SetPosition() reading the suspension register (+0x90 lane region).
    //
    // FLAG (rodata): the scale factor unk_82FB8AB0 is an un-homed runtime/.rdata scratch global
    // absent from the function export -> carried as an honest flagged-0 placeholder. The normalize
    // (mag/Newton) and the member stores it touches are EXACT; the scaled integration lane stays 0
    // until the factor is recovered. NEVER fabricated.
    void Wheel::Reset(Vector3 lvPosition)
    {
        // ⭐ IDENTIFIED, not just filled. unk_82FB8AB0 <- flt_8200D4DC, static-init splat @0x82C5AF90,
        // and the value 0.447039992 is BIT-IDENTICAL to flt_82F31928 -- the MPH->m/s conversion this
        // image uses everywhere. So this is not an "un-homed rodata scale factor" of unknown meaning:
        // Reset seeds the integration register from a direction expressed in MPH.
        static const f32 KF_RESET_SCALE = 0.447039992f;   // unk_82FB8AB0 == flt_82F31928 (MPH -> m/s)

        // Zero the integration / slip-region lanes the asm clears (vrlimi cascades at +0x30/+0x40),
        // and the suspension-inertia register touched at +0x70 region.
        mIntegrationVariables.SetZero();
        mSlipVariables.SetZero();
        mSpeedAndMassOnWheelVariables.SetZero();

        // Normalize the supplied position vector (vmsum3fp128 |v|^2 + vrsqrtefp/Newton, zero-guarded).
        const f32 lfMagSq = lvPosition.x * lvPosition.x
                          + lvPosition.y * lvPosition.y
                          + lvPosition.z * lvPosition.z;
        Vector3 lvUnit{ 0.0f, 0.0f, 0.0f, 0.0f };
        if (lfMagSq > 0.0f)
        {
            const f32 lfInvMag = 1.0f / std::sqrt(lfMagSq);
            lvUnit = Vector3{ lvPosition.x * lfInvMag,
                              lvPosition.y * lfInvMag,
                              lvPosition.z * lfInvMag, 0.0f };
        }

        // Scaled unit direction -> integration register (flagged-inert until KF_RESET_SCALE lands).
        mIntegrationVariables = Vector4{ lvUnit.x * KF_RESET_SCALE,
                                         lvUnit.y * KF_RESET_SCALE,
                                         lvUnit.z * KF_RESET_SCALE, 0.0f };

        // The asm tail loads the suspension register (+0x90) into v1 and calls SetPosition with it.
        SetPosition(mStreamedPositionPlusTwistAmount.GetVector3());
    }

    // ===========================================================================================
    //  Wheel::Prepare   @0x825FEBE8
    // ===========================================================================================
    // Prepare the wheel from a streamed position + per-wheel scalars + tire attribs: Clear() the
    // running state, assert the attribs pointer, store mpTireAttribs (+0xD0), scatter the four
    // scalars into their SIMD lanes, then SetPosition().
    //
    // ⭐ LANE MAP CORRECTED (seat wave 2026-08-05). The old body's scatter -- "inferred from the
    // SwitchAttribs sibling" -- was wrong on every lane: it put (radius ± min/max) into the +0x30/
    // +0x40 Y lanes and invented a twist store into the +0x90 w lane. The raw asm (dossier,
    // 0x825FEC48..0x825FED10) does none of that:
    //   0x825FECCC  vsubfp v10, v0, v10   ; v0 = splat(lStreamedPosition.y), v10 = splat(f4)
    //   0x825FECD8  vrlimi128 v9, v10, 8, 0    -> +0x60 lane x = pos.y - f4
    //   0x825FECD0  vaddfp v0, v0, v9          ; v9 = splat(f3)
    //   0x825FECE4  vrlimi128 v10, v0, 4, 3    -> +0x60 lane y = pos.y + f3
    //   0x825FECF0  vrlimi128 v0, v13, 1, 1    -> +0x30 lane w = f2   (v13 = splat(f2))
    //   0x825FECFC  vrlimi128 v0, v12, 1, 1    -> +0x40 lane w = f1   (v12 = splat(f1))
    //   0x825FED08  vrlimi128 v11, v0, 1, 0    -> +0x90 = {pos.xyz, OLD +0x90 w} (w PRESERVED)
    // The caller (SimpleVehiclePhysics::SetAttributes, asm 0x826027F4..0x82602840) passes
    //   f1 = lpafWheelRadii[i]  (lfs f1, 0(r31))       -- ⭐ the analytic seat reads this lane
    //   f2 = flt_82FB8BB0       (a .data scalar, 0 in the image -- runtime-initialised)
    //   f3 = mSimpleAttribs+0x00 lane x
    //   f4 = max(mSimpleAttribs+0x00 lane y, unk_82FB8440)
    //   v1 = (specWheelPos + front/rear ride height) - mSimpleAttribs.mCOMOffset
    bool Wheel::Prepare(Vector3 lStreamedPosition, f32 lfWheelRadius, f32 lfIntegrationSeed,
                        f32 lfSuspensionTravelUp, f32 lfSuspensionTravelDown,
                        const TireAttribs* lpTireAttribs)
    {
        Clear();

        CGS_ASSERT(lpTireAttribs != 0, "lpTireAttribs != NULL");        // X360 Wheel.cpp:478
        mpTireAttribs = lpTireAttribs;                                  // stw r30, 0xD0(r31)

        // +0x60: the suspension bounds around the streamed hub height (see the asm quote above).
        mSuspensionAndInertiaVariables.x = lStreamedPosition.y - lfSuspensionTravelDown;
        mSuspensionAndInertiaVariables.y = lStreamedPosition.y + lfSuspensionTravelUp;

        // +0x30 lane w / +0x40 lane w.
        mIntegrationVariables.w = lfIntegrationSeed;                    // vrlimi128 (+0x30), 1, 1
        mSlipVariables.w        = lfWheelRadius;                        // vrlimi128 (+0x40), 1, 1  ⭐ seat input

        // +0x90: xyz replaced with the streamed position, the w lane PRESERVED (vrlimi128 v11, v0,
        // 1, 0 re-inserts the OLD w -- SetVector3 writes only xyz, which is exactly that). The
        // merged register is also what SetPosition receives (vmr v1).
        mStreamedPositionPlusTwistAmount.SetVector3(lStreamedPosition);

        SetPosition(lStreamedPosition);
        return true;
    }

    // ===========================================================================================
    //  Wheel::SwitchAttribs   @0x825D6D38   (83 insns, leaf)
    // ===========================================================================================
    // ⭐⭐ REBODIED 2026-08-09 (attribs-setup wave) from its OWN asm -- the standing FLAG on the
    // old body ("inherits Prepare's disproven scatter, re-verify before anything consumes it")
    // was right to demand it: every derived lane was wrong. The real scatter MIRRORS Prepare's
    // PROVEN map (f1 -> mSlipVariables.w == radius; f2 -> mIntegrationVariables.w;
    // f3/f4 -> mSuspensionAndInertiaVariables .y/.x travel bounds), and v1 is NOT an absolute
    // position: it is a POSITION DELTA that is SUBTRACTED from both +0x90 and +0x80 (the sole
    // caller, SimpleVehiclePhysics::SwitchAttribs @0x82601978, builds it as
    // (oldCOM - newCOM) + (new - old heightOffset) * yhat against the OLD mSimpleAttribs).
    // The suspension bounds are derived from the NEW streamed hub height
    // (+0x90.y - delta.y), computed BEFORE the +0x90 commit -- `vsubfp128 v0,v0,v127` @0x825D6DD0,
    // `vspltw v0,v0,1` @0x825D6E14. The +0x90 w lane (twist) is PRESERVED
    // (`vrlimi128 v9,v0,1,0` re-inserts the OLD w); +0x80 is decremented whole.
    void Wheel::SwitchAttribs(Vector3 lPositionDelta, f32 lfWheelRadius, f32 lfIntegrationSeed,
                              f32 lfSuspensionTravelUp, f32 lfSuspensionTravelDown,
                              const TireAttribs* lpTireAttribs)
    {
        CGS_ASSERT(lpTireAttribs != NULL, "lpTireAttribs != NULL");     // Wheel.cpp:516 (0x204)
        mpTireAttribs = lpTireAttribs;                                  // stw r30, 0xD0(r31)

        // The new streamed hub height, from the running +0x90 register minus the delta.
        const f32 lfNewHubY = mStreamedPositionPlusTwistAmount.y - lPositionDelta.y;

        // +0x60: the suspension bounds around the new hub height (same lanes as Prepare).
        mSuspensionAndInertiaVariables.x = lfNewHubY - lfSuspensionTravelDown;  // vrlimi(8)
        mSuspensionAndInertiaVariables.y = lfNewHubY + lfSuspensionTravelUp;    // vrlimi(4)

        // +0x30 lane w / +0x40 lane w -- exactly Prepare's scatter.
        mIntegrationVariables.w = lfIntegrationSeed;                    // vrlimi128 (+0x30), 1, 1
        mSlipVariables.w        = lfWheelRadius;                        // vrlimi128 (+0x40), 1, 1

        // +0x90: xyz -= delta, w (twist amount) PRESERVED.
        mStreamedPositionPlusTwistAmount.x -= lPositionDelta.x;
        mStreamedPositionPlusTwistAmount.y -= lPositionDelta.y;
        mStreamedPositionPlusTwistAmount.z -= lPositionDelta.z;

        // +0x80: the running position, decremented whole (vsubfp128 v0,v0,v127).
        mPosition.x -= lPositionDelta.x;
        mPosition.y -= lPositionDelta.y;
        mPosition.z -= lPositionDelta.z;
    }

    // ===========================================================================================
    //  Wheel::UpdateVelocity   @0x825D7008   (97 insns, leaf)
    // ===========================================================================================
    // ⭐⭐ REBODIED 2026-08-07 (wheel-cluster wave). The previous body was a SLICE ARTIFACT twice
    // over: its 3-arg signature dropped the five VecFloat args the callee consumes (see Wheel.h),
    // and it read/wrote the WRONG LANE (+0x30 lane .y -- the torque accumulator -- where the asm's
    // final `vrlimi128 v7,v13,8,0` writes lane .x, the angular velocity). Its "un-homed globals"
    // FLAG was also false: every constant is a static-init'd BSS splat with an rdata-attested
    // writer in the 0x82C5Cxxx initializer bank, all read this wave (x360rd, self-test 10/10):
    //   unk_82FB9CF0 = 9000.0  (flt_8209D728, writer @0x82C5CDB0)  -- the hard spin clamp
    //   unk_82FB8BC0 =  100.0  (flt_820049E0, writer @0x82C5CE00)  -- momentum-test scale
    //   unk_82FB8B40 =  500.0  (flt_8200A034, writer @0x82C5CDD8)  -- freewheel decel rate
    //   unk_8327F240 = the shared {FALSE(0x0..0), TRUE(0xF..F)} vsel mask pair (writer
    //   @0x82C74368) -- the bool->vector-mask idiom (cntlzw/rlwinm row select), NOT a data table.
    //
    // The asm, in order (register-traced):
    //   1. candidate = mIntegrationVariables.x + mIntegrationVariables.y * dt *
    //      mSuspensionAndInertiaVariables.w        (torque -> velocity integrate, invInertia lane)
    //   2. candidate = clamp(candidate, +/-9000.0)          (vmaxfp/vminfp vs unk_82FB9CF0)
    //   3. sign = {+1, 0, -1}(candidate)                    (the two-vsel sign select)
    //   4. momentum  = |candidate * mSuspensionAndInertiaVariables.z|   (inertia lane)
    //      keepSpin  = momentum > brakeFactor * 100.0       (vcmpgtfp v8 > v7)
    //   5. decel = brakeFactor > 0 ? 1000.0 * brakeFactor   (braking)
    //            : (gasReleased ? 500.0 : 0.0)              (engine-off freewheel drag)
    //      slowed = max(|candidate| - decel * dt * invInertia, 0) * sign
    //   6. result = keepSpin ? slowed : 0                   (brake capacity stops the wheel dead)
    //   7. if (!inReverse) result = max(result, 0)          (no backwards spin in forward gear)
    //   8. rev limit: maxAngVel >= 0 ? min(result, maxAngVel) : max(result, maxAngVel)
    //   9. mu8State == eWheelInertiaTypeLocked -> result = 0
    //  10. mIntegrationVariables.x = result; .y = 0         (vrlimi 8,0 then vrlimi 4,3)
    //  11. lockInertia = !(keepSpin || |candidate| < 100.0):
    //      mSuspensionAndInertiaVariables.z/.w are KEPT when the wheel keeps spinning or is slow,
    //      and ZEROED otherwise (the braked wheel stops accepting torque until UpdateWheelInertia
    //      re-seeds the lanes next frame)  (vor v13,v8,v10 ; vsel ; vrlimi 2,2 / 1,1)
    void Wheel::UpdateVelocity(VecFloat lvfTimeStep, VecFloat lvfMaxAngularVelocity,
                               VecFloat lvfBrakeFactor, VecFloat lvfBrakeCapacityScale,
                               VecFloat lvfBrakeDecelScale, bool lbGasReleased, bool lbInReverse)
    {
        static const f32 KF_SPIN_HARD_CLAMP     = 9000.0f;   // unk_82FB9CF0 <- flt_8209D728
        static const f32 KF_FREEWHEEL_DECEL     = 500.0f;    // unk_82FB8B40 <- flt_8200A034
        static const f32 KF_SLOW_SPIN_THRESHOLD = 100.0f;    // unk_82FB8BC0 <- flt_820049E0

        const f32 lfDt         = lvfTimeStep.x;
        const f32 lfInertia    = mSuspensionAndInertiaVariables.z;
        const f32 lfInvInertia = mSuspensionAndInertiaVariables.w;

        // 1-2. integrate the accumulated torque, hard-clamp the candidate spin.
        f32 lfCandidate = mIntegrationVariables.x
                        + mIntegrationVariables.y * lfDt * lfInvInertia;
        if (lfCandidate >  KF_SPIN_HARD_CLAMP) lfCandidate =  KF_SPIN_HARD_CLAMP;
        if (lfCandidate < -KF_SPIN_HARD_CLAMP) lfCandidate = -KF_SPIN_HARD_CLAMP;

        // 3. sign select {+1, 0, -1} (vsel chain -- exactly zero stays zero).
        const f32 lfSign = (lfCandidate > 0.0f) ? 1.0f : ((lfCandidate >= 0.0f) ? 0.0f : -1.0f);

        // 4. does the wheel's angular momentum exceed what the brakes can absorb this frame?
        const f32 lfMomentum      = std::fabs(lfCandidate * lfInertia);
        const f32 lfBrakeCapacity = lvfBrakeFactor.x * lvfBrakeCapacityScale.x;   // * 100.0
        const bool lbKeepSpinning = lfMomentum > lfBrakeCapacity;

        // 5. deceleration: braking beats freewheeling beats driven.
        const f32 lfDecel = (lvfBrakeFactor.x > 0.0f)
                                ? lvfBrakeDecelScale.x * lvfBrakeFactor.x     // 1000.0 * factor
                                : (lbGasReleased ? KF_FREEWHEEL_DECEL : 0.0f);
        f32 lfSlowed = std::fabs(lfCandidate) - lfDecel * lfDt * lfInvInertia;
        if (lfSlowed < 0.0f) lfSlowed = 0.0f;
        lfSlowed *= lfSign;

        // 6. within brake capacity the wheel stops dead this frame.
        f32 lfResult = lbKeepSpinning ? lfSlowed : 0.0f;

        // 7. forward gear cannot spin the wheel backwards.
        if (!lbInReverse && lfResult < 0.0f)
            lfResult = 0.0f;

        // 8. the engine rev limit, sign-aware (maxAngVel is negative in reverse gear).
        const f32 lfMax = lvfMaxAngularVelocity.x;
        if (lfMax >= 0.0f) { if (lfResult > lfMax) lfResult = lfMax; }
        else               { if (lfResult < lfMax) lfResult = lfMax; }

        // 9. a locked wheel (handbrake/brake lock selected by UpdateWheelInertia) does not spin.
        if (mu8State == eWheelInertiaTypeLocked)
            lfResult = 0.0f;

        // 10. writeback: velocity lane .x, torque accumulator .y cleared.
        mIntegrationVariables.x = lfResult;   // vrlimi128 mask 8 (x)
        mIntegrationVariables.y = 0.0f;       // vrlimi128 mask 4 (y)

        // 11. the lock-up latch: a fast wheel fully absorbed by the brakes loses its inertia
        // lanes (stops integrating torque) until UpdateWheelInertia re-seeds them next frame.
        const bool lbKeepInertia = lbKeepSpinning
                                || std::fabs(lfCandidate) < KF_SLOW_SPIN_THRESHOLD;
        mSuspensionAndInertiaVariables.z = lbKeepInertia ? lfInertia    : 0.0f;   // vrlimi 2 (z)
        mSuspensionAndInertiaVariables.w = lbKeepInertia ? lfInvInertia : 0.0f;   // vrlimi 1 (w)
    }

    // ===========================================================================================
    //  Wheel::TireAttribs::Prepare*Tire   (eight variants)
    // ===========================================================================================
    // Pure VMX permute-scatter routines. Each lays a handful of scalar inputs out across the three
    // grip curves (mLongGripCurve @+0x00, mLatGripCurve @+0x10, mDriftLatGripCurve @+0x20) and the
    // packed register (maPackedVariables @+0x30) by `vperm`-ing the scalars through one rodata
    // permute table (unk_8327F140, four 16-byte permute rows at +0x00/+0x40/+0x80/+0xC0). They are
    // logically identical and differ only in their SOURCE numbers:
    //   * PrepareDefault{Front,Rear}Tire / Prepare{Front,Rear}TireForAI / ...ForDonutAI source the
    //     preset tuning constants from un-homed .rdata (the eight stack spills v33..v41 are loaded
    //     from those seeds before the scatter);
    //   * Prepare{Front,Rear}Tire source the per-car numbers from the supplied base-attribs block.
    //
    // FLAG (rodata): BOTH the permute table unk_8327F140 AND the per-preset source constants are
    // un-homed .rdata absent from the function exports. The scatter STRUCTURE (which lane of which
    // curve/packed register each source feeds) is faithful, but the laid-out numbers cannot be
    // reconstructed without the table/seeds, so every preset is carried as faithful-but-INERT:
    // the three curves + packed register are zero-initialised (the honest flagged-0 placeholder),
    // never fabricated. The attrib-driven pair additionally read the source block by reference so
    // the data-flow is pinned even while the lane numbers stay inert. When the permute table + seeds
    // are recovered from the XEX .rdata these bodies fill in without touching the layout.

    // Shared honest placeholder: zero the three curves + packed register (faithful-but-inert until
    // the permute table + preset seeds are recovered).
    static inline void ScatterTireInert(Wheel::TireGripCurve& lrLong,
                                        Wheel::TireGripCurve& lrLat,
                                        Wheel::TireGripCurve& lrDriftLat,
                                        Vector4& lrPacked)
    {
        lrLong.maGripVariables.SetZero();       // +0x00  FLAG: scatter target (numbers un-homed)
        lrLat.maGripVariables.SetZero();        // +0x10
        lrDriftLat.maGripVariables.SetZero();   // +0x20
        lrPacked.SetZero();                     // +0x30
    }

    void Wheel::TireAttribs::PrepareDefaultFrontTire()
    {
        ScatterTireInert(mLongGripCurve, mLatGripCurve, mDriftLatGripCurve, maPackedVariables);
    }

    void Wheel::TireAttribs::PrepareDefaultRearTire()
    {
        ScatterTireInert(mLongGripCurve, mLatGripCurve, mDriftLatGripCurve, maPackedVariables);
    }

    // ⭐ 2026-08-09 (attribs-setup wave): signatures conformed to the REAL generated wrapper
    // (the retired PhysicsVehicleBaseAttribs stand-in's own contract). Still FLAGGED-INERT: the
    // per-car source scalars live behind the wrapper's data pointer (`lwz +4`, lfs +0xE0..) and
    // the rodata permute table that places them is un-homed, so the scatter stays the inert
    // curve-zeroing until that table is recovered.
    void Wheel::TireAttribs::PrepareFrontTire(const Attrib::Gen::physicsvehiclebaseattribs& lrAttribs)
    {
        (void)lrAttribs;
        ScatterTireInert(mLongGripCurve, mLatGripCurve, mDriftLatGripCurve, maPackedVariables);
    }

    void Wheel::TireAttribs::PrepareRearTire(const Attrib::Gen::physicsvehiclebaseattribs& lrAttribs)
    {
        (void)lrAttribs;
        ScatterTireInert(mLongGripCurve, mLatGripCurve, mDriftLatGripCurve, maPackedVariables);
    }

    void Wheel::TireAttribs::PrepareFrontTireForAI()
    {
        ScatterTireInert(mLongGripCurve, mLatGripCurve, mDriftLatGripCurve, maPackedVariables);
    }

    void Wheel::TireAttribs::PrepareRearTireForAI()
    {
        ScatterTireInert(mLongGripCurve, mLatGripCurve, mDriftLatGripCurve, maPackedVariables);
    }

    void Wheel::TireAttribs::PrepareFrontTireForDonutAI()
    {
        ScatterTireInert(mLongGripCurve, mLatGripCurve, mDriftLatGripCurve, maPackedVariables);
    }

    void Wheel::TireAttribs::PrepareRearTireForDonutAI()
    {
        ScatterTireInert(mLongGripCurve, mLatGripCurve, mDriftLatGripCurve, maPackedVariables);
    }
}
}
