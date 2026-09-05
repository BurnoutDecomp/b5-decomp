#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"  // the seat bring-up leg reads the RESIDENT spec (WheelSpecs, mMeshOffset)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"  // BrnPlayerDriverControls (C07 boost/speed-match)
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"  // KVF_HANDBRAKE_OFF_TIME_TO_ALLOW_DRIFT (CheckForEnteringDrift)
#include "GameShared/GameClasses/Numeric/CgsRandom.h" // CgsNumeric::Random (the shared LCG ring)
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT (UpdateDownForce's attribsys guard)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint ([tyre] bring-up probe only)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu::Start/StopMonitor (Update's stage brackets)
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"        // the car-asset wrapper (SetAttributes' key chase)
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h" // the handling wrapper + its checked copy ctor
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerPerfMonHandles.h" // the seven gs_iVPhys* monitor ids (hoisted, orchestrator wave)
#include "rw/math/vpu/vector3_operation.h"            // rw::math::vpu::{MagnitudeSquared, Normalize, Dot, operator*}
#include "rw/math/vpu/vector4_operation.h"            // VecFloat broadcast arithmetic used by UpdateInAirBehaviour
#include "rw/math/vpu/matrix44affine_operation.h"     // rw::math::vpu::{InverseOfMatrixWithOrthonormal3x3, operator*}
#include "rw/math/fpu/scalar_operation.h"            // rw::math::fpu::IsZero (SetWheelVelocities' per-axle power gates)

#include <cstdlib>    // getenv ([tyre] bring-up probe only)
#include <algorithm>  // std::min / std::max (the driving spine's vmaxfp/vminfp lowerings)
#include <cstring>    // std::memcpy (controls/engine state copies)
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

    // DecFIGS VehiclePhysics.cpp:103; Breaker unk_82FB9EC0, initialised by the
    // unmarked thunk at 0x82C5C3C0 from flt_8200473C (0.4f).
    VecFloat KF_SHOWTIME_GRIP_SCALE{0.4f, 0.4f, 0.4f, 0.4f};
    bool VehiclePhysics::msbInShowtime = false;
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
    f32 VehiclePhysics::GetCarGroundDistanceCheck()
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
    VecFloat VehiclePhysics::GetDownForce()
    {
        static const f32 KF_AERO_RHO = 6.1f;     // kAero_Rho_Scalar @0x820948D4 (0x40C33333) -> g_vAero_Rho
        static const f32 KF_AERO_CDA = 1.225f;   // kAero_CdA_Scalar @0x820948D0 (0x3F9CCCCD) -> g_vAero_CdA
        static const f32 KF_HALF     = 0.5f;   // vcfsx v0=1, scale 1 -> 0.5

        const f32 lfSpeedSquared = vpu::MagnitudeSquared(mLinearVelocity);
        const f32 lfCoeff        = mpAttribs->mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.w;   // .w lane of the aero params register

        const f32 lfDownForce = KF_HALF * KF_AERO_RHO * KF_AERO_CDA * lfSpeedSquared * lfCoeff;

        return VecFloat{ lfDownForce, lfDownForce, lfDownForce, lfDownForce };
    }

    // ---------------------------------------------------------------------------------------
    // Surface-response group: GetSurfaceGrip / GetSurfaceRoughness / GetSurfaceLinearDrag
    //   @0x825D51B8 / @0x825D5328 / @0x825D50A8. Each derives a 6-bit surface id from a
    //   RoadContact CollisionTag and looks up a global per-surface property table, blended with a
    //   lane of mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.
    //   The per-surface grip/roughness/linear-drag tables are populated by
    //   VehicleManager::ReadSurfaceProperties @0x825C7BB8 from the generated surface attributes.
    //   The debug "properties loaded" and surface-id-bound asserts are retained below.
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

    // @0x825D51B8  GetSurfaceGrip: result = 1 - (1 - gripTable[id]) * blend  (a lerp toward 1.0).
    VecFloat VehiclePhysics::GetSurfaceGrip(EVehicleDrivenWheel leWheel) const
    {
        CGS_ASSERT(gbReadSurfaceProperties,
                   "Trying to read surface grip before properties have been loaded");
        const s32 liSurfaceId = SurfaceIdFromTag(GetWheel(leWheel).GetRoadContact().mCollisionTag);
        CGS_ASSERT(liSurfaceId < KI_NUM_USED_SURFACES,
                   "static_cast<int32_t>( luSurfaceId ) < KI_NUM_USED_SURFACES");
        const f32 lfGrip = KAVF_SURFACE_GRIP[liSurfaceId].x;

        // FRONT wheels (index < eRearLeftWheel) use lane .x, REAR wheels lane .y (asm: if a3<2).
        const f32 lfBlend = (leWheel < eRearLeftWheel) ? mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.x
                                                       : mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.y;
        f32 lfResult = 1.0f - (1.0f - lfGrip) * lfBlend;
        if (msbInShowtime)
            lfResult *= KF_SHOWTIME_GRIP_SCALE.x;
        return VecFloat{ lfResult, lfResult, lfResult, lfResult };
    }

    // @0x825D5328  GetSurfaceRoughness = roughTable[id] * globalRoughScale * blend.z.
    VecFloat VehiclePhysics::GetSurfaceRoughness(EVehicleDrivenWheel leWheel) const
    {
        CGS_ASSERT(gbReadSurfaceProperties,
                   "Trying to read surface roughness before properties have been loaded");
        const s32 liSurfaceId = SurfaceIdFromTag(GetWheel(leWheel).GetRoadContact().mCollisionTag);
        CGS_ASSERT(liSurfaceId < KI_NUM_USED_SURFACES,
                   "static_cast<int32_t>( luSurfaceId ) < KI_NUM_USED_SURFACES");
        const f32 lfRoughness = KAVF_SURFACE_ROUGHNESS[liSurfaceId].x;
        // unk_82FB9220 <- flt_82004744 = 0.2f (static-init splat @0x82C5A498).
        static const f32 KF_GLOBAL_ROUGHNESS_SCALE = 0.2f;                   // unk_82FB9220

        const f32 lfResult = lfRoughness * KF_GLOBAL_ROUGHNESS_SCALE * mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.z;
        return VecFloat{ lfResult, lfResult, lfResult, lfResult };
    }

    // @0x825D50A8  GetSurfaceLinearDrag = dragTable[id] * blend.w  (single representative contact).
    VecFloat VehiclePhysics::GetSurfaceLinearDrag() const
    {
        CGS_ASSERT(gbReadSurfaceProperties,
                   "Trying to read surface linear drag before properties have been loaded");
        // +0x596 is the BE-LOW halfword of mAboveGroundTestResult.mCollisionTag; taken by shift
        // (never by byte offset -- the halves swap position on a little-endian host).
        const s32 liSurfaceId = static_cast<s32>((GetAboveGroundTagLo() >> 4) & 0x3Fu);
        CGS_ASSERT(liSurfaceId < KI_NUM_USED_SURFACES,
                   "static_cast<int32_t>( luSurfaceId ) < KI_NUM_USED_SURFACES");
        const f32 lfDrag = KAVF_SURFACE_LINEAR_DRAG[liSurfaceId].x;

        const f32 lfResult = lfDrag * mpAttribs->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.w;
        return VecFloat{ lfResult, lfResult, lfResult, lfResult };
    }

    // =======================================================================================
    // Surface-grip/drag/friction group (C05): UpdateRoadNoise + the two tyre-friction solvers.
    // =======================================================================================

    // @0x825F6980  BrnPhysics::Vehicle::VehiclePhysics::UpdateRoadNoise
    //   ABI: r3 = this; r4 = &Random; the leading VecFloat timestep arrives in v1 and is dead.
    //   r31 = this+0x130 (maWheels[0]); stride 0xE0; four wheels.
    //   Pre-loop: lfPre = RandomFloat() * 0.5                           [f28]
    //   Per grounded wheel (RoadContact.mbIsOnGround @ wheel+0x28):
    //     lfWheel = RandomFloat() * 0.5 + lfPre                         [in [0,1)]
    //     lvRough = GetSurfaceRoughness(wheel)
    //     lfMaxSpeed = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z
    //     lfSpeed = mfSpeedMPH.x (+0x6C0)
    //     0x825F6A8C..0x825F6AB0 splats attribs+0x70 lane 2 (DecFIGS GetMaxSpeed) and refines
    //     its reciprocal. The tail
    //     multiplies the speed-scaled ratio by roughness AFTER the 1.0 clamp:
    //     `vmulfp128 v0,v0,v124(speed) ; vminfp128 v0,v0,v127(1.0) ; vmulfp128 v0,v13(roughness),v0`
    //     -- noise = lfWheel * roughness * min(speed / maxSpeed, 1.0).
    //   Destination is exact: 0x825F6AE0/0x825F6B30 load mNormal/mPosition, 0x825F6B80 forms
    //   position + normal * noise, and 0x825F6B84 stores the complete vector to wheel+0x00.
    void VehiclePhysics::UpdateRoadNoise(VecFloat /*lvfTimeStep*/, CgsNumeric::Random& lrRandom)
    {
        const f32 lfWheelDiff = 0.5f;   // flt_82001DA0; DecFIGS local name

        // One pre-loop draw shared across all wheels (matches the single pre-loop LCG step in the asm).
        const f32 lfLCGWheelLimitedRandom = lrRandom.RandomFloat() * lfWheelDiff;

        for (u32 luLoop = 0; luLoop < static_cast<u32>(eNumDrivenWheels); ++luLoop)
        {
            const EVehicleDrivenWheel leWheel = static_cast<EVehicleDrivenWheel>(luLoop);
            Wheel& lrWheel = maWheels[leWheel];

            // Only grounded wheels rumble (lbz r11,0x28(r31) ; beq skip).
            if (!lrWheel.GetRoadContact().mbIsOnGround)
                continue;

            // Per-wheel draw, combined with the shared pre-draw -> noise in [0, 1).
            const f32 lfRandom = lrRandom.RandomFloat() * lfWheelDiff + lfLCGWheelLimitedRandom;

            const f32 lfMaxSpeed =
                mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z;
            const f32 lfSpeedRatio = std::min(mfSpeedMPH.x / lfMaxSpeed, 1.0f);
            const f32 lfRoughness = GetSurfaceRoughness(leWheel).x;
            // Breaker first multiplies roughness by the clamped speed ratio (0x825F6B24), then
            // multiplies that product by lfRandom (0x825F6B40).
            const f32 lfRoadNoiseScale = lfRoughness * lfSpeedRatio;
            const f32 lfNoise = lfRandom * lfRoadNoiseScale;

            const Vector3 lRoadNoise = lrWheel.GetRoadContact().mNormal * lfNoise;
            lrWheel.SetRoadContactPosition(lrWheel.GetRoadContact().mPosition + lRoadNoise);
        }
    }

    // ===========================================================================================
    //  @0x825FB458  BrnPhysics::Vehicle::VehiclePhysics::HandleWheelPairFriction  (1141 instrs)
    // ===========================================================================================
    // THE TYRE FORCES -- the function that turns wheel rotation into force against the road.
    // Deferred for twenty-two waves as "FIDELITY: BLOCKED", on two stated grounds: (1) the X360
    // export is a DEGENERATE VMX128 routine ("local variable allocation has failed", args render as
    // a1..a19, the vector registers are invisible in the pseudocode), and (2) VMX lane order is not
    // SSE lane order, so the BPR twin's shuffles could not be transcribed. Both grounds are now
    // DISCHARGED, from the image:
    //
    //   * The pseudocode is degenerate; the ASSEMBLY is not. All 1141 instructions were read.
    //   * The BPR twin `RoadVehiclePhysics::UpdateWheels @0xBA1420 -> sub_B9BD60` matches this
    //     function STEP FOR STEP -- every rsqrt+Newton, every cross product, the friction cone, the
    //     grip-curve evaluation, the write-back, the r x F torque. It is used ONLY as an algorithm
    //     oracle and a second witness: BPR is the diverged sibling RoadVehiclePhysics and NOT ONE
    //     of its offsets is used here. Every offset below is read out of THIS function's own X360
    //     asm and named against the committed DWARF members.
    //   * The lane question is answered, not assumed, and then made moot. Answered: stage 1 packs
    //     the pair with `vmrghw <A>,<B>` (lane0 = wheel A, lane1 = wheel B) everywhere; stage 2
    //     re-packs to {A-long, A-lat, B-long, B-lat} for the four grip-curve evaluations (the
    //     `vrlimi128 ..,3,2` / `..,0xC,2` pair on the two curves' vmrghw/vmrglw, and the slip
    //     vector `vmrghw v6, longSlip, -latSpeed` matches that packing exactly); stage 3 un-packs
    //     with `vpermwi128 ..,0x27` = {longA,longB} and `..,0x72` = {latA,latB}; the write-back
    //     splats lane 0 for A and lane 1 for B. BPR's `_mm_shuffle_epi32(v85,216)` and `(v85,141)`
    //     are the SAME two permutations, an independent witness on the one thing that mattered.
    //     Made moot: both lanes run the IDENTICAL computation on per-wheel data with no cross-lane
    //     term, so this is written per wheel and the lane order stops being load-bearing. Nothing
    //     here is a transcribed shuffle.
    //
    // ARGUMENT REGISTERS (settled by semantics, not by convention): the vector arguments start at
    // v1, not v2. `vspltw v7,v1,0 / v6,v1,1 / v5,v1,2` pulls x/y/z out of v1 for the cross product,
    // so v1 is the Vector3 roll direction; and the two adhesive-limit multiplies are
    // `vmulfp128 v0,v0(A.adhesiveLimit),v126` and `vmulfp128 v13,v13(B.adhesiveLimit),v125`, so
    // v126 <- v4 is lvfSurfaceGripA and v125 <- v5 is lvfSurfaceGripB, in DWARF order. That fixes
    // v2 = lvfDownForce and v3 = lvfTimeStep, and it agrees with UpdateWheels' own note that
    // HandleWheelFrictionCrashing takes dt in v1. r4/r5 = the wheel indices, r6 =
    // lbMostWheelsHaveTraction, r7 = lbUnusedFalse.
    //
    // OFFSETS (this function's asm; `mulli r11,r25,0xE0` + `addi r31,r27,0x130` = maWheels @+0x130,
    // stride 0xE0): wheel +0x00 RoadContact.mPosition, +0x10 .mNormal, +0x2A .mbIsCloseToGround,
    // +0x30 mIntegrationVariables, +0x40 mSlipVariables, +0x50 mForceVariables, +0x60
    // mSuspensionAndInertiaVariables, +0x70 mSpeedAndMassOnWheelVariables, +0x90
    // mStreamedPositionPlusTwistAmount, +0xB0 mWheelLongDirection, +0xC0 mWheelLatDirection,
    // +0xD0 mpTireAttribs, +0xD5 mbBrokenAdhesiveLimit, +0xD6 mbHasTraction, +0xD7 mu8State.
    // this+0x710 mbCrashing, this+0x720 mpAttribs, this+0x1352 mu8DriftState, this+0xFD0
    // mvfWheelFrictionLinearMultiplier, this+0x10 mTransform (rows +0x10/+0x20/+0x30, pos +0x40),
    // this+0x50 mLinearVelocity, this+0x60 mAngularVelocity, attribs+0x270
    // mBodyRollAttribs.mvWheelLongForceHeightOffset_WheelLatForceHeightOffset.
    //
    // TWO CORRECTIONS to the skeleton this replaces, both from the asm:
    //   * the linear residual accumulates into **mTotalLinearForce (this+0xF0)**, not "+0x240";
    //   * the drift/normal grip-curve selector at this+0x1352 is **mu8DriftState**, a member this
    //     tree already owned (BPR reads the same flag at its own `*(a1+4768)`).
    //
    // CONSTANTS -- every one that enters a FORCE is homed, and none is guessed:
    //   * unk_82FB9160 = KF_GRAVITY 9.81000042 (BrnVehicleConstants.h; the .bss splat of
    //     flt_8208F83C, image-read by an earlier wave, and the SAME slot UpdateSuspensionSprings
    //     uses for the spring stiffness -- so the load term here is literally m*g*cos(slope)).
    //   * unk_8327F240 = the shared {FALSE, TRUE} vsel mask pair (Wheel.cpp) -- a boolean select,
    //     no value needed. unk_82FB8100 / unk_82FB80D0 are the lane-0 / lane-1 clear masks, and
    //     unk_82CDA3F0 / unk_82CDADC0 are the "gather lane 0 / lane 1" permute controls: all four
    //     are pure data routing that disappears in the per-wheel form.
    //   * unk_82FBA1E0 <- flt_82013A78 (0.85, already homed) is INITIALISED here behind
    //     `dword_82FBA1F0` bit 0 and then never read by this function -- the old skeleton's
    //     "~0.85 linear-force cap" is a lazy cache for some OTHER consumer, and applying it here
    //     would have been a fabricated clamp. Not emitted.
    // * THE ONE GATE: unk_82FB9BF0 / unk_82FB9150 / unk_82FB83F0 are .bss slots referenced by
    //     NO other function in the 30,084-entry X360 export set, so their static-init writers are
    //     not recoverable here. They feed EXACTLY ONE store -- mSlipVariables.z, which
    //     UpdateRaceCarState copies to WheelLite::mfSkidFactor. That is a REPORTED value (skid FX
    //     and audio), not a force: no torque, no linear force and no wheel speed depends on it.
    //     The shape is known and written out below; the lane is left untouched rather than emitted
    //     with three unknown numbers. This is the only thing in this function that is gated.
    //
    // THE MODEL, per wheel (all of it verified twice -- X360 asm and the BPR twin):
    //   1. contact frame: n = normalize(RoadContact.mNormal);
    //      lat = normalize(n x rollDirection); lng = normalize(lat x n).   (Gram-Schmidt)
    //   2. contact velocity: v = mLinearVelocity + mAngularVelocity x (contactPos - mTransform.Pos()),
    //      recomputed here rather than read from mBodyPointVelocity.
    //   3. slip: wheelSurfaceSpeed = omega * radius;
    //      longSlip = (wheelSurfaceSpeed - dot(v,lng)) / max(|dot(v,lng)|, 1).
    //   4. load: N = max(massOnWheel * g * mNormal.y, 0).
    //   5. grip: longCoef = LongGripCurve(longSlip); latCoef = LatGripCurve(-latSpeed), the lat
    //      curve being mDriftLatGripCurve while mu8DriftState != 0.
    //   6. forces: longForce = longCoef * N * |longSpeed - wheelSurfaceSpeed|;
    //              latForce  = latCoef  * N * |latSpeed|.
    //   7. friction cone: cap = min(frictionCo * (N + downForce), adhesiveLimit * surfaceGrip),
    //      frictionCo being the tyre's DYNAMIC coefficient once mbBrokenAdhesiveLimit is latched
    //      and its STATIC one before that. If longForce^2 + latForce^2 exceeds cap^2, rebuild the
    //      force as lng*longForce + lat*latForce*(1-longForceBias), renormalise it, scale it by the
    //      DYNAMIC cap, and re-project onto lng and lat. mbBrokenAdhesiveLimit records that.
    //   8. a wheel only gets force when (crashing ? mbHasTraction : lbMostWheelsHaveTraction &&
    //      mbIsCloseToGround && mNormal.y > 0.5); otherwise every solved quantity is zeroed.
    //   9. wheel spin reaction: Wheel::ApplyFrictionReaction(-longForce*radius, longSpeed, dt).
    //  10. body reaction: each of the two force components is applied at the wheel's streamed
    //      x/z with the attrib height offset for y, as r x F into AddWorldSpaceTorque, and
    //      accumulated into mTotalLinearForce scaled by mvfWheelFrictionLinearMultiplier.
    //
    // AT REST this produces EXACTLY ZERO: v = 0 and omega = 0 give longSlip = 0 and latSpeed = 0,
    // both grip curves return 0 at zero slip, so both forces are 0, the cone is not exceeded, the
    // wheel torque is 0 and both AddWorldSpaceTorque/mTotalLinearForce contributions are 0. That is
    // a property of the model, not a special case -- and it is the test this wave was held for.
    // MEASURED: the settled body position is BIT-IDENTICAL with and without this function
    // (2986.941406 / -3.207705 / -2011.413696, vel 0/0/0, over a 275 s boot). Nothing drifts.
    //
    // ⛔ THE PARAGRAPH THAT USED TO SIT HERE IS RETIRED, AND ITS DELETE-WHEN IS DISCHARGED.
    // It said, in bold, "**NOTHING IN THIS TREE WRITES** maWheels[i].mSpeedAndMassOnWheelVariables.z
    // ... so today N == 0, and therefore longForce == latForce == 0 for every wheel on every frame".
    // That was true when it was written and it is FALSE NOW: the writer landed in
    // CalculateWeightTransfer @0x825F9DD0 (this file, `maWheels[liWheel].mSpeedAndMassOnWheelVariables.z
    // = lfMass * lfScaler + KF_RECIP_GRAVITY * lafCornerTransfer[liWheel]`, gated on
    // RoadContact.mbIsOnGround) -- exactly the `vrlimi128 mask 2` store the note predicted, found by
    // scanning the exports' ASSEMBLY for the +0xED0 mvSpringMassScalers read instead of for the
    // (inlined, unexported) SetMassOnWheel name. The old text's own DELETE-WHEN line named that
    // condition; nobody came back to delete it.
    // ⚠️ IT IS NOT A HARMLESS LEFTOVER. A stale banner that says the tyre model produces no force is
    // a diagnosis handed to every later reader: the steering wave of 2026-09-02 read it mid-
    // investigation and was one step from reporting "steering does nothing because the tyres carry no
    // load" -- while the same build was measuring a clean 0 -> 0.90 rad/s yaw ramp at 60 mph under
    // full lock with all four wheels loaded. [[diagnostics-that-lie]] / "a file's own comment can be
    // the regression": the comment WAS the regression, and the code was right.
    // ⭐ WHAT IS WORTH KEEPING FROM IT, because it is still the fastest way to falsify a "the tyre
    // model is broken" claim: the load term is that lane, N = max(massOnWheel * g * mNormal.y, 0),
    // and both grip curves return 0 at zero slip. If a wave sees zero tyre force, print
    // massOnWheel FIRST -- a zero there is a LOAD failure (weight transfer / on-ground gate), not a
    // model failure, and the two need completely different fixes.
    void VehiclePhysics::HandleWheelPairFriction(EVehicleDrivenWheel leWheelA,
                                                 EVehicleDrivenWheel leWheelB,
                                                 Vector3 lvRollDirection,
                                                 VecFloat lvfDownForce,
                                                 VecFloat lvfTimeStep,
                                                 VecFloat lvfSurfaceGripA,
                                                 VecFloat lvfSurfaceGripB,
                                                 bool lbMostWheelsHaveTraction,
                                                 bool lbUnusedFalse)
    {
        static const f32 KF_SLIP_DENOM_FLOOR  = 1.0f;   // vmaxfp against vcfsx(1,0) == 1.0
        static const f32 KF_TRACTION_NORMAL_Y = 0.5f;   // flt_82001DA0

        const EVehicleDrivenWheel laeWheel[2] = { leWheelA, leWheelB };
        const f32 lafSurfaceGrip[2] = { lvfSurfaceGripA.x, lvfSurfaceGripB.x };

        // One console SIMD lane's worth of solved state (lane0 = wheel A, lane1 = wheel B).
        struct WheelSolve
        {
            Vector3 mvLongDir;              // the normalised longitudinal direction
            Vector3 mvLatDir;               // the normalised lateral direction
            f32     mfLongSpeed;            // dot(contactVelocity, longDir)
            f32     mfLatSpeed;             // dot(contactVelocity, latDir)
            f32     mfLongSlip;             // (wheelSurfaceSpeed - longSpeed) / max(|longSpeed|,1)
            f32     mfWheelSurfaceSpeed;    // omega * radius
            f32     mfLongForce;            // pre-cone
            f32     mfLatForce;             // pre-cone
            f32     mfLongFinal;            // post-cone
            f32     mfLatFinal;             // post-cone
            f32     mfWheelTorque;          // -longFinal * radius
            f32     mfSlipDenom;            // max(|longSpeed|, 1) -- NOT zeroed by the enable gate
            bool    mbConeExceeded;
            bool    mbEnabled;
        };
        // ⛔ D7 (drift-symmetry wave 2026-09-02, raw words 0x825FBF4C..0x825FBFBC wheel A /
        // 0x825FC070..0x825FC0E8 wheel B): the console writes a THIRD slip lane the old body never
        // wrote -- mSlipVariables.z, the skid factor BrnVehicleOutputInterface copies into
        // WheelLite::mfSkidFactor (the VFX/audio skid input):
        //   z = min(1, (|latSpeed| * K_LAT + |longSlip * K_SLIP|) * min(1, denom * (1/K_DENOM)))
        // with K_DENOM = unk_82FB9BF0 (0.5, flt_82001DA0), K_SLIP = unk_82FB83F0 (0.9,
        // flt_82005450), K_LAT = unk_82FB9150 (1/15, flt_8200D4E0). The lat speed and the slip are
        // the ENABLE-GATED lanes (v29/v21 after the mask-8 / mask-4 zeroing), the denominator is
        // the ungated v22; both reciprocals are vrefp + two Newton steps.
        static const f32 KF_SKID_DENOM_SCALE = 0.5f;          // unk_82FB9BF0
        static const f32 KF_SKID_SLIP_SCALE  = 0.9f;          // unk_82FB83F0
        static const f32 KF_SKID_LAT_SCALE   = 0.0666666701f; // unk_82FB9150
        WheelSolve laSolve[2];

        // ---- phase 1: solve both wheels (the console's 2-wide SIMD body) --------------------
        for (s32 li = 0; li < 2; ++li)
        {
            Wheel& lrWheel = maWheels[laeWheel[li]];
            WheelSolve& lrOut = laSolve[li];

            // `lwz r11,0xD0(r31) ; cmplwi ; bne` -> the console's dev assert, Wheel.h:549.
            CGS_ASSERT(lrWheel.mpTireAttribs != NULL, "mpTireAttribs != NULL");
            if (lrWheel.mpTireAttribs == NULL)
            {
                // The console asserts and then dereferences anyway; a null deref here is an AV, so
                // the wheel is skipped instead. Unreachable once Wheel::Prepare has run.
                lrOut = WheelSolve();
                lrOut.mvLongDir = lrWheel.mWheelLongDirection;
                lrOut.mvLatDir  = lrWheel.mWheelLatDirection;
                continue;
            }
            const Wheel::TireAttribs& lrTire = *lrWheel.mpTireAttribs;

            // 1. the contact frame. `vrsqrtefp` + one Newton step on each of the three magnitudes.
            const Vector3 lvNormal = lrWheel.GetRoadContact().mNormal;
            const Vector3 lvN      = vpu::Normalize(lvNormal);
            const Vector3 lvLat    = vpu::Normalize(vpu::Cross(lvN, lvRollDirection));
            const Vector3 lvLong   = vpu::Normalize(vpu::Cross(lvLat, lvN));

            // 2. the contact-point velocity (`vpermwi128 0x63` cross-product idiom against
            //    mAngularVelocity, added to mLinearVelocity).
            const Vector3& lvBodyPos = mTransform.Pos();
            const Vector3  lvContact = lrWheel.GetRoadContact().mPosition;
            const Vector3  lvR{ lvContact.x - lvBodyPos.x,
                                lvContact.y - lvBodyPos.y,
                                lvContact.z - lvBodyPos.z, 0.0f };
            const Vector3  lvOmegaXR = vpu::Cross(mAngularVelocity, lvR);
            const Vector3  lvVel{ mLinearVelocity.x + lvOmegaXR.x,
                                  mLinearVelocity.y + lvOmegaXR.y,
                                  mLinearVelocity.z + lvOmegaXR.z, 0.0f };

            const f32 lfLongSpeed = vpu::Dot(lvVel, lvLong);
            const f32 lfLatSpeed  = vpu::Dot(lvVel, lvLat);

            // 3. slip. `vmulfp128 v23,v30(omega),v20(radius)` then the clamped reciprocal.
            const f32 lfRadius   = lrWheel.mSlipVariables.w;
            const f32 lfSurfSpd  = lrWheel.mIntegrationVariables.x * lfRadius;
            f32 lfDenom = std::fabs(lfLongSpeed);
            if (lfDenom < KF_SLIP_DENOM_FLOOR) lfDenom = KF_SLIP_DENOM_FLOOR;
            const f32 lfLongSlip = (lfSurfSpd - lfLongSpeed) / lfDenom;

            // 4. the normal load: massOnWheel * g * n.y, floored at 0 (`vmaxfp v28,v5,0`). The
            //    RAW normal .y is used here, not the normalised one -- as the asm does.
            f32 lfLoad = lrWheel.mSpeedAndMassOnWheelVariables.z * KF_GRAVITY * lvNormal.y;
            if (lfLoad < 0.0f) lfLoad = 0.0f;

            // 5. the grip curves. mu8DriftState != 0 swaps in the dedicated drift lateral curve
            //    (`lbz r8,0x1352(r26)` selecting tireAttribs+0x20 over +0x10).
            const Wheel::TireGripCurve& lrLatCurve =
                (mu8DriftState != 0) ? lrTire.mDriftLatGripCurve : lrTire.mLatGripCurve;
            const f32 lfLongCoef = lrTire.mLongGripCurve.GetCoefficient(
                                       VecFloat{ lfLongSlip, lfLongSlip, lfLongSlip, lfLongSlip }).x;
            const f32 lfLatCoef  = lrLatCurve.GetCoefficient(
                                       VecFloat{ -lfLatSpeed, -lfLatSpeed, -lfLatSpeed, -lfLatSpeed }).x;

            // 6. the raw forces.
            const f32 lfLongForce = lfLongCoef * lfLoad * std::fabs(lfLongSpeed - lfSurfSpd);
            const f32 lfLatForce  = lfLatCoef  * lfLoad * std::fabs(lfLatSpeed);

            // 7. the friction cone. maPackedVariables = {static, dynamic, adhesiveLimit, longBias}.
            const f32 lfAdhesiveCap = lrTire.maPackedVariables.z * lafSurfaceGrip[li];
            const f32 lfNormalPlusDF = lfLoad + lvfDownForce.x;
            // `vsel v15,v15(static),v25(dynamic),v26` -- once the limit has been broken the tyre
            // stays on its DYNAMIC coefficient until the flag clears (the standard stiction latch).
            const f32 lfSelectedCo = lrWheel.mbBrokenAdhesiveLimit ? lrTire.maPackedVariables.y
                                                                   : lrTire.maPackedVariables.x;
            f32 lfConeCap = lfSelectedCo * lfNormalPlusDF;
            if (lfConeCap > lfAdhesiveCap) lfConeCap = lfAdhesiveCap;
            f32 lfScaleCap = lrTire.maPackedVariables.y * lfNormalPlusDF;
            if (lfScaleCap > lfAdhesiveCap) lfScaleCap = lfAdhesiveCap;

            // `vcmpgtfp v28, latF^2+longF^2, cap^2` OR'd with the lbUnusedFalse mask (the caller
            // passes false at every site, so the OR is inert -- modelled because the asm has it).
            bool lbCone = (lfLongForce * lfLongForce + lfLatForce * lfLatForce)
                              > (lfConeCap * lfConeCap);
            if (lbUnusedFalse) lbCone = true;

            f32 lfLongFinal = lfLongForce;
            f32 lfLatFinal  = lfLatForce;
            if (lbCone)
            {
                // rebuild, renormalise (rsqrt + Newton), rescale to the dynamic cap, re-project.
                const f32 lfBiasedLat = lfLatForce * (1.0f - lrTire.maPackedVariables.w);
                const Vector3 lvF{ lvLong.x * lfLongForce + lvLat.x * lfBiasedLat,
                                   lvLong.y * lfLongForce + lvLat.y * lfBiasedLat,
                                   lvLong.z * lfLongForce + lvLat.z * lfBiasedLat, 0.0f };
                const Vector3 lvFhat = vpu::Normalize(lvF);
                const Vector3 lvCapped{ lvFhat.x * lfScaleCap,
                                        lvFhat.y * lfScaleCap,
                                        lvFhat.z * lfScaleCap, 0.0f };
                lfLongFinal = vpu::Dot(lvCapped, lvLong);
                lfLatFinal  = vpu::Dot(lvCapped, lvLat);
            }

            // 8. the per-wheel enable. `lbz r7,0x710(r26)` selects between the crash test and the
            //    grounded test; the grounded test re-reads the contact normal's .y vs 0.5, which is
            //    exactly the mbHasTraction rule SetRoadContact applies (BPR agrees, `>0.5`).
            const bool lbEnabled =
                mbCrashing ? lrWheel.mbHasTraction
                           : (lbMostWheelsHaveTraction
                              && lrWheel.GetRoadContact().mbIsCloseToGround
                              && lrWheel.GetRoadContact().mNormal.y > KF_TRACTION_NORMAL_Y);

            lrOut.mvLongDir           = lvLong;
            lrOut.mvLatDir            = lvLat;
            lrOut.mfWheelSurfaceSpeed = lfSurfSpd;
            lrOut.mfSlipDenom         = lfDenom;      // D7: the ungated v22 lane
            lrOut.mbEnabled           = lbEnabled;

            // A disabled lane is zeroed by `vrlimi128 vX, 0, 8|4, 0` on EVERY solved register, and
            // the cone mask is ANDed with the matching lane-clear constant. The directions and the
            // wheel-surface speed are NOT in that list, so they are written as computed.
            if (lbEnabled)
            {
                lrOut.mfLongSpeed    = lfLongSpeed;
                lrOut.mfLatSpeed     = lfLatSpeed;
                lrOut.mfLongSlip     = lfLongSlip;
                lrOut.mfLongForce    = lfLongForce;
                lrOut.mfLatForce     = lfLatForce;
                lrOut.mfLongFinal    = lfLongFinal;
                lrOut.mfLatFinal     = lfLatFinal;
                lrOut.mfWheelTorque  = -lfLongFinal * lfRadius;   // `vxor` sign flip * radius
                lrOut.mbConeExceeded = lbCone;
            }
            else
            {
                lrOut.mfLongSpeed    = 0.0f;
                lrOut.mfLatSpeed     = 0.0f;
                lrOut.mfLongSlip     = 0.0f;
                lrOut.mfLongForce    = 0.0f;
                lrOut.mfLatForce     = 0.0f;
                lrOut.mfLongFinal    = 0.0f;
                lrOut.mfLatFinal     = 0.0f;
                lrOut.mfWheelTorque  = 0.0f;
                lrOut.mbConeExceeded = false;
            }
        }

        // ---- phase 2: write both wheels back (console order: A's block, then B's) ------------
        for (s32 li = 0; li < 2; ++li)
        {
            Wheel& lrWheel = maWheels[laeWheel[li]];
            const WheelSolve& lrIn = laSolve[li];

            // `vcmpeqfp. splat(coneMask.lane) , 0 ; mfocrf ; not ; extrwi ; stb 0x205(wheel)`
            lrWheel.mbBrokenAdhesiveLimit = lrIn.mbConeExceeded;

            // the wheel-spin reaction (inlined for wheel A at 0x825FBE6C, a real `bl` for wheel B).
            lrWheel.ApplyFrictionReaction(
                VecFloat{ lrIn.mfWheelTorque, lrIn.mfWheelTorque, lrIn.mfWheelTorque, lrIn.mfWheelTorque },
                VecFloat{ lrIn.mfLongSpeed,   lrIn.mfLongSpeed,   lrIn.mfLongSpeed,   lrIn.mfLongSpeed },
                lvfTimeStep);

            // mForceVariables = {longPreCone, latPreCone, longFinal, latFinal}
            //   (+0x50: vrlimi mask 2 -> .z, mask 1 -> .w, mask 8 -> .x, mask 4 -> .y)
            lrWheel.mForceVariables.x = lrIn.mfLongForce;
            lrWheel.mForceVariables.y = lrIn.mfLatForce;
            lrWheel.mForceVariables.z = lrIn.mfLongFinal;
            lrWheel.mForceVariables.w = lrIn.mfLatFinal;

            // mSlipVariables: .x = lateral speed, .y = longitudinal slip, .w = radius (untouched).
            lrWheel.mSlipVariables.x = lrIn.mfLatSpeed;
            lrWheel.mSlipVariables.y = lrIn.mfLongSlip;
            {
                // D7 -- the skid factor lane (see the banner above the solve loop).
                f32 lfDenomScale = lrIn.mfSlipDenom * (1.0f / KF_SKID_DENOM_SCALE);
                if (lfDenomScale > 1.0f) lfDenomScale = 1.0f;                  // vminfp vs 1.0
                f32 lfSkid = (std::fabs(lrIn.mfLatSpeed) * KF_SKID_LAT_SCALE
                              + std::fabs(lrIn.mfLongSlip * KF_SKID_SLIP_SCALE)) * lfDenomScale;
                if (lfSkid > 1.0f) lfSkid = 1.0f;                              // vminfp vs 1.0
                lrWheel.mSlipVariables.z = lfSkid;                              // vrlimi 2,2 -> .z
            }
            // GATED: mSlipVariables.z (WheelLite::mfSkidFactor). The console computes
            //       min( (|latSpeed| * unk_82FB9150 + |longSlip * unk_82FB83F0|)
            //            * min(max(|longSpeed|,1) / unk_82FB9BF0, 1.0), 1.0 )
            //   -- the shape is fully read (0x825FBF4C..0x825FBFC0, and the BPR twin's
            //   `min((|lat|*K1 + |longSlip*K2|) * min(|long|/K3,1), 1)` is the second witness) --
            //   but all THREE constants are .bss slots that no other function in the export set
            //   references, so their static-init writers cannot be recovered from what is here.
            //   Emitting them as zero would make the ratio 1/0 and store a NaN into a value the
            //   race-car state copies out every frame. This lane is a REPORTED skid amount and
            //   feeds no force, so it is left alone rather than fabricated. Reinstate the three
            //   lines above the moment the writers are read out of the IDB.

            // mSpeedAndMassOnWheelVariables: .x = road long speed, .y = road lat speed,
            // .w = wheel surface speed (omega*radius). .z (mass on wheel) is NOT touched.
            lrWheel.mSpeedAndMassOnWheelVariables.x = lrIn.mfLongSpeed;
            lrWheel.mSpeedAndMassOnWheelVariables.y = lrIn.mfLatSpeed;
            lrWheel.mSpeedAndMassOnWheelVariables.w = lrIn.mfWheelSurfaceSpeed;

            // the two contact-frame axes (`stvx128 v127,r31,0xB0` / `stvx128 v126,r31,0xC0`).
            lrWheel.mWheelLongDirection = lrIn.mvLongDir;
            lrWheel.mWheelLatDirection  = lrIn.mvLatDir;
        }

        // ---- phase 3: the body reaction -- torque + linear residual, A then B ----------------
        const Vector4& lvHeightOffsets =
            mpAttribs->mBodyRollAttribs.mvWheelLongForceHeightOffset_WheelLatForceHeightOffset;
        const f32 lfLinearMultiplier = mvfWheelFrictionLinearMultiplier.x;

        for (s32 li = 0; li < 2; ++li)
        {
            if (!laSolve[li].mbEnabled)
                continue;

            const Wheel& lrWheel = maWheels[laeWheel[li]];
            const WheelSolve& lrIn = laSolve[li];
            const Vector3 lvStreamed = lrWheel.mStreamedPositionPlusTwistAmount.GetVector3();

            // The two application points: the wheel's streamed x/z with the attrib height offset
            // substituted for y (`vperm128 v123,v3,v19,<gather lane0>` then `vrlimi128 v123,v2,2,2`).
            const Vector3 lavPoint[2] = {
                Vector3{ lvStreamed.x, lvHeightOffsets.x, lvStreamed.z, 0.0f },   // longitudinal
                Vector3{ lvStreamed.x, lvHeightOffsets.y, lvStreamed.z, 0.0f }    // lateral
            };
            const Vector3 lavForce[2] = {
                Vector3{ lrIn.mvLongDir.x * lrIn.mfLongFinal,
                         lrIn.mvLongDir.y * lrIn.mfLongFinal,
                         lrIn.mvLongDir.z * lrIn.mfLongFinal, 0.0f },
                Vector3{ lrIn.mvLatDir.x * lrIn.mfLatFinal,
                         lrIn.mvLatDir.y * lrIn.mfLatFinal,
                         lrIn.mvLatDir.z * lrIn.mfLatFinal, 0.0f }
            };

            for (s32 lj = 0; lj < 2; ++lj)
            {
                const Vector3& lvF = lavForce[lj];

                // the console's per-lane IsValid asserts (:5669/:5676 for wheel A, :5686/:5693 for
                // wheel B), lowered to the same NaN test the rest of this file uses.
                CGS_ASSERT(lvF.x == lvF.x && lvF.y == lvF.y && lvF.z == lvF.z,
                           (lj == 0) ? "Invalid long wheel force: lWheelLongDirection * lvfLongForcePostAdhesion"
                                     : "Invalid lateral wheel force: lWheelLatDirection * lvfLatForcePostAdhesion");

                // r = the application point ROTATED by the body transform (rows only -- the asm
                // never adds mTransform.Pos()), then tau = r x F.
                const Vector3& lvP = lavPoint[lj];
                const Vector3& lvRight = mTransform.Right();
                const Vector3& lvUp    = mTransform.Up();
                const Vector3& lvAt    = mTransform.At();
                const Vector3 lvArm{ lvRight.x * lvP.x + lvUp.x * lvP.y + lvAt.x * lvP.z,
                                     lvRight.y * lvP.x + lvUp.y * lvP.y + lvAt.y * lvP.z,
                                     lvRight.z * lvP.x + lvUp.z * lvP.y + lvAt.z * lvP.z, 0.0f };

                AddWorldSpaceTorque(vpu::Cross(lvArm, lvF));

                // `lvx128 v12,[this+0xF0] ; vmaddfp128 v12,v127,v0,v12 ; stvx128` -- a DIRECT
                // accumulate into mTotalLinearForce, not a call to AddWorldSpaceForce.
                mTotalLinearForce.x += lvF.x * lfLinearMultiplier;
                mTotalLinearForce.y += lvF.y * lfLinearMultiplier;
                mTotalLinearForce.z += lvF.z * lfLinearMultiplier;
            }
        }

        // ---- [tyre] PC bring-up instrument -- DELETE WHEN the yaw settles -------------------
        // OPT-IN (BRN_TYRE_PROBE=1) so a default run is byte-identical to a build without it.
        // Three aliasing hazards are closed by construction:
        //   * PER-INSTANCE, not global -- this method runs for EVERY vehicle, so the counter is
        //     keyed on `this`.
        //   * PER-PAIR, not per call -- UpdateWheels calls this twice per frame (rear, then
        //     front), so rear and front get separate counters and print on the SAME frames.
        //   * IDENTITY IS PRINTED, not assumed: `this`, the driver type, and the body's linear
        //     velocity, so a probe sitting on the wrong car is visible rather than silent.
        {
            static s32 siTyreProbe = -1;
            if (siTyreProbe < 0)
            {
                const char* lpcEnv = getenv("BRN_TYRE_PROBE");
                siTyreProbe = (lpcEnv != 0 && lpcEnv[0] != '0') ? 1 : 0;
            }
            if (siTyreProbe == 1 && CgsDev::Log::gpDebugPrint != 0)
            {
                const s32 KI_PROBE_SLOTS = 8;
                static const void* sapInstance[KI_PROBE_SLOTS] = { 0 };
                static u32         sauCount[KI_PROBE_SLOTS][2] = { { 0 } };
                s32 liSlot = -1;
                for (s32 liS = 0; liS < KI_PROBE_SLOTS; ++liS)
                {
                    if (sapInstance[liS] == this) { liSlot = liS; break; }
                    if (sapInstance[liS] == 0) { sapInstance[liS] = this; liSlot = liS; break; }
                }
                // The front pair is the one whose A wheel is eFrontLeftWheel (UpdateWheels' own
                // two call sites); rear and front therefore land in separate counters and print
                // on the SAME frames.
                const s32 liPair = (leWheelA == eFrontLeftWheel) ? 1 : 0;
                if (liSlot >= 0)
                {
                    u32& suCount = sauCount[liSlot][liPair];
                    ++suCount;
                    if ((suCount % 20u) == 0u)
                    {
                        const Vector3& lvUp2 = mTransform.Up();
                        const Vector3& lvAt2 = mTransform.At();
                        const f32 lfYawRate = vpu::Dot(mAngularVelocity, lvUp2);
                        *CgsDev::Log::gpDebugPrint
                            << "[tyre] n " << static_cast<s32>(suCount)
                            << (liPair ? " FRONT" : " REAR ")
                            << " car " << static_cast<u32>(reinterpret_cast<u64>(this))
                            << " drv " << static_cast<s32>(mPreviousControls.GetType())
                            << " at " << lvAt2.x << " " << lvAt2.z
                            << " vel " << mLinearVelocity.x << " " << mLinearVelocity.y
                            << " " << mLinearVelocity.z
                            << " yawRate " << lfYawRate
                            << " steerRad " << mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x
                            << " steerIn " << mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y
                            << " drift " << static_cast<s32>(mu8DriftState)
                            << " mostTrac " << (lbMostWheelsHaveTraction ? 1 : 0)
                            << " downF " << lvfDownForce.x
                            << " rollDir " << lvRollDirection.x << " " << lvRollDirection.z
                            // invYaw = Up . (W * Up): the scalar that turns a yaw torque into a
                            // yaw acceleration. A too-small yaw INERTIA (too LARGE an invYaw)
                            // spins a car exactly like the reported defect, so the number that
                            // would prove or clear suspect 3 is printed beside the torques.
                            << " invYaw " << (lvUp2.x * (mWorldInverseInertia.xAxis.x * lvUp2.x
                                                       + mWorldInverseInertia.yAxis.x * lvUp2.y
                                                       + mWorldInverseInertia.zAxis.x * lvUp2.z)
                                            + lvUp2.y * (mWorldInverseInertia.xAxis.y * lvUp2.x
                                                       + mWorldInverseInertia.yAxis.y * lvUp2.y
                                                       + mWorldInverseInertia.zAxis.y * lvUp2.z)
                                            + lvUp2.z * (mWorldInverseInertia.xAxis.z * lvUp2.x
                                                       + mWorldInverseInertia.yAxis.z * lvUp2.y
                                                       + mWorldInverseInertia.zAxis.z * lvUp2.z))
                            << " mass " << mfMass.x
                            << "\n";

                        for (s32 lk = 0; lk < 2; ++lk)
                        {
                            const Wheel& lrW = maWheels[laeWheel[lk]];
                            const WheelSolve& lrS = laSolve[lk];
                            const Vector3 lvStr = lrW.mStreamedPositionPlusTwistAmount.GetVector3();
                            const f32 lfN = lrW.mSpeedAndMassOnWheelVariables.z * KF_GRAVITY
                                          * lrW.GetRoadContact().mNormal.y;

                            // The lateral force's YAW moment about the body Up axis -- the number
                            // this whole leg is about. Built exactly as phase 3 builds it.
                            const Vector3 lvP2{ lvStr.x, lvHeightOffsets.y, lvStr.z, 0.0f };
                            const Vector3& lvRt2 = mTransform.Right();
                            const Vector3 lvArm2{ lvRt2.x * lvP2.x + lvUp2.x * lvP2.y + lvAt2.x * lvP2.z,
                                                  lvRt2.y * lvP2.x + lvUp2.y * lvP2.y + lvAt2.y * lvP2.z,
                                                  lvRt2.z * lvP2.x + lvUp2.z * lvP2.y + lvAt2.z * lvP2.z,
                                                  0.0f };
                            const Vector3 lvFl2{ lrS.mvLatDir.x * lrS.mfLatFinal,
                                                 lrS.mvLatDir.y * lrS.mfLatFinal,
                                                 lrS.mvLatDir.z * lrS.mfLatFinal, 0.0f };
                            const f32 lfYawTorque = vpu::Dot(vpu::Cross(lvArm2, lvFl2), lvUp2);

                            // ... and the LONGITUDINAL force's yaw moment, which a left/right
                            // drive or brake imbalance also feeds. Reported separately so a
                            // "the tyres are not resisting" reading cannot be confused with a
                            // "the drive is pushing it round" one.
                            const Vector3 lvPl2{ lvStr.x, lvHeightOffsets.x, lvStr.z, 0.0f };
                            const Vector3 lvArmL{ lvRt2.x * lvPl2.x + lvUp2.x * lvPl2.y + lvAt2.x * lvPl2.z,
                                                  lvRt2.y * lvPl2.x + lvUp2.y * lvPl2.y + lvAt2.y * lvPl2.z,
                                                  lvRt2.z * lvPl2.x + lvUp2.z * lvPl2.y + lvAt2.z * lvPl2.z,
                                                  0.0f };
                            const Vector3 lvFg2{ lrS.mvLongDir.x * lrS.mfLongFinal,
                                                 lrS.mvLongDir.y * lrS.mfLongFinal,
                                                 lrS.mvLongDir.z * lrS.mfLongFinal, 0.0f };
                            const f32 lfYawTorqueL = vpu::Dot(vpu::Cross(lvArmL, lvFg2), lvUp2);

                            *CgsDev::Log::gpDebugPrint
                                << "[tyre]   w" << static_cast<s32>(laeWheel[lk])
                                << " en " << (lrS.mbEnabled ? 1 : 0)
                                << " grnd " << (lrW.GetRoadContact().mbIsCloseToGround ? 1 : 0)
                                << " nY " << lrW.GetRoadContact().mNormal.y
                                << " mass " << lrW.mSpeedAndMassOnWheelVariables.z
                                << " N " << lfN
                                << " latSpd " << lrS.mfLatSpeed
                                << " Flat " << lrS.mfLatForce << " -> " << lrS.mfLatFinal
                                << " lngSlip " << lrS.mfLongSlip
                                << " Flng " << lrS.mfLongForce << " -> " << lrS.mfLongFinal
                                << " cone " << (lrS.mbConeExceeded ? 1 : 0)
                                // THE CONE, printed rather than inferred. `sGrip` is
                                // GetSurfaceGrip's result for this wheel, `adhCap` the adhesive
                                // ceiling it scales, `statCap` the load-based one; whichever is
                                // SMALLER is the cap that binds. Front and rear reading different
                                // sGrip is the whole defect, so the number is on the line.
                                << " sGrip " << lafSurfaceGrip[lk]
                                << " adhCap " << (lrW.mpTireAttribs != 0
                                        ? lrW.mpTireAttribs->maPackedVariables.z * lafSurfaceGrip[lk]
                                        : 0.0f)
                                << " statCap " << (lrW.mpTireAttribs != 0
                                        ? lrW.mpTireAttribs->maPackedVariables.y
                                              * (lfN + lvfDownForce.x)
                                        : 0.0f)
                                << " arm " << lvStr.x << " " << lvStr.z
                                << " latDir " << lrS.mvLatDir.x << " " << lrS.mvLatDir.z
                                << " yawTqLat " << lfYawTorque
                                << " yawTqLng " << lfYawTorqueL
                                << "\n";
                        }
                    }
                }
            }
        }
        // ---- end [tyre] ---------------------------------------------------------------------
    }

    // ===========================================================================================
    //  @0x825D41A8  BrnPhysics::Vehicle::VehiclePhysics::HandleWheelFrictionCrashing (433 instrs)
    // ===========================================================================================
    // The single-wheel scrub that runs while crashing (the caller separately applies its mass
    // gate). Breaker passes `this` in r3 and the wheel enum in r4; the DecFIGS-declared VecFloat
    // timestep is in v1 and is deliberately unused by this body.
    //
    // 0x825D4210..0x825D4224 sends a tractionless or burnout wheel to the 0.95 spin-decay leg.
    // The active leg forms the contact-point velocity, removes its component along the contact
    // normal, and compares MagnitudeSquared(the planar remainder) > 0.1f at 0x825D4318..0x4338.
    // 0x825D433C..0x43F8 then forms Normalize(normal x bodyAt) and -NormalizeFast(movement).
    //
    // The eight DecFIGS-named local constants are image-pinned by the lazy initialisers at
    // 0x825D4400..0x825D45DC. Player/network use {30, 3, .95, 1}; AI/traffic use
    // {20, 4, 1, .2}. The selector is mPreviousControls.meDriverType (+0x10D4), tested for 1/3
    // at 0x825D45E4..0x825D4608. The final cascade (0x825D460C..0x825D4694) lowers to:
    //   scale = min(|pointVelocity|, speedForMaxRoll) / speedForMaxRoll;
    //   point.y -= maxHeightOffset * scale;
    //   friction = wheelLat * dot(wheelLat, -movementHat) * massOnWheel * g * lateralFriction.
    // It applies (point - bodyPos) x friction as torque and accumulates friction times the selected
    // linear multiplier into mTotalLinearForce (0x825D47C0..0x825D47FC).
    void VehiclePhysics::HandleWheelFrictionCrashing(EVehicleDrivenWheel leWheel,
                                                     VecFloat lvfTimeStep)
    {
        static const f32 KF_CRASH_SPIN_DECAY                          = 0.95f;
        static const f32 KF_MIN_PLANAR_MOVEMENT_SQUARED               = 0.1f;
        static const f32 KF_SPEED_FOR_MAX_ROLL_PLAYER                 = 30.0f;
        static const f32 KF_MAX_FRICTION_HEIGHT_OFFSET_PLAYER         = 3.0f;
        static const f32 KF_LAT_CRASHING_DYNAMIC_FRICTION_PLAYER      = 0.95f;
        static const f32 KF_LIN_FORCE_MULT_PLAYER                     = 1.0f;
        static const f32 KF_SPEED_FOR_MAX_ROLL_AI                     = 20.0f;
        static const f32 KF_MAX_FRICTION_HEIGHT_OFFSET_AI             = 4.0f;
        static const f32 KF_LAT_CRASHING_DYNAMIC_FRICTION_AI          = 1.0f;
        static const f32 KF_LIN_FORCE_MULT_AI                         = 0.2f;

        (void)lvfTimeStep;

        CGS_ASSERT(IsCrashing(), "IsCrashing()");   // :5724

        Wheel& lrWheel = maWheels[leWheel];

        const bool lbActiveContact =
            lrWheel.mbHasTraction && lrWheel.mu8State != Wheel::eWheelInertiaTypeDriven_Burnout;

        if (!lbActiveContact)
        {
            // loc_825D4804: only mIntegrationVariables.x is multiplied by flt_82004FDC.
            lrWheel.mIntegrationVariables.x *= KF_CRASH_SPIN_DECAY;
            return;
        }

        const Wheel::RoadContact& lContact = lrWheel.GetRoadContact();
        const Vector3& lBodyPosition = mTransform.Pos();
        const Vector3 lWheelPosition{ lContact.mPosition.x - lBodyPosition.x,
                                      lContact.mPosition.y - lBodyPosition.y,
                                      lContact.mPosition.z - lBodyPosition.z, 0.0f };
        const Vector3 lAngularPointVelocity = vpu::Cross(mAngularVelocity, lWheelPosition);
        const Vector3 lPointVelocityAtWheel{
            mLinearVelocity.x + lAngularPointVelocity.x,
            mLinearVelocity.y + lAngularPointVelocity.y,
            mLinearVelocity.z + lAngularPointVelocity.z, 0.0f };

        const f32 lfNormalVelocity = vpu::Dot(lPointVelocityAtWheel, lContact.mNormal);
        const Vector3 lMovementInPlane{
            lPointVelocityAtWheel.x - lContact.mNormal.x * lfNormalVelocity,
            lPointVelocityAtWheel.y - lContact.mNormal.y * lfNormalVelocity,
            lPointVelocityAtWheel.z - lContact.mNormal.z * lfNormalVelocity, 0.0f };

        if (!(vpu::MagnitudeSquared(lMovementInPlane) > KF_MIN_PLANAR_MOVEMENT_SQUARED))
            return;

        const Vector3 lWheelLatDirection =
            vpu::Normalize(vpu::Cross(lContact.mNormal, mTransform.At()));
        const Vector3 lMovementDirection = vpu::NormalizeFast(lMovementInPlane);
        const Vector3 lFrictionDirection{ -lMovementDirection.x,
                                          -lMovementDirection.y,
                                          -lMovementDirection.z, 0.0f };

        const f32 lvfMassOnWheel = lrWheel.mSpeedAndMassOnWheelVariables.z;
        const f32 lvfWeightOnWheel = lvfMassOnWheel * KF_GRAVITY;

        const E_DRIVER_TYPE leDriverType = mPreviousControls.GetType();
        const bool lbUseAIConstants =
            leDriverType == E_DRIVER_TYPE_AI || leDriverType == E_DRIVER_TYPE_TRAFFIC;
        const f32 lvfSpeedForMaxRoll = lbUseAIConstants ? KF_SPEED_FOR_MAX_ROLL_AI
                                                        : KF_SPEED_FOR_MAX_ROLL_PLAYER;
        const f32 lvfMaxFrictionHeightOffset =
            lbUseAIConstants ? KF_MAX_FRICTION_HEIGHT_OFFSET_AI
                             : KF_MAX_FRICTION_HEIGHT_OFFSET_PLAYER;
        const f32 lvfLatCrashDynamicFriction =
            lbUseAIConstants ? KF_LAT_CRASHING_DYNAMIC_FRICTION_AI
                             : KF_LAT_CRASHING_DYNAMIC_FRICTION_PLAYER;
        const f32 lvfLinearForceMultiplier = lbUseAIConstants ? KF_LIN_FORCE_MULT_AI
                                                               : KF_LIN_FORCE_MULT_PLAYER;

        const f32 lfVelocity = vpu::Magnitude(lPointVelocityAtWheel);
        const f32 lfScaleFactor = std::min(lfVelocity, lvfSpeedForMaxRoll)
                                / lvfSpeedForMaxRoll;

        Vector3 lLocalPosition = lContact.mPosition;
        lLocalPosition.y -= lvfMaxFrictionHeightOffset * lfScaleFactor;

        const f32 lfLateralProjection = vpu::Dot(lWheelLatDirection, lFrictionDirection);
        const f32 lfFrictionMagnitude =
            lfLateralProjection * lvfWeightOnWheel * lvfLatCrashDynamicFriction;
        const Vector3 lFriction{ lWheelLatDirection.x * lfFrictionMagnitude,
                                 lWheelLatDirection.y * lfFrictionMagnitude,
                                 lWheelLatDirection.z * lfFrictionMagnitude, 0.0f };

        CGS_ASSERT(lvfLinearForceMultiplier == lvfLinearForceMultiplier,
                   "RwMathVPU::IsValid( lvfLinearForceMultiplier )");
        CGS_ASSERT(lFriction.x == lFriction.x && lFriction.y == lFriction.y
                       && lFriction.z == lFriction.z,
                   "RwMathVPU::IsValid( lFriction )");

        const Vector3 lFrictionArm{ lLocalPosition.x - lBodyPosition.x,
                                    lLocalPosition.y - lBodyPosition.y,
                                    lLocalPosition.z - lBodyPosition.z, 0.0f };
        const Vector3 lTorque = vpu::Cross(lFrictionArm, lFriction);
        AddWorldSpaceTorque(lTorque);

        mTotalLinearForce.x += lFriction.x * lvfLinearForceMultiplier;
        mTotalLinearForce.y += lFriction.y * lvfLinearForceMultiplier;
        mTotalLinearForce.z += lFriction.z * lvfLinearForceMultiplier;
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
    // SIGNATURE CONFORMED 2026-08-07 (wheel-cluster wave) to the DWARF 3-arg form; both extra
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
    // THE "BLOCKED" LABEL WAS INHERITED AND WRONG. The ledger and this file's own group notes
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
    // THE PARAMETER IS DEAD IN THE CONSOLE BODY. Vector arguments arrive in v1 (proved by this
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
    // Two independent corroborations of the per-wheel store, both from ALREADY-COMMITTED code:
    //    StoreLocalWheelPositions above uses the transpose of the very same Right/Up/At basis, and
    //    the C07 speed-match block at VehiclePhysics.cpp:1033-1036 uses the identical
    //    `mIntegrationVariables.x = target / mSlipVariables.w` idiom with the front pair taking
    //    maWheels[0].mSlipVariables.w and the rear pair maWheels[2]'s.
    //
    // Why the yaw-rate kill is deliberate and not a misread of the lanes: each wheel's spin is
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
    //   ⭐ BOUNDED 2026-09-03 (drive-spine 1:1 audit), so the flag stops reading open-ended: the
    //   argument here is mvSteeringAngle...x, a STEERING ANGLE in radians whose own attribute
    //   ceiling (SteeringAttribs mvMaxAngle, +0x10.x) keeps it well inside one quadrant. The 2*pi
    //   range reduction therefore never engages on this call, and what is left is XNAMath's
    //   degree-11 odd minimax on a |x| < pi/2 argument -- relative error ~1e-7, i.e. below the
    //   single-precision spacing of the steering angle itself. There is no reachable input on
    //   which this leaf and the console can differ by more than a float ulp.
    //
    // NOT REPRODUCED, deliberately: the console prologue lazily initialises two function-scope
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
    // THE DECODE IS NOT MINE. This function is an `.ida-exports` HOLE; the previous wave pulled
    //    0x825FDD78..0x825FE118 out of BURNOUT_X360_ARTIST.XEX.i64 with headless IDA and replayed it
    //    through a symbolic VMX128 simulator, closing the member map against the DWARF eight ways.
    //    That store-by-store decode is preserved verbatim in the block comment in VehiclePhysics.h;
    //    this body is its transcription, member for member, in the console's own order. It became
    //    writable this wave only because SetWheelVelocities did (the `mpAttribs != NULL` branch
    //    calls it out of line, so bodying Reset before it would have been a guaranteed LNK2019).
    //
    // THE TWO SILENT-ZERO SEEDS IN HERE. Both slots read all-zero in the shipped image and are
    //    filled at static init by IDA-unmarked thunks, so a literal scan finds only readers:
    //      * TimeSinceLastHandBrake (the +0x1080 .w lane) is seeded from unk_82FB9080 == 10000.0f
    //        (thunk 0x82C5C398 -> flt_82005D9C). Left at the image's 0.0f it would read as "the
    //        handbrake was released THIS INSTANT" on every single reset, which is exactly what the
    //        two UpdateHandBrake reads of that slot gate on.
    //      * TimeSinceLastRaceCarContact (the +0x1050 .z lane) is the same shape at 100.0f.
    //    Both are written here as the recovered values, NOT as the image's zeros.
    //
    // The partial clears are partial ON PURPOSE and are reproduced as such: mSlamEffect keeps
    //    mForce / mfDecay / mfRecoveryTime (so this is an inlined partial clear, not SlamEffect::
    //    Clear()); mvSteeringAngle keeps .w (DriftGasLetOffAmount); mvSpare_MaintainedSpeed keeps .x
    //    (Spare); mvSideForceMag keeps .z (TimeSinceLastBoostKick); mvDampRollVel keeps .y.
    void VehiclePhysics::Reset(Vector3 lvVelocity)
    {
        // Breaker forwards lvVelocity in v1 to the base Vector3 overload @0x825D9A58.  That
        // overload's current body ignores the value, but the source/ABI parameter is real.
        SimpleVehiclePhysics::Reset(lvVelocity);

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
    // THIS BODY WAS *NOT* WRITTEN FROM THE BANKED DECODE. The note that sat in VehiclePhysics.h
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
    // NOT AN EXPORT HOLE. `.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8262DBD0.json` carries the full
    //    assembly and xrefs_from (only its Hex-Rays pseudocode is degenerate). Every callee below is
    //    taken from that xrefs_from -- Wheel::Clear @0x825D6E88, SuspensionSpring::Prepare
    //    @0x825A7A28, VehicleAttribs::EngineAttribs::Construct @0x825B7B90, Engine::Reset
    //    @0x825CF130, VehicleAttribs::Construct @0x825F3FB8, SimpleVehiclePhysics::Reset(Vector3)
    //    @0x825D9A58, and VehiclePhysics::Reset(Vector3) @0x825FDD78. Construct supplies zero in
    //    v1 and clears the base frozen byte after the base call, i.e. the source zero-wrapper
    //    semantics are inlined; there is no ambiguity about either branch target.
    //
    // CORROBORATED BY A SECOND IMAGE. The PS3 DecFIGS build carries the same function at
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

        // Breaker supplies zero in v1 to the base Vector3 overload @0x825D9A58 and then clears
        // mbFrozen, which is exactly the source-level SimpleVehiclePhysics::Reset() wrapper.
        // The PS3 build wraps these last two in its own VehiclePhysics::Reset() 0-arg
        // (._ZN10BrnPhysics7Vehicle14VehiclePhysics5ResetEv @0x6EAEC4), whose entire body is
        // `SimpleVehiclePhysics::Reset(); Reset(Vector3(0));` -- which is what the X360 does here
        // in line.
        SimpleVehiclePhysics::Reset();

        Reset(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
    }

    // ==============================================================================================
    //  @0x8262DD58  BrnPhysics::Vehicle::VehiclePhysics::Destruct
    // ==============================================================================================
    void VehiclePhysics::Destruct()
    {
        SimpleVehiclePhysics::Destruct();

        // The derived teardown deliberately clears the wheel bank again after the base teardown.
        // Breaker emits this second four-wheel loop at 0x8262DD78..0x8262DD90.
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            maWheels[liWheel].Clear();

        const VecFloat lvfZero{ 0.0f, 0.0f, 0.0f, 0.0f };
        mEngine.Reset(lvfZero);
        mpAttribs = NULL;

        // These are the same intentionally partial effect clears emitted by Breaker.  In
        // particular, mForce/mfDecay/mfRecoveryTime and the untouched shunt lanes survive here.
        mSlamEffect.mfSteering         = 0.0f;
        mSlamEffect.mfOriginalSteering = 0.0f;
        mSlamEffect.mi8SlamNumber      = -1;
        mSlamEffect.mfTotalSlamTime    = 0.0f;
        mSlamEffect.mfSlamLife         = 0.0f;

        mShuntEffect.mDirectionPlusDesiredSpeed.SetZero();
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y = 0.0f;
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = -1.0f;

        mUsedAirRams.UnSetAll();
        mUsedSpins.UnSetAll();

        SimpleVehiclePhysics::Reset();
        Reset(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
    }

    void VehiclePhysics::Release()
    {
        // Breaker folds this source method into VehicleManager::Release and
        // ProcessRemoveEvents: std 0 at +0x1158/+0x1220, then stb 0 at +0x1359.
        mUsedAirRams.UnSetAll();
        mUsedSpins.UnSetAll();
        mbDeformationModelIsActive = false;
    }

    // =====================================================================================
    // C08 airborne/water/freeze/spin group -- BODIES.
    //   UpdateInWaterBehaviour @0x825B81A8, UpdateAirRam @0x825FC8D8, UpdateSpinEffects @0x825FCCF8,
    //   AddAirRam @0x825FE118 are bodied here.
    // this banner's "UpdateInAirBehaviour @0x825D0BE8 and UpdateFreezing
    //   @0x825CFD20 are BLOCKED -- structural skeletons" line is RETRACTED. BOTH are bodied:
    //   UpdateFreezing since the 2026-08-07 orchestrator wave (@ line ~4011) and
    //   UpdateInAirBehaviour since the 2026-08-11 driving-path wave (immediately below AddAirRam).
    // =====================================================================================

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
    void VehiclePhysics::UpdateInWaterBehaviour(VecFloat /*lvfDeltaTime*/)
    {
        // Breaker loads the low halfword of the packed collision tag (the same half used by
        // GetSurfaceLinearDrag), then extracts bits 4..9.
        const s32 liSurfaceId = static_cast<s32>((GetAboveGroundTagLo() >> 4) & 0x3Fu);
        if (!KAB_SURFACE_IS_WATER[liSurfaceId])
            return;

        // flt_82F2A4E4 @0x82F2A4E4 .data = 0x40000000 = 2.0 (already in the image; no initialiser).
        // ROLE CORROBORATED from the consumer: UpdateInWaterBehaviour @0x825B81D8 does `fcmpu ; bgelr`,
        // i.e. return early once the depth reaches the constant -- a drown DEPTH, exactly as named.
        // At 0.0f this test read `if (!(dist < 0))` and the in-water behaviour NEVER RAN.
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
    //         start 0; bit3(0x8) += +Y; bit4(0x10) += +X; bit5(0x20) += +Z;
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
    //   The axis constants are image-settled: unk_82181510={0,1,0}, the RenderWare gIVector is
    //   {1,0,0}, and unk_82181520={0,0,1}. The 50.0 scale, the (attribLane * lfFactor) product, the normalize, the
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

            if (luFlags & 0x8u)   lvDirection.y += 1.0f; // unk_82181510
            if (luFlags & 0x10u)  lvDirection.x += 1.0f; // rw::math::vpu::detail::gIVector
            if (luFlags & 0x20u)  lvDirection.z += 1.0f; // unk_82181520

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

    // =====================================================================================
    // @0x825D0BE8  BrnPhysics::Vehicle::VehiclePhysics::UpdateInAirBehaviour  (809 instructions)
    //
    // THE ACTIVE AIRBORNE ATTITUDE CONTROLLER -- the reason Burnout jumps feel good. It is NOT
    // "tuned gravity": while the car is off the ground the game actively damps and steers its
    // rotation so it lands flat, and it does that with three cooperating mechanisms:
    //   (1) a ONE-SHOT take-off damp that preserves the yaw you were carrying and scales the roll damp
    //       by HOW ROLLED the car already was as it left the ground (a car that took off level gets
    //       damped hard; a car that took off sideways is left alone so a deliberate barrel roll
    //       survives), then snapshots the resulting pitch/yaw/roll rates into mPitchYawRollFromTakeOff;
    //   (2) a per-frame LANDING ASSIST that, once you have rolled past a threshold and come back
    //       towards level, lets the player's steering-against-the-roll bleed the roll off;
    //   (3) a per-frame RESTORING TORQUE that pitches/rolls the body towards its own velocity
    //       vector (climb) or bleeds the corresponding angular component out of the angular
    //       velocity AND the two pending accumulators (dive).
    //
    // The banner that stood here called it BLOCKED on two
    // grounds, and BOTH were wrong:
    //   * "the pitch-damp RATIONAL interpolation uses vrefp+Newton segment slopes whose exact
    //     polynomial form is not algebraically pinned" -- it is pinned exactly. Every `vrefp` here
    //     is followed by the compiler's standard TWO Newton-Raphson refinement steps
    //     (`e1 = e + e*(1 - b*e)` twice), i.e. it is a plain `1.0f / b` and nothing else. The
    //     "segment slopes" are an ordinary two-segment linear ramp (see the piecewise below).
    //   * "EVERY damping constant is un-homed .rdata absent from the export" -- every one of the
    //     SIXTEEN constants is IMAGE-READ below, zero flagged. The eight that read as 0 in a raw
    //     .bss dump are seeded by the init-thunk bank @0x82C5C4F0..0x82C5C95C, which copies each
    //     from a named rdata float and vspltw-splats it; the thunk for each is cited inline.
    //
    // ARGUMENTS (asm-confirmed): r3 = this, r4 = lpControls (only `lfs f12, 0x10(r4)` ==
    // mfSteering is read), v1 = lvfTimeStep. PPC float-ABI note for the verifier: the timestep
    // arrives in a VECTOR register (v1, stashed to v122 at 0x825D0C08), NOT in f1 and NOT in a GPR
    // slot -- Hex-Rays' `double a9` in the exported prototype is an artifact.
    //
    // FRAME NOTE: r22/r23/r20 are this+0x10/+0x20/+0x30 == mTransform's xAxis/yAxis/zAxis rows
    // (the ExternallySimulatedBody sub-object sits at VehiclePhysics+0x10), r27 = +0x50 ==
    // mLinearVelocity, r21 = +0x60 == mAngularVelocity, +0xE0 == mfMass, +0x100 == mTotalTorque,
    // +0x120 == mTotalAngularImpulse. `mr r3, r22` at both call sites is the ExternalPhysicsBody
    // `this`, which is why the two callees are reached as plain inherited member calls here.
    //
    // STAGE MAP:
    //   0x825D0C0C  GATE: if (!mbHasAir) { mPitchYawRollFromTakeOff = 0; mbRollingInAir = 0; return; }
    //   0x825D0C44  lfRollVelocity = dot3(mAngularVelocity, zAxis);
    //               lfYawVelocity  = dot3(mAngularVelocity, yAxis);        [both BEFORE the branch]
    //   0x825D0C64  branch on mbHadAirLastFrame:
    //     0x825D0C68  A) THE TAKE-OFF FRAME (flag clear) -- the one-shot damp + snapshot.
    //     0x825D1208  B) ALREADY AIRBORNE  (flag set)    -- landing assist + roll-limit bleed.
    //   0x825D1494  SHARED TAIL: the restoring-torque / angular-bleed pair, then
    //               mWheelFFSpring.mfSpringCoefficient = 0 on every path.
    //
    // ⭐ CORROBORATED 2026-09-03 (drive-spine 1:1 audit): all fourteen cited constants re-read out
    // of the image with tools/re/x360rd.py by a second wave with a different tool path, and every
    // one matched the value written above -- the .data run 0x82F2A258..0x82F2A274
    // (0.14 / 1.4 / 0.3 / 11.0 / 0.15 / 0.125 / 0.25 / 0.3) and the five rdata sources the .bss
    // splat thunks copy (0.05 / 0.01 / 60.0 / 0.0 / 1.0 / 10.0). The "every constant is un-homed"
    // claim this banner already retired stays retired; nothing here is flagged.
    // =====================================================================================
    // The two-vsel sign ladder the X360 emits for `sign(x)` (`vcmpgtfp`+`vsel`, then
    // `vcmpgefp`+`vsel` against -1.0): +1 above zero, 0 AT zero, -1 below (and -1 for NaN).
    // Used verbatim in four places below; not a std::copysign (which has no zero case).
    static inline VecFloat InAirSelectSign(VecFloat lvfValue)
    {
        const f32 lfPositive = (lvfValue.x > 0.0f) ? 1.0f : 0.0f;   // vcmpgtfp . vsel
        const f32 lfSign = (lvfValue.x >= 0.0f) ? lfPositive : -1.0f; // vcmpgefp . vsel
        return vpu::Splat(lfSign);
    }

    // @0x82FB7E20, written at 0x825D0E34 (`stfs f29, kfRollDampingUsed@l`). A DEV WATCH ONLY:
    // VehicleManagerDebugComponent::OnActivate @0x825B5D90 registers this exact address with
    // CgsDev::DebugComponent::RegisterVariable under the label "Roll damping used". Nothing in the
    // sim reads it. Kept at namespace scope (not function-static) so that component's own
    // reconstruction can `extern` it rather than re-inventing a second copy.
    // FLAG (name): the console symbol is `kfRollDampingUsed`; the `k` prefix is the X360 export's,
    // not the source's -- the variable is mutable, so it is spelled `g`-prefixed here per
    // CXX_NAMING_CONVENTIONS. The PS3 DWARF's nearest candidate is `msfTakeoffRollDamping`
    // (VehiclePhysics.cpp:163); NOT adopted, because that mapping is inference, not attestation.
    f32 gfRollDampingUsed = 0.0f;

    void VehiclePhysics::UpdateInAirBehaviour(const BrnPlayerDriverControls* lpControls,
                                              VecFloat lvfTimeStep)
    {
        // ----- every constant image-read; NONE guessed, NONE flagged --------------------------
        // .data tunables (a contiguous run at 0x82F2A258..0x82F2A274, image-read as
        // 0.14/1.4/0.3/11.0/0.15/0.125/0.25/0.3; the PS3 DWARF names each as
        // a file-scope `static float32_t` debug tunable, cited per line):
        static const f32 KF_X_AXIS_Y_FULL_DAMP_THRESHOLD = 0.125f;      // kfFullDampThreshold @0x82F2A26C (0x3E000000); DWARF msfXAxisYFullDampThreshold (:166)
        static const f32 KF_X_AXIS_Y_NO_DAMP_THRESHOLD   = 0.25f;       // kfNoDampThreshold   @0x82F2A270 (0x3E800000); DWARF msfXAxisYNoDampThreshold  (:167)
        static const f32 KF_MIN_ROLL_FACTOR              = 0.30000001f; // kfMinRollFactor     @0x82F2A274 (0x3E99999A); DWARF msfMinRollFactor          (:168)
        static const f32 KF_MIN_ROLL_TO_ALLOW_CORRECTION = 0.30000001f; // kfMinRollToAllowCorrection @0x82F2A260 (0x3E99999A); DWARF msfInAirMinRollToAllowCorrection (:154)
        static const f32 KF_LANDING_ASSIST_DAMPING       = 0.14f;       // kfLandingAssistDamping     @0x82F2A258 (0x3E0F5C29); DWARF msfInAirLandingAssistDamping    (:148)
        // .bss splats, each seeded once by the init-thunk bank (thunk address cited):
        static const f32 KF_YAW_DAMP_TARGET   = 0.050000001f;  // kYawDamp_Target @0x82FB8FF0 <- flt_820047C8 0.05 (thunk 0x82C5C938)
        static const f32 KF_YAW_DAMP_MIN      = 0.0099999998f; // kYawDamp_Min    @0x82FB91B0 <- flt_82002138 0.01 (thunk 0x82C5C910)
        static const f32 KF_DAMP_BLEND_RATE   = 60.0f;         // kDamp_BlendRate @0x82FB8470 <- flt_82092BC4 60.0 (thunk 0x82C5C4F0)
        static const f32 KF_ZERO              = 0.0f;          // unk_82FB9310    <- flt_82001CC0 0.0  (thunk 0x82C5C848)
        static const f32 KF_ALIGN_TORQUE_SCALE= 1.0f;          // unk_82FB8B60    <- flt_82001C98 1.0  (thunk 0x82C5C870)
        static const f32 KF_RATE_TORQUE_SCALE = 10.0f;         // unk_82FB9330    <- flt_82004A20 10.0 (thunk 0x82C5C898)
        static const f32 KF_ANG_BLEED_TIME    = 0.1f;          // unk_82FB9230    <- flt_82004014 0.1  (thunk 0x82C5C8C0)
        static const f32 KF_TAKEOFF_ROLL_GATE = 0.25f;         // unk_82FB9190    <- flt_8208F834 0.25 (thunk 0x82C5C8E8)
        // plain rdata:
        static const f32 KF_REVS_TO_HALF_TURNS = 2.0f;         // flt_82001D9C
        static const f32 KF_PI                 = 3.1415927f;   // flt_8208F5FC (0x40490FDB)
        static const f32 KF_DAMP_RATE_SCALE    = 60.0f;        // flt_82092BC4 (path B's own copy of 60)

        // ----- GATE (0x825D0C0C). On the ground this is the whole function -- which is exactly why
        //       the trap this replaces flooded the assert channel: it fires per driving car per
        //       frame even though the console body does nothing here. -----
        if (!mbHasAir)
        {
            mPitchYawRollFromTakeOff = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // `stvx128 v0, r30, 0x13C0`
            mbRollingInAir           = false;                               // `stb r10, 0x13D8`
            return;
        }

        const Vector3& lvRight = mTransform.Right();   // xAxis, base +0x00 (this +0x10)
        const Vector3& lvUp    = mTransform.Up();      // yAxis, base +0x10 (this +0x20)
        const Vector3& lvAt    = mTransform.At();      // zAxis, base +0x20 (this +0x30)

        // DecFIGS types all three attitude rates as VecFloat. The two initial dots are computed
        // before the mbHadAirLastFrame branch and stashed as full broadcast registers
        // (vmsum3fp128 + stvx128 @0x825D0C44-0x825D0C60), not scalar f32 locals.
        VecFloat lvfPitchVelocity;
        VecFloat lvfYawVelocity  = vpu::Splat(vpu::Dot(mAngularVelocity, lvUp)); // rotation about UP
        VecFloat lvfRollVelocity = vpu::Splat(vpu::Dot(mAngularVelocity, lvAt)); // rotation about FORWARD

        if (!mbHadAirLastFrame)
        {
            // =============================================================================
            // A) THE TAKE-OFF FRAME (0x825D0C68). Runs exactly once per jump.
            // =============================================================================
            const VehicleAttribs::VehicleBaseAttribs& lrBaseAttribs = mpAttribs->mBaseAttribs;
            const VecFloat lvfPitchDamping = lrBaseAttribs.GetPitchDampingOnTakeOff();

            // How rolled the car is at the instant it leaves the ground: |right.y| is 0 when the
            // car is level and 1 when it is on its side.
            const VecFloat lvfAbsXAxisY = vpu::Splat(std::fabs(lvRight.y)); // `vandc` with sign-mask splat
            const VecFloat lvfXAxisYFullDampThreshold = vpu::Splat(KF_X_AXIS_Y_FULL_DAMP_THRESHOLD);
            const VecFloat lvfXAxisYNoDampThreshold   = vpu::Splat(KF_X_AXIS_Y_NO_DAMP_THRESHOLD);
            const VecFloat lvfMinRollFactor           = vpu::Splat(KF_MIN_ROLL_FACTOR);
            const VecFloat lvfOne                     = vpu::GetVector4_One();

            // The two-segment ramp (0x825D0CD4 / 0x825D0D1C / 0x825D0D98). Level -> ramp 0 .. 0.3
            // over [0, 0.125]; then 0.3 .. 1.0 over [0.125, 0.25]; then flat 1.0.
            // The asm re-tests `rollAmount > FULL` before the second segment; that test is the
            // exact complement of the first branch (its only effect is to route NaN to the 1.0
            // leg), so it is folded into the else-if chain here.
            VecFloat lvfRollFactor;
            if (lvfXAxisYFullDampThreshold.x >= lvfAbsXAxisY.x)
            {
                const VecFloat lvfInterpParam = vpu::Splat(
                    lvfAbsXAxisY.x / lvfXAxisYFullDampThreshold.x);
                lvfRollFactor = lvfMinRollFactor * lvfInterpParam;
            }
            else if (lvfXAxisYNoDampThreshold.x >= lvfAbsXAxisY.x)
            {
                const VecFloat lvfInterpParam = vpu::Splat(
                    (lvfAbsXAxisY.x - lvfXAxisYFullDampThreshold.x)
                    / (lvfXAxisYNoDampThreshold.x - lvfXAxisYFullDampThreshold.x));
                lvfRollFactor = lvfMinRollFactor + (lvfOne - lvfMinRollFactor) * lvfInterpParam;
            }
            else
            {
                lvfRollFactor = lvfOne;
            }

            // The yaw damping the ALREADY-AIRBORNE leg's roll-limit bleed will use for the rest of the
            // jump (it is parked in the member and re-read every later frame):
            // lerp Target -> Min by the roll factor, pre-multiplied by the blend rate and dt.
            // Only lane .x is written (`vrlimi128 v12, v13, 8, 0` -- the other three timers survive).
            const VecFloat lBaseDampRollVel = vpu::Splat(KF_YAW_DAMP_TARGET)
                + (vpu::Splat(KF_YAW_DAMP_MIN) - vpu::Splat(KF_YAW_DAMP_TARGET)) * lvfRollFactor;
            mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.x =
                (lBaseDampRollVel * vpu::Splat(KF_DAMP_BLEND_RATE) * lvfTimeStep).x;

            // The three damping values handed to the body are genuine VecFloats in DecFIGS. The
            // YAW value is the literal zero splatted in v126. DampPitchYawRoll subtracts
            // axis*dot*pow(damping, 60*dt), so a zero base yields a zero subtraction for positive
            // dt: it PRESERVES yaw on take-off; it does not annihilate it.
            const VecFloat lvfYawDamping  = vpu::Splat(0.0f); // v126 -- literal, not the yaw attrib
            const VecFloat lvfRollDamping = lvfOne - lvfRollFactor;

            gfRollDampingUsed = lvfRollDamping.x; // dev watch "Roll damping used" (@0x82FB7E20)

            // The three console tripwires, in order, with their own line numbers. Each is the
            // `x >= 0.0f && x <= 1.0f` shape built out of flt_82001CC0 (0.0) and flt_82001C98 (1.0).
            // The second and third stream their values through CgsDev::StrStream over
            // gpcMessageBuffer -- lowered to the literal message per house style; the streamed
            // operands are noted so the text still reads as the console's.
            CGS_ASSERT(lvfPitchDamping.x >= 0.0f && lvfPitchDamping.x <= 1.0f,
                       "lvfPitchDamping >= 0.0f && lvfPitchDamping <= 1.0f");            // :2497 (0x9C1)
            CGS_ASSERT(lvfYawDamping.x >= 0.0f && lvfYawDamping.x <= 1.0f,
                       "Excessive yaw damping on take-off: YawVelocity = ");             // :2498 (0x9C2)
                       // streams: lvfYawVelocity, ", MinYawDamping = " GetYawDampingOnTakeOff(), ", YawDamping = " lvfYawDamping, "\n"
            CGS_ASSERT(lvfRollDamping.x >= 0.0f && lvfRollDamping.x <= 1.0f,
                       "Excessive roll damping on take-off: RollVelocity = ");           // :2499 (0x9C3)
                       // streams: lvfRollVelocity, ", MinRollDamping = " GetRollDampingOnTakeOff(), ", RollDamping = " lvfRollDamping, "\n"
            (void)lvfYawVelocity;   // read only by the (lowered) assert stream above

            DampPitchYawRoll(lvfPitchDamping, lvfYawDamping, lvfRollDamping, lvfTimeStep);

            // ----- snapshot the POST-damp attitude rates (0x825D10E8) -----
            // The roll limit is an attrib in TURNS; x2 x PI converts it to rad/s.
            const VecFloat lvfMaxRollVelocity = lrBaseAttribs.GetRollLimitOnTakeOff()
                * vpu::Splat(KF_REVS_TO_HALF_TURNS) * vpu::Splat(KF_PI);

            lvfPitchVelocity = vpu::Splat(vpu::Dot(mAngularVelocity, lvRight));
            lvfYawVelocity   = vpu::Splat(vpu::Dot(mAngularVelocity, lvUp));
            lvfRollVelocity  = vpu::Splat(vpu::Dot(mAngularVelocity, lvAt));

            if (std::fabs(lvfRollVelocity.x) > lvfMaxRollVelocity.x)
            {
                // Clamp the take-off roll rate by removing the excess along the forward axis.
                const VecFloat lvfClamped = lvfMaxRollVelocity * InAirSelectSign(lvfRollVelocity);
                mAngularVelocity = mAngularVelocity - lvAt * (lvfRollVelocity - lvfClamped);
                lvfRollVelocity  = lvfClamped;
            }

            // Three separate lane inserts (`vrlimi128` masks 8/4/2) with a store after each --
            // the .w lane is deliberately left as it was.
            mPitchYawRollFromTakeOff.x = lvfPitchVelocity.x;
            mPitchYawRollFromTakeOff.y = lvfYawVelocity.x;
            mPitchYawRollFromTakeOff.z = lvfRollVelocity.x;
        }
        else
        {
            // =============================================================================
            // B) ALREADY AIRBORNE (0x825D1208).
            // =============================================================================
            const VecFloat lvfXAxisY = vpu::Splat(std::fabs(lvRight.y));
            const VecFloat lvfMinRollToAllowCorrection =
                vpu::Splat(KF_MIN_ROLL_TO_ALLOW_CORRECTION);

            // Latch "the player has rolled this jump" once |right.y| passes the threshold...
            if (lvfXAxisY.x > lvfMinRollToAllowCorrection.x)
                mbRollingInAir = true;

            // ...and only assist once the car has rolled BACK inside it, is the right way up, and
            // the player is steering AGAINST the roll (0x825D1240..0x825D1340). Three nested gates
            // in the asm, each an early exit to the roll-limit bleed below.
            if (mbRollingInAir
                && lvfMinRollToAllowCorrection.x > lvfXAxisY.x
                && lvUp.y > 0.0f)
            {
                const f32 lfSteering = lpControls->mfSteering;   // `lfs f12, 0x10(r4)`

                // `fcmpu` against 0.0 first, then `fsel f0, f12, 1.0, -1.0` -- so a dead stick
                // yields 0.0, which never matches the +-1 roll sign and therefore still assists.
                const f32 lfSteerSign = (lfSteering == 0.0f)
                                            ? 0.0f
                                            : ((lfSteering >= 0.0f) ? 1.0f : -1.0f);

                if (InAirSelectSign(lvfRollVelocity).x != lfSteerSign)
                {
                    const VecFloat lvfAssist = vpu::Splat(
                        std::fabs(lfSteering) * KF_LANDING_ASSIST_DAMPING)
                        * lvfTimeStep * vpu::Splat(KF_DAMP_RATE_SCALE);
                    mAngularVelocity = mAngularVelocity - lvAt * (lvfRollVelocity * lvfAssist);
                }
            }

            // ----- roll-limit bleed (0x825D13B0). While the car is STILL rolling the same way it
            //       was at take-off, bleed the roll rate off at the per-frame rate the take-off
            //       frame parked in mvDampRollVel.x, never overshooting past zero. -----
            const VecFloat lvfCurrentRoll = vpu::Splat(vpu::Dot(mAngularVelocity, lvAt));
            if (InAirSelectSign(vpu::Splat(mPitchYawRollFromTakeOff.z)).x
                == InAirSelectSign(lvfCurrentRoll).x)
            {
                const VecFloat lvfRate = vpu::Splat(
                    mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.x)
                    * lvfTimeStep * vpu::Splat(KF_DAMP_RATE_SCALE);
                const VecFloat lvfStep = vpu::Splat(
                    std::min(lvfRate.x, std::fabs(lvfCurrentRoll.x))) // vminfp
                    * InAirSelectSign(lvfCurrentRoll);
                mAngularVelocity = mAngularVelocity - lvAt * lvfStep;
            }
        }

        // =================================================================================
        // SHARED TAIL (0x825D1494) -- the restoring-torque / angular-bleed pair.
        // Two independent axes, each with the same climb-vs-dive shape:
        //   CLIMBING (the velocity component along that body axis points UP): add a torque that
        //     rotates the body towards its own velocity vector, minus a rate-proportional brake.
        //   DIVING: instead bleed that axis's component out of mAngularVelocity AND out of the two
        //     pending accumulators (mTotalTorque, mTotalAngularImpulse) -- the console damps the
        //     PENDING torque/impulse too, so a force already banked this frame cannot re-inject
        //     the rotation the bleed just removed.
        // =================================================================================
        const VecFloat lvLinearVelocityDotZAxis = vpu::Splat(vpu::Dot(mLinearVelocity, lvAt));
        const VecFloat lvLinearVelocityDotXAxis = vpu::Splat(vpu::Dot(mLinearVelocity, lvRight));
        const Vector3 lvLinearVelocityZAxis = lvAt * lvLinearVelocityDotZAxis;       // var_120
        const Vector3 lvLinearVelocityXAxis = lvRight * lvLinearVelocityDotXAxis;    // var_110

        // ----- PITCH axis (about the body X/right axis) -----
        if (lvLinearVelocityZAxis.y > KF_ZERO)
        {
            // `vcsxwfp128 v11, v123, 0` converts the all-ones splat to -1.0f: the alignment target
            // flips with the direction of travel so a car flying backwards is not pitched over.
            const Vector3 lvLinearVelocityZAxisDir =
                (lvLinearVelocityDotZAxis.x >= 0.0f) ? lvAt : lvAt * -1.0f;
            const Vector3 lLinearVelocityUp = mLinearVelocity - lvLinearVelocityXAxis;
            // cross(velInPlane, right) . +-at -- the console's `vpermwi128 0x63` + `vnmsubfp` +
            // `vpermwi128 0x63` cross-product idiom (0x63 == the yzx word permute).
            const VecFloat lvfAlignment = vpu::Splat(
                vpu::Dot(vpu::Cross(lLinearVelocityUp, lvRight), lvLinearVelocityZAxisDir));
            const VecFloat lvfPitchVelocity = vpu::Splat(vpu::Dot(mAngularVelocity, lvRight));

            // mfMass is itself a VecFloat (DecFIGS ExternalPhysicsBody.h:93); retain that
            // broadcast flow through the two vector products instead of scalarising mfMass.x.
            const Vector3 lTorque = lvRight * lvfAlignment
                * (mfMass * vpu::Splat(KF_ALIGN_TORQUE_SCALE));
            const Vector3 lTorqueDamp = lvRight * lvfPitchVelocity
                * (mfMass * vpu::Splat(KF_RATE_TORQUE_SCALE));
            AddWorldSpaceTorque(lTorque - lTorqueDamp);
        }
        else
        {
            // dt / 0.1 -- the console spells the reciprocal as vrefp + two Newton steps.
            const VecFloat lvfValue = vpu::Splat(KF_ANG_BLEED_TIME);
            const VecFloat lvfAirDampWhenTilted =
                vpu::Splat(lvfTimeStep.x / lvfValue.x);

            const VecFloat lvfPitchVelocity = vpu::Splat(vpu::Dot(lvRight, mAngularVelocity));
            if ((lvfPitchVelocity * lvLinearVelocityDotZAxis).x > 0.0f)
                mAngularVelocity = mAngularVelocity - lvRight * (lvfPitchVelocity * lvfAirDampWhenTilted);

            const VecFloat lvfPitchTorque = vpu::Splat(vpu::Dot(lvRight, mTotalTorque));
            if ((lvfPitchTorque * lvLinearVelocityDotZAxis).x > 0.0f)
                mTotalTorque = mTotalTorque - lvRight * (lvfPitchTorque * lvfAirDampWhenTilted);

            const VecFloat lvfPitchImpulse = vpu::Splat(vpu::Dot(lvRight, mTotalAngularImpulse));
            if ((lvfPitchImpulse * lvLinearVelocityDotZAxis).x > 0.0f)
                mTotalAngularImpulse = mTotalAngularImpulse
                    - lvRight * (lvfPitchImpulse * lvfAirDampWhenTilted);
        }

        // ----- ROLL axis (about the body Z/forward axis), gated on the take-off roll snapshot.
        //       A jump that left the ground already spinning hard about Z (|snapshot| >= 0.25) is
        //       left alone entirely -- that is the deliberate barrel roll. -----
        if (KF_TAKEOFF_ROLL_GATE > std::fabs(mPitchYawRollFromTakeOff.z))
        {
            if (lvLinearVelocityXAxis.y > KF_ZERO)
            {
                const Vector3 lvLinearVelocityXAxisDir =
                    (lvLinearVelocityDotXAxis.x >= 0.0f) ? lvRight : lvRight * -1.0f;
                const Vector3 lLinearVelocityUp = mLinearVelocity - lvLinearVelocityZAxis;
                const VecFloat lvfRollVelocity = vpu::Splat(vpu::Dot(mAngularVelocity, lvAt));
                const VecFloat lvfAlignment = vpu::Splat(
                    vpu::Dot(vpu::Cross(lLinearVelocityUp, lvAt), lvLinearVelocityXAxisDir));

                const Vector3 lTorque = lvAt * lvfAlignment
                    * (mfMass * vpu::Splat(KF_ALIGN_TORQUE_SCALE));
                const Vector3 lTorqueDamp = lvAt * lvfRollVelocity
                    * (mfMass * vpu::Splat(KF_RATE_TORQUE_SCALE));
                AddWorldSpaceTorque(lTorque - lTorqueDamp);
            }
            else
            {
                const VecFloat lvfValue = vpu::Splat(KF_ANG_BLEED_TIME);
                const VecFloat lvfAirDampWhenTilted =
                    vpu::Splat(lvfTimeStep.x / lvfValue.x);

                // NOTE THE ASYMMETRY, IT IS THE CONSOLE'S: the roll bleed's three gates test the
                // roll component against lvLinearVelocityDotZAxis (v125), not the X-axis dot.
                const VecFloat lvfRollVelocity = vpu::Splat(vpu::Dot(lvAt, mAngularVelocity));
                if ((lvfRollVelocity * lvLinearVelocityDotZAxis).x > 0.0f)
                    mAngularVelocity = mAngularVelocity
                        - lvAt * (lvfRollVelocity * lvfAirDampWhenTilted);

                const VecFloat lvfRollTorque = vpu::Splat(vpu::Dot(lvAt, mTotalTorque));
                if ((lvfRollTorque * lvLinearVelocityDotZAxis).x > 0.0f)
                    mTotalTorque = mTotalTorque
                        - lvAt * (lvfRollTorque * lvfAirDampWhenTilted);

                const VecFloat lvfRollImpulse = vpu::Splat(vpu::Dot(lvAt, mTotalAngularImpulse));
                if ((lvfRollImpulse * lvLinearVelocityDotZAxis).x > 0.0f)
                    mTotalAngularImpulse = mTotalAngularImpulse
                        - lvAt * (lvfRollImpulse * lvfAirDampWhenTilted);
            }
        }

        // `stfs f30, 0x13D0(r30)` -- reached on EVERY tail path (including the take-off-roll-gated
        // skip): no force-feedback wheel spring while the car is in the air.
        mWheelFFSpring.mfSpringCoefficient = 0.0f;
    }

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
    // as a flagged-0 placeholder. kfMaxWheelieAngle / kfWheelieLimitDamping are image-pinned at
    // 0x82F2A264/0x82F2A268 as 11.0f and 0.15f respectively.
    // unk_82FB8A90 (the speed-match clamp) is NO LONGER a placeholder -- it is 50.0f; see the note
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
    void VehiclePhysics::UpdateBoost(BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep)
    {
        const f32 lfTimeStep = lvfTimeStep.x;
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
            // RESOLVED 2026-08-03. The X360 reads controls+0x34 (`lfs f0, 0x34(r4)` @0x825FAD98).
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
                    ApplyBoostKickForce(lvfTimeStep);
                else
                    ApplyNormalBoostForce(lvfTimeStep);

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
    void VehiclePhysics::ApplyNormalBoostForce(VecFloat lvfTimeStep)
    {
        const f32 lfTimeStep = lvfTimeStep.x;
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
    //   Breaker loads kfMaxWheelieAngle from 0x82F2A264 (11.0f) and
    //   kfWheelieLimitDamping from 0x82F2A268 (0.15f). flt_8208F5F4 is the
    //   0.017453292 degrees-to-radians factor.
    void VehiclePhysics::ApplyBoostKickForce(VecFloat lvfTimeStep)
    {
        static const f32 KF_DEG_TO_RAD         = 0.017453292f;   // flt_8208F5F4 (deg->rad, findings)
        static const f32 KF_MAX_WHEELIE_ANGLE  = 11.0f;          // kfMaxWheelieAngle @0x82F2A264
        static const f32 KF_WHEELIE_LIMIT_DAMP = 0.15f;          // kfWheelieLimitDamping @0x82F2A268
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
        (void)lvfTimeStep;   // this applier advances no timer with it.
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
    // RESOLVED 2026-08-03 (both of this body's two flags).
    //   (1) CONTROL OFFSETS. +0x44/+0x48/+0x4C are not "past the layout" -- +0x44 is meDriverType and
    //   the other two are BrnAIDriverControls::mfSpeedMatchSpeed / mbDoSpeedMatch. The console is
    //   doing exactly what it looks like: check the driver type, then read the AI payload. The DWARF
    //   NAMES the two AI members after this very function, which is what settles it beyond offsets.
    //   The raw byte view is gone.
    //   (2) THE CLAMP BOUND. unk_82FB8A90 is a .data slot that reads zero in the image; it is filled
    //   at static-init time by an unexported initialiser at 0x82C5CB28 that splats the .rdata scalar
    //   flt_820138DC == 50.0f. So the bound is 50, not 0 -- and with the placeholder 0 the clamped
    //   delta was identically zero, i.e. the speed-match nudge did NOTHING.
    void VehiclePhysics::UpdateSpeedMatch(const BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep)
    {
        const f32 lfTimeStep = lvfTimeStep.x;
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

        // Clamp the delta to +/-(clampVec * dt). RESOLVED 2026-08-03: unk_82FB8A90 is zero in the
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
    // Historical note: this block originally described several constants as un-homed zero
    // placeholders. Their values and consumers are now pinned below from Breaker image data/static
    // initialisers. In particular, unk_82014AC0..AF0 are the PPC compiler's inlined Pow
    // approximation coefficients, not source-level steering/drift tuning data.
    // =====================================================================================

    // File-static steering/drift gains, recovered from Breaker image data and initialiser thunks.
    // stru_8208F620 is plain readable .rdata and holds 1.1920929e-07 -- FLT_EPSILON. This is the one
    //   placeholder in this file whose zero really WAS harmless: a zero-vs-epsilon guard on a magnitude
    //   differs only for denormal inputs. Recording it as an honest negative rather than quietly
    //   "fixing" it, because the standing rule here is that a 0.0f placeholder is never inert -- and
    //   this is the documented exception, not a counter-example to the rule.
    static const f32  KF_DRIFT_STEER_EPSILON       = 1.1920929e-07f;  // stru_8208F620 == FLT_EPSILON
    // unk_82FB9020 = 0.785398185, and its NAME and VALUE are both confirmed by its initialiser:
    //   @0x82C5CA80 computes flt_82009B80 (45.0) * flt_8208F5F4 (0.0174532924 = deg->rad). It is
    //   literally 45 degrees in radians. GetSteeringAngle @0x825D4150 then loads it, XORs the sign
    //   mask and `vmaxfp`s against the negation -- a symmetric +/-45 degree clamp. At 0.0f the drift
    //   steering angle was left entirely unclamped (+/-pi instead of +/-pi/4).
    static const f32  KF_STEER_ANGLE_CLAMP         = 0.785398185f;    // unk_82FB9020 = 45deg in rad
    static const f32  KF_WHEEL_STEER_BLEND         = 0.05f;           // unk_82FB9370 <- flt_820047C8 (splat)
    // THREE OF THE "un-homed" PLACEHOLDERS ABOVE ARE NOW RECOVERED (2026-08-03). All three read
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
    // 0x82004F5C block). VALUE PROVED, ROLE NOT: nothing in this TU reads this constant today, so
    // the "drift-scale grow clamp" name is still the tree's prior guess and cannot be checked against
    // a consumer. Seated so the number stops being a lie; the NAME stays suspect.
    static const f32  KF_DRIFT_SCALE_GROW_LIMIT    = 90.0f;          // unk_82FB80F0 <- flt_82004F64
    static const f32  KF_HANDBRAKE_TIME_CAP        = 10000.0f;       // unk_82FB9080 <- flt_82005D9C (splat)
    static const f32  KF_HANDBRAKE_ONTIME_RELEASE  = 0.275f;         // unk_82FB8B00 <- flt_8209D720 (splat)
    static const f32  KF_HANDBRAKE_STOPPED_EPSILON = 1.1920928955078125e-07f; // stru_8208F620 lane 0 == FLT_EPSILON (UpdateHandBrake's "car has stopped" release arm, 0x825CFB0C)
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
    VecFloat VehiclePhysics::GetSteeringAngle() const
    {
        // cached lane (.x of the steering register @+0xFE0).
        const f32 lfCached = mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x;

        // NOT drifting -> the cached value (the asm's LABEL_6 fast path).
        if (mu8DriftState == eDriftState_None)
            return VecFloat{ lfCached, lfCached, lfCached, lfCached };

        // 0x825D4064..0x825D4094 compares every absolute xyz velocity lane with epsilon and takes
        // the cached path only when all three comparisons are false.  It is not a test of y alone.
        const f32 lfMaxAbsVelocity =
            std::max(std::fabs(mLinearVelocity.x),
                     std::max(std::fabs(mLinearVelocity.y), std::fabs(mLinearVelocity.z)));
        if (!(lfMaxAbsVelocity > KF_DRIFT_STEER_EPSILON))
            return VecFloat{ lfCached, lfCached, lfCached, lfCached };

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

        // Breaker 0x825D4138..0x825D4170 clamps abs(DriftScale) to [0,1], then performs
        // angle*s + cached*(1-s).  The previous implementation dropped the cached term and reversed
        // the angle's weight.
        f32 lfDriftScale = std::fabs(
            mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w);
        if (lfDriftScale > 1.0f)
            lfDriftScale = 1.0f;
        const f32 lfBlendedUnclamped =
            lfAngle * lfDriftScale + lfCached * (1.0f - lfDriftScale);
        f32 lfBlended = lfBlendedUnclamped;

        // final symmetric clamp to +-KF_STEER_ANGLE_CLAMP. The `> 0.0f` gate that used to wrap this
        // existed only so a flagged zero would not collapse the angle; the clamp is real now.
        if (lfBlended >  KF_STEER_ANGLE_CLAMP) lfBlended =  KF_STEER_ANGLE_CLAMP;
        if (lfBlended < -KF_STEER_ANGLE_CLAMP) lfBlended = -KF_STEER_ANGLE_CLAMP;
        return VecFloat{ lfBlended, lfBlended, lfBlended, lfBlended };
    }

    // @0x825D34D8  VehiclePhysics::GetMaxSteeringAngleDuringDrift
    //   Signed drift steering target in radians. Breaker uses the live cached steering magnitude,
    //   the angle between normalized velocity and body forward, a 1.5 scale, and the attrib max-angle
    //   cap; body-right dot supplies the sign. The DecFIGS f32 parameter is optimized out on X360.
    f32 VehiclePhysics::GetMaxSteeringAngleDuringDrift(f32 /*lfSteeringInput*/)
    {
        // Breaker 0x825D350C..0x825D3570: normalize the linear velocity with the standard
        // zero-select around the vrsqrtefp/Newton chain. DecFIGS retains a trailing f32 parameter,
        // but the X360 body never reads f1; it reads the live steering lane at this+0xFE0.y.
        const Vector3 lvUnitVelocity = vpu::Normalize(mLinearVelocity);

        // 0x825D35F8..0x825D3634: acos(clamp(dot(unitVel, forward), -1, 1)). The lower
        // clamp is -1, not zero, so reverse travel can produce the full pi-angle response.
        f32 lfForwardDot = vpu::Dot(lvUnitVelocity, mTransform.zAxis);
        if (lfForwardDot < -1.0f) lfForwardDot = -1.0f;
        if (lfForwardDot >  1.0f) lfForwardDot =  1.0f;
        const f32 lfVelocityAngle = std::acos(lfForwardDot);   // XMVectorACos

        // 0x825D3644..0x825D36D4: scale by |Steering| and 1.5, cap at the attrib max
        // angle converted to radians, then take the sign from dot(unitVel, transform X).
        static const f32 KF_DRIFT_STEERING_ANGLE_SCALE = 1.5f;   // flt_820945DC
        const f32 lfSteeringMagnitude =
            std::fabs(mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y);
        const f32 lfMaxAngle =
            mpAttribs->mSteeringAttribs.mvMaxAngle_StraightReactionBias.x * KF_DEG_TO_RAD;
        f32 lfResult = std::min(
            lfVelocityAngle * lfSteeringMagnitude * KF_DRIFT_STEERING_ANGLE_SCALE,
            lfMaxAngle);
        if (vpu::Dot(lvUnitVelocity, mTransform.xAxis) < 0.0f)
            lfResult = -lfResult;
        return lfResult;
    }

    // @0x825CFB70  VehiclePhysics::ModifyControlsForSteeringWheelInput
    //   Breaker 0x825CFB70..0x825CFC60: apply the quartic wheel curve
    //     clamp(sign(s) * 1.25 * s^4, -1, 1),
    //   then damp 5% of the angular-velocity component about transform Y when the resulting
    //   steering and the current yaw rate have the same sign. The caller has already gated this
    //   method on controls+0x41 (mbIsSteeringWheel); the method itself has no second device test.
    void VehiclePhysics::ModifyControlsForSteeringWheelInput(BrnPlayerDriverControls* lpControls)
    {
        BrnPlayerDriverControls& lrControls = *lpControls;
        const f32 lfS = lrControls.mfSteering;   // lfs f0,0x10(r4)
        const f32 lfSign = (lfS > 0.0f) ? 1.0f : ((lfS < 0.0f) ? -1.0f : 0.0f);
        const f32 lfS2 = lfS * lfS;
        const f32 lfS4 = lfS2 * lfS2;
        f32 lfSteering = lfSign * lfS4 * KF_QUARTIC_STIFFEN;
        if (lfSteering < -1.0f) lfSteering = -1.0f;   // fsel @0x825CFBFC
        if (lfSteering >  1.0f) lfSteering =  1.0f;   // fsel @0x825CFC04

        const Vector3& lvUp = mTransform.yAxis;       // lvx128 this+0x20
        const f32 lfYawRate = vpu::Dot(lvUp, mAngularVelocity);
        if (lfYawRate * lfSteering > 0.0f)             // 0x825CFC24..0x825CFC38
        {
            mAngularVelocity = mAngularVelocity
                - lvUp * (lfYawRate * KF_WHEEL_STEER_BLEND);        // unk_82FB9370 = 0.05
        }

        lrControls.mfSteering = lfSteering;             // stfs f0,0x10(r4) @0x825CFC5C
    }

    // @0x825CFC68  VehiclePhysics::ModifyControlsForDrift
    //   While sliding (mu8DriftState != 0) and the ORIGINAL controls mode is NOT 1 (0x825CFC74-7C:
    //   `lwz r10,0x44(r4) ; cmpwi cr6,r10,1 ; beqlr cr6` -- returns when mode==1, i.e. proceeds only
    //   when mode != 1), re-signs and re-maps the steer input by the drift direction. The blend
    //   weights come from mpAttribs->mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale (@+0x120) lanes .z (zLane) and .w (wLane).
    //   asm (0x825CFCC4-825CFD14): sign = (mu8DriftState==1) ? +1 : -1 ; s = sign * steer ;
    //     gas = controls.mfGas (+0x04) ; wGas = wLane * gas ;
    //     steer' = sign * ( (1 - wLane*gas^2) * max(s,0) + (zLane + wGas) * min(s,0) + wGas ).
    void VehiclePhysics::ModifyControlsForDrift(BrnPlayerDriverControls* lpControls)
    {
        BrnPlayerDriverControls& lrControls = *lpControls;
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
    //   ⭐⭐ REWRITTEN LANE-EXACT 2026-08-24 (deform-land wave, drift bundle R1; blindfns audit
    //   leg 3/7, asm 0x825B8220..0x825B82B0 decoded mask-by-mask). The previous body had FIVE
    //   lane errors and one INVENTED store:
    //     wrote +0x1000.x (Spare)          -- console clears +0x1000.w (DriftScale)
    //     wrote +0x1020.w (TimeInFriction) -- console clears +0x1020.x (DesiredDriftAngleScale)
    //     wrote +0x1030.y = 1.0 (PushTime) -- console writes +0x1030.x = 1.0 (vcfsx 1): the
    //                                         LatDriftForceFactor LATCH (its only non-crash writer)
    //     wrote +0x1030.x = 0.0            -- ⛔ THE INVENTED KILL: with it, LatDriftForceFactor
    //                                         is 0 forever after the first drift ends (no other
    //                                         writer until crash/reset), and ApplyDriftLatForce's
    //                                         side force dies. DELETED.
    //     wrote +0x1040.w                  -- console clears +0x1040.x (SideForceMag)
    //     missed +0xEF0.z (CounterSteerSideMag) entirely.
    //   Console store set, in asm order: state=0; 1000.w=0; 1010.z=0; 1020.x=0; 1030.x=1.0;
    //   1030.w=0; 1040.x=0; EF0.z=0; flags=0xFF.
    void VehiclePhysics::ExitDrift()
    {
        mu8DriftState = eDriftState_None;                                                        // stb 0, +0x1352
        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = 0.0f;                          // +0x1000 mask1 (DriftScale)
        mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z = 0.0f;            // +0x1010 mask2 (TimeDrifting)
        mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.x = 0.0f; // +0x1020 mask8
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.x = 1.0f;         // +0x1030 mask8 = vcfsx(1) -- the latch
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.w = 0.0f;         // +0x1030 mask1 (CurrentDriftAngle)
        mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.x = 0.0f;        // +0x1040 mask8
        mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.z = 0.0f;                   // +0xEF0 mask2
        mDriftFlags.mu8DriftFlags = DriftFlags::KU_DRIFT_FLAG_DO_ALL;                            // stb 0xFF, +0x10F4
    }

    // @0x825FA268  VehiclePhysics::EnterDrift
    //   ⭐⭐ REWRITTEN LANE-EXACT 2026-08-24 (deform-land wave, drift bundle R2; blindfns audit
    //   leg 3/7, asm 0x825FA268..0x825FA448 decoded mask-by-mask). The previous body diverged
    //   heavily: it stored +0x1010.y = "lfSlip" (the console NEVER writes +0x1010.y here, and
    //   the second argument IS the entry speed -- the caller CheckForEnteringDrift passes
    //   f1 = lfSpeedMPS, f2 = attribs DriftPushTime), invented +0x1000.z = 0 and the
    //   mDriftFlags = DO_ALL write (only ExitDrift/SetCrashing write 0xFF), and MISSED the
    //   +0x1020.x/.y seeds, the +0x1050.x/.y weights, +0x1030.y/.w, +0xFE0.w, +0x1040.x and
    //   +0xEF0.z. Parameters renamed to the console roles (signature arity unchanged).
    //   Console store set, in asm order:
    //     state = (steer > 0) ? 1 : 2                              (+0x1352)
    //     1020.x = 0.9 + 0.1 * |Steering reg .y|                   (flt_82005450 + flt_82004014)
    //     1020.y = (1.0 > -min(TimeDrifting, 0)) ? 1.0 : attribs+0x120.w (CappedScale)
    //     1000.y = lfSpeed;  1000.w = 0;  1010.z = 0
    //     1050.x = -0.1 (unk_8208FAE4);  1050.y = 1.0 (unk_8208FAE8)
    //     1030.y = lfPushTime;  FE0.w = 0;  1030.w = 0;  1040.x = 0;  EF0.z = 0
    //     NO mDriftFlags write, NO 1030.x write, NO 1000.z write.
    void VehiclePhysics::EnterDrift(const BrnPlayerDriverControls* lpControls, f32 lfSpeed, f32 lfPushTime)
    {
        const f32 lfSteer = lpControls ? lpControls->mfSteering : 0.0f;   // *(a2+16)

        mu8DriftState = (lfSteer > 0.0f) ? eDriftState_FacingLeft : eDriftState_FacingRight;

        // +0x1020.x = 0.9 + 0.1 * |steering register .y| (the smoothed steering lane).
        mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.x =
            0.9f + 0.1f * std::fabs(mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y);

        // +0x1020.y: 1.0 unless the car was ALREADY drifting (TimeDrifting > 1s pre-clear),
        // in which case the per-car capped scale (attribs +0x120 .w).
        {
            const f32 lfTimeDrifting =
                mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z;
            const f32 lfNegMin = -std::min(lfTimeDrifting, 0.0f);
            mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.y =
                (1.0f > lfNegMin)
                    ? 1.0f
                    : mpAttribs->mDriftAttribs
                          .mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.w;
        }

        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y = lfSpeed;                       // +0x1000 mask4 (MaintainedSpeed)
        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = 0.0f;                          // +0x1000 mask1 (DriftScale)
        mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z = 0.0f;            // +0x1010 mask2 (TimeDrifting)
        mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.x = -0.1f; // +0x1050 mask8 (unk_8208FAE4)
        mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.y =  1.0f; // +0x1050 mask4 (unk_8208FAE8)
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.y = lfPushTime;   // +0x1030 mask4
        mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.w = 0.0f;                     // +0xFE0 mask1 (GasLetOff)
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.w = 0.0f;         // +0x1030 mask1 (CurrentDriftAngle)
        mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.x = 0.0f;        // +0x1040 mask8
        mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.z = 0.0f;                   // +0xEF0 mask2
    }

    // @0x825CFA10  VehiclePhysics::UpdateHandBrake
    //   A latch+timer with hysteresis around input 0.1.
    //     * If currently engaged (mbHandBrake): advance TimeHandbrakeHasBeenOn by dt (capped at
    //       KF_HANDBRAKE_TIME_CAP). When input < 0.1, release ONLY if a drift is active (mu8DriftState!=0)
    //       OR the on-time exceeds KF_HANDBRAKE_ONTIME_RELEASE; otherwise reset TimeSinceLastHandBrake.
    //     * If currently released: when input <= 0.1, advance TimeSinceLastHandBrake by dt (capped);
    //       when input > 0.1, engage (mbHandBrake = 1) and clear TimeHandbrakeHasBeenOn.
    //   The two timers live in the +0x1080 lane (TimeHandbrakeHasBeenOn .z / TimeSinceLastHandBrake .w).
    void VehiclePhysics::UpdateHandBrake(VecFloat lvfTimeStep, f32 lfHandBrakeInput)
    {
        const f32 lfTimeStep = lvfTimeStep.x;
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
                // THE THIRD RELEASE ARM (0x825CFAEC..0x825CFB48), MISSING until 2026-09-02: a car
                // that is STANDING STILL releases at once. The console splats |mfSpeedMPH| (+0x6C0,
                // vandc sign mask), compares it against stru_8208F620 lane 0 (FLT_EPSILON,
                // 0x34000000) with a non-recording vcmpgtfp, vperm's lane 0 out and `bnelr`s --
                // i.e. it RETURNS WITHOUT RELEASING only while the car is moving. The old
                // "vperm store branch" reading was wrong: nothing is stored on that path; the
                // vperm is the lane extract of the compare mask.
                const bool lbReleaseByStopped = !(std::fabs(mfSpeedMPH.x) > KF_HANDBRAKE_STOPPED_EPSILON);
                if (lbReleaseByDrift || lbReleaseByOnTime || lbReleaseByStopped)
                {
                    mbHandBrake = false;
                    lrTimers.z = 0.0f;   // TimeHandbrakeHasBeenOn cleared
                    lrTimers.w = 0.0f;   // restart TimeSinceLastHandBrake
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
    //   Breaker consumes v1/v2/v3/v5 as {time step, drift angle, factor, brake}; v4 (gas) is dead.
    //   Once |drift angle| exceeds NaturalYawTorqueCutOffAngle, NaturalDriftDecay*factor*dt moves
    //   DriftScale toward zero without crossing it. NaturalYawTorque*brake is then applied about
    //   the body up axis. The shared {false,true} mask at unk_8327F240 selects the NEGATED torque
    //   for drift state 1 (FacingLeft); every other state keeps it positive.
    void VehiclePhysics::ApplyNaturalDriftForces(VecFloat lvfTimeStep, VecFloat lvfDriftAngle,
                                                  VecFloat lvfFactor, VecFloat lvfGas,
                                                  VecFloat lvfBrake)
    {
        (void)lvfGas;

        const VehicleAttribs::DriftAttribs& lrDriftAttribs = mpAttribs->mDriftAttribs;
        f32& lrfDriftScale = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w;

        const f32 lfAbsDriftAngle = std::fabs(lvfDriftAngle.x);
        const f32 lfAngleForNoDecay =
            lrDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.y;
        if (lfAbsDriftAngle > lfAngleForNoDecay)
        {
            const f32 lfDecayFactor =
                lrDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.z
                * lvfFactor.x * lvfTimeStep.x;

            if (lrfDriftScale > 0.0f)
                lrfDriftScale -= std::min(lfDecayFactor, lrfDriftScale);
            else
                lrfDriftScale += std::min(lfDecayFactor, -lrfDriftScale);
        }

        const f32 lfTorqueMagnitude =
            lrDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.x
            * lvfBrake.x;
        const f32 lfSignedTorque = (mu8DriftState == eDriftState_FacingLeft)
                                       ? -lfTorqueMagnitude
                                       :  lfTorqueMagnitude;
        const Vector3 lTorque{ mTransform.yAxis.x * lfSignedTorque,
                               mTransform.yAxis.y * lfSignedTorque,
                               mTransform.yAxis.z * lfSignedTorque,
                               0.0f };
        AddWorldSpaceTorque(lTorque);
    }

    // @0x825D2270  VehiclePhysics::MaintainDriftSpeed
    //   Keeps a sliding car from scrubbing off speed. Gated by mDriftFlags.DoMaintainSpeed(), the
    //   MaintainedSpeed lane (@+0x1000 .y) exceeding current speed, throttle >= 0.3, grounded
    //   (!mbHandBrake-equivalent + mAboveGroundTestResult.mbValid). Builds a ground-tangent world impulse along
    //   a Z/velocity blend (mvPropSpeedMaintainAlong* @+0x1050) scaled by the per-car push attrib, then
    //   AddWorldSpaceImpulse. The "Invalid total linear impulse during drift" assert is elided.
    void VehiclePhysics::MaintainDriftSpeed(const BrnPlayerDriverControls* lpControls,
                                             Vector3 lvDirection, VecFloat lvfSpeed)
    {
        // ⭐ R3 (deform-land wave 2026-08-24, blindfns audit leg 5) -- three divergences + a
        // micro fixed against the asm @0x825D2270:
        //   D1 the console BAILS when the handbrake is held (0x1358) -- the gate was missing;
        //   D2 the impulse scale is the vehicle MASS (BASE attribs +0x70 .x -- impulse units),
        //      not the DriftAttribs +0x160.y push-time attrib the old body used;
        //   D3 the velocity-direction term is gated on |speedParam| > FLT_EPSILON (skipped at
        //      standstill) -- the old unconditional Normalize was a NaN risk;
        //   micro: the console proceeds on maintained >= speed (the old body used strict >).
        //
        // ⛔ D5 (drift-symmetry wave 2026-09-02, raw words 0x825D2270..0x825D25A0): EVERY failing
        // gate -- the maintain flag (0x825D22A4), speed > MaintainedSpeed (0x825D22CC), no traction
        // (0x825D22D8), gas < 0.3 (0x825D22EC), handbrake (0x825D22F8), invalid ground (0x825D2304)
        // -- branches to loc_825D2578, which stores the SPEED PARAMETER (v2, m/s) into the
        // MaintainedSpeed lane: `lvx128 v0,+0x1000 ; vrlimi128 v0, v2, 4, 0 ; stvx128` (lane .y).
        // Only the success path skips it (`b loc_825D2588` at 0x825D2574). The old body stored on
        // the flag gate ALONE, and stored mfSpeedMPH.x there -- MPH into a lane EnterDrift seeds
        // with CheckForEnteringDrift's lfSpeedMPS (m/s). So a drift that lost gas or traction for a
        // moment kept pushing back toward the ENTRY speed instead of re-basing to the current one.
        const f32 lfMaintained = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y;
        const f32 lfSpeed      = lvfSpeed.x;
        const f32 lfThrottle   = (lpControls != NULL) ? lpControls->mfGas : 0.0f;   // lfs f13,4(r4)
        const bool lbMaintain  = mDriftFlags.DoMaintainSpeed()          // 0x825D229C flags & 1
                              && !(lfSpeed > lfMaintained)              // 0x825D22BC vcmpgtfp. v2, .y
                              && mbAllWheelsHaveTraction                // 0x825D22D0 +0x135B
                              && !(lfThrottle < 0.30000001f)            // 0x825D22E8 flt_82004740
                              && !mbHandBrake                           // 0x825D22F0 +0x1358
                              && mAboveGroundTestResult.mbValid;        // 0x825D22FC +0x598
        if (!lbMaintain)
        {
            mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y = lfSpeed;   // loc_825D2578
            return;
        }

        {
            {
                // deficit-scaled impulse direction = blend of body Z (forward) and velocity, weighted by
                // mvPropSpeedMaintainAlongZ (.x) / mvPropSpeedMaintainAlongVel (.y). The deficit =
                // MaintainedSpeed - speed. D2: scaled by the vehicle MASS (base attribs +0x70 .x).
                const f32 lfDeficit  = lfMaintained - lfSpeed;
                const f32 lfAlongZ   = mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.x;
                const f32 lfAlongVel = mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.y;
                const f32 lfPush     = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;   // D2: MASS

                // D3: the velocity-direction term only when moving (|speedParam| > eps).
                Vector3 lVelDir{ 0.0f, 0.0f, 0.0f, 0.0f };
                if (std::fabs(lfSpeed) > 1.1920929e-07f)   // FLT_EPSILON, the asm's gate
                {
                    lVelDir = vpu::Normalize(mLinearVelocity);
                }
                Vector3 lDir{ lvDirection.x * lfAlongZ + lVelDir.x * lfAlongVel,
                              lvDirection.y * lfAlongZ + lVelDir.y * lfAlongVel,
                              lvDirection.z * lfAlongZ + lVelDir.z * lfAlongVel,
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
    // THE LAST UNRESOLVED EXTERNAL OF THIS TRANSLATION UNIT. It had been declare-only since this
    //   header was written ("bodied by its own TU"), which no TU ever did, and the arity it was
    //   declared with (five trailing f32) never existed on any platform.
    //
    // It is ABSENT from `.ida-exports/BURNOUT_X360_ARTIST.XEX/` -- the third confirmed hole in
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
    // REBUILT FROM THE ASM 2026-08-03. The committed guard battery was written before the three
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
    // Guard 3 is the reason the dropped `VecFloat` mattered: it is the ONLY consumer of the
    //   time-step in the whole drift family, and with the parameter missing it had been written as a
    //   `(void)` no-op. `vsel` selects between {0,0,0,0} and {~0,~0,~0,~0} -- the two halves of the
    //   static-init table at unk_8327F240 (0x82C74368), read out of the IDB -- so it is a plain
    //   conditional select, not a blend.

    // ---- [drift] PC bring-up instrument -- DELETE WHEN drift entry/exit is proven 1:1 -----------
    // OPT-IN (BRN_DRIFT_PROBE=1, flow_run.ps1 -DiagEnv "BRN_DRIFT_PROBE=1"). Prints BOTH SIDES of
    // every compare CheckForEnteringDrift / UpdateDriftState make, every frame the brake or the
    // steering is past the 0.1 dead zone or a drift is latched (else every 60th frame), plus the
    // guard id of every ExitDrift the battery fires. A verdict-only print has lied to three waves;
    // this one prints the operands. Nothing here is console state.
    namespace
    {
        bool DriftProbeArmed()
        {
            static s32 siArmed = -1;
            if (siArmed < 0)
            {
                const char* lpcEnv = getenv("BRN_DRIFT_PROBE");
                siArmed = (lpcEnv != NULL && lpcEnv[0] != '0') ? 1 : 0;
            }
            return (siArmed == 1) && (CgsDev::Log::gpDebugPrint != NULL);
        }
        u32 s_uDriftProbeFrame = 0u;
    }

    void VehiclePhysics::UpdateDriftState(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                                          f32 lfAbsDriftScale, f32 lfSpeedMPS, VecFloat lvfTimeStep)
    {
        // CheckForEnteringDrift may latch a NEW drift this frame; every argument is forwarded.
        CheckForEnteringDrift(lpControls, lfAbsSteering, lfAbsDriftScale, lfSpeedMPS, lvfTimeStep);

        // [drift] probe (opt-in, see DriftProbeArmed above). Only the PLAYER car is printed.
        const bool lbDriftProbe = DriftProbeArmed()
                               && (lpControls == NULL || lpControls->GetType() == E_DRIVER_TYPE_PLAYER);
        if (lbDriftProbe)
        {
            ++s_uDriftProbeFrame;
            const f32 lfBrakeIn = (lpControls != NULL) ? lpControls->mfBrake : -1.0f;
            const f32 lfGasIn   = (lpControls != NULL) ? lpControls->mfGas   : -1.0f;
            const f32 lfSteerIn = (lpControls != NULL) ? lpControls->mfSteering : -1.0f;
            const f32 lfHBIn    = (lpControls != NULL) ? lpControls->mfHandBrake : -1.0f;
            const bool lbActive = (lfBrakeIn > 0.1f) || (lfAbsSteering > 0.1f) || (mu8DriftState != eDriftState_None);
            if (lbActive || (s_uDriftProbeFrame % 60u) == 0u)
            {
                const f32 lfRecip = (lfSpeedMPS != 0.0f) ? (1.0f / lfSpeedMPS) : 0.0f;
                const Vector3 lUnit{ mLinearVelocity.x * lfRecip, mLinearVelocity.y * lfRecip, mLinearVelocity.z * lfRecip, 0.0f };
                *CgsDev::Log::gpDebugPrint
                    << "[drift] n " << static_cast<s32>(s_uDriftProbeFrame)
                    << " st " << static_cast<s32>(mu8DriftState)
                    << " in g/b/hb/s " << lfGasIn << " " << lfBrakeIn << " " << lfHBIn << " " << lfSteerIn
                    << " absS " << lfAbsSteering
                    << " force " << static_cast<s32>(lpControls != NULL ? lpControls->mbForceDrift : 0)
                    << " hb " << static_cast<s32>(mbHandBrake)
                    << " allTr " << static_cast<s32>(mbAllWheelsHaveTraction)
                    << " agValid " << static_cast<s32>(mAboveGroundTestResult.mbValid)
                    << " mph " << mfSpeedMPH.x
                    << " mps " << lfSpeedMPS
                    << " minDrift " << mpAttribs->mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x
                    << " nWC " << static_cast<s32>(mi8NumWorldCollisions)
                    << " nC " << miNumCollisions
                    << " tHBon " << mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.z
                    << " tHBoff " << mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.w
                    << " tStatic " << mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.y
                    << " tDrift " << mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z
                    << " brkScale " << mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.w
                    << " tNoTr " << mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z
                    << " dScale " << mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w
                    << " broken " << static_cast<s32>(maWheels[eFrontLeftWheel].mbBrokenAdhesiveLimit)
                    << static_cast<s32>(maWheels[eFrontRightWheel].mbBrokenAdhesiveLimit)
                    << static_cast<s32>(maWheels[eRearLeftWheel].mbBrokenAdhesiveLimit)
                    << static_cast<s32>(maWheels[eRearRightWheel].mbBrokenAdhesiveLimit)
                    << " yaw " << vpu::Dot(mAngularVelocity, mTransform.yAxis)
                    << " slipX " << vpu::Dot(lUnit, mTransform.xAxis)
                    << " cosZ " << vpu::Dot(lUnit, mTransform.zAxis)
                    << " steerReg " << mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.x
                    << " " << mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y
                    << "\n";
            }
        }

        // only run the exit battery while drifting and not being HELD in drift. 0x8261F74C:
        // `lbz r11,0x3E(r31)` where r31 == controls -> mbForceDrift.
        const bool lbHeldInDrift = (lpControls != NULL) ? lpControls->mbForceDrift : false;
        if (mu8DriftState == eDriftState_None || lbHeldInDrift)
            return;

        // 1. the handbrake has been held down too long to still count as a drift.
        if (mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.z
            > KF_DRIFT_HANDBRAKE_ON_LIMIT)
        { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 1\n"; ExitDrift(); return; }

        // 2. the handbrake is down and was released too recently (vcfsx v0,1,1 == 0.5f).
        if (mbHandBrake
            && 0.5f > mvDampRollVel_TimeInDriftWithStaticFriction_TimeHandbrakeHasBeenOn_TimeSinceLastHandBrake.w)
        { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 2\n"; ExitDrift(); return; }

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
        { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 3\n"; ExitDrift(); return; }

        // 5. exit collision counters.
        if (mi8NumWorldCollisions > 0) { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 4\n"; ExitDrift(); return; }
        if (miNumCollisions       > 0) { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 5\n"; ExitDrift(); return; }

        // 6. below the per-car minimum drift speed. mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x is in MPH, the parameter is in
        //    m/s -- see the KF_MPS_TO_MPH note. This is the same lane, same threshold and same unit
        //    that CheckForEnteringDrift tests the other way round to allow entry.
        if (mpAttribs->mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x > lfSpeedMPS * KF_MPS_TO_MPH)
        { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 6\n"; ExitDrift(); return; }

        // 7. off the ground too long (lane .z, and NOT gated on the handbrake).
        if (mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z > 1.0f)
        { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 7\n"; ExitDrift(); return; }

        // 8. above-ground result invalid -> exit.
        if (!mAboveGroundTestResult.mbValid) { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 8\n"; ExitDrift(); return; }

        // 9. speed too low (mfSpeedMPH is the splatted body speed at +0x6C0).
        if (KF_DRIFT_SPEED_EXIT_LIMIT > mfSpeedMPH.x)
        { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 9\n"; ExitDrift(); return; }

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
                { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 10\n"; ExitDrift(); return; }
            }
            else
            {
                if (lfLateralSlip <= 0.0099999998f && lfSteer <= 0.0049999999f)
                { if (lbDriftProbe) *CgsDev::Log::gpDebugPrint << "[drift] EXIT guard 11\n"; ExitDrift(); return; }
            }
        }

        (void)lfAbsSteering;     // [V] consumed by CheckForEnteringDrift, not by the exit battery
        (void)lfAbsDriftScale;   // [V] idem
    }

    // @0x825FA748  VehiclePhysics::UpdateDriftScale
    //   Breaker ABI: r3=this, r4=controls, f1=lfAbsSteering, f2=lfAbsDriftScale (unused),
    //   v1=lvfTimeStep. The function computes the signed forward/velocity angle, integrates
    //   DriftScale through the counter-steer or normal branch, then applies natural decay.
    //   The low-steering path deliberately calls ApplyNaturalDriftForces twice: once with a
    //   factor of one at 0x825FAC10, then again with min(1-gas,1-brake) at 0x825FACD0.
    void VehiclePhysics::UpdateDriftScale(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                                          f32 lfAbsDriftScale, VecFloat lvfTimeStep)
    {
        static const f32 KF_TWO_PI = 6.2831855f;                // flt_8208F600
        static const f32 KF_HALF_TURN_DEG = 180.0f;             // flt_820025FC
        static const f32 KF_FULL_TURN_DEG = 360.0f;             // flt_82004928
        static const f32 KF_MIN_STEERING_FOR_SCALE = 0.1f;      // flt_82004014
        static const f32 KF_SLIP_ANGLE_NORMALISER_DEG = 90.0f;  // unk_82FB80F0

        bool lbForceComeOutOfDrift = false;
        if (lpControls->GetType() == E_DRIVER_TYPE_AI)
            lbForceComeOutOfDrift = static_cast<const BrnAIDriverControls*>(lpControls)->mbForceComeOutOfDrift;

        const f32 lfControlSteering = lpControls->mfSteering;
        const f32 lfGas = lpControls->mfGas;
        const f32 lfBrake = lpControls->mfBrake;
        const f32 lfOldDriftScale =
            mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w;
        const bool lbCounterSteering = (lfOldDriftScale * lfControlSteering) < 0.0f;

        const Vector3 lLinearVelocityDir = vpu::Normalize(mLinearVelocity);
        const Vector3 lForwardDir = vpu::Normalize(mTransform.zAxis);
        f32 lfCosAngle = vpu::Dot(lForwardDir, lLinearVelocityDir);
        lfCosAngle = std::min(1.0f, std::max(-1.0f, lfCosAngle));

        f32 lfDriftAngle = std::acos(lfCosAngle);
        if (vpu::Dot(vpu::Cross(lForwardDir, lLinearVelocityDir), mTransform.yAxis) < 0.0f)
            lfDriftAngle = KF_TWO_PI - lfDriftAngle;

        lfDriftAngle *= KF_RAD_TO_DEG;
        if (lfDriftAngle > KF_HALF_TURN_DEG)
            lfDriftAngle -= KF_FULL_TURN_DEG;

        const VecFloat lvfDriftAngle{
            lfDriftAngle, lfDriftAngle, lfDriftAngle, lfDriftAngle
        };
        const VecFloat lvfOne{ 1.0f, 1.0f, 1.0f, 1.0f };
        const VecFloat lvfGas{ lfGas, lfGas, lfGas, lfGas };
        const VecFloat lvfBrake{ lfBrake, lfBrake, lfBrake, lfBrake };

        const VehicleAttribs::DriftAttribs& lrDriftAttribs = mpAttribs->mDriftAttribs;
        const bool lbAboveMinimumDriftSpeed =
            mfSpeedMPH.x > lrDriftAttribs
                .mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x;

        if (!lbAboveMinimumDriftSpeed || lbForceComeOutOfDrift)
        {
            ApplyNaturalDriftForces(lvfTimeStep, lvfDriftAngle, lvfOne, lvfGas, lvfBrake);
            return;
        }

        if (lfAbsSteering > KF_MIN_STEERING_FOR_SCALE)
        {
            const f32 lfSteering =
                mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y;
            const Vector4& lrScaleFactors =
                lrDriftAttribs
                    .mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor;
            const Vector4& lrControlFactors =
                lrDriftAttribs
                    .mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale;

            if (lbCounterSteering)
            {
                const f32 lfSlipAngleRatio = std::fabs(std::min(
                    1.0f,
                    std::max(-1.0f, lfDriftAngle / KF_SLIP_ANGLE_NORMALISER_DEG)));
                const f32 lfSteeringSquared = lfSteering * lfSteering;
                const f32 lfDriftScaleDelta =
                    (lfSteering * lrScaleFactors.z)
                        * (lrScaleFactors.w + lfSlipAngleRatio * lfSlipAngleRatio)
                        * lfSteeringSquared
                    + lfSteering * lfBrake * lrControlFactors.x;

                mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w =
                    lfOldDriftScale + lfDriftScaleDelta * lvfTimeStep.x;
            }
            else
            {
                f32 lfDriftScaleDelta =
                    lfSteering * lrScaleFactors.y
                    + lfSteering * lfGas * lrControlFactors.y
                    + lfSteering * lfBrake * lrControlFactors.x;
                lfDriftScaleDelta *= lvfTimeStep.x;

                const f32 lfDeltaSign = (lfDriftScaleDelta >= 0.0f)
                    ? ((lfDriftScaleDelta > 0.0f) ? 1.0f : 0.0f)
                    : -1.0f;
                const f32 lfMagnitude = std::max(
                    0.0f,
                    std::min(std::fabs(lfDriftScaleDelta) + lfOldDriftScale * lfDeltaSign,
                             std::fabs(lfSteering)));

                mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w =
                    lfMagnitude * lfDeltaSign;
            }
        }
        else
        {
            ApplyNaturalDriftForces(lvfTimeStep, lvfDriftAngle, lvfOne, lvfGas, lvfBrake);
        }

        const f32 lfNaturalDecayFactor = std::min(1.0f - lfGas, 1.0f - lfBrake);
        const VecFloat lvfNaturalDecayFactor{
            lfNaturalDecayFactor, lfNaturalDecayFactor,
            lfNaturalDecayFactor, lfNaturalDecayFactor
        };
        ApplyNaturalDriftForces(
            lvfTimeStep, lvfDriftAngle, lvfNaturalDecayFactor, lvfGas, lvfBrake);

        (void)lfAbsDriftScale;
    }

    // @0x825D25A0  VehiclePhysics::ApplyDriftYaw
    //   Breaker first computes the signed velocity/forward angle, then monotonically latches
    //   DriftGasLetOffAmount=max(old,1-gas), even on frames that fail the later torque gates.
    //   The normal torque is mDriftScaleToYawTorque.GetInterped(absDriftScale), with the low-steer
    //   fall-off and counter-steer branches below matching 0x825D28CC..0x825D2A70. A separate
    //   gas-let-off kick is added before applying the result about mTransform.yAxis.
    void VehiclePhysics::ApplyDriftYaw(const BrnPlayerDriverControls* lpControls,
                                        VecFloat lvfAbsSteering, VecFloat lvfAbsDriftScale)
    {
        static const f32 KF_TWO_PI = 6.2831855f;   // flt_8208F600
        static const f32 KF_HALF_TURN_DEG = 180.0f; // flt_820025FC
        static const f32 KF_FULL_TURN_DEG = 360.0f; // flt_82004928
        static const f32 KF_LOW_STEER = 0.1f;       // flt_82004014

        const Vector3 lLinearVelocityDir = vpu::Normalize(mLinearVelocity);
        const Vector3 lForwardDir = vpu::Normalize(mTransform.zAxis);
        f32 lfCosAngle = vpu::Dot(lForwardDir, lLinearVelocityDir);
        lfCosAngle = std::min(1.0f, std::max(-1.0f, lfCosAngle));

        f32 lfDriftAngle = std::acos(lfCosAngle);
        if (vpu::Dot(vpu::Cross(lForwardDir, lLinearVelocityDir), mTransform.yAxis) < 0.0f)
            lfDriftAngle = KF_TWO_PI - lfDriftAngle;

        lfDriftAngle *= KF_RAD_TO_DEG;
        if (lfDriftAngle > KF_HALF_TURN_DEG)
            lfDriftAngle -= KF_FULL_TURN_DEG;

        const f32 lfGas = lpControls->mfGas;
        f32& lrfGasLetOffAmount = mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.w;
        lrfGasLetOffAmount = std::max(lrfGasLetOffAmount, 1.0f - lfGas);

        const f32 lfDriftScale = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w;
        const f32 lfAbsCurrentDriftScale = std::fabs(lfDriftScale);
        if (!(lfAbsCurrentDriftScale > KF_DRIFT_STEER_EPSILON) || !mDriftFlags.DoApplyTorque())
            return;

        const VehicleAttribs::DriftAttribs& lrDriftAttribs = mpAttribs->mDriftAttribs;
        f32 lfTorque = 0.0f;

        const f32 lfMaxDriftAngle =
            lrDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.y;
        if (lfMaxDriftAngle > std::fabs(lfDriftAngle))
        {
            // 0x825D2888..0x825D28EC: the curve is evaluated at |+0x1000.w| -- the MEMBER DriftScale,
            // which UpdateDriftScale has already advanced this frame -- not at the lvfAbsDriftScale
            // argument (that is UpdateDrift's pre-update snapshot; the console only reads v2 for the
            // fall-off sqrt below). D4 of the drift-symmetry wave, 2026-09-02.
            lfTorque = lrDriftAttribs.mDriftScaleToYawTorque.GetInterped(
                VecFloat{ lfAbsCurrentDriftScale, lfAbsCurrentDriftScale,
                          lfAbsCurrentDriftScale, lfAbsCurrentDriftScale }).x;

            if (KF_LOW_STEER > lvfAbsSteering.x)
            {
                // ⛔ D3 (0x825D2918..0x825D2968): `vspltw v0, +0x1000, 3 ; vcmpgtfp. v0, v0, 0.0` --
                // the SIGNED DriftScale is tested against zero and the torque is negated only when
                // it is positive. The old `lfAbsCurrentDriftScale > 0.0f` was true on every frame
                // that reached this line (the eps gate above guarantees it), so the low-steer torque
                // was negated for BOTH drift directions.
                if (lfDriftScale > 0.0f)
                    lfTorque = -lfTorque;

                const f32 lfDriftTorqueFallOff =
                    lrDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.x;
                if (lfDriftTorqueFallOff > lvfAbsDriftScale.x)
                    lfTorque *= std::sqrt(lvfAbsDriftScale.x / lfDriftTorqueFallOff);
            }
            else
            {
                // ⛔⛔ D2 -- THE "CAN ONLY DRIFT LEFT" DEFECT (0x825D2A0C..0x825D2A70). The console
                // multiplies the curve torque by (-Steering * |Steering|) UNCONDITIONALLY:
                //   vspltw v11,+0xFE0,1 ; vandc v0 = |Steering| ; vxor v13 = -Steering ;
                //   vmulfp128 v0 = v13*v0 ; vmulfp128 v12 = v12*v0        @0x825D2A54
                // and only THEN tests `0 > DriftScale*Steering` (counter-steering) to scale the
                // result by CounterSteerTorqueScaleFactor (+0x170.z, @0x825D2A68-70). The old body
                // applied the -Steering*|Steering| product only inside the counter-steer arm, so a
                // normal (steer-with-the-drift) drift got an UNSIGNED, always-positive yaw torque:
                // correct by accident for one steer sign, fighting the steering for the other.
                // Measured before the fix (drw_right3): full-right + brake at 81 mph latched
                // state 1 and DriftScale +1.0, yet yaw rate went -0.31 -> +0.73 rad/s (turning
                // LEFT) with slipX ~ +0.01 -- no slide ever built. The mirrored left run
                // (sq1_drift2) reached slipX -1.0 and yaw +2.0.
                const f32 lfSteering =
                    mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y;
                lfTorque *= -lfSteering * std::fabs(lfSteering);

                const bool lbCounterSteering = (lfDriftScale * lfSteering) < 0.0f;
                if (lbCounterSteering)
                {
                    const f32 lfCounterSteerTorqueScale =
                        lrDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.z;
                    lfTorque *= lfCounterSteerTorqueScale;
                }
            }
        }

        const f32 lfSteering = mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y;
        const f32 lfTorqueFromGas =
            lrDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.z
            * lfGas * lrfGasLetOffAmount * -lfSteering * (1.0f - lfGas);
        const f32 lfTotalTorque = lfTorque + lfTorqueFromGas;

        const Vector3 lTorque{ mTransform.yAxis.x * lfTotalTorque,
                               mTransform.yAxis.y * lfTotalTorque,
                               mTransform.yAxis.z * lfTotalTorque,
                               0.0f };
        AddWorldSpaceTorque(lTorque);
    }

    // @0x825D2B20  VehiclePhysics::ApplyDriftLatForce
    //   Breaker consumes only v1/v3/v6: abs drift scale, steering, and time step. It stores the
    //   unsigned velocity/forward angle, builds independent scale/angle and speed factors, updates
    //   CounterSteerSideMag from yaw before decaying it at 2/sec, and stores the resulting force in
    //   SideForceMag. The state-signed body-right force is projected onto the road tangent before
    //   AddWorldSpaceForce. The debug-only mAboveGroundTestResult.mbValid assert is elided.
    void VehiclePhysics::ApplyDriftLatForce(VecFloat lvfAbsDriftScale, VecFloat lvfSpeedMPS,
                                             VecFloat lvfSteering, VecFloat lvfBrake,
                                             VecFloat lvfGas, VecFloat lvfTimeStep)
    {
        static const f32 KF_SIDE_FORCE_CAP = 5.0f;        // unk_82FB9290 <- flt_8200426C
        static const f32 KF_COUNTER_DECAY_PER_SEC = 2.0f; // vspltisw 2; vcfsx

        (void)lvfSpeedMPS;
        (void)lvfBrake;
        (void)lvfGas;

        const VehicleAttribs::DriftAttribs& lrDriftAttribs = mpAttribs->mDriftAttribs;
        const Vector3 lUnitVel = vpu::Normalize(mLinearVelocity);
        f32 lfCosAngle = vpu::Dot(lUnitVel, mTransform.zAxis);
        lfCosAngle = std::min(1.0f, std::max(-1.0f, lfCosAngle));
        const f32 lfAngle = std::acos(lfCosAngle) * KF_RAD_TO_DEG;
        mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.w = lfAngle;

        f32 lfSideForceFactor = 0.0f;
        const bool lbVelocityIsNonZero =
            std::fabs(mLinearVelocity.x) > KF_DRIFT_STEER_EPSILON
            || std::fabs(mLinearVelocity.y) > KF_DRIFT_STEER_EPSILON
            || std::fabs(mLinearVelocity.z) > KF_DRIFT_STEER_EPSILON;
        if (lbVelocityIsNonZero)
        {
            const f32 lfScaleCutOff =
                lrDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.y;
            const f32 lfPeakAngle =
                lrDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.x;
            const f32 lfAngleCutOff =
                lrDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.z;

            const f32 lfScaleRatio = std::min(lvfAbsDriftScale.x, lfScaleCutOff) / lfScaleCutOff;
            const f32 lfAngleRange = lfAngleCutOff - lfPeakAngle;
            const f32 lfAnglePastPeak = std::min(lfAngleRange,
                                                 std::max(0.0f, std::fabs(lfAngle) - lfPeakAngle));
            const f32 lfAngleRatio = lfAnglePastPeak / lfAngleRange;
            lfSideForceFactor = lfScaleRatio * (1.0f - lfAngleRatio);
        }

        const f32 lfMinSpeed =
            lrDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x;
        const f32 lfSpeedCutOff =
            lrDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.w;
        f32 lfSpeedRatio = 1.0f;
        if (lfSpeedCutOff > lfMinSpeed)
        {
            const f32 lfClampedSpeed = std::min(lfSpeedCutOff,
                                                std::max(lfMinSpeed, mfSpeedMPH.x));
            lfSpeedRatio = (lfClampedSpeed - lfMinSpeed) / (lfSpeedCutOff - lfMinSpeed);
        }
        lfSpeedRatio = std::min(1.0f, std::max(0.0f, lfSpeedRatio));

        const f32 lfYaw = vpu::Dot(mAngularVelocity, mTransform.yAxis);
        f32& lrfCounterSteerSideMag =
            mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.z;
        if (lvfSteering.x * lfYaw > 0.0f)
        {
            const f32 lfYawFactor = std::min(1.0f, std::max(0.0f, std::fabs(lfYaw) * 0.5f));
            const f32 lfMaxPowerSlideFactor =
                lrDriftAttribs.mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor.z;
            const f32 lfCounterSteerTarget = lfMaxPowerSlideFactor
                                           * std::fabs(mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w)
                                           * lfYawFactor;
            lrfCounterSteerSideMag = std::max(lrfCounterSteerSideMag, lfCounterSteerTarget);
        }
        lrfCounterSteerSideMag = std::max(0.0f,
                                          lrfCounterSteerSideMag
                                          - lvfTimeStep.x * KF_COUNTER_DECAY_PER_SEC);

        // ⭐ P7 (deform-land wave 2026-08-24; physics11 leg B + blindfns leg 2): the third
        // multiplicand is +0x1030.x (LatDriftForceFactor -- a LATCH held at 1.0 by
        // Reset/SetCrashing/ExitDrift; the whole-image +0x1030 write census proves NO ramp
        // writer exists), NOT the current drift angle in DEGREES the old body multiplied in
        // (which scaled the side force ~30-40x at typical drift angles). Lands PAIRED with the
        // R1 ExitDrift fix -- without R1 the latch reads the invented 0.0 after the first
        // drift ends and the side force dies.
        const f32 lfLatDriftForceFactor = lfSideForceFactor * lfSpeedRatio
            * mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.x;
        const f32 lfUnscaledSideForce =
            std::min(lfLatDriftForceFactor * (1.0f + lrfCounterSteerSideMag), KF_SIDE_FORCE_CAP);
        const f32 lfSideForceMag = lfUnscaledSideForce
                                 * mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x
                                 * lrDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.y;
        mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.x = lfSideForceMag;

        const f32 lfSignedSideForce = (mu8DriftState == eDriftState_FacingLeft)
                                          ?  lfSideForceMag
                                          : -lfSideForceMag;
        Vector3 lImpulse{ mTransform.xAxis.x * lfSignedSideForce,
                          mTransform.xAxis.y * lfSignedSideForce,
                          mTransform.xAxis.z * lfSignedSideForce,
                          0.0f };

        const Vector3& lrGroundNormal = mAboveGroundTestResult.mIntersectionNormal;
        const f32 lfNormalComponent = vpu::Dot(lrGroundNormal, lImpulse);
        lImpulse.x -= lrGroundNormal.x * lfNormalComponent;
        lImpulse.y -= lrGroundNormal.y * lfNormalComponent;
        lImpulse.z -= lrGroundNormal.z * lfNormalComponent;
        AddWorldSpaceForce(lImpulse);
    }

    // @0x8261FAB0  VehiclePhysics::ApplyDriftForces
    //   Dispatches the four drift sub-forces in order. Each is preceded by an elided CheckState debug
    //   call. ApplyDriftLatForce is gated by mbAllWheelsHaveTraction && mAboveGroundTestResult.mbValid &&
    //   !mbHandBrake (asm: `_R31[4955] && _R31[1432] && !_R31[4952]`).
    // SIGNATURE CORRECTED 2026-08-03 (DWARF VehiclePhysics.h:1460). Its two leading f32s and the
    //   VecFloat dt are UpdateDrift's, forwarded unchanged; only MaintainDriftSpeed is handed the
    //   speed (0x8261FB00-18: `lvlx v0,r0,&arg_34 ; vspltw v2,v0,0` -- the spilled f3).
    void VehiclePhysics::ApplyDriftForces(const BrnPlayerDriverControls* lpControls, f32 lfAbsSteering,
                                          f32 lfAbsDriftScale, f32 lfSpeedMPS, VecFloat lvfTimeStep)
    {
        // DecFIGS supplies the vector-heavy subordinate signatures, and Breaker pins their
        // register order:
        //      MaintainDriftSpeed     (const BrnPlayerDriverControls*, Vector3, VecFloat)   :1463
        //      ApplyDriftLatForce     (VecFloat x6)                                          :1472
        //      ApplyNaturalDriftForces(VecFloat x5)                                          :1475
        //    The asm operands are:
        //      MaintainDriftSpeed  v1 = mTransform.zAxis (this+0x30), v2 = splat(lfSpeedMPS)
        //      ApplyDriftYaw       v1 = splat(lfAbsSteering),  v2 = splat(lfAbsDriftScale)
        //      ApplyDriftLatForce  v1 = splat(lfAbsDriftScale), v2 = splat(lfSpeedMPS),
        //                          v3 = splat(controls->mfSteering), v4 = splat(controls->mfBrake),
        //                          v5 = splat(controls->mfGas),      v6 = the VecFloat dt
        const VecFloat lvfSpeed{ lfSpeedMPS, lfSpeedMPS, lfSpeedMPS, lfSpeedMPS };
        const VecFloat lvfAbsSteer{ lfAbsSteering, lfAbsSteering, lfAbsSteering, lfAbsSteering };
        const VecFloat lvfAbsScale{ lfAbsDriftScale, lfAbsDriftScale, lfAbsDriftScale, lfAbsDriftScale };
        MaintainDriftSpeed(lpControls, mTransform.zAxis, lvfSpeed);
        UpdateDriftScale(lpControls, lfAbsSteering, lfAbsDriftScale, lvfTimeStep);
        ApplyDriftYaw(lpControls, lvfAbsSteer, lvfAbsScale);

        if (mbAllWheelsHaveTraction && mAboveGroundTestResult.mbValid && !mbHandBrake)
        {
            const f32 lfSteering = (lpControls != NULL) ? lpControls->mfSteering : 0.0f;
            const f32 lfBrake = (lpControls != NULL) ? lpControls->mfBrake : 0.0f;
            const f32 lfGas = (lpControls != NULL) ? lpControls->mfGas : 0.0f;
            const VecFloat lvfSteering{ lfSteering, lfSteering, lfSteering, lfSteering };
            const VecFloat lvfBrake{ lfBrake, lfBrake, lfBrake, lfBrake };
            const VecFloat lvfGas{ lfGas, lfGas, lfGas, lfGas };
            ApplyDriftLatForce(lvfAbsScale, lvfSpeed, lvfSteering, lvfBrake, lvfGas, lvfTimeStep);
        }
    }

// [clean] UpdateDrift  @0x8262E200
    // @0x8262E200  VehiclePhysics::UpdateDrift
    //   The per-frame drift entry. Refreshes the cached steering direction (normalize(mLinearVelocity)
    //   into the steering register), runs the drift state machine (UpdateDriftState), then:
    //     * when drifting: advances TimeDrifting; damps body-Y angular velocity with
    //       Pow(DriftAngularDamping, 60*dt); damps body-X linear velocity with
    //       Pow(DriftSidewaysDamping, 60*dt); then applies drift forces when traction permits.
    //     * when NOT drifting: decrements TimeDrifting and applies the speed-scaled
    //       VehicleBaseAttribs high-speed yaw damping while off the handbrake.
    //   Uses the ORIGINAL controls when the drift-override byte is set. The per-phase CheckState debug
    //   calls are elided.
    //   The vexptefp/vlogefp + unk_82014AC0..AF0 cascades are compiler-expanded
    //   rw::math::vpu::Pow calls: Breaker supplies the dataflow, and DecFIGS names both VecFloat
    //   damping locals plus Pow. They are lowered to source-level std::pow below.
    void VehiclePhysics::UpdateDrift(const BrnPlayerDriverControls* lpOriginalControls, VecFloat lvfTimeStep)
    {
        // THE THREE SCALARS, CORRECTED 2026-08-03 FROM THE ASM (0x8262E230-2D8). The committed
        //    version read three different members and mis-ordered them; every one of the three below
        //    is an asm-literal load, and each independently confirms the PS3 DWARF's parameter name
        //    for the slot it lands in:
        //      f29 = |this[0xFE0].y|  -> |Steering|    (vspltw lane 1 + vandc 0x80000000) lfAbsSteering
        //      f30 = |this[0x1000].w| -> |DriftScale|  (vspltw lane 3 + vandc)            lfAbsDriftScale
        //      f31 = sqrt(dot3(mLinearVelocity, mLinearVelocity))                         lfSpeedMPS
        //            (vmsum3fp128 + vrsqrtefp + two Newton steps, then `vsel` back to 0 when the
        //             squared magnitude is exactly 0 -- so a stationary car yields 0, not NaN)
        // RETIRED with them: a write of `dot(normalize(v), zAxis)` into
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

            // Breaker 0x8262E344..0x8262E520: the first vlogefp/vexptefp polynomial is the
            // inlined rw::math::vpu::Pow(GetDriftAngularDamping(), 60*dt), followed by
            //   angularVelocity -= transform.Y * dot(transform.Y, angularVelocity) * factor.
            // DecFIGS names the source local `VecFloat lvfDriftAngularDamping` and the Pow call.
            // Its VecFloat is genuine declaration shape, while the damping getter's one scalar
            // attrib lane and 60*dt are uniform splats; spelling the equivalent lane operation as
            // scalar std::pow on PC avoids reproducing the PPC compiler's approximation tables.
            static const f32 KF_DRIFT_DAMP_BLEND_RATE = 60.0f;   // kDamp_BlendRate
            const f32 lfDampExponent = KF_DRIFT_DAMP_BLEND_RATE * lvfTimeStep.x;
            const f32 lfAngularDamping = static_cast<f32>(std::pow(
                mpAttribs->mDriftAttribs
                    .mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.x,
                lfDampExponent));
            const Vector3& lvUp = mTransform.yAxis;
            const f32 lfYawRate = vpu::Dot(lvUp, mAngularVelocity);
            mAngularVelocity = mAngularVelocity - lvUp * (lfYawRate * lfAngularDamping);

            // Breaker 0x8262E528..0x8262E6BC: the second identical Pow implementation consumes
            // GetDriftSidewaysDamping(), then removes that fraction of the linear-velocity
            // component along transform X. DecFIGS names this genuine source local
            // `VecFloat lvfDriftSidewaysDamping`; again every lane is a scalar splat here.
            const f32 lfSidewaysDamping = static_cast<f32>(std::pow(
                mpAttribs->mDriftAttribs
                    .mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.w,
                lfDampExponent));
            const Vector3& lvRight = mTransform.xAxis;
            const f32 lfSidewaysSpeed = vpu::Dot(lvRight, mLinearVelocity);
            mLinearVelocity = mLinearVelocity - lvRight * (lfSidewaysSpeed * lfSidewaysDamping);

            if (mbAllWheelsHaveTraction)
                ApplyDriftForces(lpOriginalControls, lfAbsSteering, lfAbsDriftScale, lfSpeedMPS, lvfTimeStep);
        }
        else
        {
            // 0x8262E708..0x8262E72C: IncPackedTimeDrifting(-lfTimeStep). The xor with the
            // sign-bit splat negates the incoming VecFloat, then vrlimi writes only lane .z.
            mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z
                -= lvfTimeStep.x;

            // 0x8262E72C..0x8262E824: above LowSpeedDrivingMPH, remove a progressively larger
            // component of angular velocity about the body Y axis. The damping reaches the
            // HighSpeedAngularDamping value at MaxSpeed and is capped there. These are the exact
            // +0x90.y, +0x70.z and +0xA0.z VehicleBaseAttribs lanes loaded by the assembly.
            const VehicleAttribs::VehicleBaseAttribs& lrBase = mpAttribs->mBaseAttribs;
            const f32 lfLowSpeedMPH =
                lrBase.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.y;
            if (mfSpeedMPH.x > lfLowSpeedMPH && !mbHandBrake)
            {
                const f32 lfMaxSpeedMPH =
                    lrBase.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z;
                const f32 lfHighSpeedAngularDamping =
                    lrBase.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.z;
                const f32 lfDampeningFactor = std::min(
                    (mfSpeedMPH.x - lfLowSpeedMPH) / (lfMaxSpeedMPH - lfLowSpeedMPH),
                    1.0f);
                const Vector3& lvUp = mTransform.yAxis;
                const f32 lfYawRate = vpu::Dot(lvUp, mAngularVelocity);
                mAngularVelocity = mAngularVelocity
                    - lvUp * (lfYawRate * lfHighSpeedAngularDamping * lfDampeningFactor);
            }
        }
    }

    // ============================ C03 suspension/downforce/weight group ============================

    // ---- [wsus] PC bring-up instrument -- DELETE-WHEN the kerb response is proven 1:1 ------------
    // OPT-IN (BRN_WHEEL_SUS_PROBE=1, flow_run.ps1 -DiagEnv BRN_WHEEL_SUS_PROBE=1). NOT console code.
    // Off by default: the latch reads 0 once and every print is unreachable thereafter.
    //
    // WHY. The kerb wave (2026-09-02, kerbw_r1..r4) proved the world-contact path applies NO body
    // impulse at a kerb; the one sharp event on film (6.2 mph lost in ONE frame at the frame the
    // front wheel mounted) had zero solver impulses behind it and therefore lives in THIS chain --
    // ApplyWheelWeight -> UpdateSuspensionSprings -> ApplySuspensionForces -> UpdateWheels' tyre
    // pass -> UpdateSuspensionPostSimulation. No existing probe sees it: [traction] prints only the
    // hit height (no normal, no spring state) and [tyre] prints every 20th call. This prints, for
    // the PLAYER car only, EVERY physics step:
    //   [wsus]      per frame: pose, velocity, mph, angular velocity, air/crash/world-collision state
    //   [wsus-w]    per wheel: ground flags, hit position + NORMAL, line distance, wheel y vs the
    //               streamed seat and both travel stops, the spring's eight lanes after the
    //               integrate, and the mass-on-wheel lane
    //   [wsus-t]    per wheel, after UpdateWheels: tyre long/lat force (pre-cone / post-cone), slip,
    //               wheel spin vs road speed
    //   [wsus-post] per frame, UpdateSuspensionPostSimulation: the compressed-spring flags, the
    //               deepest penetration and its normal, the body translation applied
    //   [wsus-rep]  the first-loop "wheel above its plane" repair, when it fires
    //   [wsus-imp]  each inanimate-world recovery impulse, with the normal velocity it answered
    //   [wsus-attr] every 600 frames: the suspension attribs the maths above consumes
    // Budget-limited and SAYS SO when the budget runs out -- a silent stop reads exactly like
    // "nothing happened". All numbers are the members the console reads, printed unmodified.
    namespace
    {
        const u32 KU_WSUS_BUDGET_LINES = 600000u;
        u32 guWheelSusFrame = 0u;      // the most recently advanced per-car counter (for the notice)
        u32 guWheelSusLines = 0u;

        // PER-INSTANCE frame counters. UpdateSuspension/UpdateWheels run per car inside that car's
        // UpdateDriving, but UpdateSuspensionPostSimulation runs in a LATER pass over all cars, so a
        // single shared counter mis-labels every post-sim line with the last car's frame (measured
        // on wsus_r30: parked full-physics traffic carries driver type 0 == PLAYER, three
        // instances printed, and the post lines landed on the wrong car). Every line carries `car`.
        struct WheelSusSlot
        {
            const void* mpCar;
            u32         muFrame;
        };
        const s32 KI_WSUS_SLOTS = 16;
        WheelSusSlot gaWheelSusSlots[KI_WSUS_SLOTS] = { { NULL, 0u } };
        u32 guWheelSusOverflowFrame = 0u;

        u32& WheelSusFrameFor(const void* lpCar)
        {
            for (s32 liS = 0; liS < KI_WSUS_SLOTS; ++liS)
            {
                if (gaWheelSusSlots[liS].mpCar == lpCar)
                    return gaWheelSusSlots[liS].muFrame;
                if (gaWheelSusSlots[liS].mpCar == NULL)
                {
                    gaWheelSusSlots[liS].mpCar = lpCar;
                    gaWheelSusSlots[liS].muFrame = 0u;
                    return gaWheelSusSlots[liS].muFrame;
                }
            }
            return guWheelSusOverflowFrame;   // a 17th instance shares one counter; visible by `car`
        }

        bool WheelSusProbeArmed()
        {
            static s32 siArmed = -1;
            if (siArmed < 0)
            {
                const char* lpcEnv = getenv("BRN_WHEEL_SUS_PROBE");
                siArmed = (lpcEnv != NULL && lpcEnv[0] != '0') ? 1 : 0;
            }
            return (siArmed == 1) && (CgsDev::Log::gpDebugPrint != NULL);
        }

        // One line of budget. Prints the exhaustion notice exactly once.
        bool WheelSusTakeLine()
        {
            if (guWheelSusLines < KU_WSUS_BUDGET_LINES)
            {
                ++guWheelSusLines;
                return true;
            }
            if (guWheelSusLines == KU_WSUS_BUDGET_LINES)
            {
                ++guWheelSusLines;
                *CgsDev::Log::gpDebugPrint
                    << "[wsus] BUDGET EXHAUSTED after " << static_cast<s32>(KU_WSUS_BUDGET_LINES)
                    << " lines at frame " << static_cast<s32>(guWheelSusFrame)
                    << " -- every later [wsus*] line is DROPPED, the run is not silent because "
                       "nothing happened\n";
            }
            return false;
        }
    }

    // ---- [susv] THE POST-SIM VELOCITY LEDGER -- DELETE-WHEN-STABLE -------------------------------
    // OPT-IN (BRN_SUSV_PROBE=<metres per second>, flow_run.ps1 -DiagEnv BRN_SUSV_PROBE=1). NOT
    // console code. Unset => the latch reads 0 once and every print below is unreachable.
    //
    // ⭐ WHY IT EXISTS. One one-step velocity drop in this campaign is still unexplained: the
    // st_scoutB f1011->f1012 event, 9.45 m/s lost in a single 16.67 ms step at 28 mph, with
    // `crash 0` on every surrounding frame and NO impulse on either solver arm. Every other
    // one-step drop is now attributed (car-on-car traffic contacts, proven with a call-site tag).
    // The closed mechanism list leaves exactly one function unwatched --
    // VehiclePhysics::UpdateSuspensionPostSimulation @0x825F6BB0 -- and it holds TWO writers that
    // leave no accumulator entry, no contact and no counter:
    //   * StabiliseAfterHardLanding @0x825D1890, which OVERWRITES the velocity directly:
    //       v -= agNormal * (dot(v, agNormal) * pow(damp, dt*60))
    //   * the mi8NumWorldCollisions == 0 recovery impulses (restitution -0.7 @0x8208FB10)
    // ⭐⭐ AND A PROBE CAN WATCH HALF A SOLVER. This one states its coverage on its face: it is a
    // LEDGER, printing mLinearVelocity at every boundary INSIDE the function, so the residual
    // column ("other") is the honest name for anything the named stages do not account for. It
    // does NOT see writers outside this function; [dv] and the contact probes cover those.
    //
    // Threshold-gated on the WHOLE-FUNCTION |dv| so an ordinary step prints nothing: the value of
    // BRN_SUSV_PROBE is that threshold in m/s (a bare "1" means 1.0 m/s). One line per crossing.
    namespace
    {
        // ⛔⛔⛔ THIS FUNCTION SHIPPED BROKEN ONCE, AND THE BUG IS WORTH THE BANNER (2026-09-03).
        //   The first cut copied the repo's standard BOOLEAN probe idiom -- `lpcEnv[0] == '0'` means
        //   off -- into a probe whose value is a THRESHOLD. `BRN_SUSV_PROBE=0.5` and `=0.01` both
        //   START WITH '0', so both read as DISABLED, and three runs produced zero [susv] lines that
        //   were about to be published as "UpdateSuspensionPostSimulation moved nothing". The probe
        //   was never armed. ⭐ Nothing in the log said so: an unarmed probe and a quiet one look
        //   identical, which is the whole reason a probe must be seen to FIRE before its silence is
        //   read as a measurement. Parse the number and let the NUMBER decide.
        f32 SusVProbeThreshold()
        {
            static f32 sfThreshold = -1.0f;
            if (sfThreshold < 0.0f)
            {
                const char* lpcEnv = getenv("BRN_SUSV_PROBE");
                sfThreshold = (lpcEnv != NULL && lpcEnv[0] != '\0')
                                  ? static_cast<f32>(std::atof(lpcEnv))
                                  : 0.0f;
                if (sfThreshold < 0.0f) sfThreshold = 0.0f;   // <= 0 is off; see SusVProbeArmed
            }
            return sfThreshold;
        }
        // ⭐ AND IT ANNOUNCES ITSELF, ONCE. An unarmed probe and a probe whose threshold was never
        //   crossed are indistinguishable in a log -- that is how the bug above nearly got
        //   published as a result. With this line, "[susv] ARMED" present + no [susv] rows IS a
        //   measurement; its absence says the run never armed the probe at all.
        bool SusVProbeArmed()
        {
            const bool lbArmed = SusVProbeThreshold() > 0.0f && CgsDev::Log::gpDebugPrint != NULL;
            static bool sbAnnounced = false;
            if (lbArmed && !sbAnnounced)
            {
                sbAnnounced = true;
                *CgsDev::Log::gpDebugPrint
                    << "[susv] ARMED at threshold " << SusVProbeThreshold()
                    << " m/s -- one line per UpdateSuspensionPostSimulation call whose whole-function"
                       " |dv| crosses it. No rows after this line means the threshold was never"
                       " crossed; no ARMED line at all means the probe was never on.\n";
            }
            return lbArmed;
        }

        // Filled by StabiliseAfterHardLanding so the caller's ledger can separate that function's
        // OWN two halves -- its entry CalculateNewVelocity (which drains the ordinary accumulators)
        // from its direct overwrite of mLinearVelocity. Without this split the whole of
        // StabiliseAfterHardLanding reads as one opaque stage and the very distinction the probe
        // was built to make would be lost.
        Vector3 gvSusVStabAfterIntegrate = { 0.0f, 0.0f, 0.0f, 0.0f };
        bool    gbSusVStabDamped         = false;
        f32     gfSusVStabDampFactor     = 0.0f;
        f32     gfSusVStabVDotN          = 0.0f;

        f32 SusVMag(const Vector3& lrA, const Vector3& lrB)
        {
            const f32 lfX = lrA.x - lrB.x, lfY = lrA.y - lrB.y, lfZ = lrA.z - lrB.z;
            return static_cast<f32>(std::sqrt(static_cast<double>(lfX * lfX + lfY * lfY + lfZ * lfZ)));
        }
    }
    // ---- end [susv] helpers ---------------------------------------------------------------------
    // ---- end [wsus] helpers ---------------------------------------------------------------------

// [clean] UpdateSuspension  @0x8261F698
    // @0x8261F698  BrnPhysics::Vehicle::VehiclePhysics::UpdateSuspension  (virtual)
    //   The driving-path suspension spine. The X360 first advances the hard-landing timer: it loads the
    //   +0x4208 register (= the +0x1070 drift-bank register, mvTimeSinceHardLanding_...), splats its .x
    //   lane and adds dt (vspltw v0,v0,0 ; vaddfp v0,v0,v127 ; vrlimi128 lane0 ; stvx128) -- i.e.
    //   TimeSinceHardLanding += dt. It then runs the four suspension phases in order, snapshotting the
    //   +0x50 weight register into the +4912 mirror between UpdateSuspensionSprings and
    //   CalculateWeightTransfer (lvx128 r31,0x50 ; stvx128 r31,4912).
    void VehiclePhysics::UpdateSuspension(VecFloat lvfTimeStep)
    {
        const f32 lfDeltaTime = lvfTimeStep.x;

        // Advance the hard-landing timer (TimeSinceHardLanding lane .x of the +0x1070 register).
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.x += lfDeltaTime;

        // 1) push the static chassis weight down each wheel's contact normal.
        ApplyWheelWeight(lvfTimeStep);

        // 2) set each spring's stiffness/mass/damping/velocity/position from the current ride state
        // and integrate it one step. dt IS AN ARGUMENT: the X360 parks the incoming vector
        //    (0x8261F6B4 `vmr128 v127,v1`) and re-issues `vmr128 v1,v127` before every phase call,
        //    including this one at 0x8261F6D8.
        UpdateSuspensionSprings(lvfTimeStep);

        // 3) snapshot mLinearVelocity (+0x50 == base+0x40) into mPreviousWorldSpaceVelocity (+0x1330),
        //    so CalculateWeightTransfer can difference it against this frame's velocity.
        //    asm @0x8261F6E4-0x8261F6F8: `lvx128 v0,this,0x50 ; stvx128 v0,this,0x1330`.
        mPreviousWorldSpaceVelocity = Vector3{ mLinearVelocity.x, mLinearVelocity.y, mLinearVelocity.z, 0.0f };

        // 4) build the dynamic load-transfer external force per spring -- and, the finding that
        //    unblocked the whole tyre model, WRITE EACH WHEEL'S MassOnWheel LANE. dt is re-issued
        //    into v1 for this call exactly as for the others (0x8261F6EC `vmr128 v1,v127`).
        // Note the ordering above: step 3 makes the delta this function differences ZERO.
        CalculateWeightTransfer(lvfTimeStep);

        // 5) emit the spring push forces + recompute velocity.
        ApplySuspensionForces(lvfTimeStep);

        // ---- [wsus] the per-step suspension witness (player car only) ------------------------
        if (WheelSusProbeArmed() && mPreviousControls.GetType() == E_DRIVER_TYPE_PLAYER)
        {
            u32& lruFrame = WheelSusFrameFor(this);
            ++lruFrame;
            const u32 luF = lruFrame;
            guWheelSusFrame = luF;
            const u32 luCar = static_cast<u32>(reinterpret_cast<u64>(this));

            if (WheelSusTakeLine())
            {
                *CgsDev::Log::gpDebugPrint
                    << "[wsus] f " << static_cast<s32>(luF)
                    << " car " << luCar
                    << " pos " << mTransform.Pos().x << " " << mTransform.Pos().y << " " << mTransform.Pos().z
                    << " vel " << mLinearVelocity.x << " " << mLinearVelocity.y << " " << mLinearVelocity.z
                    << " mph " << mfSpeedMPH.x
                    << " ang " << mAngularVelocity.x << " " << mAngularVelocity.y << " " << mAngularVelocity.z
                    << " up " << mTransform.Up().x << " " << mTransform.Up().y << " " << mTransform.Up().z
                    << " at " << mTransform.At().x << " " << mTransform.At().y << " " << mTransform.At().z
                    << " air " << (mbHasAir ? 1 : 0)
                    << " crash " << (mbCrashing ? 1 : 0)
                    << " nwc " << static_cast<s32>(mi8NumWorldCollisions)
                    << " dt " << lfDeltaTime
                    << "\n";
            }

            for (s32 liW = 0; liW < eNumDrivenWheels && WheelSusTakeLine(); ++liW)
            {
                const Wheel& lrW = maWheels[liW];
                const Wheel::RoadContact& lrC = lrW.GetRoadContact();
                const SuspensionSpring& lrS = maSprings[liW];
                // What ApplySuspensionForces pushed with this frame (mass * acceleration, > 0 only,
                // traction only) -- recomputed from the same lanes it read.
                const f32 lfPush = (lrW.mbHasTraction && lrW.mu8State != 2)
                    ? lrS.mvStiffness_Damping_Mass_Position.z
                      * lrS.mvVelocity_Acceleration_DampingForce_SpringForce.y
                    : 0.0f;
                *CgsDev::Log::gpDebugPrint
                    << "[wsus-w] f " << static_cast<s32>(luF) << " car " << luCar << " w " << liW
                    << " onG " << (lrC.mbIsOnGround ? 1 : 0)
                    << " close " << (lrC.mbIsCloseToGround ? 1 : 0)
                    << " hasT " << (lrW.mbHasTraction ? 1 : 0)
                    << " st " << static_cast<s32>(lrW.mu8State)
                    << " hit " << lrC.mPosition.x << " " << lrC.mPosition.y << " " << lrC.mPosition.z
                    << " n " << lrC.mNormal.x << " " << lrC.mNormal.y << " " << lrC.mNormal.z
                    << " ld " << lrC.mfLineDistanceToRoad
                    << " wy " << lrW.mPosition.y
                    << " sy " << lrW.mStreamedPositionPlusTwistAmount.y
                    << " tdn " << lrW.mSuspensionAndInertiaVariables.x
                    << " tup " << lrW.mSuspensionAndInertiaVariables.y
                    << " k " << lrS.mvStiffness_Damping_Mass_Position.x
                    << " c " << lrS.mvStiffness_Damping_Mass_Position.y
                    << " m " << lrS.mvStiffness_Damping_Mass_Position.z
                    << " p " << lrS.mvStiffness_Damping_Mass_Position.w
                    << " v " << lrS.mvVelocity_Acceleration_DampingForce_SpringForce.x
                    << " a " << lrS.mvVelocity_Acceleration_DampingForce_SpringForce.y
                    << " Fd " << lrS.mvVelocity_Acceleration_DampingForce_SpringForce.z
                    << " Fs " << lrS.mvVelocity_Acceleration_DampingForce_SpringForce.w
                    << " push " << (lfPush > 0.0f ? lfPush : 0.0f)
                    << " mow " << lrW.mSpeedAndMassOnWheelVariables.z
                    << "\n";
            }

            if ((luF % 600u) == 1u && WheelSusTakeLine())
            {
                const VehicleAttribs::SuspensionAttribs& lrSus = mpAttribs->mSuspensionAttribs;
                *CgsDev::Log::gpDebugPrint
                    << "[wsus-attr] f " << static_cast<s32>(luF)
                    << " car " << luCar
                    << " mass " << mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x
                    << " mfMass " << mfMass.x
                    << " rest " << lrSus.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.x
                    << " damp " << lrSus.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.y
                    << " upMov " << lrSus.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.z
                    << " dnMov " << lrSus.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.w
                    << " frontOff " << lrSus.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.x
                    << " rearOff " << lrSus.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.y
                    << " inAir " << lrSus.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.z
                    << " scal " << mvSpringMassScalers.x << " " << mvSpringMassScalers.y
                    << " " << mvSpringMassScalers.z << " " << mvSpringMassScalers.w
                    << " rad " << maWheels[0].mSlipVariables.w << " " << maWheels[1].mSlipVariables.w
                    << " " << maWheels[2].mSlipVariables.w << " " << maWheels[3].mSlipVariables.w
                    << " unsprung " << maWheels[0].mIntegrationVariables.w
                    << " tll " << mSimpleAttribs.mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.w
                    << "\n";
            }
        }
        // ---- end [wsus] -----------------------------------------------------------------------
    }

// [clean] ApplyWheelWeight  @0x825F7898
    // @0x825F7898  BrnPhysics::Vehicle::VehiclePhysics::ApplyWheelWeight   (149 insns)
    // =============================================================================================
    // THE "PARTIAL" VERDICT THAT STOOD HERE
    // IS RETRACTED, AND IT WAS WRONG IN A WAY THAT MATTERED: it claimed the sources were "wheel
    // +0x180/+0x1A0-region offsets, well past the committed Wheel layout" and the destination "a
    // wheel-internal suspension-length lane NOT pinned in this minimal slice". EVERY offset this
    // function touches is a committed member, and the un-pinned addresses were a mis-attribution --
    // the walker's base register is `this + 0x1B0`, which is maWheels[0].mPosition (maWheels @0x130
    // + 0x80), not a raw this-relative address in some unmapped region. The same trap as
    // [[junkyard-state-writer-found]]: an offset read against the wrong base.
    //
    // WHY IT MATTERS EXACTLY: **this writes maWheels[i].mPosition.y, which is the one and only
    // input the grounded arm of UpdateSuspensionSprings reads** (0x825F874C `lvx128 v12,[wheel+0x80]`
    // ; `vspltw v12,v12,1`). With this inert the spring's position never changes, Hooke's law
    // returns a constant, and the car cannot settle no matter how correct the spring solver is.
    // It is not "push static chassis weight down the contact normal" (that is
    // ApplySuspensionForces, the LAST phase); it is the suspension-compression solve, and the
    // asserts in the sibling call its output "suspension length".
    //
    // READ STORE-FOR-STORE (r31 = &maWheels[i].mPosition, stepping 0xE0; r11 = this + 0x10):
    //   gate   `lbz r11, -0x58(r31)`  == maWheels[i].mRoadContact.mbIsOnGround  (0x1B0-0x58 = 0x158)
    //   0x825F7A80/AA4/AA8   world = xAxis*p.x + yAxis*p.y + zAxis*p.z + Pos()
    //                        (three vmaddfp off [this+0x10]/[+0x20]/[+0x30], seeded with [this+0x40]
    //                         -- the translation IS included here, because this is a POINT; the
    //                         sibling's lever-arm transform deliberately omits it)
    //   0x825F7AAC           world -= mRoadContact.mPosition        (the 48-byte contact copy)
    //   0x825F7AB0           gap = dot3([this+0x20] == mTransform.Up(), that)
    //   0x825F7AB4/AB8       -(gap - mSlipVariables.w)              (the WHEEL RADIUS -- Wheel::
    //                        Prepare's committed lane map, `f1 -> mSlipVariables.w = THE WHEEL RADIUS`)
    //   0x825F7ABC           += mPosition.y
    //   0x825F7AC0           vmaxfp against mSuspensionAndInertiaVariables.x  (= streamedY - travelDown,
    //                        again Wheel::Prepare's committed lane map: the travel-DOWN bound)
    //   0x825F7AC4/AC8       vrlimi128 mask 4 (lane 1) ; stvx128 -> mPosition.y
    // ⇒ raise the wheel, in body space, by exactly how far it is penetrating the road, and stop at
    // the down-travel bump stop. Nothing here is fabricated; the two "magic" lanes are both named
    // by the lane map this tree already proved from Wheel::Prepare's own asm.
    // The per-wheel CgsDev::Assert "Invalid wheel position" (Wheel.h:412) guards are elided as
    // debug-build plumbing, per the project convention.
    // =============================================================================================
    void VehiclePhysics::ApplyWheelWeight(VecFloat lvfTimeStep)
    {
        (void)lvfTimeStep; // v1 is live at entry in the X360 ABI, but this body does not consume it.
        const Vector3& lvRight = mTransform.Right();   // [this+0x10]
        const Vector3& lvUp    = mTransform.Up();      // [this+0x20] -- also the projection axis
        const Vector3& lvAt    = mTransform.At();      // [this+0x30]
        const Vector3& lvPos   = mTransform.Pos();     // [this+0x40]

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];

            // Gate: only wheels actually touching the road (X360 `lbz r11,-0x58(r31)`).
            if (!lrWheel.GetRoadContact().mbIsOnGround)
                continue;

            // (debug-only "Invalid wheel position" assert on the wheel's position -- elided.)

            // The wheel's body-space position taken out to world space (point, so + translation).
            const Vector3& lvLocal = lrWheel.mPosition;
            const f32 lfWorldX = lvRight.x * lvLocal.x + lvUp.x * lvLocal.y + lvAt.x * lvLocal.z + lvPos.x;
            const f32 lfWorldY = lvRight.y * lvLocal.x + lvUp.y * lvLocal.y + lvAt.y * lvLocal.z + lvPos.y;
            const f32 lfWorldZ = lvRight.z * lvLocal.x + lvUp.z * lvLocal.y + lvAt.z * lvLocal.z + lvPos.z;

            // Signed distance from the contact point to the wheel centre, measured along the body
            // up axis (X360: vsubfp then vmsum3fp128 against [this+0x20]).
            const Vector3& lvContact = lrWheel.GetRoadContact().mPosition;
            const f32 lfGap = lvUp.x * (lfWorldX - lvContact.x)
                            + lvUp.y * (lfWorldY - lvContact.y)
                            + lvUp.z * (lfWorldZ - lvContact.z);

            // Raise the wheel by its penetration (radius - gap), clamped at the down-travel stop.
            const f32 lfWheelRadius   = lrWheel.mSlipVariables.w;                  // wheel+0x40 .w
            const f32 lfTravelDownEnd = lrWheel.mSuspensionAndInertiaVariables.x;  // wheel+0x60 .x
            const f32 lfNewY = lrWheel.mPosition.y + (lfWheelRadius - lfGap);

            lrWheel.mPosition.y = (lfNewY > lfTravelDownEnd) ? lfNewY : lfTravelDownEnd;
        }
    }

// [clean] CalculateWeightTransfer  @0x825F9DD0
    // @0x825F9DD0  BrnPhysics::Vehicle::VehiclePhysics::CalculateWeightTransfer   (296 insns)
    // =============================================================================================
    // THIS IS THE MassOnWheel WRITER. Fifty-plus waves built a complete, correct tyre model
    // that multiplied `maWheels[i].mSpeedAndMassOnWheelVariables.z` -- the DWARF MassOnWheel lane --
    // and NOTHING IN THE TREE WROTE IT, so every tyre force was identically zero on every frame
    // whatever the slip. The writer is here, and it is the `vrlimi128 mask 2` store the last wave
    // predicted -- just not in a suspension setup routine. It is in the PER-FRAME weight-transfer
    // pass, 0x825FA1FC..0x825FA21C:
    //     lvx128    v13, r0, r10        ; r10 = this+0xED0  == mvSpringMassScalers
    //     vperm     v13, v13, v13, v7   ; v7 = vspltw(lvsl(0, i*4), 0)  -> splat lane i
    //     vmulfp128 v10, v10, v0        ; v10 = 0.10193679 * delta[i]
    //     vmaddfp   v13, v11, v10, v13  ; v13 = mass*scaler[i] + 0.10193679*delta[i]
    //     vrlimi128 v12, v13, 2, 2      ; v12.z <- v13.x           <<< MASK 2 == THE Z LANE
    //     stvx128   v12, r0, r30        ; r30 = this+0x1A0 + i*0xE0
    //                                   ;     = &maWheels[i].mSpeedAndMassOnWheelVariables
    // HOW IT WAS FOUND, and why five earlier waves missed it: `SetMassOnWheel` is in NO X360
    // export name (a trivial inlined setter), so a NAME search can never reach it. Scanning all
    // 30,084 exports' ASSEMBLY for the offset `0xED0` -- SetupSuspension's OUTPUT, i.e. the thing
    // whoever fills the lane must read -- returns four physics functions, and one of them is
    // named WeightTransfer. [[unnamed-sub-bodies-and-env-faults]]: search for the DATA, not the name.
    //
    // THE OPERAND ORDER IS READ FROM THE IMAGE, NOT FROM IDA'S PRINTING. Every semantic here
    // turns on how `vmaddfp`/`vnmsubfp` group their operands, and IDA prints VA-form in ENCODING
    // order (vD, vA, vB, vC) while the ISA multiplies vA*vC and adds vB. Raw words (x360rd, self
    // test 10/10):
    //     0x825FA214 = 11AB536E -> opcd 4, xo 46, vD=13 vA=11 vB=10 vC=13  => v13 = v11*v13 + v10
    //     0x825F9F08 = 112849EE -> vD=9  vA=8  vB=9  vC=7                  => the 3x3 product
    //     0x825F9E34 = 116D006F -> xo 47 (vnmsubfp), vD=11 vA=13 vB=0 vC=1 => 1 - e*d  (Newton)
    // Read the other way the reciprocal cascade does not converge and the 3x3 is not a matrix
    // product, so this reading is FORCED, not chosen. The store's lane is decoded the same way:
    //     0x825FA218 = 19826F90 -> VMX128_4: vD=12, vB=13, mask(b12..15)=2 (the z lane),
    //                              rotate(b24..25)=2  => dest.z = src.x.
    //
    // THE ATTRIBUTE LANES ARE NAMED BY THE DWARF AND THEY MATCH THE DECODE LANE FOR LANE.
    // mpAttribs+0x260 is already in this tree as BodyRollAttribs::
    // mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ, and the asm uses
    // .x/.y as the two per-frame DECAYS, .z/.w as the two per-axis GAINS *and* the two clamp
    // limits. Three independent witnesses (asm dataflow, physical units, DWARF names) agree.
    // The units scalar 0.10193679 (flt_82097C60, image-read) is 1/9.8100004 == 1/g exactly: it
    // converts the transfer FORCE into a MASS, which is what closes the units on the lane.
    // The permute unk_82CDA350 is READ, not guessed: `00 01 02 03 | 14 15 16 17 | 00 01 02 03 |
    // 00 01 02 03` == {lateral, 0.0, lateral, lateral}, whose z lane is then overwritten by the
    // longitudinal term (vrlimi128 mask 2) -- so only .x and .z carry meaning.
    //
    // AND THE THING THIS FUNCTION'S NAME PROMISES DOES NOT HAPPEN. The transfer is driven by
    // `(mLinearVelocity - mPreviousWorldSpaceVelocity) / dt`, and the SOLE caller, UpdateSuspension
    // @0x8261F698, copies the one into the other IMMEDIATELY BEFORE the call:
    //     0x8261F6F4  lvx128  v0, r31, 0x50      ; mLinearVelocity
    //     0x8261F6F8  stvx128 v0, r31, 0x1330    ; mPreviousWorldSpaceVelocity
    //     0x8261F6FC  bl      CalculateWeightTransfer
    // so the delta is structurally ZERO, the body acceleration is zero, both increments are zero,
    // and mWeightTransfer decays from zero to zero forever. ⇒ **the shipped X360 game has no
    // load transfer under braking or cornering**, and massOnWheel[i] is exactly the STATIC per
    // corner mass, recomputed every frame. All the machinery (decay, gains, geometry, clamps) is
    // real and reached; its one input is killed by the caller. That is emitted faithfully here --
    // this is a transcription, not a repair. The lvx128 destination and the vsubfp128 source are
    // the same VMX128 register (low5 == 30 in both raw words), so the delta is not a mis-read.
    // VERIFIED AT RUNTIME (this wave): the probe printed dV = (0,0,0) and W = (0,0,0) on every
    // sampled frame, and massOnWheel came out as mass*scaler to the last bit.
    //
    // The geometry is all read out of the wheels' STREAMED rest positions (this+0x1C0/0x380/0x460
    // == maWheels[0/2/3].mStreamedPositionPlusTwistAmount, and this+0x170/0x330 ==
    // maWheels[0/2].mSlipVariables.w == the wheel radii, per Wheel::Prepare's proven lane map):
    // the CoM ride height is the front/rear |radius - restY| pair lerped at the CoM's longitudinal
    // station, over the front-to-rear wheelbase and the rear track.
    // The four `vandc <v>, <0x80000000 splat>` are fabsf, as in SetupSuspension.
    // The reciprocals are vrefp + two Newton refinements == a plain divide; the console does not
    // guard any of them, and neither does this (no fabricated clamp).
    // =============================================================================================
    void VehiclePhysics::CalculateWeightTransfer(VecFloat lvfTimeStep)
    {
        // flt_82001CC0 / flt_82097C60, both read out of the X360 image this wave.
        static const f32 KF_ZERO           = 0.0f;
        static const f32 KF_RECIP_GRAVITY  = 0.10193679f;   // == 1/9.8100004, the force -> mass unit

        const VehicleAttribs::BodyRollAttribs& lrRoll = mpAttribs->mBodyRollAttribs;
        const f32 lfDecayX =
            lrRoll.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.x;
        const f32 lfDecayZ =
            lrRoll.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.y;
        const f32 lfFactorOfWeightX =
            lrRoll.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.z;
        const f32 lfFactorOfWeightZ =
            lrRoll.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.w;

        // attribs+0x70 .x -- the SAME body mass SetupSuspension multiplies the scalers by before it
        // hands them to SuspensionSpring::Prepare, which is what makes mass*scaler a mass in kg.
        const f32 lfMass =
            mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;

        // ---- 1) decay the running transfer, and hard-zero the y lane (0x825F9E3C..0x825FA0A0).
        mWeightTransfer.x *= lfDecayX;
        mWeightTransfer.y  = KF_ZERO;
        mWeightTransfer.z *= lfDecayZ;

        // ---- 2) the body-space acceleration (0x825F9DF4 + 0x825F9EA4..0x825F9F0C).
        //         The console's vmrghw/vmrglw pair transposes the orientation 3x3 and multiplies it
        //         by the world acceleration, which is exactly the three row dots below.
        const f32 lfRecipTimeStep = 1.0f / lvfTimeStep.x;   // vrefp + 2 Newton refinements

        const f32 lfWorldAccelX =
            (mLinearVelocity.x - mPreviousWorldSpaceVelocity.x) * lfRecipTimeStep;
        const f32 lfWorldAccelY =
            (mLinearVelocity.y - mPreviousWorldSpaceVelocity.y) * lfRecipTimeStep;
        const f32 lfWorldAccelZ =
            (mLinearVelocity.z - mPreviousWorldSpaceVelocity.z) * lfRecipTimeStep;

        const f32 lfBodyAccelX = mTransform.xAxis.x * lfWorldAccelX
                               + mTransform.xAxis.y * lfWorldAccelY
                               + mTransform.xAxis.z * lfWorldAccelZ;
        const f32 lfBodyAccelZ = mTransform.zAxis.x * lfWorldAccelX
                               + mTransform.zAxis.y * lfWorldAccelY
                               + mTransform.zAxis.z * lfWorldAccelZ;

        // The two per-axis gains (0x825F9F28 / 0x825F9F5C).
        const f32 lfScaledAccelLat  = lfBodyAccelX * lfFactorOfWeightX;
        const f32 lfScaledAccelLong = lfBodyAccelZ * lfFactorOfWeightZ;

        // ---- 3) the chassis geometry, from the STREAMED rest positions + the wheel radii
        //         (0x825F9F4C..0x825FA030). Wheel 0 is front-left, 2 rear-left, 3 rear-right.
        const f32 lfHeightFront = std::fabs(maWheels[0].mSlipVariables.w
                                          - maWheels[0].mStreamedPositionPlusTwistAmount.y);
        const f32 lfHeightRear  = std::fabs(maWheels[2].mSlipVariables.w
                                          - maWheels[2].mStreamedPositionPlusTwistAmount.y);
        const f32 lfWheelbase   = std::fabs(maWheels[2].mStreamedPositionPlusTwistAmount.z
                                          - maWheels[0].mStreamedPositionPlusTwistAmount.z);
        const f32 lfTrack       = std::fabs(maWheels[2].mStreamedPositionPlusTwistAmount.x
                                          - maWheels[3].mStreamedPositionPlusTwistAmount.x);

        const f32 lfRecipWheelbase = 1.0f / lfWheelbase;
        const f32 lfRecipTrack     = 1.0f / lfTrack;

        // The CoM ride height: the front/rear heights lerped at the front axle's longitudinal
        // station (0x825FA02C/0x825FA030 -- `vmaddfp v8, v7, v8, v10` == v7*v10 + v8).
        const f32 lfCoMHeight =
            (lfHeightRear - lfHeightFront)
                * (maWheels[0].mStreamedPositionPlusTwistAmount.z * lfRecipWheelbase)
            + lfHeightFront;

        // ---- 4) the increment: m * a * h / L, per axis. The multiply association mirrors the asm
        //         (0x825FA05C..0x825FA068 and 0x825FA090..0x825FA098).
        const f32 lfLongitudinal = ((lfRecipWheelbase * lfCoMHeight) * lfMass) * lfScaledAccelLong;
        const f32 lfLateral      = ((lfRecipTrack     * lfCoMHeight) * lfMass) * lfScaledAccelLat;

        // The unk_82CDA350 vperm + the mask-2 vrlimi128 + the vaddfp at 0x825FA09C..0x825FA0A8.
        // The .w lane is DEAD -- nothing reads it -- but the console writes it, so it is written.
        mWeightTransfer.x += lfLateral;
        mWeightTransfer.y += KF_ZERO;
        mWeightTransfer.z += lfLongitudinal;
        mWeightTransfer.w += lfLateral;

        // ---- 5) the per-axis saturation (0x825FA0E4..0x825FA12C). vmaxfp then vminfp against
        //         +-(gain * mass) -- the same two attribute lanes that gained the accelerations.
        const f32 lfLimitX = lfFactorOfWeightX * lfMass;
        const f32 lfLimitZ = lfFactorOfWeightZ * lfMass;

        mWeightTransfer.x = (mWeightTransfer.x < -lfLimitX) ? -lfLimitX : mWeightTransfer.x;
        mWeightTransfer.x = (lfLimitX < mWeightTransfer.x) ?  lfLimitX : mWeightTransfer.x;
        mWeightTransfer.z = (mWeightTransfer.z < -lfLimitZ) ? -lfLimitZ : mWeightTransfer.z;
        mWeightTransfer.z = (lfLimitZ < mWeightTransfer.z) ?  lfLimitZ : mWeightTransfer.z;

        // ---- 6) the per-corner split (0x825FA134..0x825FA1A0). The lateral term flips sign
        //         left/right, the longitudinal term flips front/rear -- which is what identifies
        //         .x as the roll (lateral) transfer and .z as the pitch (longitudinal) one.
        const f32 lafCornerTransfer[eNumDrivenWheels] =
        {
             mWeightTransfer.x - mWeightTransfer.z,   // 0 front-left
            -mWeightTransfer.x - mWeightTransfer.z,   // 1 front-right
             mWeightTransfer.x + mWeightTransfer.z,   // 2 rear-left
            -mWeightTransfer.x + mWeightTransfer.z,   // 3 rear-right
        };

        // ---- 7) THE STORE. Gate: `lbz r11, -0x48(r30)` == wheel+0x28 == mRoadContact.mbIsOnGround.
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            if (!maWheels[liWheel].GetRoadContact().mbIsOnGround)
                continue;

            const f32 lfScaler = (liWheel == 0) ? mvSpringMassScalers.x
                               : (liWheel == 1) ? mvSpringMassScalers.y
                               : (liWheel == 2) ? mvSpringMassScalers.z
                                                : mvSpringMassScalers.w;

            // THE LANE. `vmaddfp v13, v11, v10, v13` == v11*v13 + v10 == mass*scaler[i] plus the
            // transfer force converted to a mass by 1/g, then `vrlimi128 v12, v13, 2, 2` puts it in
            // the z lane of the wheel's +0x70 register.
            maWheels[liWheel].mSpeedAndMassOnWheelVariables.z =
                lfMass * lfScaler + KF_RECIP_GRAVITY * lafCornerTransfer[liWheel];

            // The spring's external force: the console reads the CURRENT lane and SUBTRACTS the
            // corner transfer (`lvx128 v13,[spring+0x20] ; vspltw ; vaddfp v1, v13, -delta`).
            // UpdateSuspensionSprings consumes and clears this lane earlier in the same frame
            // (SetExternalForce(0) @0x825F9084), so the read is 0 in practice -- but the read is
            // what the console does, so it is what is emitted.
            maSprings[liWheel].SetExternalForce(
                maSprings[liWheel].mvExternalForce.x - lafCornerTransfer[liWheel]);
        }


        // ---- 8) the tail store (0x825FA248): snapshot the velocity for the next frame. Redundant
        //         with the caller's own pre-call copy -- the console issues both.
        mPreviousWorldSpaceVelocity.x = mLinearVelocity.x;
        mPreviousWorldSpaceVelocity.y = mLinearVelocity.y;
        mPreviousWorldSpaceVelocity.z = mLinearVelocity.z;
        mPreviousWorldSpaceVelocity.w = mLinearVelocity.w;
    }

// [clean] ApplySuspensionForces  @0x825D1EE8
    // @0x825D1EE8  BrnPhysics::Vehicle::VehiclePhysics::ApplySuspensionForces
    // =============================================================================================
    // THREE THINGS WERE WRONG HERE, AND THE
    // WORST OF THEM LAUNCHED THE CAR AT 91 m/s THE MOMENT THE MAGNITUDE STOPPED BEING ZERO.
    // Every one was invisible while `mag` was identically 0 -- [[silent-drop-stubs]]: "not on the
    // live path" expires silently, and it expired this wave.
    //
    // 1. THE LEVER ARM. The X360 passes **maWheels[i].mPosition** -- the BODY-SPACE wheel
    //    position, which is why r5 == 1 (BODY_SPACE) -- straight out of the register the whole
    //    function walks (`r27 = this + 0x1B0`, i.e. &maWheels[0].mPosition, stepping 0xE0):
    //        0x825D20F8  vmr128 v1, v127          ; the force
    //        0x825D20FC  li     r5, 1             ; position space = BODY
    //        0x825D2100  li     r4, 0             ; force space    = WORLD
    //        0x825D2104  lvx128 v2, r0, r27       ; <- maWheels[i].mPosition
    //        0x825D2108  addi   r3, r20, 0x10
    //        0x825D210C  bl     ExternalPhysicsBody::AddLocalForce
    //    The committed body passed `mRoadContact.mPosition`, which is a WORLD position -- in the
    //    junkyard, (2986, -3.5, -2012). Declared BODY_SPACE, that is a **~3,600 m lever arm**, so
    //    a perfectly correct 4,933 N suspension push became ~1.8e7 N.m of torque, the body spun
    //    up in a single step, and the suspension-point velocity (omega x r) reached ~860 m/s.
    //    MEASURED, not reasoned: with this wrong the car left the ground at +91.7 m/s on step 21.
    //
    // 2. THE DIRECTION is the NORMALIZED BODY UP AXIS, not the contact normal:
    //        0x825D1FB4  lvx128 v13, r20, 0x20        ; mTransform.Up()
    //        0x825D1FD4  vmsum3fp128 v0, v13, v13     ; |up|^2
    //        0x825D1FFC..0x825D201C  vrsqrtefp + two Newton refinements
    //        0x825D2020  vmulfp128 v0, v13, v0        ; up * (1/|up|)
    //        0x825D2024  vmulfp128 v127, v0, v9       ; * magnitude
    //    The old note guessed "the normalized contact normal ... FLAG if the +0x1B0 lane proves
    //    distinct". It is distinct, and the +0x1B0 "un-pinned region" was never un-pinned -- it is
    //    maWheels[0].mPosition (maWheels @0x130 + 0x80). Same mis-based-offset trap as the one in
    //    ApplyWheelWeight's old banner.
    //
    // 3. THE GATE is **mbHasTraction**, not mbIsOnGround: `lbz r11,0x56(r27)` reads wheel+0xD6
    //    (0x1B0+0x56 = 0x206 = wheel base 0x130 + 0xD6), and `lbz r11,0x57(r27)` is mu8State.
    //    On flat ground the two agree (SetRoadContact derives traction from normal.y > 0.5), which
    //    is exactly why it never showed -- it would only have shown on a steep wall.
    //
    // UNCHANGED and already correct: the magnitude is maSprings[i].reg0.z * reg1.y (mass x
    // acceleration -- so it IS the net spring force), gated > 0 by `fcmpu cr6,f0,f31 ; ble`.
    // The trailing ExternalPhysicsBody::CalculateNewVelocity(this+0x10) checkpoint stays a
    // faithful comment (base-owned, not declared on this slice), as before.
    // The two per-wheel "Invalid wheel position" (Wheel.h:412) asserts are elided -- debug.
    //
    // ⭐ RE-VERIFIED 2026-09-03 (drive-spine 1:1 audit), instruction by instruction against the
    // 225-instruction body. Everything above holds: the two gate bytes, the mass x acceleration
    // magnitude and its `ble` on > 0, the vrsqrtefp + two-Newton normalise of the body up axis, the
    // WORLD force / BODY position argument pair, the wheel-position lever arm, the four-iteration
    // loop (r18 0x3B0..0x3E0 step 0x10, r27 += 0xE0, r16 += 0x30) and the tail CalculateNewVelocity.
    // ONE FURTHER ELISION, named here so the list is complete: the console also records the frame's
    // result into a debug/telemetry block behind a null-checked pointer at this+0x13E4 --
    // `stvx128 v127, r18, r11` writes the force at block+0x3B0+i*0x10 (0x825D220C) and
    // `stb 1, 0x3F0(r11+i)` / `stb 0, 0x3F0(r11+i)` sets or clears that wheel's applied flag
    // (0x825D2210 / 0x825D2228). Nothing in the sim reads it; it is the debug component's buffer,
    // elided on the same footing as the two asserts. NOT a silent drop -- recorded, not forgotten.
    // =============================================================================================
    void VehiclePhysics::ApplySuspensionForces(VecFloat lvfTimeStep)
    {
        // The push direction: the body up axis, normalized (0x825D1FB4..0x825D2020). Hoisted --
        // the console re-derives it per wheel, but it cannot change inside the loop.
        const Vector3& lvUp = mTransform.Up();
        const f32 lfUpMagSq = lvUp.x * lvUp.x + lvUp.y * lvUp.y + lvUp.z * lvUp.z;
        const f32 lfRecipUpMag = 1.0f / std::sqrt(lfUpMagSq);
        const Vector3 lvUpUnit{ lvUp.x * lfRecipUpMag, lvUp.y * lfRecipUpMag, lvUp.z * lfRecipUpMag, 0.0f };

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];

            // 0x825D1F68 / 0x825D1F74: wheel+0xD6 (mbHasTraction) and wheel+0xD7 (mu8State).
            if (!lrWheel.mbHasTraction)
                continue;
            if (lrWheel.mu8State == 2)
                continue;

            // Push magnitude = maSprings[i].reg0.z (mass lane) * maSprings[i].reg1.y (acceleration
            // lane) -- i.e. the net spring force. (0x825D1F80..0x825D1F94, then `ble` on > 0.)
            const f32 lfMagnitude =
                maSprings[liWheel].mvStiffness_Damping_Mass_Position.z *
                maSprings[liWheel].mvVelocity_Acceleration_DampingForce_SpringForce.y;

            if (lfMagnitude <= 0.0f)
                continue;   // no push this wheel

            // (debug-only 'Invalid wheel position' assert on mPosition -- elided.)

            const Vector3 lvForce{ lvUpUnit.x * lfMagnitude,
                                   lvUpUnit.y * lfMagnitude,
                                   lvUpUnit.z * lfMagnitude,
                                   0.0f };
            // asm @0x825D2100/0x825D20FC: r4 = 0 (WORLD force), r5 = 1 (BODY position), and
            // 0x825D2104 loads the position from r27 == &maWheels[i].mPosition.
            AddLocalForce(lvForce, rw::physics::WORLD_SPACE,
                          lrWheel.mPosition, rw::physics::BODY_SPACE);
        }

        // 0x825D211C-0x825D2124 restores the incoming v1 and tail-integrates after all wheel forces.
        CalculateNewVelocity(lvfTimeStep);
    }

    // @0x825D1890  BrnPhysics::Vehicle::VehiclePhysics::StabiliseAfterHardLanding
    // Assembly-first reconstruction. DecFIGS supplies the source locals and accessor names;
    // Breaker fixes every lane and branch: suspension+0x20.w is the damping window,
    // +0x10.w/+0x20.y/+0x20.z are the pitch/roll/vertical limits, and this+0x1070.x is
    // TimeSinceHardLanding. The large VMX block is the SDK pow implementation, not unknown
    // vehicle math: its base is min(timeFactor,maxVerticalDamping) and its exponent is
    // `BrnPhysics::kvfSixty * dt`. DecFIGS' static initializer @0x12DF24 pins kvfSixty to 60.0.
    void VehiclePhysics::StabiliseAfterHardLanding(VecFloat lvfTimeStep)
    {
        // @0x825D18C4: pending force/impulse accumulators are integrated before the window test.
        CalculateNewVelocity(lvfTimeStep);

        // [susv] the split point -- see the helper banner. Everything above this line is the
        // ordinary accumulator drain; everything below is this function's OWN writing.
        gvSusVStabAfterIntegrate = mLinearVelocity;
        gbSusVStabDamped         = false;
        gfSusVStabDampFactor     = 0.0f;
        gfSusVStabVDotN          = 0.0f;

        const VehicleAttribs::SuspensionAttribs& lrSuspension = mpAttribs->mSuspensionAttribs;
        const f32 lfSecondsToDampAfterLanding =
            lrSuspension
                .mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.w;
        const f32 lfTimeSinceHardLanding =
            mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.x;

        if (!(lfTimeSinceHardLanding < lfSecondsToDampAfterLanding))
            return;

        const f32 lfTimeDampingFactor =
            1.0f - (lfTimeSinceHardLanding / lfSecondsToDampAfterLanding);

        // @0x825D1968..199C: at least one front and at least one rear wheel on the road.
        const bool lbWheelsOnGround =
            (maWheels[eFrontLeftWheel].GetRoadContact().mbIsOnGround ||
             maWheels[eFrontRightWheel].GetRoadContact().mbIsOnGround) &&
            (maWheels[eRearLeftWheel].GetRoadContact().mbIsOnGround ||
             maWheels[eRearRightWheel].GetRoadContact().mbIsOnGround);

        // @0x825D199C..19CC: source local `lbFrontOnGround`. A valid ground normal which points
        // back against the car's forward axis selects the nose-first landing case.
        bool lbFrontOnGround = false;
        if (mAboveGroundTestResult.mbValid)
        {
            lbFrontOnGround =
                vpu::Dot(mTransform.zAxis, mAboveGroundTestResult.mIntersectionNormal) < 0.0f;
        }

        f32 lfPitchDamping = 0.0f;
        if (!lbFrontOnGround || lbWheelsOnGround)
        {
            lfPitchDamping = std::min(
                lfTimeDampingFactor,
                lrSuspension
                    .mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.w);
        }

        const f32 lfYawDamping = 0.0f;
        const f32 lfRollDamping = std::min(
            lfTimeDampingFactor,
            lrSuspension
                .mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.y);

        DampPitchYawRoll(
            VecFloat{ lfPitchDamping, lfPitchDamping, lfPitchDamping, lfPitchDamping },
            VecFloat{ lfYawDamping,   lfYawDamping,   lfYawDamping,   lfYawDamping },
            VecFloat{ lfRollDamping,  lfRollDamping,  lfRollDamping,  lfRollDamping },
            lvfTimeStep);

        if (mAboveGroundTestResult.mbValid)
        {
            const f32 lfMaxLinearVelocityDamping =
                lrSuspension
                    .mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.z;
            const f32 lfLinearVelocityDamping =
                std::min(lfTimeDampingFactor, lfMaxLinearVelocityDamping);
            const f32 lfDampingFactor =
                std::pow(lfLinearVelocityDamping, lvfTimeStep.x * 60.0f);

            const Vector3& lrRoadNormal = mAboveGroundTestResult.mIntersectionNormal;
            const f32 lfVelocityAlongNormal = vpu::Dot(mLinearVelocity, lrRoadNormal);
            mLinearVelocity = vpu::Subtract(
                mLinearVelocity,
                vpu::Mult(lrRoadNormal, lfVelocityAlongNormal * lfDampingFactor));
            // [susv] THE DIRECT OVERWRITE FIRED. Recorded, not printed: the caller's ledger owns
            // the print so the whole-function threshold can gate it.
            gbSusVStabDamped     = true;
            gfSusVStabDampFactor = lfDampingFactor;
            gfSusVStabVDotN      = lfVelocityAlongNormal;
        }
    }

// [clean] SetupSuspension  @0x825CF718
    // @0x825CF718  BrnPhysics::Vehicle::VehiclePhysics::SetupSuspension   (190 insns)
    // =============================================================================================
    // THE 2026-08-03 "BLOCKED" VERDICT THAT STOOD
    // HERE IS RETRACTED, AND IT WAS WRONG FOR A REASON WORTH RECORDING: it rested on four
    // "un-homed rodata" symbols. Three of them are not rodata VALUES at all, and the fourth is now
    // readable. Re-derived from the image this wave, nothing assumed:
    //   * `flt_8208F83C`  = **9.8100004196167**  -- GRAVITY. (A third independent witness for
    //     KF_GRAVITY, alongside BrnVehicleConstants.h:10-46 and the ReadUpdatedBodies wave.)
    //   * `flt_82001C98`  = **1.0**
    //   * `dword_8208FAFC[0..3]` = **{2, 3, 0, 1}** -- the SAME-SIDE, OTHER-AXLE wheel (FL<->RL,
    //     FR<->RR).
    //   * `dword_8208FAEC[0..3]` = **{1, 0, 3, 2}** -- the SAME-AXLE, OTHER-SIDE wheel (FL<->FR,
    //     RL<->RR).
    //   * `byte_8327F240` IS NOT A LOOP BOUND VALUE -- it is the END ADDRESS the walker is compared
    //     against (`cmpw cr6, r11, r6` with r11 stepping +0x40 from `unk_8327F140`), so the loop
    //     runs (0x8327F240 - 0x8327F140)/0x40 == **4** times: once per wheel. The old banner read
    //     it as a value.
    //   * `unk_8327F140` is a 4-entry (stride 0x40) table of `vperm` CONTROL vectors consumed as
    //     `vperm v0, <current mvSpringMassScalers>, <splat of the new scalar>, control` and stored
    //     straight back to +0xED0. Its SEMANTICS are pinned by that dataflow and by the destination's
    //     own DWARF name: **it inserts lane i**. Expressed here as the named lane write it is; no
    //     permute table is fabricated and none is needed.
    //
    // THE DESTINATION NAMES THE ALGORITHM. `this+0xED0` is `mvSpringMassScalers` (DWARF
    // VehiclePhysics.h:849, already declared) -- "the per-spring mass scalers" -- and what loop 1
    // builds is exactly a four-lane weight distribution that sums to 1 for a symmetric car.
    //
    // LOOP 1 (0x825CF824..0x825CF94C) -- the per-wheel weight split, read store-for-store.
    // The four staged vectors are built in the prologue from `maWheels[i] + 0x90`
    // (== Wheel::mStreamedPositionPlusTwistAmount; the four `lvx128` bases 0x1C0/0x2A0/0x380/0x460
    // are 0x130 + i*0xE0 + 0x90, i.e. the committed maWheels seat and stride):
    //     staged[i].lane0 = pos[i].x   (`vrlimi128 vD, vB, 8, 0`  -> lane 0, no rotate)
    //     staged[i].lane1 = pos[i].z   (`vrlimi128 vD, vB, 4, 1`  -> lane 1 of vB rotated left 1)
    // then, per wheel:
    //     dz     = staged[ FAFC[i] ].lane1 - staged[i].lane1      (0x825CF868 vsubfp + vspltw ,1)
    //     dx     = staged[ FAEC[i] ].lane0 - staged[i].lane0      (0x825CF870 vsubfp + vspltw ,0)
    //     ratioZ = | staged[i].lane1 * (1/dz) |                   (vrefp + TWO Newton refinements,
    //     ratioX = | staged[i].lane0 * (1/dx) |                    then `vandc` against a 0x80000000
    //                                                              splat == fabsf)
    //     mvSpringMassScalers[i] = (1 - ratioX) * (1 - ratioZ)    (0x825CF928 fmuls, then the vperm
    //                                                              lane insert at 0x825CF944/48)
    // SANITY, CHECKED BY HAND BEFORE WRITING: for a symmetric car (x = -+a, z = -+b) every ratio
    // is |-+a / (2*-+a)| = 0.5, so every lane is 0.25 and the four scalers sum to 1. An asymmetric
    // wheelbase biases the split toward the heavier end. That is what a mass scaler must do.
    //
    // LOOP 2 (0x825CF958..0x825CFA04) -- four springs, `maSprings[i]` at +0xE10 stride 0x30 (the
    // console spells the seat `(0x4B + i) * 0x30` == 3600 + i*48; both are the committed member).
    // The three arguments are recovered from the INTERLEAVE of the stores to var_F0 and the three
    // `lvlx`+`vspltw` reads of it -- read in address order, not source order:
    //     0x825CF9B4 var_F0 = scaler[i] * mpAttribs->mBaseAttribs.mv...Mass...x
    //     0x825CF9C8/CC  v3 (arg3, MASS)      <- that value
    //     0x825CF9D0 var_F0 = MASS * 9.81 / mSuspensionAttribs.mv...RestDisplacement...x
    //     0x825CF9DC/E4  v1 (arg1, STIFFNESS) <- that value
    //     0x825CF9E8 var_F0 = mSuspensionAttribs.mv...Dampening...y
    //     0x825CF9EC/F0  v2 (arg2, DAMPING)   <- that value
    //     0x825CF9F4 bl SuspensionSpring::Prepare(stiffness, damping, mass)   [that parameter order
    //                is asm-literal and is already recorded in SuspensionSpring.cpp:79-83]
    // `stiffness = mass * g / restDisplacement` is the textbook "the spring settles by exactly
    // its rest displacement under its own share of the weight" -- which is why the constant at
    // flt_8208F83C is gravity and not a tuning number.
    //
    // THE `a2 double` IN THE HEX-RAYS PROTOTYPE IS NOT A PARAMETER OF THIS FUNCTION. Nothing in
    // the 190 instructions reads f1/f2; the old banner's "plus the rest displacement and dt (the a2
    // double)" described an argument the body never touches. The committed declaration is already
    // `void SetupSuspension()` and stays that way.
    // Faithful, NOT bit-exact: the console's `vrefp` + two Newton-Raphson refinements and its
    // `fdivs` are both spelled as C division here (more accurate, not identical in the last ulp) --
    // the same standing allowance the traction-line drain records for its `1/sqrt`.
    //
    // WHAT THIS DOES **NOT** DO, measured, so nobody reads more into it than is there: it makes
    // `maSprings[i].mvStiffness_Damping_Mass_Position` real, but `ApplySuspensionForces`
    // (:3096 below) multiplies the MASS lane by the ACCELERATION lane, and the acceleration lane's
    // only writer is `UpdateSuspensionSprings` @0x825F7AF0, still an empty [blocked] body. So the
    // push magnitude stays 0 and this changes no behaviour today. It is the SEED, not the solve.
    // =============================================================================================
    void VehiclePhysics::SetupSuspension()
    {
        // flt_8208F83C, read out of the X360 image this wave.
        static const f32 KF_SETUP_SUSPENSION_GRAVITY = 9.8100004196167f;
        // flt_82001C98, ditto (the `1 -` in each ratio).
        static const f32 KF_ONE = 1.0f;

        // dword_8208FAFC / dword_8208FAEC, read out of the X360 image this wave.
        static const s32 KAI_OTHER_AXLE_SAME_SIDE[eNumDrivenWheels] = { 2, 3, 0, 1 };
        static const s32 KAI_SAME_AXLE_OTHER_SIDE[eNumDrivenWheels] = { 1, 0, 3, 2 };

        // ---- the prologue's four staged vectors: each wheel's streamed rest position, x in lane
        //      0 and z in lane 1 (the two `vrlimi128` passes at 0x825CF76C..0x825CF80C).
        f32 lafWheelX[eNumDrivenWheels];
        f32 lafWheelZ[eNumDrivenWheels];
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            lafWheelX[liWheel] = maWheels[liWheel].mStreamedPositionPlusTwistAmount.x;
            lafWheelZ[liWheel] = maWheels[liWheel].mStreamedPositionPlusTwistAmount.z;
        }

        // ---- LOOP 1: the per-wheel mass split into mvSpringMassScalers (this+0xED0).
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const s32 liAxlePartner = KAI_OTHER_AXLE_SAME_SIDE[liWheel];
            const s32 liSidePartner = KAI_SAME_AXLE_OTHER_SIDE[liWheel];

            const f32 lfDeltaZ = lafWheelZ[liAxlePartner] - lafWheelZ[liWheel];
            const f32 lfDeltaX = lafWheelX[liSidePartner] - lafWheelX[liWheel];

            // `vandc <value>, <0x80000000 splat>` is fabsf; the reciprocal is vrefp + 2 NR.
            const f32 lfRatioZ = std::fabs(lafWheelZ[liWheel] / lfDeltaZ);
            const f32 lfRatioX = std::fabs(lafWheelX[liWheel] / lfDeltaX);

            const f32 lfScaler = (KF_ONE - lfRatioX) * (KF_ONE - lfRatioZ);

            // the `vperm` lane insert at 0x825CF944 + the `stvx128 v0, r0, r28` at 0x825CF948.
            switch (liWheel)
            {
            case 0: mvSpringMassScalers.x = lfScaler; break;
            case 1: mvSpringMassScalers.y = lfScaler; break;
            case 2: mvSpringMassScalers.z = lfScaler; break;
            default: mvSpringMassScalers.w = lfScaler; break;
            }
        }

        // ---- LOOP 2: seed the four springs.
        const f32 lfBodyMass =
            mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;   // attribs+0x70 .x
        const f32 lfRestDisplacement =
            mpAttribs->mSuspensionAttribs
                     .mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.x;    // attribs+0x230 .x
        const f32 lfDampening =
            mpAttribs->mSuspensionAttribs
                     .mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.y;    // attribs+0x230 .y

        for (s32 liSpring = 0; liSpring < eNumDrivenWheels; ++liSpring)
        {
            // `vperm v0, <mvSpringMassScalers>, <same>, vspltw(lvsl(0, i*4), 0)` == lane i, splatted.
            const f32 lfScaler = (liSpring == 0) ? mvSpringMassScalers.x
                               : (liSpring == 1) ? mvSpringMassScalers.y
                               : (liSpring == 2) ? mvSpringMassScalers.z
                                                 : mvSpringMassScalers.w;

            const f32 lfSpringMass = lfScaler * lfBodyMass;
            const f32 lfStiffness  = (lfSpringMass * KF_SETUP_SUSPENSION_GRAVITY) / lfRestDisplacement;

            maSprings[liSpring].Prepare(VecFloat{ lfStiffness, lfStiffness, lfStiffness, lfStiffness },
                                        VecFloat{ lfDampening, lfDampening, lfDampening, lfDampening },
                                        VecFloat{ lfSpringMass, lfSpringMass, lfSpringMass, lfSpringMass });
        }
    }

// [clean] UpdateSuspensionSprings  @0x825F7AF0
    // @0x825F7AF0  BrnPhysics::Vehicle::VehiclePhysics::UpdateSuspensionSprings   (2,231 insns)
    // =============================================================================================
    // THE "BLOCKED" VERDICT IS RETRACTED,
    // and every one of its four reasons is now answered rather than argued around:
    //   1. "Hex-Rays: local variable allocation has failed"  -> nothing here is read from the
    //      pseudocode. The whole body is decoded from the ASM.
    //   2. "the ~25 setter inputs are dense VMX through un-pinned wheel lanes + un-homed rodata"
    //      -> every input is named twice over, see the register table below.
    //   3. "several setters (SetDampingForce/SetSpringForce/SetAcceleration) are not even in the
    //      committed SuspensionSpring slice" -> all nine are now bodied in SuspensionSpring.cpp
    //      (SetDampingForce landed with this wave; its export is an IDA hole and the lane came out
    //      of the image bytes).
    //   4. "~800 lines of CgsDev::Assert plumbing" -> true, and it is the KEY, not the noise:
    //      **the assert message strings name every hoisted register.**
    //
    // HOW THE ~20 HOISTED PROLOGUE REGISTERS WERE CLOSED WITHOUT TOUCHING THE IDA VMX+32 SKEW.
    // [[ida-vmx-plus32-and-rdata-unlock]] says IDA prints VMX128 SOURCE registers 32 too high per
    // operand field, which is why the previous wave refused to trust v110..v127. It never needed
    // to. Each hoisted value is `stvx128`d to a stack slot which a prologue assert re-loads with
    // `lfs` and prints -- so the GAME NAMES IT. Every one then matches a committed DWARF member:
    //
    //   reg         built from                     the game's own assert string    committed member
    //   v112     lvx128 unk_82FB9160          (feeds stiffness)                 KF_GRAVITY 9.81
    //   v114     [mpAttribs+0x70].x           "Invalid mass: "                  mBaseAttribs.mvMass_....x
    //   v117     [mpAttribs+0x230].x          "Invalid rest displacement: "     mSuspensionAttribs.mvRestDisplacement_....x
    //   v111     vrefp+2NR(v117)              "Invalid recip. rest displ.: "    (1/restDisplacement)
    //   v113     [mpAttribs+0x230].y          "Invalid spring damping: "        ...mvRestDisplacement_**Dampening**_....y
    //   v110     [mpAttribs+0x240].z          "Invalid in air damping: "        ...mv..._**InAirDamping**_....z
    //   v116     [this+0xED0]                 "Invalid spring mass scalar: "    mvSpringMassScalers
    //   v118     the incoming v1              --                                THE TIMESTEP ARGUMENT
    //
    // The "duplicate splats" that made the prologue look ambiguous (v117/v126, v114/v124,
    // v113/v121, v110/v122) are simply those assert copies, `stvx128`d to var_3D0/3E0/3F0/400/410.
    // v112 == unk_82FB9160 reads ALL ZEROS in the image; it is the static-init splat of
    // flt_8208F83C == 9.81000042, exactly as BrnVehicleConstants.h:35-42 already records -- and
    // that header ALREADY said "UpdateSuspensionSprings computes k = massOnSpring * g /
    // restDisplacement", which is precisely what 0x825F81CC..0x825F81EC does. No new constant.
    //
    // ARITY: `vmr128 v118,v1` at 0x825F7B04 reads an INCOMING register, and the caller
    // UpdateSuspension @0x8261F698 parks dt (`vmr128 v127,v1`) then re-issues `vmr128 v1,v127`
    // before each phase call including 0x825F6D8. So the parameter is real and the committed
    // no-argument declaration was a slice artifact -- [[the-work-is-in-a-prepare-stage]]: recover
    // the signature from the asm, not from the PC declaration.
    //
    // ---------------------------------------------------------------------------------------------
    // PASS 1 (0x825F7FF4..0x825F803C) -- WHERE DOES THE AIRBORNE WHEELS' WEIGHT GO?
    //   for each wheel, taking mvSpringMassScalers lane-by-lane (v116 rotated one lane per step):
    //     state==2 OR onGround -> groundedScalerSum += scaler        (vaddfp128 v125)
    //     otherwise            -> airborneMassSum   += mass*scaler   (vmaddfp128 v115)
    //   then recip = 1/groundedScalerSum (0x825F8044 vrefp + two Newton refinements).
    // SELF-CHECK DONE BEFORE WRITING, AND IT IS EXACT: the grounded arm below assigns
    //   m_i = scaler_i * (M + airborneMassSum/groundedScalerSum), so summing over the grounded
    //   wheels gives M*S_g + M*(sum of airborne scalers) = M * (sum of ALL scalers) = M * 1.0.
    //   **Total mass is conserved to the last term** -- which is exactly what a suspension must do
    //   when a wheel leaves the road: the wheels still down carry the whole car.
    //
    // PASS 2 (0x825F81AC..0x825F9BE8) -- four wheels, `mulli r11,r10,0xE0` off the committed 0x130
    // seat, spring at `(0x4B+i)*0x30` == the committed +0xE10.
    //   0x825F81B4  maWheels[i].mu8State == 2 -> skip the wheel ENTIRELY (and, note, WITHOUT
    //               rotating the scaler register -- the rotate at 0x825F9BC8 is jumped over; that
    //               is reproduced literally below rather than tidied away).
    //   0x825F81EC  SetStiffness( mass * scaler * GRAVITY / restDisplacement )
    //   0x825F81F0  **`lbz r11,0x158(r30)` == mRoadContact.mbIsOnGround** -- the fork:
    //     GROUNDED (0x825F8200..0x825F89EC):
    //        SetMass    ( scaler*(M + airborneMassSum/groundedScalerSum) )
    //        SetDamping ( spring damping )
    //        SetVelocity( dot3( mLinearVelocity + omega x (R * mPosition), mRoadContact.mNormal ) )
    //                     -- the classic two-vpermwi 0x63 cross product at 0x825F83D0..0x825F83E0;
    //                        the transform here deliberately OMITS the translation (a lever arm,
    //                        not a point), which is how it differs from ApplyWheelWeight's.
    //        SetPosition( -((mPosition.y - mStreamedPositionPlusTwistAmount.y) + restDisplacement) )
    //     AIRBORNE (0x825F89F0..0x825F8A08):
    //        SetMass    ( maWheels[i].mIntegrationVariables.w )   -- the unsprung wheel mass
    //        SetDamping ( in-air damping )
    //        and NOTHING else: velocity and position carry over, owned by pass 3.
    //   COMMON INTEGRATE (0x825F8F98..0x825F9084), re-loading the spring after every setter:
    //        SetDampingForce ( -velocity * damping * mass )
    //        SetSpringForce  ( -stiffness * position )                        <- HOOKE
    //        SetAcceleration ( (dampingForce+springForce+externalForce)/mass) <- NEWTON
    //        SetVelocity     ( velocity + acceleration*dt )                   <- semi-implicit Euler
    //        SetPosition     ( position + velocity*dt )
    //        SetExternalForce( 0 )      -- the weight-transfer force is CONSUMED, then cleared.
    //
    // PASS 3 (0x825F9C0C..0x825F9DB4) -- AIRBORNE WHEELS ONLY. Drive the visible wheel from the
    // spring and clamp it to the travel stops, then round-trip the clamp back into the spring:
    //        y = clamp( streamedY - restDisplacement + springPosition,
    //                   mSuspensionAndInertiaVariables.x, mSuspensionAndInertiaVariables.y )
    //        mPosition.y = y ;  SetPosition( y + restDisplacement - streamedY )
    // The two clamp bounds are Wheel::Prepare's already-committed lane map (`f4 -> .x =
    // streamedY - travelDown`, `f3 -> .y = streamedY + travelUp`) -- an independent confirmation
    // that arrived from a different function in a different wave.
    //
    // ON THE TWO OPPOSITE POSITION SIGNS, because they look like a bug and are not.
    // Grounded stores -(compression), pass 3 stores +(compression). BOTH have their zero at the
    // SAME physical place (wheel fully extended, y == streamedY - restDisplacement), and each sign
    // makes `springForce = -k*position` push the right way for its own arm: a grounded spring
    // pushes the BODY up, an airborne spring pushes the WHEEL down toward full extension. Pass 3
    // is also exactly the algebraic inverse of its own forward map, so with no clamping it is a
    // no-op -- which is what proves the reading rather than merely permitting it.
    //
    // ELIDED, and named so nobody thinks the body is short: the six prologue asserts and the 24
    // in-loop CgsDev::Assert blocks (BeginAssert/AppendFormat x N/FireAssert/EndAssert) -- ~840 of
    // the 2,231 instructions. They are debug-build finite-value guards ("..., please tell Graham
    // D."), the project convention elides them, and they write nothing the game reads.
    // =============================================================================================
    void VehiclePhysics::UpdateSuspensionSprings(VecFloat lvfTimeStep)
    {
        // ---- the hoisted per-vehicle constants (prologue 0x825F7B08..0x825F7BF8) ----
        const VehicleAttribs::SuspensionAttribs& lrSus = mpAttribs->mSuspensionAttribs;

        const f32 lfBodyMass =
            mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;          // v114
        const f32 lfRestDisplacement =
            lrSus.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.x;               // v117
        const f32 lfSpringDamping =
            lrSus.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.y;               // v113
        const f32 lfInAirDamping =
            lrSus.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.z; // v110
        const f32 lfRecipRestDisplacement = 1.0f / lfRestDisplacement;                          // v111
        const f32 lfTimeStep = lvfTimeStep.x;                                                   // v118

        // (the six prologue "Invalid <thing>, please tell Graham D" asserts are elided -- debug.)

        // v116: mvSpringMassScalers, rotated one lane left per SERVICED wheel. Kept as an explicit
        // rotating register because the console's rotate is INSIDE the not-skipped path (see below).
        Vector4 lvRotatingScalers = mvSpringMassScalers;

        // ---- PASS 1: redistribute the airborne wheels' share of the body mass ----
        f32 lfGroundedScalerSum = 0.0f;   // v125
        f32 lfAirborneMassSum   = 0.0f;   // v115
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const Wheel& lrWheel = maWheels[liWheel];
            const f32 lfScaler = lvRotatingScalers.x;

            if (lrWheel.mu8State == 2 || lrWheel.GetRoadContact().mbIsOnGround)
                lfGroundedScalerSum += lfScaler;
            else
                lfAirborneMassSum += lfBodyMass * lfScaler;

            // 0x825F8024/0x825F8034: unconditional one-lane rotate in this pass.
            lvRotatingScalers = Vector4{ lvRotatingScalers.y, lvRotatingScalers.z,
                                         lvRotatingScalers.w, lvRotatingScalers.x };
        }
        const f32 lfRecipGroundedScalerSum = 1.0f / lfGroundedScalerSum;   // v123 (0x825F8044)

        // ---- PASS 2: per wheel -- seed the spring, then integrate it one step ----
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];

            // 0x825F81B4: a detached wheel is skipped whole, and the console jumps PAST the scaler
            // rotate to do it (loc_825F9BDC is after 0x825F9BD8). Reproduced, not tidied.
            if (lrWheel.mu8State == 2)
                continue;

            SuspensionSpring& lrSpring = maSprings[liWheel];
            const f32 lfScaler     = lvRotatingScalers.x;            // v127
            const f32 lfSprungMass = lfBodyMass * lfScaler;          // v126

            // 0x825F81E4/E8: k = m*g/restDisplacement -- the same law SetupSuspension seeds with.
            lrSpring.SetStiffness((lfSprungMass * KF_GRAVITY) * lfRecipRestDisplacement);

            if (lrWheel.GetRoadContact().mbIsOnGround)
            {
                // -- GROUNDED --------------------------------------------------------------------
                // 0x825F8200/0x825F8204: carry this wheel's own share PLUS its slice of whatever
                // the airborne wheels are no longer carrying.
                lrSpring.SetMass(lfRecipGroundedScalerSum * (lfAirborneMassSum * lfScaler) + lfSprungMass);
                lrSpring.SetDamping(lfSpringDamping);

                // (debug-only "Invalid wheel position" assert on mPosition -- elided.)

                // 0x825F8340..0x825F83E4: the velocity of the suspension point, resolved along the
                // road normal. The lever arm is R * mPosition with NO translation.
                const Vector3& lvLocal = lrWheel.mPosition;
                const Vector3& lvRight = mTransform.Right();
                const Vector3& lvUp    = mTransform.Up();
                const Vector3& lvAt    = mTransform.At();
                const f32 lfArmX = lvRight.x * lvLocal.x + lvUp.x * lvLocal.y + lvAt.x * lvLocal.z;
                const f32 lfArmY = lvRight.y * lvLocal.x + lvUp.y * lvLocal.y + lvAt.y * lvLocal.z;
                const f32 lfArmZ = lvRight.z * lvLocal.x + lvUp.z * lvLocal.y + lvAt.z * lvLocal.z;

                const Vector3& lvOmega = mAngularVelocity;
                const f32 lfVx = mLinearVelocity.x + (lvOmega.y * lfArmZ - lvOmega.z * lfArmY);
                const f32 lfVy = mLinearVelocity.y + (lvOmega.z * lfArmX - lvOmega.x * lfArmZ);
                const f32 lfVz = mLinearVelocity.z + (lvOmega.x * lfArmY - lvOmega.y * lfArmX);

                const Vector3& lvNormal = lrWheel.GetRoadContact().mNormal;
                lrSpring.SetVelocity(lfVx * lvNormal.x + lfVy * lvNormal.y + lfVz * lvNormal.z);

                // (debug-only "Invalid suspension velocity" assert -- elided.)

                // 0x825F8730..0x825F875C: the suspension length, negated. Zero when the wheel sits
                // at full extension (streamedY - restDisplacement); negative once compressed, so
                // -k*position pushes the body UP.
                const f32 lfStreamedY = lrWheel.mStreamedPositionPlusTwistAmount.y;
                lrSpring.SetPosition(-((lrWheel.mPosition.y - lfStreamedY) + lfRestDisplacement));

                // (debug-only "Invalid suspension length" assert -- elided.)
            }
            else
            {
                // -- AIRBORNE (0x825F89F0) -- the unsprung wheel falls on its own mass ------------
                lrSpring.SetMass(lrWheel.mIntegrationVariables.w);
                lrSpring.SetDamping(lfInAirDamping);
                // velocity + position are NOT touched here; pass 3 owns the airborne position.
            }

            // -- COMMON INTEGRATE (0x825F8F98..0x825F9084) ---------------------------------------
            // The console re-loads the spring registers after each setter; these reads do the same.
            // (the "Invalid {velocity,mass,position} before integrate" asserts are elided.)
            {
                const f32 lfVelocity  = lrSpring.mvVelocity_Acceleration_DampingForce_SpringForce.x;
                const f32 lfDamping   = lrSpring.mvStiffness_Damping_Mass_Position.y;
                const f32 lfMass      = lrSpring.mvStiffness_Damping_Mass_Position.z;
                lrSpring.SetDampingForce(-lfVelocity * lfDamping * lfMass);            // 0x825F8FC4
            }
            {
                const f32 lfStiffness = lrSpring.mvStiffness_Damping_Mass_Position.x;
                const f32 lfPosition  = lrSpring.mvStiffness_Damping_Mass_Position.w;
                lrSpring.SetSpringForce(-lfStiffness * lfPosition);                    // 0x825F8FE4
            }
            {
                const f32 lfDampingForce = lrSpring.mvVelocity_Acceleration_DampingForce_SpringForce.z;
                const f32 lfSpringForce  = lrSpring.mvVelocity_Acceleration_DampingForce_SpringForce.w;
                const f32 lfExternal     = lrSpring.mvExternalForce.x;
                const f32 lfMass         = lrSpring.mvStiffness_Damping_Mass_Position.z;
                lrSpring.SetAcceleration((lfDampingForce + lfSpringForce + lfExternal) / lfMass);  // 0x825F9044
            }
            {
                const f32 lfVelocity     = lrSpring.mvVelocity_Acceleration_DampingForce_SpringForce.x;
                const f32 lfAcceleration = lrSpring.mvVelocity_Acceleration_DampingForce_SpringForce.y;
                lrSpring.SetVelocity(lfAcceleration * lfTimeStep + lfVelocity);        // 0x825F905C
            }
            {
                const f32 lfVelocity = lrSpring.mvVelocity_Acceleration_DampingForce_SpringForce.x;
                const f32 lfPosition = lrSpring.mvStiffness_Damping_Mass_Position.w;
                lrSpring.SetPosition(lfVelocity * lfTimeStep + lfPosition);            // 0x825F9078
            }
            lrSpring.SetExternalForce(0.0f);                                           // 0x825F9084
            // (the "Invalid {velocity,mass,position} after integrate" + acceleration/force asserts
            //  are elided -- eight more blocks, all debug.)

            // 0x825F9BC8..0x825F9BD8: advance the scaler register -- reached ONLY by a serviced wheel.
            lvRotatingScalers = Vector4{ lvRotatingScalers.y, lvRotatingScalers.z,
                                         lvRotatingScalers.w, lvRotatingScalers.x };
        }

        // ---- PASS 3 (0x825F9C0C): airborne wheels -- extend the visible wheel, clamp, round-trip ----
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];
            if (lrWheel.mu8State == 2)                          // 0x825F9C18
                continue;
            if (lrWheel.GetRoadContact().mbIsOnGround)          // 0x825F9C24
                continue;

            SuspensionSpring& lrSpring = maSprings[liWheel];

            const f32 lfStreamedY   = lrWheel.mStreamedPositionPlusTwistAmount.y;
            const f32 lfTravelDown  = lrWheel.mSuspensionAndInertiaVariables.x;   // streamedY - travelDown
            const f32 lfTravelUp    = lrWheel.mSuspensionAndInertiaVariables.y;   // streamedY + travelUp
            const f32 lfSpringPos   = lrSpring.mvStiffness_Damping_Mass_Position.w;

            f32 lfWheelY = (lfStreamedY - lfRestDisplacement) + lfSpringPos;      // 0x825F9C54/58
            lfWheelY = (lfWheelY > lfTravelDown) ? lfWheelY : lfTravelDown;       // 0x825F9C5C vmaxfp
            lfWheelY = (lfWheelY < lfTravelUp)   ? lfWheelY : lfTravelUp;         // 0x825F9C60 vminfp

            lrWheel.mPosition.y = lfWheelY;                                       // 0x825F9C6C/74

            // (debug-only "RwMathVPU::VecFloat( lvfClampedWheelPos... )" assert -- elided.)

            // 0x825F9D94/9D9C: the exact inverse of the map above, so an unclamped wheel round-trips
            // to the identical value and only a wheel that hit a stop has its spring corrected.
            lrSpring.SetPosition((lfWheelY + lfRestDisplacement) - lfStreamedY);
        }
    }

    // VehiclePhysics::UpdatePostSimulation(VecFloat)
    // DecFIGS emits this at PS3 0x6EDFA4. Breaker inlines it twice: race cars at
    // 0x826428A8..0x826428D8 and full-physics traffic at 0x82637388..0x826373B4.
    // Both copies call virtual slot +0x10 (IsPlayerVehicleInShowtime) and forward the
    // untouched splatted timestep to UpdateSuspensionPostSimulation only when it is false.
    void VehiclePhysics::UpdatePostSimulation(VecFloat lvfTimeStep)
    {
        if (!IsPlayerVehicleInShowtime())
            UpdateSuspensionPostSimulation(lvfTimeStep);
    }

    // @0x825F6BB0  BrnPhysics::Vehicle::VehiclePhysics::UpdateSuspensionPostSimulation
    // Assembly-first reconstruction. Hex-Rays failed allocation, but the PPC body is regular once
    // its four wheel loops are followed by address: RoadContact is copied from wheel+0x00, mPosition
    // sits at +0x80, streamed position at +0x90, and each wheel advances by 0xE0. DecFIGS supplies
    // the source-local names (`labCompressedSpring`, `lWheelContactPoint`, `lTranslation`, and the
    // impulse locals). Constants are image/static-init pinned: -0.7 restitution @0x8208FB10,
    // 0.05 bottom tolerance @0x820047C8, and KF_GRAVITY via unk_82FB9160.
    void VehiclePhysics::UpdateSuspensionPostSimulation(VecFloat lvfTimeStep)
    {
        if (mbFrozen)   // @0x825F6BE8, VehiclePhysics+0x70
            return;

        // [susv] entry snapshot -- see the helper banner. Player car only; the ledger prints at the
        // bottom, gated on the WHOLE-FUNCTION |dv|.
        const bool lbSusV = SusVProbeArmed()
                            && mPreviousControls.GetType() == E_DRIVER_TYPE_PLAYER;
        const Vector3 lSusV0 = mLinearVelocity;
        gbSusVStabDamped = false;

        bool labCompressedSpring[eNumDrivenWheels] = { false, false, false, false };

        // @0x825F6C74..7130: project each wheel's suspension line onto its road plane and
        // repair a wheel which is still above that plane along the vehicle's up axis.
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];
            const Wheel::RoadContact& lrRoadContact = lrWheel.GetRoadContact();

            if (!lrRoadContact.mbIsOnGround || lrWheel.mu8State == 2)
                continue;

            CGS_ASSERT(vpu::IsValid(lrWheel.mPosition),
                       "Invalid wheel position, please tell Graham D.");

            const Vector3& lrPlaneNormal = lrRoadContact.mNormal;
            const f32 lfPlaneDistance = vpu::Dot(lrPlaneNormal, lrRoadContact.mPosition);
            const Vector3 lLineStart = vpu::TransformPoint(mTransform, lrWheel.mPosition);
            const Vector3 lLineDirection = vpu::Negate(lrPlaneNormal);
            const f32 lfLinePlaneDistance =
                (lfPlaneDistance - vpu::Dot(lrPlaneNormal, lLineStart)) /
                vpu::Dot(lrPlaneNormal, lLineDirection);
            const Vector3 lWheelContactPoint =
                vpu::Add(lLineStart, vpu::Mult(lLineDirection, lfLinePlaneDistance));

            CGS_ASSERT(vpu::IsValid(lrWheel.mPosition),
                       "Invalid wheel position, please tell Graham D.");

            const f32 lfRadius = lrWheel.mSlipVariables.w;
            const f32 lfMinSuspensionHeight = lrWheel.mSuspensionAndInertiaVariables.x;
            const Vector3 lRoadContactPoint = vpu::Subtract(
                vpu::TransformPoint(mTransform, lrWheel.mPosition),
                vpu::Mult(lrPlaneNormal, lfRadius));

            const bool lbWheelWithinSuspensionReach =
                (lrWheel.mPosition.y - lfMinSuspensionHeight + lfRadius) >= 0.0f;
            const bool lbRoadFacesVehicleUp =
                vpu::Dot(mTransform.yAxis, lrPlaneNormal) > 0.5f;

            if (lbWheelWithinSuspensionReach && lbRoadFacesVehicleUp)
            {
                const f32 lfDistanceToRoad =
                    vpu::Dot(mTransform.yAxis,
                             vpu::Subtract(lWheelContactPoint, lRoadContactPoint));
                if (lfDistanceToRoad > 0.0f)
                {
                    // ---- [wsus-rep] the above-plane repair, when it fires ---------------------
                    if (WheelSusProbeArmed() && mPreviousControls.GetType() == E_DRIVER_TYPE_PLAYER
                        && WheelSusTakeLine())
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[wsus-rep] f " << static_cast<s32>(WheelSusFrameFor(this))
                            << " car " << static_cast<u32>(reinterpret_cast<u64>(this))
                            << " w " << liWheel
                            << " d " << lfDistanceToRoad
                            << " wyBefore " << lrWheel.mPosition.y
                            << " n " << lrPlaneNormal.x << " " << lrPlaneNormal.y << " " << lrPlaneNormal.z
                            << "\n";
                    }
                    // ---- end [wsus-rep] -------------------------------------------------------
                    lrWheel.mPosition.y = std::max(
                        lrWheel.mPosition.y + lfDistanceToRoad,
                        lfMinSuspensionHeight);
                }
            }
        }

        // @0x825F7134..729C: identify springs compressed beyond the configured upward travel,
        // remembering the deepest penetration and that wheel's road normal.
        f32 lfMaximumSuspensionPenetration = 0.0f;
        Vector3 lMaximumPenetrationNormal{ 0.0f, 0.0f, 0.0f, 0.0f };
        const f32 lfUpwardMovement =
            mpAttribs->mSuspensionAttribs
                .mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.z;

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];
            const Wheel::RoadContact& lrRoadContact = lrWheel.GetRoadContact();
            if (!lrRoadContact.mbIsOnGround || lrWheel.mu8State == 2)
                continue;

            const f32 lfDisplacement =
                lrWheel.mPosition.y - lrWheel.mStreamedPositionPlusTwistAmount.y;
            if (lfDisplacement >= lfUpwardMovement)
            {
                labCompressedSpring[liWheel] = true;
                const f32 lfSuspensionPenetration = lfDisplacement - lfUpwardMovement;
                if (lfSuspensionPenetration > lfMaximumSuspensionPenetration)
                {
                    lfMaximumSuspensionPenetration = lfSuspensionPenetration;
                    lMaximumPenetrationNormal = lrRoadContact.mNormal;
                }
            }
        }

        // ---- [wsus-post] the post-simulation half of the witness (player car only) -----------
        const bool lbWheelSusProbe =
            WheelSusProbeArmed() && mPreviousControls.GetType() == E_DRIVER_TYPE_PLAYER;
        const u32 luWheelSusF   = lbWheelSusProbe ? WheelSusFrameFor(this) : 0u;
        const u32 luWheelSusCar = static_cast<u32>(reinterpret_cast<u64>(this));
        if (lbWheelSusProbe && WheelSusTakeLine())
        {
            *CgsDev::Log::gpDebugPrint
                << "[wsus-post] f " << static_cast<s32>(luWheelSusF) << " car " << luWheelSusCar
                << " comp " << (labCompressedSpring[0] ? 1 : 0) << (labCompressedSpring[1] ? 1 : 0)
                << (labCompressedSpring[2] ? 1 : 0) << (labCompressedSpring[3] ? 1 : 0)
                << " upMov " << lfUpwardMovement
                << " disp " << (maWheels[0].mPosition.y - maWheels[0].mStreamedPositionPlusTwistAmount.y)
                << " " << (maWheels[1].mPosition.y - maWheels[1].mStreamedPositionPlusTwistAmount.y)
                << " " << (maWheels[2].mPosition.y - maWheels[2].mStreamedPositionPlusTwistAmount.y)
                << " " << (maWheels[3].mPosition.y - maWheels[3].mStreamedPositionPlusTwistAmount.y)
                << " maxPen " << lfMaximumSuspensionPenetration
                << " penN " << lMaximumPenetrationNormal.x << " " << lMaximumPenetrationNormal.y
                << " " << lMaximumPenetrationNormal.z
                << " nwc " << static_cast<s32>(mi8NumWorldCollisions)
                << " vel " << mLinearVelocity.x << " " << mLinearVelocity.y << " " << mLinearVelocity.z
                << "\n";
        }
        // ---- end [wsus-post] ------------------------------------------------------------------

        // @0x825F72A0..7398: move the body out along the selected contact normal, then pull every
        // attached grounded wheel back by the same amount in suspension-y space.
        if (lfMaximumSuspensionPenetration > 0.0f)
        {
            const Vector3 lTranslation =
                vpu::Mult(mTransform.yAxis, lfMaximumSuspensionPenetration);
            const f32 lfTranslationInVehicleYAxis =
                vpu::Dot(lTranslation, lMaximumPenetrationNormal);
            const Vector3 lTranslationNormal =
                vpu::Mult(lMaximumPenetrationNormal, lfTranslationInVehicleYAxis);
            mTransform.wAxis = vpu::Add(mTransform.wAxis, lTranslationNormal);

            for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            {
                Wheel& lrWheel = maWheels[liWheel];
                if (!lrWheel.GetRoadContact().mbIsOnGround || lrWheel.mu8State == 2)
                    continue;

                lrWheel.mPosition.y = std::max(
                    lrWheel.mPosition.y - lfTranslationInVehicleYAxis,
                    lrWheel.mSuspensionAndInertiaVariables.x);
            }
        }

        // @0x825F739C..7498: latch a hard landing after more than half a second without
        // traction when either a wheel, or a sufficiently close world contact, reaches ground.
        if (mbHasAir &&
            mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z > 0.5f &&
            !IsCrashing())
        {
            const bool lbAtLeastOneWheelOnGround =
                maWheels[eRearLeftWheel].GetRoadContact().mbIsOnGround ||
                maWheels[eRearRightWheel].GetRoadContact().mbIsOnGround ||
                maWheels[eFrontLeftWheel].GetRoadContact().mbIsOnGround ||
                maWheels[eFrontRightWheel].GetRoadContact().mbIsOnGround;

            bool lbHardLanding = lbAtLeastOneWheelOnGround;
            if (!lbHardLanding && mi8NumWorldCollisions > 0 && mAboveGroundTestResult.mbValid)
            {
                lbHardLanding =
                    mAboveGroundTestResult.mfVerticalDistance < GetCarGroundDistanceCheck();
            }

            if (lbHardLanding)
            {
                mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.x = 0.0f;
            }
        }

        const Vector3 lSusV1 = mLinearVelocity;          // [susv] before StabiliseAfterHardLanding
        StabiliseAfterHardLanding(lvfTimeStep);
        const Vector3 lSusV2 = mLinearVelocity;          // [susv] after it
        CalculateNewVelocity(lvfTimeStep);
        const Vector3 lSusV3 = mLinearVelocity;          // [susv] after the following integrate

        // @0x825F74B8..7654: with no world collision response, compressed traction springs get
        // an inanimate-world recovery impulse at their streamed suspension point.
        if (mi8NumWorldCollisions == 0)
        {
            const VecFloat lvfRestitution{ -0.7f, -0.7f, -0.7f, -0.7f };

            for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            {
                Wheel& lrWheel = maWheels[liWheel];
                if (!lrWheel.mbHasTraction || lrWheel.mu8State == 2 ||
                    !labCompressedSpring[liWheel])
                {
                    continue;
                }

                const Vector3 lSusPointLocal =
                    lrWheel.mStreamedPositionPlusTwistAmount.GetVector3();
                const Vector3 lSusPointWorld = vpu::TransformPoint(mTransform, lSusPointLocal);
                const Vector3 lPointVelocity =
                    GetLocalVelocity(lSusPointWorld, rw::physics::WORLD_SPACE);
                const Vector3& lrCollisionNormal = lrWheel.GetRoadContact().mNormal;

                if (vpu::Dot(lPointVelocity, lrCollisionNormal) < 0.0f)
                {
                    Vector3 lImpulse{ 0.0f, 0.0f, 0.0f, 0.0f };
                    VecFloat lvfInvInertia{ 0.0f, 0.0f, 0.0f, 0.0f };
                    CalculateCollisionImpulseWithInanimateObject(
                        lSusPointWorld, lPointVelocity, lrCollisionNormal, lvfRestitution,
                        &lImpulse, &lvfInvInertia);
                    // ---- [wsus-imp] one line per recovery impulse, both sides -----------------
                    if (lbWheelSusProbe && WheelSusTakeLine())
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[wsus-imp] f " << static_cast<s32>(luWheelSusF) << " car " << luWheelSusCar
                            << " w " << liWheel
                            << " vDotN " << vpu::Dot(lPointVelocity, lrCollisionNormal)
                            << " n " << lrCollisionNormal.x << " " << lrCollisionNormal.y
                            << " " << lrCollisionNormal.z
                            << " pv " << lPointVelocity.x << " " << lPointVelocity.y << " " << lPointVelocity.z
                            << " J " << lImpulse.x << " " << lImpulse.y << " " << lImpulse.z
                            << " velBefore " << mLinearVelocity.x << " " << mLinearVelocity.y
                            << " " << mLinearVelocity.z
                            << "\n";
                    }
                    // ---- end [wsus-imp] -------------------------------------------------------
                    AddLocalImpulse(lImpulse, rw::physics::WORLD_SPACE,
                                    lSusPointLocal, rw::physics::BODY_SPACE);
                    CalculateNewVelocity(lvfTimeStep);
                }
            }
        }

        const Vector3 lSusV4 = mLinearVelocity;          // [susv] after the recovery-impulse arm

        // @0x825F7658..7858: an unattached wheel resting on its lower stop contributes its
        // own weight at the wheel's body-space position.
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Wheel& lrWheel = maWheels[liWheel];
            const f32 lfDistanceFromBottom =
                lrWheel.mPosition.y - lrWheel.mSuspensionAndInertiaVariables.x;

            if (lrWheel.mbHasTraction || lfDistanceFromBottom > 0.05f || lrWheel.mu8State == 2)
                continue;

            const Vector3 lWheelForce{
                0.0f, -lrWheel.mIntegrationVariables.w * KF_GRAVITY, 0.0f, 0.0f };
            CGS_ASSERT(vpu::IsValid(lrWheel.mPosition),
                       "Invalid wheel position, please tell Graham D.");
            AddLocalForce(lWheelForce, rw::physics::WORLD_SPACE,
                          lrWheel.mPosition, rw::physics::BODY_SPACE);
        }

        CalculateNewVelocity(lvfTimeStep);

        // ---- [susv] THE LEDGER ------------------------------------------------------------------
        // One line whenever this function moved the player car's velocity by more than the armed
        // threshold, with every stage's contribution named. DELETE-WHEN-STABLE.
        // ⭐ WHAT THIS PROBE COVERS, stated so it cannot be over-read: the stages BETWEEN the entry
        //   and exit of UpdateSuspensionPostSimulation only.
        //     bump   the pre-Stabilise section (the above-plane wheel repair and the bump-stop body
        //            translation -- neither writes velocity, so a non-zero here is itself a finding)
        //     stabI  StabiliseAfterHardLanding's ENTRY CalculateNewVelocity (the ordinary drain)
        //     stabD  its DIRECT overwrite  v -= n * (dot(v,n) * pow(damp, dt*60))
        //     integ  the CalculateNewVelocity that follows it
        //     imp    the mi8NumWorldCollisions == 0 recovery-impulse arm (restitution -0.7)
        //     wforce the unattached-wheel weight loop plus the tail integrate
        //   `total` is v(exit) - v(entry) for this call. Anything a caller sees beyond `total` in
        //   the same step was written somewhere else and this line says nothing about it.
        if (lbSusV)
        {
            const Vector3 lSusV5 = mLinearVelocity;
            const f32 lfTotal = SusVMag(lSusV5, lSusV0);
            if (lfTotal >= SusVProbeThreshold())
            {
                *CgsDev::Log::gpDebugPrint
                    << "[susv] car " << static_cast<u32>(reinterpret_cast<u64>(this))
                    << " total " << lfTotal
                    << " v0 " << lSusV0.x << " " << lSusV0.y << " " << lSusV0.z
                    << " v5 " << lSusV5.x << " " << lSusV5.y << " " << lSusV5.z
                    << " | bump "   << SusVMag(lSusV1, lSusV0)
                    << " stabI "    << SusVMag(gvSusVStabAfterIntegrate, lSusV1)
                    << " stabD "    << SusVMag(lSusV2, gvSusVStabAfterIntegrate)
                    << " integ "    << SusVMag(lSusV3, lSusV2)
                    << " imp "      << SusVMag(lSusV4, lSusV3)
                    << " wforce "   << SusVMag(lSusV5, lSusV4)
                    << " | damped " << (gbSusVStabDamped ? 1 : 0)
                    << " dampF "    << gfSusVStabDampFactor
                    << " vDotN "    << gfSusVStabVDotN
                    << " nwc "      << static_cast<s32>(mi8NumWorldCollisions)
                    << " air "      << (mbHasAir ? 1 : 0)
                    << " crash "    << (IsCrashing() ? 1 : 0)
                    << " tSinceLand " << mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.x
                    << "\n";
            }
        }
        // ---- end [susv] -------------------------------------------------------------------------
    }


    // ============================ C09 crash/contact-impulse group (verifier-corrected) ============================
// [clean] ApplyCarContactImpulse
    // @0x825D4C10  BrnPhysics::Vehicle::VehiclePhysics::ApplyCarContactImpulse
    //   ++miNumCollisions ; GetImpulsesFromLocalImpulse(localImpulse, contactPos) -> (J, rxJ).
    //   if ( !mbIsCrashing (this+0x710) ): remove linear Up completely, then linear At by
    //     GetCarCarResponse(); remove angular At by kvfCarImpulseRollDamping, angular Up by
    //     kvfCarImpulseYawDamping, then angular Right by kvfCarImpulsePitchDamping. Breaker's
    //     exact var_40/var_30 dataflow is 0x825D4C58..0x825D4D14; DecFIGS names the three globals,
    //     and the X360/PS3 initialisers pin roll=1.0f, yaw=0.75f, pitch=1.0f. When crashing the raw
    //     pair passes through unchanged (branch at 0x825D4C4C..0x825D4C54).
    //   AddWorldSpaceImpulse(J) ; AddWorldSpaceAngularImpulse(rxJ).
    void VehiclePhysics::ApplyCarContactImpulse(Vector3 lvLocalImpulse,
                                                rw::physics::InputSpace leImpulseSpace,
                                                Vector3 lvWorldImpulseDirection,
                                                Vector3 lvContactPosition,
                                                rw::physics::InputSpace lePositionSpace)
    {
    // Breaker 0x825D4C28 replaces v2 with v3 before the base call.  The direction is part of the
    // console contract, but this handler intentionally uses the contact position instead.
    (void)lvWorldImpulseDirection;
    ++miNumCollisions;   // +0x1354

    Vector3 lvJ;
    Vector3 lvAngularJ;
    // The base kernel @0x825A1A80. r4/r5 (the two InputSpace tags) are NEVER written before the
    // `bl` at 0x825D4C48 -- they are this function's own arguments, passed straight through.
    GetImpulsesFromLocalImpulse(lvLocalImpulse, leImpulseSpace,
                                lvContactPosition, lePositionSpace, &lvJ, &lvAngularJ);

    if (!IsCrashing())   // +0x710 master gate
    {
        static const f32 KVF_CAR_IMPULSE_ROLL_DAMPING  = 1.0f;  // kvfCarImpulseRollDamping / unk_82FB8870
        static const f32 KVF_CAR_IMPULSE_YAW_DAMPING   = 0.75f; // kvfCarImpulseYawDamping  / unk_82FB9B70
        static const f32 KVF_CAR_IMPULSE_PITCH_DAMPING = 1.0f;  // kvfCarImpulsePitchDamping / unk_82FB9120

        const Vector3& lvRight = mTransform.Right();
        const Vector3& lvUp = mTransform.Up();
        const Vector3& lvAt = mTransform.At();

        // var_40, 0x825D4C8C/0x4CB4..0x4CBC: linear Up is removed with no damping.
        const f32 lfLinearUp = vpu::Dot(lvJ, lvUp);
        lvJ.x -= lvUp.x * lfLinearUp;
        lvJ.y -= lvUp.y * lfLinearUp;
        lvJ.z -= lvUp.z * lfLinearUp;

        // 0x825D4CCC..0x825D4CF0: +0x1070 lane z is the DWARF GetCarCarResponse() value.
        const f32 lfCarCarResponse =
            mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.z;
        const f32 lfLinearAt = vpu::Dot(lvJ, lvAt) * lfCarCarResponse;
        lvJ.x -= lvAt.x * lfLinearAt;
        lvJ.y -= lvAt.y * lfLinearAt;
        lvJ.z -= lvAt.z * lfLinearAt;

        // var_30, 0x825D4C80/0x4CA8..0x4CC0: damp roll about the forward (At) axis.
        const f32 lfAngularAt =
            vpu::Dot(lvAngularJ, lvAt) * KVF_CAR_IMPULSE_ROLL_DAMPING;
        lvAngularJ.x -= lvAt.x * lfAngularAt;
        lvAngularJ.y -= lvAt.y * lfAngularAt;
        lvAngularJ.z -= lvAt.z * lfAngularAt;

        // 0x825D4CD8..0x825D4D00: damp yaw about Up after the roll component is removed.
        const f32 lfAngularUp =
            vpu::Dot(lvAngularJ, lvUp) * KVF_CAR_IMPULSE_YAW_DAMPING;
        lvAngularJ.x -= lvUp.x * lfAngularUp;
        lvAngularJ.y -= lvUp.y * lfAngularUp;
        lvAngularJ.z -= lvUp.z * lfAngularUp;

        // 0x825D4CFC..0x825D4D14: the final Right-axis pitch pass also targets var_30.
        const f32 lfAngularRight =
            vpu::Dot(lvAngularJ, lvRight) * KVF_CAR_IMPULSE_PITCH_DAMPING;
        lvAngularJ.x -= lvRight.x * lfAngularRight;
        lvAngularJ.y -= lvRight.y * lfAngularRight;
        lvAngularJ.z -= lvRight.z * lfAngularRight;
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
    void VehiclePhysics::ApplyCrashedContactImpulse(Vector3 lvLocalImpulse,
                                                    rw::physics::InputSpace leImpulseSpace,
                                                    Vector3 lvContactPosition,
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

// [clean] ApplyWallContactImpulse  @ FLAGS: none -- both floats are IMAGE-READ (flt_82097F40 == 0.65 is the
// gate, flt_82004C68 == 0.70 the scale) and the whole 0x825FEA18..0x825FEBA4 span is transcribed
// instruction for instruction; see the 2026-09-05 re-audit note below.
    // @0x825FEA18  BrnPhysics::Vehicle::VehiclePhysics::ApplyWallContactImpulse
    //
    // ⭐⭐ THE `[partial]` MARKER AND ITS "flagged-inert projection" WERE STALE (momentum wave,
    // 2026-09-05) -- SEVENTH "a file's own comment is the regression" this campaign. The banner
    // claimed "the per-axis VMX tangential-projection magnitude ... is not store-faithfully
    // recoverable" and "the un-homed projection permute vector stays inert", while the body ten
    // lines below already carried the permute DECODED (the note on lvBodyContactPosition names
    // unk_82CDA350's control bytes and shows the .w lane is a duplicate of .x that no consumer
    // reads). The whole function was re-read off the asm this wave and there is nothing structural
    // left in it:
    //   0x825FEA5C/6C  ++mi8NumWorldCollisions (+0x1353)              -- lbz/addi/stb
    //   0x825FEA80/88  ++miNumCollisions (+0x1354)                    -- lwz/addi/stw
    //   0x825FEA70/74  vrlimi128 mask 1 -> +0x1070 lane .w = 0
    //   0x825FEA78/8C  v126 = impulse * 0.5 * 0.5                     -- two vcfsx(vspltisw 1,1)
    //   0x825FEACC     r30 = this + 0x10                              -- the ExternalPhysicsBody base
    //   0x825FEAC8/D0  delta = contactPos - mTransform.wAxis (+0x40)
    //   0x825FEAD8/E8/EC   the three rotation rows +0x20 (Up) / +0x30 (At) / +0x10 (Right)
    //   0x825FEB04/0C/14   three vmsum3fp128 == dot3(row, delta)
    //   0x825FEB30/34  vperm(unk_82CDA350) + vrlimi mask 2 -> {R.d, U.d, At.d, R.d}
    //   0x825FEB2C     vcmpgtfp. v10(=splat flt_82097F40), v11(=splat dir.y)   -- the ONLY gate
    //   0x825FEB44     beq -> loc_825FEB8C  (skip the whole block when NOT (K > dir.y))
    //   0x825FEB4C/54  vrlimi128 mask 4 -> BOTH the body contact position's .y AND v126's .y = 0
    //   0x825FEB7C/80/84   pos * splat(flt_82004C68), vrlimi mask 2 -> keep only the .z lane
    //   0x825FEB98/9C  li r5,1 (BODY_SPACE position) / r4 = the forwarded impulse space
    //   0x825FEBA4..C4 GetImpulsesFromLocalImpulse -> AddWorldSpaceImpulse + AddWorldSpaceAngularImpulse
    // BOTH constants are now IMAGE-READ rather than "inline literals": x360rd 0x82097F40 ==
    // 0x3F266666 == 0.649999976 and 0x82004C68 == 0x3F333333 == 0.699999988. And the asm has NO
    // else-arm -- `beq` jumps straight to the call -- so KF_WALL_RESTITUTION_LOW below is
    // genuinely dead on the console too, not a missing branch.
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
    // FIDELITY: CLEAN. Every store, gate and constant above is asm- or image-attested; the only
    // deliberate divergence is the .w lane of lvBodyContactPosition (0 here, R.d on the console),
    // which the note at that line proves no consumer reads.
    // ⚠️ WHAT THIS RE-AUDIT CANNOT SAY: it does not make the WALL RESPONSE 1:1 end to end. It says
    // this function is a faithful transcription of 0x825FEA18. The magnitude that ARRIVES here is
    // decided upstream (ApplyCarWorldImpulse -> ApplySensorImpulse -> the sensor's pass-on), and
    // that supply is where any remaining "the car stops too fast" would have to live.
    void VehiclePhysics::ApplyWallContactImpulse(Vector3 lvLocalImpulse,
                                                 rw::physics::InputSpace leImpulseSpace,
                                                 Vector3 lvWorldImpulseDirection,
                                                 Vector3 lvContactPosition,
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

    // "closing speed" IS the world-impulse-direction y lane (0x825FEAB4-C0), NOT a dot
    // product against mLinearVelocity -- the function never loads +0x50.
    const f32 lfClosingSpeed = lvWorldImpulseDirection.y;

    const bool lbLowDirectionY = lfClosingSpeed < KF_WALL_CLOSING_SPEED_THRESHOLD;

    // Breaker 0x825FEAC8-0x825FEB38 converts the WORLD contact position to BODY coordinates before
    // calling the base impulse kernel with the literal BODY_SPACE tag.
    const Vector3 lvDelta{ lvContactPosition.x - mTransform.Pos().x,
                           lvContactPosition.y - mTransform.Pos().y,
                           lvContactPosition.z - mTransform.Pos().z,
                           0.0f };
    // .w lane: the console's vperm control (unk_82CDA350 = 00010203 14151617 00010203 00010203,
    // image-read by the physics11 audit) makes the body position {R.d, U.d, At.d, R.d} -- its .w
    // is a DUPLICATE of lane x. Verified consumed-nowhere (2026-08-24, showtime wave):
    // GetImpulsesFromLocalImpulse receives this with the literal BODY_SPACE tag below, and the
    // BODY_SPACE arm is RotateToWorld = xAxis*p.x + yAxis*p.y + zAxis*p.z -- the .w lane never
    // enters the arithmetic. The 0.0 here is observably identical to the console's R.d.
    Vector3 lvBodyContactPosition{ vpu::Dot(mTransform.Right(), lvDelta),
                                   vpu::Dot(mTransform.Up(), lvDelta),
                                   vpu::Dot(mTransform.At(), lvDelta),
                                   0.0f };

    // v126 is the incoming impulse multiplied by 0.5 twice.  Only the conditional lane operations
    // at 0x825FEB48-0x825FEB88 modify it further: clear Y, clear contact Y, and scale contact Z.
    static const f32 KF_WALL_IMPULSE_PRESCALE = 0.25f;   // vcfsx(1,1)=0.5 applied twice
    Vector3 lvScaledImpulse = { lvLocalImpulse.x * KF_WALL_IMPULSE_PRESCALE,
                                lvLocalImpulse.y * KF_WALL_IMPULSE_PRESCALE,
                                lvLocalImpulse.z * KF_WALL_IMPULSE_PRESCALE,
                                0.0f };
    if (lbLowDirectionY)
    {
        lvScaledImpulse.y = 0.0f;             // vrlimi128 mask 4
        lvBodyContactPosition.y = 0.0f;       // vrlimi128 mask 4
        lvBodyContactPosition.z *= KF_WALL_RESTITUTION_HIGH; // vrlimi128 mask 2
    }

    Vector3 lvJ;
    Vector3 lvAngularJ;
    // 0x825FEB98-0x825FEBA4: `li r5,1` (BODY_SPACE for the position) and `mr r4,r29` where r29
    // was `mr r29,r4` at entry -- the impulse space is forwarded, the position space is a literal.
    GetImpulsesFromLocalImpulse(lvScaledImpulse, leImpulseSpace,
                                lvBodyContactPosition, rw::physics::BODY_SPACE, &lvJ, &lvAngularJ);

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
    void VehiclePhysics::ApplyShowtimeContactImpulse(Vector3 lvLocalImpulse,
                                                     rw::physics::InputSpace leImpulseSpace,
                                                     Vector3 lvContactPosition,
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

// [clean] AddSlam  @ FLAGS: none -- flt_82F2A294 (the air-time taper denominator) is image-read = 150.0 and landed (KF_SLAM_TAPER_DENOM below); the rate-limit 0.5, base 4.0, fsel clamps and all member stores are exact. (Banner refreshed 2026-08-24, showtime wave: it still said "flagged-0 placeholder" after the value landed.)
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
    s8 VehiclePhysics::AddSlam(bool lbTaper, f32 lfDuration, f32 lfSteer, f32 lfRecoveryTime, s8 li8RaceCarId)
    {
    static const f32 KF_SLAM_RATE_LIMIT = 0.5f;            // inline 0.5 -- min gap between slams
    static const f32 KF_SLAM_BASE_SCALE = 4.0f;           // inline 4.0 -- base steering kick
    // flt_82F2A294 @0x82F2A294 .data = 0x43160000 = 150.0 (already in the image). ROLE CORROBORATED:
    // AddSlam @0x825D48E0 does `fdivs f0, airTime, K` and then clamps to [0,1] -- a denominator, which
    // is exactly what the name claims.
    static const f32 KF_SLAM_TAPER_DENOM = 150.0f;        // flt_82F2A294
    static const s8  KI8_SLAM_NUMBER_MAX = 2;             // saturate

    if (!(mSlamEffect.mfSlamLife <= 0.0f) && !((mSlamEffect.mfTotalSlamTime - mSlamEffect.mfSlamLife) >= KF_SLAM_RATE_LIMIT))
        return 0;   // Breaker 0x825D4944 returns zero on both paths.

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
    return 0;
    }

// [clean] AddShunt  @0x825FC630
    // @0x825FC630  BrnPhysics::Vehicle::VehiclePhysics::AddShunt
    //   __fastcall with THREE VMX128 args the pseudocode drops (v1=magnitude,
    //   v2=world direction, v3=speed-increase-to-quit) alongside the char li8RaceCarId.
    //   Gate: if the shunt is already active (mDirectionPlusDesiredSpeed.w (+0x1130 lane3) > 0 AND
    //   mv4_Life..x (+0x1140 lane0 = Life) > 0) AND the existing Life (lane0) > 1.0, do NOT overwrite
    //   (the existing, stronger shunt wins). Otherwise (re)arm (0x825FC6C4-73C):
    //     direction (+0x1130 xyz) = lvWorldDirection ARGUMENT v2 verbatim (NOT a computed
    //       perpendicular/normalize -- `vmr v11,v2 ; vrlimi128 v11,v10,1,0` only re-inserts the OLD
    //       .w lane into v11, xyz stay the raw v2 argument).
    //     desiredSpeed (+0x1130 .w) = min(dot3(v2, mLinearVelocity - mUpAxis*dot(mUpAxis,mLinearVelocity))
    //       + lfMagnitude, unk_82FB8B30 ceiling)   [strips the UP component off velocity, dots
    //       the STRIPPED velocity against the direction ARGUMENT v2, not a self-derived direction]
    //     +0x1140 .y = lfSpeedIncreaseToQuit ARGUMENT v3 (vrlimi128 mask4);
    //       +0x1140 .x = unk_82FB90A0 life seed (vrlimi128 mask8).
    //     mi8LastAttackersRaceCarIndex(+0x13E0) = li8RaceCarId.
    //
    // The gating, verbatim direction store, desired-speed dot/clamp, speed-increase-to-quit
    // store, life seed, attacker id, and zero return all follow Breaker.
    s8 VehiclePhysics::AddShunt(VecFloat lvfMagnitude, Vector3 lvWorldDirection,
                                VecFloat lvfSpeedIncreaseToQuit, s8 li8RaceCarId)
    {
    static const f32 KF_SHUNT_ACTIVE_QUIT_THRESHOLD = 1.0f;   // inline vcfsx(1,0)=1.0 compare
    const f32 lfMagnitude = lvfMagnitude.x;
    const f32 lfSpeedIncreaseToQuit = lvfSpeedIncreaseToQuit.x;

    const f32 lfExistingDesiredSpeed = mShuntEffect.mDirectionPlusDesiredSpeed.GetPlus();   // +0x1130 .w
    const f32 lfExistingLife         = mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x;          // +0x1140 .x

    bool lbActive = (lfExistingDesiredSpeed > 0.0f) && (lfExistingLife > 0.0f);
    if (lbActive)
    {
        // an already-active, sufficiently-strong shunt is not overwritten
        if (lfExistingLife > KF_SHUNT_ACTIVE_QUIT_THRESHOLD)
            return 0;
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
    const f32 lfDot = lvWorldDirection.x * lvVelMinusUp.x + lvWorldDirection.y * lvVelMinusUp.y
                     + lvWorldDirection.z * lvVelMinusUp.z;
    f32 lfDesiredSpeed = lfDot + lfMagnitude;
    // plain min, matching the asm's unconditional `vminfp v0, v0, v12`. The `!= 0.0f` guard is gone
    // with the flagged zero it was protecting against.
    if (lfDesiredSpeed > KF_SHUNT_DESIRED_SPEED_CEIL)
        lfDesiredSpeed = KF_SHUNT_DESIRED_SPEED_CEIL;

    // store the direction ARGUMENT verbatim + desired speed into mDirectionPlusDesiredSpeed (+0x1130).
    mShuntEffect.mDirectionPlusDesiredSpeed.SetVector3(lvWorldDirection);
    mShuntEffect.mDirectionPlusDesiredSpeed.SetPlus(lfDesiredSpeed);

    // +0x1140 .y = the speed-increase-to-quit argument; .x = the life seed.
    // THE SHUNT SYSTEM WAS DEAD ON ARRIVAL. This lane seeds mv4_Life_SpeedIncreaseToQuit.x, and
    //   the very first thing AddShunt does on the next call is test that Life lane `> 0` (asm
    //   0x825FC644-4C, `vspltw v13,v13,3 ; vcmpgtfp. v13, v0`). Seeded with 0 the test could never
    //   pass, so EVERY shunt was born already expired -- an unguarded assignment of a placeholder
    //   zero straight into the field that gates the whole AI shunt behaviour.
    //   unk_82FB90A0 <- flt_82001D9C (2.0), static-init splat @0x82C5CB00.
    static const f32 KF_SHUNT_LIFE_SEED_LANE = 2.0f;   // unk_82FB90A0 lane0
    mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = KF_SHUNT_LIFE_SEED_LANE;
    mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y = lfSpeedIncreaseToQuit;

    mi8LastAttackersRaceCarIndex = li8RaceCarId;   // +0x13E0
    return 0; // 0x825FC73C
    }

// [clean] UpdateSlam  @ FLAGS: none -- flt_82F2A500 (0.0025, the mode==1 steering suppression clamp) and flt_82F2A4FC (0.005, the mode==1 steering scale) are image-read and landed (KF_MODE1_STEER_* below); the parabolic envelope env=r-r^2, the -10.0 floor, the 2.0 amplitude and the 0.95/0.9 else-branch clamps are INLINE literals used exactly. (Banner refreshed 2026-08-24, showtime wave.)
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
    void VehiclePhysics::UpdateSlam(BrnPlayerDriverControls* lpControlsCopy, VecFloat lvfFrameTime)
    {
    static const f32 KF_SLAM_LIFE_FLOOR = -10.0f;     // inline -10.0
    static const f32 KF_SLAM_AMPLITUDE  = 2.0f;       // inline 2.0
    static const f32 KF_SLAM_STEER_CLAMP = 0.94999999f; // inline 0.95
    static const f32 KF_SLAM_GAS_FLOOR   = 0.89999998f; // inline 0.9
    static const f32 KF_SLAM_GAS_BLEND   = 0.1f;        // inline 0.1
    // THESE TWO WERE HELD BACK ON PURPOSE and are released only now that the mode-1 branch has been
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

    f32& lrGas    = lpControlsCopy->mfGas;       // +0x04
    f32& lrBrake  = lpControlsCopy->mfBrake;     // +0x08
    f32& lrHand   = lpControlsCopy->mfHandBrake; // +0x0C
    f32& lrSteer  = lpControlsCopy->mfSteering;  // +0x10
    const E_DRIVER_TYPE leDriverType = lpControlsCopy->GetType(); // +0x44

    // decay (floored at -10.0)
    f32 lfLife = mSlamEffect.mfSlamLife - lvfFrameTime.x;
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

    if (leDriverType == E_DRIVER_TYPE_AI)
    {
        // mode==1 slam-steer-ADD (flt_82F2A500/4FC image-read; see the release note above).
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

// [clean] SetCrashing  @ FLAGS: none -- the whole 0x825FD088..0x825FD1FC span is transcribed store for
// store; there is NO "weight-vector recompute" and nothing reads mpAttribs+0x280 here (see the
// 2026-09-05 re-audit note below).
    // @0x825FD088  BrnPhysics::Vehicle::VehiclePhysics::SetCrashing  (virtual override)
    //
    // ⭐⭐ THE `[partial]` MARKER WAS STALE, AND ITS DESCRIPTION OF THE TAIL WAS WRONG (momentum
    // wave, 2026-09-05). It said the function ends by "recomputing a contact/weight vector from
    // mpAttribs+0x280 (vspltw .w) and the body axes (+0x30 columns)". The tail reads mpAttribs
    // **+0x30**, not +0x280, and +0x30 is mCrashExtraVelocityFactors (VehicleAttribs.h carries the
    // static_assert). There are no body-axis columns anywhere in the block. Decoded operand for
    // operand (the classic-`vmaddfp` rule: printed D,A,B,C == D = A*C + B):
    //   0x825FD164/6C  r9 = mpAttribs (+0x720) ; v0 = *(mpAttribs+0x30) == the four factors
    //   0x825FD174/78/90   v8 = splat(f.x)  v10 = splat(f.z)  v12 = splat(f.y)
    //   0x825FD188/A0/A8   v9 = splat(w.x)  v13 = splat(w.y)  v11 = splat(w.z)   (w == +0x60 omega)
    //   0x825FD1A4  vmaddfp v0, v9, v0, v8    -> omega.x*f.x + omega   ; vrlimi mask 8 keeps lane .x
    //   0x825FD1BC  vmaddfp v0, v13, v0, v12  -> omega.y*f.y + omega   ; vrlimi mask 4 keeps lane .y
    //   0x825FD1D0  vmaddfp v0, v11, v0, v10  -> omega.z*f.z + omega   ; vrlimi mask 2 keeps lane .z
    //   0x825FD1E4/E8   v0 = mLinearVelocity with lane .y replaced by v7 (== vspltisw 0)
    //   0x825FD1F4/F8   v13 = splat(f.w) ; vmaddfp v0, v0, v12, v13 -> vWithZeroY*f.w + v
    //   0x825FD1FC  stvx128 -> +0x50            (so x,z scale by 1+f.w and y is preserved)
    // ⇒ the committed five lines below ARE the console's arithmetic, and the `[partial]` was
    //   costing every reader a phantom missing block. (Eighth "a file's own comment is the
    //   regression" this campaign -- see the sibling retirement on ApplyWallContactImpulse.)
    // ⛔ AND IT SETTLES A CAMPAIGN QUESTION: the three ANGULAR factors are named
    //   CrashExtraPitch/Yaw/RollVelocityFactor (VehicleAttribs.cpp:837-839) and read 0/0.3/0.3 on
    //   the Cavalry -- but the console MULTIPLIES the EXISTING omega by them. Measured at crash
    //   entry (run mom_B1) omega is ~(0.004, 0.001, -0.018) rad/s, so 1.3x of it is still ~0.
    //   **The "crash extra roll" is NOT a barrel-roll seed on the console either**; only the .w
    //   LINEAR factor does visible work (the exact 1.3x on x/z the [dv] witness banked three times).
    //   Zeroes selected lanes of the drift/boost SIMD bank (vrlimi128 mask convention 8=.x,4=.y,2=.z,
    //   1=.w, cross-validated against ApplyBoostKickForce/UpdateBoost in this TU): +0x1000 mask1 ->
    //   .w=DriftScale=0 ; +0x1010 mask2 -> .z=TimeDrifting=0 ; +0x1020 mask8 -> .x=DesiredDriftAngleScale=0 ;
    //   +0x1030 mask8 -> .x=LatDriftForceFactor=1.0, then mask1 -> .w=CurrentDriftAngle=0 ;
    //   +0x1040 mask8 -> .x=SideForceMag=0 (pre-base-call). Clears mu8DriftState: *(this+4946)=0
    //   (mu8DriftState +0x1352). Sets the slam marker mDriftFlags(+0x10F4) = -1 (stb -1). Chains to
    //   SimpleVehiclePhysics::SetCrashing() (sets mbIsCrashing +0x710, rebuilds wheel reciprocal mass).
    //   POST-base-call: +0x1040 mask4 -> .y=TimeBoosting=0 (0x825FD158-160, a SECOND +0x1040 write the
    //   committed body previously omitted). Then *(this+4958)=0 (+0x135E byte, mbOverrideSteering)
    //   and the four crash-extra-velocity factors are applied (see the decode above).
    //
    // FIDELITY: CLEAN. Every store in 0x825FD088..0x825FD1FC has a counterpart below, and the last
    // silent-zero in the function is RECOVERED this wave rather than assumed:
    //   unk_83018040 (loaded at 0x825FD134, multiplied by mNormLinearVelocityMag.w into the
    //   SpeedOnLastCrashMPH lane) is a .data splat -- 0 in the image BY DEFINITION. findinit.py
    //   gives it exactly two sites: the reader here and a CRT thunk at 0x82C6D180, which decodes
    //   `lis r11,0x8302 / addi r11,r11,-0x7F50 (== 0x830180B0) / lvx v0 / vspltw v0,v0,0 /
    //    addi r11,...,-0x7FC0 (== 0x83018040) / stvx v0` -- a splat of the scalar at 0x830180B0.
    //   That scalar is ITSELF dyn-init, by the thunk at 0x82C6D0C0:
    //     lfs f0, 0x1C98(0x8200)  == flt_82001C98 == 1.0        (image-read)
    //     lfs f13, 0x1928(0x82F3) == flt_82F31928 == 0.44703999 (image-read, the MPH->MPS constant)
    //     fdivs f0, f0, f13 ; stfs f0, 0x830180B0
    //   i.e. 1.0 / 0.44703999 == 2.2369363 == KF_MPS_TO_MPH. The value the body uses was right;
    //   it is now DERIVED instead of inferred, and the same 0x82F31928 is the third independent
    //   confirmation of the MPH<->MPS pair this campaign has banked.
    void VehiclePhysics::SetCrashing()
    {
    mu8DriftState = eDriftState_None;

    // zero the drift/boost bank lanes the asm clears (named-member lane writes where pinned)
    mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = 0.0f;                 // +0x1000 mask1 -> .w
    mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.z = 0.0f;    // +0x1010 mask2 -> .z
    mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.x = 0.0f; // +0x1020 mask8 -> .x
    mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.x = 1.0f; // +0x1030 mask8 -> .x = 1.0
    mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.w = 0.0f; // +0x1030 mask1 -> .w = 0
    mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.x = 0.0f; // +0x1040 mask8 -> .x
    mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.z = 0.0f;            // +0xEF0 mask2

    // slam marker: mDriftFlags = -1
    mDriftFlags.mu8DriftFlags = static_cast<u8>(0xFFu);   // stb -1 (0x10F4)

    // Chain to the base crash arm: latch the crash and match all wheel angular velocities to v/r.
    SimpleVehiclePhysics::SetCrashing();

    // Snapshot the current speed in MPH into the first crash-state lane.
    mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.x =
        mNormLinearVelocityMag.w * KF_MPS_TO_MPH;

    // second +0x1040 write, AFTER the base-class chain (0x825FD158-160): mask4 -> .y = TimeBoosting = 0.
    mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime.y = 0.0f; // +0x1040 mask4 -> .y

    mbOverrideSteering = false;

    // 0x825FD164..0x825FD1FC: apply the four packed crash-extra-velocity factors.  The first
    // three scale the corresponding angular-velocity components; the fourth scales horizontal
    // linear velocity while deliberately preserving Y.
    //
    // ⭐⭐⭐ THIS BLOCK IS THE ANSWER TO "THE CAR CHANGES SPEED ENORMOUSLY IN ONE PHYSICS STEP",
    // AND IT IS CONSOLE BEHAVIOUR (kerb wave 6, 2026-09-03). MEASURED -- three independent
    // events across two runs, with the [dv] one-step velocity witness:
    //     kw6_dv2 f6988->f6989   105.3 -> 137.0 mph
    //         v (13.619181, 0.156167, 45.092186) -> v (17.704935, 0.156167, 58.619843)
    //     kw6_dv1 f6794           34.85 -> 45.30 m/s
    //     kw6_dv1 f8348           50.62 -> 65.81 m/s   (113 -> 147 mph)
    // In every one: x and z multiplied by EXACTLY 1.3 to the last printed digit, y BIT-IDENTICAL,
    // no accumulator drain anywhere in the step, and it lands on the same frame the car's `crash`
    // flag goes 0 -> 1. 1.3 == 1 + mCrashExtraVelocityFactors.w, i.e. CrashExtraLinearVelocityFactor
    // == 0.3 in this car's attribute record (VehicleAttribs.cpp:811, record +0x12C). The [dv] stage
    // marks pin the write to the fixup->crashpred window, i.e. inside
    // VehicleManager::DoCrashPrediction @0x82645FE0 -- the caller path that reaches SetCrashing on a
    // player crash entry.
    // ⇒ WHY IT MATTERS TO THE "weird physics" CAMPAIGN: the giant one-step LOSSES that follow are
    // the deformation solver resolving a real contact -- but against a car whose horizontal
    // momentum the console itself raised 30% one step earlier. Do not read a post-crash
    // deceleration as a defect without subtracting this first.
    // ⛔ NOT TUNABLE. It is a named vehicle attribute applied exactly as the asm applies it;
    // changing it to make a symptom go away is the tuning this project forbids.
    // ⚠️ WHAT IS NOT CONFIRMED: 0.3 is INFERRED from the exact 1.3 ratio, not read out of the
    // attribute record. BRN_CRASH_RESPONSE_DIAG=1 prints `crashExtra=(x,y,z,w)` at crash entry
    // (BrnVehicleManager_UpdateVehiclePhysics.cpp:754) -- one run closes that, and nobody has
    // spent it yet.
    const Vector3Plus& lrCrashFactors = mpAttribs->mBaseAttribs.mCrashExtraVelocityFactors;
    mAngularVelocity.x += mAngularVelocity.x * lrCrashFactors.x;
    mAngularVelocity.y += mAngularVelocity.y * lrCrashFactors.y;
    mAngularVelocity.z += mAngularVelocity.z * lrCrashFactors.z;
    mLinearVelocity.x  += mLinearVelocity.x  * lrCrashFactors.w;
    mLinearVelocity.z  += mLinearVelocity.z  * lrCrashFactors.w;
    }

    // ==============================================================================================
    //  @0x825D5450  BrnPhysics::Vehicle::VehiclePhysics::ClearCrashing   (virtual override)
    //
    //  RACECARPHYSICS VTABLE SLOT 1 -- SETTLED 2026-08-26 BY PROBING THE IMAGE, NOT BY REASONING
    //  (the tree has paid once already for guessing a slot's identity: the slot-0 `Create` shim).
    //  How it was pinned, so no later wave repeats the work:
    //    * VehicleManager::VehicleManager @0x827E4D58 seats `off_820D1034` as the FINAL vptr of
    //      every element of the 0x1460-stride maRaceCarVehicles array (`v2 = a1 + 1856 ; ... ;
    //      *v2 = off_820D1034 ; v2 += 1304`), so 0x820D1034 IS the RaceCarPhysics vtable.
    //    * Cross-check, independent of that constructor: 0x820D1034 + 0x30 == 0x820D1064 ==
    //      0x82639CB8 == RaceCarPhysics::Prepare, which this tree already documents as reached
    //      through "vtable slot +0x30" (see SetTransformFromPositionOnRoad's banner).
    //    * vtable[1] (0x820D1038) == 0x825D5450. That address is an EXPORT-SET HOLE -- there is no
    //      .json for it -- so it was read out of the image bytes directly.
    //    * Its identity is MEASURED, not inferred. It opens with CGS_ASSERT("IsCrashing()") citing
    //      ".../Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp":7408 (both strings read
    //      from .rdata at 0x82094B54 and 0x82094860), and its first two stores are the whole body
    //      of SimpleVehiclePhysics::ClearCrashing @0x825B8EA8, byte for byte
    //      (`li r11,0 ; stb r11,0x710(r3) ; stb r11,0x711(r3) ; blr`). It is therefore the derived
    //      override of the virtual declared in BrnSimpleVehiclePhysics.h.
    //    * The neighbouring slots agree with the declared virtual order:
    //        [0] 0x825D4028 VehiclePhysics::GetSteeringAngle
    //        [1] 0x825D5450 THIS
    //        [2] 0x825FFBB0 VehiclePhysics::SetCrashing   (the `vtable +8` this tree already cites)
    //        [3] 0x826415E8 RaceCarPhysics::Update
    //
    //  THE TAIL IS NOT NEW MATH. 0x825D54BC..0x825D54E8 is the SAME partial slam/shunt clear that
    //  VehiclePhysics::Reset and VehiclePhysics::Destruct already emit in this TU, instruction for
    //  instruction: `stfs f0,0x1114/0x1118/0x111C/0x1120` with f0 == flt_82001CC0 == 0.0f,
    //  `stb -1,0x1128`, `stvx128 <zero> -> +0x1130`, then the two vrlimi128 read-modify-writes of
    //  +0x1140 -- word 0x19840710 is `vrlimi128 v12, v0, 4, 0` (calibrated against the IDA-labelled
    //  copy of that identical word at 0x82633AE0; mask 4 == lane .y) and word 0x18086F10 is mask 8
    //  == lane .x, with v13 == vcfsx(vspltisw -1) == splat(-1.0f).
    //  It is PARTIAL ON PURPOSE, exactly as at the other two sites: mSlamEffect.mForce, .mfDecay
    //  and .mfRecoveryTime survive, and so do the .z/.w lanes of the shunt register.
    //
    //  WHAT IT DOES **NOT** DO, said out loud because a reader will go looking for it: it does not
    //  clear mfTimeCrashing (+0xEF0 lane .y). The console does not clear it here either -- the
    //  counter simply stops accumulating, because its only writer (UpdateCrashing) is gated on
    //  mbCrashing, and SetCrashing re-seeds the register at the start of the next crash.
    // ==============================================================================================
    void VehiclePhysics::ClearCrashing()
    {
        // 0x825D5464..0x825D548C -- the console's own assert, kept verbatim. It is also a genuine
        // tripwire on this build: it says the only legal caller is a reset that FOLLOWS a crash.
        CGS_ASSERT(IsCrashing(), "IsCrashing()");

        // 0x825D54B0 / 0x825D54B4 -- SimpleVehiclePhysics::ClearCrashing's two stores, which
        // Breaker inlined here rather than chaining to the base body.
        mbCrashing               = false;   // +0x710
        mbStartedFatallyCrashing = false;   // +0x711

        // The intentionally partial effect clears -- see the banner.
        mSlamEffect.mfSteering         = 0.0f;   // +0x1114
        mSlamEffect.mfOriginalSteering = 0.0f;   // +0x1118
        mSlamEffect.mfSlamLife         = 0.0f;   // +0x111C
        mSlamEffect.mfTotalSlamTime    = 0.0f;   // +0x1120
        mSlamEffect.mi8SlamNumber      = -1;     // +0x1128

        mShuntEffect.mDirectionPlusDesiredSpeed.SetZero();     // +0x1130 (whole register)
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y =  0.0f;   // +0x1140 vrlimi128 mask 4 == .y
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = -1.0f;   // +0x1140 vrlimi128 mask 8 == .x
    }

    // the vtable-closure gate `IsIgnoringPassedOnImpulses`
    // that lived here is RETIRED. The +0x10 slot is now image-settled as the DWARF virtual
    // IsPlayerVehicleInShowtime (both concrete vtables read off the image -- see the header's
    // banner), and the base default `return false` in VehiclePhysics.h IS the recovered console
    // default (`li r3,0 ; blr`), so there is no missing body left for a trap to guard.

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
    // * COMeff = spec.mMeshOffset -- ONE STEP HERE IS INFERRED FROM MEASUREMENT, stated
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
    // * TWO CONSOLE TERMS DELIBERATELY NOT REPRODUCED, stated plainly:
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
    // THE ORCHESTRATOR (orchestrator wave, 2026-08-07): VehiclePhysics::Update @0x826412C0
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
    // +0x1353 is mi8NumWorldCollisions in the member map; this function reads it as a
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

        ApplyEngineForces(lfGas, lpControls->mfBrake, mbHandBrake, lpControls->mfSteering,
                          lfForwardSpeed, lpControls->mfBoostMaxSpeedScale, lvfTimeStep);
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
    void VehiclePhysics::ApplyEngineForces(f32 lfGas, f32 lfBrake, bool lbHandBrake,
                                           f32 lfSteering, f32 lfForwardSpeed,
                                           f32 lfBoostMaxSpeedScale, VecFloat lvfTimeStep)
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

        // ⛔ D6 (drift-symmetry wave 2026-09-02, raw words 0x8261FD78..0x8261FDE0): the
        // standing-still arm compares TimeStandingStill (+0x1060 .x, `vspltw v13, v12, 0`) against
        // var_60, and the ONLY write to var_60 before the compare is `stfs f13, var_60` at
        // 0x8261FD78 with f13 == flt_8208F834 == 0.25f (the same register the counter-steer block
        // uses as its 0.25 floor -- one shared constant, two roles). The old `> 0.0f` let reverse
        // engage the instant the car stopped; the console waits a quarter second.
        static const f32 KF_TIME_STANDING_STILL_TO_ALLOW_REVERSE = 0.25f;   // flt_8208F834
        const bool lbAllowReverseDrive =
            (mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.x
             > KF_TIME_STANDING_STILL_TO_ALLOW_REVERSE)
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
            ApplyEngineForcesOntoWheels(lfDriveScale, lbHandBrake, lfForwardSpeed,
                                        lfBoostMaxSpeedScale);
    }

// [clean] ApplyEngineForcesOntoWheels  @0x825FB000
    // @0x825FB000  BrnPhysics::Vehicle::VehiclePhysics::ApplyEngineForcesOntoWheels  (128 insns)
    //
    // Register-traced. The drive force is mEngine.GetEngineDrive() (mvEngineDrive lane, +0xFA0)
    // scaled by the counter-steer factor. It is zeroed above the max speed -- the cap is the boost
    // MaxBoostSpeed (mBoostAttribs +0x290 .y) while a boost is running (TimeBoosting == +0x1040 .y
    // > 0), else the attribs MaxSpeed (mvMass_..._MaxSpeed_... +0x70 .z), each times the
    // boost-max-speed scale. Then the drive lands on the wheels' TORQUE ACCUMULATOR
    // (maWheels[i].mIntegrationVariables **.y**):
    //
    // This body used to add the drive into `.z`, and
    // `.z` is the wheel's VISUAL ROTATION ANGLE -- `UpdateWheels` publishes it as
    // `WheelLite::mfRotation` and advances it as `wrap(.z + .x * dt)` (0x8261F494). The drive was
    // therefore thrown away every frame: nothing wrote `.y`, so the torque-integrate stage
    // (0x8261EA58, `.x += .y * dt * invInertia ; .y = 0`) added zero and every wheel stayed a pure
    // ROLLING SLAVE of the body -- measured in-game as omega*radius == |v| to four figures at every
    // sample, with a 5x change in engine drive moving the car's trajectory by nothing at all.
    // Every store in the X360 body is `vrlimi128 ..., 4, 0` over the four wheel bases
    // r3+0x160/0x240/0x320/0x400, and mask 4 is lane **.y** (the 8/4/2/1 == x/y/z/w convention is
    // re-proved in SetWheelVelocities' banner above). `UpdateEngine` @0x82638320 runs before
    // `UpdateWheels` @0x8263840C, so accumulate -> integrate -> clear lands in one tick.
    //   * no handbrake, OR handbrake below 5 mph (unk_8208FB14 == KF_SPEED_TO_ALLOW_LOCKING_WHEELS):
    //     all four wheels, split PowerToRear(+0xB0 .z) for the rear pair / PowerToFront(+0xB0 .y)
    //     for the front pair;
    //   * handbrake at/above 5 mph: the raw drive is added into the REAR pair only (wheel lock).
    void VehiclePhysics::ApplyEngineForcesOntoWheels(f32 lfDriveScale, bool lbHandBrake,
                                                     f32 lfForwardSpeed,
                                                     f32 lfBoostMaxSpeedScale)
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
            maWheels[eRearLeftWheel ].mIntegrationVariables.y += lfDrive;
            maWheels[eRearRightWheel].mIntegrationVariables.y += lfDrive;
            return;
        }

        // normal distribution: rear pair scaled by PowerToRear, front pair by PowerToFront.
        maWheels[eRearLeftWheel  ].mIntegrationVariables.y += lfDrive * lfPowerToRear;
        maWheels[eRearRightWheel ].mIntegrationVariables.y += lfDrive * lfPowerToRear;
        maWheels[eFrontLeftWheel ].mIntegrationVariables.y += lfDrive * lfPowerToFront;
        maWheels[eFrontRightWheel].mIntegrationVariables.y += lfDrive * lfPowerToFront;
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

            const VecFloat lvDown = GetDownForce();
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
        SetupSuspension();
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
        SetupSuspension();
        mbIsUsingAIDonutAttribs = false;
    }

    // ==============================================================================================
    // THE WHEEL CLUSTER (wheel-cluster wave, 2026-08-07). UpdateWheels @0x8261E4F0 (1130
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
    // THE PER-WHEEL TRACTION/CONTACT CORE -- the stage UpdateDriving runs between the
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
            { const VecFloat lvGripRR = GetSurfaceGrip(eRearRightWheel);
              const VecFloat lvGripRL = GetSurfaceGrip(eRearLeftWheel);
              HandleWheelPairFriction(eRearLeftWheel, eRearRightWheel, lvAt,
                                      VecFloat{ lfRearGrip, lfRearGrip, lfRearGrip, lfRearGrip },
                                      lvfTimeStep,
                                      VecFloat{ lvGripRL.x, lvGripRL.y, lvGripRL.z, lvGripRL.w },
                                      VecFloat{ lvGripRR.x, lvGripRR.y, lvGripRR.z, lvGripRR.w },
                                      lbMostWheels, false); }

            CalculateNewVelocity(lvfTimeStep);

            // front axle.
            { const VecFloat lvGripFR = GetSurfaceGrip(eFrontRightWheel);
              const VecFloat lvGripFL = GetSurfaceGrip(eFrontLeftWheel);
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

        // ---- [wsus-t] the tyre half of the witness (player car only; same frame index) -------
        if (WheelSusProbeArmed() && mPreviousControls.GetType() == E_DRIVER_TYPE_PLAYER)
        {
            const u32 luF   = WheelSusFrameFor(this);
            const u32 luCar = static_cast<u32>(reinterpret_cast<u64>(this));
            for (s32 liW = 0; liW < eNumDrivenWheels && WheelSusTakeLine(); ++liW)
            {
                const Wheel& lrW = maWheels[liW];
                *CgsDev::Log::gpDebugPrint
                    << "[wsus-t] f " << static_cast<s32>(luF) << " car " << luCar << " w " << liW
                    << " Flong " << lrW.mForceVariables.x << " Flat " << lrW.mForceVariables.y
                    << " FlongC " << lrW.mForceVariables.z << " FlatC " << lrW.mForceVariables.w
                    << " slip " << lrW.mSlipVariables.y << " latSpd " << lrW.mSlipVariables.x
                    << " omega " << lrW.mIntegrationVariables.x
                    << " roadLong " << lrW.mSpeedAndMassOnWheelVariables.x
                    << " surfSpd " << lrW.mSpeedAndMassOnWheelVariables.w
                    << " mow " << lrW.mSpeedAndMassOnWheelVariables.z
                    << " broke " << (lrW.mbBrokenAdhesiveLimit ? 1 : 0)
                    << " vel " << mLinearVelocity.x << " " << mLinearVelocity.y << " " << mLinearVelocity.z
                    << "\n";
            }
        }
        // ---- end [wsus-t] ---------------------------------------------------------------------
    }

// [clean] SetAttributes  @0x8262DE58
    // @0x8262DE58  BrnPhysics::Vehicle::VehiclePhysics::SetAttributes  (185 insns)
    // The post-reset attribs re-derivation (UpdateDriving's copy.mbReset branch, before
    // HackedResetAndFlyAround). Decoded store-for-store:
    //   1. SimpleVehiclePhysics::SetAttributes()           (result discarded -- bl @0x8262DE70)
    //   2. capture radii (each wheel's mSlipVariables.w) and positions
    // (streamed pos + mpAttribs COM, y -= mpAttribs suspension height offset) -- the
    //      console DEREFERENCES mpAttribs during this capture BEFORE asserting it non-null;
    //      order preserved
    //   3. assert mpAttribs != NULL (0x17A) / mpAttribs->IsValid() (0x17B)
    //   4. the AttribSys chase: burnoutcarasset(mpAttribs->mAttribsKey) ->
    //      handling RefSpec (data+0x158) -> physicsvehiclehandling -> checked copy @0x825BDB88
    //      -> VehicleAttribs::SetupAttribs(handling)        [TRAP until its wave -- see the
    //      link-stubs census; the re-stream is the one leg of this function not yet real]
    //   5. mpAttribs->mBaseAttribs.mCOMOffset += mHandlingBodyOffset
    //   6. SimpleVehiclePhysics::SetAttributes(positions, radii)   (result discarded)
    //   7. mEngine.Prepare(&mpAttribs->mEngineAttribs) -- the X360 INLINES Engine::Prepare
    //      @0x825F3F38 here (the 0xA0 memcpy + clutch lane zero + gear 1 + Engine::Reset(0));
    //      the named call is that function instruction-for-instruction
    //   8. SetupSuspension(); return true (li r3,1)
    bool VehiclePhysics::SetAttributes()
    {
        SimpleVehiclePhysics::SetAttributes();   // the 0-arg base refresh, result unused

        // ---- the capture (mpAttribs dereferenced before the null assert -- as shipped) --------
        f32 lafRadii[eNumDrivenWheels];
        Vector3 laPositions[eNumDrivenWheels];
        {
            const f32 lfFrontOffset =
                mpAttribs->mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.x;
            const f32 lfRearOffset =
                mpAttribs->mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.y;
            for (s32 li = 0; li < eNumDrivenWheels; ++li)
            {
                lafRadii[li] = maWheels[li].mSlipVariables.w;
                laPositions[li] = vpu::Add(maWheels[li].mStreamedPositionPlusTwistAmount.GetVector3(),
                                           mpAttribs->mBaseAttribs.mCOMOffset);
                laPositions[li].y -= (li < 2) ? lfFrontOffset : lfRearOffset;   // vrlimi(4)
            }
        }

        CGS_ASSERT(mpAttribs != NULL,     "mpAttribs != NULL");       // console line 0x17A
        CGS_ASSERT(mpAttribs->IsValid(),  "mpAttribs->IsValid()");    // console line 0x17B

        // ---- the AttribSys handling re-stream -------------------------------------------------
        {
            Attrib::Gen::burnoutcarasset lCarAsset(mpAttribs->mAttribsKey, NULL);
            Attrib::Gen::physicsvehiclehandling lHandling(
                const_cast<Attrib::Collection*>(
                    lCarAsset.GetPhysicsVehicleHandlingRefSpec()->GetCollection()), NULL);
            Attrib::Gen::physicsvehiclehandling lHandlingCopy(lHandling);   // @0x825BDB88
            mpAttribs->SetupAttribs(lHandlingCopy);
        }

        // mpAttribs COM nudge ([mpAttribs+0x20] += [this+0x690]).
        mpAttribs->mBaseAttribs.mCOMOffset =
            vpu::Add(mpAttribs->mBaseAttribs.mCOMOffset, mHandlingBodyOffset);

        SimpleVehiclePhysics::SetAttributes(laPositions, lafRadii);   // result unused

        mEngine.Prepare(&mpAttribs->mEngineAttribs);   // the inlined Engine::Prepare @0x825F3F38
        SetupSuspension();
        return true;   // li r3, 1
    }

// [clean] SetAttributes(VehicleAttribs*, const Vector3*, const f32*)  @0x8262E140
    // ==============================================================================================
    // @0x8262E140  BrnPhysics::Vehicle::VehiclePhysics::SetAttributes  (48 insns)
    //
    // THE IDA DATABASE LEAVES THIS ONE UNNAMED (`sub_8262E140`); the identification is settled by
    // three independent facts, not by its role -- see the declaration banner in VehiclePhysics.h.
    // The decisive one is the second assert's BAKED __FILE__/__LINE__ pair:
    // ".../VehicleManager/VehiclePhysics/VehiclePhysics.cpp", 0x19A == 410. A SimpleVehiclePhysics
    // function cannot carry a VehiclePhysics.cpp line number.
    //
    // Store/call map, read off the asm:
    //   0x8262E168  the :299 assert -- file ".../BrnSimpleVehiclePhysics.cpp", line 0x12B == 299
    //   0x8262E190  bl SimpleVehicleAttribs::SetupAttribs      r3 = this + 0x5A0 == &mSimpleAttribs
    //   0x8262E1A0  bl SimpleVehiclePhysics::SetAttributes     r4 = positions, r5 = radii
    //   0x8262E1A8  the :410 assert (VehiclePhysics.cpp), same "lpAttribs" message string
    //   0x8262E1CC  stw r30, 0x720(r31)                        mpAttribs = lpAttribs
    //   0x8262E1D8  bl SimpleVehiclePhysics::SetAttributes     the SECOND call, same two arguments
    //   0x8262E1E8  bl Engine::Prepare  r3 = this+0xF00, r4 = mpAttribs + 0x190 (mEngineAttribs);
    //               note the console RE-READS mpAttribs from +0x720 rather than reusing r30
    //   0x8262E1F0  bl VehiclePhysics::SetupSuspension
    //   0x8262E1F4  li r3, 1
    //
    // THE IDENTIFICATION IS NO LONGER CIRCUMSTANTIAL -- CONFIRMED 2026-08-11 by the sibling
    // wave: the PS3 build EXPORTS THIS FUNCTION NAMED at 0x735D20
    // (`_ZN10BrnPhysics7Vehicle14VehiclePhysics13SetAttributesEPNS0_14VehicleAttribsE`
    // `PKN2rw4math3vpu7Vector3EPKf` -- verified in references/DecFIGS/decfigs_func_files.json),
    // and the PS3 body of VehiclePhysics::Prepare @0x735DEC calls it BY THAT NAME with
    // `this + 2704 == &mPlayerVehicleAttribs` -- exactly the `addi r4,r31,0xAA0` the X360 passes.
    // The three circumstantial facts above still hold; they are corroboration now, not the proof.
    //
    // The first three statements are the console-INLINED
    // `SimpleVehiclePhysics::SetAttributes(VehicleAttribs*, const Vector3*, const f32*)`. That
    // overload was DECLARE-ONLY when this body first landed, so it was spelled flat here. It is no
    // longer: the sibling wave bodied it (BrnSimpleVehiclePhysics.cpp, recovered from this very
    // inline plus the matching one in SimpleVehiclePhysics::Prepare, PS3-attested at 0x734B10), so
    // the inline is REVERSED back into the call the original source wrote. That is this project's
    // own de-optimization rule, and it closes a declared-but-undefined symbol no per-TU gate could
    // see (the [[shadowing-redeclarations]] landmine).
    // ==============================================================================================
    bool VehiclePhysics::SetAttributes(VehicleAttribs* lpAttribs,
                                       const Vector3* lpaWheelPositions,
                                       const f32* lpafWheelRadii)
    {
        // ---- the base 3-arg overload, which the console INLINES here (0x8262E16C..0x8262E1A0):
        //      the :299 assert, SimpleVehicleAttribs::SetupAttribs @0x8262E190, then the 2-arg
        //      SetAttributes @0x8262E1A0. Reached BY NAME now that it has a body of its own.
        SimpleVehiclePhysics::SetAttributes(lpAttribs, lpaWheelPositions, lpafWheelRadii);

        // ---- this function's own body ----------------------------------------------------------
        CGS_ASSERT(lpAttribs != 0, "lpAttribs");                 // VehiclePhysics.cpp:410 (0x19A)
        mpAttribs = lpAttribs;                                   // stw r30, 0x720(r31)
        SimpleVehiclePhysics::SetAttributes(lpaWheelPositions, lpafWheelRadii);   // bl @0x8262E1D8

        mEngine.Prepare(&mpAttribs->mEngineAttribs);             // bl @0x8262E1E8 -- the console
                                                                 // RE-READS mpAttribs from +0x720
                                                                 // rather than reusing r30
        SetupSuspension();                                       // bl @0x8262E1F0
        return true;                                             // li r3, 1
    }

// [clean] Prepare  @0x82637C80
    // ==============================================================================================
    // @0x82637C80  BrnPhysics::Vehicle::VehiclePhysics::Prepare  (306 insns)
    //
    // The car-PLACEMENT entry point: install the
    // caller's attribute set as this car's PLAYER set, forward the whole nine-parameter placement
    // into SimpleVehiclePhysics::Prepare, re-derive everything that depends on the attribs, build
    // the parallel AI attribute set, full Reset, then seed ~40 own-block members.
    //
    // THE SIGNATURE IS DWARF-ATTESTED (PS3 export 0x735DEC / DecFIGS VehiclePhysics.h:1069) and the
    // X360 PROLOGUE AGREES REGISTER FOR REGISTER -- which matters here because of the PPC float/
    // vector ABI trap this project keeps paying for. There is NO float scalar in this signature; the
    // four Vector3s ride v1..v4 and consume NO GPR slot at all, so the GPR sequence is dense:
    //     r3  = this                                    (r31)
    //     r4  = &lTransform                             (r28)   -- Matrix44Affine BY VALUE == the
    //                                                              64-byte hidden-pointer form
    //     v1/v2/v3/v4 = lLinearVelocity / lAngularVelocity / lHandlingBodyOffset / lHalfExtent
    //                                                   (v124/v127/v126/v125, saved at entry)
    //     r5  = &lrAABB                                 (r30)
    //     r6  = lpAttribs                               (r25)   -- the ONLY asserted parameter
    //     r7  = lpaWheelPositions                       (r27)
    //     r8  = lpafWheelRadii                          (r26)
    // and the forwarding `bl SimpleVehiclePhysics::Prepare` at 0x82637D24 restores exactly those
    // eight, in the same seats, having touched none of them -- which is the strongest possible
    // confirmation that the two declarations are the same nine parameters in the same order.
    //
    // Asserts, in console order, with the baked line numbers:
    //   :538 (0x21A) "lpAttribs != NULL"
    //   :555 (0x22B) "rw::math::IsValid( mHandlingBodyOffset )"          -- reads this+0x690, 3 lanes
    //   :560 (0x230) "rw::math::IsValid( mLocalInverseInertia )"         -- this+0x80/0x90/0xA0, 3
    //                                                                      lanes each, short-circuit
    //   :561 (0x231) "rw::math::IsValid( mfMass )"                       -- this+0xE0, WHOLE register
    //                                                                      (no vspltw -- all 4 lanes)
    // Note the message spelling: these say `rw::math::IsValid`, where the SimpleVehiclePhysics
    // sibling's say `RwMathVPU::IsValid`. Both are reproduced verbatim from their own .rdata.
    //
    // Call/store map after the forward:
    //   0x82637CE8  addi r29, r31, 0xAA0                      r29 = &mPlayerVehicleAttribs
    //   0x82637CF4  bl VehicleAttribs::operator=              mPlayerVehicleAttribs = *lpAttribs
    //   0x82637D18  stw r29, 0x720(r31)                       mpAttribs = &mPlayerVehicleAttribs
    //   0x82637D24  bl SimpleVehiclePhysics::Prepare          all nine parameters, untouched
    //   0x82637DC8  bl VehiclePhysics::SetAttributes(&mPlayerVehicleAttribs, positions, radii)
    // r4 is r29 == the class's OWN copy, NOT the caller's lpAttribs. mpAttribs is
    //               therefore re-pointed at the same address it already holds -- reproduced as-is.
    //   0x82637F90  bl VehicleAttribs::Construct(this+0x730)  mAIVehicleAttribs.Construct()
    //   0x82637F9C  bl VehicleAttribs::SetupAttribsForAI(this+0x730, lpAttribs)   -- r4 is r25, the
    //               CALLER's set, not the copy
    //   0x82637FA0  ld r11,0x358(r25) ; std r11,0xA88(r31)    mAIVehicleAttribs.mAttribsKey =
    //               lpAttribs->mAttribsKey   (0xA88 - 0x730 == 0x358 -- the same field)
    //   0x82637FB0  bl VehiclePhysics::Reset @0x825FDD78      the Vector3 overload; `vmr128 v1,v124`
    //               one instruction earlier makes the argument lLinearVelocity
    //
    // The seed tail, lane by lane (vrlimi128 mask 8/4/2/1 == x/y/z/w, the convention this class's
    // Reset decode already proved on two ISAs):
    //   +0xFF0  .x .y .z .w = 1.0f     FOUR separate insert+store pairs, i.e. four source statements
    //   +0x1050 .x = unk_8208FAE4, .y = unk_8208FAE8   -- the SAME two rodata floats Reset seeds
    //           this register with (-0.1f / 1.0f, already homed in this file), and .w =
    //           unk_8208FB18 at the very end of the function
    //   +0x1128 = -1  and  +0x1114/+0x1118/+0x1120/+0x111C = flt_82001CC0 (0.0f)   -- the SAME
    //           PARTIAL mSlamEffect clear Reset does (mForce/mfDecay/mfRecoveryTime untouched)
    //   +0x1130 = 0 (whole register), +0x1140 .y = 0 then .x = -1.0f (vcfsx of vspltisw -1)
    //   +0x1158 / +0x1220 = 0 (std)   mUsedAirRams / mUsedSpins
    //   the eight byte stores 0x710/0x712/0x1359/0x10F7/0x135A/0x135C(=1)/0x135D/0x1362, then
    //           0x135E after the +0x1070 .w insert -- order preserved below
    //   +0x1070 .w = unk_8208FADC (0.4f, already homed here as KF_WALL_CONTACT_RESEED), then .y = 0
    // +0x13B0 = 0 ... and then +0x13B0 = v124 near the end. TWO STORES TO THE SAME 16 BYTES,
    //           the second overwriting the first. Reproduced rather than optimised away: the
    //           console emits both (`stvx128 v0,r0,r5` @0x826380D8 and `stvx128 v124,r0,r5`
    //           @0x82638120, r5 == this+0x13B0 in both).
    //   +0x1060 .x = 0, .y = 0
    //   +0x10D4 = 0            mPreviousControls.meDriverType
    //   +0x1370 = lTransform   four rows copied straight out of r28 (the by-value parameter)
    //   +0x13DC = 3            meCarType
    //   +0xFD0  = 1.0f splat   mvfWheelFrictionLinearMultiplier (stvx128 of the same v13)
    //   li r3, 1               returns true
    //
    // ---- CROSS-READ AGAINST THE PS3 BUILD OF THE SAME SOURCE (folded in at the 2026-08-11
    //      merge, from the sibling wave that derived this body independently) ------------------
    // The PS3 twin is export 0x735DEC, 424 insns, symbolled. It was read store for store, lane
    // for lane, against the X360 stream above and AGREES on every store, every lane and every
    // callee. Four things it settled that the X360 alone could not:
    //   * `sub_8262E140` is `VehiclePhysics::SetAttributes` -- named in the PS3 export (0x735D20)
    //     and called by that name from the PS3 Prepare. The X360-side identification above was
    //     by elimination; this is by symbol.
    //   * the `bl Reset` really does CONSUME the incoming v1 (PS3 spells `Reset(_R19, v104)`),
    //     so unlike the SimpleVehiclePhysics::Reset case this is NOT a dropped-argument trap.
    // RE-ARBITRATED AGAINST THE X360 ASM AT THE MERGE and it holds: `vmr128 v1, v124`
    //     @0x82637FA4 sits immediately before `bl Reset` @0x82637FB0, and v124 is the incoming
    //     v1 saved at 0x82637C98 -- i.e. lLinearVelocity. `VehiclePhysics::Reset(Vector3)` is
    //     itself a PS3 symbol (`...VehiclePhysics5ResetEN2rw4math3vpu7Vector3E`), distinct from
    //     the 0-arg `...5ResetEv`. Both bodies already called the Vector3 overload; no divergence.
    //   * `std r30,0x1158` / `std r30,0x1220` are `CgsContainers::BitArray<N>::Prepare()` calls,
    //     not raw `= 0`: the PS3 emits `BitArray<4u>::Prepare(this+0x1148)` and its BitArray<8u>
    // sibling at the matching (Δ = -0x10) offsets. CORROBORATED HERE INDEPENDENTLY: this
    //     class declares KU_MAX_AIR_RAMS == 4 and KU_MAX_SPINS == 8, so the two instantiations
    //     the PS3 names are exactly mUsedAirRams and mUsedSpins. `Prepare()` is a real DWARF
    //     method of the template (11 instantiations carry `..EE7PrepareEv` out of line in the
    //     PS3 export); `UnSetAll()` is DWARF-declared too but is inlined at every site, so the
    //     name the console source used HERE is Prepare(). Changed to match -- same semantics
    //     (one 64-bit zero per single-field array), console-attested name.
    //   * FOUR of the five rodata slots have real PS3 NAMES:
    //     KF_DEFAULT_PROP_SPEED_MAINTAIN_ALONG_Z / _ALONG_VEL, KF_WALL_CONTACT_TIME_SECONDS and
    //     KF_DEFAULT_SOLVE_PENETRATION_WEIGHT_FACTOR -- each landing on the lane whose own name
    //     matches it (`..._SolvePenetrationWeightFactor` .w, `..._SecondsSinceLastWallContact`
    // .w). All four verified present in references/DecFIGS at the merge; adopted below in
    //     place of the role-derived names this body first carried.
    //
    // ---- THE CONSTANTS, READ FROM THE IMAGE (x360rd), not guessed ----------------------------
    //   flt_82001CC0 = 0x00000000 = 0.0f     unk_8208FADC = 0x3ECCCCCD = 0.4f
    //   unk_8208FAE4 = 0xBDCCCCCD = -0.1f    unk_8208FAE8 = 0x3F800000 = 1.0f
    //   unk_8208FB18 = 0x3F800000 = 1.0f
    // The same read returns 0x8208FB0C == 0.035f, which is the analytic-seat constant this file
    // already homes -- an in-band calibration check of the reader on this very rodata page.
    // unk_8208FAE4/E8 are the SAME two slots VehiclePhysics::Reset already seeds into the same two
    // lanes (see Reset above), and unk_8208FADC is the same slot HackedResetAndFlyAround already
    // homes as KF_WALL_CONTACT_RESEED. Three independent prior witnesses, no new guessing.
    //
    // THE SEED BLOCK LEGITIMATELY REPEATS WORK Reset JUST DID (both seed mSlamEffect,
    // mShuntEffect, the air-ram/spin allocators and the +0x1050 pair). That is not a
    // transcription error and it is not a scheduling artifact: the redundancy is present in the
    // PS3 build too, and it is the same shape SimpleVehiclePhysics::Prepare already carries
    // (it clears mbCrashing/mbStartedDeforming, and this function clears them AGAIN at
    // 0x82638088/8C). Reproduced as shipped.
    //
    // mLastLinearVelocity (+0x13B0) IS WRITTEN TWICE -- zeroed at 0x826380D8, then assigned
    // lLinearVelocity at 0x82638120, both `stvx128 ... r0, r5` with r5 == this+0x13B0. Checked
    // rather than "optimised away": the PS3 emits BOTH stores as well (`stvx v26,this,r4` then
    // `stvx v25,this,r4`, r4 == 5024 == its own mLastLinearVelocity), so the dead first store is
    // a real source statement on two compilers. Kept.
    //
    // ONE PS3/X360 DISAGREEMENT, and the X360 wins. Every byte seed maps between the builds at
    // a clean Δ = -0x10, except meDriverType: X360 `stw r30,0x10D4`, PS3 `*(this+0x10C0)` -- Δ =
    // -0x14. So BrnPlayerDriverControls differs by four bytes between the two builds ahead of
    // that field, which this tree's own BrnVehicleDriverControls.h already records (the X360's
    // THIRTEENTH control float at +0x34 pushes meDriverType from +0x40 to +0x44). Reached BY NAME.
    // ==============================================================================================
    bool VehiclePhysics::Prepare(Matrix44Affine lTransform, Vector3 lLinearVelocity,
                                 Vector3 lAngularVelocity, Vector3 lHandlingBodyOffset,
                                 Vector3 lHalfExtent,
                                 const CgsGeometric::AxisAlignedBox& lrAABB,
                                 VehicleAttribs* lpAttribs, const Vector3* lpaWheelPositions,
                                 const f32* lpafWheelRadii)
    {
        // THE NAMES ARE PS3-ATTESTED, not role-derived (adopted at the 2026-08-11 merge from the
        // sibling wave; all four verified present in references/DecFIGS). They replace the
        // KF_..._SEED / KF_..._RESEED_ON_PREPARE stand-ins this body first carried.
        // AND THE VALUE OF THE FOURTH IS GROUND TRUTH, not inference: the conductor's targeted IDA
        // export over BURNOUT_X360_ARTIST.XEX.i64 (the b53e2523 technique, run for the
        // RaceCarPhysics::Prepare hole) dumped the sixteen bytes at 0x8208FB18 --
        // `3f800000 3e800000 3e19999a c1200000` -- and the first big-endian word is 0x3F800000 ==
        // exactly 1.0f. Two waves reached 1.0f from two directions; the image settles it.
        static const f32 KF_DEFAULT_PROP_SPEED_MAINTAIN_ALONG_Z      = -0.1f;  // unk_8208FAE4
        static const f32 KF_DEFAULT_PROP_SPEED_MAINTAIN_ALONG_VEL    =  1.0f;  // unk_8208FAE8
        static const f32 KF_WALL_CONTACT_TIME_SECONDS                =  0.4f;  // unk_8208FADC
        static const f32 KF_DEFAULT_SOLVE_PENETRATION_WEIGHT_FACTOR  =  1.0f;  // unk_8208FB18 (READ)
        static const s32 KI_PREPARE_CAR_TYPE = 3;                    // `li r8,3 ; stw r8,0x13DC`
                                                                     // FLAG: role-derived name --
                                                                     // the 3 is asm-literal, the
                                                                     // NAME has no console witness.

        CGS_ASSERT(lpAttribs != 0, "lpAttribs != NULL");                                    // :538

        // Take a private copy of the incoming set and point the live pointer at it.
        mPlayerVehicleAttribs = *lpAttribs;          // bl VehicleAttribs::operator= @0x82637CF4
        mpAttribs             = &mPlayerVehicleAttribs;                     // stw r29, 0x720(r31)

        // Every one of the nine parameters is forwarded UNCHANGED (r4/v1..v4/r5/r6/r7/r8 are all
        // straight `mr`/`vmr128` moves out of the prologue saves at 0x82637CF8..0x82637D20). Note
        // the base gets the CALLER's lpAttribs, not the copy -- checked, r6 == r25.
        SimpleVehiclePhysics::Prepare(lTransform, lLinearVelocity, lAngularVelocity,
                                      lHandlingBodyOffset, lHalfExtent, lrAABB,
                                      lpAttribs, lpaWheelPositions, lpafWheelRadii);   // @0x82637D24

        CGS_ASSERT(vpu::IsValid(mHandlingBodyOffset),
                   "rw::math::IsValid( mHandlingBodyOffset )");                             // :555

        SetAttributes(&mPlayerVehicleAttribs, lpaWheelPositions, lpafWheelRadii);   // @0x82637DC8

        CGS_ASSERT(vpu::IsValid(mLocalInverseInertia.xAxis) &&
                   vpu::IsValid(mLocalInverseInertia.yAxis) &&
                   vpu::IsValid(mLocalInverseInertia.zAxis),
                   "rw::math::IsValid( mLocalInverseInertia )");                            // :560
        CGS_ASSERT(rw::math::fpu::IsValid(mfMass.x) && rw::math::fpu::IsValid(mfMass.y) &&
                   rw::math::fpu::IsValid(mfMass.z) && rw::math::fpu::IsValid(mfMass.w),
                   "rw::math::IsValid( mfMass )");                                          // :561

        mAIVehicleAttribs.Construct();                                            // @0x82637F90
        mAIVehicleAttribs.SetupAttribsForAI(lpAttribs);                           // @0x82637F9C
        mAIVehicleAttribs.mAttribsKey = lpAttribs->mAttribsKey;   // ld 0x358(r25); std 0xA88(r31)

        Reset(lLinearVelocity);   // bl @0x82637FB0 -- `vmr128 v1,v124` @0x82637FA4 puts the
                                  // incoming lLinearVelocity in v1; the Vector3 overload CONSUMES it
                                  // (PS3 twin agrees: `Reset(_R19, v104)`). Not a dropped argument.

        // ---- the own-block seed tail ----------------------------------------------------------
        mvPlayerStatSpeed_PlayerStatStrength_PlayerStatControl_PlayerStatBoost.x = 1.0f;  // +0xFF0
        mvPlayerStatSpeed_PlayerStatStrength_PlayerStatControl_PlayerStatBoost.y = 1.0f;
        mvPlayerStatSpeed_PlayerStatStrength_PlayerStatControl_PlayerStatBoost.z = 1.0f;
        mvPlayerStatSpeed_PlayerStatStrength_PlayerStatControl_PlayerStatBoost.w = 1.0f;

        mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.x
            = KF_DEFAULT_PROP_SPEED_MAINTAIN_ALONG_Z;                                          // +0x1050 .x
        mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.y
            = KF_DEFAULT_PROP_SPEED_MAINTAIN_ALONG_VEL;                                        // +0x1050 .y

        // The same PARTIAL slam clear Reset does (mForce / mfDecay / mfRecoveryTime kept).
        mSlamEffect.mi8SlamNumber      = -1;                                           // +0x1128
        mSlamEffect.mfSteering         = 0.0f;                                         // +0x1114
        mSlamEffect.mfOriginalSteering = 0.0f;                                         // +0x1118
        mSlamEffect.mfTotalSlamTime    = 0.0f;                                         // +0x1120
        mSlamEffect.mfSlamLife         = 0.0f;                                         // +0x111C

        mShuntEffect.mDirectionPlusDesiredSpeed.SetZero();                             // +0x1130
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y =  0.0f;                           // +0x1140 .y
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = -1.0f;                           // +0x1140 .x

        mUsedAirRams.Prepare();                                                         // +0x1158   <- BitArray<4u>::Prepare (PS3-named; see banner)
        mUsedSpins.Prepare();                                                           // +0x1220   <- BitArray<8u>::Prepare

        mbCrashing                 = false;                                            // +0x710
        mbStartedDeforming         = false;                                            // +0x712
        mbDeformationModelIsActive = false;                                            // +0x1359
        mbIsUsingAIDonutAttribs    = false;                                            // +0x10F7
        mbDeformedThisFrame        = false;                                            // +0x135A
        mbResetCarTransform        = true;                                             // +0x135C
        mbJustBeenSlammed          = false;                                            // +0x135D
        mbContactingWall           = false;                                            // +0x1362

        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w
            = KF_WALL_CONTACT_TIME_SECONDS;                                       // +0x1070 .w
        mbOverrideSteering = false;                                                    // +0x135E
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.y
            = 0.0f;                                                                    // +0x1070 .y

        mLastLinearVelocity.SetZero();                                                 // +0x13B0
        mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.x = 0.0f;     // +0x1060 .x
        mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.y = 0.0f;     // +0x1060 .y

        mPreviousControls.ResetType();                       // +0x10D4 <- 0 == E_DRIVER_TYPE_PLAYER
        mPreviousTransform             = lTransform;                                   // +0x1370
        meCarType                      = KI_PREPARE_CAR_TYPE;                          // +0x13DC
        mvfWheelFrictionLinearMultiplier = VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f };         // +0xFD0

        // The SECOND store to +0x13B0 -- the zero above is overwritten by the argument. Both
        // instructions are in the console body; neither is dropped.
        mLastLinearVelocity = lLinearVelocity;                                         // +0x13B0

        mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.w
            = KF_DEFAULT_SOLVE_PENETRATION_WEIGHT_FACTOR;                                 // +0x1050 .w

        return true;                                                                   // li r3, 1
    }

// [clean] HackedResetAndFlyAround  @0x825D0008
    // @0x825D0008  BrnPhysics::Vehicle::VehiclePhysics::HackedResetAndFlyAround  (139 insns,
    // 0x825D0008..0x825D0230, leaf -- no callee but the GPR save/restore thunks)
    // The dev reset / fly-around handler, gated on copy.mbReset by UpdateDriving @0x82638604.
    // Levels the car onto a world-up basis, flies the position from the stick, kills all
    // motion, and re-seats the wall-contact/slam/shunt/air-ram state.
    //
    // Constants (x360rd image reads, 10/10 self-test): flt_82001CC0 = 0.0f,
    // flt_82001C98 = 1.0f, flt_82004014 = 0.1f, unk_8208FADC = 0.4f.
    //
    // dt (v1) is never read by the body -- the fly speeds are per-CALL, not per-second.
    void VehiclePhysics::HackedResetAndFlyAround(const BrnPlayerDriverControls* lpControls,
                                                 VecFloat lvfTimeStep)
    {
        (void)lvfTimeStep;   // carried but unused, as shipped

        static const f32 KF_FLY_RISE_PER_CALL   = 0.1f;   // flt_82004014 (image-read)
        static const f32 KF_WALL_CONTACT_RESEED = 0.4f;   // unk_8208FADC (image-read) -- the
                                                          // same window UpdateDriving's else-leg
                                                          // compares against (flt_8200473C)

        // ---- 0x825D0010..0x825D00AC: re-level the basis about world up -----------------------
        // up = {0,1,0,0} is built on the stack (raw stack decode: words at r1-0x70/-0x6C/-0x68/
        // -0x64 = {0,1,0,0} -- the w word IS zeroed, via `stw r11,0(r1-0x64)`).
        //   yAxis <- up
        //   xAxis <- cross(up, old zAxis)        (shifted-permwi idiom: vpermwi128 0x63 +
        //   zAxis <- cross(new xAxis, up)         vnmsubfp, twice; nothing is normalised)
        const Vector3 lvUp    = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
        const Vector3 lvOldAt = mTransform.zAxis;
        mTransform.yAxis = lvUp;                                    // stvx128 -> +0x20
        mTransform.xAxis = vpu::Cross(lvUp, lvOldAt);               // stvx128 -> +0x10
        mTransform.zAxis = vpu::Cross(mTransform.xAxis, lvUp);      // stvx128 -> +0x30

        // ---- 0x825D00B0..0x825D0178: fly the position from the controls ----------------------
        //   pos += xAxis * -mfSteering           (lfs 0x10(r4), fneg, splat, vmaddfp)
        //   pos += zAxis * (mfGas - mfBrake)     (lfs 4(r4) - lfs 8(r4), splat, vmaddfp)
        //   pos += yAxis * 0.1f                  (the constant per-call rise)
        mTransform.wAxis = vpu::Add(vpu::Mult(mTransform.xAxis, -lpControls->mfSteering),
                                    mTransform.wAxis);
        mTransform.wAxis = vpu::Add(vpu::Mult(mTransform.zAxis,
                                              lpControls->mfGas - lpControls->mfBrake),
                                    mTransform.wAxis);
        mTransform.wAxis = vpu::Add(vpu::Mult(mTransform.yAxis, KF_FLY_RISE_PER_CALL),
                                    mTransform.wAxis);

        // ---- 0x825D0180..0x825D0194: kill all motion and pending forces ----------------------
        static const Vector3 KV_ZERO = { 0.0f, 0.0f, 0.0f, 0.0f };   // vspltisw v0, 0
        mLinearVelocity      = KV_ZERO;   // +0x50   == base+0x40
        mAngularVelocity     = KV_ZERO;   // +0x60   == base+0x50
        mTotalLinearImpulse  = KV_ZERO;   // +0x110  == base+0x100
        mTotalAngularImpulse = KV_ZERO;   // +0x120  == base+0x110
        mTotalLinearForce    = KV_ZERO;   // +0xF0   == base+0xE0
        mTotalTorque         = KV_ZERO;   // +0x100  == base+0xF0

        // ---- 0x825D0198..0x825D022C: re-seat the contact/effect state ------------------------
        mbContactingWall = false;                                        // stb 0 -> +0x1362
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w
            = KF_WALL_CONTACT_RESEED;                                    // vrlimi(1) -> +0x1070.w

        // Partial SlamEffect clear -- the same five fields Reset(Vector3) seats (mForce, mfDecay
        // and mfRecoveryTime are NOT written).
        mSlamEffect.mi8SlamNumber      = -1;      // stb -1  -> +0x1128
        mSlamEffect.mfSteering         = 0.0f;    // stfs 0  -> +0x1114
        mSlamEffect.mfOriginalSteering = 0.0f;    // stfs 0  -> +0x1118
        mSlamEffect.mfTotalSlamTime    = 0.0f;    // stfs 0  -> +0x1120
        mSlamEffect.mfSlamLife         = 0.0f;    // stfs 0  -> +0x111C

        // Shunt clear, the Reset(Vector3) shape: direction+speed zeroed whole; Life = -1
        // (vspltisw -1 + vcfsx), SpeedIncreaseToQuit = 0; z/w lanes untouched.
        mShuntEffect.mDirectionPlusDesiredSpeed = Vector3Plus{ 0.0f, 0.0f, 0.0f, 0.0f };
                                                                         // stvx128 0 -> +0x1130
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y = 0.0f;              // vrlimi(4) -> +0x1140.y
        mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = -1.0f;             // vrlimi(8) -> +0x1140.x

        // AS SHIPPED: the image stores +0x1158 TWICE (two consecutive `std r11,0x1158(r3)`,
        // raw bytes 0xF9631158 x2 @0x825D01EC/0x825D01F0). Reset(Vector3) zeroes mUsedAirRams
        // AND mUsedSpins (+0x1220) here, so the second store is plausibly a source-level typo
        // that was meant for mUsedSpins -- but the shipped bytes hit mUsedAirRams both times,
        // so mUsedSpins is NOT cleared by this function. Transcribed as shipped; flagged.
        mUsedAirRams.UnSetAll();                                         // std 0 -> +0x1158
        mUsedAirRams.UnSetAll();                                         // std 0 -> +0x1158 (again)

        // Timer lane re-seats (lane-inserts of 0.0f).
        mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z = 0.0f;
                                                                         // vrlimi(2) -> +0x1060.z
        mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.w = 0.0f;
                                                                         // vrlimi(1) -> +0x1060.w
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.x
            = 0.0f;                                                      // vrlimi(8) -> +0x1070.x
    }

// [clean] UpdateDriving  @0x82638148
    // @0x82638148  BrnPhysics::Vehicle::VehiclePhysics::UpdateDriving  (433 insns)
    // THE ORDERER -- the phase chain the whole campaign has been aimed at. Transcribed
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
    //   0x82638270  UpdateShunt(&copy, dt)                   [BODIED 2026-08-09; v1 = v126 dt]
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
    //   0x8263842C  UpdateInAirBehaviour(&copy, dt)          [BODIED 2026-08-11]
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
    void VehiclePhysics::UpdateDriving(VecFloat lvfTimeStep,
                                       const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                                       const BrnPlayerDriverControls* lpControls,
                                       CgsNumeric::Random& lrRandom)
    {
        static const f32 KF_EPSILON = 1.1920928955078125e-07f;   // stru_8208F620[0]
        (void)lpCameraMatrix;   // r4 is carried for UpdateCrashing's sibling path; this body
                                // does not read it (the console keeps it in r26 untouched).

        CheckState("Before driving update");

        BrnPlayerDriverControls lCopy;
        std::memcpy(&lCopy, lpControls, sizeof(BrnPlayerDriverControls));   // the 0x48 memcpy

        if (lCopy.mbIsSteeringWheel)
            ModifyControlsForSteeringWheelInput(&lCopy);
        ModifyControlsForDrift(&lCopy);

        { const f32 lfMPH = vpu::Dot(mLinearVelocity, mTransform.zAxis) * 2.2369363f;
        mfSpeedMPH = VecFloat{ lfMPH, lfMPH, lfMPH, lfMPH }; }   // splat
        { const f32 lfM = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;
          mfMass = VecFloat{ lfM, lfM, lfM, lfM }; }   // splat (refreshed per frame)

        UpdateInAirStats(lvfTimeStep.x);

        mEngine.SetAllowGearChanges(!mbHasAir);   // stb 0xFC4 / 0xFC5

        UpdateSlam(&lCopy, lvfTimeStep);
        UpdateShunt(&lCopy, lvfTimeStep);   // v1 = v126 (the dt splat), restored before the bl
        CheckState("After update slam");
        CheckState("After LayOffGasWhilstInAir");

        UpdateRoadNoise(lvfTimeStep, lrRandom);
        CheckState("After update road noise");

        CalculateWorldIntertia();

        UpdateSpeedMatch(lpControls, lvfTimeStep);   // the ORIGINAL controls (r28)
        CheckState("After update speed match");

        UpdateDownForce();
        CheckState("After Update down force");

        UpdateBoost(&lCopy, lvfTimeStep);
        CheckState("After update boost");

        UpdateEngine(&lCopy, lvfTimeStep);
        CheckState("After update engine");

        CalculateNewVelocity(lvfTimeStep);

        UpdateSteering(lCopy.mfSteering, lCopy.mfGas, lvfTimeStep, lCopy.mbIsSteeringWheel);
        CheckState("After update steering");

        UpdateDrift((lpControls->GetType() != E_DRIVER_TYPE_PLAYER) ? lpControls : &lCopy,
                    lvfTimeStep);
        CheckState("After update drift");

        UpdateSuspension(lvfTimeStep);   // the +0x2C vcall (see banner)

        mbAllWheelsHaveTraction =
            maWheels[eFrontLeftWheel].GetRoadContact().mbIsOnGround  &&   // +0x158
            maWheels[eFrontRightWheel].GetRoadContact().mbIsOnGround &&   // +0x238
            maWheels[eRearLeftWheel].GetRoadContact().mbIsOnGround   &&   // +0x318
            maWheels[eRearRightWheel].GetRoadContact().mbIsOnGround;      // +0x3F8
        CheckState("After update suspension");

        UpdateWheels(&lCopy, lvfTimeStep);
        CheckState("After update Wheels");

        UpdateInAirBehaviour(&lCopy, lvfTimeStep);
        UpdateInWaterBehaviour(lvfTimeStep);
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
                const VecFloat lvSurface = GetSurfaceLinearDrag();
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

// [clean] UpdateShunt  @0x825FC748
    // @0x825FC748  BrnPhysics::Vehicle::VehiclePhysics::UpdateShunt  (100 insns, read line by
    // line; the LOUD TRAP in VehiclePhysicsLinkStubs.cpp is deleted in this commit).
    //
    // Consume the queued shunt effect: while a shunt is active (the inlined
    // ShuntEffect::IsActive() lane pair -- desired speed w > 0 AND life x > 0), and the car
    // still has traction, close the horizontal-speed deficit along the shunt direction with a
    // single world-space impulse, floor the gas, drop brake + handbrake, and decay the life.
    // The X360, store for store:
    //   0x825FC778..0x825FC7C8  the active pair (vspltw 3 of +0x1130 / vspltw 0 of +0x1140,
    //                           both vcmpgtfp vs 0) -- inactive -> plain return, NO clear
    //   0x825FC7CC  `lbz 0x135B` mbAllWheelsHaveTraction -- zero -> CLEAR the effect
    //   0x825FC7D8..0x825FC818  the deficit: horiz = v - up*dot3(up, v)  (up = mTransform.yAxis
    //                           @this+0x20, v = mLinearVelocity @this+0x50);
    //                           deficit = desiredSpeed - dot3(dir, horiz); if deficit >
    //                           SpeedIncreaseToQuit (+0x1140 lane y) -> CLEAR (quit: the car
    //                           has fallen too far behind the shunt's desired speed)
    //   0x825FC82C..0x825FC84C  impulse = dir * deficit * mfMass (+0xE0, the splat register),
    //                           minus its up-axis component (the same project-out), fed to
    //                           ExternalPhysicsBody::AddWorldSpaceImpulse (r3 = this+0x10)
    //   0x825FC850..0x825FC874  controls: mfBrake(+8) = mfHandBrake(+0xC) = 0.0
    //                           [flt_82001CC0]; mfGas(+4) = fsel-max(mfGas, 0.8
    //                           [flt_8208F9C8 == 0x3F4CCCCD, image-read x360rd])
    //   0x825FC878  `stb 0x1358` mbHandBrake = false
    //   0x825FC87C..0x825FC890  life -= dt (the v127-saved v1 argument; vrlimi mask 8 = x lane)
    //   CLEAR (0x825FC8A4..): mDirectionPlusDesiredSpeed = 0; then +0x1140 .y = 0 (vrlimi 4)
    //                         and .x = -1.0f (vcfsx of vspltisw -1; vrlimi 8) -- the ctor's
    //                         partial-clear pattern, reproduced store for store.
    void VehiclePhysics::UpdateShunt(BrnPlayerDriverControls* lpControls, VecFloat lvfTimeStep)
    {
        static const f32 KF_SHUNT_GAS_FLOOR = 0.8f;   // flt_8208F9C8 (image-read: 0x3F4CCCCD)

        // The inlined ShuntEffect::IsActive() pair. Inactive -> return WITHOUT clearing.
        if (!(mShuntEffect.mDirectionPlusDesiredSpeed.w > 0.0f) ||
            !(mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x > 0.0f))
            return;

        bool lbClear = true;   // both early legs fall into the clear block
        if (mbAllWheelsHaveTraction)
        {
            // horizontal velocity = v - up * dot3(up, v)
            const Vector3& lvUp  = mTransform.yAxis;                    // this+0x20
            const Vector3& lvVel = mLinearVelocity;                     // this+0x50
            const f32 lfUpDot = vpu::Dot(lvUp, lvVel);
            const Vector3 lvHoriz = { lvVel.x - lvUp.x * lfUpDot,
                                      lvVel.y - lvUp.y * lfUpDot,
                                      lvVel.z - lvUp.z * lfUpDot, 0.0f };

            const f32 lfAlong   = vpu::Dot(mShuntEffect.mDirectionPlusDesiredSpeed.GetVector3(),
                                           lvHoriz);
            const f32 lfDeficit = mShuntEffect.mDirectionPlusDesiredSpeed.w - lfAlong;

            if (!(lfDeficit > mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y))
            {
                // live branch: impulse = (dir * deficit * mass) with the up component
                // projected out, then the control/latch writes and the life decay.
                const f32 lfMass = mfMass.x;   // +0xE0, a splat register -- lane-wise multiply
                Vector3 lvImpulse = { mShuntEffect.mDirectionPlusDesiredSpeed.x * lfDeficit * lfMass,
                                      mShuntEffect.mDirectionPlusDesiredSpeed.y * lfDeficit * lfMass,
                                      mShuntEffect.mDirectionPlusDesiredSpeed.z * lfDeficit * lfMass,
                                      0.0f };
                const f32 lfUpComp = vpu::Dot(lvUp, lvImpulse);
                lvImpulse = Vector3{ lvImpulse.x - lvUp.x * lfUpComp,
                                     lvImpulse.y - lvUp.y * lfUpComp,
                                     lvImpulse.z - lvUp.z * lfUpComp, 0.0f };
                AddWorldSpaceImpulse(lvImpulse);

                lpControls->mfBrake     = 0.0f;                          // stfs +8
                lpControls->mfHandBrake = 0.0f;                          // stfs +0xC
                lpControls->mfGas       = (lpControls->mfGas >= KF_SHUNT_GAS_FLOOR)
                                              ? lpControls->mfGas
                                              : KF_SHUNT_GAS_FLOOR;      // fsel-max
                mbHandBrake = false;                                     // stb 0x1358

                mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x -= lvfTimeStep.x;   // vrlimi 8
                lbClear = false;
            }
        }

        if (lbClear)
        {
            // The ctor's partial clear, store for store (NOT a call -- the console inlines it):
            mShuntEffect.mDirectionPlusDesiredSpeed = Vector3Plus{ 0.0f, 0.0f, 0.0f, 0.0f };
            mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y = 0.0f;    // vrlimi mask 4
            mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = -1.0f;   // vrlimi mask 8
        }
    }

// [clean] UpdateCrashing  @0x82638810
    // @0x82638810  BrnPhysics::Vehicle::VehiclePhysics::UpdateCrashing  (732 insns, read line
    // by line; the LOUD TRAP in VehiclePhysicsLinkStubs.cpp is deleted in this commit).
    //
    // THE CRASH-STATE ORCHESTRATOR -- UpdateDriving's twin for a crashing car. Same per-frame
    // phase chain (air stats, air rams, spins, engine, steering, suspension, wheels, velocity
    // passes, the CheckState brackets), plus the crash-specific work: the exponential
    // velocity damping pair, the per-body-axis angular clamp, the synthetic crash mass, the
    // aftertouch dispatch and the down-force leg. Callers: VehiclePhysics::Update @0x826414F8
    // (race cars) and TrafficPhysics::Update @0x82639CA4 (traffic).
    //
    // The three vcalls are IMAGE-SETTLED slots (vtables @0x820D0C68/@0x820D0C98/@0x820D1034
    // read via x360rd): +0x18 = IsCrashingNormally (twice), +0x28 = UpdateAftertouch,
    // +0x10 = IsPlayerVehicleInShowtime. +0x2C (UpdateSuspension) has no override in the
    // image, so the direct call is dispatch-identical (the UpdateDriving precedent).
    //
    // Damping constants, image-read (x360rd) from the .data block @0x82F2A5xx (already valued
    // in the image, no initialiser):
    //     IsCrashingNormally():  linear 0.995  [flt_82F2A530]   angular 0.995  [flt_82F2A52C]
    //     else:                  linear 0.9999 [flt_82F2A528]   angular 0.992  [flt_82F2A524]
    // The exponent scale 60.0 [flt_82092BC4] makes the decay frame-rate-correct against a
    // 60 Hz reference: v *= damp^(60*dt).
    // ⭐⭐ FLAG RETIRED 2026-09-03 (drive-spine 1:1 audit). The pow here is the EARenderWare
    // vlogefp/vexptefp polynomial, and THIS function is where it was finally read end to end
    // (0x826388D4..0x82638AE0) -- see the long note on ExternalPhysicsBody::DampenAngularVelocity
    // for the four coefficient rows (0x82014AC0/AD0/AE0/AF0, all readable .rdata) and the proof
    // that they are a degree-8 log2(1+x) minimax and a degree-8 2^-f series whose reciprocal is
    // taken. With the special-case fixup tail at 0x82638A54..0x82638AD0 (NaN for a negative base
    // with a fractional exponent, signed zero / infinity for a zero base, 1.0 for a zero exponent,
    // sign re-applied for an odd integer exponent) it is a complete IEEE-shaped powf, so std::pow
    // is faithful to within float rounding. Bit-exactness is still not claimed: an
    // estimate+Newton chain and a correctly-rounded CRT differ in the last ulp or two.
    //
    // The angular clamp: omega -> body axes (R^T via the vmrghw/vmrglw transpose), clamp each
    // lane to +/-6.5, back to world (R). The 6.5 splat is unk_82FB9DA0 -- BSS, seeded by the
    // static-init thunk @0x82C5C5B0 from flt_82054378 == 6.5 (image-read), and debug-registered
    // as "Max X/Y/Z Angular Velocity" by VehicleManagerDebugComponent::OnActivate @0x825B61C4.
    //
    // The synthetic crash mass (the non-normal leg): t = clamp01((Mass - 721) / (3250 - 721));
    // mfMass = splat(1400 + t*(2100 - 1400)). All four scalars are BSS splats seeded by the
    // init-thunk bank @0x82C5C410..0x82C5C4A0 from rdata @0x8209D710..1C (721 / 3250 / 1400 /
    // 2100, image-read). The asm stores the clamped t into mfMass and overwrites it with the
    // lerp on the very next store -- the intermediate is dead and not reproduced.
    void VehiclePhysics::UpdateCrashing(f32 lfTimeStep,
                                        const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                                        const BrnPlayerDriverControls* lpControls,
                                        bool lbImpactTime,
                                        bool lbPlayerAftertouchForceAdditive,
                                        bool lbShowtimeAllowed)
    {
        static const f32 KF_CRASH_LIN_DAMP_NORMAL = 0.995f;    // flt_82F2A530 (0x3F7EB852)
        static const f32 KF_CRASH_ANG_DAMP_NORMAL = 0.995f;    // flt_82F2A52C (0x3F7EB852)
        static const f32 KF_CRASH_LIN_DAMP_OTHER  = 0.9999f;   // flt_82F2A528 (0x3F7FF972)
        static const f32 KF_CRASH_ANG_DAMP_OTHER  = 0.992f;    // flt_82F2A524 (0x3F7DF3B6)
        static const f32 KF_DAMP_RATE_SCALE       = 60.0f;     // flt_82092BC4
        static const f32 KF_MAX_CRASH_ANGVEL      = 6.5f;      // unk_82FB9DA0 <- flt_82054378
        static const f32 KF_CRASHMASS_IN_LO       = 721.0f;    // unk_82FB9270 <- 0x8209D710
        static const f32 KF_CRASHMASS_IN_HI       = 3250.0f;   // unk_82FB9DC0 <- 0x8209D714
        static const f32 KF_CRASHMASS_OUT_LO      = 1400.0f;   // unk_82FB9030 <- 0x8209D718
        static const f32 KF_CRASHMASS_OUT_HI      = 2100.0f;   // unk_82FB9EA0 <- 0x8209D71C
        static const f32 KF_WALL_CONTACT_WINDOW   = 0.4f;      // flt_8200473C (image-read)

        (void)lbImpactTime;   // DEAD in the body (never read; the caller's register map only)

        const VecFloat lvfTimeStep{ lfTimeStep, lfTimeStep, lfTimeStep, lfTimeStep };

        CheckState("Start of update crashing");

        BrnPlayerDriverControls lCopy;
        std::memcpy(&lCopy, lpControls, sizeof(BrnPlayerDriverControls));   // the 0x48 memcpy

        CheckState("After update slam");   // the console REUSES the slam bracket string here

        // ----- the crash damping pair (asm 0x82638888..0x82638C5C) -----
        {
            f32 lfLinDamp, lfAngDamp;
            if (IsCrashingNormally())              // vcall +0x18, first dispatch
            {
                lfLinDamp = KF_CRASH_LIN_DAMP_NORMAL;
                lfAngDamp = KF_CRASH_ANG_DAMP_NORMAL;
            }
            else
            {
                lfLinDamp = KF_CRASH_LIN_DAMP_OTHER;
                lfAngDamp = KF_CRASH_ANG_DAMP_OTHER;
            }
            const f32 lfExponent = KF_DAMP_RATE_SCALE * lfTimeStep;
            { const f32 lfF = std::pow(lfLinDamp, lfExponent);
              mLinearVelocity  = vpu::Mult(mLinearVelocity, lfF); }    // stvx this+0x50
            { const f32 lfF = std::pow(lfAngDamp, lfExponent);
              mAngularVelocity = vpu::Mult(mAngularVelocity, lfF); }   // stvx this+0x60
        }

        // ----- clamp omega per BODY axis (asm 0x82638C60..0x82638CE8) -----
        {
            f32 lfLx = vpu::Dot(mTransform.xAxis, mAngularVelocity);   // R^T * omega, the
            f32 lfLy = vpu::Dot(mTransform.yAxis, mAngularVelocity);   // vmrghw/vmrglw transpose
            f32 lfLz = vpu::Dot(mTransform.zAxis, mAngularVelocity);
            if (lfLx < -KF_MAX_CRASH_ANGVEL) lfLx = -KF_MAX_CRASH_ANGVEL;   // vmaxfp -limit
            if (lfLx >  KF_MAX_CRASH_ANGVEL) lfLx =  KF_MAX_CRASH_ANGVEL;   // vminfp +limit
            if (lfLy < -KF_MAX_CRASH_ANGVEL) lfLy = -KF_MAX_CRASH_ANGVEL;
            if (lfLy >  KF_MAX_CRASH_ANGVEL) lfLy =  KF_MAX_CRASH_ANGVEL;
            if (lfLz < -KF_MAX_CRASH_ANGVEL) lfLz = -KF_MAX_CRASH_ANGVEL;
            if (lfLz >  KF_MAX_CRASH_ANGVEL) lfLz =  KF_MAX_CRASH_ANGVEL;
            mAngularVelocity = Vector3{
                mTransform.xAxis.x * lfLx + mTransform.yAxis.x * lfLy + mTransform.zAxis.x * lfLz,
                mTransform.xAxis.y * lfLx + mTransform.yAxis.y * lfLy + mTransform.zAxis.y * lfLz,
                mTransform.xAxis.z * lfLx + mTransform.yAxis.z * lfLy + mTransform.zAxis.z * lfLz,
                0.0f };
        }

        // ----- the mass regime (asm 0x82638CF4..0x82638DA0) -----
        if (IsCrashingNormally())                  // vcall +0x18, second dispatch
        {
            const f32 lfM = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;
            mfMass = VecFloat{ lfM, lfM, lfM, lfM };                    // splat, this+0xE0
        }
        else
        {
            const f32 lfM = mpAttribs->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;
            f32 lfT = (lfM - KF_CRASHMASS_IN_LO) / (KF_CRASHMASS_IN_HI - KF_CRASHMASS_IN_LO);
            if (lfT < 0.0f) lfT = 0.0f;                                 // vmaxfp 0
            if (lfT > 1.0f) lfT = 1.0f;                                 // vminfp 1
            const f32 lfMass = lfT * (KF_CRASHMASS_OUT_HI - KF_CRASHMASS_OUT_LO)
                               + KF_CRASHMASS_OUT_LO;                   // vmaddfp lerp
            mfMass = VecFloat{ lfMass, lfMass, lfMass, lfMass };
        }

        mLastLinearVelocity = mLinearVelocity;     // stvx this+0x13B0 (before UpdateInAirStats)

        UpdateInAirStats(lfTimeStep);

        mEngine.SetAllowGearChanges(!mbHasAir);    // stb 0xFC4 / 0xFC5

        lCopy.mfGas *= 0.5f;                       // flt_82001DA0 -- the crash gas halving

        CalculateWorldIntertia();

        UpdateAirRam(lvfTimeStep);
        CheckState("After update air ram");

        UpdateSpinEffects(lvfTimeStep);

        if (lbPlayerAftertouchForceAdditive)
        {
            // vcall +0x28 -- RaceCarPhysics::UpdateAftertouch on race cars, the empty default
            // on traffic. Register map @0x82638E3C: r4 = &copy, r5 = camera, r6 = additive,
            // r7 = showtime, v1 = dt.
            UpdateAftertouch(&lCopy, lpCameraMatrix, lvfTimeStep,
                             lbPlayerAftertouchForceAdditive, lbShowtimeAllowed);
            CheckState("After aftertouch");
        }

        if (IsCrashingNormally())                  // vcall +0x18, third dispatch
        {
            // down force, applied along body -Y at the body origin (asm 0x82638E9C..0x82638F00:
            // force = {0, -GetDownForce().x, 0}, position = {0,0,0}, both tags `li 1` ==
            // BODY_SPACE).
            const VecFloat lvDown = GetDownForce();
            AddLocalForce(Vector3{ 0.0f, -lvDown.x, 0.0f, 0.0f }, rw::physics::BODY_SPACE,
                          Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, rw::physics::BODY_SPACE);
        }
        CheckState("After Update down force");

        UpdateEngine(&lCopy, lvfTimeStep);
        CheckState("After update engine");

        CalculateNewVelocity(lvfTimeStep);

        UpdateSteering(lCopy.mfSteering, lCopy.mfGas, lvfTimeStep, lCopy.mbIsSteeringWheel);

        UpdateSuspension(lvfTimeStep);             // vcall +0x2C; no override in the image ->
                                                   // direct call is dispatch-identical

        // mbAllWheelsHaveTraction: the four road-contact bytes AND the +0x10 vcall
        // (IsPlayerVehicleInShowtime -- image-settled; traffic defaults false, so a crashing
        // traffic car never reports traction here). Short-circuit order preserved.
        mbAllWheelsHaveTraction =
            maWheels[0].GetRoadContact().mbIsOnGround &&    // lbz +0x158
            maWheels[1].GetRoadContact().mbIsOnGround &&    // lbz +0x238
            maWheels[2].GetRoadContact().mbIsOnGround &&    // lbz +0x318
            maWheels[3].GetRoadContact().mbIsOnGround &&    // lbz +0x3F8
            IsPlayerVehicleInShowtime();                    // vcall +0x10

        UpdateWheels(lpControls, lvfTimeStep);     // ⭐ the ORIGINAL pointer (r22), NOT the
                                                   // copy -- UpdateDriving passes the copy;
                                                   // this function does not. As shipped.
        SimpleVehiclePhysics::CalculateNewWheelPlane();
        CheckState("After update wheels");

        CalculateNewVelocity(lvfTimeStep);

        // ----- per-wheel body-point velocities (asm 0x82639030..0x826391CC, unrolled x4) -----
        // maWheels[i].mBodyPointVelocity = v + omega x (R * streamedLocalPos). The console
        // computes the cross with the vpermwi128-0x63 (yzx) idiom; wheel base +0x130, stride
        // 0xE0: inputs +0x1C0/+0x2A0/+0x380/+0x460, outputs +0x1D0/+0x2B0/+0x390/+0x470.
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            const Vector3Plus& lrLocal = maWheels[liWheel].mStreamedPositionPlusTwistAmount;
            const Vector3 lvR = {
                mTransform.xAxis.x * lrLocal.x + mTransform.yAxis.x * lrLocal.y + mTransform.zAxis.x * lrLocal.z,
                mTransform.xAxis.y * lrLocal.x + mTransform.yAxis.y * lrLocal.y + mTransform.zAxis.y * lrLocal.z,
                mTransform.xAxis.z * lrLocal.x + mTransform.yAxis.z * lrLocal.y + mTransform.zAxis.z * lrLocal.z,
                0.0f };
            const Vector3 lvCross = vpu::Cross(mAngularVelocity, lvR);
            maWheels[liWheel].mBodyPointVelocity = Vector3{ mLinearVelocity.x + lvCross.x,
                                                            mLinearVelocity.y + lvCross.y,
                                                            mLinearVelocity.z + lvCross.z, 0.0f };
        }

        { const f32 lfMPH = vpu::Dot(mLinearVelocity, mTransform.zAxis) * 2.2369363f;
        mfSpeedMPH = VecFloat{ lfMPH, lfMPH, lfMPH, lfMPH }; }   // splat, KF_MPS_TO_MPH
        mbCrashedThisFrame = false;                              // stb 0 -> +0x713

        // ----- the start-line velocity re-seat (asm 0x826391E8..0x82639250) -----
        // Gated on the ORIGINAL controls' mbIsOnStartLine (+0x40, r22) and the above-ground
        // test being valid. Unfreezes and keeps only the velocity component along the ground
        // normal (the asm negates BOTH factors -- (-n) * dot3(v, -n) == n * dot3(v, n)).
        if (lpControls->mbIsOnStartLine && mAboveGroundTestResult.mbValid)
        {
            mbFrozen = false;                                    // stb 0 -> +0x70
            const Vector3& lvN = mAboveGroundTestResult.mIntersectionNormal;   // +0x580
            const f32 lfAlong = vpu::Dot(mLinearVelocity, lvN);
            mLinearVelocity = Vector3{ lvN.x * lfAlong, lvN.y * lfAlong,
                                       lvN.z * lfAlong, 0.0f };
        }

        // ----- the wall-contact window (asm 0x82639254..0x826392D8; unconditional here,
        //       unlike UpdateDriving's else-of-mbReset placement) -----
        mbContactingWall =
            KF_WALL_CONTACT_WINDOW >
            mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w;
        mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w
            += lfTimeStep;                                       // vrlimi mask 1 = w lane

        // ----- the water hard-kill (asm 0x826392DC..0x82639334) -----
        // The console INLINES UpdateInWaterBehaviour's exact gate + six zero stores here
        // (same surface-table test, same 2.0 depth constant, same store order +0x50/+0x60/
        // +0xF0/+0x110/+0x120/+0x100); the committed function is behaviour-identical, so the
        // call replaces the inline.
        UpdateInWaterBehaviour(lvfTimeStep);

        // TimeCrashing += dt (asm 0x82639338..0x82639358; vrlimi mask 4 = y lane). Note
        // UpdateDriving ZEROES this lane each frame -- the two bodies are each other's
        // complement on it.
        mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y += lfTimeStep;

        // No trailing CheckState -- the console body ends here.
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
    //       Drift(steer)| > |currentRad| AND turning into the slide (sideSpeed*STEER < 0 --
    //       the steer input, not 30: see D1 at the gate):
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
        // ⛔ D1 (drift-symmetry wave 2026-09-02, asm 0x825D38D4..0x825D395C): the third operand
        // of this gate is `sideSpeed * lfSteering` -- `stfs f31, var_130` at 0x825D38D4 OVERWRITES
        // the 30.0 (flt_82004F5C) that the same stack slot held at 0x825D387C, and it is the
        // steering splat (`lvx128 v11, var_130 ; vspltw v11,v11,0 ; vmulfp128 v11, v0(sideSpeed),
        // v11`) that feeds `vcmpgtfp128. v12, v119(0), v11`. The old `* 30.0f` was a stack-slot
        // reuse misread: it made the gate `sideSpeed < 0` -- one-sided -- so only one drift
        // direction could grow the steering lock toward the drift max. The console's product with
        // the steer input is symmetric: "turning INTO the slide".
        if (lbRearSliding &&
            std::fabs(lfDriftMaxRad) > std::fabs(lfCurrentRad) &&
            (0.0f > lfSideSpeed * lfSteering))
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
    // THE PER-CAR CONDUCTOR. The DWARF declares it virtual at vtable slot +0xC -- the slot
    // VehicleManager::UpdateVehiclePhysics dispatches through. Breaker confirms v1/v2 carry
    // sim/game time and r4..r9 carry the remaining arguments in DecFIGS declaration order.
    // The PerfMon ids are the seven hoisted
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
    //   0x82641470  UpdateHandBrake(dt, lpControls->mfHandBrake)
    //   0x82641474  [mbFrozen] the ENGINE-ONLY leg:
    //                 UpdateEngine(lpControls, dt); CheckState("After update engine");
    //                 [mbCrashing] TimeCrashing (+0xEF0.y) += dt  ELSE  = 0
    //               [else, mbCrashing] StartMonitor(gs_iVPhysUpdateCrashingPM);
    //                 UpdateCrashing(dt, camera, controls, impact, aftertouchAdd, showtime)
    //               [else] the DRIVING leg:
    //                 UpdateAirRam(dt)      [gs_iVPhysUpdateAirRamsPM]
    //                 UpdateSpinEffects(dt) [gs_iVPhysUpdateSpinPM]
    //                 UpdateDriving(dt, camera, controls, random) [gs_iVPhysUpdateDrivingPM]
    //               then (both non-frozen legs) UpdateLinearVelocityMagnitude()
    //               [gs_iVPhysUpdateLVPM]
    //   0x826415A0  miNumCollisions (+0x1354) = 0
    //   0x826415AC  TimeSinceLastRaceCarContact (+0x1050.z) = min(z + dt, 100.0)
    //               (unk_82FB9BE0, static-init @0x82C5C540 from flt_820049E0 == 100.0)
    //   0x826415D0  StopMonitor(gs_iVPhysUpdatePM)
    //
    // RE-VERIFIED 2026-08-11 (orchestrator re-audit wave) against the ARTIST export
    // 0x826412C0.json -- asm and xrefs_from, not pseudocode. Result: FAITHFUL.
    //   * CALLEE SET 16/16 EXACT vs xrefs_from -- StartMonitor, VehiclePhysics::SwitchAttribs,
    //     memcpy, SwitchAIDonuttingAttribs, SimpleVehiclePhysics::SwitchAttribs, SetupSuspension,
    //     StopMonitor, UpdateFreezing, UpdateHandBrake, UpdateEngine, CheckState, UpdateCrashing,
    //     UpdateAirRam, UpdateSpinEffects, UpdateDriving, UpdateLinearVelocityMagnitude. No
    //     absent callee, nothing extra, nothing inlined that the console calls out-of-line.
    //   * CALL ORDER EXACT against the 29 `bl` sites 0x826412C4..0x826415D0.
    //   * ARG MAP EXACT off the prologue: v1/v2 are sim/game time; r4=r26 camera,
    //     r5=r29 controls, r6=r23, r7=r22, r8=r21, and r9=r25 Random. This is the
    //     canonical DecFIGS declaration order.
    //   * PPC FLOAT-ABI CHECK at the UpdateCrashing call (0x826414EC..0x82641508): r4 is
    //     deliberately UNSET -- f1 (reloaded from the arg_20 spill of v127, i.e. dt.x) owns that
    //     slot. The committed `UpdateCrashing(lvfTimeStep.x, camera, controls, ...)` is right;
    //     an author who read Hex-Rays' numbering here would have shifted every later arg by one.
    //   * PerfMon balance: the crashing leg's StopMonitor is TAIL-MERGED with the driving leg's
    //     (0x8264150C `lwz r3, dword_82F2A280` then `b loc_8264157C`). It is NOT an unbalanced
    //     StartMonitor; the explicit StopMonitor written below is correct.
    //
    // TWO DIVERGENCES, both flagged as benign -- do not "fix" without re-reading this:
    //   1. START-LINE PROJECTION W LANE. The console negates and multiplies all FOUR lanes, so
    //      it commits mLinearVelocity.w = -mIntersectionNormal.w * dot. This body builds
    //      lvNegNormal with w = 0, so it commits w = 0. Deliberate: rw/math/vpu/types.h types
    //      Vector3's w as "unused 4th lane", and every other body in this tree honours that
    //      convention; reproducing the console's garbage-w would be the divergence.
    //   2. The prose above says "while (lpControls->mbIsOnStartLine && ...)". It is an `if` --
    //      0x826413F8/0x82641404 are forward `beq`s with no back-edge. Comment-only slip; the
    //      code below is the `if`.
    void VehiclePhysics::Update(VecFloat lvfSimTimeStep, VecFloat lvfRealTimeStep,
                                const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                                const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                                bool lbPlayerAftertouchForceAdditive, bool lbShowtimeAllowed,
                                CgsNumeric::Random& lrRandom)
    {
        // 0x826412F8 saves v1 as the simulation timestep. v2 (the real/game timestep) is not read
        // by this body, but remains part of the virtual ABI.
        const VecFloat lvfTimeStep = lvfSimTimeStep;
        const f32 lfDT = lvfTimeStep.x;
        (void)lvfRealTimeStep;


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
            SetupSuspension();
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

        UpdateHandBrake(lvfTimeStep, lpControls->mfHandBrake);

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

                // [crash-probe] witness. NOT X360. Inert unless BRN_CRASH_PLAYER is set. Proves
                // UpdateCrashing @0x82638810 is EXECUTING and that the TimeCrashing lane rises.
                // ⚠ READ **AFTER** THE CALL, DELIBERATELY: the lane's `+= dt` lives in the OTHER
                // arm of this if/else (the non-crashing branch at the top of this block), so a
                // pre-call read reports the previous frame's value and shows 0.0 forever on the
                // first sample. That is exactly the class of diagnostic that lies.
                // Dense early (first 5 frames) then every 60, so both the RISE and the DURATION
                // are visible rather than inferred.
                {
                    static const char* const kspW = getenv("BRN_CRASH_PLAYER");
                    if (kspW != 0)
                    {
                        static u32 sluW = 0;
                        ++sluW;
                        if ((sluW <= 5u || (sluW % 60u) == 0u) && CgsDev::Log::gpDebugPrint != 0)
                        {
                            *CgsDev::Log::gpDebugPrint
                                // Identity is the body's POSITION, not a pointer: it can be
                                // cross-checked against the player position that
                                // [world->director] publish and [T4-player] print, so this
                                // cannot silently report a different car.
                                << "[crash-probe] UpdateCrashing @0x82638810 ran: at ("
                                << mTransform.wAxis.x << ", " << mTransform.wAxis.z << ")"
                                << " mbCrashing=" << (mbCrashing ? 1 : 0)
                                << " mfTimeCrashing="
                                << mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y
                                << " frame=" << sluW << "\n";
                        }
                    }
                }
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
                UpdateDriving(lvfTimeStep, lpCameraMatrix, lpControls, lrRandom);
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
