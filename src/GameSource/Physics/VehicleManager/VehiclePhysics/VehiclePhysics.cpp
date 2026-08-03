#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"  // BrnPlayerDriverControls (C07 boost/speed-match)
#include "GameShared/GameClasses/Numeric/CgsRandom.h" // CgsNumeric::Random (the shared LCG ring)
#include "rw/math/vpu/vector3_operation.h"            // rw::math::vpu::{MagnitudeSquared, Normalize, Dot, operator*}
#include "rw/math/vpu/matrix44affine_operation.h"     // rw::math::vpu::{InverseOfMatrixWithOrthonormal3x3, operator*}

#include <stdint.h>   // uint32_t / uint64_t for the road-noise LCG draw
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
    //   asm: |v|^2 via vmsum3fp128 of mLinearVelocity(+0x50); coeff = mpAttribs(+0x720)->mvAeroParams
    //        (+0xB0) lane .w (vspltw v11,v11,3); 0.5 from vcfsx(1, scale 1); rho/CdA lazily cached
    //        (g_AeroConstInitMask) into g_vAero_Rho / g_vAero_CdA from the .rdata seeds
    //        kAero_Rho_Scalar / kAero_CdA_Scalar. The product order in the asm is
    //        ((0.5*CdA)*|v|^2)*coeff*rho; multiplication is commutative so the grouping is free.
    //
    //   FLAG (rodata): the rho/CdA seed scalars are un-homed .rdata absent from the function
    //   exports -- carried as honest flagged-0 placeholders (faithful-but-inert). The formula, the
    //   v^2 dependence and the attrib-coeff lane are EXACT; the numeric output stays 0 until the
    //   seeds are recovered from the XEX .rdata. NEVER fabricated. (The X360 also caches an unrelated
    //   third aero constant flt_8200D57C -> unk_82FBA0B0 for a SIBLING aero function; it does not
    //   enter this product and is not modelled here.)
    Vector3 VehiclePhysics::GetDownForce() const
    {
        static const f32 KF_AERO_RHO = 0.0f;   // FLAG: un-homed kAero_Rho_Scalar (.rdata) -> g_vAero_Rho
        static const f32 KF_AERO_CDA = 0.0f;   // FLAG: un-homed kAero_CdA_Scalar (.rdata) -> g_vAero_CdA
        static const f32 KF_HALF     = 0.5f;   // vcfsx v0=1, scale 1 -> 0.5

        const f32 lfSpeedSquared = vpu::MagnitudeSquared(mLinearVelocity);
        const f32 lfCoeff        = mpAttribs->mvAeroParams.w;   // .w lane of the aero params register

        const f32 lfDownForce = KF_HALF * KF_AERO_RHO * KF_AERO_CDA * lfSpeedSquared * lfCoeff;

        return Vector3{ lfDownForce, lfDownForce, lfDownForce, lfDownForce };
    }

    // ---------------------------------------------------------------------------------------
    // Surface-response group: GetSurfaceGrip / GetSurfaceRoughness / GetSurfaceLinearDrag
    //   @0x825D51B8 / @0x825D5328 / @0x825D50A8. Each derives a 6-bit surface id from a
    //   RoadContact CollisionTag and looks up a global per-surface property table, blended with a
    //   lane of mpAttribs->mvSurfaceBlend.
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
        const f32 lfBlend = (leWheel < eRearLeftWheel) ? mpAttribs->mvSurfaceBlend.x
                                                       : mpAttribs->mvSurfaceBlend.y;
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
        static const f32 KF_GLOBAL_ROUGHNESS_SCALE = 0.0f;                   // FLAG: un-homed unk_82FB9220

        const f32 lfResult = lfRoughness * KF_GLOBAL_ROUGHNESS_SCALE * mpAttribs->mvSurfaceBlend.z;
        return Vector3{ lfResult, lfResult, lfResult, lfResult };
    }

    // @0x825D50A8  GetSurfaceLinearDrag = dragTable[id] * blend.w  (single representative contact).
    Vector3 VehiclePhysics::GetSurfaceLinearDrag() const
    {
        // +0x596 is the BE-LOW halfword of mAboveGroundTestResult.mCollisionTag; taken by shift
        // (never by byte offset -- the halves swap position on a little-endian host).
        const s32 liSurfaceId = static_cast<s32>((GetAboveGroundTagLo() >> 4) & 0x3Fu);
        const f32 lfDrag = SurfacePropertyPlaceholder(liSurfaceId);   // unk_82FB8BD0[id]

        const f32 lfResult = lfDrag * mpAttribs->mvSurfaceBlend.w;
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
    //     lfFactor = mpAttribs->mvBaseParams.z                          (per-vehicle road-noise scale)
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

        const f32 lfFactor = mpAttribs->mvBaseParams.z;   // per-vehicle road-noise scale (.z lane)
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
    void VehiclePhysics::HandleWheelPairFriction(EVehicleDrivenWheel /*leWheelA*/,
                                                 EVehicleDrivenWheel /*leWheelB*/)
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
    void VehiclePhysics::HandleWheelFrictionCrashing(EVehicleDrivenWheel /*leWheel*/)
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
    void VehiclePhysics::CalculateBodyVelocityAtWheelContact(EVehicleDrivenWheel leWheel)
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

        static const f32 KF_WATER_DROWN_DEPTH = 0.0f;   // flt_82F2A4E4 (un-homed) -- flagged-inert
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
        static const f32 KF_AIR_RAM_ALIVE_EPSILON_SQ = 0.0f;   // flt_82F2A430 (un-homed) -- flagged-inert

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
    //   MAGNITUDE: mpAttribs->mvBaseParams.x (lane0 @ mpAttribs+0x70) * lfFactor * 50.0;
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
        const f32 lfMagnitude = mpAttribs->mvBaseParams.x * lfFactor * KF_AIR_RAM_SCALE;

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
    // (+0x720) supplies Mass (mvBaseParams.x @+0x70) + the BoostAttribs lanes (+0x290..+0x2B0). The
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

            const f32 lfMass  = mpAttribs->mvBaseParams.x;   // mvMass_..._.x lane (+0x70 lane0)
            const f32 lfAccel =
                lrBA.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.x;
            const f32 lfHeightOffset =
                lrBA.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.w;

            // Force = forward * (Mass * accel) (asm: vmulfp128 v1, v12(=mTransform.zAxis), scalar).
            const Vector3 lvForce = mTransform.zAxis * (lfMass * lfAccel);

            // Local application point: (0, heightOffset, -mHalfExtent.z) -- behind the centre of mass.
            // FLAG (rodata): flt_82001CC0 is the un-homed zero-seed for the unused lane(s); honest 0.
            static const f32 KF_BOOST_POS_SEED = 0.0f;   // FLAG: un-homed flt_82001CC0
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
        static const f32 KF_KICK_TIMER_SEED    = 0.0f;           // FLAG: un-homed flt_82001CC0 (zero-seed)

        Vector4& lrBoost = mvSideForceMag_TimeBoosting_TimeSinceLastBoostKick_CurrentBoostKickTime;
        lrBoost.z = 0.0f;   // TimeSinceLastBoostKick = 0 (kick resets the cooldown; vrlimi128 lane z)

        const VehicleAttribs::BoostAttribs& lrBA = mpAttribs->mBoostAttribs;

        const f32 lfMass         = mpAttribs->mvBaseParams.x;                            // Mass
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
    static const f32  KF_DRIFT_STEER_EPSILON       = 0.0f;   // FLAG: flt_8208F620 (speed/guard epsilon splat)
    static const f32  KF_STEER_ANGLE_CLAMP         = 0.0f;   // FLAG: unk_82FB9020 (steering-angle clamp)
    static const f32  KF_WHEEL_STEER_BLEND         = 0.0f;   // FLAG: unk_82FB9370 (wheel-device steer blend)
    static const f32  KF_DRIFT_SLIP_TIME_GAIN      = 0.0f;   // FLAG: flt_830180B0 (drift slip-time gain)
    static const f32  KF_DRIFT_SLIP_EXIT_LIMIT     = 0.0f;   // FLAG: unk_82FB8AC0 (slip-too-small exit limit)
    static const f32  KF_DRIFT_SPEED_EXIT_LIMIT    = 0.0f;   // FLAG: unk_82FB9ED0 (speed-too-low exit limit)
    static const f32  KF_DRIFT_SCALE_GROW_LIMIT    = 0.0f;   // FLAG: unk_82FB80F0 (drift-scale grow clamp)
    static const f32  KF_HANDBRAKE_TIME_CAP        = 0.0f;   // FLAG: unk_82FB9080 (handbrake timer cap)
    static const f32  KF_HANDBRAKE_ONTIME_RELEASE  = 0.0f;   // FLAG: unk_82FB8B00 (handbrake on-time release thresh)
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
        // clamped. FLAG: the clamp magnitude unk_82FB9020 is un-homed (inert here).
        const f32 lfDriftScale = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w;
        f32 lfBlended = lfAngle + (0.0f - lfAngle) * lfDriftScale;   // shrink authority as drift scale grows

        // final clamp to +-KF_STEER_ANGLE_CLAMP (flagged-inert -> leaves lfBlended unclamped when 0).
        if (KF_STEER_ANGLE_CLAMP > 0.0f)
        {
            if (lfBlended >  KF_STEER_ANGLE_CLAMP) lfBlended =  KF_STEER_ANGLE_CLAMP;
            if (lfBlended < -KF_STEER_ANGLE_CLAMP) lfBlended = -KF_STEER_ANGLE_CLAMP;
        }
        return lfBlended;
    }

    // @0x825D34D8  VehiclePhysics::GetMaxSteeringAngleDuringDrift
    //   Builds a steering direction from the quartic-stiffened steer input, takes acos against the
    //   velocity direction, then scales by the per-car max steering angle (mpAttribs->mvSteeringParams @+0xF0 .x,
    //   in DEGREES) * deg->rad. Returns the capped wheel angle (radians).
    //   asm: dir = normalize(mLinearVelocity); stiff = -1 - sign(s)*(s); angle = acos(dot(dir, steerDir))
    //        result = angle... * (mpAttribs->mvSteeringParams.x * 0.017453292).
    f32 VehiclePhysics::GetMaxSteeringAngleDuringDrift(f32 lfSteeringInput) const
    {
        const Vector3 lUnitVel = vpu::Normalize(mLinearVelocity);   // zero-guarded

        // the steer-direction lane built from the (already stiffened) input; clamp dot to [0,1] then acos.
        f32 lfDot = vpu::Dot(lUnitVel, mTransform.zAxis);           // forward axis
        if (lfDot < 0.0f) lfDot = 0.0f;
        if (lfDot > 1.0f) lfDot = 1.0f;
        const f32 lfAngle = std::acos(lfDot);                      // XMVectorACos

        // per-car max angle (degrees) -> radians cap.
        const f32 lfMaxDeg = mpAttribs->mvSteeringParams.x;                    // mpAttribs->mvSteeringParams (+0xF0) .x
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
            // FLAG: unk_82FB9370 weight is un-homed (inert). The blend reads mTransform.zAxis (forward)
            // and the cached steering register; with the weight 0 the direction is unchanged.
            (void)KF_WHEEL_STEER_BLEND;
        }
    }

    // @0x825CFC68  VehiclePhysics::ModifyControlsForDrift
    //   While sliding (mu8DriftState != 0) and the ORIGINAL controls mode is NOT 1 (0x825CFC74-7C:
    //   `lwz r10,0x44(r4) ; cmpwi cr6,r10,1 ; beqlr cr6` -- returns when mode==1, i.e. proceeds only
    //   when mode != 1), re-signs and re-maps the steer input by the drift direction. The blend
    //   weights come from mpAttribs->mvDriftParams1 (@+0x120) lanes .z (zLane) and .w (wLane).
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

        // the two drift remap weights (mpAttribs->mvDriftParams1 @+0x120 .z and .w).
        const f32 lfWeightZ = mpAttribs->mvDriftParams1.z;
        const f32 lfWeightW = mpAttribs->mvDriftParams1.w;

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
        // the per-car drift register lanes (mpAttribs->mvDriftParams1 @+0x120 .w) seed the drift control scale.
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
            if (KF_HANDBRAKE_TIME_CAP > 0.0f && lfOnTime > KF_HANDBRAKE_TIME_CAP)
                lfOnTime = KF_HANDBRAKE_TIME_CAP;
            lrTimers.z = lfOnTime;

            if (lfHandBrakeInput < 0.1f)
            {
                const bool lbReleaseByDrift  = (mu8DriftState != eDriftState_None);
                // KF_HANDBRAKE_ONTIME_RELEASE is a flagged-0 placeholder (unk_82FB8B00, un-homed).
                // With a real nonzero threshold `onTime > threshold` is normally false; naively
                // comparing against 0 makes it true on nearly every frame (onTime only grows), the
                // opposite of "faithful-but-inert" -- guard the comparison like this TU's other
                // flagged-0 constants so the release path stays disabled until the value is recovered.
                const bool lbReleaseByOnTime = (KF_HANDBRAKE_ONTIME_RELEASE != 0.0f)
                                             && (lfOnTime > KF_HANDBRAKE_ONTIME_RELEASE);
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
                if (KF_HANDBRAKE_TIME_CAP > 0.0f && lfSince > KF_HANDBRAKE_TIME_CAP)
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
    //   drift push-time attrib (mpAttribs->mvDriftParams5 @+0x160 .y > DriftPushTime lane). The torque magnitude
    //   reads mpAttribs->mvDriftParams4 (@+0x150) lanes, and the SIGN is selected by mu8DriftState through a
    //   permute table (unk_8327F240). Applied via AddWorldSpaceTorque(this+0x10).
    void VehiclePhysics::ApplyNaturalDriftForces()
    {
        // gate: DriftPushTime (mvLatDriftForceFactor..._.y) vs the per-car push-time threshold
        // (mpAttribs->mvDriftParams5 @+0x160 .y). The asm: `vandc(DriftPushTime) ; vcmpgtfp. vs attrib.y`.
        const f32 lfPushTime    = mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.y;
        const f32 lfPushThresh  = mpAttribs->mvDriftParams5.y;
        if (lfPushTime > lfPushThresh)
        {
            // within the active-push window the asm grows/decays the DriftScale lane toward a target
            // (mpAttribs->mvDriftParams4 @+0x150 .z), signed by whether DriftScale exceeds it. FLAG: the gain is the
            // attrib lane; the math is exact, the numeric target comes from the (homed) attrib register.
            const f32 lfTarget = mpAttribs->mvDriftParams4.z;
            f32 lfScale = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w;   // DriftScale lane
            if (lfScale > lfTarget) lfScale = lfScale - (lfScale - lfTarget);  // decay toward target
            else                    lfScale = lfScale + (lfTarget - lfScale);  // grow toward target
            mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = lfScale;
        }

        // the self-aligning yaw torque: magnitude = mpAttribs->mvDriftParams4 (@+0x150) .x, signed by the drift
        // state (FacingLeft -> +, FacingRight -> - via the unk_8327F240 select), applied about the body
        // up axis (mTransform.yAxis). FLAG: the asm routes the magnitude through a packed select whose
        // sign mask is exact (state-driven); the magnitude lane is the homed attrib.
        const f32 lfMag  = mpAttribs->mvDriftParams4.x;
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
                // push-time attrib (mpAttribs->mvDriftParams5 @+0x160 .y). The deficit = MaintainedSpeed - speed.
                const f32 lfDeficit  = lfMaintained - lfSpeed;
                const f32 lfAlongZ   = mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.x;
                const f32 lfAlongVel = mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.y;
                const f32 lfPush     = mpAttribs->mvDriftParams5.y;

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

    // @0x8261F728  VehiclePhysics::UpdateDriftState
    //   The drift state machine. Runs CheckForEnteringDrift (owned elsewhere -- declare-only), then a
    //   battery of ExitDrift guards while drifting (mu8DriftState != 0 AND controls.mbHorn (+0x3E) is
    //   NOT set -- 0x8261F74C reads r31=controls, not the mode byte). The guards (in asm order):
    //     1. drift slip too small        (drift attrib slip lane > current slip -> exit)
    //     2. airborne too long           (mbHandBrake (this+0x1358) && TimeWithoutTraction over a cap
    //                                    -> exit; NOT gated on mbAllWheelsHaveTraction==false)
    //     3. wheel adhesive-limit fold   (all 4 wheels' mbBrokenAdhesiveLimit @wheel+0xD5 -> a vsel
    //                                    CurrentDriftAngle accumulate; NOT a RoadContact.mbIsOnGround
    //                                    test and NOT a direct exit branch)
    //     4. slip-ratio below threshold  (steering^4 vs a slip lane -> exit)
    //     5. exit timers                 (mi8NumWorldCollisions > 0, miNumCollisions > 0 -> exit)
    //     6. attribs limit               (per-car drift slip limit * slip-time-gain vs seed slip -> exit)
    //     7. desired-slip cap            (DesiredDriftSlip lane > 1.0 -> exit)
    //     8. above-ground invalid        (!mAboveGroundTestResult.mbValid -> exit)
    //     9. speed too low               (current speed below KF_DRIFT_SPEED_EXIT_LIMIT -> exit)
    //    10. steering crossed centre     (for the latched direction, slip & steer both back across 0 -> exit)
    //   FLAG: the comparison limits (4,6,9) are un-homed rodata carried as flagged-inert placeholders;
    //   the guard structure + the named lanes/flags are exact.
    void VehiclePhysics::UpdateDriftState(const BrnPlayerDriverControls* lpControls, f32 lfSlipAngle,
                                          f32 lfSpeed, f32 lfSteeringDir)
    {
        // CheckForEnteringDrift may latch a NEW drift this frame (owned by another TU -- declare-only).
        CheckForEnteringDrift(lpControls, lfSlipAngle, lfSpeed, lfSpeed, lfSteeringDir);

        // only run the exit battery while drifting and not being HELD in drift. 0x8261F74C:
        // `lbz r11,0x3E(r31)` where r31 == controls. ⭐ RE-NAMED 2026-08-03: with the corrected
        // controls layout +0x3E is **mbForceDrift**, not mbHorn (the old layout had mbHorn there).
        // That is also the better fit for the gate: a car being force-drifted skips the exit battery.
        const bool lbHeldInDrift = lpControls ? lpControls->mbForceDrift : false;
        if (mu8DriftState == eDriftState_None || lbHeldInDrift)
            return;

        // 1. slip too small: CappedDriftScale (@+0x1020 .y) vs a drift-slip exit limit (flagged-inert).
        if (mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.y > KF_DRIFT_SLIP_EXIT_LIMIT
            && KF_DRIFT_SLIP_EXIT_LIMIT > 0.0f)
        { ExitDrift(); return; }

        // 2. airborne too long: gated on mbHandBrake (this+0x1358), NOT mbAllWheelsHaveTraction==false
        // (0x8261F78C: `lbz r9,0x1358(r3)`). TimeWithoutTraction (@+0x1060 .w) vs a cap.
        if (mbHandBrake)
        {
            if (mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.w > 1.0f)
            { ExitDrift(); return; }
        }

        // 3. NOT an on-ground test: 0x8261F7C0-E4 reads each wheel's mbBrokenAdhesiveLimit (wheel+0xD5,
        // stride 0xE0 -- Wheel.h's pinned member), not RoadContact.mbIsOnGround (wheel+0x28). The
        // "all four wheels still have adhesive grip" result feeds a vsel-based CurrentDriftAngle
        // accumulate (unk_8327F240 permute table), not a direct ExitDrift branch -- structural, not a
        // simple exit condition; the accumulate itself is not store-faithfully recoverable here.
        const bool lbAllWheelsHaveAdhesive = !maWheels[eFrontLeftWheel].mbBrokenAdhesiveLimit
                                           && !maWheels[eFrontRightWheel].mbBrokenAdhesiveLimit
                                           && !maWheels[eRearLeftWheel].mbBrokenAdhesiveLimit
                                           && !maWheels[eRearRightWheel].mbBrokenAdhesiveLimit;
        (void)lbAllWheelsHaveAdhesive;   // feeds the un-reconstructed vsel CurrentDriftAngle accumulate

        // 4. slip-ratio below threshold: steering^4 vs the CappedDriftScale lane (flagged compare).
        {
            const f32 lfSteer = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y;  // a control lane
            const f32 lfSteer4 = lfSteer * lfSteer * lfSteer * lfSteer;
            if (lfSteer4 > mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y && false)
            { ExitDrift(); return; }   // guarded false: the literal compare is un-homed (inert)
        }

        // 5. exit collision counters.
        if (mi8NumWorldCollisions > 0) { ExitDrift(); return; }
        if (miNumCollisions      > 0) { ExitDrift(); return; }

        // 6. attribs limit: per-car drift slip limit (mpAttribs->mvDriftParams0 @+0x110 .x) vs the seed slip
        //    (StartSlip lane * slip-time gain flt_830180B0, flagged-inert).
        {
            const f32 lfSeed = mvTimeToReachTargetDriftSlipRecip_StartSlip_TimeDrifting_BrakeScale.y
                               * KF_DRIFT_SLIP_TIME_GAIN;
            if (mpAttribs->mvDriftParams0.x > lfSeed && KF_DRIFT_SLIP_TIME_GAIN > 0.0f)
            { ExitDrift(); return; }
        }

        // 7. desired-slip cap: DesiredDriftSlip (@+0x1020 .z) > 1.0 -> exit.
        if (mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.z > 1.0f)
        { ExitDrift(); return; }

        // 8. above-ground invalid -> exit.
        if (!mAboveGroundTestResult.mbValid) { ExitDrift(); return; }

        // 9. speed too low: current body speed (mfSpeedMPH) below a limit (flagged-inert).
        if (KF_DRIFT_SPEED_EXIT_LIMIT > mfSpeedMPH.x && KF_DRIFT_SPEED_EXIT_LIMIT > 0.0f)
        { ExitDrift(); return; }

        // 10. steering crossed centre for the latched direction.
        if (mu8DriftState == eDriftState_FacingLeft || mu8DriftState == eDriftState_FacingRight)
        {
            const f32 lfSteer = lpControls ? lpControls->mfSteering : 0.0f;
            if (mu8DriftState == eDriftState_FacingRight)
            {
                if (lfSteeringDir >= -0.0099999998f && lfSteer >= -0.0049999999f)
                { ExitDrift(); return; }
            }
            else
            {
                if (lfSteeringDir <= 0.0099999998f && lfSteer <= 0.0049999999f)
                { ExitDrift(); return; }
            }
        }
    }

    // @0x825FA748  VehiclePhysics::UpdateDriftScale
    //   Grows mDriftScale toward the target drift slip and applies the natural self-aligning drift yaw.
    //   asm: reads *(this+68)==1 (a control mode) -> latches an aftertouch term; recomputes the local
    //   drift angle (acos of velocity vs the body basis, signed, in degrees via 57.29578, wrapped into
    //   [-180,180]); if the per-car drift slip lane (mpAttribs->mvDriftParams1 @+0x120 .x) exceeds the body speed
    //   OR the aftertouch latch is set, eases the scale; else integrates the scale toward the target by
    //   the time-step. Always tail-calls ApplyNaturalDriftForces.
    //   FIDELITY: PARTIAL -- the outer state machine (angle compute, the two ease/integrate branches,
    //   the ApplyNaturalDriftForces tail) is reconstructed against named lanes; the inner per-branch
    //   polynomial (the vexptefp/vlogefp + unk_82014AC0.. coefficient cascade) is an un-homed rodata
    //   curve carried as a flagged-inert blend so no fabricated coefficients are emitted.
    void VehiclePhysics::UpdateDriftScale(const BrnPlayerDriverControls* lpControls, f32 lfTimeStep, f32 lfSlip)
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

        // gate: per-car drift slip lane (mpAttribs->mvDriftParams0 @+0x110 .x) vs body speed, OR the
        // AI's force-come-out-of-drift request.
        const bool lbEase = !(mfSpeedMPH.x > mpAttribs->mvDriftParams0.x) || lbForceComeOutOfDrift;
        if (lbEase)
        {
            // ease branch: leave the drift scale where it is (the asm's "set to 1.0 seed" no-op store path
            // when below the slip threshold). FLAG: the eased target curve is the un-homed coefficient set.
            mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.x = 1.0f;  // seed
        }
        else
        {
            // integrate branch: grow CappedDriftScale toward DesiredDriftSlip by the time-step. The asm
            // mixes in the per-car drift register lanes (mpAttribs->mvDriftParams5 @+0x160 .y push) + the
            // mpAttribs->mvDriftParams1 weights; the exact polynomial blend is the flagged coefficient cascade.
            const f32 lfTarget  = mvDesiredDriftAngleScale_CappedDriftScale_DesiredDriftSlip_TimeInFrictionState.z; // DesiredDriftSlip
            f32 lfScale = mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w;   // DriftScale lane
            const f32 lfStep = lfTimeStep * mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.x; // *LatDriftForceFactor
            lfScale += (lfTarget - lfScale) * lfStep;   // first-order approach toward the target slip
            mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.w = lfScale;
        }
        (void)lfSlip;

        // always apply the natural self-aligning yaw.
        ApplyNaturalDriftForces();
    }

    // @0x825D25A0  VehiclePhysics::ApplyDriftYaw
    //   A world-space yaw torque rotating the car toward the drift direction. Gated on a computed local
    //   drift angle (the asm wraps acos(dot(vel,driftDir)) into degrees, requires it != 0) AND
    //   mDriftFlags.DoApplyTorque(). The torque magnitude is built from the per-car drift attrib lanes
    //   (mpAttribs->mvDriftParams2 @+0x130 .x gate, mpAttribs->mvDriftParams4 @+0x150 lanes for the response shape) and the
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

        // attrib-shaped response: the per-car drift register (mpAttribs->mvDriftParams2 @+0x130 .x) gates, and
        // mpAttribs->mvDriftParams4 (@+0x150) lanes shape the magnitude vs the (homed) speed. The response is an
        // interpolation between the attrib lanes -> a torque magnitude.
        const f32 lfGate = mpAttribs->mvDriftParams2.x;
        if (!(lfGate > 0.0f))
            return;

        // magnitude from the attrib response lanes (the asm's vmaddfp interp between .x/.y/.z of +0x150).
        const f32 lfA = mpAttribs->mvDriftParams4.x;
        const f32 lfB = mpAttribs->mvDriftParams4.y;
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
    //   (mpAttribs->mvDriftParams0 @+0x110 .y reciprocal, mpAttribs->mvDriftParams3 @+0x140 .y, mpAttribs->mvDriftParams5 @+0x160 lanes,
    //   mpAttribs->mvDriftParams7 @+0x180 .z) and the speed, signs it by the drift state, tangent-projects it
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

        // lateral magnitude shaped by the per-car drift attrib lanes. mpAttribs->mvDriftParams0.y (slip recip),
        // mpAttribs->mvDriftParams3.y (lat shape), mpAttribs->mvDriftParams5.x (force gate), mpAttribs->mvDriftParams7.z (final scale).
        const f32 lfSlipRecip = mpAttribs->mvDriftParams0.y;
        const f32 lfLatShape  = mpAttribs->mvDriftParams3.y;
        const f32 lfForceGate = mpAttribs->mvDriftParams5.x;
        const f32 lfFinalScale = mpAttribs->mvDriftParams7.z;

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
    void VehiclePhysics::ApplyDriftForces(const BrnPlayerDriverControls* lpControls, f32 lfSlipAngle,
                                          f32 lfSpeed, f32 lfSteeringDir)
    {
        MaintainDriftSpeed(lpControls, lfSpeed);
        UpdateDriftScale(lpControls, lfSpeed, lfSlipAngle);
        ApplyDriftYaw(lpControls, lfSlipAngle, lfSpeed);

        if (mbAllWheelsHaveTraction && mAboveGroundTestResult.mbValid && !mbHandBrake)
            ApplyDriftLatForce(lfSlipAngle, lfSpeed, lfSteeringDir, lfSpeed);
    }

    // @0x8262E200  VehiclePhysics::UpdateDrift
    //   The per-frame drift entry. Refreshes the cached steering direction (normalize(mLinearVelocity)
    //   into the steering register), runs the drift state machine (UpdateDriftState), then:
    //     * when drifting (mu8DriftState != 0): applies the drift forces (ApplyDriftForces), advancing
    //       the drift register from the per-car drift attrib lanes (mvDriftParams* @+0x110/+0x120) +
    //       the landing/damp coefficient cascade (unk_82014AC0..AF0, flagged).
    //     * when NOT drifting: eases the cached steering direction back toward forward by the per-car
    //       drift damp factor (mpAttribs->mvDriftParams6 @+0x170 .y) when above the drift slip threshold.
    //   Uses the ORIGINAL controls when the drift-override byte is set. The per-phase CheckState debug
    //   calls are elided.
    //   FIDELITY: PARTIAL -- the dispatch (steering-dir refresh, the drift/not-drift split, the
    //   UpdateDriftState + ApplyDriftForces calls, the not-drift ease) is reconstructed against named
    //   lanes; the two drift-register advance cascades (the vexptefp/vlogefp + unk_82014AC0.. landing/
    //   damp polynomials) are un-homed rodata curves carried as flagged-inert blends -- no fabricated
    //   coefficients emitted.
    void VehiclePhysics::UpdateDrift(const BrnPlayerDriverControls* lpOriginalControls, f32 lfTimeStep)
    {
        // refresh the cached steering direction: normalize(mLinearVelocity) -> the steering register's
        // packed direction (the asm vrsqrtefp/Newton + vsel zero-guard, stored to the +0x1000-region scratch).
        const Vector3 lUnitVel = vpu::Normalize(mLinearVelocity);
        mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.x = vpu::Dot(lUnitVel, mTransform.zAxis);

        // gather the slip/speed/steering scalars the state machine + forces consume (the v60/v61/v62
        // locals the asm fills before the UpdateDriftState call).
        const f32 lfSlipAngle  = mvLatDriftForceFactor_DriftPushTime_MaxSteeringAngle_CurrentDriftAngle.w;
        const f32 lfSpeed      = mfSpeedMPH.x;
        const f32 lfSteeringDir = mvSteeringAngle_Steering_PrevSteering_DriftGasLetOffAmount.y;

        UpdateDriftState(lpOriginalControls, lfSlipAngle, lfSpeed, lfSteeringDir);

        if (mu8DriftState != eDriftState_None)
        {
            // drift register advance: the asm runs two unk_82014AC0.. landing/damp polynomial cascades
            // over the per-car drift attrib lanes (mpAttribs->mvDriftParams1 @+0x120 +16 / +0 = +0x130/+0x120) and
            // folds the result into the +0x1000-region scratch + the body local velocity (+0x60). FLAG:
            // the coefficient tables are un-homed -> carried inert; the named lanes + the structure are
            // exact, the numeric advance stays 0 until the .rdata is recovered.
            if (mbAllWheelsHaveTraction)
                ApplyDriftForces(lpOriginalControls, lfSlipAngle, lfSpeed, lfSteeringDir);
        }
        else
        {
            // NOT drifting: ease the cached steering direction back toward forward by the per-car drift
            // damp factor (mpAttribs->mvDriftParams6 @+0x170 .y) when the body speed exceeds the per-car slip lane
            // (mpAttribs->mvDriftParams1 @+0x120 .y). The asm builds (1 - 1/speed*...)*dampedDir and subtracts it.
            const f32 lfSlipThresh = mpAttribs->mvDriftParams1.y;   // *(attribs+144) .y
            if (mfSpeedMPH.x > lfSlipThresh && !mbHandBrake)   // && !*(this+4952)
            {
                const f32 lfDamp = mpAttribs->mvDriftParams6.y;     // *(attribs+160) .y drift damp
                // ease the local velocity's lateral component toward the forward direction by lfDamp.
                mAngularVelocity.x -= mAngularVelocity.x * lfDamp * 0.0f;   // FLAG: damp gain folded with the
                mAngularVelocity.z -= mAngularVelocity.z * lfDamp * 0.0f;   // (homed) speed-recip; carried inert.
                (void)lfDamp;
            }
        }
        (void)lfTimeStep;
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
        static const Vector3 KV_CARCAR_SCALE0 = { 0.0f, 0.0f, 0.0f, 0.0f };   // FLAG: un-homed unk_82FB8870
        static const Vector3 KV_CARCAR_SCALE1 = { 0.0f, 0.0f, 0.0f, 0.0f };   // FLAG: un-homed unk_82FB9B70
        static const Vector3 KV_CARCAR_SCALE2 = { 0.0f, 0.0f, 0.0f, 0.0f };   // FLAG: un-homed unk_82FB9120

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
    //   else:                  rxJ *= mpAttribs->mvCrashImpulseScale.y  (lvx128 mpAttribs+0x280 ;
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
        const f32 lfScale = mpAttribs->mvCrashImpulseScale.y;
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
    static const f32 KF_SLAM_TAPER_DENOM = 0.0f;          // FLAG: un-homed flt_82F2A294 (air-time denom)
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
    static const f32 KF_SHUNT_DESIRED_SPEED_CEIL = 0.0f;   // FLAG: un-homed unk_82FB8B30 (clamp ceiling)

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
    if (KF_SHUNT_DESIRED_SPEED_CEIL != 0.0f && lfDesiredSpeed > KF_SHUNT_DESIRED_SPEED_CEIL)
        lfDesiredSpeed = KF_SHUNT_DESIRED_SPEED_CEIL;

    // store the direction ARGUMENT verbatim + desired speed into mDirectionPlusDesiredSpeed (+0x1130).
    mShuntEffect.mDirectionPlusDesiredSpeed.SetVector3(lvShuntDirection);
    mShuntEffect.mDirectionPlusDesiredSpeed.SetPlus(lfDesiredSpeed);

    // +0x1140 .y = the Life-seed ARGUMENT; .x = the un-homed unk_82FB90A0 seed lane (flagged-inert).
    static const f32 KF_SHUNT_LIFE_SEED_LANE = 0.0f;   // FLAG: un-homed unk_82FB90A0 lane0
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
    static const f32 KF_MODE1_STEER_CLAMP = 0.0f;     // FLAG: un-homed flt_82F2A500
    static const f32 KF_MODE1_STEER_SCALE = 0.0f;     // FLAG: un-homed flt_82F2A4FC

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
    // const f32 lfScale = mpAttribs->mvCrashImpulseScale.w;  (+0x280 .w)
    // mWeightTransferRow(+0xEF0) = bodyAxes * lfScale  (lane-by-lane vrlimi insert)
    }


}
}
