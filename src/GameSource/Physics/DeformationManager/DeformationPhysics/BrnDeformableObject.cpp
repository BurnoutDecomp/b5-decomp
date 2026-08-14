#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsRandom.h"       // CgsNumeric::Random
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{Dot, Cross, Subtract, ...}

// BrnPhysics::Deformation::DeformableObject::ApplyCarCarImpulse  @0x82624C08
//
// The two-body CAR-ON-CAR shunt impulse ORCHESTRATOR. The hard physics (the impulse solver) lives
// in ExternalPhysicsBody::CalculateCollisionImpulseWithBody; this function wires it up: it computes
// the closing velocity at the contact, early-outs if the cars are separating, asks the solver for
// the impulse, applies the showtime/bounce-boost SHAPING (the part that makes Burnout's takedowns
// feel punchy), and then applies the equal-and-opposite impulse to BOTH cars via ApplySensorImpulse
// (this car with the contact as-is, the other car with the reversed contact + negated impulse).
//
// The X360 build is VMX128 inline assembly. Per project convention (cf. ExternalPhysicsBody.cpp,
// BrnTagPoint.cpp) the body below is the DE-SIMD'd scalar/Vector3 equivalent written against the
// reconstructed members BY NAME -- no __asm, no raw offset pokes. The control flow + the named
// locals are recovered from the DecFIGS body hint (BrnDeformableObject.cpp:1518, which names every
// local: lPointVel/lPoint2Vel/lRelativeMotion/lvfClosingSpeed/lImpulse/lvfInvInertiaA/lvfInvInertiaB/
// lvfRestitution/lvfImpulseMagnitude/lParams/lReverseContact + the bounce-block locals
// lpRaceCarPhysics/lfNormalStressMagnitude/lfMinBouncePower/lfMassFactorScale/lScale/lLinearVelocity)
// and the X360 pseudocode/asm. The hard-tuning rodata vectors the bounce shaping scales by have no
// project home and are NOT fabricated -- they are file-static KVF_/KF_ placeholders, each FLAGGED.

namespace BrnPhysics
{
namespace Deformation
{
    namespace vpu = rw::math::vpu;

    // ---------------------------------------------------------------------------------------------
    // Unrecovered .rodata tuning values. The X360 build loads these from constant pools with no
    // symbol we can resolve; per the project no-fabrication rule they are honest zero/identity
    // placeholders, NOT invented numbers. The bounce shaping that consumes them is therefore
    // structurally faithful but numerically inert until the rodata is recovered.
    //   * unk_82FB81F0 -- the minimum bounce-stress threshold (squared-magnitude compare).
    //   * unk_82FB8210 / unk_82FB9E40 -- the bounce-boost scale vector (boosting vs not boosting).
    //   * unk_82FB7F70 / unk_82FB8040 -- the per-axis clamp band (min/max) for the shaped scale.
    //   * unk_82FB82F0 -- the double-bounce damp scale (the "already bounced this frame" path).
    // ---------------------------------------------------------------------------------------------
    // ⭐ RECOVERED 2026-08-03. These are NOT "un-homed rodata": they are .data slots that read zero
    // in the image and are filled at static-init time by tiny unexported blr-terminated splat runs.
    // Each line names its initialiser and the .rdata scalar it splats. The clamp band reading
    // [0.75, 1.5] and the two bounce scales landing either side of it is a self-consistent set.
    static const f32     KF_MIN_BOUNCE_STRESS_SQ   = 2000000.0f;                  // unk_82FB81F0 @82C5D598 <- flt_8209D734
    static const Vector3 KVF_BOUNCE_BOOST_SCALE     = { 2.79999995f, 2.79999995f, 2.79999995f, 2.79999995f }; // unk_82FB8210 @82C5D4A8 <- flt_8200C6B8
    static const Vector3 KVF_BOUNCE_NOBOOST_SCALE   = { 1.5f, 1.5f, 1.5f, 1.5f }; // unk_82FB9E40 @82C5D480 <- flt_820945DC
    static const Vector3 KVF_BOUNCE_CLAMP_MIN       = { 0.75f, 0.75f, 0.75f, 0.75f }; // unk_82FB7F70 @82C5D548 <- flt_82004018
    static const Vector3 KVF_BOUNCE_CLAMP_MAX       = { 1.5f, 1.5f, 1.5f, 1.5f }; // unk_82FB8040 @82C5D570 <- flt_820945DC
    static const Vector3 KVF_DOUBLE_BOUNCE_DAMP     = { 30.0f, 30.0f, 30.0f, 30.0f }; // unk_82FB82F0 @82C5D430 <- flt_82004F5C

    // The one literal that IS visible in the asm: v143 = 0.00066666666f == 1/1500, the bounce-power
    // y-floor the shaped scale's Y lane is seeded with before the clamp/max.
    static const f32 KF_BOUNCE_POWER_Y_FLOOR = 0.00066666666f;   // asm-visible (= 1/1500)

    // The other-car game-mode HIGH byte the cross-car bounce gate compares against: the takedown/
    // showtime-eligible game mode (the asm tests `HIBYTE(otherCar.mGameModeState) == 2`).
    static const u8 KU_GAMEMODE_BOUNCE_ELIGIBLE = 2;

    // ---------------------------------------------------------------------------------------------
    // ApplyCarCarImpulse
    // ---------------------------------------------------------------------------------------------
    bool DeformableObject::ApplyCarCarImpulse(const StoredImpulseContact& lContact, VecFloat lvfTimeStep,
                                              VecFloat lvfIteration, s32 liSensorIndex,
                                              const CgsNumeric::Random& lRandom)
    {
        // The contact must reference the other car + the sensor on it that owns the contact.
        CGS_ASSERT(lContact.mpOtherSensor  != nullptr, "lContact.mpOtherSensor");
        CGS_ASSERT(lContact.mpOtherVehicle != nullptr, "lContact.mpOtherVehicle");

        ExternalPhysicsBody&       lThisBody  = GetVehicleBody();
        DeformableObject&          lOtherCar  = *lContact.mpOtherVehicle;
        ExternalPhysicsBody&       lOtherBody = lOtherCar.GetVehicleBody();

        // The contact wakes both bodies: the impulse must integrate, so neither stays frozen
        // (asm: the +112 reset + the SetFrozen(false) the body hint names).
        lThisBody.SetFrozen(false);

        // -------- closing velocity at the contact --------
        // Velocity of each body at its contact point (linear + omega x r), in world space.
        const Vector3 lPointVel  = lThisBody.GetLocalVelocity(lContact.mPointOnA, rw::physics::WORLD_SPACE);
        const Vector3 lPoint2Vel = lOtherBody.GetLocalVelocity(lContact.mPointOnB, rw::physics::WORLD_SPACE);

        // Relative motion of A w.r.t. B, projected onto the contact normal -> closing speed.
        const Vector3 lRelativeMotion = vpu::Subtract(lPointVel, lPoint2Vel);
        const f32     lfClosingSpeed  = vpu::Dot(lRelativeMotion, lContact.mNormal);

        // Separating (or at rest): nothing to do (asm: the `0 >= closingSpeed` early `_restvmx_122(0)`).
        if (lfClosingSpeed <= 0.0f)
            return false;

        // -------- solve the two-body collision impulse --------
        const VecFloat lvfRestitution = GetVehicleWorldRestitution(lContact);

        Vector3  lImpulse;        // j * n  (the impulse vector the solver writes)
        VecFloat lvfInvInertiaA;  // this car's inverse effective-mass term
        VecFloat lvfInvInertiaB;  // the other car's inverse effective-mass term
        const VecFloat lvfImpulseMagnitude = lThisBody.CalculateCollisionImpulseWithBody(
            lOtherBody, lContact.mPointOnA, lContact.mPointOnB, lRelativeMotion, lContact.mNormal,
            lvfRestitution, &lImpulse, &lvfInvInertiaA, &lvfInvInertiaB);

        // -------- showtime / bounce-boost shaping --------
        // The asm gates the shaping on a virtual showtime predicate on THIS car's physics body (the
        // inlined `(*(vtbl+16))(body)` -> IsPlayerVehicleActuallyInShowtime). It is a TRUE/FALSE split
        // (asm: the `else` at ~line 958), NOT two nested cases:
        //   * predicate TRUE  (this car in showtime): the punchy BOUNCE-BOOST path, itself gated on the
        //       OTHER car's game-mode HIGH byte == 2 (takedown/bounce-eligible). Any other mode: nothing.
        //   * predicate FALSE (this car NOT in showtime): the DOUBLE-BOUNCE-DAMP path. It (a) ALWAYS runs
        //       a bare-shaping step on the impulse, then (b) gates on THIS car's mode HIGH byte == 2, and
        //       (c) only then checks (other-car-in-showtime || other-car-has-bounced) before applying the
        //       damp scale and latching THIS car's mbHasBouncedThisFrame (+26414).
        // A body that is not a race car (no showtime) takes the FALSE/damp path with this car's mode.
        Vehicle::RaceCarPhysics* lpRaceCarPhysics = AsRaceCarPhysics();
        const bool lbThisInShowtime = (lpRaceCarPhysics != nullptr) &&
                                      lpRaceCarPhysics->IsPlayerVehicleActuallyInShowtime();
        if (lbThisInShowtime)
        {
            // ----- predicate TRUE: bounce-boost (asm ~lines 843-957) -----
            // Gated on the OTHER car's game-mode HIGH byte (asm: `HIBYTE(*(otherCar+26392)) == 2`).
            if (lOtherCar.GetGameModeByte() == KU_GAMEMODE_BOUNCE_ELIGIBLE)
            {
                // Was the impact above the minimum bounce stress? (squared magnitude vs the rodata band).
                const f32  lfNormalStressMagnitude = vpu::MagnitudeSquared(lImpulse);
                const bool lbOverMinStress         = lfNormalStressMagnitude >= KF_MIN_BOUNCE_STRESS_SQ;

                // Tell the race car it just bounced (so showtime can react). The bounce direction is
                // the impulse direction; the entity id is this car's. (asm: SetJustBounced(body,1,<stress>,id)).
                lpRaceCarPhysics->SetJustBounced(lImpulse, true, lbOverMinStress, GetGlobalEntityId());

                // Build the shaped bounce velocity scale. The boost-scale vector differs when the car
                // is actively bounce-boosting; the impulse is scaled by it, the Y lane is floored to
                // the visible 1/1500 power, then clamped into the rodata band. Every rodata vector here
                // is a FLAGGED placeholder, so this reproduces the SHAPE of the shaping, not the values.
                const Vector3& lrBoostScale = lpRaceCarPhysics->IsBounceBoosting()
                                                ? KVF_BOUNCE_BOOST_SCALE
                                                : KVF_BOUNCE_NOBOOST_SCALE;

                Vector3 lScale = vpu::Mult(lImpulse, lrBoostScale);   // per-lane scale of the impulse
                lScale.y = (lScale.y > KF_BOUNCE_POWER_Y_FLOOR) ? lScale.y : KF_BOUNCE_POWER_Y_FLOOR;

                // Mass-factor scale: the other car's accumulated linear impulse contributes a per-lane
                // factor (asm: lvx body+224 -> mTotalLinearImpulse, multiplied into the band before the
                // clamp). Modelled by name; the multiplier vector is the FLAGGED clamp band itself.
                const Vector3 lLinearVelocity   = lOtherBody.GetLocalVelocity(lContact.mPointOnB, rw::physics::WORLD_SPACE);
                const f32     lfMassFactorScale = vpu::Magnitude(lLinearVelocity);
                (void)lfMassFactorScale;   // folds into the clamp band below; band is unrecovered (FLAG)

                // Clamp the shaped scale into the rodata min/max band (asm: vmaxfp then vminfp).
                lScale.x = (lScale.x < KVF_BOUNCE_CLAMP_MIN.x) ? KVF_BOUNCE_CLAMP_MIN.x
                         : (lScale.x > KVF_BOUNCE_CLAMP_MAX.x) ? KVF_BOUNCE_CLAMP_MAX.x : lScale.x;
                lScale.y = (lScale.y < KVF_BOUNCE_CLAMP_MIN.y) ? KVF_BOUNCE_CLAMP_MIN.y
                         : (lScale.y > KVF_BOUNCE_CLAMP_MAX.y) ? KVF_BOUNCE_CLAMP_MAX.y : lScale.y;
                lScale.z = (lScale.z < KVF_BOUNCE_CLAMP_MIN.z) ? KVF_BOUNCE_CLAMP_MIN.z
                         : (lScale.z > KVF_BOUNCE_CLAMP_MAX.z) ? KVF_BOUNCE_CLAMP_MAX.z : lScale.z;

                // The shaped scale replaces the raw impulse for the apply step.
                lImpulse = lScale;

                // Pick the random double-bounce parity for the OTHER car (asm: an LCG draw whose %3==0
                // result is stored into the other car's +26413 parity flag). Faithful in structure;
                // the draw uses the supplied Random.
                const u32 luDraw = const_cast<CgsNumeric::Random&>(lRandom).RandomUInt();
                lOtherCar.SetBounceRandomParity((luDraw % 3u) == 0u);
            }
            // (asm: when HIBYTE(otherCar+26392) != 2 the TRUE branch does nothing -- raw impulse kept.)
        }
        else
        {
            // ----- predicate FALSE: double-bounce damp (asm `else` ~lines 958-1176) -----
            // (a) UNCONDITIONAL bare-shaping step (asm ~lines 960-975): scale the impulse per-lane by
            //     `(lvfIteration + 1) * 0.5`. The 1.0/0.5 are asm-visible vcfsx immediates (NOT rodata):
            //     `vcfsx 1>>0 = 1.0`, `vcfsx 1>>1 = 0.5`. lvfIteration is a broadcast VecFloat, so the
            //     per-lane multiply is a scalar broadcast of that factor.
            const f32 lfBareShapeFactor = (lvfIteration.x + 1.0f) * 0.5f;
            lImpulse = vpu::Mult(lImpulse, lfBareShapeFactor);

            // (b) Gate the rest on THIS car's game-mode HIGH byte == 2 (asm ~line 976:
            //     `HIBYTE(*(this+26392)) != 2` -> skip straight to apply). NOTE: this is THIS car's
            //     mode (GetGameModeByte() on `this`), NOT the other car's.
            if (GetGameModeByte() == KU_GAMEMODE_BOUNCE_ELIGIBLE)
            {
                // (c) Only when this-car-mode == 2: apply the damp + latch iff the OTHER car is in
                //     showtime OR the OTHER car has already bounced this frame (asm ~line 1163:
                //     `otherCar.IsPlayerVehicleActuallyInShowtime() || *(otherCar+26414)`).
                const Vehicle::RaceCarPhysics* lpOtherRaceCar = lOtherCar.AsRaceCarPhysics();
                const bool lbOtherInShowtime = (lpOtherRaceCar != nullptr) &&
                                               lpOtherRaceCar->IsPlayerVehicleActuallyInShowtime();
                if (lbOtherInShowtime || lOtherCar.HasBouncedThisFrame())
                {
                    // Latch THIS car's bounced-this-frame flag (asm ~line 1166: `*(this+26414) = 1`),
                    // then apply the double-bounce DAMP scale (asm: the unk_82FB82F0 multiply). The
                    // damp vector is a FLAGGED placeholder.
                    SetHasBouncedThisFrame(true);
                    lImpulse = vpu::Mult(lImpulse, KVF_DOUBLE_BOUNCE_DAMP);
                }
            }
        }

        // -------- apply the equal-and-opposite impulse to both cars --------
        // Build the impulse parameter block. The fields the orchestrator sets directly are the
        // impulse magnitude, the world-space impulse direction and the per-body inverse inertia; the
        // remaining ImpulseParams fields are filled by ApplySensorImpulse / the body apply.
        ImpulseParams lParams;
        lParams.mvfImpulseMagnitude    = lvfImpulseMagnitude;
        lParams.mWorldImpulseDirection = lContact.mNormal;
        lParams.mvfInverseInertia      = lvfInvInertiaA;
        lParams.mvfTimeStep            = lvfTimeStep;
        lParams.mePositionSpace        = rw::physics::WORLD_SPACE;

        // This car: apply the impulse as-is at the contact (sensor liSensorIndex), adding to the spy.
        DeformationSensor* lpSensor = GetDeformationSensor(liSensorIndex);
        ApplySensorImpulse(lvfTimeStep, lContact, lParams, lRelativeMotion, lImpulse,
                           lvfImpulseMagnitude, lpSensor, /*lbAddToSpy*/ true,
                           /*lbUseNormalScaledFriction*/ true);

        // The other car: apply the reversed contact (A<->B, normal negated) with the negated impulse
        // and its own inverse-inertia term -- the equal-and-opposite half of the shunt.
        StoredImpulseContact lReverseContact;
        lContact.GetInverse(lReverseContact);
        lParams.mvfInverseInertia      = lvfInvInertiaB;
        lParams.mWorldImpulseDirection = lReverseContact.mNormal;
        lOtherCar.ApplySensorImpulse(lvfTimeStep, lReverseContact, lParams, vpu::Negate(lRelativeMotion),
                                     vpu::Negate(lImpulse), lvfImpulseMagnitude, lContact.mpOtherSensor,
                                     /*lbAddToSpy*/ false, /*lbUseNormalScaledFriction*/ true);

        return true;
    }

    // =============================================================================================
    // WALLS LEG 4 (2026-08-14): ApplyCarWorldImpulse -- the CAR-vs-WORLD impulse orchestrator.
    // X360 @0x82624898 is an EXPORT HOLE; the PS3 body @0x746D68 (253 insns) is the authority,
    // with the sibling ApplyCarCarImpulse (this TU, derived from both consoles) settling the
    // shared idioms (the (iteration+1)*0.5 shaping literals are asm-visible vcfsx immediates in
    // BOTH bodies). Showtime constants recovered this wave from the PS3 static initializer
    // (__static_init_22 @0x6C2CCC): KF_SHOWTIME_BOUNCE_BOOST_SCALE = 2.0,
    // KF_SHOWTIME_MIN_WORLD_BOUNCE_POWER = 8.0, KVF_Y_COMPONENT_BIG_BOUNCE_MIN = 0.5.
    //
    // Flow (PS3, register for register):
    //   r        = lContact.mPointOnA - body position (row +0x40);
    //   relMotion= linVel (+0x50) + angVel (+0x60) x r      (the gCrossProductPermuteConstant pair);
    //   closing  = dot(relMotion, lContact.mNormal); if (closing >= 0) return false  (separating);
    //   restitution = GetVehicleWorldRestitution(lContact);
    //   mag = body.CalculateCollisionImpulseWithInanimateObject(r, relMotion, normal, restitution,
    //                                                           &impulse, &invInertia);
    //   if (vehicle->IsPlayerVehicleInShowtime()) {           (the vtable +0x10 virtual)
    //       if (n.y*|n.y| >= n.x^2 + n.z^2 && n.y > 0) {      (upward-dominant landing normal)
    //           RaceCarPhysics::SetJustBounced(contact-normal bounce, ...);
    //           if (ShouldBounceBoostNextImpact())
    //               mag = max(mag * KF_SHOWTIME_BOUNCE_BOOST_SCALE,
    //                         KF_SHOWTIME_MIN_WORLD_BOUNCE_POWER);   [FLAG: the exact fsel fold of
    //               the boost is modelled scale-then-floor -- showtime is dead on the junkyard
    //               path; re-derive when showtime lands]
    //       }
    //   } else impulse *= (lvfIteration + 1) * 0.5;           (the sibling's asm-immediate shaping)
    //   build the WORLD ImpulseParams and ApplySensorImpulse(...) -> return true.
    // =============================================================================================
    namespace
    {
        // PS3 __static_init_22 values (see the wave log): the showtime world-bounce family.
        const f32 KF_SHOWTIME_BOUNCE_BOOST_SCALE     = 2.0f;
        const f32 KF_SHOWTIME_MIN_WORLD_BOUNCE_POWER = 8.0f;
    }

    bool DeformableObject::ApplyCarWorldImpulse(const StoredImpulseContact& lContact,
                                                VecFloat lvfTimeStep, VecFloat lvfIteration,
                                                s32 liSensorIndex)
    {
        BrnPhysics::Vehicle::VehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();

        // Relative motion of the body point at the contact.
        const Vector3 lvR = vpu::Subtract(lContact.mPointOnA, lpVehicle->GetPosition());
        const Vector3 lRelativeMotion =
            vpu::Add(lpVehicle->GetLinearVelocity(), vpu::Cross(lpVehicle->GetAngularVelocity(), lvR));

        // Separating contact: no impulse.
        if ( vpu::Dot(lRelativeMotion, lContact.mNormal) >= 0.0f )
        {
            return false;
        }

        const VecFloat lvfRestitution = GetVehicleWorldRestitution(lContact);

        Vector3  lImpulse{ 0.0f, 0.0f, 0.0f, 0.0f };
        VecFloat lvfInvInertia{ 0.0f, 0.0f, 0.0f, 0.0f };
        VecFloat lvfImpulseMagnitude = GetVehicleBody().CalculateCollisionImpulseWithInanimateObject(
            lvR, lRelativeMotion, lContact.mNormal, lvfRestitution, &lImpulse, &lvfInvInertia);

        if ( lpVehicle->IsPlayerVehicleInShowtime() )   // the vtable +0x10 virtual
        {
            const Vector3& lrN = lContact.mNormal;
            if ( lrN.y > 0.0f && (lrN.y * lrN.y) >= (lrN.x * lrN.x + lrN.z * lrN.z) )
            {
                BrnPhysics::Vehicle::RaceCarPhysics* lpRaceCar = AsRaceCarPhysics();
                if ( lpRaceCar != nullptr )
                {
                    lpRaceCar->SetJustBounced(lContact.mNormal, false, false, EntityId{ 0u });
                    if ( lpRaceCar->ShouldBounceBoostNextImpact() )
                    {
                        f32 lfMag = lvfImpulseMagnitude.x * KF_SHOWTIME_BOUNCE_BOOST_SCALE;
                        if ( lfMag < KF_SHOWTIME_MIN_WORLD_BOUNCE_POWER )
                        {
                            lfMag = KF_SHOWTIME_MIN_WORLD_BOUNCE_POWER;   // the fsel floor
                        }
                        lvfImpulseMagnitude = VecFloat{ lfMag, lfMag, lfMag, lfMag };
                    }
                }
            }
        }
        else
        {
            // The sibling ApplyCarCarImpulse's asm-immediate shaping (vcfsx 1.0 / 0.5), identical
            // instruction pair in this body: impulse *= (iteration + 1) * 0.5.
            const f32 lfShape = (lvfIteration.x + 1.0f) * 0.5f;
            lImpulse = vpu::Mult(lImpulse, lfShape);
        }

        // The WORLD-contact params block; the remaining fields are filled by ApplySensorImpulse.
        ImpulseParams lParams;
        lParams.mvfImpulseMagnitude    = lvfImpulseMagnitude;
        lParams.mWorldImpulseDirection = lContact.mNormal;
        lParams.mvfInverseInertia      = lvfInvInertia;
        lParams.mvfTimeStep            = lvfTimeStep;
        lParams.mePositionSpace        = rw::physics::WORLD_SPACE;
        lParams.mImpulsePosition       = lContact.mPointOnA;
        lParams.mbWorldContact         = true;
        lParams.meAbsorptionSet        = meAbsorptionSet;

        ApplySensorImpulse(lvfTimeStep, lContact, lParams, lRelativeMotion, lImpulse,
                           lvfImpulseMagnitude, GetDeformationSensor(liSensorIndex),
                           /*lbAddToSpy*/ true, /*lbUseNormalScaledFriction*/ true);
        return true;
    }

}
}
