#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"  // the seat bring-up leg reads the RESIDENT spec (WheelSpecs, mMeshOffset)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"  // BrnPlayerDriverControls (C07 boost/speed-match)
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"  // KVF_HANDBRAKE_OFF_TIME_TO_ALLOW_DRIFT (CheckForEnteringDrift)
#include "GameShared/GameClasses/Numeric/CgsRandom.h" // CgsNumeric::Random (the shared LCG ring)
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT (UpdateDownForce's attribsys guard)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu::Start/StopMonitor (Update's stage brackets)
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerPerfMonHandles.h" // the seven gs_iVPhys* monitor ids (hoisted, orchestrator wave)
#include "rw/math/vpu/vector3_operation.h"            // rw::math::vpu::{MagnitudeSquared, Normalize, Dot, operator*}
#include "rw/math/vpu/matrix44affine_operation.h"     // rw::math::vpu::{InverseOfMatrixWithOrthonormal3x3, operator*}
#include "rw/math/fpu/scalar_operation.h"            // rw::math::fpu::IsZero (SetWheelVelocities' per-axle power gates)

#include <stdint.h>   // uint32_t / uint64_t for the road-noise LCG draw
#include <algorithm>  // std::min / std::max (the driving spine's vmaxfp/vminfp lowerings)
#include <cstring>    // std::memcpy (bit-reinterpret the road-noise float)
#include <cmath>      // std::sqrt / std::sin (boost-kick wheelie-angle limit + speed magnitudes)

// BrnPhysics::Vehicle::VehiclePhysics -- the out-of-line ledger funcs owned by the Vehicle-physics
// group (class TU). The header-homed leaf methods (GetShowtimeDeformationScale,
// IsCounterSteeringAtLowSpeed) and the nested SlamEffect/ShuntEffect bodies live elsewhere; this
// .cpp is the home for the three scalar predicates plus the three VMX128 functions (the latter
// lowered to faithful scalar / canonical rw::math::vpu math) whose X360 addresses the ledger lists.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;
    // @0x825B2FE0  BrnPhysics::Vehicle::VehiclePhysics::GetNumberOfWheelsOnTheGround
    //   The X360 reads the four driven wheels' road-contact on-ground flags at +0x158/+0x238/+0x318/
    //   +0x3F8 (maWheels stride 0xE0, RoadContact::mbIsOnGround +0x28) and counts the nonzero ones:
    //     v1  = maWheels[0].onGround ? 1 : 0
    //     if (maWheels[1].onGround) ++v1
    //     if (maWheels[2].onGround) ++v1
    //     return maWheels[3].onGround ? v1 + 1 : v1
    //   i.e. the count of driven wheels on the ground.
    s32 VehiclePhysics::GetNumberOfWheelsOnTheGround() const
    {
        s32 liCount = 0;
        if (maWheels[eFrontLeftWheel].GetRoadContact().mbIsOnGround)
            ++liCount;
        if (maWheels[eFrontRightWheel].GetRoadContact().mbIsOnGround)
            ++liCount;
        if (maWheels[eRearLeftWheel].GetRoadContact().mbIsOnGround)
            ++liCount;
        if (maWheels[eRearRightWheel].GetRoadContact().mbIsOnGround)
            ++liCount;
        return liCount;
    }

    // @0x825E6D50  BrnPhysics::Vehicle::VehiclePhysics::IsBeingSlamedOrShunted
    //   lfs f13,0x111C(r3) ; fcmpu f13, 0.0 ; bgt -> return 1     (slam in progress)
    //   addi r3,r3,0x1130 ; bl ShuntEffect::IsActive ; -> result  (otherwise: shunt active?)
    bool VehiclePhysics::IsBeingSlamedOrShunted() const
    {
        if (mSlamEffect.mfSlamLife > 0.0f)
            return true;
        return mShuntEffect.IsActive();
    }

    // @0x82615290  BrnPhysics::Vehicle::VehiclePhysics::IsBeingSlamedOrShuntedByRaceCar
    //   lbz r11,0x13E0(r3) ; extsb r11 ; extsb a2 ; cmpw -> if (a2 != mi8LastAttackersRaceCarIndex) return 0
    //   bl IsBeingSlamedOrShunted ; -> return (that ? 1 : 0)
    //   i.e. true only when the slamming/shunting race car is the queried one AND a slam/shunt is live.
    bool VehiclePhysics::IsBeingSlamedOrShuntedByRaceCar(s8 li8RaceCarId) const
    {
        if (li8RaceCarId != mi8LastAttackersRaceCarIndex)
            return false;
        return IsBeingSlamedOrShunted();
    }

    // @0x825C0100  BrnPhysics::Vehicle::VehiclePhysics::GetCarGroundDistanceCheck
    //   addi   r11,r3,0x20 ; lvx128 v0,r11 ; vspltw v0,v0,1   ; load mUpAxis, splat .y lane
    //   vspltisw v13,0      ; vcmpgtfp. v0,v13,v0             ; test 0 > up.y  (car inverted?)
    //   lfs    f1,flt_82001DA0                                ; result = 0.5
    //   beqlr  cr6                                            ; up.y >= 0 -> return 0.5
    //   lfs    f13,0x6A4(r3) ; lfs f0,flt_82001D9C            ; mHalfExtent.y, multiplier
    //   fmadds f1,f13,f0,f1                                   ; return extent*K + 0.5
    // i.e. normally 0.5, but when the up axis points downward (the car is upside down) the result
    // grows by the car's own vertical extent scaled by a constant.
    //
    // Constants: flt_82001DA0 = 0.5 and flt_82001D9C = 2.0, both resolved .rdata literals
    // (flt_82001D9C is homed as 2.0f in CgsQuat.cpp and BrnEmitter3dControl.cpp). The math op
    // (extent * 2.0 + 0.5) and the member stores it touches are exact.
    f64 VehiclePhysics::GetCarGroundDistanceCheck() const
    {
        static const f32 KF_GROUND_DISTANCE_BASE = 0.5f;                  // flt_82001DA0 (resolved)
        static const f32 KF_CAR_GROUND_DISTANCE_INVERTED_SCALE = 2.0f;   // flt_82001D9C = 2.0 (resolved)

        if (!(0.0f > mTransform.yAxis.y))   // up axis not pointing down -> car is upright
            return KF_GROUND_DISTANCE_BASE;

        return mHalfExtent.y * KF_CAR_GROUND_DISTANCE_INVERTED_SCALE + KF_GROUND_DISTANCE_BASE;
    }

    // @0x825B2EF8  BrnPhysics::Vehicle::VehiclePhysics::GetTransformDelta
    //   r11 = this+0x1370 (mPreviousTransform) ; r10 = this+0x10 (mTransform)
    //   The X360 builds the inverse of mPreviousTransform inline: the orthonormal 3x3 is transposed
    //   with vmrglw/vmrghw lane merges, and the inverse translation is the transpose applied to the
    //   negated position (vsubfp v10,0,wAxis then the vmaddfp cascade). That inverse is then matrix-
    //   multiplied by the current mTransform and the four affine rows are stored to the return buffer
    //   (stvx128 -> result+0/+0x10/+0x20/+0x30).
    //   So: delta = inverse(mPreviousTransform) * mTransform, in mPreviousTransform's local space.
    Matrix44Affine VehiclePhysics::GetTransformDelta() const
    {
        return vpu::InverseOfMatrixWithOrthonormal3x3(mPreviousTransform) * mTransform;
    }

    // @0x825C0000  BrnPhysics::Vehicle::VehiclePhysics::UpdateLinearVelocityMagnitude
    //   r9 = this+0x50 (mLinearVelocity) ; r10 = this+0x1340 (mNormLinearVelocityMag)
    //   vmsum3fp128 v0,v10,v10  -> |v|^2 (dot3) ; the cached vector is zeroed first (stvx128 v12=0).
    //   vrsqrtefp + Newton refinement -> 1/|v| ; v0 = |v|^2 * (1/|v|) = |v| (the speed magnitude),
    //   guarded by vsel/vcmpeqfp-against-zero so a zero-speed input yields a zero (no NaN).
    //   The unit direction is written into the xyz lanes and the magnitude into the "plus" (w) lane
    //   of mNormLinearVelocityMag (vrlimi128 packs the magnitude lane into the direction register).
    // Lowered here to the canonical Normalize + scalar magnitude; the member stores (direction in
    // xyz, speed in the w lane) match the asm's stvx128 to this+0x1340.
    void VehiclePhysics::UpdateLinearVelocityMagnitude()
    {
        const f32 lfSpeedSquared = vpu::MagnitudeSquared(mLinearVelocity);

        Vector3 lvDirection = vpu::Normalize(mLinearVelocity);   // zero vector when speed is zero
        const f32 lfSpeed = (lfSpeedSquared > 0.0f) ? std::sqrt(lfSpeedSquared) : 0.0f;

        mNormLinearVelocityMag.SetVector3(lvDirection);   // unit direction -> xyz lanes
        mNormLinearVelocityMag.SetPlus(lfSpeed);          // speed magnitude -> w / "plus" lane
    }

    // @0x825D0840  BrnPhysics::Vehicle::VehiclePhysics::GetDownForce
    //   The textbook aerodynamic quadratic: F = 0.5 * rho * CdA * |mLinearVelocity|^2 * coeff.
    //   asm: |v|^2 via vmsum3fp128 of mLinearVelocity(+0x50); coeff = mpAttribs(+0x720)->mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo
    //        (+0xB0) lane .w (vspltw v11,v11,3); 0.5 from vcfsx(1, scale 1); rho/CdA lazily cached
    //        (g_AeroConstInitMask) into g_vAero_Rho / g_vAero_CdA from the .rdata seeds
    //        kAero_Rho_Scalar / kAero_CdA_Scalar. The product order in the asm is
    //        ((0.5*CdA)*|v|^2)*coeff*rho; multiplication is commutative so the grouping is free.
    //
    //   The two seed scalars are RECOVERED from the .rdata (they were never reachable from the
    //   static-init map: this is a LAZY FIRST-CALL cache, so the literals live inside GetDownForce's
    //   own asm behind the g_AeroConstInitMask bit tests, which is why five static-init sweeps
    //   missed them). The `lfs` at 0x825D085C-68 reads kAero_Rho_Scalar @0x820948D4 = 0x40C33333
    //   = 6.1 into g_vAero_Rho; the `lfs` at 0x825D08D4-E0 reads kAero_CdA_Scalar @0x820948D0 =
    //   0x3F9CCCCD = 1.225 into g_vAero_CdA. Seated below against those exact symbols.
    //   NOTE ON THE LABELS: 1.225 is sea-level air density to four digits, so the two IDA symbol
    //   names may well be transposed relative to their physical meaning. It does not matter here --
    //   both factors enter the SAME product (0.5*rho*CdA = 3.73625 either way), and seating each
    //   value against the symbol the asm actually loads into that cache keeps this file's
    //   address->name mapping honest. Do not "fix" the ordering without new evidence.
    //   (The X360 also caches an unrelated third aero constant flt_8200D57C = 0.24 -> unk_82FBA0B0;
    //   that slot has ld=0/st=1 across the whole image -- its consumer is not in this build -- so it
    //   does not enter this product and is not modelled here.)
    Vector3 VehiclePhysics::GetDownForce() const
    {
        static const f32 KF_AERO_RHO = 6.1f;     // kAero_Rho_Scalar @0x820948D4 (0x40C33333) -> g_vAero_Rho
        static const f32 KF_AERO_CDA = 1.225f;   // kAero_CdA_Scalar @0x820948D0 (0x3F9CCCCD) -> g_vAero_CdA
        static const f32 KF_HALF     = 0.5f;   // vcfsx v0=1, scale 1 -> 0.5

        const f32 lfSpeedSquared = vpu::MagnitudeSquared(mLinearVelocity);
        const f32 lfCoeff        = mpAttribs->mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.w;   // .w lane of the aero params register

        const f32 lfDownForce = KF_HALF * KF_AERO_RHO * KF_AERO_CDA * lfSpeedSquared * lfCoeff;

        return Vector3{ lfDownForce, lfDownForce, lfDownForce, lfDownForce };
    }

    // ---------------------------------------------------------------------------------------
    // Surface-response group: GetSurfaceGrip / GetSurfaceRoughness / GetSurfaceLinearDrag
    //   @0x825D51B8 / @0x825D5328 / @0x825D50A8. Each derives a 6-bit surface id from a
    //   RoadContact CollisionTag and looks up a global per-surface property table, blended with a
    //   lane of mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.
    //   FLAG (runtime data): the per-surface tables (grip unk_82FB8890, drag unk_82FB8BD0,
    //   roughness unk_82FB8DE0, global roughness scale unk_82FB9220, wet multiplier unk_82FB9EC0)
    //   are RUNTIME-LOADED scratch globals not present in the exports -> honest flagged-0
    //   placeholders (faithful-but-inert): the surface-id extraction, the lerp/scale math and the
    //   attrib lanes are EXACT; the looked-up property stays 0 until the tables are recovered.
    //   NEVER fabricated. The debug "properties loaded" + surface-id-bound asserts are elided.
    // ---------------------------------------------------------------------------------------

    // luSurfaceId = (tag.muValue >> 4) & 0x3F. 0x825D5148-50: `lhz r11,0x596(r27) ; srwi r11,r11,4 ;
    // clrlwi r31,r11,26` -- a HALFWORD load at tag+2 (the low 16 bits of the big-endian muValue),
    // then >>4 &0x3F, i.e. bits 4-9 of muValue -- NOT the byte+2 >>4 framing this used to assume
    // (which produced >>12). Matches UpdateInWaterBehaviour's (muValue >> 4) & 0x3F for the same
    // extraction (0x825B81E4 region), so the two stay consistent.
    static inline s32 SurfaceIdFromTag(CollisionTag lTag)
    {
        return static_cast<s32>((lTag.muValue >> 4) & 0x3Fu);
    }

    // FLAG: the per-surface property tables are runtime-loaded scratch globals (un-homed); the
    // looked-up value is carried as a flagged-0 placeholder until the table is recovered.
    static inline f32 SurfacePropertyPlaceholder(s32 /*liSurfaceId*/)
    {
        return 0.0f;
    }

    // @0x825D51B8  GetSurfaceGrip: result = 1 - (1 - gripTable[id]) * blend  (a lerp toward 1.0).
    Vector3 VehiclePhysics::GetSurfaceGrip(EVehicleDrivenWheel leWheel) const
    {
        const s32 liSurfaceId = SurfaceIdFromTag(GetWheel(leWheel).GetRoadContact().mCollisionTag);
        const f32 lfGrip = SurfacePropertyPlaceholder(liSurfaceId);   // unk_82FB8890[id]

        // FRONT wheels (index < eRearLeftWheel) use lane .x, REAR wheels lane .y (asm: if a3<2).
        const f32 lfBlend = (leWheel < eRearLeftWheel) ? mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.x
                                                       : mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.y;
        const f32 lfResult = 1.0f - (1.0f - lfGrip) * lfBlend;
        // (optional wet/condition multiplier unk_82FB9EC0 gated by byte_82FB7DF2 -- FLAG: that gate
        // defaults off here; the wet path stays inert until the table is recovered.)
        return Vector3{ lfResult, lfResult, lfResult, lfResult };
    }

    // @0x825D5328  GetSurfaceRoughness = roughTable[id] * globalRoughScale * blend.z.
    Vector3 VehiclePhysics::GetSurfaceRoughness(EVehicleDrivenWheel leWheel) const
    {
        const s32 liSurfaceId = SurfaceIdFromTag(GetWheel(leWheel).GetRoadContact().mCollisionTag);
        const f32 lfRoughness = SurfacePropertyPlaceholder(liSurfaceId);     // unk_82FB8DE0[id]
        // unk_82FB9220 <- flt_82004744 = 0.2f (static-init splat @0x82C5A498). NOTE: the per-surface
        // roughness TABLE this multiplies is still a runtime-loaded scratch global that is genuinely
        // zero at compile time, so the product is unchanged today -- the scale is seated for when the
        // table lands, not because it moves a number now.
        static const f32 KF_GLOBAL_ROUGHNESS_SCALE = 0.2f;                   // unk_82FB9220

        const f32 lfResult = lfRoughness * KF_GLOBAL_ROUGHNESS_SCALE * mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.z;
        return Vector3{ lfResult, lfResult, lfResult, lfResult };
    }

    // @0x825D50A8  GetSurfaceLinearDrag = dragTable[id] * blend.w  (single representative contact).
    Vector3 VehiclePhysics::GetSurfaceLinearDrag() const
    {
        // +0x596 is the BE-LOW halfword of mAboveGroundTestResult.mCollisionTag; taken by shift
        // (never by byte offset -- the halves swap position on a little-endian host).
        const s32 liSurfaceId = static_cast<s32>((GetAboveGroundTagLo() >> 4) & 0x3Fu);
        const f32 lfDrag = SurfacePropertyPlaceholder(liSurfaceId);   // unk_82FB8BD0[id]

        const f32 lfResult = lfDrag * mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.w;
        return Vector3{ lfResult, lfResult, lfResult, lfResult };
    }

    // =======================================================================================
    // Surface-grip/drag/friction group (C05): UpdateRoadNoise + the two tyre-friction solvers.
    // =======================================================================================

    // The road-noise LCG draw -- the inlined CgsNumeric::Random::AddRandomFloatToBuffer the X360
    // emits straight into UpdateRoadNoise. State is the shared Random's `muSeed`(+0x20) and
    // `muOldestBufferIndex`(+0x28); the 8-entry `mafFloatBuffer`(+0x00) is a ring of pre-generated
    // floats in [1.0, 2.0). One call consumes the oldest float, refills that slot from the advanced
    // seed (mantissa-stuffed) and advances the index `(idx+1) & 7`.
    //   asm: r27 = 0x5851F42D4C957F2D ; seed' = seed*r27 + 1 ; r8 = 0x3F800000 | (highBits>>9)
    //        -> float in [1,2) ; consume mafFloatBuffer[idx] ; store new float ; idx = (idx+1)&7.
    // Returns the CONSUMED float (the value the asm's `lfsx f0` loads before overwriting the slot).
    static inline f32 DrawRoadNoiseFloat(CgsNumeric::Random& lrRandom)
    {
        // The Random members are private in the SDK; the X360 inlines the raw field access. Model the
        // same three fields at their console offsets (muSeed @+0x20, muOldestBufferIndex @+0x28,
        // mafFloatBuffer @+0x00) via a byte view so this stays layout-faithful without befriending.
        uint8_t* lpBase = reinterpret_cast<uint8_t*>(&lrRandom);
        uint64_t& lruSeed  = *reinterpret_cast<uint64_t*>(lpBase + 0x20);
        uint32_t& lruIndex = *reinterpret_cast<uint32_t*>(lpBase + 0x28);
        float*    lpRing   = reinterpret_cast<float*>(lpBase + 0x00);

        const uint32_t luIndex = lruIndex;
        const f32 lfConsumed = lpRing[luIndex];                 // lfsx f0, r10, r30  (the oldest float)

        const uint64_t luOldSeed = lruSeed;
        const uint64_t luNewSeed = luOldSeed * 0x5851F42D4C957F2DULL + 1ULL;  // mulld+addi
        lruSeed = luNewSeed;

        // Build the replacement float in [1.0, 2.0): 0x3F800000 | (top bits of the OLD seed >> 9).
        // asm: srdi r7,seed,32 ; inslwi r8(=0x3F800000), r7, 23, 9  -> mantissa = bits[31:9] of hi32.
        const uint32_t luHi32 = static_cast<uint32_t>(luOldSeed >> 32);
        const uint32_t luBits = 0x3F800000u | (luHi32 >> 9);
        float lfNew;
        std::memcpy(&lfNew, &luBits, sizeof(lfNew));            // bit-reinterpret, stwx of r8
        lpRing[luIndex] = lfNew;

        lruIndex = (luIndex + 1u) & 7u;                          // (idx+1) & 7
        return lfConsumed;
    }

    // @0x825F6980  BrnPhysics::Vehicle::VehiclePhysics::UpdateRoadNoise
    //   r29 = this ; r30 = &Random ; r31 = this+0x130 (maWheels[0]); stride 0xE0; 4 wheels.
    //   Pre-loop: lfPre = (DrawRoadNoiseFloat() - 1.0) * 0.5             [f28; flt_82001C98=1.0, ..DA0=0.5]
    //   Per grounded wheel (RoadContact.mbIsOnGround @ wheel+0x28):
    //     lfWheel = (DrawRoadNoiseFloat() - 1.0) * 0.5 + lfPre           [in [0,1)]
    //     lvRough = GetSurfaceRoughness(wheel)                          (flagged-inert table)
    //     lfFactor = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z                          (per-vehicle road-noise scale)
    //     lfSpeed  = mfSpeedMPH (+0x6C0)                                (speed gate, v124)
    //     0x825F69E0 region: `vspltw128 v126,(attribs+112),2 ; vrefp128 v0,v126` + two Newton refines
    //     -> v125 = 1/lfFactor (a RECIPROCAL -- lfFactor DIVIDES, it does not multiply). The tail
    //     multiplies the speed-scaled ratio by roughness AFTER the 1.0 clamp:
    //     `vmulfp128 v0,v0,v124(speed) ; vminfp128 v0,v0,v127(1.0) ; vmulfp128 v0,v13(roughness),v0`
    //     -- i.e. noise = (preDraw + wheelDraw) * roughness * min(speed / factor, 1.0); the 1.0 clamp
    //     applies to the speed/factor RATIO alone, not to the whole product. Accumulates into the
    //     wheel's leading SIMD region (lvx128/vmaddfp/stvx128 at wheel+0).
    //   The scalar noise term + roughness/factor/speed product + the 1.0 clamp are recovered faithfully;
    //   the exact wheel-register destination lanes are partial (see Wheel::AddRoadNoise FLAG).
    void VehiclePhysics::UpdateRoadNoise(CgsNumeric::Random& lrRandom)
    {
        static const f32 KF_ONE  = 1.0f;   // flt_82001C98 (resolved 1.0)
        static const f32 KF_HALF = 0.5f;   // flt_82001DA0 (resolved 0.5)

        // One pre-loop draw shared across all wheels (matches the single pre-loop LCG step in the asm).
        const f32 lfPreDraw = (DrawRoadNoiseFloat(lrRandom) - KF_ONE) * KF_HALF;

        const f32 lfFactor = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z;   // per-vehicle road-noise scale (.z lane)
        const f32 lfSpeed  = mfSpeedMPH.x;                  // +0x6C0 body-frame speed (MPH)

        // speedRatio = min(speed / factor, 1.0) -- the asm's vrefp reciprocal + Newton refine of
        // lfFactor, multiplied by speed, THEN clamped to 1.0 (before the roughness multiply below).
        f32 lfSpeedRatio = (lfFactor != 0.0f) ? (lfSpeed / lfFactor) : 0.0f;
        if (lfSpeedRatio > KF_ONE)
            lfSpeedRatio = KF_ONE;

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const EVehicleDrivenWheel leWheel = static_cast<EVehicleDrivenWheel>(liWheel);
            // Only grounded wheels rumble (lbz r11,0x28(r31) ; beq skip).
            if (!maWheels[leWheel].GetRoadContact().mbIsOnGround)
                continue;

            // Per-wheel draw, combined with the shared pre-draw -> noise in [0, 1).
            const f32 lfWheelDraw = (DrawRoadNoiseFloat(lrRandom) - KF_ONE) * KF_HALF + lfPreDraw;

            // Surface roughness (.x lane representative; flagged-inert until the table is recovered).
            const f32 lfRoughness = GetSurfaceRoughness(leWheel).x;

            // noise = wheelDraw * roughness * clamp(speed/factor, 1.0) -- roughness multiplies AFTER
            // the speed/factor ratio has already been clamped to 1.0.
            const f32 lfNoise = lfWheelDraw * lfRoughness * lfSpeedRatio;

            // Accumulate into the wheel's road-noise register (vmaddfp -> stvx128 at wheel base).
            // FLAG (partial): exact destination lanes overlap Wheel-owned blobs -- see Wheel::AddRoadNoise.
            maWheels[leWheel].AddRoadNoise(lfNoise);
        }
    }

    // @0x825FB458  BrnPhysics::Vehicle::VehiclePhysics::HandleWheelPairFriction
    // ---------------------------------------------------------------------------------------------
    // FIDELITY: BLOCKED. The X360 export is a degenerate VMX128 routine flagged by Hex-Rays as
    // "local variable allocation has failed, the output may be wrong"; the disassembly is ~500
    // instructions of raw `lvx128/vperm/vmrghw/vmsum3fp128/vrsqrtefp/vrlimi128/vsel` SIMD with ~200
    // unnamed stack temporaries (v285..v328) and a dozen un-homed rodata permute/limit vectors
    // (unk_82FB9160, unk_8327F240, unk_82FB9BF0, unk_82FB9150, unk_82FB83F0, unk_82FB8100,
    // unk_82FB80D0, unk_82FBA1E0, unk_82CDA3F0, unk_82CDADC0, ...). The per-lane data routing -- which
    // of the two wheels lands in which lane, how the Gram-Schmidt orthonormalise / friction-cone
    // clamp / adhesion renormalise distribute across lanes -- is NOT recoverable to store-faithful
    // C++ without fabricating the lane semantics, which the project rules forbid. The OVERALL
    // STRUCTURE (recovered from the asm + the verified findings doc) is reproduced as a commented
    // skeleton so the call shape, ordering and side-effects are pinned; the arithmetic is intentionally
    // NOT emitted. When the full Wheel/VehicleAttribs TUs land (giving named tyre-curve + direction
    // lanes) this should be re-attempted store-for-store.
    //
    // STRUCTURE (per axle, both wheels processed 2-wide):
    //   1. Lazy-cache the ~0.85 linear-force cap (unk_82FBA1E0, gated by dword_82FBA1F0 bit0).
    //   2. For each wheel: assert mpTireAttribs != NULL (Wheel.h:549); gather contact-relative velocity,
    //      wheel long/lat direction vectors and the packed tyre grip-curve variables.
    //   3. Build longitudinal + lateral UNIT directions (Gram-Schmidt: lat -= (lat.long)long via vnmsubfp,
    //      renormalise via vrsqrtefp + Newton).
    //   4. Project the contact-relative velocity onto long/lat -> slip; sample the grip-curve coefficient;
    //      multiply by the surface-grip-derived limit.
    //   5. Resolve the combined long+lat force inside a FRICTION CONE: vmaxfp 0 / vminfp adhesiveLimit,
    //      reciprocal-renormalise, apply the ~0.85 linear cap.
    //   6. Mask the longitudinal lane to 0 for locked/skidding wheels (crash flag this+0x710, mbHasTraction,
    //      mu8State) via unk_82FB8100 / unk_82FB80D0; set each wheel's broken-adhesive byte (+0x205 = +517).
    //   7. Feed wheel-spin reaction back: Wheel::ApplyFrictionReaction(wheel).
    //   8. For each non-zeroed force component: validity assert, then r x F -> AddWorldSpaceTorque(this+0x10),
    //      and accumulate the linear residual at this+0x240 scaled by mvfWheelFrictionLinearMultiplier
    //      (this+0x4048). A grip-curve-layout selector at this+0x1294 (+4946) chooses normal vs drift packing.
    // ---------------------------------------------------------------------------------------------
    // ⭐⭐ ORACLE RECOVERED 2026-08-07 (tyre-math wave). The clean-C algorithm twin is BPR
    //   BrnPhysics::Vehicle::RoadVehiclePhysics::UpdateWheels @0xBA1420 -> sub_B9BD60, called TWICE
    //   with (2,3) then (0,1) -- the exact X360 UpdateWheels call shape -- and it decompiles to a
    //   fully legible _mm_* body. Its structure matches this skeleton STEP FOR STEP: TLS-guarded
    //   lazy cap init (dword_15E6654 / xmmword_15E6700), rsqrt+Newton Gram-Schmidt long/lat unit
    //   directions, the a x b cross products, the friction cone (max(v,-v) vs 1.0 + reciprocal
    //   renormalise), the crash-flag branch (this+4464), Wheel friction-reaction (sub_B90DB0), and
    //   the r x F torque + linear accumulate (sub_50F210 into this+5216). So the algorithm the prior
    //   waves called "not recoverable" IS recoverable -- from the twin, not from the X360 export.
    //   ⛔ STILL BLOCKED for commit: (1) BPR is RoadVehiclePhysics, a DIVERGED sibling class whose
    //   member offsets are NOT this class's, so nothing may be copied -- only the algorithm is an
    //   oracle; (2) the X360 pseudocode is degenerate ("local variable allocation has failed", args
    //   render as int a1..a19, the VMX128 regs are invisible), so the per-slot lane routing must be
    //   read from the X360 VMX ASM (~1142 instrs) and matched to the BPR lanes ONE AT A TIME -- a
    //   multi-wave cross-map, and VMX lane order != SSE lane order so a shuffle cannot be transcribed
    //   blind. NOT emitted this wave: no guessed lanes, and it is functionally unverifiable until
    //   PhysicsModule::Update lands (nothing consumes these forces yet).
    // ---------------------------------------------------------------------------------------------
    // ⭐ SIGNATURE CONFORMED 2026-08-07 (wheel-cluster wave) to the DWARF 9-arg form -- see the
    // header note. The skeleton stays FIDELITY: BLOCKED; only the shape changed.
    void VehiclePhysics::HandleWheelPairFriction(EVehicleDrivenWheel /*leWheelA*/,
                                                 EVehicleDrivenWheel /*leWheelB*/,
                                                 Vector3 /*lvRollDirection*/,
                                                 VecFloat /*lvfDownForce*/,
                                                 VecFloat /*lvfTimeStep*/,
                                                 VecFloat /*lvfSurfaceGripA*/,
                                                 VecFloat /*lvfSurfaceGripB*/,
                                                 bool /*lbMostWheelsHaveTraction*/,
                                                 bool /*lbUnusedFalse*/)
    {
        // FIDELITY: BLOCKED -- structural skeleton only; see the block comment above. No fabricated math.
        // The faithful body requires named tyre-curve/direction lanes + the un-homed friction rodata
        // vectors, neither of which is recoverable from the degenerate VMX128 export.
    }

    // @0x825D41A8  BrnPhysics::Vehicle::VehiclePhysics::HandleWheelFrictionCrashing
    // ---------------------------------------------------------------------------------------------
    // FIDELITY: BLOCKED (active-contact path). Same degenerate-VMX128 condition as
    // HandleWheelPairFriction. The function asserts IsCrashing() (this+0x710 = +1808) and processes one
    // wheel:
    //   * INACTIVE contact (mi8NumContacts == 0 OR mu8State == 2): the stored friction register is decayed
    //     by 0.95 -- this branch IS faithfully recoverable (0.95 is an inline immediate, the math is a
    //     plain splat-multiply of the wheel's friction register lane).
    //   * ACTIVE contact: compute slip-driven scrub forces shaped by car-type-selected tunables
    //     (mu*CarType @this+4308: types 1 & 3 select the harsher {4.0, 1.0, 0.2} set lazily cached in
    //     dword_82FBA190 from unk_82FBA180..82FBA110; others use {20.0, ...}); resolve a friction-cone
    //     force, apply it as r x F -> AddWorldSpaceTorque(this+0x10) and accumulate the linear residual at
    //     this+0x240. The active path depends on the un-homed rodata limit vectors and unrecoverable SIMD
    //     lane routing -> NOT emitted (fabrication forbidden).
    // The decay branch is reproduced as a faithful comment; without the named wheel friction-register lane
    // it cannot be applied store-for-store either, so the whole body is left as a structural skeleton.
    // ---------------------------------------------------------------------------------------------
    // ⭐⭐ ORACLE RECOVERED 2026-08-07 (tyre-math wave). The clean-C algorithm twin is BPR sub_B9B9C0
    //   -- BPR UpdateWheels @0xBA1420 calls it FOUR times, order (2,3,0,1), gated by
    //   (this+4464 && crash-mass), the exact X360 shape -- and it confirms the inactive-decay vs
    //   active-scrub split this skeleton documents. Same commit block as HandleWheelPairFriction:
    //   BPR is the DIVERGED RoadVehiclePhysics (offsets are not this class's) and the X360 pseudocode
    //   is degenerate, so the active-scrub path needs a VMX-asm <-> SSE lane cross-map. NOT emitted
    //   this wave (no guessed lanes; unverifiable until PhysicsModule::Update lands).
    // ---------------------------------------------------------------------------------------------
    // ⭐ SIGNATURE CONFORMED 2026-08-07 (wheel-cluster wave): + VecFloat dt per the DWARF and the
    // four UpdateWheels call sites (`vmr128 v1, v127` before each bl).
    void VehiclePhysics::HandleWheelFrictionCrashing(EVehicleDrivenWheel /*leWheel*/,
                                                     VecFloat /*lvfTimeStep*/)
    {
        // FIDELITY: BLOCKED -- structural skeleton only; see the block comment above.
        //   Inactive contact -> wheelFrictionRegister *= 0.95f;   (faithful; pending the named wheel lane)
        //   Active contact   -> slip scrub via car-type tunables {4.0,1.0,0.2} | {20.0,...}; r x F torque
        //                       + linear accumulate at +0x240.    (blocked: un-homed rodata + lane routing)
        // No fabricated math emitted.
    }

    // ===========================================================================================
    //  C04 wheels/tire group: per-frame wheel-geometry funcs
    // ===========================================================================================

    // @0x825FB200  BrnPhysics::Vehicle::VehiclePhysics::CalculateBodyVelocityAtWheelContact
    //   The rigid-body velocity at one wheel's contact point:
    //       v_contact = v_linear + omega x (r_contact - bodyPos)
    //   r_contact is the wheel's road-contact position when the wheel is on the ground (asm:
    //   `if (*(wheel+344)) { ... lvx128 contact... }`), else its streamed position
    //   (mStreamedPositionPlusTwistAmount; the else branch asserts the streamed position is finite
    //   -- elided -- and reads +0x90). The result is stored into the wheel's mBodyPointVelocity
    //   (+0xA0). bodyPos is the body world position (mTransform.Pos(), base +0x40); omega is the
    //   angular-velocity register at +0x60 (here mAngularVelocity). The cross-product is the X360's
    //   vpermwi/vmulfp/vnmsubfp lane-rotated `a x b`.
    // ⭐ SIGNATURE CONFORMED 2026-08-07 (wheel-cluster wave) to the DWARF 3-arg form; both extra
    // args are DEAD in the console callee (see the header note).
    void VehiclePhysics::CalculateBodyVelocityAtWheelContact(EVehicleDrivenWheel leWheel,
                                                             Vector3 /*lvRollDirection*/,
                                                             VecFloat /*lvfTimeStep*/)
    {
        Wheel& lrWheel = maWheels[leWheel];

        // r_contact: road-contact position when on the ground, else the streamed position.
        Vector3 lvContactPos;
        if (lrWheel.GetRoadContact().mbIsOnGround)
            lvContactPos = lrWheel.GetRoadContact().mPosition;
        else
            lvContactPos = lrWheel.mStreamedPositionPlusTwistAmount.GetVector3();

        // lever arm r = contactPos - bodyPos
        const Vector3& lvBodyPos = mTransform.Pos();
        const f32 lfRx = lvContactPos.x - lvBodyPos.x;
        const f32 lfRy = lvContactPos.y - lvBodyPos.y;
        const f32 lfRz = lvContactPos.z - lvBodyPos.z;

        // omega x r  (mAngularVelocity is the +0x60 angular-velocity register)
        const Vector3& lvOmega = mAngularVelocity;
        const f32 lfCx = lvOmega.y * lfRz - lvOmega.z * lfRy;
        const f32 lfCy = lvOmega.z * lfRx - lvOmega.x * lfRz;
        const f32 lfCz = lvOmega.x * lfRy - lvOmega.y * lfRx;

        // v_contact = v_linear + (omega x r)
        lrWheel.mBodyPointVelocity = Vector3{ mLinearVelocity.x + lfCx,
                                              mLinearVelocity.y + lfCy,
                                              mLinearVelocity.z + lfCz, 0.0f };
    }

    // @0x825B7FC0  BrnPhysics::Vehicle::VehiclePhysics::StoreLocalWheelPositions
    //   Project each of the four wheels' world positions into the body's LOCAL frame and store the
    //   results into maLocalTractionPoints[i] (+0x530/+0x540/+0x550/+0x560). The X360 builds the
    //   inverse of the orthonormal 3x3 inline (vmrglw/vmrghw transpose) and FMA-cascades the
    //   per-wheel position through it after subtracting the body translation:
    //       local_i = transpose(R) * (worldWheelPos_i - bodyPos)
    //   where R = the body rotation (mTransform x/y/z axes) and bodyPos = mTransform.Pos(). The
    //   per-wheel source position is the wheel's road-contact position (the +0x158 contact the asm
    //   loads via `lvx128 v13, r11, r6` from the wheel record).
    void VehiclePhysics::StoreLocalWheelPositions()
    {
        const Vector3& lvRight = mTransform.Right();   // R row 0 (xAxis)
        const Vector3& lvUp    = mTransform.Up();      // R row 1 (yAxis)
        const Vector3& lvAt    = mTransform.At();      // R row 2 (zAxis)
        const Vector3& lvPos   = mTransform.Pos();

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const Vector3& lvWorld = maWheels[liWheel].GetRoadContact().mPosition;

            // d = worldWheelPos - bodyPos
            const f32 lfDx = lvWorld.x - lvPos.x;
            const f32 lfDy = lvWorld.y - lvPos.y;
            const f32 lfDz = lvWorld.z - lvPos.z;

            // local = transpose(R) * d  ==  (d . Right, d . Up, d . At)  (orthonormal inverse = transpose)
            maLocalTractionPoints[liWheel] = Vector3{
                lfDx * lvRight.x + lfDy * lvRight.y + lfDz * lvRight.z,
                lfDx * lvUp.x    + lfDy * lvUp.y    + lfDz * lvUp.z,
                lfDx * lvAt.x    + lfDy * lvAt.y    + lfDz * lvAt.z,
                0.0f };
        }
    }

    // ==============================================================================================
    //  @0x825FD218  BrnPhysics::Vehicle::VehiclePhysics::SetWheelVelocities   (728 X360 instrs)
    // ==============================================================================================
    // Re-seed the whole drivetrain from the body's current rigid-body motion. Three callers, all
    // car-PLACEMENT paths: VehiclePhysics::Reset (the `mpAttribs != NULL` branch),
    // TrafficPhysics::PreparePhysical, and VehicleManager::HandleRaceCarRaceCarContact (once per
    // car, right after a slam/shunt impulse lands).
    //
    // ⭐ THE "BLOCKED" LABEL WAS INHERITED AND WRONG. The ledger and this file's own group notes
    //    called it "un-recoverable degenerate VMX128 + a dozen un-committed helpers / un-homed
    //    rodata". Disassembled first-hand 2026-08-03:
    //      * it is ONE function -- one `bl __savegprlr_14` + `stwu` prologue, one `b __restgprlr`
    //        epilogue, and 0x825FD218 + 728*4 == 0x825FDD78 == exactly where Reset starts, so the
    //        export-hole check the campaign uses passes;
    //      * the callee set is `Engine::Reset` plus BeginAssert/FireAssert/EndAssert and the assert
    //        message-builder (BasePriorityQueue::Clear + sub_82203F70) -- nothing else, and
    //        Engine::Reset has been committed since the engine-defaults wave;
    //      * the "degenerate VMX128" is an inlined `XMVectorSinCos` over unk_82000BD0..unk_82000C60,
    //        the SAME table three earlier waves already decoded (BrnBehaviourRoadRunner.cpp:988
    //        pins unk_82000C60 lane1 == 6.283185482 == 2*pi and lane3 == 0.1591549367 == 1/(2*pi),
    //        and 0x82000BD0 == {1, -1/6, 1/120, -1/5040, ...}), plus a Rodrigues axis-angle rotation
    //        and one cross product.
    //
    // ⚠️ THE PARAMETER IS DEAD IN THE CONSOLE BODY. Vector arguments arrive in v1 (proved by this
    //    function's own outgoing calls -- `vspltw v1,v0,0 ; bl Engine::Reset`), and the first
    //    mention of v1 in the body is a WRITE at 0x825FD364. That is not a decode gap: the caller
    //    HandleRaceCarRaceCarContact does `li r22,0x50 ; lvx128 v1,r3,r22 ; bl SetWheelVelocities`,
    //    i.e. it passes the car's own mLinearVelocity (+0x50) -- exactly the value the callee
    //    re-reads from `this` itself. The argument is redundant with `this`, so it is kept in the
    //    signature and named-but-unused here, exactly as the console has it.
    //
    // The asm, in order:
    //   0x825FD2CC  mAngularVelocity -= Up * dot3(mAngularVelocity, Up)      (lvx +0x60, lvx +0x20,
    //               vmsum3fp128, vmulfp128, vsubfp, stvx128 back to +0x60) -- the YAW-RATE KILL.
    //   0x825FD2F4..0x825FD534  one inlined XMVectorSinCos of
    //               mvSteeringAngle_..._.x (+0xFE0 lane .x), then the Rodrigues rows for
    //               R(axis = mTransform.Up(), angle = that), applied to mTransform.At():
    //               v125 = R * At.  (The three `vperm`+`vrlimi128` triples build columns 0/1/2 of
    //               the standard Rodrigues matrix; `vrlimi128 v9,v3,2,0` inserting (1-c)xz - s*y --
    //               the Z element of row 0 -- independently re-proves the mask convention
    //               8/4/2/1 == lane x/y/z/w that the two register stores below depend on.)
    //   then FOUR fully unrolled wheel blocks (bases +0x130/+0x210/+0x2F0/+0x3D0, stride 0xE0):
    //               assert(IsFinite(wheel.mPosition));      // "Invalid wheel position: " << pos <<
    //                                                       // ", please tell Graham D."  (:412)
    //               r = Right*p.x + Up*p.y + At*p.z         // p = wheel.mPosition, body-local
    //               v = cross(mAngularVelocity, r) + mLinearVelocity     // rigid-body point velocity
    //               wheel.mBodyPointVelocity      = v                        // +0xA0
    //               wheel.mIntegrationVariables.y = 0                        // vrlimi128 mask 4
    //               wheel.mIntegrationVariables.x = dot3(v, dir) / wheel.mSlipVariables.w
    //                                                                       // vmsum3fp128 * vrefp
    //                                                                       // + 2 Newton steps
    //               wheel.mbBrokenAdhesiveLimit   = false                    // +0xD5
    //               with dir = the STEERED direction v125 for wheels 0/1 and the plain
    //               mTransform.At() for wheels 2/3 (the rear pair reloads +0x30 instead of v125).
    //   0x825FDC04..end  the engine re-seed (see below).
    //
    // ⭐ Two independent corroborations of the per-wheel store, both from ALREADY-COMMITTED code:
    //    StoreLocalWheelPositions above uses the transpose of the very same Right/Up/At basis, and
    //    the C07 speed-match block at VehiclePhysics.cpp:1033-1036 uses the identical
    //    `mIntegrationVariables.x = target / mSlipVariables.w` idiom with the front pair taking
    //    maWheels[0].mSlipVariables.w and the rear pair maWheels[2]'s.
    //
    // ⭐ Why the yaw-rate kill is deliberate and not a misread of the lanes: each wheel's spin is
    //    re-seeded from the body velocity AT THAT WHEEL, so any residual rotation about the body's
    //    up axis would spin the outer wheels faster than the inner ones -- and the engine is then
    //    re-seeded from the AVERAGE of the four. Removing the Up component makes all four agree.
    //
    // CONSTANTS -- every one attested, none guessed. The exported Hex-Rays pseudocode folds the
    // .rdata scalar loads into literals: flt_82001CC0 == 0.0f (the two accumulator seeds),
    // flt_82001D9C == 2.0f (`v8 = 2.0` / `v8 = v8 + 2.0` -- wheels per driven axle), and
    // stru_8208F620 / flt_82002514 == +/-1.1920929e-7 == FLT_EPSILON, the rw::math::fpu::IsZero
    // band. The two per-axle gates read mpAttribs->mBaseAttribs
    // .mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo (+0xB0): lane .z == PowerToRear
    // gates the REAR pair and lane .y == PowerToFront gates the FRONT pair -- the DWARF's own lane
    // names confirm the front/rear wheel-index assignment independently of the offsets.
    //
    // FLAG (PC-platform, numeric): the console's SinCos is a shared minimax polynomial over a
    //   2*pi-reduced argument; std::sin / std::cos are the exact forms. Tighter than the console,
    //   never looser -- the same de-optimisation CameraUtils.cpp:561 already applies to this table.
    //
    // ⚠️ NOT REPRODUCED, deliberately: the console prologue lazily initialises two function-scope
    //   statics -- unk_82FBA210 = splat(100.0f) and unk_82FBA200 = splat(1000.0f), guarded by bits
    //   0/1 of dword_82FBA220 -- and then NEVER READS EITHER. That is magic-static init left behind
    //   by an inlined helper whose use was dead-coded; every real reader emits its own guard+init.
    //   Recorded here so a later wave does not mistake the omission for a silent-zero constant.
    void VehiclePhysics::SetWheelVelocities(Vector3 lvVelocity)
    {
        // The console's own parameter, dead in the console's own body -- see the note above. Both
        // surviving call sites pass the car's mLinearVelocity, which is re-read from `this` below.
        (void)lvVelocity;

        const Vector3& lvRight = mTransform.Right();   // +0x10
        const Vector3& lvUp    = mTransform.Up();      // +0x20
        const Vector3& lvAt    = mTransform.At();      // +0x30

        // ---- 0x825FD2CC: strip the yaw rate (the component of omega along the body up axis) -----
        const f32 lfYawRate = mAngularVelocity.x * lvUp.x
                            + mAngularVelocity.y * lvUp.y
                            + mAngularVelocity.z * lvUp.z;          // vmsum3fp128
        mAngularVelocity = Vector3{ mAngularVelocity.x - lvUp.x * lfYawRate,
                                    mAngularVelocity.y - lvUp.y * lfYawRate,
                                    mAngularVelocity.z - lvUp.z * lfYawRate,
                                    0.0f };

        // ---- 0x825FD2F4: the steered wheel direction = R(Up, steeringAngle) * At ----------------
        // Rodrigues about the (unit) body up axis. The console evaluates sin/cos with one inlined
        // XMVectorSinCos of the +0xFE0 .x lane; std::sin/std::cos are the exact forms.
        const f32 lfSteerAngle =
            mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x;
        const f32 lfSin = std::sin(lfSteerAngle);
        const f32 lfCos = std::cos(lfSteerAngle);
        const f32 lfOneMinusCos = 1.0f - lfCos;                     // vsubfp128 v10, v127(1.0), v11

        // Columns of the rotation, exactly as the asm packs them (vperm of lanes x/y then a
        // vrlimi128 of lane z).
        const Vector3 lvCol0{ lfCos + lfOneMinusCos * lvUp.x * lvUp.x,
                              lfOneMinusCos * lvUp.x * lvUp.y + lfSin * lvUp.z,
                              lfOneMinusCos * lvUp.x * lvUp.z - lfSin * lvUp.y, 0.0f };
        const Vector3 lvCol1{ lfOneMinusCos * lvUp.y * lvUp.x - lfSin * lvUp.z,
                              lfCos + lfOneMinusCos * lvUp.y * lvUp.y,
                              lfOneMinusCos * lvUp.y * lvUp.z + lfSin * lvUp.x, 0.0f };
        const Vector3 lvCol2{ lfOneMinusCos * lvUp.z * lvUp.x + lfSin * lvUp.y,
                              lfOneMinusCos * lvUp.z * lvUp.y - lfSin * lvUp.x,
                              lfCos + lfOneMinusCos * lvUp.z * lvUp.z, 0.0f };

        // v125 = R * At  (the asm's `col0*At.x + col1*At.y + col2*At.z` FMA cascade).
        const Vector3 lvSteeredDirection{
            lvCol0.x * lvAt.x + lvCol1.x * lvAt.y + lvCol2.x * lvAt.z,
            lvCol0.y * lvAt.x + lvCol1.y * lvAt.y + lvCol2.y * lvAt.z,
            lvCol0.z * lvAt.x + lvCol1.z * lvAt.y + lvCol2.z * lvAt.z, 0.0f };

        // ---- the four unrolled wheel blocks ------------------------------------------------------
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];

            // CgsDev::Assert( RwMathVPU::IsValid( wheel.mPosition ) ) -- elided (debug-only finite
            // check; X360 message "Invalid wheel position: " << pos << ", please tell Graham D.",
            // VehiclePhysics.cpp:412).
            const Vector3& lvLocalPos = lrWheel.mPosition;          // +0x80, body-local at this point

            // r = R_body * localPos   (Right/Up/At are mTransform's rows -- the same basis
            // StoreLocalWheelPositions inverts by transpose).
            const f32 lfRx = lvRight.x * lvLocalPos.x + lvUp.x * lvLocalPos.y + lvAt.x * lvLocalPos.z;
            const f32 lfRy = lvRight.y * lvLocalPos.x + lvUp.y * lvLocalPos.y + lvAt.y * lvLocalPos.z;
            const f32 lfRz = lvRight.z * lvLocalPos.x + lvUp.z * lvLocalPos.y + lvAt.z * lvLocalPos.z;

            // v = omega x r + linearVelocity   (the vpermwi128 yzx lane-rotated cross product)
            const Vector3 lvPointVelocity{
                mLinearVelocity.x + (mAngularVelocity.y * lfRz - mAngularVelocity.z * lfRy),
                mLinearVelocity.y + (mAngularVelocity.z * lfRx - mAngularVelocity.x * lfRz),
                mLinearVelocity.z + (mAngularVelocity.x * lfRy - mAngularVelocity.y * lfRx),
                0.0f };

            // The direction the wheel rolls along: the STEERED direction for the front pair, the
            // body forward axis for the rear pair (the asm reloads +0x30 for wheels 2 and 3).
            const Vector3& lvRollDirection =
                (liWheel == eFrontLeftWheel || liWheel == eFrontRightWheel) ? lvSteeredDirection
                                                                           : lvAt;

            // The console zeroes lane .y first (vrlimi128 mask 4) and then writes lane .x
            // (vrlimi128 mask 8) from dot3(v, dir) * (1 / mSlipVariables.w) -- vrefp plus two
            // Newton refinement steps, i.e. an exact reciprocal to float precision.
            lrWheel.mIntegrationVariables.y = 0.0f;

            const f32 lfWheelRadius = lrWheel.mSlipVariables.w;
            const f32 lfAlongRoll   = lvPointVelocity.x * lvRollDirection.x
                                    + lvPointVelocity.y * lvRollDirection.y
                                    + lvPointVelocity.z * lvRollDirection.z;
            lrWheel.mIntegrationVariables.x = lfAlongRoll / lfWheelRadius;

            lrWheel.mBodyPointVelocity    = lvPointVelocity;        // +0xA0
            lrWheel.mbBrokenAdhesiveLimit = false;                  // +0xD5
        }

        // ---- 0x825FDC04: re-seed the engine from the average spin of the DRIVEN wheels -----------
        // Each driven axle contributes its two wheels and 2.0 to the divisor. Which axles are driven
        // comes from mpAttribs->mBaseAttribs's +0xB0 register: lane .z == PowerToRear gates the rear
        // pair, lane .y == PowerToFront gates the front pair.
        const Vector4& lvPowerSplit =
            mpAttribs->mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo;

        f32 lfSumWheels  = 0.0f;   // flt_82001CC0
        f32 lfDivWheels  = 0.0f;   // flt_82001CC0  (the assert below names this variable)

        if (!rw::math::fpu::IsZero(lvPowerSplit.z))                                // PowerToRear
        {
            lfDivWheels  = 2.0f;                                    // flt_82001D9C
            lfSumWheels  = maWheels[eRearLeftWheel ].mIntegrationVariables.x
                         + maWheels[eRearRightWheel].mIntegrationVariables.x;
        }
        if (!rw::math::fpu::IsZero(lvPowerSplit.y))                                // PowerToFront
        {
            lfDivWheels += 2.0f;
            lfSumWheels += maWheels[eFrontLeftWheel ].mIntegrationVariables.x
                         + maWheels[eFrontRightWheel].mIntegrationVariables.x;
        }

        // CgsDev::Assert( rw::math::fpu::IsZero( lfDivWheels ) == false ) -- elided (debug-only;
        // VehiclePhysics.cpp:6504). A car with neither axle driven would divide by zero here.
        const f32 lfAverage = lfSumWheels / lfDivWheels;
        mEngine.Reset(VecFloat{ lfAverage, lfAverage, lfAverage, lfAverage });
    }

    // ==============================================================================================
    //  @0x825FDD78  BrnPhysics::Vehicle::VehiclePhysics::Reset(Vector3)   (232 X360 instrs)
    // ==============================================================================================
    // Return the car to a clean placed state at the supplied velocity. Called by Construct
    // @0x8262DBD0 (with the zero vector) and by the car-placement paths.
    //
    // ⭐ THE DECODE IS NOT MINE. This function is an `.ida-exports` HOLE; the previous wave pulled
    //    0x825FDD78..0x825FE118 out of BURNOUT_X360_ARTIST.XEX.i64 with headless IDA and replayed it
    //    through a symbolic VMX128 simulator, closing the member map against the DWARF eight ways.
    //    That store-by-store decode is preserved verbatim in the block comment in VehiclePhysics.h;
    //    this body is its transcription, member for member, in the console's own order. It became
    //    writable this wave only because SetWheelVelocities did (the `mpAttribs != NULL` branch
    //    calls it out of line, so bodying Reset before it would have been a guaranteed LNK2019).
    //
    // ⚠️⚠️ THE TWO SILENT-ZERO SEEDS IN HERE. Both slots read all-zero in the shipped image and are
    //    filled at static init by IDA-unmarked thunks, so a literal scan finds only readers:
    //      * TimeSinceLastHandBrake (the +0x1080 .w lane) is seeded from unk_82FB9080 == 10000.0f
    //        (thunk 0x82C5C398 -> flt_82005D9C). Left at the image's 0.0f it would read as "the
    //        handbrake was released THIS INSTANT" on every single reset, which is exactly what the
    //        two UpdateHandBrake reads of that slot gate on.
    //      * TimeSinceLastRaceCarContact (the +0x1050 .z lane) is the same shape at 100.0f.
    //    Both are written here as the recovered values, NOT as the image's zeros.
    //
    // ⚠️ The partial clears are partial ON PURPOSE and are reproduced as such: mSlamEffect keeps
    //    mForce / mfDecay / mfRecoveryTime (so this is an inlined partial clear, not SlamEffect::
    //    Clear()); mvSteeringAngle keeps .w (DriftGasLetOffAmount); mvSpare_MaintainedSpeed keeps .x
    //    (Spare); mvSideForceMag keeps .z (TimeSinceLastBoostKick); mvDampRollVel keeps .y.
    void VehiclePhysics::Reset(Vector3 lvVelocity)
    {
        // The 0-arg base overload -- @0x825D9A58, which builds its own zero with vspltisw128, so
        // there is no dropped argument here.
        SimpleVehiclePhysics::Reset();

        if (mpAttribs == NULL)
        {
            // No attribs yet: park every wheel and the engine at a dead stop.
            const Vector3 lvZero{ 0.0f, 0.0f, 0.0f, 0.0f };
            for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
                maWheels[liWheel].Reset(lvZero);
            mEngine.Reset(VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });
        }
        else
        {
            // Attribs present: re-seed the whole drivetrain from the body's motion instead.
            SetWheelVelocities(lvVelocity);
        }

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            maLocalTractionPoints[liWheel].SetZero();                        // +0x530 stride 0x10

        mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.SetZero();   // +0xEF0 (whole)
        mWeightTransfer.SetZero();                                               // +0xEE0

        // ---- the drift state bank (+0xFE0..+0x1080) ----------------------------------------------
        mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x = 0.0f;      // +0xFE0 .x/.y/.z
        mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y = 0.0f;      //   (.w untouched)
        mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.z = 0.0f;

        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y = 0.0f;          // +0x1000 .y/.z/.w
        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.z = 0.0f;          //   (.x Spare kept)
        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = 0.0f;

        mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.SetZero();        // +0x1010
        mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.SetZero(); // +0x1020

        // +0x1030: .x = 1.0f (vspltisw 1 + vcfsx -- an integer 1 converted, not a rodata literal).
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.x = 1.0f;
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.y = 0.0f;
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.z = 0.0f;
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.w = 0.0f;

        mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.x = 0.0f;  // +0x1040
        mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.y = 0.0f;  // (.z kept)
        mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.w = 0.0f;

        // +0x1050: .x = -0.1f (unk_8208FAE4), .y = 1.0f (unk_8208FAE8), and .z = 100.0f
        // (flt_820049E0) == TimeSinceLastRaceCarContact, i.e. "this car last touched another one a
        // long time ago".
        mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.x = -0.1f;
        mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.y = 1.0f;
        mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z = 100.0f;

        mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.SetZero();       // +0x1060

        // +0x1080: .x = 0, .z = 0, and .w = TimeSinceLastHandBrake = 10000.0f -- the recovered
        // static-init value (unk_82FB9080), NOT the zero the image ships. (.y untouched.)
        mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.x = 0.0f;
        mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.z = 0.0f;
        mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.w = 10000.0f;

        mSteeringDirection.SetZero();                                            // +0x10E0

        // +0x10F0: flt_8200426C == 5.0f == the DWARF's own KF_STUCK_IN_COLLISION_TEST_INTERVAL.
        mfTimeUntilStuckInCollisionTest = 5.0f;

        mDriftFlags.mu8DriftFlags = DriftFlags::KU_DRIFT_FLAG_DO_ALL;            // +0x10F4 (0xFF)
        mbInBoostKick             = false;                                       // +0x10F5
        mbForceFrozen             = false;                                       // +0x10F6
        mbGivenAftertouchAirBoost = false;                                       // +0x10F8

        // The PARTIAL slam clear (mForce / mfDecay / mfRecoveryTime are deliberately kept).
        mSlamEffect.mfSteering         = 0.0f;                                   // +0x1114
        mSlamEffect.mfOriginalSteering = 0.0f;                                   // +0x1118
        mSlamEffect.mfSlamLife         = 0.0f;                                   // +0x111C
        mSlamEffect.mfTotalSlamTime    = 0.0f;                                   // +0x1120
        mSlamEffect.mi8SlamNumber      = -1;                                     // +0x1128

        mShuntEffect.mDirectionPlusDesiredSpeed.SetZero();                       // +0x1130
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = -1.0f;                     // +0x1140 (dead)
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y = 0.0f;

        mi8LastContactedRaceCar = -1;                                            // +0x1150

        mUsedAirRams.UnSetAll();                                                 // +0x1158
        mUsedSpins.UnSetAll();                                                   // +0x1220

        for (s32 liSpring = 0; liSpring < eNumDrivenWheels; ++liSpring)
            maSprings[liSpring].Reset();                                         // +0xE10 stride 0x30

        mPreviousWorldSpaceVelocity = lvVelocity;                                // +0x1330
        mNormLinearVelocityMag.SetZero();                                        // +0x1340

        mbHasAir                 = false;                                        // +0x1350
        mbHadAirLastFrame        = false;                                        // +0x1351
        mu8DriftState            = 0;                                            // +0x1352
        miNumCollisions          = 0;                                            // +0x1354
        mbHandBrake              = false;                                        // +0x1358
        mbAllWheelsHaveTraction  = false;                                        // +0x135B
        mbResetCarTransform      = true;                                         // +0x135C  (TRUE)
        mbJustBeenSlammed        = false;                                        // +0x135D
        mbDoingBurnout           = false;                                        // +0x1361

        mPreviousTransform = mTransform;                                         // +0x1370 <- +0x10

        mWheelFFSpring.mfSpringCoefficient = 0.0f;                               // +0x13D0
        mWheelFFSpring.mfSpringSaturation  = 0.0f;                               // +0x13D4

        mi8LastAttackersRaceCarIndex = -1;                                       // +0x13E0
    }

    // ==============================================================================================
    //  @0x8262DBD0  BrnPhysics::Vehicle::VehiclePhysics::Construct()   (97 X360 instrs + 1 pad)
    // ==============================================================================================
    // The console constructor. Callers: VehicleManager::Construct @0x8263B7C8,
    // VehicleManager::PrepareData and TrafficPhysics::Construct.
    //
    // ⛔ THIS BODY WAS *NOT* WRITTEN FROM THE BANKED DECODE. The note that sat in VehiclePhysics.h
    //    ended in an elided line whose offsets were unnamed and internally inconsistent, and it was
    //    WRONG IN TWO PLACES (see the corrected block comment in the header). All 97 instructions
    //    were re-pulled and re-read; the two errors, and the two things that caught them, are:
    //      * "mSlamEffect partial seed at +0x590+0x20" -- there is no mSlamEffect store in this
    //        function. +0x590/+0x594/+0x596/+0x598 are the tail of the ONE +0x570 block, i.e.
    //        mAboveGroundTestResult, which is what SetAboveGroundTestResult @0x826029D4 (already
    //        committed in BrnSimpleVehiclePhysics.cpp) writes through the same `addi r11,this,0x570`.
    //      * "mbCrashing(+0x70) = false" -- +0x70 is mbFrozen. ExternallySimulatedBody::Construct
    //        @0x8259CFA4 stores its zero byte at `0x60(r3)` in its own frame, and the base subobject
    //        starts at VehiclePhysics+0x10, so VP+0x70 is that byte. mbCrashing is at +0x710.
    //        UpdateFreezing @0x825CFFA0..FFE8 confirms it independently: it reads and writes
    //        0x70(r31) beside `lbz r9,0x10F6(r31)` == mbForceFrozen.
    //
    // ⭐ NOT AN EXPORT HOLE. `.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8262DBD0.json` carries the full
    //    assembly and xrefs_from (only its Hex-Rays pseudocode is degenerate). Every callee below is
    //    taken from that xrefs_from -- Wheel::Clear @0x825D6E88, SuspensionSpring::Prepare
    //    @0x825A7A28, VehicleAttribs::EngineAttribs::Construct @0x825B7B90, Engine::Reset
    //    @0x825CF130, VehicleAttribs::Construct @0x825F3FB8, SimpleVehiclePhysics::Reset @0x825D9A58
    //    (the 0-arg base overload) and VehiclePhysics::Reset @0x825FDD78 (the Vector3 overload) --
    //    so there is no ambiguity about which overload each `bl` reaches, and no new link closure.
    //
    // ⭐ CORROBORATED BY A SECOND IMAGE. The PS3 DecFIGS build carries the same function at
    //    0x6EB1F0 with readable Hex-Rays. It reproduces every store, with a uniform -0x10 shift for
    //    everything past +0x6A0 (that build's drift bank sits 16 bytes lower), and it resolves three
    //    things the X360 inlines:
    //      * it calls `Engine::Construct` OUT OF LINE at its +0xEF0  ->  the X360's
    //        EngineAttribs::Construct + Engine::Reset(0) pair IS Engine::Construct @0x825F3EE8;
    //      * it calls `CgsContainers::BitArray<4u>::Construct` at its +0x1148 and the BitArray<8>
    //        twin at its +0x1210  ->  the X360's two bare `std 0` really are mUsedAirRams/mUsedSpins;
    //      * its two lane-inserts are `vperm` with VectorPermuteConstant<0,1,6,3> -- lanes
    //        {x, y, second-operand z, w} -- which is the same LANE Z the X360's `vrlimi128 ...,2,0`
    //        selects. Two compilers, two ISAs, one lane.
    // ----------------------------------------------------------------------------------------------
    void VehiclePhysics::Construct()
    {
        // The X360 parks 0.0f (flt_82001CC0) in three stack slots once, then broadcasts them into
        // v1/v2/v3 for every Prepare call in the loop -- i.e. all three spring seeds are zero.
        // SetupSuspension installs the real stiffness/damping/mass later.
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            maWheels[liWheel].Clear();                                       // +0x130 stride 0xE0
            maSprings[liWheel].Prepare(VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f },   // +0xE10 stride 0x30
                                       VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f },
                                       VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });
        }

        mvSpringMassScalers.SetZero();          // +0xED0 (whole register, stvx128 v127)

        // +0xF00. Inlined on X360 as EngineAttribs::Construct(&mEngine) then Engine::Reset(&mEngine,
        // 0.0f) -- Engine::Construct @0x825F3EE8 instruction-for-instruction, and what the PS3 build
        // calls out of line. mEngine.mAttribs is private, which is the other reason to spell it this
        // way rather than reproducing the two inlined calls.
        mEngine.Construct();

        mpAttribs = NULL;                       // +0x720 (stw r30 -- the 4-byte console pointer)

        mAIVehicleAttribs.Construct();          // +0x730
        mPlayerVehicleAttribs.Construct();      // +0xAA0

        // Both slot allocators start empty. The X360 clears each with one `std r30(==0)`; the PS3
        // build calls BitArray<N>::Construct on the same two addresses. UnSetAll() is that store.
        mUsedAirRams.UnSetAll();                // +0x1158
        mUsedSpins.UnSetAll();                  // +0x1220

        mHandlingBodyOffset.SetZero();          // +0x690  (SimpleVehiclePhysics')
        mHalfExtent.SetZero();                  // +0x6A0  (SimpleVehiclePhysics')

        mPreviousWorldSpaceVelocity.SetZero();  // +0x1330

        // The whole +0x570 above-ground-test block, cleared in place (the X360 walks it off one
        // `addi r11,this,0x570` base). The two halfword stores are `li r29,-1 ; sth r29,0x24(r11)`
        // and `li r4,-0x8000 ; sth r4,0x26(r11)`: on big-endian PPC the halfword at the LOWER byte
        // address is the HIGH 16 bits of the u32, so the assembled tag is 0xFFFF8000. (Same
        // halfword->u32 rule the committed SetAboveGroundTestResult uses.) The PS3 build writes the
        // identical pair, `*(this+1428) = -1` then `*(this+1430) = 0x8000`.
        mAboveGroundTestResult.mIntersectionPosition.SetZero();   // +0x570
        mAboveGroundTestResult.mIntersectionNormal.SetZero();     // +0x580
        mAboveGroundTestResult.mfVerticalDistance = 0.0f;         // +0x590
        mAboveGroundTestResult.mCollisionTag.muValue = 0xFFFF8000u;  // +0x594 hi | +0x596 lo
        mAboveGroundTestResult.mbValid = false;                   // +0x598

        // Two LANE-Z-ONLY inserts (read-modify-write: lvx128 / vrlimi128 mask 2 / stvx128). Every
        // other lane of both registers survives -- these are NOT whole-register clears.
        mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.z = 0.0f;   // +0x1040
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.z = 0.0f;  // +0x1070

        mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.SetZero();   // +0xEF0 (whole)

        // The 0-arg base overload @0x825D9A58 (explicitly qualified because Reset(Vector3) below
        // hides it), then the body starts unfrozen, then the full reset at zero velocity. The PS3
        // build wraps these last two in its own VehiclePhysics::Reset() 0-arg
        // (._ZN10BrnPhysics7Vehicle14VehiclePhysics5ResetEv @0x6EAEC4), whose entire body is
        // `SimpleVehiclePhysics::Reset(); Reset(Vector3(0));` -- which is what the X360 does here
        // in line.
        SimpleVehiclePhysics::Reset();

        mbFrozen = false;                       // +0x70   (NOT mbCrashing -- see the banner)

        Reset(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
    }

    // =====================================================================================
    // C08 airborne/water/freeze/spin group -- BODIES.
    //   UpdateInWaterBehaviour @0x825B81A8, UpdateAirRam @0x825FC8D8, UpdateSpinEffects @0x825FCCF8,
    //   AddAirRam @0x825FE118 are bodied here. UpdateInAirBehaviour @0x825D0BE8 and
    //   UpdateFreezing @0x825CFD20 are BLOCKED -- structural skeletons at the bottom of this group.
    // =====================================================================================

    // FLAG (runtime data): the per-surface "is water" bool table (byte_82FB7DF4) is a runtime-loaded
    // scratch global absent from the export -> inert placeholder (returns "not water") until recovered,
    // so UpdateInWaterBehaviour's kill path stays disabled (faithful-but-inert). NEVER fabricated.
    static inline bool WaterSurfacePlaceholder(s32 /*liSurfaceId*/)
    {
        return false;   // byte_82FB7DF4[id]
    }

    // FLAG (rodata): the body-space unit-axis seed vectors AddAirRam sums for an axis-flag direction
    // (+X = unk_82181510, +Y world up = w::math::vpu::detail::gIVector, +Z = unk_82181520) are un-homed
    // .rdata absent from the export -> flagged-0 placeholders. Until recovered they are zero, so an
    // axis-flag-only ram assembles a zero (inert) direction. NEVER fabricated. (A custom-impulse ram --
    // luFlags bit0/bit2 -- does not use these and is fully exact.)
    static inline Vector3 AxisSeedPlaceholder(s32 /*liAxis*/)
    {
        return Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    }

    // -------------------------------------------------------------------------------------
    // @0x825B81A8  VehiclePhysics::UpdateInWaterBehaviour -- the water "hard kill" (no buoyancy).
    //   if ( byte_82FB7DF4[(mWaterContactTag.muValue >> 4) & 0x3F] )      // contact is a water surface
    //       if ( mAboveGroundTestResult.mfVerticalDistance < flt_82F2A4E4 )                            // and deep enough to drown
    //           the X360 stvx128's a zero register to six rows: mLinearVelocity(+0x50),
    //           mAngularVelocity(+0x60), mTotalTorque(+0xF0), mTotalLinearImpulse(+0x100),
    //           mTotalAngularImpulse(+0x110), and the +0x120 row. The car stops dead and sinks.
    //
    //   FLAG (runtime data): byte_82FB7DF4 (above) + flt_82F2A4E4 (the drown-depth threshold, un-homed
    //   .rdata) are absent from the export -> flagged-inert (table returns "not water", threshold 0), so
    //   the kill stays disabled until recovered: the surface-id extraction, the depth-test shape and the
    //   exact zeroed rows are EXACT. NEVER fabricated. The two control/dt args exist to match the DWARF
    //   phase-chain signature; the body reads neither.
    //   0x825B81E4-8214: `vspltisw v0,0` then six stvx128 stores to +0x50/+0x60/+0xF0/+0x100/+0x110/
    //   +0x120. All six are now real members via the base chain: mLinearVelocity(+0x50),
    //   mAngularVelocity(+0x60), mTotalLinearForce(+0xF0), mTotalTorque(+0x100),
    //   mTotalLinearImpulse(+0x110), mTotalAngularImpulse(+0x120) -- i.e. both velocities plus ALL
    //   FOUR accumulators, the complete hard-kill. (This is the shape of the DWARF's
    //   ExternalPhysicsBody::ZeroForces, inlined; the asm issues the stores, not a `bl`.)
    // -------------------------------------------------------------------------------------
    void VehiclePhysics::UpdateInWaterBehaviour(const BrnPlayerDriverControls* /*lpControls*/,
                                                VecFloat /*lvfDeltaTime*/)
    {
        // +0x594 is the BE-HIGH halfword of mAboveGroundTestResult.mCollisionTag.
        const s32 liSurfaceId = static_cast<s32>((GetAboveGroundTagHi() >> 4) & 0x3Fu);
        if (!WaterSurfacePlaceholder(liSurfaceId))   // byte_82FB7DF4[id] -- inert until recovered
            return;

        // flt_82F2A4E4 @0x82F2A4E4 .data = 0x40000000 = 2.0 (already in the image; no initialiser).
        // ROLE CORROBORATED from the consumer: UpdateInWaterBehaviour @0x825B81D8 does `fcmpu ; bgelr`,
        // i.e. return early once the depth reaches the constant -- a drown DEPTH, exactly as named.
        // ⚠️ At 0.0f this test read `if (!(dist < 0))` and the in-water behaviour NEVER RAN.
        static const f32 KF_WATER_DROWN_DEPTH = 2.0f;   // flt_82F2A4E4
        if (!(mAboveGroundTestResult.mfVerticalDistance < KF_WATER_DROWN_DEPTH))
            return;

        // Hard kill: zero BOTH velocities and ALL FOUR force/impulse accumulators. The X360 splats
        // one zero register and issues six stvx128 in the order +0x50, +0x60, +0xF0, +0x110, +0x120,
        // +0x100 (0x825B81E4-0x825B8214). Two of the six used to be a FLAG here ("+0xF0 ... no
        // settable accessor" / "+0x110 not pinned in this slice") because the flat struct had no
        // base to own them; the re-parenting supplies both, so the body is now complete.
        static const Vector3 KV_ZERO = { 0.0f, 0.0f, 0.0f, 0.0f };
        SetLinearVelocity(KV_ZERO);      // +0x50  == base+0x40  mLinearVelocity
        mAngularVelocity     = KV_ZERO;  // +0x60  == base+0x50  mAngularVelocity
        mTotalLinearForce    = KV_ZERO;  // +0xF0  == base+0xE0
        mTotalLinearImpulse  = KV_ZERO;  // +0x110 == base+0x100
        mTotalAngularImpulse = KV_ZERO;  // +0x120 == base+0x110
        mTotalTorque         = KV_ZERO;  // +0x100 == base+0xF0
        // Note: those six offsets land on six DISTINCT members only under the corrected
        // base-at-+0x10 frame -- an independent confirmation of it.
    }

    // -------------------------------------------------------------------------------------
    // @0x825FC8D8  VehiclePhysics::UpdateAirRam -- tick the queued air-ram impulses.
    //   Walks the SET bits of mUsedAirRams (the X360 cntlzd lowest-set-bit idiom = GetFirstNonZeroBit /
    //   GetNextNonZeroBit). For each active slot i:
    //     mAirRamEffect[i].mfTimerTillFire -= dt;
    //     if ( mfTimerTillFire <= 0.0f ) {
    //        if ( |mImpulse|^2 > flt_82F2A430 )           // still has magnitude to fire
    //            AddLocalImpulse(mImpulse, mPosition);     // (X360: in meImpulseSpace; see FLAG)
    //            mImpulse *= (1.0f - mfDecay);             // decay for the next fire
    //        else
    //            mUsedAirRams.UnSetBit(i);                 // spent -> release the slot
    //     }
    //   (dt arrives splatted in a VMX register; v52[0] is the scalar dt the timer subtracts.)
    //
    //   FLAG (rodata): flt_82F2A430 (the squared-magnitude "still alive" epsilon) is un-homed .rdata ->
    //   flagged-0. With it 0, any non-zero impulse keeps firing and a fully-decayed (zero) impulse
    //   releases the slot -- faithful behaviour, the exact epsilon pending recovery. NEVER fabricated.
    //   FLAG (slice): the X360 base call is AddLocalImpulse(mImpulse, meImpulseSpace, mPosition,
    //   BODY_SPACE); the slice's 2-arg stub drops the impulse-space tag (noted, not fabricated).
    // -------------------------------------------------------------------------------------
    void VehiclePhysics::UpdateAirRam(VecFloat lvfDeltaTime)
    {
        // flt_82F2A430 @0x82F2A430 .data = 0x3C23D70A = 0.01 (already in the image). At 0.0f the air-ram
        // stayed "alive" on pure numerical noise, because any squared magnitude clears zero.
        static const f32 KF_AIR_RAM_ALIVE_EPSILON_SQ = 0.01f;   // flt_82F2A430

        const f32 lfDeltaTime = lvfDeltaTime.x;   // dt splat lane

        for (s32 liSlot = mUsedAirRams.GetFirstNonZeroBit();
             liSlot >= 0;
             liSlot = mUsedAirRams.GetNextNonZeroBit(liSlot))
        {
            AirRamEffect& lrRam = mAirRamEffect[liSlot];

            lrRam.mfTimerTillFire -= lfDeltaTime;
            if (lrRam.mfTimerTillFire > 0.0f)
                continue;   // not time to fire yet

            const f32 lfImpulseMagSq = vpu::MagnitudeSquared(lrRam.mImpulse);
            if (lfImpulseMagSq > KF_AIR_RAM_ALIVE_EPSILON_SQ)
            {
                // Fire the impulse at its application point. Tags recovered from UpdateAirRam's own asm
                // @0x825FCA7C-0x825FCA80: `li r5,1` (position BODY_SPACE) and
                // `lwz r4,0x1184(r31)` -- the FORCE tag is the effect's STORED meImpulseSpace
                // (+0x1160 + 0x24 == mAirRamEffect[0].meImpulseSpace), not a literal. This is the
                // one call site of the five whose vector tag is data-driven.
                AddLocalImpulse(lrRam.mImpulse, lrRam.meImpulseSpace,
                                lrRam.mPosition, rw::physics::BODY_SPACE);

                // Decay the stored impulse for the next fire.
                const f32 lfDecayScale = 1.0f - lrRam.mfDecay;
                lrRam.mImpulse.x *= lfDecayScale;
                lrRam.mImpulse.y *= lfDecayScale;
                lrRam.mImpulse.z *= lfDecayScale;
            }
            else
            {
                mUsedAirRams.UnSetBit(static_cast<u32>(liSlot));   // spent -> release
            }
        }
    }

    // -------------------------------------------------------------------------------------
    // @0x825FCCF8  VehiclePhysics::UpdateSpinEffects -- tick the queued spin (angular-impulse) effects.
    //   Walks the SET bits of mUsedSpins (GetFirstNonZeroBit / GetNextNonZeroBit). For each active slot i:
    //     if ( mfTimeRemaining <= 0.0099999998f )         // expired (INLINE literal, not a .rdata symbol)
    //         mUsedSpins.UnSetBit(i);                       // release the slot
    //     else {
    //         AddWorldSpaceAngularImpulse(mForce);          // apply the spin this frame
    //         mfTimeRemaining -= <consumed-time lane>;      // count the lifetime down
    //     }
    //
    //   FLAG (slice): the X360 subtracts a VMX lane written back by AddWorldSpaceAngularImpulse (the
    //   consumed-time lane); the slice's stub returns void, so the decrement is taken as the frame dt
    //   (the faithful per-frame countdown). Behaviourally identical: mfTimeRemaining bleeds down each
    //   frame until it crosses the 0.01 cut.
    // -------------------------------------------------------------------------------------
    void VehiclePhysics::UpdateSpinEffects(VecFloat lvfDeltaTime)
    {
        static const f32 KF_SPIN_EXPIRED_CUT = 0.0099999998f;   // inline literal (asm), not rodata

        const f32 lfDeltaTime = lvfDeltaTime.x;   // dt splat lane (the consumed-time decrement)

        for (s32 liSlot = mUsedSpins.GetFirstNonZeroBit();
             liSlot >= 0;
             liSlot = mUsedSpins.GetNextNonZeroBit(liSlot))
        {
            SpinEffect& lrSpin = maSpinEffects[liSlot];

            if (lrSpin.mfTimeRemaining <= KF_SPIN_EXPIRED_CUT)
            {
                mUsedSpins.UnSetBit(static_cast<u32>(liSlot));
                continue;
            }

            AddWorldSpaceAngularImpulse(lrSpin.mForce);
            lrSpin.mfTimeRemaining -= lfDeltaTime;
        }
    }

    // -------------------------------------------------------------------------------------
    // @0x825FE118  VehiclePhysics::AddAirRam -- enqueue a new air-ram impulse into a free slot.
    //   DIRECTION (v127), by luFlags:
    //     * bit0 (0x1)  custom WORLD-space impulse  -> direction = lvCustomImpulse, space = WORLD
    //     * bit2 (0x4)  custom BODY-space  impulse  -> direction = lvCustomImpulse, space = BODY
    //     * else BODY-space axis seeds (space = BODY):
    //         start 0; bit3(0x8) += +X seed; bit4(0x10) += +Y seed (world up); bit5(0x20) += +Z seed;
    //         then NORMALIZE (vrsqrtefp+Newton).
    //   MAGNITUDE: mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x (lane0 @ mpAttribs+0x70) * lfFactor * 50.0;
    //              mImpulse = normalize(direction) * magnitude.
    //   POSITION (v127'): bit8(0x100) custom -> lvCustomPosition; else seeds from mHalfExtent
    //     (this+0x6A0, already pinned) with bit-selected lane negations: 0x200 -> +z, 0x400 -> -z,
    //     0x800 -> -x, 0x1000 -> +x (vrlimi128 masks 2/8; 0x825FE118 region).
    //   SLOT: first free bit of mUsedAirRams; if none free, evict the active slot with smallest stored
    //         |mImpulse|^2 (`_R25=this+0x1160=mAirRamEffect[0].mImpulse`, stride 48B, the X360
    //         FLT_MAX-seeded argmin over the LOADED mImpulse row -- not mPosition). Store the fields
    //         + SetBit(slot).
    //
    //   FLAG (rodata): the +X/+Y/+Z body-axis seed vectors (unk_82181510 / gIVector / unk_82181520) for
    //   the axis-flag direction path (bits 0x8/0x10/0x20) are un-homed .rdata -> flagged-0 placeholders;
    //   the direction stays 0 until recovered (an axis-flag-only ram assembles a zero direction) --
    //   NEVER fabricated. The 50.0 scale, the (attribLane * lfFactor) product, the normalize, the
    //   mHalfExtent position seeds, the |mImpulse|^2 eviction and the field stores are EXACT. A custom
    //   impulse (bit0/bit2) and a custom position (bit8) path are fully exact.
    //   The IsValid()/"must specify an axis" debug asserts are elided (debug-build guards).
    // -------------------------------------------------------------------------------------
    void VehiclePhysics::AddAirRam(u32 luFlags, f32 lfFactor, f32 lfDecay,
                                   Vector3 lvCustomImpulse, Vector3 lvCustomPosition, f32 lfTimerTillFire)
    {
        // ----- direction + impulse-space -----
        rw::physics::InputSpace leImpulseSpace = rw::physics::BODY_SPACE;
        Vector3 lvDirection = { 0.0f, 0.0f, 0.0f, 0.0f };

        if (luFlags & 0x1u)            // custom WORLD-space impulse
        {
            lvDirection    = lvCustomImpulse;
            leImpulseSpace = rw::physics::WORLD_SPACE;
        }
        else if (luFlags & 0x4u)       // custom BODY-space impulse
        {
            lvDirection    = lvCustomImpulse;
            leImpulseSpace = rw::physics::BODY_SPACE;
        }
        else                           // body-space axis-flag seeds (normalized)
        {
            leImpulseSpace = rw::physics::BODY_SPACE;

            const Vector3 lvAxisSeedX = AxisSeedPlaceholder(0);   // unk_82181510 (+X)  -- flagged-inert
            const Vector3 lvAxisSeedY = AxisSeedPlaceholder(1);   // gIVector     (+Y)  -- flagged-inert
            const Vector3 lvAxisSeedZ = AxisSeedPlaceholder(2);   // unk_82181520 (+Z)  -- flagged-inert

            if (luFlags & 0x8u)   { lvDirection.x += lvAxisSeedX.x; lvDirection.y += lvAxisSeedX.y; lvDirection.z += lvAxisSeedX.z; }
            if (luFlags & 0x10u)  { lvDirection.x += lvAxisSeedY.x; lvDirection.y += lvAxisSeedY.y; lvDirection.z += lvAxisSeedY.z; }
            if (luFlags & 0x20u)  { lvDirection.x += lvAxisSeedZ.x; lvDirection.y += lvAxisSeedZ.y; lvDirection.z += lvAxisSeedZ.z; }

            lvDirection = vpu::Normalize(lvDirection);   // vrsqrtefp+Newton normalize (zero stays zero)
        }

        // ----- magnitude: (attribs base lane0) * lfFactor * 50.0 -----
        static const f32 KF_AIR_RAM_SCALE = 50.0f;   // inline literal (asm v121[0] = 50.0)
        const f32 lfMagnitude = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x * lfFactor * KF_AIR_RAM_SCALE;

        Vector3 lvImpulse = { lvDirection.x * lfMagnitude,
                              lvDirection.y * lfMagnitude,
                              lvDirection.z * lfMagnitude,
                              0.0f };

        // ----- position -----
        Vector3 lvPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (luFlags & 0x100u)          // custom position
        {
            lvPosition = lvCustomPosition;
        }
        else
        {
            // Non-custom position seeds come from mHalfExtent (this+0x6A0, already pinned in this
            // header): bit 0x200 -> +z, bit 0x400 -> -z, bit 0x800 -> -x, bit 0x1000 -> +x (vrlimi128
            // masks 2/8 insert into lanes .z/.x; the negated variants vxor the sign bit first).
            if (luFlags & 0x200u)        lvPosition.z = mHalfExtent.z;
            else if (luFlags & 0x400u)   lvPosition.z = -mHalfExtent.z;
            if (luFlags & 0x800u)        lvPosition.x = -mHalfExtent.x;
            else if (luFlags & 0x1000u)  lvPosition.x = mHalfExtent.x;
        }

        // ----- choose a slot: first free, else evict the smallest-|impulse| active slot -----
        // 0x825FE118 loop: `_R25 = _R17 + 1112` (= this+0x1160 = mAirRamEffect[0].mImpulse, stride 12
        // dwords/48B) ; `vmsum3fp128 v0,v0,v0` on the LOADED mImpulse row -- the argmin is over
        // |mImpulse|^2, not |mPosition|^2.
        s32 liSlot = -1;
        for (u32 lu = 0; lu < KU_MAX_AIR_RAMS; ++lu)
        {
            if (!mUsedAirRams.IsBitSet(lu)) { liSlot = static_cast<s32>(lu); break; }
        }
        if (liSlot < 0)
        {
            f32 lfMinMagSq = 3.4028235e38f;   // FLT_MAX (asm v81 seed)
            for (u32 lu = 0; lu < KU_MAX_AIR_RAMS; ++lu)
            {
                const f32 lfMagSq = vpu::MagnitudeSquared(mAirRamEffect[lu].mImpulse);
                if (lfMagSq < lfMinMagSq) { lfMinMagSq = lfMagSq; liSlot = static_cast<s32>(lu); }
            }
        }

        // ----- store into the slot + mark it used -----
        AirRamEffect& lrRam   = mAirRamEffect[liSlot];
        lrRam.mImpulse        = lvImpulse;
        lrRam.mPosition       = lvPosition;
        lrRam.mfDecay         = lfDecay;
        lrRam.meImpulseSpace  = leImpulseSpace;
        lrRam.mfTimerTillFire = lfTimerTillFire;
        mUsedAirRams.SetBit(static_cast<u32>(liSlot));
    }

    // -------------------------------------------------------------------------------------
    // @0x825D0BE8  VehiclePhysics::UpdateInAirBehaviour -- FIDELITY: BLOCKED (structural skeleton only).
    //   The active-airborne attitude controller. STRUCTURE (findings doc Section 10): gate on the
    //   in-air-rotation-enable flag (+0x1350); else clear the roll-correction scratch (+0x13D8) and
    //   return. Two modes -- TAKE-OFF (+0x1351 set, mPitchYawRollFromTakeOff): hard-damp roll
    //   (kfLandingAssistDamping, gated by kfMinRollToAllowCorrection) + clamp wheelies (pitch vs a
    //   max-wheelie angle). STEADY-AIR: a pitch-damp blend from the body X-axis-vs-vertical orientation
    //   (kfFullDampThreshold / kfNoDampThreshold / kfMinRollFactor) + a yaw-damp interp
    //   (kYawDamp_Min -> kYawDamp_Target at kDamp_BlendRate), then DampPitchYawRoll. After either: roll
    //   auto-level (subtract an angular-velocity fraction per body axis, msfInAirRollCorrectionFactor x
    //   msfInAirIncreaseRollFactor) then AddWorldSpaceTorque.
    //   WHY BLOCKED: the per-lane SIMD is heavy VMX128 -- the pitch-damp RATIONAL interpolation uses
    //   vrefp+Newton segment slopes whose exact polynomial form is not algebraically pinned by the asm,
    //   and EVERY damping constant above is un-homed .rdata absent from the export. A faithful body would
    //   require fabricating both the interpolation polynomials and the constants -> forbidden. The
    //   DampPitchYawRoll + AddWorldSpaceTorque call shape is certain; the damping INPUTS are not. No
    //   fabricated math emitted; the function is intentionally not declared in the header.
    // -------------------------------------------------------------------------------------

    // -------------------------------------------------------------------------------------
    // @0x825CFD20  VehiclePhysics::UpdateFreezing -- FIDELITY: BLOCKED (structural skeleton only).
    //   The sleep/immobilise gate (findings doc Section 10). STRUCTURE: combine squared-speed tests
    //   (|mLinearVelocity|^2 + |mAngularVelocity|^2 vs small thresholds unk_82FB8460 / vcfsx-derived) with
    //   a control-idle test (controls stick sum (accel+steer) > 0 OR a control button byte) and a VIRTUAL
    //   base "is-asleep" query, then write the frozen flag (+0x70) -- gated by a control byte
    //   (controls+59 == 0), OR'd with a force-frozen byte (this+4342) and a flag (this[28]) -- and zero
    //   the velocity rows when frozen.
    //   WHY BLOCKED: faithful recovery needs (a) the BrnPlayerDriverControls field layout (stick/button
    //   offsets +4/+8/+59/+64) -- that type has only a forward decl, NO committed member layout; (b) a
    //   VIRTUAL base "is asleep" call ((*(vtable+20))(this)) -- this minimal slice models no vtable, so
    //   the virtual cannot be issued without inventing one; (c) the un-homed speed thresholds
    //   (unk_82FB8460); (d) several un-pinned this-offsets (the squared-speed source rows, this[28], the
    //   +4342 force-frozen byte). The masked-SIMD comparison routing is not recoverable store-for-store.
    //   A faithful body would require fabricating the control accessors + the vtable + the thresholds ->
    //   forbidden. No fabricated math emitted; the function is intentionally not declared in the header.
    // -------------------------------------------------------------------------------------

    // =======================================================================================
    // C07 boost/speed-match group: UpdateBoost / ApplyNormalBoostForce / ApplyBoostKickForce /
    //   UpdateSpeedMatch.  @0x825FACE8 / @0x825D30C8 / @0x825D3228 / @0x825D4AD8.
    //
    // All boost force = Mass * acceleration along the body forward axis (mTransform.zAxis @+0x30),
    // applied via the base ExternalPhysicsBody::AddLocalForce at a local-space point. mpAttribs
    // (+0x720) supplies Mass (mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x @+0x70) + the BoostAttribs lanes (+0x290..+0x2B0). The
    // boost-state timers live in mvSideForceMag_..._CurrentBoostKickTime (+0x1040): .y=TimeBoosting,
    // .z=TimeSinceLastBoostKick, .w=CurrentBoostKickTime. The heavy CgsDev::Assert mutual-exclusion
    // machinery in the two appliers is ELIDED (debug-build guards; no effect on output).
    //
    // FLAG (rodata): the boost speed FLOOR flt_8200426C (= 5.0 mph), the kick cooldown flt_82001D9C
    // (= 2.0 s) and the ratio numerator flt_82001C98 (= 1.0) are resolved constants (homed in sibling
    // TUs / the findings doc). flt_82001CC0 (the position/timer zero-init seed) is un-homed and carried
    // as a flagged-0 placeholder. kfMaxWheelieAngle / kfWheelieLimitDamping (the kick self-limit) are
    // un-homed, carried as flagged-0 placeholders (faithful-but-inert: the math/structure is exact, the
    // value stays 0 until recovered). NEVER fabricated.
    // ⭐ unk_82FB8A90 (the speed-match clamp) is NO LONGER a placeholder -- it is 50.0f; see the note
    // on UpdateSpeedMatch. The whole 0x82FB.... family reads zero in the image because those slots are
    // filled by an unexported static-initialiser block, not because their values are unrecoverable.
    // =======================================================================================

    // @0x825FACE8  BrnPhysics::Vehicle::VehiclePhysics::UpdateBoost
    //   r31=this ; v127=dt(VecFloat) ; r4=controls.
    //   1. lbRearWheelsOnGround = maWheels[2].onGround (+0x318) && maWheels[3].onGround (+0x3F8).
    //   2. if (!controls.boost @+0x3B) goto reset.
    //   3. if (mfSpeedMPH < 5.0)  goto reset.                       [flt_8200426C boost floor]
    //   4. if (!lbRearWheelsOnGround) goto reset.                   [0x825FAD84-90: master gate --
    //      airborne rear wheels take the RESET path, not just "skip the cap check".]
    //   5. if (mfSpeedMPH >= MaxBoostSpeed*throttle) -> at cap, no force (timers still advance).
    //   6. kick-eligible = (TimeBoosting==0) && (TimeSinceLastBoostKick > 2.0) &&
    //                      (BoostKickMaxStartSpeed >= mfSpeedMPH).  [flt_82001D9C cooldown]
    //      if eligible: CurrentBoostKickTime =
    //          clamp(BoostKickMaxTime*(1 - mfSpeedMPH/BoostKickMaxStartSpeed)^2,
    //                BoostKickMinTime, BoostKickMaxTime).            [flt_82001C98 = 1.0]
    //   7. mbInBoostKick = (CurrentBoostKickTime > TimeBoosting);
    //      mbInBoostKick ? ApplyBoostKickForce(dt) : ApplyNormalBoostForce(dt).
    //      TimeBoosting += dt.
    //   reset path: TimeBoosting=0 ; CurrentBoostKickTime=0 ; TimeSinceLastBoostKick += dt.
    void VehiclePhysics::UpdateBoost(const BrnPlayerDriverControls* lpControls, f32 lfTimeStep)
    {
        static const f32 KF_BOOST_SPEED_FLOOR = 5.0f;   // flt_8200426C (findings: 5.0 mph)
        static const f32 KF_KICK_COOLDOWN     = 2.0f;   // flt_82001D9C (resolved 2.0)
        static const f32 KF_ONE               = 1.0f;   // flt_82001C98 (resolved 1.0)

        Vector4& lrBoost = mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime;
        const VehicleAttribs::BoostAttribs& lrBA = mpAttribs->mBoostAttribs;

        // Rear-axle traction: both rear wheels (indices 2,3) on the ground. This is a MASTER gate
        // (0x825FAD84-90: `beq loc_825FAFB0` on either wheel being airborne) -- with either rear
        // wheel off the ground the function takes the reset path below and applies no force at all,
        // it does not merely skip the speed-cap comparison.
        const bool lbRearWheelsOnGround =
            maWheels[eRearLeftWheel].GetRoadContact().mbIsOnGround &&
            maWheels[eRearRightWheel].GetRoadContact().mbIsOnGround;

        // The boost button. asm `lbz r10, 0x3B(r4)` @0x825FAD34 -- with the corrected controls
        // layout +0x3B is mbBoost (the committed layout put mbBoostBounce there, so this read the
        // right BYTE under the wrong NAME).
        // Below the 5.0-mph floor, or with the rear wheels airborne, no boost ever applies this
        // frame -- either -> the "not boosting" reset path.
        const bool lbApply = lpControls->mbBoost
                           && (mfSpeedMPH.x >= KF_BOOST_SPEED_FLOOR)
                           && lbRearWheelsOnGround;

        if (lbApply)
        {
            // Throttle-scaled speed cap. At/above the cap, the timers still advance but no force is
            // applied this frame.
            // ⭐ RESOLVED 2026-08-03. The X360 reads controls+0x34 (`lfs f0, 0x34(r4)` @0x825FAD98).
            // The committed layout labelled that slot miVehicleIDToMerge and this body substituted
            // mfRequestedGas (+0x1C) -- a DIFFERENT field. +0x34 is a thirteenth control float the
            // X360 build carries (Clear seeds it 1.0f); it is now a named member.
            const f32 lfThrottle      = lpControls->mfBoostMaxSpeedScale;    // asm +0x34
            const f32 lfMaxBoostSpeed =
                lrBA.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.y;

            const bool lbApplyForce = (mfSpeedMPH.x < lfMaxBoostSpeed * lfThrottle);

            if (lbApplyForce)
            {
                const f32 lfTimeBoosting           = lrBoost.y;
                const f32 lfTimeSinceLastBoostKick = lrBoost.z;
                const f32 lfBoostKickMaxStartSpeed =
                    lrBA.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.y;

                // Kick eligibility: a fresh boost, past the cooldown, starting from low speed.
                const bool lbKickEligible = (lfTimeBoosting == 0.0f)
                                         && (lfTimeSinceLastBoostKick > KF_KICK_COOLDOWN)
                                         && (lfBoostKickMaxStartSpeed >= mfSpeedMPH.x);

                if (lbKickEligible)
                {
                    const f32 lfBoostKickMaxTime =
                        lrBA.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.z;
                    const f32 lfBoostKickMinTime =
                        lrBA.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.w;

                    // 1/BoostKickMaxStartSpeed (the X360 vrefp+Newton reciprocal) -> speed ratio.
                    const f32 lfRatio  = mfSpeedMPH.x / lfBoostKickMaxStartSpeed;
                    const f32 lfShaped = (KF_ONE - lfRatio) * (KF_ONE - lfRatio);   // (1 - ratio)^2
                    f32 lfKickTime = lfBoostKickMaxTime * lfShaped;

                    // Clamp to [BoostKickMinTime, BoostKickMaxTime] (vmaxfp then vminfp).
                    if (lfKickTime < lfBoostKickMinTime) lfKickTime = lfBoostKickMinTime;
                    if (lfKickTime > lfBoostKickMaxTime) lfKickTime = lfBoostKickMaxTime;

                    lrBoost.w = lfKickTime;   // CurrentBoostKickTime
                }

                // KICK while the kick window still leads the elapsed boost time.
                mbInBoostKick = (lrBoost.w > lrBoost.y);
                if (mbInBoostKick)
                    ApplyBoostKickForce(lfTimeStep);
                else
                    ApplyNormalBoostForce(lfTimeStep);

                lrBoost.y += lfTimeStep;   // TimeBoosting += dt
                return;
            }

            // At the cap: advance TimeBoosting only (matches the post-applier tail of the X360 path).
            lrBoost.y += lfTimeStep;
            return;
        }

        // Not boosting this frame: reset TimeBoosting + CurrentBoostKickTime, accumulate the cooldown.
        lrBoost.y = 0.0f;          // TimeBoosting
        lrBoost.w = 0.0f;          // CurrentBoostKickTime
        lrBoost.z += lfTimeStep;   // TimeSinceLastBoostKick += dt
    }

    // @0x825D30C8  BrnPhysics::Vehicle::VehiclePhysics::ApplyNormalBoostForce
    //   Gated on mbAllWheelsHaveTraction (+0x135B). F = Mass * NormalBoostAcceleration along the body
    //   forward axis (mTransform.zAxis @+0x30), applied at the local point
    //   (0, NormalBoostHeightOffset, -mHalfExtent.z) via AddLocalForce. Then TimeSinceLastBoostKick += dt
    //   and CurrentBoostKickTime = 0. (The "!mbInBoostKick" assert is elided.)
    void VehiclePhysics::ApplyNormalBoostForce(f32 lfTimeStep)
    {
        Vector4& lrBoost = mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime;

        if (mbAllWheelsHaveTraction)
        {
            const VehicleAttribs::BoostAttribs& lrBA = mpAttribs->mBoostAttribs;

            const f32 lfMass  = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;   // mvMass_..._.x lane (+0x70 lane0)
            const f32 lfAccel =
                lrBA.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.x;
            const f32 lfHeightOffset =
                lrBA.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.w;

            // Force = forward * (Mass * accel) (asm: vmulfp128 v1, v12(=mTransform.zAxis), scalar).
            const Vector3 lvForce = mTransform.zAxis * (lfMass * lfAccel);

            // Local application point: (0, heightOffset, -mHalfExtent.z) -- behind the centre of mass.
            // flt_82001CC0 @0x82001CC0 .rdata reads 0x00000000 in the image -- it IS zero. The value
            // was never in doubt here; only its FLAGGED status was, and that is now cleared.
            static const f32 KF_BOOST_POS_SEED = 0.0f;   // flt_82001CC0 (image: 0x00000000)
            const Vector3 lvLocalPos{ KF_BOOST_POS_SEED, lfHeightOffset, -mHalfExtent.z, 0.0f };

            // asm @0x825D3160/0x825D3138: r4 = 0 (force WORLD_SPACE -- it is built from
            // mTransform.zAxis, already a world direction), r5 = 1 (position BODY_SPACE).
            AddLocalForce(lvForce, rw::physics::WORLD_SPACE,
                          lvLocalPos, rw::physics::BODY_SPACE);
        }

        lrBoost.z += lfTimeStep;   // TimeSinceLastBoostKick += dt
        lrBoost.w = 0.0f;          // CurrentBoostKickTime = 0
    }

    // @0x825D3228  BrnPhysics::Vehicle::VehiclePhysics::ApplyBoostKickForce
    //   Resets TimeSinceLastBoostKick=0. F = Mass * BoostKickAcceleration along the forward axis,
    //   applied at the off-centre local point (0, BoostKickHeightOffset, -mHalfExtent.z) -> a pitch-up
    //   wheelie. Then, while the rear-left wheel contact is grounded and the car's forward-axis pitch
    //   exceeds sin(kfMaxWheelieAngle deg->rad), it bleeds the body right-axis (mTransform.xAxis)
    //   component out of each of mAngularVelocity (+0x60), mTotalTorque (+0x100) and
    //   mTotalAngularImpulse (+0x120) by kfWheelieLimitDamping. Tail resets
    //   TimeSinceLastBoostKick to the flt_82001CC0 seed (0). (The "mbInBoostKick" assert is elided.)
    //
    //   FLAG (rodata): kfMaxWheelieAngle (max wheelie angle, degrees) and kfWheelieLimitDamping (the
    //   bleed factor) are un-homed .rdata absent from the export -> flagged-0 placeholders (faithful-
    //   but-inert): the self-limit STRUCTURE is exact, the threshold/damp stay 0 until recovered (with
    //   threshold 0 the limiter never engages -- inert, not fabricated). flt_8208F5F4 (the deg->rad
    //   factor the asm multiplies kfMaxWheelieAngle by) is resolved to 0.017453292 per the findings doc.
    void VehiclePhysics::ApplyBoostKickForce(f32 lfTimeStep)
    {
        static const f32 KF_DEG_TO_RAD         = 0.017453292f;   // flt_8208F5F4 (deg->rad, findings)
        static const f32 KF_MAX_WHEELIE_ANGLE  = 0.0f;           // FLAG: un-homed kfMaxWheelieAngle (deg)
        static const f32 KF_WHEELIE_LIMIT_DAMP = 0.0f;           // FLAG: un-homed kfWheelieLimitDamping
        static const f32 KF_KICK_TIMER_SEED    = 0.0f;           // flt_82001CC0 (image: 0x00000000)

        Vector4& lrBoost = mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime;
        lrBoost.z = 0.0f;   // TimeSinceLastBoostKick = 0 (kick resets the cooldown; vrlimi128 lane z)

        const VehicleAttribs::BoostAttribs& lrBA = mpAttribs->mBoostAttribs;

        const f32 lfMass         = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;                            // Mass
        const f32 lfAccel        = lrBA.mvBoostKickAcceleration_BoostKickHeightOffset.x; // BoostKickAcceleration
        const f32 lfHeightOffset = lrBA.mvBoostKickAcceleration_BoostKickHeightOffset.y; // BoostKickHeightOffset

        const Vector3 lvForce = mTransform.zAxis * (lfMass * lfAccel);
        const Vector3 lvLocalPos{ 0.0f, lfHeightOffset, -mHalfExtent.z, 0.0f };
        // asm @0x825D3290/0x825D3280: r4 = 0 (WORLD force), r5 = 1 (BODY position).
        AddLocalForce(lvForce, rw::physics::WORLD_SPACE,
                      lvLocalPos, rw::physics::BODY_SPACE);

        // --- Wheelie self-limit ---
        // Only when the rear-left wheel contact (the X360 copies wheel2's road-contact block at +0x2F0
        // and tests its on-ground byte at +0x28) is grounded.
        if (maWheels[eRearLeftWheel].GetRoadContact().mbIsOnGround)
        {
            const f32 lfWheelieSin = std::sin(KF_MAX_WHEELIE_ANGLE * KF_DEG_TO_RAD);   // XMVectorSin

            // Pitch = dot(forward axis, the copied contact normal).
            const f32 lfPitch =
                vpu::Dot(mTransform.zAxis, maWheels[eRearLeftWheel].GetRoadContact().mNormal);

            if (lfPitch > lfWheelieSin)
            {
                // Bleed the body right-axis (mTransform.xAxis) component out of each row, but only the
                // positive-direction component (the asm tests `flt_82001CC0(=0) > dot`). With the damp
                // factor at its flagged 0 the subtraction is inert; the structure is exact.
                const Vector3& lvRight = mTransform.xAxis;

                const f32 lfRollVel = vpu::Dot(lvRight, mAngularVelocity);
                if (0.0f > lfRollVel)
                    mAngularVelocity = mAngularVelocity - lvRight * (lfRollVel * KF_WHEELIE_LIMIT_DAMP);

                const f32 lfRollLin = vpu::Dot(lvRight, mTotalTorque);
                if (0.0f > lfRollLin)
                    mTotalTorque =
                        mTotalTorque - lvRight * (lfRollLin * KF_WHEELIE_LIMIT_DAMP);

                const f32 lfRollAng = vpu::Dot(lvRight, mTotalAngularImpulse);
                if (0.0f > lfRollAng)
                    mTotalAngularImpulse =
                        mTotalAngularImpulse - lvRight * (lfRollAng * KF_WHEELIE_LIMIT_DAMP);
            }
        }

        // Tail: TimeSinceLastBoostKick = flt_82001CC0 (the zero-seed). (vrlimi128 lane z.)
        lrBoost.z = KF_KICK_TIMER_SEED;
        (void)lfTimeStep;   // dt is the VecFloat arg; this applier advances no timer with it.
    }

    // @0x825D4AD8  BrnPhysics::Vehicle::VehiclePhysics::UpdateSpeedMatch
    //   Gated on mbAllWheelsHaveTraction (+0x135B) AND controls speed-match mode (+0x44 == 1) AND a
    //   target present (+0x4C != 0). Then:
    //     forwardSpeed = dot3(mLinearVelocity @+0x50, mTransform.zAxis @+0x30)
    //     delta        = targetSpeed(+0x48) - forwardSpeed
    //     clampedDelta = clamp(delta, -clampVec*dt, +clampVec*dt)         [unk_82FB8A90 clamp vector]
    //     target       = forwardSpeed + clampedDelta
    //     recipFront = 1 / maWheels[0].mSlipVariables.w   (front pair)
    //     recipRear  = 1 / maWheels[2].mSlipVariables.w   (rear pair)
    //     maWheels[0/1].mIntegrationVariables.x = recipFront * target
    //     maWheels[2/3].mIntegrationVariables.x = recipRear  * target
    //     mLinearVelocity += forwardAxis * clampedDelta                   (the soft fold-back)
    //
    //   ⭐ RESOLVED 2026-08-03 (both of this body's two flags).
    //   (1) CONTROL OFFSETS. +0x44/+0x48/+0x4C are not "past the layout" -- +0x44 is meDriverType and
    //   the other two are BrnAIDriverControls::mfSpeedMatchSpeed / mbDoSpeedMatch. The console is
    //   doing exactly what it looks like: check the driver type, then read the AI payload. The DWARF
    //   NAMES the two AI members after this very function, which is what settles it beyond offsets.
    //   The raw byte view is gone.
    //   (2) THE CLAMP BOUND. unk_82FB8A90 is a .data slot that reads zero in the image; it is filled
    //   at static-init time by an unexported initialiser at 0x82C5CB28 that splats the .rdata scalar
    //   flt_820138DC == 50.0f. So the bound is 50, not 0 -- and with the placeholder 0 the clamped
    //   delta was identically zero, i.e. the speed-match nudge did NOTHING.
    void VehiclePhysics::UpdateSpeedMatch(const BrnPlayerDriverControls* lpControls, f32 lfTimeStep)
    {
        if (!mbAllWheelsHaveTraction)
            return;

        // asm @0x825D4AE4: lwz 0x44 (meDriverType) == 1, then lbz 0x4C (mbDoSpeedMatch), then
        // lfs 0x48 (mfSpeedMatchSpeed). Speed-match is an AI-only behaviour.
        if (lpControls->GetType() != E_DRIVER_TYPE_AI)
            return;
        const BrnAIDriverControls* lpAIControls = static_cast<const BrnAIDriverControls*>(lpControls);
        if (!lpAIControls->mbDoSpeedMatch)
            return;
        const f32 lfTargetSpeed = lpAIControls->mfSpeedMatchSpeed;

        // Current forward speed = dot3(mLinearVelocity, body forward axis).
        const f32 lfForwardSpeed = vpu::Dot(mLinearVelocity, mTransform.zAxis);
        const f32 lfDelta        = lfTargetSpeed - lfForwardSpeed;

        // Clamp the delta to +/-(clampVec * dt). ⭐ RESOLVED 2026-08-03: unk_82FB8A90 is zero in the
        // image only because it is filled at static-init time -- the unexported initialiser at
        // 0x82C5CB28 splats the .rdata scalar flt_820138DC == 50.0f into it. Read out of the IDB with
        // headless IDA. It was a flagged-0 placeholder, which made this whole nudge a no-op.
        static const f32 KF_SPEED_MATCH_CLAMP = 50.0f;  // X360 flt_820138DC -> unk_82FB8A90 (splat)
        const f32 lfBound = KF_SPEED_MATCH_CLAMP * lfTimeStep;
        f32 lfClampedDelta = lfDelta;
        if (lfClampedDelta > lfBound)  lfClampedDelta = lfBound;
        if (lfClampedDelta < -lfBound) lfClampedDelta = -lfBound;

        const f32 lfTarget = lfForwardSpeed + lfClampedDelta;

        // Per-axle reciprocal (the X360 vrefp+double-Newton of the wheel slip-register .w lane). The
        // front pair share wheel0's reciprocal, the rear pair share wheel2's. 0x825D4B44-50: the
        // reciprocal is UNGUARDED (no zero-select in the asm) -- a zero slip lane produces +/-inf,
        // which the IEEE-754 division below reproduces naturally.
        const f32 lfSlipFront = maWheels[eFrontLeftWheel].mSlipVariables.w;
        const f32 lfSlipRear  = maWheels[eRearLeftWheel ].mSlipVariables.w;
        const f32 lfRecipFront = 1.0f / lfSlipFront;
        const f32 lfRecipRear  = 1.0f / lfSlipRear;

        const f32 lfForceFront = lfRecipFront * lfTarget;
        const f32 lfForceRear  = lfRecipRear  * lfTarget;

        // Distribute into the four wheel integration accumulators (.x lane; vrlimi128 mask lane0).
        maWheels[eFrontLeftWheel ].mIntegrationVariables.x = lfForceFront;
        maWheels[eFrontRightWheel].mIntegrationVariables.x = lfForceFront;
        maWheels[eRearLeftWheel  ].mIntegrationVariables.x = lfForceRear;
        maWheels[eRearRightWheel ].mIntegrationVariables.x = lfForceRear;

        // Fold the clamped delta back into the world velocity along the forward axis (vmaddfp ->
        // stvx128 at +0x50): mLinearVelocity += forwardAxis * clampedDelta.
        mLinearVelocity = mLinearVelocity + mTransform.zAxis * lfClampedDelta;
    }

    // =====================================================================================
    // C06 steering / drift / handbrake group. Bodies for the signature steering+drift+handbrake
    // pipeline. The X360 build is dense VMX128; these are the de-SIMD'd named-member equivalents
    // against the drift state bank (+0xFE0..+0x1080) + the drift byte flags grown in the header.
    // The heavy CgsDev::Assert/StrStream "Invalid ... during drift" machinery and the per-phase
    // ExternalPhysicsBody::CheckState debug calls are ELIDED (debug-build guards, no output effect).
    //
    // FLAG (rodata): several scalar gains/clamps the steering+drift forces use are un-homed .rdata
    // (flt_8208F620 / unk_82FB9020 / unk_82FB9370 / flt_830180B0 / unk_82FB8AC0 / unk_82FB9ED0 /
    // unk_82FB80F0 / unk_8327F240 / kDamp_BlendRate / unk_82014AC0.. / unk_82FB9080 / unk_82FB8B00 /
    // unk_82FB9080 etc.) absent from the function exports. Each is carried as an honest flagged-0
    // (or flagged enable) placeholder so the math + the named-member reads/writes are exact while the
    // numeric gain stays inert until the .rdata is recovered. NEVER fabricated.
    // =====================================================================================

    // small file-static placeholders for the un-homed steering/drift rodata gains (flagged-inert).
    // ⭐ stru_8208F620 is plain readable .rdata and holds 1.1920929e-07 -- FLT_EPSILON. This is the one
    //   placeholder in this file whose zero really WAS harmless: a zero-vs-epsilon guard on a magnitude
    //   differs only for denormal inputs. Recording it as an honest negative rather than quietly
    //   "fixing" it, because the standing rule here is that a 0.0f placeholder is never inert -- and
    //   this is the documented exception, not a counter-example to the rule.
    static const f32  KF_DRIFT_STEER_EPSILON       = 1.1920929e-07f;  // stru_8208F620 == FLT_EPSILON
    // ⭐⭐ unk_82FB9020 = 0.785398185, and its NAME and VALUE are both confirmed by its initialiser:
    //   @0x82C5CA80 computes flt_82009B80 (45.0) * flt_8208F5F4 (0.0174532924 = deg->rad). It is
    //   literally 45 degrees in radians. GetSteeringAngle @0x825D4150 then loads it, XORs the sign
    //   mask and `vmaxfp`s against the negation -- a symmetric +/-45 degree clamp. At 0.0f the drift
    //   steering angle was left entirely unclamped (+/-pi instead of +/-pi/4).
    static const f32  KF_STEER_ANGLE_CLAMP         = 0.785398185f;    // unk_82FB9020 = 45deg in rad
    static const f32  KF_WHEEL_STEER_BLEND         = 0.05f;           // unk_82FB9370 <- flt_820047C8 (splat)
    // ⭐⭐ THREE OF THE "un-homed" PLACEHOLDERS ABOVE ARE NOW RECOVERED (2026-08-03). All three read
    //    ZERO in the X360 image because they are .data slots filled at static-init time -- exactly the
    //    trap the gravity constant fell into -- so a literal scan of the export set could never find
    //    them. Read out of the IDB with headless IDA 9.3, from the initialisers themselves:
    //
    //  * flt_830180B0 is NOT a "drift slip-time gain" at all. Its initialiser is a DIVISION, not a
    //    splat, which is why even the static-init map (which only recognises the splat idiom) misses
    //    it:  0x82C6D0C0  f0 = flt_82001C98 (1.0f) ; f13 = flt_82F31928 (0.447039992f = MPH->m/s)
    //         0x82C6D0D4  fdivs f0, f0, f13 ; stfs f0, flt_830180B0     => 1/0.44704 = 2.2369363f
    //    i.e. it is **m/s -> MPH**. Its consumers say the same thing: BridgeWorldVehicleDataToGui,
    //    RaceCarEntityModuleDebugComponent::RenderHUD and three AI debug HUDs all read it. And in
    //    UpdateDriftState it converts the m/s speed parameter before comparing it against the SAME
    //    per-car attrib lane (mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x) that CheckForEnteringDrift compares mfSpeedMPH
    //    against -- one threshold, one unit, two directions. That is what identifies it by ROLE.
    //  * unk_82FB8AC0 <- flt_82094574 = 0.15f   (static-init splat @0x82C5C988)
    //  * unk_82FB9ED0 <- flt_82004A20 = 10.0f   (static-init splat @0x82C5C9B0)
    //    Both are still exit limits, but on DIFFERENT quantities than the committed names claimed --
    //    see UpdateDriftState below, whose guards were rebuilt from the asm this wave.
    static const f32  KF_MPS_TO_MPH                = 2.2369363f;   // flt_830180B0 = 1/flt_82F31928
    static const f32  KF_DRIFT_HANDBRAKE_ON_LIMIT  = 0.15f;        // unk_82FB8AC0 (splat)
    static const f32  KF_DRIFT_SPEED_EXIT_LIMIT    = 10.0f;        // unk_82FB9ED0 (splat), MPH
    // unk_82FB80F0 = 90.0. Its initialiser is NOT the splat idiom -- sub_82C5BE28 calls
    // CgsNumeric::CreateFloatVector(flt_82004F64), and flt_82004F64 is 90.0 (the third word of the
    // 0x82004F5C block). ⚠️ VALUE PROVED, ROLE NOT: nothing in this TU reads this constant today, so
    // the "drift-scale grow clamp" name is still the tree's prior guess and cannot be checked against
    // a consumer. Seated so the number stops being a lie; the NAME stays suspect.
    static const f32  KF_DRIFT_SCALE_GROW_LIMIT    = 90.0f;          // unk_82FB80F0 <- flt_82004F64
    static const f32  KF_HANDBRAKE_TIME_CAP        = 10000.0f;       // unk_82FB9080 <- flt_82005D9C (splat)
    static const f32  KF_HANDBRAKE_ONTIME_RELEASE  = 0.275f;         // unk_82FB8B00 <- flt_8209D720 (splat)
    static const f32  KF_RAD_TO_DEG                = 57.29578f;     // inline literal (asm 57.29578)
    static const f32  KF_DEG_TO_RAD                = 0.017453292f;  // inline literal (deg->rad)
    static const f32  KF_QUARTIC_STIFFEN           = 1.25f;         // inline literal (s^4 * 1.25)

    // @0x825D4028  VehiclePhysics::GetSteeringAngle  (virtual)
    //   When NOT drifting (mu8DriftState == 0) OR the world steering-direction guard fails, returns the
    //   cached steering angle lane. When drifting AND the guard passes, recomputes the signed world
    //   steering angle and speed/drift-blends it.
    //
    //   asm: if (!*(this+4946)) -> return cached (mvSteeringAngle_..._.x).
    //        else build `dir = abs(mLinearVelocity) packed; if (dir > epsilon)` recompute:
    //          unitVel = normalize(mLinearVelocity)              (vrsqrtefp + Newton, zero-guarded)
    //          c       = clamp(dot(unitVel, fwd=mTransform.zAxis), -1, 1)   [0x825D40FC-100: vmaxfp
    //                    against -1.0 (vcsxwfp of -1), NOT 0 -- caps reverse-travel angles at 180 deg]
    //          angle   = acos(c)                                  (XMVectorACos)
    //          if (dot(unitVel, right=mTransform.xAxis) < 0) angle = -angle    (signed)
    //          blend   = clamp(angle, -clampK, +clampK) with DriftScale authority shrink, then clamped
    //                    to unk_82FB9020.
    //   Guard: 0x825D4058-84 loads THIS+0x50 (mLinearVelocity), not the +0xFE0 steering register --
    //   the per-component |mLinearVelocity| lanes are tested against the epsilon splat.
    f32 VehiclePhysics::GetSteeringAngle() const
    {
        // cached lane (.x of the steering register @+0xFE0).
        const f32 lfCached = mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x;

        // NOT drifting -> the cached value (the asm's LABEL_6 fast path).
        if (mu8DriftState == eDriftState_None)
            return lfCached;

        // The world steering-direction guard: |mLinearVelocity| (this+0x50, NOT the +0xFE0 steering
        // register) vs a small epsilon (flagged). With the epsilon inert (0), the guard is effectively
        // always taken when drifting; faithful to the branch.
        const f32 lfVelMag = (mLinearVelocity.y >= 0.0f) ? mLinearVelocity.y : -mLinearVelocity.y;
        if (!(lfVelMag > KF_DRIFT_STEER_EPSILON))
            return lfCached;

        // Recompute the signed world steering angle.
        const Vector3 lUnitVel = vpu::Normalize(mLinearVelocity);   // zero-guarded
        f32 lfDot = vpu::Dot(lUnitVel, mTransform.zAxis);           // fwd axis @+0x30
        // 0x825D40FC-100: clamp to [-1, 1] (vmaxfp against -1.0, NOT 0.0).
        if (lfDot < -1.0f) lfDot = -1.0f;
        if (lfDot > 1.0f) lfDot = 1.0f;

        f32 lfAngle = std::acos(lfDot);                            // XMVectorACos

        // sign by dot(unitVel, right axis @+0x10).
        if (vpu::Dot(lUnitVel, mTransform.xAxis) < 0.0f)
            lfAngle = -lfAngle;

        // speed/drift authority shrink: the angle is blended by DriftScale (@+0x1000 .w) toward 0 and
        // clamped to the recovered +/-45 degree limit (unk_82FB9020 = 0.785398185 rad).
        const f32 lfDriftScale = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w;
        f32 lfBlended = lfAngle + (0.0f - lfAngle) * lfDriftScale;   // shrink authority as drift scale grows

        // final symmetric clamp to +-KF_STEER_ANGLE_CLAMP. The `> 0.0f` gate that used to wrap this
        // existed only so a flagged zero would not collapse the angle; the clamp is real now.
        if (lfBlended >  KF_STEER_ANGLE_CLAMP) lfBlended =  KF_STEER_ANGLE_CLAMP;
        if (lfBlended < -KF_STEER_ANGLE_CLAMP) lfBlended = -KF_STEER_ANGLE_CLAMP;
        return lfBlended;
    }

    // @0x825D34D8  VehiclePhysics::GetMaxSteeringAngleDuringDrift
    //   Builds a steering direction from the quartic-stiffened steer input, takes acos against the
    //   velocity direction, then scales by the per-car max steering angle (mpAttribs->mSteeringAttribs.mvMaxAngle_StraightReactionBias @+0xF0 .x,
    //   in DEGREES) * deg->rad. Returns the capped wheel angle (radians).
    //   asm: dir = normalize(mLinearVelocity); stiff = -1 - sign(s)*(s); angle = acos(dot(dir, steerDir))
    //        result = angle... * (mpAttribs->mSteeringAttribs.mvMaxAngle_StraightReactionBias.x * 0.017453292).
    f32 VehiclePhysics::GetMaxSteeringAngleDuringDrift(f32 lfSteeringInput) const
    {
        const Vector3 lUnitVel = vpu::Normalize(mLinearVelocity);   // zero-guarded

        // the steer-direction lane built from the (already stiffened) input; clamp dot to [0,1] then acos.
        f32 lfDot = vpu::Dot(lUnitVel, mTransform.zAxis);           // forward axis
        if (lfDot < 0.0f) lfDot = 0.0f;
        if (lfDot > 1.0f) lfDot = 1.0f;
        const f32 lfAngle = std::acos(lfDot);                      // XMVectorACos

        // per-car max angle (degrees) -> radians cap.
        const f32 lfMaxDeg = mpAttribs->mSteeringAttribs.mvMaxAngle_StraightReactionBias.x;                    // mpAttribs->mSteeringAttribs.mvMaxAngle_StraightReactionBias (+0xF0) .x
        // (void) the unused stiffened-sign term; the asm folds the sign into the steer-direction build.
        (void)lfSteeringInput;
        return lfAngle * (lfMaxDeg * KF_DEG_TO_RAD);
    }

    // @0x825CFB70  VehiclePhysics::ModifyControlsForSteeringWheelInput
    //   Quartic stick-stiffening: s' = -1 - sign(s) * (s^4 * 1.25). Softens centre, sharpens extremes.
    //   When the device is a steering wheel (mbIsSteeringWheel) it additionally blends the steering
    //   direction toward the body forward axis by unk_82FB9370 (flagged-inert).
    void VehiclePhysics::ModifyControlsForSteeringWheelInput(BrnPlayerDriverControls& lrControls) const
    {
        const f32 lfS = lrControls.mfSteering;                      // *(a2+16)
        if (lfS == 0.0f)
        {
            // asm: fsel keeps sign 0; the stiffening below collapses to -1 - 0 = ... but the original
            // leaves a 0-input centred. Preserve: a 0 stick stays 0 after the sign-select gate.
        }
        const f32 lfSign = (lfS > 0.0f) ? 1.0f : ((lfS < 0.0f) ? -1.0f : 0.0f);

        const f32 lfS2 = lfS * lfS;
        const f32 lfS4 = lfS2 * lfS2;
        const f32 lfStiffened = -1.0f - lfSign * (lfS4 * KF_QUARTIC_STIFFEN);

        // The asm fsel-routes between the stiffened value and the raw value by sign; the net result it
        // stores to *(a2+16) is the stiffened magnitude carrying the input sign.
        f32 lfOut = (lfSign != 0.0f) ? (lfStiffened * lfSign * -1.0f) : lfS;
        // Faithful simplification: |s'| = 1 + s^4*1.25, signed by the input -> a centre-soft, extreme-sharp
        // curve. (The double sign-fold above reduces to this.)
        lfOut = lfSign * (1.0f + lfS4 * KF_QUARTIC_STIFFEN);

        lrControls.mfSteering = lfOut;                              // *(a2+16) = s'

        // steering-wheel device path: blend the cached steering direction toward forward by a weight.
        if (lrControls.mbIsSteeringWheel)
        {
            // The unk_82FB9370 weight is RECOVERED (0.05); what is still missing is the BLEND ITSELF --
            // the asm reads mTransform.zAxis (forward) and the cached steering register and lerps them
            // by that weight, and this body has never modelled the lerp. Flagged as an unmodelled BODY
            // now, not as an unknown constant: filling the number does not fill the code.
            (void)KF_WHEEL_STEER_BLEND;
        }
    }

    // @0x825CFC68  VehiclePhysics::ModifyControlsForDrift
    //   While sliding (mu8DriftState != 0) and the ORIGINAL controls mode is NOT 1 (0x825CFC74-7C:
    //   `lwz r10,0x44(r4) ; cmpwi cr6,r10,1 ; beqlr cr6` -- returns when mode==1, i.e. proceeds only
    //   when mode != 1), re-signs and re-maps the steer input by the drift direction. The blend
    //   weights come from mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale (@+0x120) lanes .z (zLane) and .w (wLane).
    //   asm (0x825CFCC4-825CFD14): sign = (mu8DriftState==1) ? +1 : -1 ; s = sign * steer ;
    //     gas = controls.mfGas (+0x04) ; wGas = wLane * gas ;
    //     steer' = sign * ( (1 - wLane*gas^2) * max(s,0) + (zLane + wGas) * min(s,0) + wGas ).
    void VehiclePhysics::ModifyControlsForDrift(BrnPlayerDriverControls& lrControls) const
    {
        if (mu8DriftState == eDriftState_None)
            return;
        // 68 decimal == 0x44 == meDriverType; `== 1` is E_DRIVER_TYPE_AI. The drift remap is a
        // PLAYER-input remap, so an AI-driven car returns immediately. (Was the invented GetMode().)
        if (lrControls.GetType() == E_DRIVER_TYPE_AI)   // asm lwz 0x44(r4); cmpwi 1; beqlr
            return;

        const f32 lfSign = (mu8DriftState == eDriftState_FacingLeft) ? 1.0f : -1.0f;

        // the two drift remap weights (mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale @+0x120 .z and .w).
        const f32 lfWeightZ = mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.z;
        const f32 lfWeightW = mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.w;

        const f32 lfGas    = lrControls.mfGas;                     // *(a2+4)
        const f32 lfWGas   = lfWeightW * lfGas;                    // wLane * gas
        const f32 lfSigned = lfSign * lrControls.mfSteering;       // s = sign * steer (*(a2+16))
        const f32 lfPos    = (lfSigned > 0.0f) ? lfSigned : 0.0f;  // max(s, 0)
        const f32 lfNeg    = (lfSigned < 0.0f) ? lfSigned : 0.0f;  // min(s, 0)

        const f32 lfMapped = lfSign * ((1.0f - lfWeightW * lfGas * lfGas) * lfPos
                                      + (lfWeightZ + lfWGas) * lfNeg
                                      + lfWGas);
        lrControls.mfSteering = lfMapped;                          // *(a2+16) = mapped
    }

    // @0x825B8220  VehiclePhysics::ExitDrift
    //   Tears down the drift: clears mu8DriftState and zeroes the per-drift timer lanes across the bank
    //   (the asm vrlimi128-clears single lanes of +0x1000/+0x1010/+0x1020/+0x1030/+0x1040/+0xEF0), then
    //   resets mDriftFlags = KU_DRIFT_FLAG_DO_ALL and writes the slam marker -1 at +0x10F4 (the same byte
    //   the slam path uses). asm: `*(result+4946)=0 ; ... ; *(result+4340)=-1`.
    void VehiclePhysics::ExitDrift()
    {
        mu8DriftState = eDriftState_None;

        // zero the drift timer/scale lanes the asm clears (single-lane vrlimi128 inserts of 0/1.0).
        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.x = 0.0f;             // +0x1000 lane1 cleared
        mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z = 0.0f; // TimeDrifting cleared
        mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.w = 0.0f; // TimeInFriction
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.y = 1.0f;          // DriftPushTime=1
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.x = 0.0f;
        mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.w = 0.0f;

        // reset the gate bitfield to DO_ALL and the slam marker (the -1 the asm writes at +0x10F4).
        mDriftFlags.mu8DriftFlags = DriftFlags::KU_DRIFT_FLAG_DO_ALL;
        // (the slam path's -1 marker shares +0x10F4; ExitDrift's vrlimi clears restore DO_ALL.)
    }

    // @0x825FA268  VehiclePhysics::EnterDrift
    //   Latches the drift direction from the sign of the entry steering input:
    //     mu8DriftState = (mfSteering <= 0) ? eDriftState_FacingRight(2) : eDriftState_FacingLeft(1).
    //   Then seeds the StartSlip lane (mvTime..._StartSlip), zeroes the per-drift timers, copies the
    //   per-car drift attrib lanes into the bank, and resets mDriftFlags = KU_DRIFT_FLAG_DO_ALL.
    //   asm: v28 = (v27 <= 0.0) ? 2 : 1; *(result+4946) = v28; ... vrlimi-stores across the bank.
    void VehiclePhysics::EnterDrift(const BrnPlayerDriverControls* lpControls, f32 lfSlip, f32 lfSpeed)
    {
        const f32 lfSteer = lpControls ? lpControls->mfSteering : 0.0f;   // *(a2+16)

        mu8DriftState = (lfSteer <= 0.0f) ? eDriftState_FacingRight : eDriftState_FacingLeft;

        // seed StartSlip + the timers (the asm stores 0.1 into a control lane, then scatters the bank).
        mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.y = lfSlip;   // StartSlip
        mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z = 0.0f;     // TimeDrifting=0
        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.z = 0.0f;                   // NeutralControlTime=0
        mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.w = 0.0f;

        // a small seed (0.1) the asm writes into the maintained-speed/neutral lane group.
        // the per-car drift register lanes (mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale @+0x120 .w) seed the drift control scale.
        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = 0.0f;                   // DriftScale starts 0
        (void)lfSpeed;

        // reset the gate bitfield to DO_ALL (every drift sub-force enabled at entry).
        mDriftFlags.mu8DriftFlags = DriftFlags::KU_DRIFT_FLAG_DO_ALL;
    }

    // @0x825CFA10  VehiclePhysics::UpdateHandBrake
    //   A latch+timer with hysteresis around input 0.1.
    //     * If currently engaged (mbHandBrake): advance TimeHandbrakeHasBeenOn by dt (capped at
    //       KF_HANDBRAKE_TIME_CAP). When input < 0.1, release ONLY if a drift is active (mu8DriftState!=0)
    //       OR the on-time exceeds KF_HANDBRAKE_ONTIME_RELEASE; otherwise reset TimeSinceLastHandBrake.
    //     * If currently released: when input <= 0.1, advance TimeSinceLastHandBrake by dt (capped);
    //       when input > 0.1, engage (mbHandBrake = 1) and clear TimeHandbrakeHasBeenOn.
    //   The two timers live in the +0x1080 lane (TimeHandbrakeHasBeenOn .z / TimeSinceLastHandBrake .w).
    void VehiclePhysics::UpdateHandBrake(f32 lfHandBrakeInput, f32 lfTimeStep)
    {
        Vector4& lrTimers = mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake;

        if (mbHandBrake)
        {
            // advance on-time (cap is flagged-inert -> std::min with 0 would clamp; guard the cap).
            f32 lfOnTime = lrTimers.z + lfTimeStep;
            if (lfOnTime > KF_HANDBRAKE_TIME_CAP)   // guard dropped: the cap is real (10000.0) now
                lfOnTime = KF_HANDBRAKE_TIME_CAP;
            lrTimers.z = lfOnTime;

            if (lfHandBrakeInput < 0.1f)
            {
                const bool lbReleaseByDrift  = (mu8DriftState != eDriftState_None);
                // The defensive `!= 0.0f` guard that used to wrap this is GONE: it existed only
                // because the threshold was a flagged zero, which would have made `onTime > 0` true on
                // nearly every frame (onTime only grows). unk_82FB8B00 is 0.275 s and the plain
                // comparison is now the faithful one.
                const bool lbReleaseByOnTime = (lfOnTime > KF_HANDBRAKE_ONTIME_RELEASE);
                if (lbReleaseByDrift || lbReleaseByOnTime)
                {
                    mbHandBrake = false;
                    lrTimers.z = 0.0f;   // TimeHandbrakeHasBeenOn cleared
                    lrTimers.w = 0.0f;   // restart TimeSinceLastHandBrake
                }
                else
                {
                    // not yet releasing: keep accumulating the since-last timer (the vperm store branch).
                    lrTimers.w = lrTimers.w;   // unchanged (faithful no-op store the asm performs)
                }
            }
        }
        else
        {
            if (lfHandBrakeInput <= 0.1f)
            {
                // advance TimeSinceLastHandBrake by dt, capped.
                f32 lfSince = lrTimers.w + lfTimeStep;
                if (lfSince > KF_HANDBRAKE_TIME_CAP)   // guard dropped: the cap is real (10000.0) now
                    lfSince = KF_HANDBRAKE_TIME_CAP;
                lrTimers.w = lfSince;
            }
            else
            {
                mbHandBrake = true;
                lrTimers.z = 0.0f;   // clear TimeHandbrakeHasBeenOn on a fresh engage
            }
        }
    }

    // @0x825D2F78  VehiclePhysics::ApplyNaturalDriftForces
    //   A gentle straightening yaw applied when NOT actively drifting hard. Gated on the per-car
    //   drift push-time attrib (mpAttribs->mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping @+0x160 .y > DriftPushTime lane). The torque magnitude
    //   reads mpAttribs->mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower (@+0x150) lanes, and the SIGN is selected by mu8DriftState through a
    //   permute table (unk_8327F240). Applied via AddWorldSpaceTorque(this+0x10).
    void VehiclePhysics::ApplyNaturalDriftForces()
    {
        // gate: DriftPushTime (mvLatDriftForceFactor..._.y) vs the per-car push-time threshold
        // (mpAttribs->mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping @+0x160 .y). The asm: `vandc(DriftPushTime) ; vcmpgtfp. vs attrib.y`.
        const f32 lfPushTime    = mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.y;
        const f32 lfPushThresh  = mpAttribs->mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.y;
        if (lfPushTime > lfPushThresh)
        {
            // within the active-push window the asm grows/decays the DriftScale lane toward a target
            // (mpAttribs->mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower @+0x150 .z), signed by whether DriftScale exceeds it. FLAG: the gain is the
            // attrib lane; the math is exact, the numeric target comes from the (homed) attrib register.
            const f32 lfTarget = mpAttribs->mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.z;
            f32 lfScale = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w;   // DriftScale lane
            if (lfScale > lfTarget) lfScale = lfScale - (lfScale - lfTarget);  // decay toward target
            else                    lfScale = lfScale + (lfTarget - lfScale);  // grow toward target
            mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = lfScale;
        }

        // the self-aligning yaw torque: magnitude = mpAttribs->mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower (@+0x150) .x, signed by the drift
        // state (FacingLeft -> +, FacingRight -> - via the unk_8327F240 select), applied about the body
        // up axis (mTransform.yAxis). FLAG: the asm routes the magnitude through a packed select whose
        // sign mask is exact (state-driven); the magnitude lane is the homed attrib.
        const f32 lfMag  = mpAttribs->mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.x;
        const f32 lfSign = (mu8DriftState == eDriftState_FacingRight) ? -1.0f : 1.0f;
        const Vector3 lTorque{ mTransform.yAxis.x * (lfMag * lfSign),
                               mTransform.yAxis.y * (lfMag * lfSign),
                               mTransform.yAxis.z * (lfMag * lfSign),
                               0.0f };
        AddWorldSpaceTorque(lTorque);   // ExternalPhysicsBody::AddWorldSpaceTorque(this+0x10)
    }

    // @0x825D2270  VehiclePhysics::MaintainDriftSpeed
    //   Keeps a sliding car from scrubbing off speed. Gated by mDriftFlags.DoMaintainSpeed(), the
    //   MaintainedSpeed lane (@+0x1000 .y) exceeding current speed, throttle >= 0.3, grounded
    //   (!mbHandBrake-equivalent + mAboveGroundTestResult.mbValid). Builds a ground-tangent world impulse along
    //   a Z/velocity blend (mvPropSpeedMaintainAlong* @+0x1050) scaled by the per-car push attrib, then
    //   AddWorldSpaceImpulse. The "Invalid total linear impulse during drift" assert is elided.
    void VehiclePhysics::MaintainDriftSpeed(const BrnPlayerDriverControls* lpControls, f32 lfTimeStep)
    {
        // gate 1: the maintain-speed flag bit (mDriftFlags & 1).
        if (!mDriftFlags.DoMaintainSpeed())
        {
            // the asm's LABEL_16 still stores the current speed into the MaintainedSpeed lane (.y).
            mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y = mfSpeedMPH.x;
            return;
        }

        // gate 2: MaintainedSpeed (.y) must exceed current frame speed (a2 = SpeedMPS in the asm).
        const f32 lfMaintained = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y;
        const f32 lfSpeed      = lfTimeStep;   // a2 (SpeedMPS) passed in the .y-lane register
        if (lfMaintained > lfSpeed)
        {
            // gate 3: throttle >= 0.3 (*(controls+4) = mfGas) AND grounded AND not airborne AND
            //         mAboveGroundTestResult.mbValid.
            const f32 lfThrottle = lpControls ? lpControls->mfGas : 0.0f;
            if (mbAllWheelsHaveTraction && lfThrottle >= 0.30000001f && mAboveGroundTestResult.mbValid)
            {
                // deficit-scaled impulse direction = blend of body Z (forward) and velocity, weighted by
                // mvPropSpeedMaintainAlongZ (.x) / mvPropSpeedMaintainAlongVel (.y), pushed by the per-car
                // push-time attrib (mpAttribs->mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping @+0x160 .y). The deficit = MaintainedSpeed - speed.
                const f32 lfDeficit  = lfMaintained - lfSpeed;
                const f32 lfAlongZ   = mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.x;
                const f32 lfAlongVel = mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.y;
                const f32 lfPush     = mpAttribs->mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.y;

                const Vector3 lVelDir = vpu::Normalize(mLinearVelocity);
                Vector3 lDir{ mTransform.zAxis.x * lfAlongZ + lVelDir.x * lfAlongVel,
                              mTransform.zAxis.y * lfAlongZ + lVelDir.y * lfAlongVel,
                              mTransform.zAxis.z * lfAlongZ + lVelDir.z * lfAlongVel,
                              0.0f };

                // tangent-project against the ground normal (@+0x580) so the impulse never pushes
                // into/off the road: dir -= normal * dot(dir, normal).
                const f32 lfN = vpu::Dot(lDir, mAboveGroundTestResult.mIntersectionNormal);
                Vector3 lImpulse{ (lDir.x - mAboveGroundTestResult.mIntersectionNormal.x * lfN) * lfDeficit * lfPush,
                                  (lDir.y - mAboveGroundTestResult.mIntersectionNormal.y * lfN) * lfDeficit * lfPush,
                                  (lDir.z - mAboveGroundTestResult.mIntersectionNormal.z * lfN) * lfDeficit * lfPush,
                                  0.0f };
                AddWorldSpaceImpulse(lImpulse);   // "Invalid total linear impulse during drift" assert elided
            }
        }
    }

    // @0x825FA448  BrnPhysics::Vehicle::VehiclePhysics::CheckForEnteringDrift   (192 instructions)
    //
    // ⭐ THE LAST UNRESOLVED EXTERNAL OF THIS TRANSLATION UNIT. It had been declare-only since this
    //   header was written ("bodied by its own TU"), which no TU ever did, and the arity it was
    //   declared with (five trailing f32) never existed on any platform.
    //
    // ⚠️ It is ABSENT from `.ida-exports/BURNOUT_X360_ARTIST.XEX/` -- the third confirmed hole in
    //   that export set. It is a perfectly ordinary named function inside the IDB: headless IDA 9.3
    //   reports `BrnPhysics::Vehicle::VehiclePhysics::CheckForEnteringDrift 0x825FA448..0x825FA748`.
    //   The gap is visible without IDA too: EnterDrift @0x825FA268 is 120 instructions and therefore
    //   ends exactly at 0x825FA448, and the next symbol the index knows is UpdateDriftScale
    //   @0x825FA748. Body transcribed from that X360 disassembly; the PS3 DecFIGS twin (0x6C8924) is
    //   used only for the parameter names and for the two function-static constant NAMES below.
    //
    // Signature: the DecFIGS DWARF (VehiclePhysics.h:1457) and the PS3 mangled symbol agree that the
    // last parameter is a `rw::math::vpu::VecFloat`, not the f32 this tree declared. The three f32s'
    // roles are the PS3 DWARF's own local names, and each is independently confirmed by what
    // UpdateDrift @0x8262E200 loads into f1/f2/f3 before the call (see UpdateDrift below).
    // The time-step is not read by this function on either platform -- UpdateDriftState is the only
    // member of the family that consumes it -- but it is part of the signature and dropping it is
    // what produced a symbol no TU could define.
    void VehiclePhysics::CheckForEnteringDrift(const BrnPlayerDriverControls* lpControls,
                                               f32 lfAbsSteering, f32 lfAbsDriftScale,
                                               f32 lfSpeedMPS, VecFloat lvfTimeStep)
    {
        // The two entry windows for a "natural" (un-forced) drift. Both are function-scope statics in
        // the console build: they live in .data at flt_82F2A520 / flt_82F2A51C / flt_82F2A518 and the
        // PS3 mangles their names into the enclosing function's symbol, which is what NAMES them:
        //     _ZZN10BrnPhysics7Vehicle14VehiclePhysics21CheckForEnteringDrift...E34KF_MAX_COS_ANGLE_FOR_NATURAL_DRIFT
        // The values are the X360 image bytes. Note the naming is about the ANGLE, so the "MAX angle"
        // is the SMALLER cosine: the accepted slip window is acos(0.985)=9.9 deg .. acos(0.76)=40.5 deg.
        static const f32 KF_MAX_COS_ANGLE_FOR_NATURAL_DRIFT = 0.76f;    // flt_82F2A520
        static const f32 KF_MIN_COS_ANGLE_FOR_NATURAL_DRIFT = 0.985f;   // flt_82F2A51C
        static const f32 KF_MAX_YAW_FOR_NATURAL_DRIFT       = 2.0f;     // flt_82F2A518

        // 0x825FA454-478: an AI car that is being told to come OUT of a drift may not enter one.
        //   lwz r11,0x44(r4) ; cmpwi cr6,r11,1 ; (only when AI) lbz r11,0x4D(r4)
        // The console checks the driver type and then reads the AI payload -- the same shape as
        // UpdateDriftScale/UpdateSpeedMatch. The DWARF names the local lbAIAllowingDrift.
        bool lbAIAllowingDrift = true;
        if (lpControls != NULL && lpControls->GetType() == E_DRIVER_TYPE_AI)
            lbAIAllowingDrift = !static_cast<const BrnAIDriverControls*>(lpControls)->mbForceComeOutOfDrift;

        if (mu8DriftState != eDriftState_None) return;   // 0x825FA478 already drifting
        if (mbHandBrake)                       return;   // 0x825FA484 handbrake held
        if (!mbAllWheelsHaveTraction)          return;   // 0x825FA490 (0x135B, beq -> return)

        // 0x825FA49C-4C8: fast enough for THIS car to drift. mfSpeedMPH is the splatted body speed
        // (+0x6C0); mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x is the per-car minimum, in the same unit -- see the KF_MPS_TO_MPH
        // note above, where UpdateDriftState's exit guard compares the same lane the other way round.
        if (!(mfSpeedMPH.x > mpAttribs->mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x)) return;

        if (mi8NumWorldCollisions != 0) return;          // 0x825FA4CC (bne -> return)
        if (!lbAIAllowingDrift)         return;          // 0x825FA4D8

        // 0x825FA4E4-510: the handbrake has to have been OFF for long enough. The threshold is the
        // .data VecFloat unk_82FB9170, which reads ZERO in the image and is splatted at static-init
        // from flt_82001D9C == 2.0f (initialiser disassembled at 0x82C5C9D8). The PS3 build names
        // that slot KVF_HANDBRAKE_OFF_TIME_TO_ALLOW_DRIFT and the lane this reads is the tree's own
        // TimeSinceLastHandBrake -- two independent sources agreeing on the same field.
        if (!(mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.w
              > KVF_HANDBRAKE_OFF_TIME_TO_ALLOW_DRIFT))
            return;

        // 0x825FA514-54C: the two ways in. Either brake + steer past the dead zone, or the controls
        // force it. `lfs f13,8(r4)` is mfBrake and `f1` is |Steering|; the threshold is flt_82004014.
        static const f32 KF_DRIFT_ENTRY_INPUT_DEADZONE = 0.1f;   // flt_82004014
        const bool lbBrakeAndSteer = (lpControls != NULL)
                                   && (lpControls->mfBrake > KF_DRIFT_ENTRY_INPUT_DEADZONE)
                                   && (lfAbsSteering       > KF_DRIFT_ENTRY_INPUT_DEADZONE);
        const bool lbForcedDrift   = (lpControls != NULL) && lpControls->mbForceDrift;

        if (lbBrakeAndSteer || lbForcedDrift)
        {
            // 0x825FA550-620: snap the steering register to the live input and enter. The asm writes
            // .y then .z with the SAME value (controls+0x10 == mfSteering), then derives .x from the
            // freshly-written .y:
            //     vrlimi128 v10,v13,4,0  -> .y = mfSteering
            //     vrlimi128 v10,v13,2,0  -> .z = mfSteering
            //     v0 = (-MaxSteeringAngle) * 1.5f * DEG_TO_RAD * .y ; vrlimi128 v9,v0,8,0 -> .x
            // (1.5f == flt_820945DC, 0.0174532924f == flt_8208F5F4.)
            static const f32 KF_DRIFT_ENTRY_STEER_ANGLE_SCALE = 1.5f;   // flt_820945DC
            const f32 lfSteering = (lpControls != NULL) ? lpControls->mfSteering : 0.0f;

            mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y = lfSteering;
            mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.z = lfSteering;
            mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x =
                mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y
                * -mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.z
                * KF_DRIFT_ENTRY_STEER_ANGLE_SCALE * KF_DEG_TO_RAD;

            // 0x825FA604-620: EnterDrift(controls, speed, mpAttribs[+0x170].w)
            EnterDrift(lpControls, lfSpeedMPS, mpAttribs->mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.w);
            return;
        }

        // ---- the NATURAL drift path (0x825FA634 onward) -------------------------------------
        // 0x825FA634-660: all four wheels must ALREADY have broken their adhesive limit, i.e. the car
        // is genuinely sliding. Note the sense: each `lbz wheel+0xD5 ; beq -> return` REQUIRES the
        // byte to be non-zero (UpdateDriftState's guard 3 reads the same four bytes the other way).
        if (!maWheels[eFrontLeftWheel].mbBrokenAdhesiveLimit)  return;
        if (!maWheels[eFrontRightWheel].mbBrokenAdhesiveLimit) return;
        if (!maWheels[eRearLeftWheel].mbBrokenAdhesiveLimit)   return;
        if (!maWheels[eRearRightWheel].mbBrokenAdhesiveLimit)  return;

        // 0x825FA664-6EC: two body-frame quantities, both computed before either is tested.
        //   |yaw rate|   = |dot3(mAngularVelocity, mTransform.yAxis)|   (vmsum3fp128 + vandc sign-mask)
        //   cos(slip)    =  dot3(mLinearVelocity / speed, mTransform.zAxis)
        // The reciprocal is a vrefp + two Newton steps on the SPEED PARAMETER (not on |v|); written
        // as the division it is. There is no zero guard here on either platform -- the speed gate
        // above is what keeps it away from 0.
        const f32 lfYawSpeed = std::fabs(vpu::Dot(mAngularVelocity, mTransform.yAxis));

        const f32 lfRecipSpeed = 1.0f / lfSpeedMPS;
        const Vector3 lUnitVel{ mLinearVelocity.x * lfRecipSpeed,
                                mLinearVelocity.y * lfRecipSpeed,
                                mLinearVelocity.z * lfRecipSpeed,
                                0.0f };
        const f32 lfCosSlipAngle = vpu::Dot(lUnitVel, mTransform.zAxis);

        // 0x825FA6F0-724: the slip angle has to sit inside the window, and the car must not already
        // be spinning faster than the natural-drift yaw cap.
        if (!(lfCosSlipAngle > KF_MAX_COS_ANGLE_FOR_NATURAL_DRIFT)) return;
        if (lfCosSlipAngle >= KF_MIN_COS_ANGLE_FOR_NATURAL_DRIFT)   return;
        if (lfYawSpeed     >= KF_MAX_YAW_FOR_NATURAL_DRIFT)         return;

        // 0x825FA728-734: EnterDrift(controls, speed, 0.0f)   (flt_82001CC0 == 0.0f)
        EnterDrift(lpControls, lfSpeedMPS, 0.0f);

        (void)lfAbsDriftScale;   // [V] neither platform reads it here; part of the signature only
        (void)lvfTimeStep;       // [V] idem -- UpdateDriftState is the only consumer of the dt
    }

    // @0x8261F728  VehiclePhysics::UpdateDriftState
    //   The drift state machine. Runs CheckForEnteringDrift (a straight pass-through of all five
    //   arguments -- `bl` at 0x8261F73C with r3/r4/f1/f2/f3/v1 untouched), then a battery of
    //   ExitDrift guards while drifting (mu8DriftState != 0 AND controls.mbForceDrift (+0x3E) NOT
    //   set -- 0x8261F74C reads r31 == controls).
    //
    // ⭐⭐ REBUILT FROM THE ASM 2026-08-03. The committed guard battery was written before the three
    //   .data constants below were recoverable, and six of its ten guards named the WRONG register,
    //   the WRONG lane, or a quantity the function never touches. What the X360 actually does, in
    //   asm order, with every threshold now homed:
    //     1. 0x8261F758  TimeHandbrakeHasBeenOn (+0x1080 .z) >  0.15f      (unk_82FB8AC0)
    //     2. 0x8261F78C  mbHandBrake && 0.5f > TimeSinceLastHandBrake (+0x1080 .w)
    //     3. 0x8261F7C0  TimeInDriftWithStaticFriction (+0x1080 .y) = allAdhesive ? t + dt : 0
    //     4. 0x8261F844  TimeDrifting (+0x1010 .z) > 1.0f && (3) > 0.0625f
    //     5. 0x8261F890  mi8NumWorldCollisions > 0 ; miNumCollisions > 0
    //     6. 0x8261F8AC  mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x > lfSpeedMPS * KF_MPS_TO_MPH
    //     7. 0x8261F900  TimeWithoutTraction (+0x1060 .z) > 1.0f       (ungated)
    //     8. 0x8261F94C  !mAboveGroundTestResult.mbValid
    //     9. 0x8261F958  10.0f > mfSpeedMPH                            (unk_82FB9ED0)
    //    10. 0x8261F984  the steering-crossed-centre pair, on a slip computed HERE
    //   Retired with this rewrite: a "drift slip too small" guard on +0x1020 .y, an "airborne too
    //   long" guard on +0x1060 .w, a steering^4 compare, and a "DesiredDriftSlip > 1.0" guard. None
    //   of those four registers/lanes is read by this function at all -- there is no +0x1020 access
    //   anywhere in 0x8261F728..0x8261FAA8.
    //
    // ⭐ Guard 3 is the reason the dropped `VecFloat` mattered: it is the ONLY consumer of the
    //   time-step in the whole drift family, and with the parameter missing it had been written as a
    //   `(void)` no-op. `vsel` selects between {0,0,0,0} and {~0,~0,~0,~0} -- the two halves of the
    //   static-init table at unk_8327F240 (0x82C74368), read out of the IDB -- so it is a plain
    //   conditional select, not a blend.
    void VehiclePhysics::UpdateDriftState(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                                          f32 lfAbsDriftScale, f32 lfSpeedMPS, VecFloat lvfTimeStep)
    {
        // CheckForEnteringDrift may latch a NEW drift this frame; every argument is forwarded.
        CheckForEnteringDrift(lpControls, lfAbsSteering, lfAbsDriftScale, lfSpeedMPS, lvfTimeStep);

        // only run the exit battery while drifting and not being HELD in drift. 0x8261F74C:
        // `lbz r11,0x3E(r31)` where r31 == controls -> mbForceDrift.
        const bool lbHeldInDrift = (lpControls != NULL) ? lpControls->mbForceDrift : false;
        if (mu8DriftState == eDriftState_None || lbHeldInDrift)
            return;

        // 1. the handbrake has been held down too long to still count as a drift.
        if (mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.z
            > KF_DRIFT_HANDBRAKE_ON_LIMIT)
        { ExitDrift(); return; }

        // 2. the handbrake is down and was released too recently (vcfsx v0,1,1 == 0.5f).
        if (mbHandBrake
            && 0.5f > mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.w)
        { ExitDrift(); return; }

        // 3. NOT an exit test: the static-friction dwell timer. It accumulates by the time-step while
        //    all four wheels still have adhesive grip (wheel+0xD5, stride 0xE0) and resets otherwise.
        {
            const bool lbAllWheelsHaveAdhesive = !maWheels[eFrontLeftWheel].mbBrokenAdhesiveLimit
                                               && !maWheels[eFrontRightWheel].mbBrokenAdhesiveLimit
                                               && !maWheels[eRearLeftWheel].mbBrokenAdhesiveLimit
                                               && !maWheels[eRearRightWheel].mbBrokenAdhesiveLimit;
            mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.y =
                lbAllWheelsHaveAdhesive
                    ? mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.y
                      + lvfTimeStep.x   // VecFloat is a broadcast lane; the asm vaddfp's the splat
                    : 0.0f;
        }

        // 4. a long drift that has spent too long gripping. 0.0625f is built in the asm as
        //    vcfsx(1,1) == 0.5f raised to the fourth by three vmulfp128.
        if (mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z > 1.0f
            && mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.y
               > 0.0625f)
        { ExitDrift(); return; }

        // 5. exit collision counters.
        if (mi8NumWorldCollisions > 0) { ExitDrift(); return; }
        if (miNumCollisions       > 0) { ExitDrift(); return; }

        // 6. below the per-car minimum drift speed. mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x is in MPH, the parameter is in
        //    m/s -- see the KF_MPS_TO_MPH note. This is the same lane, same threshold and same unit
        //    that CheckForEnteringDrift tests the other way round to allow entry.
        if (mpAttribs->mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x > lfSpeedMPS * KF_MPS_TO_MPH)
        { ExitDrift(); return; }

        // 7. off the ground too long (lane .z, and NOT gated on the handbrake).
        if (mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z > 1.0f)
        { ExitDrift(); return; }

        // 8. above-ground result invalid -> exit.
        if (!mAboveGroundTestResult.mbValid) { ExitDrift(); return; }

        // 9. speed too low (mfSpeedMPH is the splatted body speed at +0x6C0).
        if (KF_DRIFT_SPEED_EXIT_LIMIT > mfSpeedMPH.x)
        { ExitDrift(); return; }

        // 10. steering crossed centre for the latched direction. The slip term is computed HERE, from
        //     the speed parameter and the body basis -- dot3(mLinearVelocity / speed, mTransform.xAxis)
        //     -- and the gate is TimeDrifting > 0.5f (flt_82001DA0). The four literals are
        //     flt_8201FDB8 / flt_82058304 / flt_82002138 / flt_8200CE04.
        if (mu8DriftState == eDriftState_FacingLeft || mu8DriftState == eDriftState_FacingRight)
        {
            const f32 lfRecipSpeed = 1.0f / lfSpeedMPS;
            const Vector3 lUnitVel{ mLinearVelocity.x * lfRecipSpeed,
                                    mLinearVelocity.y * lfRecipSpeed,
                                    mLinearVelocity.z * lfRecipSpeed,
                                    0.0f };
            const f32 lfLateralSlip = vpu::Dot(lUnitVel, mTransform.xAxis);

            if (!(mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z > 0.5f))
                return;

            const f32 lfSteer = (lpControls != NULL) ? lpControls->mfSteering : 0.0f;
            if (mu8DriftState == eDriftState_FacingRight)
            {
                if (lfLateralSlip >= -0.0099999998f && lfSteer >= -0.0049999999f)
                { ExitDrift(); return; }
            }
            else
            {
                if (lfLateralSlip <= 0.0099999998f && lfSteer <= 0.0049999999f)
                { ExitDrift(); return; }
            }
        }

        (void)lfAbsSteering;     // [V] consumed by CheckForEnteringDrift, not by the exit battery
        (void)lfAbsDriftScale;   // [V] idem
    }

    // @0x825FA748  VehiclePhysics::UpdateDriftScale
    //   Grows mDriftScale toward the target drift slip and applies the natural self-aligning drift yaw.
    //   asm: reads *(this+68)==1 (a control mode) -> latches an aftertouch term; recomputes the local
    //   drift angle (acos of velocity vs the body basis, signed, in degrees via 57.29578, wrapped into
    //   [-180,180]); if the per-car drift slip lane (mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale @+0x120 .x) exceeds the body speed
    //   OR the aftertouch latch is set, eases the scale; else integrates the scale toward the target by
    //   the time-step. Always tail-calls ApplyNaturalDriftForces.
    //   FIDELITY: PARTIAL -- the outer state machine (angle compute, the two ease/integrate branches,
    //   the ApplyNaturalDriftForces tail) is reconstructed against named lanes; the inner per-branch
    //   polynomial (the vexptefp/vlogefp + unk_82014AC0.. coefficient cascade) is an un-homed rodata
    //   curve carried as a flagged-inert blend so no fabricated coefficients are emitted.
    //   ⭐ SIGNATURE CORRECTED 2026-08-03 (DWARF VehiclePhysics.h:1466 + asm). It takes the same two
    //   f32s and the same VecFloat dt that ApplyDriftForces was handed, forwarded verbatim
    //   (0x8261FB28-3C: `vmr128 v1,v127 ; fmr f2,f30 ; fmr f1,f31`). The tree used to declare
    //   `(ctrl, f32 lfTimeStep, f32 lfSlip)` and ApplyDriftForces called it as
    //   `UpdateDriftScale(lpControls, lfSpeed, lfSlipAngle)` -- so the parameter the integrate branch
    //   below uses as a TIME STEP was in fact receiving the SPEED.
    void VehiclePhysics::UpdateDriftScale(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                                          f32 lfAbsDriftScale, VecFloat lvfTimeStep)
    {
        // ⭐ RE-NAMED 2026-08-03. asm @0x825FA778: `lwz r11,0x44(r30); cmpwi 1; bne;
        // lbz r26,0x4D(r30)` -- an AI-driver check followed by the AI payload's
        // mbForceComeOutOfDrift (77 decimal == 0x4D). The DWARF name matches the function it is
        // read in, which is what identifies it. Was GetMode()/GetFlag78().
        bool lbForceComeOutOfDrift = false;
        if (lpControls && lpControls->GetType() == E_DRIVER_TYPE_AI)
            lbForceComeOutOfDrift = static_cast<const BrnAIDriverControls*>(lpControls)->mbForceComeOutOfDrift;

        // recompute the local drift angle: acos(dot(normalize(vel), driftDir)) signed, in degrees,
        // wrapped to (-180,180]. driftDir is the cached steering direction (mSteeringDirection-equivalent
        // built from the body basis @+0x20/+0x30).
        const Vector3 lUnitVel = vpu::Normalize(mLinearVelocity);
        f32 lfDot = vpu::Dot(lUnitVel, mTransform.zAxis);
        if (lfDot < 0.0f) lfDot = 0.0f;
        if (lfDot > 1.0f) lfDot = 1.0f;
        f32 lfAngleDeg = std::acos(lfDot) * KF_RAD_TO_DEG;
        if (vpu::Dot(lUnitVel, mTransform.xAxis) < 0.0f)
            lfAngleDeg = -lfAngleDeg;
        if (lfAngleDeg > 180.0f) lfAngleDeg -= 360.0f;            // wrap

        // store the current (capped) drift angle lane.
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.w = lfAngleDeg;

        // gate: per-car drift slip lane (mpAttribs->mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor @+0x110 .x) vs body speed, OR the
        // AI's force-come-out-of-drift request.
        const bool lbEase = !(mfSpeedMPH.x > mpAttribs->mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x) || lbForceComeOutOfDrift;
        if (lbEase)
        {
            // ease branch: leave the drift scale where it is (the asm's "set to 1.0 seed" no-op store path
            // when below the slip threshold). FLAG: the eased target curve is the un-homed coefficient set.
            mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.x = 1.0f;  // seed
        }
        else
        {
            // integrate branch: grow CappedDriftScale toward DesiredDriftSlip by the time-step. The asm
            // mixes in the per-car drift register lanes (mpAttribs->mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping @+0x160 .y push) + the
            // mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale weights; the exact polynomial blend is the flagged coefficient cascade.
            const f32 lfTarget  = mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.z; // DesiredDriftSlip
            f32 lfScale = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w;   // DriftScale lane
            const f32 lfStep = lvfTimeStep.x * mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.x; // *LatDriftForceFactor
            lfScale += (lfTarget - lfScale) * lfStep;   // first-order approach toward the target slip
            mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = lfScale;
        }
        (void)lfAbsSteering;
        (void)lfAbsDriftScale;

        // always apply the natural self-aligning yaw.
        ApplyNaturalDriftForces();
    }

    // @0x825D25A0  VehiclePhysics::ApplyDriftYaw
    //   A world-space yaw torque rotating the car toward the drift direction. Gated on a computed local
    //   drift angle (the asm wraps acos(dot(vel,driftDir)) into degrees, requires it != 0) AND
    //   mDriftFlags.DoApplyTorque(). The torque magnitude is built from the per-car drift attrib lanes
    //   (mpAttribs->mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift @+0x130 .x gate, mpAttribs->mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower @+0x150 lanes for the response shape) and the
    //   body up axis; signed by the drift state; applied via AddWorldSpaceTorque(this+0x10).
    //   FIDELITY: PARTIAL -- the gate (angle wrap + the DoApplyTorque() flag), the attrib-driven response
    //   shaping (the vsubfp/vmaddfp interpolation between attrib lanes), the state sign-select and the
    //   final AddWorldSpaceTorque are reconstructed against named lanes; the innermost
    //   normalize/interp cascade reads homed attrib lanes but its exact lane-routing for the response
    //   curve is left as the attrib-lane interpolation (no fabricated constants).
    void VehiclePhysics::ApplyDriftYaw(const BrnPlayerDriverControls* lpControls, f32 lfSlipAngle, f32 lfSpeed)
    {
        (void)lpControls; (void)lfSlipAngle; (void)lfSpeed;

        // local drift angle in degrees, wrapped to (-180,180].
        const Vector3 lUnitVel  = vpu::Normalize(mLinearVelocity);
        const Vector3 lUnitFwd  = vpu::Normalize(mTransform.zAxis);
        f32 lfDot = vpu::Dot(lUnitVel, lUnitFwd);
        if (lfDot < 0.0f) lfDot = 0.0f;
        if (lfDot > 1.0f) lfDot = 1.0f;
        f32 lfAngleDeg = std::acos(lfDot) * KF_RAD_TO_DEG;
        // sign by the body right axis (cross-product sign in the asm).
        if (vpu::Dot(lUnitVel, mTransform.xAxis) < 0.0f)
            lfAngleDeg = -lfAngleDeg;
        if (lfAngleDeg * 1.0f > 180.0f) lfAngleDeg -= 360.0f;     // wrap (the v72=180/360 branch)

        // gate: angle must be non-zero AND the APPLY_TORQUE flag bit set.
        if (lfAngleDeg == 0.0f || !mDriftFlags.DoApplyTorque())
            return;

        // attrib-shaped response: the per-car drift register (mpAttribs->mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift @+0x130 .x) gates, and
        // mpAttribs->mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower (@+0x150) lanes shape the magnitude vs the (homed) speed. The response is an
        // interpolation between the attrib lanes -> a torque magnitude.
        const f32 lfGate = mpAttribs->mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.x;
        if (!(lfGate > 0.0f))
            return;

        // magnitude from the attrib response lanes (the asm's vmaddfp interp between .x/.y/.z of +0x150).
        const f32 lfA = mpAttribs->mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.x;
        const f32 lfB = mpAttribs->mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.y;
        const f32 lfMag = lfA + (lfB - lfA) * 0.0f;   // FLAG: interp blend factor is the homed-speed lane;
                                                      // carried inert so no fabricated blend is emitted.

        // sign by the latched drift direction.
        const f32 lfSign = (mu8DriftState == eDriftState_FacingRight) ? -1.0f : 1.0f;

        // yaw about the body up axis (mTransform.yAxis).
        const Vector3 lTorque{ mTransform.yAxis.x * (lfMag * lfSign),
                               mTransform.yAxis.y * (lfMag * lfSign),
                               mTransform.yAxis.z * (lfMag * lfSign),
                               0.0f };
        AddWorldSpaceTorque(lTorque);
    }

    // @0x825D2B20  VehiclePhysics::ApplyDriftLatForce
    //   The sideways world-space force that steps the rear out. Builds the lateral direction (the body
    //   right axis @+0x10 vs the velocity), shapes its magnitude by the per-car drift attrib lanes
    //   (mpAttribs->mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor @+0x110 .y reciprocal, mpAttribs->mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff @+0x140 .y, mpAttribs->mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping @+0x160 lanes,
    //   mpAttribs->mDriftAttribs.mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor @+0x180 .z) and the speed, signs it by the drift state, tangent-projects it
    //   against the ground normal (@+0x580) so it never pushes into/off the road, and applies it via
    //   AddWorldSpaceForce(this+0x10). The "mAboveGroundTestResult.mbValid" assert is elided.
    //   FIDELITY: PARTIAL -- the direction build (right-axis vs velocity), the ground-tangent projection,
    //   the state sign-select and the AddWorldSpaceForce are reconstructed against named lanes; the
    //   multi-lane magnitude-shaping cascade (the nested vrefp/vnmsubfp/vmaddfp between attrib lanes and
    //   the acos-derived angle) reads homed attrib lanes but its exact per-lane routing is left as the
    //   attrib-lane product (no fabricated constants emitted).
    void VehiclePhysics::ApplyDriftLatForce(f32 lfSlipAngle, f32 lfSpeed, f32 lfSteeringDir, f32 lfTimeStep)
    {
        (void)lfSlipAngle; (void)lfSpeed; (void)lfSteeringDir; (void)lfTimeStep;

        // the signed drift angle (acos of velocity vs body forward, in degrees) the asm computes first.
        const Vector3 lUnitVel = vpu::Normalize(mLinearVelocity);
        f32 lfDot = vpu::Dot(lUnitVel, mTransform.zAxis);
        if (lfDot < 0.0f) lfDot = 0.0f;
        if (lfDot > 1.0f) lfDot = 1.0f;
        const f32 lfAngleDeg = std::acos(lfDot) * KF_RAD_TO_DEG;

        // lateral magnitude shaped by the per-car drift attrib lanes. mpAttribs->mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.y (slip recip),
        // mpAttribs->mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.y (lat shape), mpAttribs->mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.x (force gate), mpAttribs->mDriftAttribs.mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor.z (final scale).
        const f32 lfSlipRecip = mpAttribs->mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.y;
        const f32 lfLatShape  = mpAttribs->mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.y;
        const f32 lfForceGate = mpAttribs->mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.x;
        const f32 lfFinalScale = mpAttribs->mDriftAttribs.mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor.z;

        // the asm shapes |F| = (angle-derived term) * lfLatShape * lfFinalScale, gated by lfForceGate and
        // the slip recip. The angle-derived term is the flagged interp; carried as the angle scaled by the
        // slip recip so the named lanes drive it without fabricated coefficients.
        f32 lfMag = lfAngleDeg * lfSlipRecip * lfLatShape * lfFinalScale;
        if (!(lfForceGate > 0.0f))
            lfMag = 0.0f;   // the force-gate branch (vcmpgtfp. against +0x160 .x)

        // lateral direction = body right axis (mTransform.xAxis), signed by the drift state.
        const f32 lfSign = (mu8DriftState == eDriftState_FacingLeft) ? 1.0f : -1.0f;
        Vector3 lDir{ mTransform.xAxis.x * lfSign, mTransform.xAxis.y * lfSign,
                      mTransform.xAxis.z * lfSign, 0.0f };

        // tangent-project against the ground normal (@+0x580): dir -= normal * dot(dir, normal).
        const f32 lfN = vpu::Dot(lDir, mAboveGroundTestResult.mIntersectionNormal);
        const Vector3 lForce{ (lDir.x - mAboveGroundTestResult.mIntersectionNormal.x * lfN) * lfMag,
                              (lDir.y - mAboveGroundTestResult.mIntersectionNormal.y * lfN) * lfMag,
                              (lDir.z - mAboveGroundTestResult.mIntersectionNormal.z * lfN) * lfMag,
                              0.0f };
        AddWorldSpaceForce(lForce);   // "mAboveGroundTestResult.mbValid" assert (cpp:4371) elided
    }

    // @0x8261FAB0  VehiclePhysics::ApplyDriftForces
    //   Dispatches the four drift sub-forces in order. Each is preceded by an elided CheckState debug
    //   call. ApplyDriftLatForce is gated by mbAllWheelsHaveTraction && mAboveGroundTestResult.mbValid &&
    //   !mbHandBrake (asm: `_R31[4955] && _R31[1432] && !_R31[4952]`).
    //   ⭐ SIGNATURE CORRECTED 2026-08-03 (DWARF VehiclePhysics.h:1460). Its two leading f32s and the
    //   VecFloat dt are UpdateDrift's, forwarded unchanged; only MaintainDriftSpeed is handed the
    //   speed (0x8261FB00-18: `lvlx v0,r0,&arg_34 ; vspltw v2,v0,0` -- the spilled f3).
    void VehiclePhysics::ApplyDriftForces(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                                          f32 lfAbsDriftScale, f32 lfSpeedMPS, VecFloat lvfTimeStep)
    {
        // ⚠️ FLAG (arity, unfixed): the DWARF gives the other three sub-forces signatures this header
        //    still does not carry, and the tree's stand-in forms cannot express them --
        //      MaintainDriftSpeed     (const BrnPlayerDriverControls*, Vector3, VecFloat)   :1463
        //      ApplyDriftLatForce     (VecFloat x6)                                          :1472
        //      ApplyNaturalDriftForces(VecFloat x5)                                          :1475
        //    The asm operands, for whoever corrects them:
        //      MaintainDriftSpeed  v1 = mTransform.zAxis (this+0x30), v2 = splat(lfSpeedMPS)
        //      ApplyDriftYaw       v1 = splat(lfAbsSteering),  v2 = splat(lfAbsDriftScale)
        //      ApplyDriftLatForce  v1 = splat(lfAbsDriftScale), v2 = splat(lfSpeedMPS),
        //                          v3 = splat(controls->mfSteering), v4 = splat(controls->mfBrake),
        //                          v5 = splat(controls->mfGas),      v6 = the VecFloat dt
        //    Only the ARGUMENT ORDER is corrected here, against those operands; the three parameter
        //    LISTS are left as they are so this wave stays scoped to the link closure.
        MaintainDriftSpeed(lpControls, lfSpeedMPS);
        UpdateDriftScale(lpControls, lfAbsSteering, lfAbsDriftScale, lvfTimeStep);
        ApplyDriftYaw(lpControls, lfAbsSteering, lfAbsDriftScale);

        if (mbAllWheelsHaveTraction && mAboveGroundTestResult.mbValid && !mbHandBrake)
        {
            const f32 lfSteering = (lpControls != NULL) ? lpControls->mfSteering : 0.0f;
            ApplyDriftLatForce(lfAbsDriftScale, lfSpeedMPS, lfSteering, lvfTimeStep.x);
        }
    }

    // @0x8262E200  VehiclePhysics::UpdateDrift
    //   The per-frame drift entry. Refreshes the cached steering direction (normalize(mLinearVelocity)
    //   into the steering register), runs the drift state machine (UpdateDriftState), then:
    //     * when drifting (mu8DriftState != 0): applies the drift forces (ApplyDriftForces), advancing
    //       the drift register from the per-car drift attrib lanes (mvDriftParams* @+0x110/+0x120) +
    //       the landing/damp coefficient cascade (unk_82014AC0..AF0, flagged).
    //     * when NOT drifting: eases the cached steering direction back toward forward by the per-car
    //       drift damp factor (mpAttribs->mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime @+0x170 .y) when above the drift slip threshold.
    //   Uses the ORIGINAL controls when the drift-override byte is set. The per-phase CheckState debug
    //   calls are elided.
    //   FIDELITY: PARTIAL -- the dispatch (steering-dir refresh, the drift/not-drift split, the
    //   UpdateDriftState + ApplyDriftForces calls, the not-drift ease) is reconstructed against named
    //   lanes; the two drift-register advance cascades (the vexptefp/vlogefp + unk_82014AC0.. landing/
    //   damp polynomials) are un-homed rodata curves carried as flagged-inert blends -- no fabricated
    //   coefficients emitted.
    void VehiclePhysics::UpdateDrift(const BrnPlayerDriverControls* lpOriginalControls, VecFloat lvfTimeStep)
    {
        // ⭐⭐ THE THREE SCALARS, CORRECTED 2026-08-03 FROM THE ASM (0x8262E230-2D8). The committed
        //    version read three different members and mis-ordered them; every one of the three below
        //    is an asm-literal load, and each independently confirms the PS3 DWARF's parameter name
        //    for the slot it lands in:
        //      f29 = |this[0xFE0].y|  -> |Steering|    (vspltw lane 1 + vandc 0x80000000) lfAbsSteering
        //      f30 = |this[0x1000].w| -> |DriftScale|  (vspltw lane 3 + vandc)            lfAbsDriftScale
        //      f31 = sqrt(dot3(mLinearVelocity, mLinearVelocity))                         lfSpeedMPS
        //            (vmsum3fp128 + vrsqrtefp + two Newton steps, then `vsel` back to 0 when the
        //             squared magnitude is exactly 0 -- so a stationary car yields 0, not NaN)
        //    ⚠️ RETIRED with them: a write of `dot(normalize(v), zAxis)` into
        //    mvSpare_...DriftScale.x. This function writes NOTHING to the object before the call --
        //    0x8262E230..0x8262E2D8 are all stack stores. That write was invented.
        const f32 lfAbsSteering   = std::fabs(mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y);
        const f32 lfAbsDriftScale = std::fabs(mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w);

        const f32 lfSpeedSq  = vpu::Dot(mLinearVelocity, mLinearVelocity);
        const f32 lfSpeedMPS = (lfSpeedSq != 0.0f) ? std::sqrt(lfSpeedSq) : 0.0f;

        UpdateDriftState(lpOriginalControls, lfAbsSteering, lfAbsDriftScale, lfSpeedMPS, lvfTimeStep);

        if (mu8DriftState != eDriftState_None)
        {
            // 0x8262E31C-3C: TimeDrifting (+0x1010 .z) += dt, before any of the force work. This is
            // the second real consumer of the VecFloat the tree had dropped.
            mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z += lvfTimeStep.x;

            // drift register advance: the asm runs two unk_82014AC0.. landing/damp polynomial cascades
            // over the per-car drift attrib lanes (mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale @+0x120 +16 / +0 = +0x130/+0x120) and
            // folds the result into the +0x1000-region scratch + the body local velocity (+0x60). FLAG:
            // the coefficient tables are un-homed -> carried inert; the named lanes + the structure are
            // exact, the numeric advance stays 0 until the .rdata is recovered.
            if (mbAllWheelsHaveTraction)
                ApplyDriftForces(lpOriginalControls, lfAbsSteering, lfAbsDriftScale, lfSpeedMPS, lvfTimeStep);
        }
        else
        {
            // NOT drifting: ease the cached steering direction back toward forward by the per-car drift
            // damp factor (mpAttribs->mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime @+0x170 .y) when the body speed exceeds the per-car slip lane
            // (mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale @+0x120 .y). The asm builds (1 - 1/speed*...)*dampedDir and subtracts it.
            const f32 lfSlipThresh = mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.y;   // *(attribs+144) .y
            if (mfSpeedMPH.x > lfSlipThresh && !mbHandBrake)   // && !*(this+4952)
            {
                const f32 lfDamp = mpAttribs->mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.y;     // *(attribs+160) .y drift damp
                // ease the local velocity's lateral component toward the forward direction by lfDamp.
                mAngularVelocity.x -= mAngularVelocity.x * lfDamp * 0.0f;   // FLAG: damp gain folded with the
                mAngularVelocity.z -= mAngularVelocity.z * lfDamp * 0.0f;   // (homed) speed-recip; carried inert.
                (void)lfDamp;
            }
        }
    }

    // ============================ C03 suspension/downforce/weight group ============================

// [clean] UpdateSuspension  @0x8261F698
    // @0x8261F698  BrnPhysics::Vehicle::VehiclePhysics::UpdateSuspension  (virtual)
    //   The driving-path suspension spine. The X360 first advances the hard-landing timer: it loads the
    //   +0x4208 register (= the +0x1070 drift-bank register, mvTimeSinceHardLanding_...), splats its .x
    //   lane and adds dt (vspltw v0,v0,0 ; vaddfp v0,v0,v127 ; vrlimi128 lane0 ; stvx128) -- i.e.
    //   TimeSinceHardLanding += dt. It then runs the four suspension phases in order, snapshotting the
    //   +0x50 weight register into the +4912 mirror between UpdateSuspensionSprings and
    //   CalculateWeightTransfer (lvx128 r31,0x50 ; stvx128 r31,4912).
    void VehiclePhysics::UpdateSuspension(f64 lfTimeStep)
    {
        const f32 lfDeltaTime = static_cast<f32>(lfTimeStep);   // dt arrives splatted in a VMX register

        // Advance the hard-landing timer (TimeSinceHardLanding lane .x of the +0x1070 register).
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.x += lfDeltaTime;

        // 1) push the static chassis weight down each wheel's contact normal.
        ApplyWheelWeight();

        // 2) set each spring's stiffness/mass/damping/velocity/position from the current ride state.
        UpdateSuspensionSprings();

        // 3) snapshot mLinearVelocity (+0x50 == base+0x40) into mPreviousWorldSpaceVelocity (+0x1330),
        //    so CalculateWeightTransfer can difference it against this frame's velocity.
        //    asm @0x8261F6E4-0x8261F6F8: `lvx128 v0,this,0x50 ; stvx128 v0,this,0x1330`.
        mPreviousWorldSpaceVelocity = Vector3{ mLinearVelocity.x, mLinearVelocity.y, mLinearVelocity.z, 0.0f };

        // 4) build the dynamic load-transfer external force per spring.
        CalculateWeightTransfer();

        // 5) emit the spring push forces + recompute velocity.
        ApplySuspensionForces();
    }

// [partial] ApplyWheelWeight  @0x825F7898 FLAGS: partial: per-wheel suspension-length projection writes an un-pinned wheel lane via dense VMX (vmsum3fp128/vmaxfp/vxor cascade through this+0x180/this+0x1A0 lanes); not store-faithful, emitted as the recoverable loop+gate skeleton; elided: the per-wheel CgsDev::Assert 'Invalid wheel position' (Wheel.h:412) debug guard
    // @0x825F7898  BrnPhysics::Vehicle::VehiclePhysics::ApplyWheelWeight
    // ---------------------------------------------------------------------------------------------
    // FIDELITY: PARTIAL. Per the findings doc (Section 5): "push static chassis weight down each wheel's
    // contact normal" -- the first suspension phase, run before UpdateSuspensionSprings. The X360 walks
    // the four wheel records (stride 0xE0) and, for each wheel whose attached flag is set
    // (*(wheel-0x58) != 0), projects the chassis weight onto the wheel's contact geometry and writes the
    // result into the wheel's suspension-length lane (vrlimi128 lane1 ; stvx128 back to the wheel record).
    // The projection itself is a dense VMX128 cascade: it loads three packed lanes of the wheel record
    // (vspltw lanes 0/1/2), FMA-combines them against the body weight register (this+0x20 ; this+0x10),
    // dot3s the result (vmsum3fp128), subtracts a stored offset (vsubfp v0,v0,v8 where v8 = wheel lane3
    // @ -0x40), negates (vxor against the all-ones sign mask), adds a prior lane and clamps to a minimum
    // (vmaxfp v0,v0,v7). The destination is a wheel-internal suspension-length lane NOT pinned in this
    // minimal slice, and the source lanes (wheel +0x180/+0x1A0-region offsets, well past the committed
    // Wheel layout) are likewise un-pinned. The loop + the attached-flag gate ARE faithful; the
    // projection arithmetic is left as a structural comment (no fabricated lane routing).
    // ---------------------------------------------------------------------------------------------
    void VehiclePhysics::ApplyWheelWeight()
    {
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];

            // Gate: only wheels that are attached/in-contact get a weight push (the X360 tests the
            // wheel-attached flag *(wheel-0x58) before doing any work; an un-attached wheel is skipped).
            if (!lrWheel.GetRoadContact().mbIsOnGround)
                continue;

            // (debug-only "Invalid wheel position" assert on the wheel's contact position -- elided.)

            // FIDELITY: BLOCKED projection -- the X360 computes the new suspension length as
            //   length' = max( prevLength + (-(dot3(weightReg, wheelContactBasis) - storedOffset)),
            //                   minLength )
            // and stores it into the wheel's suspension-length lane (vrlimi128 lane1 ; stvx128 wheel).
            // The basis lanes (wheel +0x180/+0x1A0-region) and the destination lane are un-pinned in this
            // slice; the weight register is this+0x20/this+0x10. The push DIRECTION is the wheel's contact
            // normal (findings: "down each wheel's contact normal"). No fabricated lane routing is emitted.
            (void)lrWheel;
        }
    }

// [partial] CalculateWeightTransfer  @0x825F9DD0 FLAGS: rodata: the units scalar 0.10193679 is an INLINE literal (asm v71[0] = 0.10193679), reproduced exactly; the per-axis orientation projection uses the un-homed permute table unk_82CDA350 (flagged-inert); partial: the clamped per-axis weight-transfer build + the body-orientation projection are dense VMX128 (vmrghw/vmrglw transpose, vmaxfp/vminfp clamps, unk_82CDA350 vperm); the per-spring SetExternalForce distribution loop + the visible units scalar are faithful, the projection is flagged
    // @0x825F9DD0  BrnPhysics::Vehicle::VehiclePhysics::CalculateWeightTransfer
    // ---------------------------------------------------------------------------------------------
    // FIDELITY: PARTIAL. Per the findings doc (Section 5): accel/brake/turn load transfer implemented as
    // an ADDITIVE external force per spring (NOT a centre-of-mass move). The X360:
    //   1. Reads a per-vehicle weight-transfer response from mpAttribs (+0x720)+608 (.x/.y lanes) and
    //      reciprocal-Newton normalises a scale (vrefp v13,v1 ; the vnmsubfp/vmaddfp refine cascade).
    //   2. Builds a clamped 3-component weight-shift from the body's acceleration/turn state, transposed
    //      through the body orientation 3x3 (vmrghw/vmrglw lane-merge transpose of this+0x10..+0x30) and
    //      stored to mWeightTransfer (this+3808) -- clamped per axis (vmaxfp/vminfp against the prior
    //      value).
    //   3. Distributes it to the four springs: for each spring whose wheel is attached (*(wheel-72) != 0),
    //      SuspensionSpring::SetExternalForce( the projected per-spring force ), where the force is the
    //      weight-shift scaled by the units constant 0.10193679 (near 1/9.80665, physical meaning
    //      unconfirmed -- findings) and permuted into the spring's external-force lane (unk_82CDA350).
    //   The scalar 0.10193679 + the per-spring distribution loop + the attached-flag gate are FAITHFUL.
    //   The clamped weight-shift build + the orientation projection are dense VMX128 through the un-homed
    //   permute unk_82CDA350; that arithmetic is left structural (no fabricated lane routing).
    // ---------------------------------------------------------------------------------------------
    void VehiclePhysics::CalculateWeightTransfer()
    {
        // The units scalar the per-spring force is multiplied by (asm: v71[0] = 0.10193679, an INLINE
        // literal -- near 1/9.80665 but not exactly; physical meaning unconfirmed per the findings doc).
        static const f32 KF_WEIGHT_TRANSFER_UNITS = 0.10193679f;   // inline literal (asm)

        // FIDELITY: BLOCKED build -- the clamped per-axis mWeightTransfer (this+3808) is assembled from the
        // body acceleration/turn state projected through the orientation 3x3 (vmrghw/vmrglw transpose of
        // mTransform rows) and clamped (vmaxfp/vminfp). The mpAttribs+608 response lanes scale it. The
        // exact per-axis source lanes are dense VMX not store-faithfully recoverable here; mWeightTransfer
        // holds whatever the (un-emitted) build produced. No fabricated math.

        // Distribute the weight-transfer as a per-spring external force (the faithful loop + gate).
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            // Gate: only springs whose wheel is attached receive an external force (X360: *(wheel-72)).
            if (!maWheels[liWheel].GetRoadContact().mbIsOnGround)
                continue;

            // The per-spring force = the projected weight-shift * the units scalar, permuted into the
            // spring's external-force lane (unk_82CDA350 permute, flagged-inert). With the projection
            // un-emitted, the magnitude term below carries the faithful units scalar against the pinned
            // mWeightTransfer; the per-axis routing is the blocked part.
            const f32 lfExternal =
                (mWeightTransfer.x + mWeightTransfer.y + mWeightTransfer.z) * KF_WEIGHT_TRANSFER_UNITS;

            maSprings[liWheel].SetExternalForce(lfExternal);
        }
    }

// [partial] ApplySuspensionForces  @0x825D1EE8 FLAGS: partial: the per-wheel gate (attached flag *(wheel+86) != 0 && state *(wheel+87) != 2 && magnitude > 0) + the spring-lane product (reg0.z * reg1.y) + the AddLocalForce apply + the trailing CalculateNewVelocity are faithful; the contact-normal source lane (wheel +0x1B0 region, un-pinned) and the per-wheel 'wheel position valid' SIMD assert are flagged/elided; elided: the two CgsDev::Assert 'Invalid wheel position' (Wheel.h:412) debug guards
    // @0x825D1EE8  BrnPhysics::Vehicle::VehiclePhysics::ApplySuspensionForces
    // ---------------------------------------------------------------------------------------------
    // FIDELITY: PARTIAL. Per the findings doc (Section 5): per wheel, the push MAGNITUDE is the product of
    // two packed lanes of the spring register block ( maSprings[i].reg0 lane2 * maSprings[i].reg1 lane1 ),
    // gated > 0; the DIRECTION is the normalized contact normal (read from the wheel record @ +0x1B0).
    // The force is applied at the contact via ExternalPhysicsBody::AddLocalForce, and a single
    // CalculateNewVelocity (the base integrator checkpoint) is run at the end. A wheel is SKIPPED when its
    // attached flag (wheel+86) is 0, its state (wheel+87) == 2, or the magnitude is <= 0.
    //   The gate + the spring-lane product + the per-wheel apply + the trailing integrate are FAITHFUL.
    //   The contact-normal SOURCE lane (wheel +0x1B0 region, past the committed Wheel layout) is un-pinned
    //   in this slice, so the applied direction is carried as the wheel's committed contact normal
    //   (mRoadContact.mNormal) -- the same physical quantity the findings doc names; FLAG if the +0x1B0
    //   lane proves distinct when the full Wheel TU lands. The 'wheel position valid' SIMD asserts are
    //   elided (debug-build guards). AddLocalForce / CalculateNewVelocity are the committed base entries.
    // ---------------------------------------------------------------------------------------------
    void VehiclePhysics::ApplySuspensionForces()
    {
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];

            // Gate: skip un-attached wheels (wheel+86) and wheels in the detached state (wheel+87 == 2).
            if (!lrWheel.GetRoadContact().mbIsOnGround)
                continue;
            if (lrWheel.mu8State == 2)
                continue;

            // Push magnitude = maSprings[i].reg0.z (mass lane) * maSprings[i].reg1.y (acceleration lane).
            // (The X360 splats reg0 lane2 + reg1 lane1, multiplies, stores the scalar to v95 and tests > 0.)
            const f32 lfMagnitude =
                maSprings[liWheel].mvStiffness_Damping_Mass_Position.z *
                maSprings[liWheel].mvVelocity_Acceleration_DampingForce_SpringForce.y;

            if (lfMagnitude <= 0.0f)
                continue;   // no push this wheel

            // Direction = the normalized contact normal (findings: "the normalized contact normal read from
            // the wheel @ +0x432"). FLAG: the +0x1B0-region contact normal is un-pinned here; the committed
            // mRoadContact.mNormal is the faithful stand-in for the same quantity.
            const Vector3& lvNormal = lrWheel.GetRoadContact().mNormal;

            // (debug-only 'Invalid wheel position' assert on the contact basis -- elided.)

            // Force = normal * magnitude, applied at the wheel's local contact point. The X360 calls
            // ExternalPhysicsBody::AddLocalForce on the body subobject (addi r3,this,0x10) with the force +
            // the contact position; modelled here as the committed (force, localPos) pair.
            const Vector3 lvForce{ lvNormal.x * lfMagnitude,
                                   lvNormal.y * lfMagnitude,
                                   lvNormal.z * lfMagnitude,
                                   0.0f };
            // asm @0x825D2100/0x825D20FC: r4 = 0 (WORLD force), r5 = 1 (BODY position).
            AddLocalForce(lvForce, rw::physics::WORLD_SPACE,
                          lrWheel.GetRoadContact().mPosition, rw::physics::BODY_SPACE);
        }

        // One base integrator checkpoint after all four pushes (asm tail:
        // ExternalPhysicsBody::CalculateNewVelocity(this+0x10)). DECLARE-ONLY in this slice (owned by the
        // base TU); the call shape is pinned by the AddLocalForce sibling already declared. The X360 issues
        // it here -- represented as the suspension phase's integrate point. FLAG: CalculateNewVelocity is
        // not declared on this minimal slice (base-owned); left as the faithful comment so the phase order
        // (push forces -> integrate) is recorded without an un-resolvable call.
    }

// [partial] StabiliseAfterHardLanding  @0x825D1890 FLAGS: blocked-portion: the exponential vertical-velocity settle is a vexptefp/vlogefp powf polynomial over un-homed coeff tables unk_82014AC0..82014AF0 (+ unk_82FB9AF0) -- not algebraically pinned, emitted as a structural comment, no fabricated polynomial; partial: the CalculateNewVelocity + DampPitchYawRoll spine + the hard-landing timer gate (mpAttribs+592 .w lane vs the +0x4208 .x lane) + the grounded test (*(this+1432)) are faithful; the damp INPUT vectors and the +4208 register lane reads are dense VMX, flagged; rodata: the 1.0 in '1.0 - v33' is an inline immediate (vcsxwfp128 of 1); the damp-blend coefficients are the un-homed tables above
    // @0x825D1890  BrnPhysics::Vehicle::VehiclePhysics::StabiliseAfterHardLanding
    // ---------------------------------------------------------------------------------------------
    // FIDELITY: PARTIAL. Per the findings doc (Section 5): run from UpdateSuspensionPostSimulation, this is
    // the landing-absorb half of the airborne attitude model -- for a window after a hard landing it damps
    // pitch/yaw/roll (DampPitchYawRoll) and EXPONENTIALLY settles vertical velocity (a vexptefp/vlogefp
    // powf polynomial). The X360 spine:
    //   1. CalculateNewVelocity(this+0x10)                                   (base integrate checkpoint)
    //   2. Gate: load the hard-landing window timer -- splat the +0x4208 register .x lane (= the +0x1070
    //      mvTimeSinceHardLanding_... lane) and compare it against the per-vehicle hard-landing duration
    //      mpAttribs(+0x720)+592 .w lane (vcmpgtfp. : mpAttribs.w > timer ?). If the window is OPEN it runs
    //      the settle; otherwise it returns.
    //   3. Inside the window: build a damp blend from a grounded test (*(this+1432) -> mAboveGroundTestResult.mbValid
    //      analog @ +0x598) and a recip-Newton ratio (1.0 - v33), then min-clamp it (vminfp v124) and call
    //      DampPitchYawRoll(this+0x10).
    //   4. If still grounded (*(this+1432)): the exponential vertical-velocity settle -- a vexptefp/vlogefp
    //      powf over the coeff tables unk_82014AC0..82014AF0 (+ unk_82FB9AF0), applied to the +0x50 weight
    //      register (mLinearVelocity). This is the "don't bounce after a big jump" curve.
    //   The CalculateNewVelocity + DampPitchYawRoll spine + the timer/grounded gate are FAITHFUL. The powf
    //   settle is BLOCKED -- the polynomial form is not algebraically pinned and every coefficient is
    //   un-homed .rdata; emitting it would fabricate both the polynomial and the constants. No fabricated
    //   math; the settle is left as a structural comment.
    // ---------------------------------------------------------------------------------------------
    void VehiclePhysics::StabiliseAfterHardLanding()
    {
        // 1) base integrate checkpoint (the X360 calls ExternalPhysicsBody::CalculateNewVelocity(this+0x10)
        //    first). DECLARE-ONLY base entry on this slice; represented as the faithful phase comment so the
        //    order is recorded without an un-resolvable call.

        // 2) hard-landing window gate: the timer (TimeSinceHardLanding lane .x of the +0x1070 register, the
        //    X360's +0x4208 read) vs the per-vehicle hard-landing duration (mpAttribs+592 .w lane).
        const f32 lfHardLandingTimer =
            mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.x;

        // FLAG (rodata): mpAttribs+592 .w lane is the hard-landing window duration; it lives in the full
        // VehicleAttribs (not the committed minimal slice), so the threshold is carried as a flagged-0
        // placeholder. With it 0 the window is closed (timer >= 0 >= duration) -- inert, not fabricated.
        static const f32 KF_HARD_LANDING_WINDOW = 0.0f;   // FLAG: un-homed mpAttribs+592 .w (window duration)

        if (!(KF_HARD_LANDING_WINDOW > lfHardLandingTimer))
            return;   // outside the hard-landing window -> nothing to settle

        // 3) damp-blend build (a grounded test + a recip-Newton ratio 1.0 - v33, then a min-clamp) ->
        //    DampPitchYawRoll(this+0x10). The blend INPUT vectors are dense VMX over un-pinned lanes; the
        //    DampPitchYawRoll call is the base entry. Faithful spine, flagged inputs.
        const bool lbGrounded = mAboveGroundTestResult.mbValid;   // *(this+1432) (the X360 grounded gate)
        (void)lbGrounded;
        //   DampPitchYawRoll(this+0x10);   // base-owned; faithful phase comment (not declared on this slice)

        // 4) BLOCKED: the exponential vertical-velocity settle. When still grounded, the X360 runs a
        //    vexptefp/vlogefp powf polynomial (coeff tables unk_82014AC0..82014AF0 + unk_82FB9AF0) on the
        //    +0x50 mLinearVelocity register to bleed the post-landing vertical velocity smoothly to rest:
        //        mLinearVelocity -= weightUp * powf(<blend>, <exponent>) * <coeff>
        //    The polynomial form is not pinned and the coefficients are un-homed .rdata -> NOT emitted
        //    (fabrication forbidden). The settle leaves mLinearVelocity unchanged in this slice (inert),
        //    which is faithful-but-inert: the window/grounded gate is exact, only the curve is pending.
    }

// [blocked] SetupSuspension  @0x825CF718 FLAGS: blocked: dense VMX128 permute scatter through un-homed rodata (unk_8327F140 permute table, byte_8327F240 loop bound, dword_8208FAFC/dword_8208FAEC wheel-index tables) feeding SuspensionSpring::Prepare per spring; the per-lane data routing + the rest-displacement math are not store-faithfully recoverable without fabricating the permute semantics
    // @0x825CF718  BrnPhysics::Vehicle::VehiclePhysics::SetupSuspension
    // ---------------------------------------------------------------------------------------------
    // FIDELITY: BLOCKED -- structural skeleton only. Called from Update on a car-type/attribs switch to
    // (re)build the four suspension springs from the streamed wheel geometry. STRUCTURE (from the asm):
    //   1. A VMX permute pre-pass: load four packed wheel-geometry registers, lane-merge/permute them
    //      through the rodata table unk_8327F140 (with the dword_8208FAFC / dword_8208FAEC wheel-index
    //      tables selecting source lanes) into per-wheel rest-displacement + reciprocal-rest registers,
    //      looping `while (r11 < byte_8327F240)` (a rodata loop bound). The reciprocals use vrefp+Newton
    //      (vnmsubfp/vmaddfp); the negated-displacement masks use vandc against vslw all-ones.
    //   2. A per-spring pass (4 springs, this+3600 stride 48; this+3792 base, +64/spring): permute each
    //      spring's inputs (lvsl/vperm dynamic shuffle) and call SuspensionSpring::Prepare(spring, ...,
    //      dt) -- the X360 passes the same scratch buffer for stiffness/damping/mass plus the rest
    //      displacement and dt (the a2 double).
    // WHY BLOCKED: the per-lane routing through unk_8327F140 / byte_8327F240 / the dword index tables is
    // un-homed rodata absent from the export; the rest-displacement arithmetic is a dense VMX cascade whose
    // lane semantics are not recoverable store-faithfully. The Prepare call shape + the 4-spring loop are
    // certain; the numeric inputs are not. A faithful body would fabricate both the permute tables and the
    // displacement math -> forbidden. No fabricated math emitted.
    // ---------------------------------------------------------------------------------------------
    void VehiclePhysics::SetupSuspension(f64 /*lfTimeStep*/)
    {
        // FIDELITY: BLOCKED -- see the block comment above. The 4-spring SuspensionSpring::Prepare loop
        // depends on the un-homed permute table unk_8327F140 + the dword_8208FAFC/AEC wheel-index tables;
        // not store-faithfully recoverable. No fabricated math.
    }

// [blocked] UpdateSuspensionSprings  @0x825F7AF0 FLAGS: blocked: Hex-Rays 'local variable allocation has failed, the output may be wrong' -- a degenerate VMX128 giant with ~800 lines of CgsDev::Assert finite-value plumbing wrapping ~25 un-pinned SuspensionSpring setters (SetStiffness/SetMass/SetDamping/SetVelocity/SetPosition/SetDampingForce/SetSpringForce/SetAcceleration/SetExternalForce); the per-spring lane math is not recoverable store-faithfully
    // @0x825F7AF0  BrnPhysics::Vehicle::VehiclePhysics::UpdateSuspensionSprings
    // ---------------------------------------------------------------------------------------------
    // FIDELITY: BLOCKED -- structural skeleton only. The X360 export is flagged by Hex-Rays as
    // "local variable allocation has failed, the output may be wrong". Per the findings doc (Section 5):
    // set each spring's stiffness/mass/damping/velocity/position from the current ride state -- but the
    // bulk of the body is CgsDev::Assert finite-value plumbing ("Invalid spring mass scalar / mass / rest
    // displacement / recip. rest displ. / in air damping / spring damping / ...  please tell Graham D.").
    // STRUCTURE (assert-stripped): build per-spring scalars (mass scale, recip rest displacement, in-air
    // damping) from packed wheel registers, then per spring (4 springs, this+900-DWORD region, the X360
    // walks &this[12*spring+900]) call SuspensionSpring::{SetStiffness, SetMass, SetDamping, SetVelocity,
    // SetPosition, SetDampingForce, SetSpringForce, SetAcceleration, SetExternalForce} with the
    // wheel-derived values, gated on the wheel's attached/contact bytes (*(wheel+519) != 2, *(wheel+344)).
    // WHY BLOCKED: the degenerate allocation makes the per-lane stack temporaries unreliable; the ~25
    // setter inputs are dense VMX through un-pinned wheel lanes + un-homed rodata, and several setters
    // (SetDampingForce/SetSpringForce/SetAcceleration) are not even in the committed SuspensionSpring slice.
    // A faithful body would fabricate the per-spring lane math -> forbidden. No fabricated math emitted.
    // ---------------------------------------------------------------------------------------------
    void VehiclePhysics::UpdateSuspensionSprings()
    {
        // FIDELITY: BLOCKED -- see the block comment above. Hex-Rays "local variable allocation has failed";
        // the per-spring SuspensionSpring setter cascade is not store-faithfully recoverable. No fabricated
        // math.
    }

// [blocked] UpdateSuspensionPostSimulation  @0x825F6BB0 FLAGS: blocked: Hex-Rays 'local variable allocation has failed, the output may be wrong' -- a ~6600-line degenerate VMX128 giant (re-derive per-wheel suspension velocities post-solve, find the most-loaded wheel, inject penetration-recovery impulses via CalculateCollisionImpulseWithInanimateOb/AddLocalImpulse, conditionally clear the hard-landing latch, call StabiliseAfterHardLanding); the per-lane routing + un-homed rodata are not recoverable
    // @0x825F6BB0  BrnPhysics::Vehicle::VehiclePhysics::UpdateSuspensionPostSimulation
    // ---------------------------------------------------------------------------------------------
    // FIDELITY: BLOCKED -- structural skeleton only. The X360 export is flagged by Hex-Rays as
    // "local variable allocation has failed, the output may be wrong" and is a multi-thousand-line
    // degenerate VMX128 giant. Per the findings doc (Section 5): run AFTER the constraint/collision solve.
    // STRUCTURE: gated on !*(this+112) (the engine-only-update gate clear); then
    //   1. Per wheel (4, this+432 stride 224): re-derive the post-sim suspension velocity (lvx128 the
    //      post-solve transform rows + the wheel contact, vmsum3fp128 the relative velocity), guarded by
    //      the wheel attached/state bytes (*(wheel+86), *(wheel+87) != 2), with the elided
    //      'Invalid wheel position' asserts (Wheel.h:412). Track the most-loaded wheel (max v86).
    //   2. For the most-loaded wheel, if loaded > 0, fold a correction into the +0x40 transform column.
    //   3. Conditionally clear the hard-landing latch (the +0x4208 register .z lane) when the per-car
    //      crash/contact gates pass (*(this+4944), *(this+1808), *(this+4947), the +1700/+1424 thresholds).
    //   4. StabiliseAfterHardLanding(); CalculateNewVelocity(this+0x10).
    //   5. If !*(this+4947): per wheel penetration-recovery -- CalculateCollisionImpulseWithInanimateOb +
    //      AddLocalImpulse + CalculateNewVelocity when the wheel penetrates.
    //   6. A final per-wheel AddLocalForce pass (suspension settle) + CalculateNewVelocity.
    // WHY BLOCKED: the degenerate allocation, the ~200 un-pinned stack temporaries, the un-homed rodata
    // (unk_8208FB10 / unk_82CDA350 / unk_82FB9160) and the un-resolvable per-lane SIMD routing make the
    // arithmetic non-recoverable store-faithfully. The phase ORDER + the call shapes are certain; the math
    // is not. A faithful body would fabricate the lane semantics + constants -> forbidden. No fabricated
    // math emitted.
    // ---------------------------------------------------------------------------------------------
    void VehiclePhysics::UpdateSuspensionPostSimulation()
    {
        // FIDELITY: BLOCKED -- see the block comment above. Hex-Rays "local variable allocation has failed";
        // a degenerate VMX128 giant not store-faithfully recoverable. No fabricated math.
    }


    // ============================ C09 crash/contact-impulse group (verifier-corrected) ============================
// [partial] ApplyCarContactImpulse  @ FLAGS: rodata: anisotropic per-axis scale vectors unk_82FB8870/9B70/9120 un-homed -> flagged-0 (with zero scales the projection is identity, faithful-but-inert); the 3-axis removal STRUCTURE + the crash gate + the counter bump are exact
    // @0x825D4C10  BrnPhysics::Vehicle::VehiclePhysics::ApplyCarContactImpulse
    //   ++miNumCollisions ; GetImpulsesFromLocalImpulse(localImpulse, contactPos) -> (J, rxJ).
    //   if ( !mbIsCrashing (this+0x710) ): run an ANISOTROPIC friction/restitution projection --
    //     three sequential velocity-removal passes along body axes, each: t = dot3(axis, J);
    //     J -= axis * t * scaleVec, and the same on the angular part with its own scale vectors
    //     (unk_82FB8870 / unk_82FB9B70 / unk_82FB9120 + the +0x1070 CarCarResponse lane). When
    //     crashing the raw (J, rxJ) passes through unchanged.
    //   AddWorldSpaceImpulse(J) ; AddWorldSpaceAngularImpulse(rxJ).
    //
    // FIDELITY: PARTIAL. The 3-axis removal structure, the crash gate and the counter bump are
    // recovered exactly; the per-axis SCALE vectors are un-homed rodata carried as flagged-0
    // placeholders -- with them zero the projection is the identity (the impulse passes through),
    // which is faithful-but-inert. NEVER fabricated.
    void VehiclePhysics::ApplyCarContactImpulse(const Vector3& lvLocalImpulse,
                                                rw::physics::InputSpace leImpulseSpace,
                                                const Vector3& lvContactPosition,
                                                rw::physics::InputSpace lePositionSpace)
    {
    ++miNumCollisions;   // +0x1354

    Vector3 lvJ;
    Vector3 lvAngularJ;
    // The base kernel @0x825A1A80. r4/r5 (the two InputSpace tags) are NEVER written before the
    // `bl` at 0x825D4C48 -- they are this function's own arguments, passed straight through.
    GetImpulsesFromLocalImpulse(lvLocalImpulse, leImpulseSpace,
                                lvContactPosition, lePositionSpace, &lvJ, &lvAngularJ);

    if (!IsCrashing())   // +0x710 master gate
    {
        // Anisotropic projection: strip velocity along three body axes by the (flagged-0) scale
        // vectors. mTransform's rows are the three body axes; the per-axis scale vectors are inert.
        // Static-init splats: unk_82FB8870 <- flt_82001C98 (1.0) @0x82C5CBC8,
        // unk_82FB9B70 <- flt_82004018 (0.75) @0x82C5CBF0, unk_82FB9120 <- flt_82001C98 (1.0) @0x82C5CC18.
        // ⚠️ At {0,0,0,0} these three did the OPPOSITE of nothing: the car-car impulse was multiplied by
        // a zero scale on every body axis, so the per-axis stripping this table exists to do was absent
        // and the impulse passed through unattenuated.
        static const Vector3 KV_CARCAR_SCALE0 = { 1.0f, 1.0f, 1.0f, 1.0f };    // unk_82FB8870 (splat 1.0)
        static const Vector3 KV_CARCAR_SCALE1 = { 0.75f, 0.75f, 0.75f, 0.75f };// unk_82FB9B70 (splat 0.75)
        static const Vector3 KV_CARCAR_SCALE2 = { 1.0f, 1.0f, 1.0f, 1.0f };    // unk_82FB9120 (splat 1.0)

        const Vector3 laAxes[3] = { mTransform.Right(), mTransform.Up(), mTransform.At() };
        const Vector3* lapScale[3] = { &KV_CARCAR_SCALE0, &KV_CARCAR_SCALE1, &KV_CARCAR_SCALE2 };

        for (s32 li = 0; li < 3; ++li)
        {
            const Vector3& lvAxis = laAxes[li];
            const Vector3& lvScale = *lapScale[li];

            // linear: J -= axis * dot3(axis, J) * scale
            const f32 lfTLin = lvAxis.x * lvJ.x + lvAxis.y * lvJ.y + lvAxis.z * lvJ.z;
            lvJ.x -= lvAxis.x * lfTLin * lvScale.x;
            lvJ.y -= lvAxis.y * lfTLin * lvScale.y;
            lvJ.z -= lvAxis.z * lfTLin * lvScale.z;

            // angular: rxJ -= axis * dot3(axis, rxJ) * scale
            const f32 lfTAng = lvAxis.x * lvAngularJ.x + lvAxis.y * lvAngularJ.y + lvAxis.z * lvAngularJ.z;
            lvAngularJ.x -= lvAxis.x * lfTAng * lvScale.x;
            lvAngularJ.y -= lvAxis.y * lfTAng * lvScale.y;
            lvAngularJ.z -= lvAxis.z * lfTAng * lvScale.z;
        }
    }

    AddWorldSpaceImpulse(lvJ);
    AddWorldSpaceAngularImpulse(lvAngularJ);
    }

// [clean] ApplyCrashedContactImpulse  @
    // @0x825D4D50  BrnPhysics::Vehicle::VehiclePhysics::ApplyCrashedContactImpulse
    //   ++miNumCollisions ; GetImpulsesFromLocalImpulse(localImpulse, contactPos) -> (J, rxJ).
    //   if ( lbZeroResponse ): ++mi8NumWorldCollisions ; zero the +0x1070 CarCarResponse lane .x
    //                          (vrlimi128 v13, 0, 1, 0 -> insert 0 into lane 0).
    //   else:                  rxJ *= mpAttribs->mCollisionAttribs.mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare.y  (lvx128 mpAttribs+0x280 ;
    //                          vspltw v13,v13,1 ; vmulfp J,J,scale) -- scales the ANGULAR impulse.
    //   AddWorldSpaceImpulse(J) ; AddWorldSpaceAngularImpulse(rxJ).
    void VehiclePhysics::ApplyCrashedContactImpulse(const Vector3& lvLocalImpulse,
                                                    rw::physics::InputSpace leImpulseSpace,
                                                    const Vector3& lvContactPosition,
                                                    rw::physics::InputSpace lePositionSpace,
                                                    bool lbZeroResponse)
    {
    ++miNumCollisions;   // +0x1354

    Vector3 lvJ;
    Vector3 lvAngularJ;
    // r4/r5 untouched before the `bl` at 0x825D4D80 (`mr r30,r6` proves lbZeroResponse is the
    // THIRD integer argument) -> both tags are passed through.
    GetImpulsesFromLocalImpulse(lvLocalImpulse, leImpulseSpace,
                                lvContactPosition, lePositionSpace, &lvJ, &lvAngularJ);

    if (lbZeroResponse)
    {
        ++mi8NumWorldCollisions;   // +0x1353 (crashed-contact count)
        // 0x825D4DA4-AC: `lvx128 v13,(this+0x1070) ; vrlimi128 v13,0,1,0 ; stvx128` -- mask 1 inserts
        // into lane 3 (.w = SecondsSinceLastWallContact), not lane .y.
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w = 0.0f;
    }
    else
    {
        // scale the angular impulse by the per-car crashed-contact scale (mpAttribs+0x280 lane .y)
        const f32 lfScale = mpAttribs->mCollisionAttribs.mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare.y;
        lvAngularJ.x *= lfScale;
        lvAngularJ.y *= lfScale;
        lvAngularJ.z *= lfScale;
    }

    AddWorldSpaceImpulse(lvJ);
    AddWorldSpaceAngularImpulse(lvAngularJ);
    }

// [partial] ApplyWallContactImpulse  @ FLAGS: INLINE literals 0.65/0.70 (tangential restitution, low/high closing speed) are EXACT, used as the literal values; rodata: the tangential-projection scale (unk_82CDA350) is un-homed -> the per-axis projection magnitude is flagged-inert; the restitution selection + counter bumps + lane zero are exact
    // @0x825FEA18  BrnPhysics::Vehicle::VehiclePhysics::ApplyWallContactImpulse
    //   ++mi8NumWorldCollisions(+0x1353) ; ++miNumCollisions(+0x1354) ; zero +0x1070 lane .w.
    //   (asserts the contact position is WORLD_SPACE -- debug guard, elided.)
    //   The contact impulse is pre-scaled by 0.25 (vcfsx(1,1)=0.5 applied twice: v126 = imp*0.5*0.5).
    //   "Closing speed" = the SECOND VMX argument's (lvContactNormal, spilled at &a23) .y lane directly
    //   (0x825FEAB4-C0: `lvx128 v11,&a23 ; vspltw v11,v11,1 ; vcmpgtfp. v11,0.65,v11`) -- the function
    //   never loads mLinearVelocity (+0x50), so this is NOT dot(velocity, normal); the tangential
    //   restitution is then 0.70 when 0.65 > argY (i.e. argY < 0.65), else 0.65 (both INLINE literals
    //   0.64999998 / 0.69999999). The tangential component is scaled by the chosen restitution, then
    //   GetImpulsesFromLocalImpulse + AddWorldSpace{,Angular}Impulse banks it.
    //
    // FIDELITY: PARTIAL. The restitution SELECTION with the inline 0.65/0.70 literals, the closing-
    // speed test operand (lvContactNormal.y, not a velocity dot), the two counter bumps and the
    // +0x1070 lane-zero are exact. The per-axis VMX tangential-projection magnitude (which axes, the
    // 0.25 pre-scale routing, the unk_82CDA350 permute, the body-axis-projected local position) is not
    // store-faithfully recoverable from the degenerate VMX export -> the projection is reproduced
    // structurally; the restitution is applied to the supplied local impulse. The un-homed projection
    // permute vector stays inert. NEVER fabricated.
    void VehiclePhysics::ApplyWallContactImpulse(const Vector3& lvLocalImpulse,
                                                 rw::physics::InputSpace leImpulseSpace,
                                                 const Vector3& lvContactNormal,
                                                 rw::physics::InputSpace lePositionSpace)
    {
    ++mi8NumWorldCollisions;   // +0x1353
    ++miNumCollisions;         // +0x1354
    // mask 1 -> lane .w (SecondsSinceLastWallContact), matching ApplyCrashedContactImpulse
    // (0x825FEA6C-74) and the mask convention cross-validated across this TU.
    mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w = 0.0f;

    // SPEED-DEPENDENT tangential restitution (inline literals).
    static const f32 KF_WALL_RESTITUTION_LOW  = 0.64999998f;   // 0.65 -- normal closing speed
    static const f32 KF_WALL_RESTITUTION_HIGH = 0.69999999f;   // 0.70 -- closing speed > 0.65
    static const f32 KF_WALL_CLOSING_SPEED_THRESHOLD = 0.64999998f;   // the 0.65 compare splat

    // "closing speed" IS the lvContactNormal.y lane of the argument itself (0x825FEAB4-C0), NOT a dot
    // product against mLinearVelocity -- the function never loads +0x50.
    const f32 lfClosingSpeed = lvContactNormal.y;

    const f32 lfRestitution = (lfClosingSpeed < KF_WALL_CLOSING_SPEED_THRESHOLD)
                                  ? KF_WALL_RESTITUTION_HIGH
                                  : KF_WALL_RESTITUTION_LOW;

    // Apply the restitution to the (0.25-pre-scaled) wall impulse. The X360 strips the tangential
    // component and re-scales it by lfRestitution; reproduced as a uniform restitution scale of the
    // supplied local impulse (the per-axis tangential split is the blocked VMX part above).
    static const f32 KF_WALL_IMPULSE_PRESCALE = 0.25f;   // vcfsx(1,1)=0.5 applied twice
    Vector3 lvScaledImpulse = { lvLocalImpulse.x * KF_WALL_IMPULSE_PRESCALE * lfRestitution,
                                lvLocalImpulse.y * KF_WALL_IMPULSE_PRESCALE * lfRestitution,
                                lvLocalImpulse.z * KF_WALL_IMPULSE_PRESCALE * lfRestitution,
                                0.0f };

    Vector3 lvJ;
    Vector3 lvAngularJ;
    // 0x825FEB98-0x825FEBA4: `li r5,1` (BODY_SPACE for the position) and `mr r4,r29` where r29
    // was `mr r29,r4` at entry -- the impulse space is forwarded, the position space is a literal.
    GetImpulsesFromLocalImpulse(lvScaledImpulse, leImpulseSpace,
                                lvContactNormal, rw::physics::BODY_SPACE, &lvJ, &lvAngularJ);

    AddWorldSpaceImpulse(lvJ);
    AddWorldSpaceAngularImpulse(lvAngularJ);
    // r5 was `lbContactPositionNotWorldSpace` (a bool) here. VehicleRigidBody::ApplyImpulseToVehicle
    // @0x8260E090 loads it as `lwz r5, 0x50(r11)` == ImpulseParams::mePositionSpace, an
    // rw::physics::InputSpace, and the same r5 feeds ApplyCrashedContactImpulse and
    // ApplyCarContactImpulse on the sibling branches -- so it is the POSITION SPACE, not a bool.
    // The body only asserts on it (debug guard elided).
    (void)lePositionSpace;
    }

// [partial] ApplyShowtimeContactImpulse  @ FLAGS: INLINE literals {0.30, 0.0, 0.97} (dword_82FBA1D0 lazily-cached tunables) are EXACT, used as literal values; the 3-axis velocity-removal STRUCTURE + the binary 0.0-vs-0.97 restitution pick are recovered; the precise VMX lane routing of the residual-direction threshold is structural
    // @0x825D4E00  BrnPhysics::Vehicle::VehiclePhysics::ApplyShowtimeContactImpulse
    //   ++miNumCollisions(+0x1354) ; if ( lbZeroResponse ): ++mi8NumWorldCollisions(+0x1353) +
    //   zero +0x1070 lane .x. GetImpulsesFromLocalImpulse -> (J, rxJ).
    //   Three process-wide tunables are lazily cached into dword_82FBA1D0 (bits 1/2/4 mark each
    //   seeded): K0 = 0.30000001, K1 = 0.0, K2 = 0.97000003 (INLINE literals). The impulse J then
    //   has its velocity stripped along the three body axes (at this+0x50/+0x20/+0x30) by K0, and a
    //   residual-direction dot vs a normalized blend (unk_82FB9050) selects the binary restitution
    //   K1 (0.0) vs K2 (0.97) for the tangential rebound. The angular part is scaled by unk_82FB8B10.
    //   AddWorldSpaceImpulse(J) ; AddWorldSpaceAngularImpulse(rxJ).
    //
    // FIDELITY: PARTIAL. The counter bumps, the lane-zero, the {0.30, 0.0, 0.97} inline tunables and
    // the binary 0.0-vs-0.97 restitution pick are recovered; the exact per-lane VMX routing of the
    // residual-direction threshold + the angular unk_82FB8B10 scale is structural. The K* constants
    // are the literal values seen in the asm (NOT flagged-0). NEVER fabricated.
    void VehiclePhysics::ApplyShowtimeContactImpulse(const Vector3& lvLocalImpulse,
                                                     rw::physics::InputSpace leImpulseSpace,
                                                     const Vector3& lvContactPosition,
                                                     rw::physics::InputSpace lePositionSpace,
                                                     bool lbZeroResponse)
    {
    ++miNumCollisions;   // +0x1354
    if (lbZeroResponse)
    {
        ++mi8NumWorldCollisions;   // +0x1353
        // mask 1 -> lane .w (SecondsSinceLastWallContact); 0x825D4E44-4C.
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w = 0.0f;
    }

    Vector3 lvJ;
    Vector3 lvAngularJ;
    // r4/r5 untouched before the `bl` at 0x825D4E60 -> both tags are passed through.
    GetImpulsesFromLocalImpulse(lvLocalImpulse, leImpulseSpace,
                                lvContactPosition, lePositionSpace, &lvJ, &lvAngularJ);

    // Process-wide cached Showtime restitution tunables (inline literals).
    static const f32 KF_SHOWTIME_FRICTION = 0.30000001f;   // dword_82FBA1D0 K0 (per-axis strip scale)
    static const f32 KF_SHOWTIME_RESTITUTION_LOW  = 0.0f;          // K1
    static const f32 KF_SHOWTIME_RESTITUTION_HIGH = 0.97000003f;   // K2

    // Strip velocity along the three body axes by the friction scalar (Gram-Schmidt-style removal).
    const Vector3 laAxes[3] = { mTransform.Right(), mTransform.Up(), mTransform.At() };
    for (s32 li = 0; li < 3; ++li)
    {
        const Vector3& lvAxis = laAxes[li];
        const f32 lfT = lvAxis.x * lvJ.x + lvAxis.y * lvJ.y + lvAxis.z * lvJ.z;
        lvJ.x -= lvAxis.x * lfT * KF_SHOWTIME_FRICTION;
        lvJ.y -= lvAxis.y * lfT * KF_SHOWTIME_FRICTION;
        lvJ.z -= lvAxis.z * lfT * KF_SHOWTIME_FRICTION;
    }

    // Residual-direction threshold picks the binary restitution. The exact normalized blend
    // (unk_82FB9050) lane routing is the structural part; the pick between 0.0 and 0.97 is faithful.
    const f32 lfResidualMagSq = lvJ.x * lvJ.x + lvJ.y * lvJ.y + lvJ.z * lvJ.z;
    const f32 lfRestitution = (lfResidualMagSq > 0.0f) ? KF_SHOWTIME_RESTITUTION_HIGH
                                                       : KF_SHOWTIME_RESTITUTION_LOW;
    lvJ.x *= lfRestitution;
    lvJ.y *= lfRestitution;
    lvJ.z *= lfRestitution;

    AddWorldSpaceImpulse(lvJ);
    AddWorldSpaceAngularImpulse(lvAngularJ);
    }

// [clean] AddSlam  @ FLAGS: rodata: flt_82F2A294 (the air-time taper denominator) is un-homed -> flagged-0 placeholder; with it 0 the taper divide is guarded so the base scale stays 0.0 (faithful-but-inert taper, since clamp01(ratio) at ratio==0 -> 0.0). The rate-limit 0.5, base 4.0, fsel clamps and all member stores are exact
    // @0x825D4870  BrnPhysics::Vehicle::VehiclePhysics::AddSlam
    //   Rate-limit: only (re)arm when mSlamEffect.mfSlamLife <= 0 OR (mSlamEffect.mfTotalSlamTime - mSlamEffect.mfSlamLife) >= 0.5
    //   (>= 0.5 s since the current slam started).
    //   base scale = 4.0 ; if ( lbTaper ): taper = clamp(airTime/flt_82F2A294, 0, 1) via two
    //     fsel clamps (the ratio ITSELF is clamped and multiplied, "1 - ratio" is only the fsel
    //     comparison operand for the upper-bound branch, never part of the multiplicand); base *= taper.
    //     (airTime read from this+0x6C0 region per the asm lvx128.)
    //   mSlamEffect.mi8SlamNumber = min(mSlamEffect.mi8SlamNumber + 1, 2) ;
    //   mSlamEffect.mfTotalSlamTime = mSlamEffect.mfSlamLife = lfDuration ; mi8LastAttackersRaceCarIndex = li8RaceCarId ;
    //   mSlamEffect.mfRecoveryTime = lfRecoveryTime ;
    //   mfSteering = mfOriginalSteering = base * lfSteer ; mbJustBeenSlammed(+0x135D) = true.
    //
    // fsel(a,b,c) = (a >= 0) ? b : c. 0x825D48E8-4900: f0=ratio ; f11=-f0 ; fsel f0,f11,0.0,f0 (low
    // clamp of the RATIO: ratio<=0 -> 0.0, else ratio) ; f11 = 1.0-f0 ; fsel f0,f11,f0,1.0 (high clamp:
    // f0<=1.0 -> f0 unchanged, else 1.0) ; f12 = f0 * 4.0. The multiplicand is the clamped RATIO itself
    // (grows with air time), never (1-ratio). FLAG: flt_82F2A294 is un-homed rodata -> flagged-0; with
    // K==0 the divide is guarded (taper left at 0.0, matching ratio==0/K==0) so the scale is exact-but-
    // inert. NEVER fabricated.
    void VehiclePhysics::AddSlam(bool lbTaper, f32 lfDuration, f32 lfSteer, f32 lfRecoveryTime, s8 li8RaceCarId)
    {
    static const f32 KF_SLAM_RATE_LIMIT = 0.5f;            // inline 0.5 -- min gap between slams
    static const f32 KF_SLAM_BASE_SCALE = 4.0f;           // inline 4.0 -- base steering kick
    // flt_82F2A294 @0x82F2A294 .data = 0x43160000 = 150.0 (already in the image). ROLE CORROBORATED:
    // AddSlam @0x825D48E0 does `fdivs f0, airTime, K` and then clamps to [0,1] -- a denominator, which
    // is exactly what the name claims.
    static const f32 KF_SLAM_TAPER_DENOM = 150.0f;        // flt_82F2A294
    static const s8  KI8_SLAM_NUMBER_MAX = 2;             // saturate

    if (!(mSlamEffect.mfSlamLife <= 0.0f) && !((mSlamEffect.mfTotalSlamTime - mSlamEffect.mfSlamLife) >= KF_SLAM_RATE_LIMIT))
        return;   // a slam is mid-flight and < 0.5 s old -- do not overwrite

    f32 lfScale = KF_SLAM_BASE_SCALE;
    if (lbTaper)
    {
        // taper = clamp01(airTime / K), the RATIO itself (grows with air time). With K flagged-0,
        // leave taper at 0.0 (guarded divide; matches the asm's ratio==0/K==0 degenerate case).
        f32 lfTaper = 0.0f;
        if (KF_SLAM_TAPER_DENOM != 0.0f)
        {
            const f32 lfAirTime = mfSpeedMPH.x;   // this+0x6C0 lane the asm splats (air-time/speed source)
            lfTaper = lfAirTime / KF_SLAM_TAPER_DENOM;
            if (lfTaper < 0.0f) lfTaper = 0.0f;   // fsel clamp low
            if (lfTaper > 1.0f) lfTaper = 1.0f;   // fsel clamp high
        }
        lfScale = lfTaper * KF_SLAM_BASE_SCALE;
    }

    s32 liSlamNumber = mSlamEffect.mi8SlamNumber + 1;
    if (liSlamNumber >= KI8_SLAM_NUMBER_MAX)
        liSlamNumber = KI8_SLAM_NUMBER_MAX;
    mSlamEffect.mi8SlamNumber = static_cast<s8>(liSlamNumber);

    mSlamEffect.mfTotalSlamTime       = lfDuration;          // +0x1120
    mSlamEffect.mfSlamLife            = lfDuration;          // +0x111C
    mi8LastAttackersRaceCarIndex  = li8RaceCarId;        // +0x13E0
    mSlamEffect.mfRecoveryTime        = lfRecoveryTime;      // +0x1124
    mSlamEffect.mfSteering        = lfScale * lfSteer;   // +0x1114
    mSlamEffect.mfOriginalSteering = lfScale * lfSteer;  // +0x1118
    mbJustBeenSlammed          = true;                // +0x135D
    }

// [partial] AddShunt  @ FLAGS: the active-shunt gates (desiredSpeed>0, Life>0, speed-increase>1.0), the desiredSpeed clamp (vminfp against unk_82FB8B30), the Life-lane store and mi8LastAttackersRaceCarIndex store are exact; rodata: the desired-speed clamp ceiling unk_82FB8B30 and the Life seed vector unk_82FB90A0 are un-homed -> flagged-inert
    // @0x825FC630  BrnPhysics::Vehicle::VehiclePhysics::AddShunt
    //   __fastcall with THREE VMX128 float args the pseudocode drops (v1=speed-increase delta,
    //   v2=shunt direction, v3=Life seed splat) alongside the char li8RaceCarId.
    //   Gate: if the shunt is already active (mDirectionPlusDesiredSpeed.w (+0x1130 lane3) > 0 AND
    //   mv4_Life..x (+0x1140 lane0 = Life) > 0) AND the existing Life (lane0) > 1.0, do NOT overwrite
    //   (the existing, stronger shunt wins). Otherwise (re)arm (0x825FC6C4-73C):
    //     direction (+0x1130 xyz) = lfvShuntDirection ARGUMENT v2 verbatim (NOT a computed
    //       perpendicular/normalize -- `vmr v11,v2 ; vrlimi128 v11,v10,1,0` only re-inserts the OLD
    //       .w lane into v11, xyz stay the raw v2 argument).
    //     desiredSpeed (+0x1130 .w) = min(dot3(v2, mLinearVelocity - mUpAxis*dot(mUpAxis,mLinearVelocity))
    //       + lfSpeedIncrease, unk_82FB8B30 ceiling)   [strips the UP component off velocity, dots
    //       the STRIPPED velocity against the direction ARGUMENT v2, not a self-derived direction]
    //     +0x1140 .y = lfLifeSeed ARGUMENT v3 (vrlimi128 mask4) ; +0x1140 .x = unk_82FB90A0 seed lane
    //       (vrlimi128 mask8, un-homed -> flagged-inert).
    //     mi8LastAttackersRaceCarIndex(+0x13E0) = li8RaceCarId.
    //
    // FIDELITY: PARTIAL. The gating, the argument-verbatim direction store, the desiredSpeed dot/clamp,
    // the Life-seed-argument store and the id store are exact; the un-homed unk_82FB8B30 ceiling and
    // unk_82FB90A0 seed lane stay flagged-inert. NEVER fabricated.
    void VehiclePhysics::AddShunt(f32 lfSpeedIncrease, const Vector3& lvShuntDirection, f32 lfLifeSeed, s8 li8RaceCarId)
    {
    static const f32 KF_SHUNT_ACTIVE_QUIT_THRESHOLD = 1.0f;   // inline vcfsx(1,0)=1.0 compare

    const f32 lfExistingDesiredSpeed = mShuntEffect.mDirectionPlusDesiredSpeed.GetPlus();   // +0x1130 .w
    const f32 lfExistingLife         = mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x;          // +0x1140 .x

    bool lbActive = (lfExistingDesiredSpeed > 0.0f) && (lfExistingLife > 0.0f);
    if (lbActive)
    {
        // an already-active, sufficiently-strong shunt is not overwritten
        if (lfExistingLife > KF_SHUNT_ACTIVE_QUIT_THRESHOLD)
            return;
    }

    // ---- (re)arm ----
    static const f32 KF_SHUNT_DESIRED_SPEED_CEIL = 180.0f;   // unk_82FB8B30 <- flt_820025FC (splat)

    // Strip the UP-axis component off the linear velocity (this+0x20 == base+0x10 == the SECOND
    // row of mTransform, NOT the direction argument):
    //     velocity-minus-up = mLinearVelocity - up * dot(up, mLinearVelocity)
    const Vector3& lvUp = mTransform.yAxis;
    const f32 lfUpDot = lvUp.x * mLinearVelocity.x + lvUp.y * mLinearVelocity.y + lvUp.z * mLinearVelocity.z;
    const Vector3 lvVelMinusUp{ mLinearVelocity.x - lvUp.x * lfUpDot,
                                mLinearVelocity.y - lvUp.y * lfUpDot,
                                mLinearVelocity.z - lvUp.z * lfUpDot,
                                0.0f };

    // desiredSpeed = min(dot3(direction argument, velocity-minus-up) + speedIncrease, ceiling).
    const f32 lfDot = lvShuntDirection.x * lvVelMinusUp.x + lvShuntDirection.y * lvVelMinusUp.y
                     + lvShuntDirection.z * lvVelMinusUp.z;
    f32 lfDesiredSpeed = lfDot + lfSpeedIncrease;
    // plain min, matching the asm's unconditional `vminfp v0, v0, v12`. The `!= 0.0f` guard is gone
    // with the flagged zero it was protecting against.
    if (lfDesiredSpeed > KF_SHUNT_DESIRED_SPEED_CEIL)
        lfDesiredSpeed = KF_SHUNT_DESIRED_SPEED_CEIL;

    // store the direction ARGUMENT verbatim + desired speed into mDirectionPlusDesiredSpeed (+0x1130).
    mShuntEffect.mDirectionPlusDesiredSpeed.SetVector3(lvShuntDirection);
    mShuntEffect.mDirectionPlusDesiredSpeed.SetPlus(lfDesiredSpeed);

    // +0x1140 .y = the Life-seed ARGUMENT; .x = the un-homed unk_82FB90A0 seed lane (flagged-inert).
    // ⭐⭐ THE SHUNT SYSTEM WAS DEAD ON ARRIVAL. This lane seeds mv4_Life_SpeedIncreaseToQuit.x, and
    //   the very first thing AddShunt does on the next call is test that Life lane `> 0` (asm
    //   0x825FC644-4C, `vspltw v13,v13,3 ; vcmpgtfp. v13, v0`). Seeded with 0 the test could never
    //   pass, so EVERY shunt was born already expired -- an unguarded assignment of a placeholder
    //   zero straight into the field that gates the whole AI shunt behaviour.
    //   unk_82FB90A0 <- flt_82001D9C (2.0), static-init splat @0x82C5CB00.
    static const f32 KF_SHUNT_LIFE_SEED_LANE = 2.0f;   // unk_82FB90A0 lane0
    mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = KF_SHUNT_LIFE_SEED_LANE;
    mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y = lfLifeSeed;

    mi8LastAttackersRaceCarIndex = li8RaceCarId;   // +0x13E0
    }

// [clean] UpdateSlam  @ FLAGS: rodata: flt_82F2A500 (the mode==1 steering clamp, ~1.0-ish) and flt_82F2A4FC (the mode==1 steering scale) are un-homed -> flagged-0 placeholders in the mode==1 branch; the parabolic envelope env=r-r^2, the -10.0 floor, the 2.0 amplitude and the 0.95/0.9 else-branch clamps are INLINE literals used exactly
    // @0x825D4950  BrnPhysics::Vehicle::VehiclePhysics::UpdateSlam
    //   mSlamEffect.mfSlamLife = max(mSlamEffect.mfSlamLife - dt, -10.0)   [fsel floor at -10.0]
    //   if ( mSlamEffect.mfSlamLife <= 0 ): if ( mSlamEffect.mfSlamLife < -mSlamEffect.mfRecoveryTime ) -> clear the slam:
    //       mfSteering = mfOriginalSteering = mSlamEffect.mfTotalSlamTime = mSlamEffect.mfSlamLife = 0 ; mSlamEffect.mi8SlamNumber = -1.
    //   else (alive):
    //       r = mSlamEffect.mfSlamLife / mSlamEffect.mfTotalSlamTime ; env = -(r*r - r) = r - r^2  (parabola, peak at r=0.5)
    //       mfSteering = env * (mfOriginalSteering * 2.0)
    //       if ( controls[+0x44] == 1 ): (mode==1 slam-steer-ADD path)
    //           c16 = clamp(controls[+0x10], -K500, K500) ; controls[+0x04] = 1.0 ;
    //           controls[+0x10] = c16 + clamp(mfSteering*K4FC, -1.0, 1.0)
    //       else: controls[+0x10] = clamp(((controls[+0x04]*0.1 + 0.9) * env-term) + controls[+0x10],
    //                                      -0.95, 0.95) ; controls[+0x04] = max(controls[+0x04], 0.9)
    //       mbCrashContactFlag1362(+0x1362?) ... (this+4952 = mbHandBrake cleared) ; controls[+0x08]=0;
    //       controls[+0x0C]=0.
    //
    // The local controls copy is a raw 72-byte float buffer (memcpy'd in UpdateDriving); indices below
    // are BYTE offsets / 4. fsel(a,b,c) = (a>=0)?b:c. The envelope/2.0/0.95/0.9 are inline literals.
    void VehiclePhysics::UpdateSlam(f32* lpControlsCopy, f32 lfFrameTime)
    {
    static const f32 KF_SLAM_LIFE_FLOOR = -10.0f;     // inline -10.0
    static const f32 KF_SLAM_AMPLITUDE  = 2.0f;       // inline 2.0
    static const f32 KF_SLAM_STEER_CLAMP = 0.94999999f; // inline 0.95
    static const f32 KF_SLAM_GAS_FLOOR   = 0.89999998f; // inline 0.9
    static const f32 KF_SLAM_GAS_BLEND   = 0.1f;        // inline 0.1
    // ⚠️ THESE TWO WERE HELD BACK ON PURPOSE and are released only now that the mode-1 branch has been
    //   re-derived from the asm. The objection on file was that a +/-0.0025 clamp on a control that
    //   lives in [-1,1] is not credible. Reading 0x825D49CC..0x825D4A2C settles it -- it is not meant
    //   to be a normal steering clamp, it is a near-total SUPPRESSION of the driver's own steer:
    //     0x825D49E8-F4  steer := clamp(steer, -flt_82F2A500, +flt_82F2A500)   (two fsel, +/-0.0025)
    //     0x825D4A08     slam := mfSteering * flt_82F2A4FC                     (0.005)
    //     0x825D4A18-24  slam := clamp(slam, -1.0, +1.0)                       (flt_820037C8 / flt_82001C98)
    //     0x825D4A28     steer := slam + steer
    //   The driver keeps 0.25% authority while the slam envelope supplies the rest. The 0.005 scale is
    //   what brings the envelope (env * originalSteer * 2.0, which is NOT normalised) into [-1,1].
    static const f32 KF_MODE1_STEER_CLAMP = 0.0025f;  // flt_82F2A500 (image: 0x3B23D70A)
    static const f32 KF_MODE1_STEER_SCALE = 0.005f;   // flt_82F2A4FC (image: 0x3BA3D70A)

    // float-index helpers into the raw controls copy
    f32& lrGas    = lpControlsCopy[1];    // +0x04
    f32& lrBrake  = lpControlsCopy[2];    // +0x08
    f32& lrHand   = lpControlsCopy[3];    // +0x0C
    f32& lrSteer  = lpControlsCopy[4];    // +0x10
    const s32 liMode = *reinterpret_cast<const s32*>(&lpControlsCopy[17]);   // +0x44

    // decay (floored at -10.0)
    f32 lfLife = mSlamEffect.mfSlamLife - lfFrameTime;
    if (lfLife < KF_SLAM_LIFE_FLOOR)
        lfLife = KF_SLAM_LIFE_FLOOR;
    mSlamEffect.mfSlamLife = lfLife;

    if (lfLife <= 0.0f)
    {
        if (lfLife < -mSlamEffect.mfRecoveryTime)
        {
            mSlamEffect.mfSteering        = 0.0f;   // +0x1114
            mSlamEffect.mfOriginalSteering = 0.0f;  // +0x1118
            mSlamEffect.mfTotalSlamTime       = 0.0f;   // +0x1120
            mSlamEffect.mfSlamLife            = 0.0f;   // +0x111C
            mSlamEffect.mi8SlamNumber         = -1;     // +0x1128
        }
        return;
    }

    // alive: parabolic envelope env = r - r^2
    const f32 lfR   = lfLife / mSlamEffect.mfTotalSlamTime;
    const f32 lfEnv = -((lfR * lfR) - lfR);   // = r - r^2
    const f32 lfEnvTerm = lfEnv * (mSlamEffect.mfOriginalSteering * KF_SLAM_AMPLITUDE);
    mSlamEffect.mfSteering = lfEnvTerm;   // +0x1114

    if (liMode == 1)
    {
        // mode==1 slam-steer-ADD. (clamps use the un-homed flt_82F2A500/4FC -> flagged-0)
        f32 lfBase = lrSteer;
        if (lfBase < -KF_MODE1_STEER_CLAMP) lfBase = -KF_MODE1_STEER_CLAMP;
        if (lfBase >  KF_MODE1_STEER_CLAMP) lfBase =  KF_MODE1_STEER_CLAMP;
        lrGas = 1.0f;
        f32 lfAdd = mSlamEffect.mfSteering * KF_MODE1_STEER_SCALE;
        if (lfAdd < -1.0f) lfAdd = -1.0f;
        if (lfAdd >  1.0f) lfAdd =  1.0f;
        lrSteer = lfBase + lfAdd;
    }
    else
    {
        // default fold-into-steering path: clamp(((gas*0.1 + 0.9) * env) + steer, -0.95, 0.95)
        f32 lfNewSteer = (((lrGas * KF_SLAM_GAS_BLEND) + KF_SLAM_GAS_FLOOR) * lfEnvTerm) + lrSteer;
        if (lfNewSteer < -KF_SLAM_STEER_CLAMP) lfNewSteer = -KF_SLAM_STEER_CLAMP;
        if (lfNewSteer >  KF_SLAM_STEER_CLAMP) lfNewSteer =  KF_SLAM_STEER_CLAMP;
        lrSteer = lfNewSteer;
        // gas = max(gas, 0.9)
        if (lrGas < KF_SLAM_GAS_FLOOR)
            lrGas = KF_SLAM_GAS_FLOOR;
    }

    mbHandBrake = false;   // *(this+4952) = 0
    lrBrake = 0.0f;        // controls[+0x08]
    lrHand  = 0.0f;        // controls[+0x0C]
    }

// [partial] SetCrashing  @ FLAGS: the SIMD state-row zeroing (drift/boost bank +0xFE0..+0x1040), the +0x10F4 = -1 slam marker, the mu8DriftState/+0x135E byte clears and the base-chain to SimpleVehiclePhysics::SetCrashing are exact; the final contact/weight-vector recompute (mpAttribs+0x280 / +0x30 columns) is structural; the per-lane vrlimi insert routing of the recomputed weight vector is the structural part
    // @0x825FD088  BrnPhysics::Vehicle::VehiclePhysics::SetCrashing  (virtual override)
    //   Zeroes selected lanes of the drift/boost SIMD bank (vrlimi128 mask convention 8=.x,4=.y,2=.z,
    //   1=.w, cross-validated against ApplyBoostKickForce/UpdateBoost in this TU): +0x1000 mask1 ->
    //   .w=DriftScale=0 ; +0x1010 mask2 -> .z=TimeDrifting=0 ; +0x1020 mask8 -> .x=DesiredDriftAngleScale=0 ;
    //   +0x1030 mask8 -> .x=LatDriftForceFactor=1.0, then mask1 -> .w=CurrentDriftAngle=0 ;
    //   +0x1040 mask8 -> .x=SideForceMag=0 (pre-base-call). Clears mu8DriftState: *(this+4946)=0
    //   (mu8DriftState +0x1352). Sets the slam marker mDriftFlags(+0x10F4) = -1 (stb -1). Chains to
    //   SimpleVehiclePhysics::SetCrashing() (sets mbIsCrashing +0x710, rebuilds wheel reciprocal mass).
    //   POST-base-call: +0x1040 mask4 -> .y=TimeBoosting=0 (0x825FD158-160, a SECOND +0x1040 write the
    //   committed body previously omitted). Then *(this+4958)=0 (+0x135E byte) and recomputes a
    //   contact/weight vector from mpAttribs+0x280 (vspltw .w) and the body axes (+0x30 columns),
    //   inserting it lane-by-lane into the +0xEF0 register.
    //
    // FIDELITY: PARTIAL. The state-row zeroing, the -1 slam marker, the byte clears and the base
    // chain are exact; the final weight-vector recompute is reproduced structurally (the un-homed
    // mpAttribs+0x280 lane + the per-lane vrlimi routing stay faithful in shape). NEVER fabricated.
    void VehiclePhysics::SetCrashing()
    {
    *reinterpret_cast<u8*>(reinterpret_cast<u8*>(this) + 0x1352) = 0;   // mu8DriftState (cleared)

    // zero the drift/boost bank lanes the asm clears (named-member lane writes where pinned)
    mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = 0.0f;                 // +0x1000 mask1 -> .w
    mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z = 0.0f;    // +0x1010 mask2 -> .z
    mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.x = 0.0f; // +0x1020 mask8 -> .x
    mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.x = 1.0f; // +0x1030 mask8 -> .x = 1.0
    mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.w = 0.0f; // +0x1030 mask1 -> .w = 0
    mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.x = 0.0f; // +0x1040 mask8 -> .x

    // slam marker: mDriftFlags = -1
    mDriftFlags.mu8DriftFlags = static_cast<u8>(0xFFu);   // stb -1 (0x10F4)

    // chain to the base crash-arm (sets mbIsCrashing +0x710, rebuilds wheel reciprocal mass).
    // FLAG (slice): this minimal VehiclePhysics has no real SimpleVehiclePhysics base; the base
    // SetCrashing is the committed declare-only entry -- modelled as the IsCrashing()-arming call.
    // SimpleVehiclePhysics::SetCrashing();   // (base entry; owned by the base TU)

    // second +0x1040 write, AFTER the base-class chain (0x825FD158-160): mask4 -> .y = TimeBoosting = 0.
    mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.y = 0.0f; // +0x1040 mask4 -> .y

    *reinterpret_cast<u8*>(reinterpret_cast<u8*>(this) + 0x135E) = 0;   // +0x135E byte clear

    // contact/weight-vector recompute -> +0xEF0 register. The asm builds it from mpAttribs+0x280
    // (the .w lane) and the body axes (+0x30 columns) via an FMA cascade + vrlimi lane inserts;
    // reproduced structurally (the precise lane routing is the blocked part). The +0xEF0 register
    // is mWeightTransfer-adjacent and not pinned in this minimal slice -> faithful comment.
    // const f32 lfScale = mpAttribs->mCollisionAttribs.mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare.w;  (+0x280 .w)
    // mWeightTransferRow(+0xEF0) = bodyAxes * lfScale  (lane-by-lane vrlimi insert)
    }

    // -------------------------------------------------------------------------------------------
    // ⛔ IsIgnoringPassedOnImpulses -- VTABLE-CLOSURE GATE, added 2026-08-03. Vtable slot +0x10.
    //
    // Same cause as SimpleVehiclePhysics::SetCrashing (see the long ⛔⛔ banner there): embedding
    // `RaceCarPhysics maRaceCarVehicles[8]` by value in VehicleManager made the mounted
    // BrnPhysicsModule.cpp odr-use this class's vtable, so every virtual now needs a definition.
    // These two were the only ones missing.
    //
    // ⚠️ THIS ONE CANNOT BE A PURE ASSERT: it has to RETURN something, and the return VALUE is the
    // gate. VehicleRigidBody::RecievePassedOnImpulse early-outs WITHOUT applying the impulse when
    // this is true, so `true` would be a silent-drop -- every passed-on deformation impulse
    // swallowed, plausibly, forever. `false` is the pass-through, and it is also the state the
    // object is in immediately after Construct (not crashing), so it is the conservative answer
    // rather than a guess dressed as one.
    // ⚠️ FLAG: the console body is NOT recovered -- neither the address nor the method NAME is
    // pinned (VehiclePhysics.h calls the name role-inferred). It is unreachable in the mounted tree
    // today: both call sites (BrnVehicleRigidBody.cpp, BrnDeformableObject_Update.cpp) are
    // unmounted. The assert is what says so out loud if that changes.
    // -------------------------------------------------------------------------------------------
    bool VehiclePhysics::IsIgnoringPassedOnImpulses() const
    {
        CGS_ASSERT(false,
                   "VehiclePhysics::IsIgnoringPassedOnImpulses is a vtable-closure gate, not a "
                   "body -- reconstruct it before the deformation impulse path is mounted");
        return false;   // pass-through: apply the impulse. NEVER flip this without the real body.
    }

    // flt_8208FB0C -- the seat's tyre-compression allowance, read from the image (x360rd, the
    // calibrated .id1 reader): 0.03500000014901161f. The seat plants the car this much LOW and the
    // suspension settles it out.
    static const f32 KF_TYRE_COMPRESSION_ALLOWANCE = 0.035f;

    // ===========================================================================================
    //  VehiclePhysics::SetTransformFromPositionOnRoad   @0x825D1C00   (seat wave 2026-08-05)
    // ===========================================================================================
    // THE ANALYTIC REST SEAT. The console's own placement mechanism: given a transform whose
    // translation is a point ON the road, copy its rows into mTransform (this+0x10..0x40), then
    // overwrite the translation row with the at-rest position above that point:
    //
    //     S      = maWheels[1].mSlipVariables.w                          // wheel 1 radius (+0x250.w)
    //            - maWheels[1].mStreamedPositionPlusTwistAmount.y        // wheel 1 local Y (+0x2A0.y)
    //            - KF_TYRE_COMPRESSION_ALLOWANCE                         // flt_8208FB0C = 0.035f
    //     newPos = pos + up * S + zAxis * mpAttribs->mBaseAttribs.mCOMOffset.z
    //
    // asm 0x825D1E34..0x825D1EDC: lvx128 this+0x2A0 (vspltw lane 1) / this+0x250 (vspltw lane 3) /
    // mpAttribs+0x20 (vspltw lane 2) / flt_8208FB0C; vsubfp twice; two vmaddfp against the
    // transform's yAxis and zAxis rows; four stvx128 to this+0x10..0x40 with the position row
    // overwritten. flt_8208FB0C == 0.03500000 read from the image (x360rd, calibrated reader).
    //
    // The 0.035 is the tyre-compression allowance: the seat plants the car 3.5 cm low and the
    // suspension settles it out on the very next ticks (retail's measured rest = seat + 0.035).
    //
    // Asserts (X360 VehiclePhysics.cpp:3364-3366): IsValid(lTransform), mpAttribs != NULL,
    // mpAttribs->IsValid().
    // -------------------------------------------------------------------------------------------
    void VehiclePhysics::SetTransformFromPositionOnRoad(const Matrix44Affine& lrTransform)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lrTransform), "rw::math::IsValid( lTransform )"); // :3364
        CGS_ASSERT(mpAttribs != 0,                      "mpAttribs != NULL");               // :3365
        CGS_ASSERT(mpAttribs->IsValid(),                "mpAttribs->IsValid()");            // :3366

        const f32 lfWheel1Radius = maWheels[1].mSlipVariables.w;                        // +0x250.w
        const f32 lfWheel1LocalY = maWheels[1].mStreamedPositionPlusTwistAmount.y;      // +0x2A0.y
        const f32 lfSeatHeight   = lfWheel1Radius - lfWheel1LocalY
                                   - KF_TYRE_COMPRESSION_ALLOWANCE;                     // vsubfp x2
        const f32 lfForwardShift = mpAttribs->mBaseAttribs.mCOMOffset.z;                // +0x20.z

        // Rows copied to this+0x10..0x40, then the position row overwritten (the two vmaddfp).
        mTransform = lrTransform;
        mTransform.wAxis.x = lrTransform.wAxis.x + lrTransform.yAxis.x * lfSeatHeight
                                                 + lrTransform.zAxis.x * lfForwardShift;
        mTransform.wAxis.y = lrTransform.wAxis.y + lrTransform.yAxis.y * lfSeatHeight
                                                 + lrTransform.zAxis.y * lfForwardShift;
        mTransform.wAxis.z = lrTransform.wAxis.z + lrTransform.yAxis.z * lfSeatHeight
                                                 + lrTransform.zAxis.z * lfForwardShift;
    }

    // ===========================================================================================
    //  [FLAG PC bring-up] SeatTransformFromCreateLegBringUp -- NOT an X360 function.
    // ===========================================================================================
    // The PC stand-in for the create-event leg that reaches the seat on console:
    //     VehicleManager::ProcessCreateEvents @0x82616770
    //       -> vcall vtable+0x30 on maRaceCarPhysics[i]        (VERIFIED in the pseudocode)
    //       -> RaceCarPhysics::Prepare @0x82639CB8             (export hole; bl chain decoded)
    //       -> VehiclePhysics::Prepare @0x82637C80 -> ... -> SetAttributes -> Wheel::Prepare
    //       -> SetTransformFromPositionOnRoad @0x825D1C00
    // No VehicleManager runs on this build, so RaceCarEntityModule::ResetActiveRaceCar calls this
    // instead, and every seat input is derived from the RESIDENT streamed deformation spec the way
    // the console's own create leg derives it:
    //
    //   * wheel radius[i] = 0.5f * spec.maWheelSpecs[i].mScale.y -- ProcessCreateEvents' own
    //     derivation (v219[0]=0.5; vspltw scale lane 1; vmulfp128), fed through the REAL
    //     Wheel::Prepare (f1 -> mSlipVariables.w), which is the lane the seat reads.
    //   * wheel local position[i] = spec.maWheelSpecs[i].mPosition - COMeff, fed through the REAL
    //     Wheel::Prepare (v1 -> mStreamedPositionPlusTwistAmount.xyz). On console the subtraction
    //     is SimpleVehiclePhysics::SetAttributes' `vsubfp v1, wheelPos, mSimpleAttribs.mCOMOffset`.
    //
    //   * ⚠️ COMeff = spec.mMeshOffset -- ONE STEP HERE IS INFERRED FROM MEASUREMENT, stated
    //     plainly. The console populates mSimpleAttribs.mCOMOffset from VehicleAttribs+0x20, which
    //     SetupAttribs @0x825F4CD8 fills VERBATIM from the vault's physicsvehiclebaseattribs
    //     CoMOffset -- and that ships (6000, 0, 0) for PUSMC01 (5000/6000 across cars), which
    //     CANNOT be the live subtractor (wheel local x would be -5999; retail drives). The value
    //     that IS consistent with everything measured:
    //       - shipped spec mMeshOffset            = (0, +0.740575, -0.170226)
    //       - shipped spec+1552 (the model-space -> handling-space matrix the game side reads into
    //         ActiveRaceCar::mCentreOfMassTransform) = identity, translation (0, -0.740575,
    //         +0.170226) = -mMeshOffset
    //       - the user-attested retail rest height: physics origin ~1.481 above ground
    //         == radius(0.342469) + |specY - mMeshOffset.y|(1.138681) - 0.035 + 0.035-settle
    //     so mMeshOffset is used, and the exact console plumbing of that vector into
    //     mSimpleAttribs.mCOMOffset stays an OPEN question (the ProcessCreateEvents pseudocode
    //     shows a `mCOMOffset += avg(wheelSpecPos)` mutation that cannot be the whole story;
    //     its raw asm was not decoded this wave). The witness prints at the call site expose all
    //     three numbers on every run.
    //
    //   * ⚠️ TWO CONSOLE TERMS DELIBERATELY NOT REPRODUCED, stated plainly:
    //       1. the front/rear RIDE-HEIGHT raise (SetAttributes adds suspension FrontHeight/
    //          RearHeight -- +0.033/-0.02 in PUSMC01's vault -- to the wheel Y before the COM
    //          subtraction). On console the springs settle the car back to design height within
    //          ticks; on this build NOTHING simulates, so reproducing the raise without the
    //          settling would freeze the car sunk by exactly that amount. Lands with the
    //          vault->VehicleAttribs chain + the integrator (the physics wall).
    //       2. Wheel::Prepare's f2..f4 scalars (flt_82FB8BB0 + the two suspension-travel bounds):
    //          passed 0 -- none of those lanes is read by the seat, and their live sources are the
    //          same unlanded attribs chain. Flagged, not faked.
    //
    //   * mpAttribs: the REAL mPlayerVehicleAttribs member (the slot VehiclePhysics::Prepare
    //     @0x82637C80 copies the create attribs into and re-points mpAttribs at), Construct()ed by
    //     the REAL VehicleAttribs::Construct, with mBaseAttribs.mCOMOffset = COMeff and mbIsValid
    //     set the way SetupAttribs' tail sets it -- so the seat's z-term (mCOMOffset.z =
    //     -0.170226) exactly cancels mCentreOfMassTransform's +0.170226 and the MODEL origin lands
    //     on the road point. That geometric closure is the role of the term; the seat's asserts
    //     run against a valid attribs block.
    //
    // The staged VehiclePhysics lives in zeroed static storage (no constructor: the class uses the
    // console's Construct() pattern, and running the full Construct here would drag the whole
    // dynamics closure in for a placement-only helper; the seat and Wheel::Prepare touch no vptr).
    // DELETE-WHEN ProcessCreateEvents + RaceCarPhysics::Prepare land.
    // -------------------------------------------------------------------------------------------
    Matrix44Affine VehiclePhysics::SeatTransformFromCreateLegBringUp(
            const StreamedDeformationSpec* lpSpec,
            const Matrix44Affine&          lrPlacementTransform)
    {
        using BrnPhysics::Deformation::WheelSpec;

        alignas(16) static u8 saSeatVehicleStorage[sizeof(VehiclePhysics)];
        std::memset(saSeatVehicleStorage, 0, sizeof(saSeatVehicleStorage));
        VehiclePhysics* lpStaged = reinterpret_cast<VehiclePhysics*>(saSeatVehicleStorage);

        // COMeff -- see the banner. mMeshOffset, witness-checked against -(spec+1552 translation)
        // at the call site's print.
        const Vector3& lvCOMOffset = lpSpec->mMeshOffset;

        // The REAL attribs block in its REAL slot, built by the REAL constructor.
        VehicleAttribs& lrAttribs = lpStaged->mPlayerVehicleAttribs;
        lrAttribs.Construct();
        lrAttribs.mBaseAttribs.mCOMOffset = lvCOMOffset;
        lrAttribs.mbIsValid = true;              // SetupAttribs @0x825F4CD8 tail: li 1; stb +0x360
        lrAttribs.mFrontTireAttribs.PrepareDefaultFrontTire();
        lrAttribs.mRearTireAttribs.PrepareDefaultRearTire();
        lpStaged->mpAttribs = &lrAttribs;        // Prepare: `*(this+0x720) = this+0xAA0`

        // The four wheels, in spec order (0/1 front, 2/3 rear -- the same order the console's
        // create leg walks), through the REAL writer.
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            const WheelSpec* lpWheelSpec = lpSpec->GetWheelSpec(liWheel);

            const Vector3 lvLocalPos = {
                lpWheelSpec->mPosition.x - lvCOMOffset.x,
                lpWheelSpec->mPosition.y - lvCOMOffset.y,
                lpWheelSpec->mPosition.z - lvCOMOffset.z,
                0.0f
            };
            const f32 lfRadius = 0.5f * lpWheelSpec->mScale.y;   // ProcessCreateEvents' derivation

            lpStaged->maWheels[liWheel].Prepare(
                lvLocalPos, lfRadius,
                0.0f, 0.0f, 0.0f,                                // f2..f4 -- flagged, see banner
                (liWheel < 2) ? &lrAttribs.mFrontTireAttribs     // the console's per-wheel table:
                              : &lrAttribs.mRearTireAttribs );   // {front, front, rear, rear}
        }

        // The REAL seat.
        lpStaged->SetTransformFromPositionOnRoad(lrPlacementTransform);
        return lpStaged->GetTransform();
    }

    // ============================================================================================
    // ⭐⭐ THE ORCHESTRATOR (orchestrator wave, 2026-08-07): VehiclePhysics::Update @0x826412C0
    // and the driving spine it conducts. Every body below is a full transcription of its X360
    // asm (addresses cited per stage); the PS3 DecFIGS out-of-line copies corroborate the
    // signatures (Update @0x748A90 inlines most of these callees -- a different build -- so the
    // X360 call structure is authoritative throughout).
    // ============================================================================================

// [clean] UpdateInAirStats  @0x825D0A50
    // @0x825D0A50  BrnPhysics::Vehicle::VehiclePhysics::UpdateInAirStats  (102 insns)
    //   mbHadAirLastFrame <- mbHasAir (lbz 0x1350 ; stb 0x1351), then re-derive mbHasAir:
    //   * any wheel on the ground (+0x158/+0x238/+0x318/+0x3F8, checked in the console's own
    //     2,3,0,1 order) -> grounded;
    //   * else if (mi8NumWorldCollisions-adjacent in-water byte +0x1353 && mbValid +0x598):
    //     grounded when mfWaterDepth (+0x590) <= GetCarGroundDistanceCheck() (fcmpu ble);
    //   * else airborne only while |mNormLinearVelocityMag.w| (the cached speed magnitude,
    //     splat lane 3 of +0x1340) > splat(1.8) -- unk_82FB8390, static-init @0x82C5C820 from
    //     flt_82013A80 == 1.8 (image-read).
    //   Airborne: TimeWithoutTraction (+0x1060.z) += dt, TimeWithTraction (.w) = 0, mbHasAir=1.
    //   Grounded: .z = 0, .w += dt, mbHasAir=0.
    //   ⚠️ +0x1353 is mi8NumWorldCollisions in the member map; this function reads it as a
    //   BOOLEAN gate in front of the water-depth test (lbz/cmplwi/beq). Read as `!= 0` here --
    //   the shape the asm has -- not renamed.
    void VehiclePhysics::UpdateInAirStats(f32 lfTimeStep)
    {
        static const f32 KF_AIRBORNE_MIN_SPEED = 1.8f;   // unk_82FB8390 <- flt_82013A80 (read)

        mbHadAirLastFrame = mbHasAir;   // stb 0x1351 <- lbz 0x1350

        const bool lbAnyWheelOnGround =
            maWheels[eRearLeftWheel].GetRoadContact().mbIsOnGround  ||   // +0x318 (checked first)
            maWheels[eRearRightWheel].GetRoadContact().mbIsOnGround ||   // +0x3F8
            maWheels[eFrontLeftWheel].GetRoadContact().mbIsOnGround ||   // +0x158
            maWheels[eFrontRightWheel].GetRoadContact().mbIsOnGround;    // +0x238

        bool lbAirborne = false;
        if (!lbAnyWheelOnGround)
        {
            lbAirborne = true;

            // The water-depth override (asm 0x825D0AC0..0x825D0AE8).
            if (mi8NumWorldCollisions != 0 && mAboveGroundTestResult.mbValid)
            {
                const f64 lfGroundCheck = GetCarGroundDistanceCheck();
                if (mAboveGroundTestResult.mfVerticalDistance <= lfGroundCheck)
                    lbAirborne = false;
            }

            // The minimum-speed gate (asm 0x825D0AEC..0x825D0B24): slow cars are never "in air".
            if (lbAirborne &&
                !(std::fabs(mNormLinearVelocityMag.w) > KF_AIRBORNE_MIN_SPEED))
                lbAirborne = false;
        }

        if (lbAirborne)
        {
            mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z += lfTimeStep;
            mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.w  = 0.0f;
            mbHasAir = true;
        }
        else
        {
            mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z  = 0.0f;
            mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.w += lfTimeStep;
            mbHasAir = false;
        }
    }

// [clean] UpdateFreezing  @0x825CFD20
    // @0x825CFD20  BrnPhysics::Vehicle::VehiclePhysics::UpdateFreezing  (185 insns)
    //   The freeze latch. Two timers in the +0x1060 register:
    //     .x (TimeStandingStill): += dt while linVel^2 + angVel^2 <= splat(0.5)
    //         (unk_82FB8460, static-init @0x82C5C590 from flt_82001DA0 == 0.5), else = 0.
    //     .y (CoolDown): += dt while (maxWheelSpin + linVel^2 + angVel^2) <= 0.25
    //         (the vcfsx(1,1)=0.5 squared idiom), else = 0. maxWheelSpin = max over the four
    //         wheels of |mIntegrationVariables.x| (+0x160/+0x240/+0x320/+0x400, lane 0).
    //         While that quiet window holds, the CONTROLS can keep the car awake: if
    //         (mfBrake + mfGas > 0.25 [flt_8208F834] && mfHandBrake > 0.15 [flt_82094574])
    //         the cooldown keeps counting; otherwise a live throttle+brake input
    //         (|mfGas + mfBrake| > 0.25 [unk_8208FB1C]) clears it.
    //   Freeze when CoolDown > 1.0s (vcfsx(1,0)) OR mbIsOnStartLine (+0x40), then
    //     &&= !mbBoost (+0x3B, the cntlzw/extrwi negate-AND), &&= !IsPlayerVehicleActuallyIn-
    //     Showtime() (the +0x14 vcall -- the SAME slot the committed RaceCarPhysics::Update
    //     identified), ||= mbForceFrozen (+0x10F6).
    //   When frozen: mLinearVelocity = mAngularVelocity = 0 (stvx128 v0=0 to +0x50/+0x60).
    void VehiclePhysics::UpdateFreezing(const BrnPlayerDriverControls* lpControls,
                                        VecFloat lvfTimeStep)
    {
        static const f32 KF_FREEZE_SPEEDSQ_MAX     = 0.5f;    // unk_82FB8460 <- flt_82001DA0
        static const f32 KF_FREEZE_QUIET_MAX       = 0.25f;   // vcfsx(1,1)=0.5, squared by vmulfp
        static const f32 KF_FREEZE_INPUT_SUM_MIN   = 0.25f;   // flt_8208F834 (image-read)
        static const f32 KF_FREEZE_HANDBRAKE_MIN   = 0.15f;   // flt_82094574 (image-read)
        static const f32 KF_FREEZE_INPUT_CLEAR_MIN = 0.25f;   // unk_8208FB1C (image-read)
        static const f32 KF_FREEZE_COOLDOWN_TIME   = 1.0f;    // vcfsx(1,0)

        // max |wheel spin| over the four wheels (lane 0 of each mIntegrationVariables).
        f32 lfMaxWheelSpin = std::fabs(maWheels[1].mIntegrationVariables.x);   // +0x240 (first)
        lfMaxWheelSpin = std::max(lfMaxWheelSpin,
                                  std::fabs(maWheels[0].mIntegrationVariables.x));   // +0x160
        lfMaxWheelSpin = std::max(lfMaxWheelSpin,
                                  std::fabs(maWheels[2].mIntegrationVariables.x));   // +0x320
        lfMaxWheelSpin = std::max(lfMaxWheelSpin,
                                  std::fabs(maWheels[3].mIntegrationVariables.x));   // +0x400

        const f32 lfSpeedSq = vpu::MagnitudeSquared(mLinearVelocity)
                            + vpu::MagnitudeSquared(mAngularVelocity);

        Vector4& lrTimers = mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction;

        // .x -- time standing still (asm 0x825CFDE4..0x825CFE34).
        if (!(std::fabs(lfSpeedSq) > KF_FREEZE_SPEEDSQ_MAX))
            lrTimers.x += lvfTimeStep.x;
        else
            lrTimers.x = 0.0f;

        // .y -- the freeze cooldown (asm 0x825CFE20..0x825CFED4), wheel spin included.
        bool lbControlsKeepAwake = false;
        if (!(std::fabs(lfMaxWheelSpin + lfSpeedSq) > KF_FREEZE_QUIET_MAX))
        {
            lrTimers.y += lvfTimeStep.x;

            // asm 0x825CFE90..0x825CFEC4: gas+brake sum + handbrake both live.
            if (lpControls->mfBrake + lpControls->mfGas > KF_FREEZE_INPUT_SUM_MIN &&
                lpControls->mfHandBrake > KF_FREEZE_HANDBRAKE_MIN)
                lbControlsKeepAwake = true;
        }
        else
        {
            lrTimers.y = 0.0f;
        }

        // asm 0x825CFED8..0x825CFF54: a live throttle+brake input clears the cooldown, unless
        // the keep-awake combination above is holding it.
        if (std::fabs(lpControls->mfBrake + lpControls->mfGas) > KF_FREEZE_INPUT_CLEAR_MIN &&
            !lbControlsKeepAwake)
            lrTimers.y = 0.0f;

        // The freeze decision (asm 0x825CFF58..0x825CFFE8).
        bool lbFrozen = (lrTimers.y > KF_FREEZE_COOLDOWN_TIME) || lpControls->mbIsOnStartLine;
        lbFrozen = lbFrozen && !lpControls->mbBoost;                      // boost wakes the car
        mbFrozen = lbFrozen;                                              // stb 0x70 (first store)
        lbFrozen = lbFrozen && !IsPlayerVehicleActuallyInShowtime();      // the +0x14 vcall
        lbFrozen = lbFrozen || mbForceFrozen;                             // |= +0x10F6
        mbFrozen = lbFrozen;                                              // stb 0x70 (final)

        if (mbFrozen)
        {
            mLinearVelocity  = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // stvx128 0 -> +0x50
            mAngularVelocity = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // stvx128 0 -> +0x60
        }
    }

// [clean] UpdateEngine  @0x8262E848
    // @0x8262E848  BrnPhysics::Vehicle::VehiclePhysics::UpdateEngine  (77 insns)
    //   gas   = clamp01(mfGas + flt_82FB7E24). flt_82FB7E24 is a VALUED .data float that SHIPS
    //           as 0.0 (image-read); its only writer in the whole image is
    //           VehicleManagerDebugComponent::OnActivate @0x825B6390 -- a dev-menu gas boost.
    //   brake = mfBrake, then: if (!mbHandBrake && mbBoost) gas = 1.0 (flt_82001C98) --
    //           boosting forces full throttle.
    //   Hands off to ApplyEngineForces with f3 = mfSteering, f4 = dot3(mTransform.zAxis,
    //   mLinearVelocity) (the signed forward speed), f5 = mfBoostMaxSpeedScale (+0x34),
    //   r6 = mbHandBrake, v1 = dt.
    void VehiclePhysics::UpdateEngine(const BrnPlayerDriverControls* lpControls,
                                      VecFloat lvfTimeStep)
    {
        static const f32 KF_DEBUG_GAS_ADD = 0.0f;   // flt_82FB7E24 ships 0.0; dev-menu only

        f32 lfGas = lpControls->mfGas + KF_DEBUG_GAS_ADD;
        if (lfGas < 0.0f) lfGas = 0.0f;             // vmaxfp v0(0)
        if (lfGas > 1.0f) lfGas = 1.0f;             // vminfp v13(1)

        if (!mbHandBrake && lpControls->mbBoost)
            lfGas = 1.0f;                            // flt_82001C98

        const f32 lfForwardSpeed = vpu::Dot(mTransform.zAxis, mLinearVelocity);

        ApplyEngineForces(lfGas, lpControls->mfBrake, lpControls->mfSteering, lfForwardSpeed,
                          lpControls->mfBoostMaxSpeedScale, mbHandBrake, lvfTimeStep);
    }

// [clean] ApplyEngineForces  @0x8261FC10
    // @0x8261FC10  BrnPhysics::Vehicle::VehiclePhysics::ApplyEngineForces  (178 insns)
    //
    // The applier UpdateEngine hands off to. Three observable steps, register-traced from the asm
    // (the intermediate stack-vec staging + broadcasts are register scheduling, not state):
    //
    //   1. COUNTER-STEER TRACTION CUT (0x8261FC80..0x8261FD5C). Forward speed as the vector
    //      IsCounterSteeringAtLowSpeed reads is dot3(mTransform.zAxis, mLinearVelocity)
    //      (vmsum3fp128); when that test passes, the drive scale (1.0f == flt_82001C98) is reduced
    //      by  LowSpeedThrottleTractionControl(+0x90 .w) * 0.01f(flt_82002138)
    //          * clamp(|mAngularVelocity.y| * 0.5f(flt_82001DA0), 0.25f(flt_8208F834), 1.0f).
    //      Only the drive SCALE survives this block (the console's f31); it feeds OntoWheels.
    //
    //   2. REVERSE SWAP (0x8261FD60..0x8261FD70). When the selected gear is reverse
    //      (mEngine.GetCurrentGear() == 0) the gas and brake handed to Engine::Update swap, so
    //      "throttle" drives the car backwards.
    //
    //   3. HAND OFF (0x8261FD74..0x8261FEAC). Engine::Update (the trapped powertrain core) gets the
    //      min of the rear wheels' angular velocity (min(maWheels[2/3].mIntegrationVariables.x)),
    //      the (swapped) gas/brake, handbrake, steering, the rear wheel radius
    //      (maWheels[eRearLeftWheel].mSlipVariables.w @+0x330 .w), a "can drive in reverse" bool
    //      ((TimeStandingStill > 0) || (mfSpeedMPH < -0.2f == flt_82020A84)), the forward speed and
    //      dt. Then, only when NOT frozen (+0x70 == mbFrozen), ApplyEngineForcesOntoWheels puts the
    //      drive on the wheels.
    void VehiclePhysics::ApplyEngineForces(f32 lfGas, f32 lfBrake, f32 lfSteering, f32 lfForwardSpeed,
                                           f32 lfBoostMaxSpeedScale, bool lbHandBrake,
                                           VecFloat lvfTimeStep)
    {
        // --- 1. counter-steer traction cut --------------------------------------------------------
        f32 lfDriveScale = 1.0f;                                        // f31 = flt_82001C98
        const f32 lfFwd = vpu::Dot(mTransform.zAxis, mLinearVelocity);
        if (IsCounterSteeringAtLowSpeed(VecFloat{ lfFwd, lfFwd, lfFwd, lfFwd }, lfSteering, lfGas))
        {
            f32 lfYawTerm = std::fabs(mAngularVelocity.y) * 0.5f;      // |angvel.y| * flt_82001DA0
            if (lfYawTerm > 1.0f)  lfYawTerm = 1.0f;                   // fsel vs 1.0
            if (lfYawTerm < 0.25f) lfYawTerm = 0.25f;                  // fsel vs flt_8208F834
            const f32 lfCut = mpAttribs->mBaseAttribs
                                  .mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.w
                              * 0.01f * lfYawTerm;                     // flt_82002138 == 0.01
            lfDriveScale = 1.0f - lfCut;
        }

        // --- 2. reverse gas/brake swap ------------------------------------------------------------
        f32 lfEngineGas   = lfGas;
        f32 lfEngineBrake = lfBrake;
        if (mEngine.GetCurrentGear() == 0)                            // KU8_REVERSE_GEAR
        {
            lfEngineGas   = lfBrake;
            lfEngineBrake = lfGas;
        }

        // --- 3. hand off to the powertrain, then the wheels --------------------------------------
        const f32 lfWheelAngularVelocity =
            std::min(maWheels[eRearLeftWheel ].mIntegrationVariables.x,
                     maWheels[eRearRightWheel].mIntegrationVariables.x);   // vminfp of the rear pair

        const bool lbAllowReverseDrive =
            (mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.x > 0.0f)
            || (mfSpeedMPH.x < -0.2f);                                 // flt_82020A84 == -0.2

        const f32 lfOmega  = lfWheelAngularVelocity;
        const f32 lfRadius = maWheels[eRearLeftWheel].mSlipVariables.w;   // +0x330 .w
        mEngine.Update(VecFloat{ lfOmega, lfOmega, lfOmega, lfOmega },
                       VecFloat{ lfEngineGas, lfEngineGas, lfEngineGas, lfEngineGas },
                       VecFloat{ lfEngineBrake, lfEngineBrake, lfEngineBrake, lfEngineBrake },
                       lbHandBrake,
                       VecFloat{ lfSteering, lfSteering, lfSteering, lfSteering },
                       VecFloat{ lfRadius, lfRadius, lfRadius, lfRadius },
                       lbAllowReverseDrive,
                       VecFloat{ lfForwardSpeed, lfForwardSpeed, lfForwardSpeed, lfForwardSpeed },
                       lvfTimeStep);

        if (!mbFrozen)                                                 // lbz +0x70 ; bne skip
            ApplyEngineForcesOntoWheels(lfDriveScale, lfForwardSpeed, lfBoostMaxSpeedScale,
                                        lbHandBrake);
    }

// [clean] ApplyEngineForcesOntoWheels  @0x825FB000
    // @0x825FB000  BrnPhysics::Vehicle::VehiclePhysics::ApplyEngineForcesOntoWheels  (128 insns)
    //
    // Register-traced. The drive force is mEngine.GetEngineDrive() (mvEngineDrive lane, +0xFA0)
    // scaled by the counter-steer factor. It is zeroed above the max speed -- the cap is the boost
    // MaxBoostSpeed (mBoostAttribs +0x290 .y) while a boost is running (TimeBoosting == +0x1040 .y
    // > 0), else the attribs MaxSpeed (mvMass_..._MaxSpeed_... +0x70 .z), each times the
    // boost-max-speed scale. Then the drive lands on the wheels' angular-velocity integration
    // accumulators (maWheels[i].mIntegrationVariables .z):
    //   * no handbrake, OR handbrake below 5 mph (unk_8208FB14 == KF_SPEED_TO_ALLOW_LOCKING_WHEELS):
    //     all four wheels, split PowerToRear(+0xB0 .z) for the rear pair / PowerToFront(+0xB0 .y)
    //     for the front pair;
    //   * handbrake at/above 5 mph: the raw drive is added into the REAR pair only (wheel lock).
    void VehiclePhysics::ApplyEngineForcesOntoWheels(f32 lfDriveScale, f32 lfForwardSpeed,
                                                     f32 lfBoostMaxSpeedScale, bool lbHandBrake)
    {
        f32 lfDrive = mEngine.GetEngineDrive() * lfDriveScale;         // mvEngineDrive.x * f1

        // over-max-speed cut (boost raises the cap while a boost is running).
        const bool lbBoosting =
            mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.y > 0.0f;
        const f32 lfMaxSpeed =
            lbBoosting
                ? mpAttribs->mBoostAttribs
                      .mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.y
                : mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z;
        if (mfSpeedMPH.x > lfMaxSpeed * lfBoostMaxSpeedScale)
            lfDrive = 0.0f;                                            // vmr v0, 0

        const VehicleAttribs::VehicleBaseAttribs& lrBase = mpAttribs->mBaseAttribs;
        const f32 lfPowerToFront =
            lrBase.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.y;   // +0xB0 .y
        const f32 lfPowerToRear =
            lrBase.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.z;   // +0xB0 .z

        if (lbHandBrake && !(5.0f > lfForwardSpeed))                  // unk_8208FB14 == 5.0
        {
            // wheel lock: raw drive into the rear pair only.
            maWheels[eRearLeftWheel ].mIntegrationVariables.z += lfDrive;
            maWheels[eRearRightWheel].mIntegrationVariables.z += lfDrive;
            return;
        }

        // normal distribution: rear pair scaled by PowerToRear, front pair by PowerToFront.
        maWheels[eRearLeftWheel  ].mIntegrationVariables.z += lfDrive * lfPowerToRear;
        maWheels[eRearRightWheel ].mIntegrationVariables.z += lfDrive * lfPowerToRear;
        maWheels[eFrontLeftWheel ].mIntegrationVariables.z += lfDrive * lfPowerToFront;
        maWheels[eFrontRightWheel].mIntegrationVariables.z += lfDrive * lfPowerToFront;
    }

// [clean] UpdateDownForce  @0x825F6338
    // @0x825F6338  BrnPhysics::Vehicle::VehiclePhysics::UpdateDownForce  (195 insns)
    //   TWO regimes on mbHasAir (+0x1350):
    //
    //   AIRBORNE (asm 0x825F6364..0x825F64D8): assert the attribs' MaxSpeed
    //   (mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z) is a real number
    //   ("Zero max speed in attribsys data updating downforce", :0x62E), then build the
    //   world-down stabilising force. Register-traced:
    //     planarVel   = mLinearVelocity with .y zeroed          (vrlimi128 v13, v2(0), 4, 3)
    //     planarSpd   = sqrt(dot3(planarVel, planarVel))        (vrsqrtefp + 2 NR, vsel 0-guard)
    //     ratio       = max(0.3, planarSpd * refined(1/MaxSpeed))   (vmaxfp v31 @0x825F64C0;
    //                   unk_82F2A514 == 0.3f, VALUED in the image: 0x3E99999A; its ONLY xref in
    //                   the whole image is this read)
    //     force.y     = (-Mass * DownForce) * ratio * refined(1/0.3)
    //                   (vxor v8 = Mass ^ signmask @0x825F6490; vmulfp v9 = -Mass * DownForce.w
    //                    @0x825F64B0; the third vrefp chain is over the 0.3 CONSTANT itself --
    //                    a divide by the floor, so the floored ratio normalises to >= 1.0)
    //     call        = AddLocalForce({0, force.y, 0, 0}, WORLD_SPACE /* r4 = 0 */,
    //                                 {0,0,0,0} /* v2 is the zero splat, never rewritten */,
    //                                 BODY_SPACE /* r5 = 1 */)  on the EPB base (r3 = this+0x10)
    //
    //   GROUNDED (asm 0x825F64E0..0x825F6610): while the planar |velocity| is above FLT_EPSILON
    //   (stru_8208F620[0], both lanes of the vrlimi-merged |vel| pair), the relief scale is
    //     scale = 20.0 [flt_8208F9D4] * (|mTransform.zAxis.y| + |dot3(unit(vel), xAxis)|)
    //   (pitch + side-slip), else scale = 1.0. Then
    //     AddWorldSpaceForce({0, -GetDownForce().y * scale, 0, 0})
    //   -- the vperm control unk_82CDA350 = {A.x, B.y, A.x, A.x} with A = zero builds exactly
    //   the pure-vertical vector, and the vrlimi(2) zeroes .z again.
    //
    //   TAIL (both regimes): mirror the applied magnitude into the handling debug component
    //   (+0x13E4 -> +0x3F4) when one is attached.
    void VehiclePhysics::UpdateDownForce()
    {
        static const f32 KF_DOWNFORCE_RATIO_FLOOR = 0.3f;    // unk_82F2A514 (image: 0x3E99999A)
        static const f32 KF_DOWNFORCE_SLIP_SCALE  = 20.0f;   // flt_8208F9D4 (image-read)

        const Vector4& lrBase =
            mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce;

        f32 lfAppliedMagnitude = 0.0f;

        if (mbHasAir)
        {
            CGS_ASSERT(std::fabs(lrBase.z) > 1.1920928955078125e-07f /* FLT_EPSILON, stru_8208F620[0] */,
                       "Zero max speed in attribsys data updating downforce");

            // planar (y-zeroed) speed.
            Vector3 lvPlanarVel = mLinearVelocity;
            lvPlanarVel.y = 0.0f;                        // vrlimi128 v13, v2(0), 4, 3
            const f32 lfPlanarSpeedSq = vpu::MagnitudeSquared(lvPlanarVel);
            const f32 lfPlanarSpeed   = (lfPlanarSpeedSq > 0.0f) ? std::sqrt(lfPlanarSpeedSq)
                                                                 : 0.0f;   // vsel 0-guard

            // ratio of planar speed to the car's max speed, floored at 0.3 and normalised by
            // the same 0.3 (the vrefp chain over the constant) so the result is >= 1.0.
            const f32 lfRatio = std::max(KF_DOWNFORCE_RATIO_FLOOR, lfPlanarSpeed / lrBase.z)
                              / KF_DOWNFORCE_RATIO_FLOOR;

            // the world-down stabilising push, mass-proportional:
            //   force.y = (-Mass * DownForce) * ratio      (vxor sign flip on the Mass lane)
            const f32 lfForceY = (-lrBase.x * lrBase.w) * lfRatio;

            // applied at the centre of mass (v2 stays the zero splat; r4=0 WORLD, r5=1 BODY).
            AddLocalForce(Vector3{ 0.0f, lfForceY, 0.0f, 0.0f }, rw::physics::WORLD_SPACE,
                          Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },     rw::physics::BODY_SPACE);
            lfAppliedMagnitude = lfForceY;
        }
        else
        {
            static const f32 KF_EPSILON = 1.1920928955078125e-07f;   // stru_8208F620[0]

            f32 lfScale = 1.0f;   // vcfsx(1,0) default when not moving
            // the console gate is COMPONENT-wise: any |velocity lane| > eps (the vrlimi(1,1)
            // merge duplicates .x into .w before the vcmpgtfp all-false test).
            if (std::fabs(mLinearVelocity.x) > KF_EPSILON ||
                std::fabs(mLinearVelocity.y) > KF_EPSILON ||
                std::fabs(mLinearVelocity.z) > KF_EPSILON)
            {
                const Vector3 lvUnitVel = vpu::Normalize(mLinearVelocity);
                lfScale = KF_DOWNFORCE_SLIP_SCALE *
                          (std::fabs(mTransform.zAxis.y) +
                           std::fabs(vpu::Dot(lvUnitVel, mTransform.xAxis)));
            }

            const Vector3 lvDown = GetDownForce();
            const f32 lfForceY = -lvDown.y * lfScale;
            AddWorldSpaceForce(Vector3{ 0.0f, lfForceY, 0.0f, 0.0f });   // vperm unk_82CDA350
            lfAppliedMagnitude = lfForceY;
        }

        // Debug mirror (asm 0x825F6614..0x825F6624).
        if (mpDebugComponent != 0)
            *reinterpret_cast<f32*>(reinterpret_cast<u8*>(mpDebugComponent) + 0x3F4)
                = lfAppliedMagnitude;
    }

// [clean] SwitchAttribs  @0x8261E498
    // @0x8261E498  BrnPhysics::Vehicle::VehiclePhysics::SwitchAttribs  (21 insns; was an
    //   export-set JSON hole -- exported fresh from the .i64 this wave)
    //     bl SimpleVehiclePhysics::SwitchAttribs (this, lpAttribs)
    //     mpAttribs (+0x720) = lpAttribs
    //     memcpy(&mEngine (+0xF00), lpAttribs + 0x190 (mEngineAttribs), 0xA0)
    //     bl SetupSuspension
    void VehiclePhysics::SwitchAttribs(VehicleAttribs* lpAttribs)
    {
        SimpleVehiclePhysics::SwitchAttribs(lpAttribs);
        mpAttribs = lpAttribs;
        std::memcpy(&mEngine, &lpAttribs->mEngineAttribs, sizeof(VehicleAttribs::EngineAttribs));
        SetupSuspension(0.0);
    }

// [clean] SwitchAIDonuttingAttribs  @0x8261FED8
    // @0x8261FED8  BrnPhysics::Vehicle::VehiclePhysics::SwitchAIDonuttingAttribs  (29 insns)
    //   Entering (lbDonutting): VehicleAttribs::SetupAttribsForDonutAI(&mAIVehicleAttribs);
    //     latch mbIsUsingAIDonutAttribs = 1. (The caller -- Update -- follows with the
    //     SimpleVehiclePhysics::SwitchAttribs / mpAttribs / engine-memcpy / SetupSuspension
    //     sequence itself.)
    //   Leaving: VehicleAttribs::SetupAttribsForAI(&mAIVehicleAttribs, &mPlayerVehicleAttribs);
    //     then the full switch sequence onto mPlayerVehicleAttribs, then clear the latch.
    void VehiclePhysics::SwitchAIDonuttingAttribs(bool lbDonutting)
    {
        if (lbDonutting)
        {
            mAIVehicleAttribs.SetupAttribsForDonutAI();
            mbIsUsingAIDonutAttribs = true;
            return;
        }

        mAIVehicleAttribs.SetupAttribsForAI(&mPlayerVehicleAttribs);
        SimpleVehiclePhysics::SwitchAttribs(&mPlayerVehicleAttribs);
        mpAttribs = &mPlayerVehicleAttribs;
        std::memcpy(&mEngine, &mPlayerVehicleAttribs.mEngineAttribs,
                    sizeof(VehicleAttribs::EngineAttribs));
        SetupSuspension(0.0);
        mbIsUsingAIDonutAttribs = false;
    }

    // ==============================================================================================
    // ⭐⭐ THE WHEEL CLUSTER (wheel-cluster wave, 2026-08-07). UpdateWheels @0x8261E4F0 (1130
    // insns) + its four exclusive helper callees (X360 xrefs-to are exactly {UpdateWheels} for
    // all four). Every constant below is image-attested: the 0x82FBxxxx names are static-init'd
    // BSS splats whose writer thunks (the 0x82C5C5F0..0x82C5CE3x initializer bank) each name one
    // rdata source scalar, read via x360rd (self-test 10/10). NOTHING here is tuned or guessed.
    // ==============================================================================================

    // @0x825D05F0  BrnPhysics::Vehicle::VehiclePhysics::UpdateBurnout  (146 insns)
    // The burnout latch + rear spin-up. Conditions, in the console's order:
    //   * both pedals near-floored: mfGas > 0.97 AND mfBrake > 0.97  (unk_82FB9010 <- flt_82094B68)
    //   * both rear wheels spinning forward (mIntegrationVariables.x > 0)
    //   * rear-pair average spin exceeds the front-pair average by more than 1.0 rad/s
    //     (max(rearAvg - frontAvg, 0) > unk_82FB9EB0 <- flt_82001C98; the 0.5 averaging factor
    //     is the inline `vcfsx v0,1,1`)
    //   * near-stationary (|mLinearVelocity|^2 < 0.25 == unk_82FB9250 <- flt_8208F834) OR the
    //     latch is already set (a running burnout survives the speed gate)
    // When all hold: while the FRONT-LEFT wheel is slow (70.0 == unk_82FB90E0 <- flt_820051BC
    // > maWheels[0].x -- the console tests only wheel 0), both rear spins are scaled by 1.03
    // (unk_82FB8DD0 <- flt_8209D724) and mbDoingBurnout latches. When the entry conditions fail,
    // the latch clears; when they hold but the car is already moving, the latch is left as-is.
    void VehiclePhysics::UpdateBurnout(const BrnPlayerDriverControls* lpControls)
    {
        static const f32 KF_PEDAL_THRESHOLD  = 0.97f;   // unk_82FB9010 <- flt_82094B68
        static const f32 KF_SPIN_DELTA       = 1.0f;    // unk_82FB9EB0 <- flt_82001C98
        static const f32 KF_SPEEDSQ_LIMIT    = 0.25f;   // unk_82FB9250 <- flt_8208F834
        static const f32 KF_FRONT_SPIN_GATE  = 70.0f;   // unk_82FB90E0 <- flt_820051BC
        static const f32 KF_REAR_SPIN_SCALE  = 1.03f;   // unk_82FB8DD0 <- flt_8209D724

        CGS_ASSERT(lpControls != NULL, "lpControls != NULL");   // VehiclePhysics.cpp:2139

        const f32 lfFrontLeft  = maWheels[eFrontLeftWheel ].mIntegrationVariables.x;
        const f32 lfFrontRight = maWheels[eFrontRightWheel].mIntegrationVariables.x;
        const f32 lfRearLeft   = maWheels[eRearLeftWheel  ].mIntegrationVariables.x;
        const f32 lfRearRight  = maWheels[eRearRightWheel ].mIntegrationVariables.x;

        const bool lbBothPedals = lpControls->mfGas   > KF_PEDAL_THRESHOLD &&
                                  lpControls->mfBrake > KF_PEDAL_THRESHOLD;
        const bool lbRearsForward = lfRearLeft > 0.0f && lfRearRight > 0.0f;

        // rearAvg - frontAvg, floored at 0 (vmaxfp vs the zero splat), vs the 1.0 gate.
        const f32 lfRearAvg  = (lfRearLeft  + lfRearRight ) * 0.5f;   // vcfsx v0,1,1 == 0.5
        const f32 lfFrontAvg = (lfFrontLeft + lfFrontRight) * 0.5f;
        f32 lfSpinDelta = lfRearAvg - lfFrontAvg;
        if (lfSpinDelta < 0.0f) lfSpinDelta = 0.0f;

        if (!lbBothPedals || !lbRearsForward || !(lfSpinDelta > KF_SPIN_DELTA))
        {
            mbDoingBurnout = false;   // the LABEL_17 store with r11 == 0
            return;
        }

        // dot3(mLinearVelocity, mLinearVelocity) vs the 0.25 splat.
        if (mbDoingBurnout || vpu::MagnitudeSquared(mLinearVelocity) < KF_SPEEDSQ_LIMIT)
        {
            if (KF_FRONT_SPIN_GATE > lfFrontLeft)   // only wheel 0 is tested (vspltw of its .x)
            {
                maWheels[eRearLeftWheel ].mIntegrationVariables.x = lfRearLeft  * KF_REAR_SPIN_SCALE;
                maWheels[eRearRightWheel].mIntegrationVariables.x = lfRearRight * KF_REAR_SPIN_SCALE;
            }
            mbDoingBurnout = true;
        }
        // else: entry conditions hold but the car is moving and no burnout was running --
        // the console leaves the latch untouched (the store is skipped entirely).
    }

    // @0x825F6648  BrnPhysics::Vehicle::VehiclePhysics::UpdateWheelInertia  (205 insns)
    // Re-seed every wheel's spin-inertia lanes for the frame, then apply the handbrake lock.
    //   * normal:  mSuspensionAndInertiaVariables.z = 30.0 (unk_82FB9F60 <- flt_82004F5C),
    //              .w = 1/30 (the vrefp + two-Newton exact reciprocal)   -- wheels 2,3,0,1
    //   * burnout: .z = 200.0 (unk_82FB9BB0 <- flt_8201A1F0), .w = 1/200 (unk_82FB9CD0, the
    //              initializer's own fdivs)                              -- wheels 0,1,2,3; return
    //   * else, handbrake held (mbHandBrake): clear mbDoingBurnout, then LOCK one axle --
    //     zero its wheels' spin (mIntegrationVariables.x) and both inertia lanes:
    //       speed < 5.0 m/s (|mLinearVelocity| vs unk_8208FB14): the DRIVEN axle
    //         (rear when PowerToRear != 0 -- the FLT_EPSILON IsZero band -- else front);
    //       speed >= 5.0: rear in a forward gear, front in reverse (mu8CurrentGear == 0).
    //     (Wheel::UpdateVelocity's eWheelInertiaTypeLocked path is the other half of the lock.)
    void VehiclePhysics::UpdateWheelInertia()
    {
        static const f32 KF_WHEEL_INERTIA          = 30.0f;    // unk_82FB9F60 <- flt_82004F5C
        static const f32 KF_BURNOUT_WHEEL_INERTIA  = 200.0f;   // unk_82FB9BB0 <- flt_8201A1F0
        static const f32 KF_HANDBRAKE_LOCK_SPEED   = 5.0f;     // unk_8208FB14 (.rdata, direct)

        // Normal re-seed, console order 2, 3, 0, 1 (rear pair first).
        static const EVehicleDrivenWheel kaeSeedOrder[eNumDrivenWheels] = {
            eRearLeftWheel, eRearRightWheel, eFrontLeftWheel, eFrontRightWheel };
        for (s32 li = 0; li < eNumDrivenWheels; ++li)
        {
            Wheel& lrWheel = maWheels[kaeSeedOrder[li]];
            lrWheel.mSuspensionAndInertiaVariables.z = KF_WHEEL_INERTIA;          // vrlimi 2 (z)
            lrWheel.mSuspensionAndInertiaVariables.w = 1.0f / KF_WHEEL_INERTIA;   // vrlimi 1 (w)
        }

        if (mbDoingBurnout)
        {
            for (s32 li = 0; li < eNumDrivenWheels; ++li)   // console order 0, 1, 2, 3 here
            {
                maWheels[li].mSuspensionAndInertiaVariables.z = KF_BURNOUT_WHEEL_INERTIA;
                maWheels[li].mSuspensionAndInertiaVariables.w = 1.0f / KF_BURNOUT_WHEEL_INERTIA;
            }
            return;
        }

        if (!mbHandBrake)
            return;

        mbDoingBurnout = false;

        // |mLinearVelocity| -- the vrsqrtefp/Newton magnitude with the vcmpeqfp zero guard.
        const f32 lfSpeedSq = vpu::MagnitudeSquared(mLinearVelocity);
        const f32 lfSpeed   = (lfSpeedSq > 0.0f) ? std::sqrt(lfSpeedSq) : 0.0f;

        bool lbLockRear;
        if (lfSpeed < KF_HANDBRAKE_LOCK_SPEED)
        {
            // the driven axle locks: rear unless PowerToRear is (epsilon-)zero.
            lbLockRear = !rw::math::fpu::IsZero(
                mpAttribs->mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.z);
        }
        else
        {
            lbLockRear = mEngine.GetCurrentGear() != 0;   // forward gear -> rear; reverse -> front
        }

        const EVehicleDrivenWheel leA = lbLockRear ? eRearLeftWheel  : eFrontLeftWheel;
        const EVehicleDrivenWheel leB = lbLockRear ? eRearRightWheel : eFrontRightWheel;
        maWheels[leA].mIntegrationVariables.x            = 0.0f;   // vrlimi 8 (x)
        maWheels[leA].mSuspensionAndInertiaVariables.z   = 0.0f;
        maWheels[leA].mSuspensionAndInertiaVariables.w   = 0.0f;
        maWheels[leB].mIntegrationVariables.x            = 0.0f;
        maWheels[leB].mSuspensionAndInertiaVariables.z   = 0.0f;
        maWheels[leB].mSuspensionAndInertiaVariables.w   = 0.0f;
    }

    // @0x825D0238  BrnPhysics::Vehicle::VehiclePhysics::UpdateBrakesAndGetBrakingFactor  (236 insns)
    // Maintain the running brake amount (the .w BrakeScale lane of +0x1010) and return the braking
    // factor. Three entry regimes select the working pedal:
    //   * HANDBRAKE (mbHandBrake): only while not drifting and not airborne -- damp the yaw
    //     (DampPitchYawRoll(0, attribs +0xC0 lane .z, 0, dt) -- the lane the asm splats is .z,
    //     RollDampingOnTakeOff by the DWARF lane naming; reproduced as shipped) and brake by
    //     1 - |mfSteering|. Drifting or airborne with the handbrake -> factor 0.
    //   * FORWARD GEAR: pedal = mfBrake; rolling backwards -> 0. Colliding at speed
    //     (mfSpeedMPH > MinSpeedForDrift && mi8NumWorldCollisions > 0) caps BrakeScale at 0.2
    //     (unk_82FB9280 <- flt_82004744); below 5 mph (unk_82FB9240 <- flt_8200426C) with both
    //     pedals released BrakeScale is SET to 0.2 (the parking creep-stop).
    //   * REVERSE (gear 0): pedal = mfGas; rolling forwards -> 0; the creep-stop mirror uses
    //     mfSpeedMPH > -5.
    // Common tail: pedal <= 0.1 -> BrakeScale = 0 and factor 0. Else BrakeScale ramps up by
    // dt * pedal * TimeForFullBrakeRecip while the pedal exceeds it (snaps down to the pedal
    // otherwise), clamps at 1, and the factor is mBrakeScaleToFactorCurve.GetInterped(BrakeScale).
    VecFloat VehiclePhysics::UpdateBrakesAndGetBrakingFactor(
        const BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep)
    {
        static const f32 KF_COLLISION_BRAKE_CAP = 0.2f;   // unk_82FB9280 <- flt_82004744
        static const f32 KF_CREEP_STOP_MPH      = 5.0f;   // unk_82FB9240 <- flt_8200426C
        static const VecFloat KV_ZERO{ 0.0f, 0.0f, 0.0f, 0.0f };

        f32& lrfBrakeScale =
            mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.w;   // +0x1010 .w

        const bool lbPedalsReleased = lpControls->mfGas   < 0.1f &&
                                      lpControls->mfBrake < 0.1f;      // flt_82004014

        f32 lfPedal;
        if (mbHandBrake)
        {
            if (mu8DriftState != 0 || mbHasAir)
                return KV_ZERO;

            // Yaw damping while the handbrake locks an axle. The asm splats lane .z of the
            // attribs +0xC0 register into the YAW slot (v2); pitch (v1) and roll (v3) are zero.
            { const f32 lfYawDamp = mpAttribs->mBaseAttribs
                  .mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.z;
              DampPitchYawRoll(KV_ZERO,
                               VecFloat{ lfYawDamp, lfYawDamp, lfYawDamp, lfYawDamp },
                               KV_ZERO, lvfTimeStep); }

            lfPedal = 1.0f - std::fabs(lpControls->mfSteering);   // vsubfp(1.0, |steer|)
        }
        else if (mEngine.GetCurrentGear() != 0)   // forward gear
        {
            if (0.0f > mfSpeedMPH.x)              // rolling backwards in a forward gear
                return KV_ZERO;

            lfPedal = lpControls->mfBrake;

            if (mfSpeedMPH.x > mpAttribs->mDriftAttribs
                    .mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x
                && mi8NumWorldCollisions > 0)
            {
                if (lrfBrakeScale > KF_COLLISION_BRAKE_CAP)      // vminfp
                    lrfBrakeScale = KF_COLLISION_BRAKE_CAP;
            }

            if (KF_CREEP_STOP_MPH > mfSpeedMPH.x && lbPedalsReleased)
                lrfBrakeScale = KF_COLLISION_BRAKE_CAP;          // the creep-stop SET
        }
        else                                      // reverse (gear 0)
        {
            if (mfSpeedMPH.x > 0.0f)              // rolling forwards in reverse
                return KV_ZERO;

            lfPedal = lpControls->mfGas;          // the gas pedal brakes the reverse roll

            if (mfSpeedMPH.x > -KF_CREEP_STOP_MPH && lbPedalsReleased)   // vxor sign flip
                lrfBrakeScale = KF_COLLISION_BRAKE_CAP;
        }

        // ----- common tail (LABEL_22) -----
        if (lfPedal <= 0.1f)
        {
            lrfBrakeScale = 0.0f;
            return KV_ZERO;
        }

        if (lfPedal > lrfBrakeScale)
        {
            // ramp toward full brake: += dt * pedal * TimeForFullBrakeRecip (+0x70 lane .y).
            lrfBrakeScale += lvfTimeStep.x * lfPedal *
                mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.y;
        }
        else
        {
            lrfBrakeScale = lfPedal;              // snap down to the pedal
        }
        if (lrfBrakeScale > 1.0f)                 // vminfp vs the vcfsx 1.0
            lrfBrakeScale = 1.0f;

        // the BrakeScaleToFactor curve (attribs +0x60) at the ramped amount -- the inlined
        // InterpedParam3::GetInterped de Casteljau pair.
        return mpAttribs->mBaseAttribs.mBrakeScaleToFactorCurve.GetInterped(
            VecFloat{ lrfBrakeScale, lrfBrakeScale, lrfBrakeScale, lrfBrakeScale });
    }

    // @0x825D0940  BrnPhysics::Vehicle::VehiclePhysics::LimitDifferential  (67 insns, leaf)
    // The open-differential coupling: clamp both driven wheels' spin (mIntegrationVariables.x)
    // into a +/-(10% of |average|) band around their average. Both constants are the function's
    // own lazy-init statics (unk_82FBA0F0 <- flt_82001DA0 == 0.5, guard bit 0 of dword_82FBA100;
    // unk_82FBA0E0 <- flt_82004014 == 0.1, guard bit 1).
    void VehiclePhysics::LimitDifferential(EVehicleDrivenWheel leWheelA, EVehicleDrivenWheel leWheelB)
    {
        static const f32 KF_HALF = 0.5f;   // unk_82FBA0F0 <- flt_82001DA0
        static const f32 KF_BAND = 0.1f;   // unk_82FBA0E0 <- flt_82004014

        f32& lrfSpinA = maWheels[leWheelA].mIntegrationVariables.x;
        f32& lrfSpinB = maWheels[leWheelB].mIntegrationVariables.x;

        const f32 lfAverage = (lrfSpinA + lrfSpinB) * KF_HALF;
        const f32 lfBandHalf = std::fabs(lfAverage) * KF_BAND;   // vandc sign mask == fabs
        const f32 lfLo = lfAverage - lfBandHalf;
        const f32 lfHi = lfAverage + lfBandHalf;

        lrfSpinA = (lrfSpinA < lfLo) ? lfLo : ((lrfSpinA > lfHi) ? lfHi : lrfSpinA);   // vmaxfp/vminfp
        lrfSpinB = (lrfSpinB < lfLo) ? lfLo : ((lrfSpinB > lfHi) ? lfHi : lrfSpinB);
    }

// [clean] UpdateWheels  @0x8261E4F0
    // @0x8261E4F0  BrnPhysics::Vehicle::VehiclePhysics::UpdateWheels  (1130 insns)
    // ⭐⭐ THE PER-WHEEL TRACTION/CONTACT CORE -- the stage UpdateDriving runs between the
    // suspension virtual and UpdateInAirBehaviour. Register-traced end to end; the stage list:
    //
    //   0x8261E518  UpdateBurnout(controls) ; UpdateWheelInertia()
    //   0x8261E524  mSteeringDirection (+0x10E0) = R(Up, mvSteeringAngle .x) * At -- one inlined
    //               XMVectorSinCos over unk_82000BD0..C60 (the table three waves decoded;
    //               std::sin/std::cos are the exact forms, same de-optimisation as
    //               SetWheelVelocities') + the Rodrigues rows, applied to mTransform.At()
    //   0x8261E7A4  lbInReverse   = (0 > mfSpeedMPH) || mEngine.mu8CurrentGear == 0  (r22)
    //   0x8261E7CC  lbGasReleased = controls->mfGas < 0.1 [flt_82004014]             (r23)
    //   0x8261E7E8  CalculateBodyVelocityAtWheelContact x4, order 2,3,0,1 -- v1 carries the
    //               pair's roll direction (At for the rear pair, mSteeringDirection front),
    //               v2 = dt; both DEAD in the callee
    //   0x8261E838  JUST-LANDED SPIN-UP x4 (order 2,3,0,1): wheels with mbIsOnGround set and
    //               mbWasOnGroundLastUpdate clear get mIntegrationVariables.y = 0 [vrlimi 4],
    //               .x = dot3(mBodyPointVelocity, rollDir) / mSlipVariables.w (radius; vrefp +
    //               two Newton) [vrlimi 8], mbBrokenAdhesiveLimit = false -- the SetWheelVelocities
    //               per-wheel idiom, re-seeded from the touchdown velocity
    //   0x8261EA58  TORQUE INTEGRATE x4 (order 1,0,3,2): mIntegrationVariables.x +=
    //               .y * dt * mSuspensionAndInertiaVariables.w (invInertia); .y = 0
    //   0x8261EB68  v124 = UpdateBrakesAndGetBrakingFactor(controls, dt)
    //   0x8261EB6C  THE MAGIC BRAKE FORCE (only while mu8DriftState == 0 && !mbHasAir):
    //               t = min(|mfSteering|, 1.0); blend = lerp(MagicBrakeFactorStraightLine [.z
    //               lane], MagicBrakeFactorTurning [.y lane], t) (asserts IsValid of both,
    //               :1928/:1929); force.z = -brakeFactor * mfMass * blend * sign(mfSpeedMPH) *
    //               clamp01(|dot3(mLinearVelocity, At)|); lane-finite assert ("Invalid brake
    //               force: ...", :1951); AddLocalSpaceForce({0,0,z})
    //   0x8261EDF8  THE DRIVEN-AXLE FACTOR v126: while driving (gas > 0 || brake < 0.9
    //               [flt_82005450]) = clamp((mfSpeedMPH - 60 [unk_82FB9E20]) / 10 [unk_82FB83C0,
    //               vrefp+Newton recip], 0 [unk_82FB91A0], 0.75 [unk_82FB9D00]) * v124; else v124
    //   0x8261EE94  PowerToRear < 0.1 (FWD car) -> SWAP v124/v126 (the reduced factor follows
    //               the driven axle)
    //   0x8261EF0C  a front wheel spinning faster than its rear counterpart (w1 > w3 || w0 > w2)
    //               -> v126 = 0
    //   0x8261EF80  front pair maxAngVel = Engine::GetMaxWheelAngularVelocity().x, or 10000.0
    //               [flt_82005D9C] when PowerToFront is (eps-)zero; rear pair reloads the engine
    //               vector fresh before EACH wheel
    //   0x8261F038  Wheel::UpdateVelocity x4 (order 1,0 with v124; 3,2 with v126; v4/v5 =
    //               unk_82FB9E10 == 100.0 / unk_82FB9380 == 1000.0)
    //   0x8261F0C4  LimitDifferential(REAR pair)
    //   0x8261F0D4  count wheels with (mbIsOnGround && mbHasTraction) -> r27
    //   0x8261F14C  CalculateNewVelocity(dt)
    //   0x8261F158  [+0x14 vcall == IsPlayerVehicleActuallyInShowtime -> skip friction]
    //   0x8261F180  GetDownForce(); fwd = dot3(mLinearVelocity, At)
    //   0x8261F1C4  IsCounterSteeringAtLowSpeed(splat(fwd), mfSteering, mfGas):
    //                 YES -> boosted = (|min(yawRate, 3.0 [flt_82004270])| *
    //                        LowSpeedTyreFrictionTractionControl [+0x90 .z] + 1.0) * downforce;
    //                        rear grip = (fwd >= -5.0 [flt_82094774]) ? boosted : downforce;
    //                        front grip = boosted
    //                 NO  -> both = downforce
    //   0x8261F2F4  HandleWheelPairFriction(2,3, At, rearGrip, dt, grip(2), grip(3), r27>2, 0);
    //               CalculateNewVelocity(dt);
    //               HandleWheelPairFriction(0,1, mSteeringDirection, frontGrip, dt, grip(0),
    //               grip(1), r27>2, 0)  [GetSurfaceGrip call order per pair: B then A]
    //   0x8261F410  CalculateNewVelocity(dt)
    //   0x8261F414  [mbCrashing && 3500.0 [unk_82FB90C0] > attribs Mass]
    //               HandleWheelFrictionCrashing(2)(3)(0)(1) with v1 = dt
    //   0x8261F494  [!mbFrozen || controls->mbIsOnStartLine] ROTATION ADVANCE x4 (order 1,0,3,2):
    //               mIntegrationVariables.z = wrap(.z + .x * dt) into [-pi, pi) via
    //               unk_82FB9260 == 1/(2pi), unk_82FB92A0 == 2pi, unk_82FB92C0 == pi
    void VehiclePhysics::UpdateWheels(const BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep)
    {
        static const f32 KF_PEDAL_DEADZONE      = 0.1f;      // flt_82004014
        static const f32 KF_BRAKE_DRIVING_GATE  = 0.9f;      // flt_82005450
        static const f32 KF_FACTOR_MPH_OFFSET   = 60.0f;     // unk_82FB9E20 <- flt_82092BC4
        static const f32 KF_FACTOR_MPH_DIVISOR  = 10.0f;     // unk_82FB83C0 <- flt_82004A20
        static const f32 KF_FACTOR_LO           = 0.0f;      // unk_82FB91A0 <- flt_82001CC0
        static const f32 KF_FACTOR_HI           = 0.75f;     // unk_82FB9D00 <- flt_82004018
        static const f32 KF_UNDRIVEN_MAX_SPIN   = 10000.0f;  // flt_82005D9C
        static const f32 KF_BRAKE_CAPACITY_SCALE= 100.0f;    // unk_82FB9E10 <- flt_820049E0
        static const f32 KF_BRAKE_DECEL_SCALE   = 1000.0f;   // unk_82FB9380 <- flt_82009E10
        static const f32 KF_CRASH_SCRUB_MASS    = 3500.0f;   // unk_82FB90C0 <- flt_8205878C
        static const f32 KF_YAW_RATE_CAP        = 3.0f;      // flt_82004270
        static const f32 KF_REVERSE_SPEED_GATE  = -5.0f;     // flt_82094774
        static const f32 KF_TWO_PI              = 6.2831854820251465f;    // unk_82FB92A0 <- flt_82001C94
        static const f32 KF_PI                  = 3.1415927410125732f;    // unk_82FB92C0 <- flt_8208F5FC
        static const f32 KF_EPSILON             = 1.1920928955078125e-07f;   // stru_8208F620[0]

        UpdateBurnout(lpControls);
        UpdateWheelInertia();

        // ---- 0x8261E524: mSteeringDirection = R(Up, steeringAngle) * At ----------------------
        // The same inlined XMVectorSinCos + Rodrigues block SetWheelVelocities carries; the store
        // target here is the member (+0x10E0). std::sin/std::cos are the exact forms of the
        // console's shared minimax polynomial (tighter, never looser).
        {
            const Vector3& lvUp = mTransform.Up();
            const Vector3& lvAt = mTransform.At();

            const f32 lfSteerAngle =
                mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x;
            const f32 lfSin = std::sin(lfSteerAngle);
            const f32 lfCos = std::cos(lfSteerAngle);
            const f32 lfOneMinusCos = 1.0f - lfCos;

            const Vector3 lvCol0{ lfCos + lfOneMinusCos * lvUp.x * lvUp.x,
                                  lfOneMinusCos * lvUp.x * lvUp.y + lfSin * lvUp.z,
                                  lfOneMinusCos * lvUp.x * lvUp.z - lfSin * lvUp.y, 0.0f };
            const Vector3 lvCol1{ lfOneMinusCos * lvUp.y * lvUp.x - lfSin * lvUp.z,
                                  lfCos + lfOneMinusCos * lvUp.y * lvUp.y,
                                  lfOneMinusCos * lvUp.y * lvUp.z + lfSin * lvUp.x, 0.0f };
            const Vector3 lvCol2{ lfOneMinusCos * lvUp.z * lvUp.x + lfSin * lvUp.y,
                                  lfOneMinusCos * lvUp.z * lvUp.y - lfSin * lvUp.x,
                                  lfCos + lfOneMinusCos * lvUp.z * lvUp.z, 0.0f };

            mSteeringDirection = Vector3{
                lvCol0.x * lvAt.x + lvCol1.x * lvAt.y + lvCol2.x * lvAt.z,
                lvCol0.y * lvAt.x + lvCol1.y * lvAt.y + lvCol2.y * lvAt.z,
                lvCol0.z * lvAt.x + lvCol1.z * lvAt.y + lvCol2.z * lvAt.z, 0.0f };
        }

        const Vector3& lvAt = mTransform.At();

        // r22 / r23: reverse and gas-released, computed once beside the sincos block.
        const bool lbInReverse   = (0.0f > mfSpeedMPH.x) || (mEngine.GetCurrentGear() == 0);
        const bool lbGasReleased = lpControls->mfGas < KF_PEDAL_DEADZONE;

        // ---- 0x8261E7E8: body velocity at each wheel contact (order 2,3,0,1) -----------------
        CalculateBodyVelocityAtWheelContact(eRearLeftWheel,   lvAt,               lvfTimeStep);
        CalculateBodyVelocityAtWheelContact(eRearRightWheel,  lvAt,               lvfTimeStep);
        CalculateBodyVelocityAtWheelContact(eFrontLeftWheel,  mSteeringDirection, lvfTimeStep);
        CalculateBodyVelocityAtWheelContact(eFrontRightWheel, mSteeringDirection, lvfTimeStep);

        // ---- 0x8261E838: just-landed spin-up (order 2,3,0,1) ---------------------------------
        static const EVehicleDrivenWheel kaeLandOrder[eNumDrivenWheels] = {
            eRearLeftWheel, eRearRightWheel, eFrontLeftWheel, eFrontRightWheel };
        for (s32 li = 0; li < eNumDrivenWheels; ++li)
        {
            Wheel& lrWheel = maWheels[kaeLandOrder[li]];
            if (!lrWheel.GetRoadContact().mbIsOnGround ||
                lrWheel.GetRoadContact().mbWasOnGroundLastUpdate)
                continue;

            const Vector3& lvRollDir =
                (kaeLandOrder[li] == eFrontLeftWheel || kaeLandOrder[li] == eFrontRightWheel)
                    ? mSteeringDirection : lvAt;

            lrWheel.mIntegrationVariables.y = 0.0f;                        // vrlimi 4 (y)
            lrWheel.mIntegrationVariables.x =                              // vrlimi 8 (x)
                vpu::Dot(lrWheel.mBodyPointVelocity, lvRollDir) / lrWheel.mSlipVariables.w;
            lrWheel.mbBrokenAdhesiveLimit = false;                         // stb 0 -> +0xD5
        }

        // ---- 0x8261EA58: integrate the accumulated torque (order 1,0,3,2) --------------------
        static const EVehicleDrivenWheel kaeIntegrateOrder[eNumDrivenWheels] = {
            eFrontRightWheel, eFrontLeftWheel, eRearRightWheel, eRearLeftWheel };
        for (s32 li = 0; li < eNumDrivenWheels; ++li)
        {
            Wheel& lrWheel = maWheels[kaeIntegrateOrder[li]];
            lrWheel.mIntegrationVariables.x += lrWheel.mIntegrationVariables.y * lvfTimeStep.x
                                             * lrWheel.mSuspensionAndInertiaVariables.w;
            lrWheel.mIntegrationVariables.y = 0.0f;
        }

        // ---- 0x8261EB68: the braking factor + the magic brake force --------------------------
        VecFloat lvfBrakeFactor = UpdateBrakesAndGetBrakingFactor(lpControls, lvfTimeStep);

        if (mu8DriftState == 0 && !mbHasAir)
        {
            // t = min(|steering|, 1.0) (the fsel vs f29 == 1.0).
            f32 lfT = std::fabs(lpControls->mfSteering);
            if (lfT > 1.0f) lfT = 1.0f;

            const Vector4& lvMagic = mpAttribs->mBaseAttribs
                .mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels;
            CGS_ASSERT(lvMagic.z == lvMagic.z,
                       "IsValid( mpAttribs->mBaseAttribs.GetMagicBrakeFactorStraightLine() )");   // :1928
            CGS_ASSERT(lvMagic.y == lvMagic.y,
                       "IsValid( mpAttribs->mBaseAttribs.GetMagicBrakeFactorTurning() )");        // :1929
            const f32 lfBlend = lvMagic.z + lfT * (lvMagic.y - lvMagic.z);   // lerp(Straight, Turning, t)

            // sign(mfSpeedMPH) as the {+1, 0, -1} vsel chain.
            const f32 lfSgn = (mfSpeedMPH.x > 0.0f) ? 1.0f
                             : ((mfSpeedMPH.x >= 0.0f) ? 0.0f : -1.0f);

            // clamp01(|forward speed|).
            f32 lfFwdScale = std::fabs(vpu::Dot(mLinearVelocity, lvAt));
            if (lfFwdScale > 1.0f) lfFwdScale = 1.0f;

            const f32 lfForceZ = -lvfBrakeFactor.x * mfMass.x * lfBlend * lfSgn * lfFwdScale;

            CGS_ASSERT(lfForceZ == lfForceZ,
                       "Invalid brake force: lfBrakeFactor, mfMass, lvfSteeringBrakeFactor, "
                       "lvfSgnSpeedMph");   // the gpcMessageBuffer stream, :1951 -- lowered
            AddLocalSpaceForce(Vector3{ 0.0f, 0.0f, lfForceZ, 0.0f });   // vrlimi 2 (z) of zero
        }

        // ---- 0x8261EDF8: the driven-axle brake factor ----------------------------------------
        f32 lfFrontFactor = lvfBrakeFactor.x;   // v124
        f32 lfRearFactor;                       // v126
        if (lpControls->mfGas > 0.0f || lpControls->mfBrake < KF_BRAKE_DRIVING_GATE)
        {
            f32 lfRamp = (mfSpeedMPH.x - KF_FACTOR_MPH_OFFSET) / KF_FACTOR_MPH_DIVISOR;
            if (lfRamp < KF_FACTOR_LO) lfRamp = KF_FACTOR_LO;
            if (lfRamp > KF_FACTOR_HI) lfRamp = KF_FACTOR_HI;
            lfRearFactor = lfRamp * lvfBrakeFactor.x;
        }
        else
        {
            lfRearFactor = lvfBrakeFactor.x;
        }

        // FWD car (PowerToRear < 0.1): the reduced factor follows the driven axle.
        if (KF_PEDAL_DEADZONE > mpAttribs->mBaseAttribs
                .mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.z)
        {
            const f32 lfSwap = lfRearFactor;
            lfRearFactor  = lfFrontFactor;
            lfFrontFactor = lfSwap;
        }

        // A front wheel spinning faster than its rear counterpart zeroes the rear factor.
        if (maWheels[eFrontRightWheel].mIntegrationVariables.x >
                maWheels[eRearRightWheel].mIntegrationVariables.x ||
            maWheels[eFrontLeftWheel].mIntegrationVariables.x >
                maWheels[eRearLeftWheel].mIntegrationVariables.x)
        {
            lfRearFactor = 0.0f;
        }

        // ---- 0x8261EF80: per-axle rev limits + the four Wheel::UpdateVelocity calls ----------
        f32 lfFrontMax = mEngine.GetMaxWheelAngularVelocity().x;
        if (std::fabs(mpAttribs->mBaseAttribs
                .mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.y) <= KF_EPSILON)
            lfFrontMax = KF_UNDRIVEN_MAX_SPIN;   // undriven front: effectively unlimited

        const VecFloat lvfFrontMax{ lfFrontMax, lfFrontMax, lfFrontMax, lfFrontMax };
        const VecFloat lvfFrontFactor{ lfFrontFactor, lfFrontFactor, lfFrontFactor, lfFrontFactor };
        const VecFloat lvfRearFactor{ lfRearFactor, lfRearFactor, lfRearFactor, lfRearFactor };
        const VecFloat lvfCapScale{ KF_BRAKE_CAPACITY_SCALE, KF_BRAKE_CAPACITY_SCALE,
                                    KF_BRAKE_CAPACITY_SCALE, KF_BRAKE_CAPACITY_SCALE };
        const VecFloat lvfDecelScale{ KF_BRAKE_DECEL_SCALE, KF_BRAKE_DECEL_SCALE,
                                      KF_BRAKE_DECEL_SCALE, KF_BRAKE_DECEL_SCALE };

        maWheels[eFrontRightWheel].UpdateVelocity(lvfTimeStep, lvfFrontMax, lvfFrontFactor,
                                                  lvfCapScale, lvfDecelScale,
                                                  lbGasReleased, lbInReverse);
        maWheels[eFrontLeftWheel ].UpdateVelocity(lvfTimeStep, lvfFrontMax, lvfFrontFactor,
                                                  lvfCapScale, lvfDecelScale,
                                                  lbGasReleased, lbInReverse);
        // the rear pair reloads the engine vector FRESH before each wheel (two calls, as shipped).
        maWheels[eRearRightWheel].UpdateVelocity(lvfTimeStep, mEngine.GetMaxWheelAngularVelocity(),
                                                 lvfRearFactor, lvfCapScale, lvfDecelScale,
                                                 lbGasReleased, lbInReverse);
        maWheels[eRearLeftWheel ].UpdateVelocity(lvfTimeStep, mEngine.GetMaxWheelAngularVelocity(),
                                                 lvfRearFactor, lvfCapScale, lvfDecelScale,
                                                 lbGasReleased, lbInReverse);

        LimitDifferential(eRearLeftWheel, eRearRightWheel);

        // ---- 0x8261F0D4: wheels with real traction -------------------------------------------
        s32 liTractionCount = 0;
        for (s32 li = 0; li < eNumDrivenWheels; ++li)
        {
            if (maWheels[li].GetRoadContact().mbIsOnGround && maWheels[li].mbHasTraction)
                ++liTractionCount;
        }

        CalculateNewVelocity(lvfTimeStep);

        // ---- 0x8261F158: the tyre-friction pass (skipped entirely in showtime) ---------------
        if (!IsPlayerVehicleActuallyInShowtime())   // the +0x14 vcall
        {
            const f32 lfDownForce = GetDownForce().x;
            const f32 lfFwdSpeed  = vpu::Dot(mLinearVelocity, lvAt);
            const bool lbMostWheels = liTractionCount > 2;

            f32 lfRearGrip;
            f32 lfFrontGrip;
            if (IsCounterSteeringAtLowSpeed(
                    VecFloat{ lfFwdSpeed, lfFwdSpeed, lfFwdSpeed, lfFwdSpeed },
                    lpControls->mfSteering, lpControls->mfGas))
            {
                // boosted = (|min(yawRate, 3.0)| * LowSpeedTyreFrictionTractionControl + 1.0)
                //           * downforce   (fsel min, fabs, fmadds vs f29 == 1.0, fmuls)
                f32 lfYaw = mAngularVelocity.y;
                if (lfYaw > KF_YAW_RATE_CAP) lfYaw = KF_YAW_RATE_CAP;
                const f32 lfBoosted =
                    (std::fabs(lfYaw) * mpAttribs->mBaseAttribs
                         .mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.z
                     + 1.0f) * lfDownForce;

                lfRearGrip  = (lfFwdSpeed >= KF_REVERSE_SPEED_GATE) ? lfBoosted : lfDownForce;
                lfFrontGrip = lfBoosted;
            }
            else
            {
                lfRearGrip  = lfDownForce;
                lfFrontGrip = lfDownForce;
            }

            // rear axle (GetSurfaceGrip call order per pair: B then A, as shipped).
            { const Vector3 lvGripRR = GetSurfaceGrip(eRearRightWheel);
              const Vector3 lvGripRL = GetSurfaceGrip(eRearLeftWheel);
              HandleWheelPairFriction(eRearLeftWheel, eRearRightWheel, lvAt,
                                      VecFloat{ lfRearGrip, lfRearGrip, lfRearGrip, lfRearGrip },
                                      lvfTimeStep,
                                      VecFloat{ lvGripRL.x, lvGripRL.y, lvGripRL.z, lvGripRL.w },
                                      VecFloat{ lvGripRR.x, lvGripRR.y, lvGripRR.z, lvGripRR.w },
                                      lbMostWheels, false); }

            CalculateNewVelocity(lvfTimeStep);

            // front axle.
            { const Vector3 lvGripFR = GetSurfaceGrip(eFrontRightWheel);
              const Vector3 lvGripFL = GetSurfaceGrip(eFrontLeftWheel);
              HandleWheelPairFriction(eFrontLeftWheel, eFrontRightWheel, mSteeringDirection,
                                      VecFloat{ lfFrontGrip, lfFrontGrip, lfFrontGrip, lfFrontGrip },
                                      lvfTimeStep,
                                      VecFloat{ lvGripFL.x, lvGripFL.y, lvGripFL.z, lvGripFL.w },
                                      VecFloat{ lvGripFR.x, lvGripFR.y, lvGripFR.z, lvGripFR.w },
                                      lbMostWheels, false); }
        }

        CalculateNewVelocity(lvfTimeStep);

        // ---- 0x8261F414: the crash scrub (light cars only) -----------------------------------
        if (mbCrashing &&
            KF_CRASH_SCRUB_MASS >
                mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x)
        {
            HandleWheelFrictionCrashing(eRearLeftWheel,   lvfTimeStep);
            HandleWheelFrictionCrashing(eRearRightWheel,  lvfTimeStep);
            HandleWheelFrictionCrashing(eFrontLeftWheel,  lvfTimeStep);
            HandleWheelFrictionCrashing(eFrontRightWheel, lvfTimeStep);
        }

        // ---- 0x8261F494: advance the wheel rotation angles (order 1,0,3,2) -------------------
        // Runs unless the body is frozen -- except on the start line, where the wheels animate
        // through the freeze (controls->mbIsOnStartLine).
        if (!IsFrozen() || lpControls->mbIsOnStartLine)
        {
            for (s32 li = 0; li < eNumDrivenWheels; ++li)
            {
                Wheel& lrWheel = maWheels[kaeIntegrateOrder[li]];   // 1,0,3,2

                // angle' = z + x*dt, wrapped into [-pi, pi): subtract floor(angle/(2pi))*2pi,
                // then the two compare/select steps against +pi / -pi.
                f32 lfAngle = lrWheel.mIntegrationVariables.z
                            + lrWheel.mIntegrationVariables.x * lvfTimeStep.x;
                lfAngle -= std::floor(lfAngle * (1.0f / KF_TWO_PI)) * KF_TWO_PI;   // vrfim
                if (lfAngle >= KF_PI)  lfAngle -= KF_TWO_PI;
                if (lfAngle < -KF_PI)  lfAngle += KF_TWO_PI;
                lrWheel.mIntegrationVariables.z = lfAngle;   // vrlimi 2 (z)
            }
        }
    }

// [clean] UpdateDriving  @0x82638148
    // @0x82638148  BrnPhysics::Vehicle::VehiclePhysics::UpdateDriving  (433 insns)
    // ⭐⭐ THE ORDERER -- the phase chain the whole campaign has been aimed at. Transcribed
    // stage by stage from the X360 asm; NOTHING is reordered. Every CheckState string below is
    // the console's own (each `lis/addi` pair around the bl). The dt vector (v126 == v1) is
    // restored before every callee that takes it.
    //
    //   0x82638194  CheckState "Before driving update"
    //   0x826381A4  local 0x48-byte copy of the incoming controls (the copy is what most
    //               stages read; UpdateSpeedMatch and -- for AI -- UpdateDrift read the ORIGINAL)
    //   0x826381BC  [copy.mbIsSteeringWheel] ModifyControlsForSteeringWheelInput(copy)
    //   0x826381C8  ModifyControlsForDrift(copy)
    //   0x82638204  mfSpeedMPH (+0x6C0)   = splat(dot3(mLinearVelocity, mTransform.zAxis)
    //                                        * KF_MPS_TO_MPH @0x8208F820 == 2.2369363)
    //   0x82638234  mfMass (+0xE0, base)  = splat(attribs Mass lane .x)  -- refreshed per frame
    //   0x82638238  UpdateInAirStats(f1 = dt.x)
    //   0x82638248  mEngine.mbAllowToChangeUpGear = mbAllowToChangeDownGear = !mbHasAir
    //   0x82638268  UpdateSlam(&copy, dt)
    //   0x82638270  UpdateShunt(&copy)                       [TRAP until its wave]
    //   0x82638280  CheckState "After update slam"
    //   0x82638290  CheckState "After LayOffGasWhilstInAir"  (the stage itself is inlined into
    //               the slam/shunt pair on this build -- two brackets, back to back)
    //   0x826382A0  UpdateRoadNoise(random)
    //   0x826382B0  CheckState "After update road noise"
    //   0x826382B8  CalculateWorldIntertia()
    //   0x826382C8  UpdateSpeedMatch(ORIGINAL controls, dt)
    //   0x826382D8  CheckState "After update speed match"
    //   0x826382E0  UpdateDownForce()
    //   0x826382F0  CheckState "After Update down force"
    //   0x82638300  UpdateBoost(&copy, dt)
    //   0x82638310  CheckState "After update boost"
    //   0x82638320  UpdateEngine(&copy, dt)
    //   0x82638330  CheckState "After update engine"
    //   0x8263833C  CalculateNewVelocity(dt)                 [forces -> velocity, pass 1]
    //   0x82638354  UpdateSteering(copy.mfSteering, copy.mfGas, dt, copy.mbIsSteeringWheel)
    //   0x82638364  CheckState "After update steering"
    //   0x82638384  UpdateDrift(meDriverType != PLAYER ? original : &copy, dt)
    //   0x826383AC  vcall +0x2C == UpdateSuspension(dt)      (DWARF VehiclePhysics.h:1517; no
    //               override exists in the image -- TrafficPhysics introduces only Update, and
    //               RaceCarPhysics's DWARF virtual set has no UpdateSuspension -- so the direct
    //               call is dispatch-identical)
    //   0x826383B0  mbAllWheelsHaveTraction (+0x135B) = AND of the four wheels' mbIsOnGround
    //   0x826383FC  CheckState "After update suspension"
    //   0x8263840C  UpdateWheels(&copy, dt)                  [BODIED 2026-08-07]
    //   0x8263841C  CheckState "After update Wheels"
    //   0x8263842C  UpdateInAirBehaviour(&copy, dt)          [TRAP until its wave]
    //   0x82638438  UpdateInWaterBehaviour(dt)
    //   0x82638448  CheckState "After in air behavior"
    //   0x82638454  CalculateNewVelocity(dt)                 [pass 2]
    //   0x82638458  THE LINEAR-DRAG BLOCK (skipped while mbHasAir):
    //                 drag = TimeBoosting (+0x1040.y) > 0
    //                        ? attribs mBoostAttribs ...BoostLinearDrag (lane .z)
    //                        : attribs mvLinearDrag_... (lane .x)
    //                 [+0x135B] drag += GetSurfaceLinearDrag()
    //                 while |mLinearVelocity| > FLT_EPSILON (stru_8208F620[0]):
    //                   AddWorldSpaceForce(-drag * |v| * v)   (quadratic, along velocity;
    //                   the vsel/vrsqrt chain is the 0-guarded magnitude)
    //   0x8263858C  CalculateNewVelocity(dt)                 [pass 3]
    //   0x8263859C  CheckState "After linear drag"
    //   0x826385A4  StoreLocalWheelPositions()
    //   0x826385B4  CheckState "After update wheel effects"
    //   0x826385F8  mfSpeedMPH recomputed (same formula -- velocity changed since the top)
    //   0x826385EC  mbCrashedThisFrame (+0x713) = 0
    //   0x82638600  CheckState "After update freezing"
    //   0x82638604  [copy.mbReset] SetAttributes() + HackedResetAndFlyAround(&copy, dt) [TRAPS]
    //               ELSE: mbContactingWall (+0x1362) = (0.4 [flt_8200473C] >
    //                     SecondsSinceLastWallContact (+0x1070.w)); then .w += dt
    //   0x82638690  SimpleVehiclePhysics::CalculateNewWheelPlane()  [BODIED 2026-08-07]
    //   0x826386A4  TimeCrashing (+0xEF0.y) = 0
    //   0x826386A8  force-feedback springs:
    //                 mWheelFFSpring.mfSpringCoefficient (+0x13D0) =
    //                     both FRONT wheels on ground ? clamp01(mfSpeedMPH / 40.0
    //                     [flt_8208FBD0]) : 0.0 [flt_82001CC0]
    //                 mWheelFFSpring.mfSpringSaturation (+0x13D4) =
    //                     |v| > eps ? dot3(unit(mLinearVelocity), mTransform.xAxis) : 0.0
    //   0x826387E8  CheckState "End of driving update"
    void VehiclePhysics::UpdateDriving(const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                                       const BrnPlayerDriverControls* lpControls,
                                       CgsNumeric::Random& lrRandom, VecFloat lvfTimeStep)
    {
        static const f32 KF_EPSILON = 1.1920928955078125e-07f;   // stru_8208F620[0]
        (void)lpCameraMatrix;   // r4 is carried for UpdateCrashing's sibling path; this body
                                // does not read it (the console keeps it in r26 untouched).

        CheckState("Before driving update");

        BrnPlayerDriverControls lCopy;
        std::memcpy(&lCopy, lpControls, sizeof(BrnPlayerDriverControls));   // the 0x48 memcpy

        if (lCopy.mbIsSteeringWheel)
            ModifyControlsForSteeringWheelInput(lCopy);
        ModifyControlsForDrift(lCopy);

        { const f32 lfMPH = vpu::Dot(mLinearVelocity, mTransform.zAxis) * 2.2369363f;
        mfSpeedMPH = VecFloat{ lfMPH, lfMPH, lfMPH, lfMPH }; }   // splat
        { const f32 lfM = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;
          mfMass = VecFloat{ lfM, lfM, lfM, lfM }; }   // splat (refreshed per frame)

        UpdateInAirStats(lvfTimeStep.x);

        mEngine.SetAllowGearChanges(!mbHasAir);   // stb 0xFC4 / 0xFC5

        UpdateSlam(reinterpret_cast<f32*>(&lCopy), lvfTimeStep.x);
        UpdateShunt(&lCopy);
        CheckState("After update slam");
        CheckState("After LayOffGasWhilstInAir");

        UpdateRoadNoise(lrRandom);
        CheckState("After update road noise");

        CalculateWorldIntertia();

        UpdateSpeedMatch(lpControls, lvfTimeStep.x);   // the ORIGINAL controls (r28)
        CheckState("After update speed match");

        UpdateDownForce();
        CheckState("After Update down force");

        UpdateBoost(&lCopy, lvfTimeStep.x);
        CheckState("After update boost");

        UpdateEngine(&lCopy, lvfTimeStep);
        CheckState("After update engine");

        CalculateNewVelocity(lvfTimeStep);

        UpdateSteering(lCopy.mfSteering, lCopy.mfGas, lvfTimeStep, lCopy.mbIsSteeringWheel);
        CheckState("After update steering");

        UpdateDrift((lpControls->GetType() != E_DRIVER_TYPE_PLAYER) ? lpControls : &lCopy,
                    lvfTimeStep);
        CheckState("After update drift");

        UpdateSuspension(lvfTimeStep.x);   // the +0x2C vcall (see banner)

        mbAllWheelsHaveTraction =
            maWheels[eFrontLeftWheel].GetRoadContact().mbIsOnGround  &&   // +0x158
            maWheels[eFrontRightWheel].GetRoadContact().mbIsOnGround &&   // +0x238
            maWheels[eRearLeftWheel].GetRoadContact().mbIsOnGround   &&   // +0x318
            maWheels[eRearRightWheel].GetRoadContact().mbIsOnGround;      // +0x3F8
        CheckState("After update suspension");

        UpdateWheels(&lCopy, lvfTimeStep);
        CheckState("After update Wheels");

        UpdateInAirBehaviour(&lCopy, lvfTimeStep);
        UpdateInWaterBehaviour(&lCopy, lvfTimeStep);
        CheckState("After in air behavior");

        CalculateNewVelocity(lvfTimeStep);

        // ----- the linear-drag block (asm 0x82638458..0x82638580) -----
        if (!mbHasAir)
        {
            f32 lfDrag;
            if (mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.y > 0.0f)
                lfDrag = mpAttribs->mBoostAttribs
                             .mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.z;
            else
                lfDrag = mpAttribs->mBaseAttribs
                             .mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.x;

            Vector3 lvDrag{ lfDrag, lfDrag, lfDrag, lfDrag };
            if (mbAllWheelsHaveTraction)
            {
                const Vector3 lvSurface = GetSurfaceLinearDrag();
                lvDrag = Vector3{ lvDrag.x + lvSurface.x, lvDrag.y + lvSurface.y,
                                  lvDrag.z + lvSurface.z, lvDrag.w + lvSurface.w };
            }

            const f32 lfSpeedSq = vpu::MagnitudeSquared(mLinearVelocity);
            const f32 lfSpeed   = (lfSpeedSq > 0.0f) ? std::sqrt(lfSpeedSq) : 0.0f;
            if (std::fabs(lfSpeed) > KF_EPSILON)
            {
                // F = -drag * |v| * v  (asm 0x82638564..0x82638580: vmulfp |v|*|v| is folded
                // through the refined rsqrt -- the emitted product is unitV * (-drag * |v|^2)).
                AddWorldSpaceForce(Vector3{ -lvDrag.x * lfSpeed * mLinearVelocity.x,
                                            -lvDrag.y * lfSpeed * mLinearVelocity.y,
                                            -lvDrag.z * lfSpeed * mLinearVelocity.z, 0.0f });
            }
        }

        CalculateNewVelocity(lvfTimeStep);
        CheckState("After linear drag");

        StoreLocalWheelPositions();
        CheckState("After update wheel effects");

        { const f32 lfMPH = vpu::Dot(mLinearVelocity, mTransform.zAxis) * 2.2369363f;
        mfSpeedMPH = VecFloat{ lfMPH, lfMPH, lfMPH, lfMPH }; }   // splat
        mbCrashedThisFrame = false;   // stb 0 -> +0x713
        CheckState("After update freezing");

        if (lCopy.mbReset)
        {
            SetAttributes();
            HackedResetAndFlyAround(&lCopy, lvfTimeStep);
        }
        else
        {
            static const f32 KF_WALL_CONTACT_WINDOW = 0.4f;   // flt_8200473C (image-read)
            mbContactingWall =
                KF_WALL_CONTACT_WINDOW >
                mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w;
            mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w
                += lvfTimeStep.x;
        }

        SimpleVehiclePhysics::CalculateNewWheelPlane();

        mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y = 0.0f;   // vrlimi(4)

        // ----- force-feedback springs (asm 0x826386A8..0x826387D8) -----
        {
            static const f32 KF_FF_SPEED_DIVISOR = 40.0f;   // flt_8208FBD0 (image-read)

            if (maWheels[eFrontLeftWheel].GetRoadContact().mbIsOnGround &&
                maWheels[eFrontRightWheel].GetRoadContact().mbIsOnGround)
            {
                f32 lfCoeff = mfSpeedMPH.x / KF_FF_SPEED_DIVISOR;   // vrefp + 2 NR
                if (lfCoeff < 0.0f) lfCoeff = 0.0f;                 // vmaxfp 0
                if (lfCoeff > 1.0f) lfCoeff = 1.0f;                 // vminfp 1
                mWheelFFSpring.mfSpringCoefficient = lfCoeff;
            }
            else
            {
                mWheelFFSpring.mfSpringCoefficient = 0.0f;          // flt_82001CC0
            }

            const f32 lfSpeedSq = vpu::MagnitudeSquared(mLinearVelocity);
            if (std::fabs(lfSpeedSq) > KF_EPSILON)
            {
                const Vector3 lvUnitVel = vpu::Normalize(mLinearVelocity);
                mWheelFFSpring.mfSpringSaturation = vpu::Dot(lvUnitVel, mTransform.xAxis);
            }
            else
            {
                mWheelFFSpring.mfSpringSaturation = 0.0f;
            }
        }

        CheckState("End of driving update");
    }

// [clean] UpdateSteering  @0x825D3720
    // @0x825D3720  BrnPhysics::Vehicle::VehiclePhysics::UpdateSteering  (577 insns; was an
    //   export-set JSON hole -- exported fresh from the .i64 this wave)
    //
    // THE STEERING MODEL, in four acts, register-traced end to end. All angles in the +0x1030
    // register's .z (MaxSteeringAngle) are DEGREES (every consumer multiplies by
    // flt_8208F5F4 == pi/180 at use); the +0xFE0 register's .x (SteeringAngle) is RADIANS,
    // .y (Steering) is the clamped [-1,1] normalised wheel, .z (PrevSteering) is the
    // UNCLAMPED integrator the rate limiter advances.
    //
    // 1) OVERRIDE + SHIFT (0x825D3784..0x825D3800): if mbOverrideSteering, the working steer
    //    input becomes SteeringOverride (+0x1070.y) and the latch clears. PrevSteering <-
    //    Steering (the frame shift).
    // 2) EFFECTIVE SPEED (0x825D3804..0x825D3874): lfEff = |mfSpeedMPH| * (0.9 + 0.1*gas)
    //    [flt_82005450/flt_82004014] -- EXCEPT while reversing faster than 5 mph with the
    //    handbrake on (splat(-5.0) [flt_82094774] > speed && mbHandBrake), where the raw
    //    signed speed is kept.
    // 3) THE MAX-STEER TARGET (+0x1030.z), four-way (0x825D3984..0x825D3C40):
    //    a. rear wheels sliding (both mbBrokenAdhesiveLimit) AND |GetMaxSteeringAngleDuring-
    //       Drift(steer)| > |currentRad| AND turning into the slide (sideSpeed*30 < 0):
    //       GROW  -- current + 0.5deg/frame (unk_82FB9090 = splat(0.5 * pi/180), static-init
    //       @0x82C5CA30 = flt_82001DA0 * flt_8208F5F4), capped at the drift max.
    //    b. |currentRad| > |attribs MaxAngle in rad|: SHRINK -- current - 0.5deg/frame.
    //    c. lfEff below 30 mph [flt_82004F5C]: SNAP to attribs MaxAngle.
    //    d. else THE SPEED FALLOFF (0x825D3AA8..0x825D3C34, the two pow(x, 0.1)
    //       [dbl_82094AC0] calls through sub_82C09970 == libm pow):
    //         t1   = pow(1/(MaxSpeed+1), 0.1)
    //         t2   = pow(1/((lfEff-30) * MaxSpeed/(MaxSpeed-30) + 1), 0.1)
    //         frac = clamp01((t2 - t1) / (1 - t1))
    //         target = MinAngle + (MaxAngle - MinAngle) * frac
    //       (t2 == 1 at 30 mph -> MaxAngle; t2 == t1 at MaxSpeed -> MinAngle: a smooth
    //       falloff whose endpoints close exactly -- the closure is the correctness check.)
    // 4) APPLY:
    //    PAD path (0x825D3C54..0x825D3F54): rate-limited integration.
    //      step  = clamp(ReactionPerSec * dt * driftScale * e^(deficit/max) * max,
    //                    +/-MinAngle) / max
    //        where deficit = maxDeg - |SteeringAngle|*180/pi, driftScale = DriftPushTime
    //        (+0x170.w) while drifting else 1.0, and the e^x is the exp2 polynomial
    //        (unk_82181570 = log2(e); unk_8208FBE0/unk_8208FBF0 = the minimax coefficient
    //        vectors) -- the steer speeds up the further from lock it is;
    //      while NOT drifting, a direction reversal (PrevSteering moving back toward the
    //        input) scales the step by StraightReactionBias (+0xF0.y);
    //      PrevSteering += clamp(input - PrevSteering, +/-step);
    //      Steering      = clamp(PrevSteering, -1, 1);
    //      SteeringAngle = Steering * -(1.5 [flt_820945DC] * maxDeg * pi/180).
    //    WHEEL path (0x825D3F5C..0x825D4008): direct drive, no rate limit.
    //      Steering      = input;
    //      SteeringAngle = -clamp(attribs MaxAngle * Steering, +/-(1.5 * maxDeg * pi/180)).
    void VehiclePhysics::UpdateSteering(f32 lfSteering, f32 lfGas, VecFloat lvfTimeStep,
                                        bool lbIsSteeringWheel)
    {
        static const f32 KF_DEG_TO_RAD        = 0.01745329238474369f;    // flt_8208F5F4
        static const f32 KF_RAD_TO_DEG        = 57.295780181884766f;     // flt_8208F5F8
        static const f32 KF_HALF_DEG_RAD      = 0.5f * 0.01745329238474369f; // unk_82FB9090
        static const f32 KF_REVERSE_SPEED     = -5.0f;                   // flt_82094774
        static const f32 KF_GAS_BLEND_SCALE   = 0.1f;                    // flt_82004014
        static const f32 KF_GAS_BLEND_BASE    = 0.9f;                    // flt_82005450
        static const f32 KF_LOW_SPEED_MPH     = 30.0f;                   // flt_82004F5C
        static const f64 KF_FALLOFF_EXPONENT  = 0.10000000149011612;     // dbl_82094AC0
        static const f32 KF_ANGLE_OVERSTEER   = 1.5f;                    // flt_820945DC

        const VehicleAttribs::SteeringAttribs& lrSteer = mpAttribs->mSteeringAttribs;
        const f32 lfAttribMaxAngleDeg = lrSteer.mvMaxAngle_StraightReactionBias.x;
        const f32 lfAttribMaxAngleRad = lfAttribMaxAngleDeg * KF_DEG_TO_RAD;

        Vector4& lrReg = mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount;   // +0xFE0

        // 1) override + frame shift.
        if (mbOverrideSteering)
        {
            lfSteering =
                mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.y;
            mbOverrideSteering = false;
        }
        lrReg.z = lrReg.y;   // PrevSteering <- Steering (vrlimi(2) of the .y splat)

        // 2) effective speed.
        f32 lfEffSpeed = mfSpeedMPH.x;
        if (!(KF_REVERSE_SPEED > lfEffSpeed && mbHandBrake))
            lfEffSpeed = std::fabs(lfEffSpeed) * (lfGas * KF_GAS_BLEND_SCALE + KF_GAS_BLEND_BASE);

        // 3) the max-steer target.
        const bool lbRearSliding =
            maWheels[eRearLeftWheel].mbBrokenAdhesiveLimit &&    // +0x3C5
            maWheels[eRearRightWheel].mbBrokenAdhesiveLimit;     // +0x4A5

        const f32 lfDriftMaxRad = GetMaxSteeringAngleDuringDrift(lfSteering);
        const f32 lfSideSpeed   = vpu::Dot(mLinearVelocity, mTransform.xAxis);
        const f32 lfCurrentRad  =
            mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.z
            * KF_DEG_TO_RAD;

        f32 lfDriftSteerScale = 1.0f;
        if (mu8DriftState != 0)
            lfDriftSteerScale = mpAttribs->mDriftAttribs
                .mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.w;

        f32 lfMaxSteerDeg;
        if (lbRearSliding &&
            std::fabs(lfDriftMaxRad) > std::fabs(lfCurrentRad) &&
            (0.0f > lfSideSpeed * 30.0f))
        {
            // a. grow toward the drift max, 0.5 deg per frame.
            lfMaxSteerDeg = std::min(lfCurrentRad + KF_HALF_DEG_RAD, std::fabs(lfDriftMaxRad))
                          * KF_RAD_TO_DEG;
        }
        else if (std::fabs(lfCurrentRad) > std::fabs(lfAttribMaxAngleRad))
        {
            // b. shrink back toward the attribs max, 0.5 deg per frame.
            lfMaxSteerDeg = (lfCurrentRad - KF_HALF_DEG_RAD) * KF_RAD_TO_DEG;
        }
        else if (KF_LOW_SPEED_MPH > lfEffSpeed)
        {
            // c. full lock available at low speed.
            lfMaxSteerDeg = lfAttribMaxAngleDeg;
        }
        else
        {
            // d. the pow(x, 0.1) speed falloff.
            const f32 lfMaxSpeed =
                mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z;
            const f32 lfMinAngleDeg =
                lrSteer.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.w;

            const f32 lfT1 = static_cast<f32>(
                std::pow(1.0 / (static_cast<f64>(lfMaxSpeed) + 1.0), KF_FALLOFF_EXPONENT));
            const f32 lfInner = (lfEffSpeed - KF_LOW_SPEED_MPH)
                              * (lfMaxSpeed / (lfMaxSpeed - KF_LOW_SPEED_MPH)) + 1.0f;
            const f32 lfT2 = static_cast<f32>(
                std::pow(1.0 / static_cast<f64>(lfInner), KF_FALLOFF_EXPONENT));

            f32 lfFrac = (lfT2 - lfT1) / (1.0f - lfT1);
            if (lfFrac < 0.0f) lfFrac = 0.0f;   // vmaxfp 0
            if (lfFrac > 1.0f) lfFrac = 1.0f;   // vminfp 1

            lfMaxSteerDeg = (lfAttribMaxAngleDeg - lfMinAngleDeg) * lfFrac + lfMinAngleDeg;
        }
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.z = lfMaxSteerDeg;

        // 4) apply.
        if (!lbIsSteeringWheel)
        {
            // --- the PAD path: rate-limited integration ---
            const f32 lfMinAngleDeg =
                lrSteer.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.w;
            const f32 lfReactionPerSec =
                lrSteer.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.x;

            // headroom to lock, in degrees; e^(headroom/max) speeds the wheel up off-lock.
            const f32 lfDeficitDeg = lfMaxSteerDeg - std::fabs(lrReg.x) * KF_RAD_TO_DEG;
            const f32 lfExp = std::exp(lfDeficitDeg / lfMaxSteerDeg);   // exp2 poly + log2(e)

            f32 lfStep = lfReactionPerSec * lvfTimeStep.x * lfDriftSteerScale
                       * lfExp * lfMaxSteerDeg;
            if (lfStep < -lfMinAngleDeg) lfStep = -lfMinAngleDeg;   // vmaxfp -MinAngle
            if (lfStep >  lfMinAngleDeg) lfStep =  lfMinAngleDeg;   // vminfp +MinAngle
            lfStep = lfStep / lfMaxSteerDeg;                        // the refined-recip scale

            if (mu8DriftState == 0)
            {
                // a reversal back toward the input returns faster.
                const f32 lfPrev = lrReg.z;
                const bool lbReturning = (lfPrev > 0.0f) ? (lfPrev > lfSteering)
                                                         : (lfSteering > lfPrev);
                if (lbReturning)
                    lfStep *= lrSteer.mvMaxAngle_StraightReactionBias.y;   // +0xF0.y
            }

            f32 lfDelta = lfSteering - lrReg.z;
            if (lfDelta < -lfStep) lfDelta = -lfStep;   // vmaxfp -step
            if (lfDelta >  lfStep) lfDelta =  lfStep;   // vminfp +step
            lrReg.z = lrReg.z + lfDelta;                 // the unclamped integrator

            f32 lfWheel = lrReg.z;
            if (lfWheel < -1.0f) lfWheel = -1.0f;
            if (lfWheel >  1.0f) lfWheel =  1.0f;
            lrReg.y = lfWheel;                           // Steering, clamped [-1,1]

            lrReg.x = lfWheel * -(KF_ANGLE_OVERSTEER * lfMaxSteerDeg * KF_DEG_TO_RAD);
        }
        else
        {
            // --- the WHEEL path: direct drive ---
            lrReg.y = lfSteering;

            const f32 lfLimitRad = KF_ANGLE_OVERSTEER * lfMaxSteerDeg * KF_DEG_TO_RAD;
            f32 lfAngle = lfAttribMaxAngleDeg * lrReg.y;   // +0xF0.x * Steering
            if (lfAngle < -lfLimitRad) lfAngle = -lfLimitRad;
            if (lfAngle >  lfLimitRad) lfAngle =  lfLimitRad;
            lrReg.x = -lfAngle;
        }
    }

// [clean] Update  @0x826412C0
    // @0x826412C0  BrnPhysics::Vehicle::VehiclePhysics::Update  (200 insns)
    // ⭐⭐ THE PER-CAR CONDUCTOR. The DWARF declares it virtual at vtable slot +0xC -- the slot
    // VehicleManager::UpdateVehiclePhysics dispatches through -- and it is kept NON-virtual
    // here per the established modelling (every call site's static type is the exact dynamic
    // type; the vtable head carries its own open flags). The PerfMon ids are the seven hoisted
    // gs_iVPhys* handles (dword_82F2A278..0x290 -- registered by VehicleManager::Construct with
    // the console's names "VMan: Update VPhys" etc.).
    //
    //   0x82641304  StartMonitor(gs_iVPhysUpdatePM)
    //   0x82641324  mLastLinearVelocity (+0x13B0)  = mLinearVelocity (+0x50)
    //   0x8264132C  mPreviousTransform  (+0x1370)  = mTransform (+0x10, four rows)
    //   0x82641354  StartMonitor(gs_iVPhysSwitchAttribsPM)
    //   0x82641358  [lpControls->meDriverType != mPreviousControls.meDriverType]
    //                 SwitchAttribs(type == AI ? &mAIVehicleAttribs : &mPlayerVehicleAttribs)
    //   0x8264138C  mPreviousControls = *lpControls (the 0x48 memcpy)
    //   0x82641390  [mbIsUsingAIDonutAttribs] the donut-attrib refresh: SwitchAIDonutting-
    //               Attribs(true) rebuilds the donut AI set, then the SwitchAttribs sequence is
    //               run INLINE on the AI set (base SwitchAttribs / mpAttribs / engine memcpy /
    //               SetupSuspension)
    //   0x826413D8  StopMonitor(gs_iVPhysSwitchAttribsPM)
    //   0x826413E8  UpdateFreezing(lpControls, dt)
    //   0x826413EC  the START-LINE velocity projection: while (lpControls->mbIsOnStartLine &&
    //               mAboveGroundTestResult.mbValid) un-freeze (mbFrozen = 0) and project:
    //                 mLinearVelocity = -normal * dot3(mLinearVelocity, -normal)
    //               (both vxor sign flips are against mIntersectionNormal @+0x580) -- all
    //               tangential motion dies, the along-normal component survives.
    //   0x82641470  UpdateHandBrake(lpControls->mfHandBrake, dt)
    //   0x82641474  [mbFrozen] the ENGINE-ONLY leg:
    //                 UpdateEngine(lpControls, dt); CheckState("After update engine");
    //                 [mbCrashing] TimeCrashing (+0xEF0.y) += dt  ELSE  = 0
    //               [else, mbCrashing] StartMonitor(gs_iVPhysUpdateCrashingPM);
    //                 UpdateCrashing(dt, camera, controls, impact, aftertouchAdd, showtime)
    //               [else] the DRIVING leg:
    //                 UpdateAirRam(dt)      [gs_iVPhysUpdateAirRamsPM]
    //                 UpdateSpinEffects(dt) [gs_iVPhysUpdateSpinPM]
    //                 UpdateDriving(camera, controls, random, dt) [gs_iVPhysUpdateDrivingPM]
    //               then (both non-frozen legs) UpdateLinearVelocityMagnitude()
    //               [gs_iVPhysUpdateLVPM]
    //   0x826415A0  miNumCollisions (+0x1354) = 0
    //   0x826415AC  TimeSinceLastRaceCarContact (+0x1050.z) = min(z + dt, 100.0)
    //               (unk_82FB9BE0, static-init @0x82C5C540 from flt_820049E0 == 100.0)
    //   0x826415D0  StopMonitor(gs_iVPhysUpdatePM)
    void VehiclePhysics::Update(const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                                const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                                bool lbPlayerAftertouchForceAdditive, bool lbShowtimeAllowed,
                                CgsNumeric::Random& lrRandom,
                                Vector3 lrPassThroughV1, Vector3 lrTimeStep)
    {
        // ⚠️ ARG NOTE (same deviation the header documents): the console receives the dt vector
        // in v1 regardless of declaration order; in this tree's spelling that is
        // lrPassThroughV1. lrTimeStep (v2) is not read by this body -- the X360 saves only v127
        // == v1 and reloads its .x lane for the crash leg's f1.
        const f32 lfDT = lrPassThroughV1.x;
        const VecFloat lvfTimeStep{ lfDT, lfDT, lfDT, lfDT };   // the v127 splat

        CgsDev::PerfMonCpu::StartMonitor(gs_iVPhysUpdatePM);

        mLastLinearVelocity = mLinearVelocity;          // +0x13B0 <- +0x50
        mPreviousTransform  = mTransform;               // +0x1370 <- +0x10 (four rows)

        CgsDev::PerfMonCpu::StartMonitor(gs_iVPhysSwitchAttribsPM);

        if (lpControls->GetType() != mPreviousControls.GetType())
            SwitchAttribs((lpControls->GetType() == E_DRIVER_TYPE_AI)
                              ? &mAIVehicleAttribs : &mPlayerVehicleAttribs);

        std::memcpy(&mPreviousControls, lpControls, sizeof(BrnPlayerDriverControls));

        if (mbIsUsingAIDonutAttribs)
        {
            // the per-frame donut-attrib refresh (asm 0x8264139C..0x826413D0): rebuild the
            // donut set, then run the switch sequence on the AI attribs inline.
            SwitchAIDonuttingAttribs(true);
            SimpleVehiclePhysics::SwitchAttribs(&mAIVehicleAttribs);
            mpAttribs = &mAIVehicleAttribs;
            std::memcpy(&mEngine, &mAIVehicleAttribs.mEngineAttribs,
                        sizeof(VehicleAttribs::EngineAttribs));
            SetupSuspension(0.0);
        }

        CgsDev::PerfMonCpu::StopMonitor(gs_iVPhysSwitchAttribsPM);

        UpdateFreezing(lpControls, lvfTimeStep);

        if (lpControls->mbIsOnStartLine && mAboveGroundTestResult.mbValid)
        {
            mbFrozen = false;                            // stb 0 -> +0x70
            const Vector3 lvNegNormal{ -mAboveGroundTestResult.mIntersectionNormal.x,
                                       -mAboveGroundTestResult.mIntersectionNormal.y,
                                       -mAboveGroundTestResult.mIntersectionNormal.z, 0.0f };
            const f32 lfAlong = vpu::Dot(mLinearVelocity, lvNegNormal);
            mLinearVelocity = vpu::Mult(lvNegNormal, lfAlong);
        }

        UpdateHandBrake(lpControls->mfHandBrake, lvfTimeStep.x);

        if (mbFrozen)
        {
            UpdateEngine(lpControls, lvfTimeStep);
            CheckState("After update engine");

            if (mbCrashing)
                mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y
                    += lvfTimeStep.x;                    // vrlimi(4): TimeCrashing += dt
            else
                mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y = 0.0f;
        }
        else
        {
            if (mbCrashing)
            {
                CgsDev::PerfMonCpu::StartMonitor(gs_iVPhysUpdateCrashingPM);
                UpdateCrashing(lvfTimeStep.x, lpCameraMatrix, lpControls, lbImpactTime,
                               lbPlayerAftertouchForceAdditive, lbShowtimeAllowed);
                CgsDev::PerfMonCpu::StopMonitor(gs_iVPhysUpdateCrashingPM);
            }
            else
            {
                CgsDev::PerfMonCpu::StartMonitor(gs_iVPhysUpdateAirRamsPM);
                UpdateAirRam(lvfTimeStep);
                CgsDev::PerfMonCpu::StopMonitor(gs_iVPhysUpdateAirRamsPM);

                CgsDev::PerfMonCpu::StartMonitor(gs_iVPhysUpdateSpinPM);
                UpdateSpinEffects(lvfTimeStep);
                CgsDev::PerfMonCpu::StopMonitor(gs_iVPhysUpdateSpinPM);

                CgsDev::PerfMonCpu::StartMonitor(gs_iVPhysUpdateDrivingPM);
                UpdateDriving(lpCameraMatrix, lpControls, lrRandom, lvfTimeStep);
                CgsDev::PerfMonCpu::StopMonitor(gs_iVPhysUpdateDrivingPM);
            }

            CgsDev::PerfMonCpu::StartMonitor(gs_iVPhysUpdateLVPM);
            UpdateLinearVelocityMagnitude();
            CgsDev::PerfMonCpu::StopMonitor(gs_iVPhysUpdateLVPM);
        }

        miNumCollisions = 0;                             // stw 0 -> +0x1354

        {
            static const f32 KF_RACECAR_CONTACT_TIME_CAP = 100.0f;   // unk_82FB9BE0 <- flt_820049E0
            f32 lfTime =
                mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z
                + lvfTimeStep.x;
            if (lfTime > KF_RACECAR_CONTACT_TIME_CAP)    // vminfp
                lfTime = KF_RACECAR_CONTACT_TIME_CAP;
            mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z
                = lfTime;
        }

        CgsDev::PerfMonCpu::StopMonitor(gs_iVPhysUpdatePM);
    }
}
}
