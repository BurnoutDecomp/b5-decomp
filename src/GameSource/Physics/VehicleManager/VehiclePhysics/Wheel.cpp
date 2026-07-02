#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"

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
        static const f32 KF_RESET_SCALE = 0.0f;   // FLAG: un-homed unk_82FB8AB0 (.rdata) scale factor

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
    // Prepare the wheel from a streamed position + suspension limits + tire attribs: Clear() the
    // running state, assert the attribs pointer (elided), store mpTireAttribs (+0xD0), scatter the
    // suspension-limit / radius / twist scalars into the integration (+0x30) / slip (+0x40) /
    // suspension (+0x60) / streamed-position (+0x90) lanes, then SetPosition().
    //
    // FLAG: the suspension-limit scalar->lane assignment (the vrlimi128 cascade laying the min/max
    // suspension heights + radius + twist across the SIMD lanes) is inferred from the SwitchAttribs
    // sibling (same scatter, same destination offsets); the console fastcall packs the scalars
    // across f-regs (a9/a10 + the &a23/&a28/&a30 stack spills). The structurally certain parts
    // (Clear, the pointer store, the streamed-position store, SetPosition) are EXACT; the per-lane
    // suspension math is faithful-to-the-stores but the exact source-lane of each scalar is partial.
    bool Wheel::Prepare(Vector3 lStreamedPosition, f32 lfMaxSuspensionHeight,
                        f32 lfMinSuspensionHeight, f32 lfRadiusSeed, f32 lfTwistSeed,
                        const TireAttribs* lpTireAttribs)
    {
        Clear();

        // CgsDev::Assert( lpTireAttribs != NULL ) -- elided (debug-only).
        mpTireAttribs = lpTireAttribs;                                  // *(this+208) = a6

        // Streamed position (xyz) + twist amount (w lane) -> mStreamedPositionPlusTwistAmount (+0x90).
        mStreamedPositionPlusTwistAmount.SetVector3(lStreamedPosition);
        mStreamedPositionPlusTwistAmount.SetPlus(lfTwistSeed);

        // Suspension-limit lanes (FLAG: source-lane assignment inferred from SwitchAttribs). The asm
        // forms (radius - min) into the integration lane and (radius + max) into a suspension lane,
        // and inserts the min/max into the +0x30/+0x40 registers via vrlimi128 lane 1.
        mIntegrationVariables.y = lfRadiusSeed - lfMinSuspensionHeight;   // vsubfp v0,v10  (lane insert 1,1)
        mSlipVariables.y        = lfRadiusSeed + lfMaxSuspensionHeight;   // vaddfp v0,v9   (lane insert 1,1)

        SetPosition(lStreamedPosition);
        return true;
    }

    // ===========================================================================================
    //  Wheel::SwitchAttribs   @0x825D6D38
    // ===========================================================================================
    // The in-place sibling of Prepare: re-point mpTireAttribs and re-derive the suspension lanes
    // WITHOUT clearing the running state. Same scatter, same destination offsets; additionally the
    // asm subtracts the new streamed position from the existing +0x80 / +0x90 registers (vsubfp128
    // v0,v0,v127) so the running position tracks the attrib swap.
    //
    // FLAG: same inferred suspension-lane scatter as Prepare. The pointer store + streamed-position
    // delta are EXACT; the per-lane suspension math is faithful-to-the-stores but partial.
    void Wheel::SwitchAttribs(Vector3 lStreamedPosition, f32 lfMaxSuspensionHeight,
                              f32 lfMinSuspensionHeight, f32 lfRadiusSeed, f32 lfTwistSeed,
                              const TireAttribs* lpTireAttribs)
    {
        // CgsDev::Assert( lpTireAttribs != NULL ) -- elided (debug-only).
        mpTireAttribs = lpTireAttribs;                                  // *(this+208) = a6

        // Re-seed the streamed position + twist (the asm reloads +0x90 then subtracts v127 = the new
        // streamed position so the suspension register tracks the swap).
        mStreamedPositionPlusTwistAmount.SetVector3(lStreamedPosition);
        mStreamedPositionPlusTwistAmount.SetPlus(lfTwistSeed);

        mIntegrationVariables.y = lfRadiusSeed - lfMinSuspensionHeight;
        mSlipVariables.y        = lfRadiusSeed + lfMaxSuspensionHeight;

        // vsubfp128 v0,v0,v127 at +0x80: the running position is offset by the new streamed position.
        mPosition.x -= lStreamedPosition.x;
        mPosition.y -= lStreamedPosition.y;
        mPosition.z -= lStreamedPosition.z;
    }

    // ===========================================================================================
    //  Wheel::UpdateVelocity   @0x825D7008
    // ===========================================================================================
    // Clamp + integrate the wheel's own angular velocity for the frame. UpdateWheels calls this per
    // wheel after computing the engine rev-limit max (lvfMaxAngularVelocity) and two booleans
    // (lbStraightLine, lbCrashing). The asm builds a friction-cone-shaped clamp: it reads the wheel's
    // current angular-velocity lane (+0x30 lane1) and torque lane (+0x60 lane3 region), forms the
    // candidate new velocity, applies a chain of min/max + masked selects against several un-homed
    // scratch globals (unk_82FB9CF0 / 82FB8BC0 / 82FB8B40 / 8327F240) and the supplied max, then
    // stores the clamped angular velocity back into the integration (+0x30) and suspension (+0x60)
    // registers.
    //
    // FLAG (rodata): the clamp tables/masks (unk_82FB9CF0/8BC0/8B40, the permute table 8327F240
    // indexed by the cntlzw-derived selectors from the two bools + mu8State) are un-homed runtime/
    // .rdata scratch globals absent from the export -> carried as honest flagged-inert. The control
    // flow (selector derivation, the min/max clamp against lvfMaxAngularVelocity, the writeback
    // lanes) is faithful; the masked table contributions stay inert until the globals are recovered.
    // The two booleans select harsher vs softer clamp rows; mu8State == eWheelInertiaTypeLocked
    // forces the angular velocity toward zero (the `*(result+215)-1` cntlzw branch).
    void Wheel::UpdateVelocity(VecFloat lvfMaxAngularVelocity, bool lbStraightLine, bool lbCrashing)
    {
        (void)lbStraightLine;   // selects a clamp row via the 8327F240 permute (flagged-inert here)
        (void)lbCrashing;       // ditto

        // Current wheel angular velocity (integration register lane 1) and the per-wheel limit lane.
        f32 lfAngular = mIntegrationVariables.y;
        const f32 lfMax = lvfMaxAngularVelocity.x;

        // Clamp to the engine-derived max (vmaxfp/vminfp against the supplied limit). The masked
        // table contributions (surface/skid rows) are flagged-inert (un-homed globals) and add 0.
        if (lfAngular >  lfMax) lfAngular =  lfMax;
        if (lfAngular < -lfMax) lfAngular = -lfMax;

        // A locked wheel (braked/handbrake) is driven toward zero spin -- the `*(this+215)-1` branch
        // (mu8State == eWheelInertiaTypeLocked) selects the zero row.
        if (mu8State == eWheelInertiaTypeLocked)
            lfAngular = 0.0f;

        mIntegrationVariables.y = lfAngular;          // writeback -> +0x30 lane1 (vrlimi128 8,0)
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

    void Wheel::TireAttribs::PrepareFrontTire(const PhysicsVehicleBaseAttribs& lrAttribs)
    {
        // FLAG: the per-car source lanes (lrAttribs.mvTireScalars{Long,Lat}) feed the scatter; the
        // permute table that places them is un-homed, so the result stays inert. Reference pinned.
        (void)lrAttribs;
        ScatterTireInert(mLongGripCurve, mLatGripCurve, mDriftLatGripCurve, maPackedVariables);
    }

    void Wheel::TireAttribs::PrepareRearTire(const PhysicsVehicleBaseAttribs& lrAttribs)
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
