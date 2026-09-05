#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsRandom.h"       // CgsNumeric::Random
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{Dot, Cross, Subtract, ...}
#include <cstdlib>                                          // getenv -- the opt-in [worldimp] probe only
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint -- the opt-in [worldimp] probe only

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
// [T5-imp] DIAG counters, DEFINED in
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_UpdateTrafficPhysics.cpp.
// NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
namespace Vehicle
{
    extern u32 gT5CarCarCalls;
    extern u32 gT5CarCarApplied;
    extern f32 gT5LastImpulseMag;
    extern f32 gT5LastClosing;
    extern f32 gT5MaxImpulseMag;
    extern f32 gT5SumImpulseMag;
    extern f32 gT5MaxClosing;
    extern s32 gT5RamFramesLeft;

    // [kerb] PC bring-up instrument -- DELETE-WHEN the kerb response is proven 1:1. Owned by
    // BrnVehicleManager_ValidateRaceCarWorldContact.cpp (banner + definitions there); this TU only
    // prints the [kerb-imp] leg. NOT IN THE X360 BINARY.
    extern u32 guKerbProbeFrame;
    bool KerbProbeArmed();
    bool KerbProbeTake(u32& lruUsed, const char* lpcTag);
}

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
    // These are NOT "un-homed rodata": they are .data slots that read zero
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

        // [T5-imp] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
        ++BrnPhysics::Vehicle::gT5CarCarCalls;

        // The contact wakes THIS body (asm 0x82624CAC `stb r29(0), 0x70(vehiclePhysics)` == mbFrozen).
        // ⚠️ ONLY this one: the console never clears the OTHER car's frozen byte here, and the
        // banner that used to read "wakes both bodies" was wrong about the console, not about the
        // code. The other car is woken by its own UpdateFreezing once this impulse gives it speed.
        lThisBody.SetFrozen(false);

        // THE CONTACT COOL-DOWN RESET THIS BODY OWNS.
        // asm 0x82624CB4..0x82624CC8:
        //     lwz  r11, 0x194C(this)            ; the attached VehiclePhysics
        //     addi r11, r11, 0x1060             ; mvTimeStandingStill_CoolDown_...
        //     lvx128 v0 ; vrlimi128 v0, v126(0), 4, 0 ; stvx128 v0
        // mask 4 == insert lane 1 == the .y CoolDown lane. Same lane ReadPotentialContact zeroes
        // for both cars on acceptance; the console zeroes it AGAIN here, for the car being solved.
        {
            BrnPhysics::Vehicle::VehiclePhysics* const lpThisVehicle = mVehicleBody.GetVehiclePhysics();
            if (lpThisVehicle != nullptr)
            {
                lpThisVehicle->mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.y = 0.0f;
            }
        }

        // -------- closing velocity at the contact --------
        // Velocity of each body at its contact point (linear + omega x r), in world space.
        const Vector3 lPointVel  = lThisBody.GetLocalVelocity(lContact.mPointOnA, rw::physics::WORLD_SPACE);
        const Vector3 lPoint2Vel = lOtherBody.GetLocalVelocity(lContact.mPointOnB, rw::physics::WORLD_SPACE);

        // THE NEGATED NORMAL. The closing-speed test is computed against -mNormal; against
        // +mNormal no car-on-car contact -- race car vs traffic or race car vs race car -- ever
        // transfers momentum.
        //   X360 0x82624CD8  lvx128  v10, r0, r23         ; r23 == &lContact.mNormal (contact+0x20)
        //        0x82624CD4  vslw128 v0, v127, v127       ; v0 == 0x80000000 splat (the sign bit)
        //        0x82624CEC  vxor    v9, v10, v0          ; v9  == -mNormal
        //        0x82624D4C  vsubfp128 v125, v0, v13      ; v125 == pointVelA - pointVelB
        //        0x82624D50  vmsum3fp128 v122, v125, v9   ; closing == dot3(relMotion, -mNormal)
        //        0x82624D54  vcmpgefp128. v0, v126(0), v122 ; `0 >= closing` -> return 0
        // The contact normal points from B to A, so a car CLOSING on the other has
        // dot(relMotion, +normal) < 0 and dot(relMotion, -normal) > 0. With the un-negated normal
        // the `<= 0` gate rejected exactly the contacts it is meant to accept.
        // ⚠️ The same -mNormal is handed to CalculateCollisionImpulseWithBody below
        // (asm 0x82624D8C `vxor v4, v10, v0` -> the v4 argument slot).
        const Vector3 lRelativeMotion = vpu::Subtract(lPointVel, lPoint2Vel);
        const Vector3 lNegatedNormal  = vpu::Negate(lContact.mNormal);          // 0x82624CEC / 0x82624D8C
        const f32     lfClosingSpeed  = vpu::Dot(lRelativeMotion, lNegatedNormal);

        // [T5-imp] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Recorded BEFORE the gate so a
        // rejected contact still prints the number it was rejected on.
        BrnPhysics::Vehicle::gT5LastClosing = lfClosingSpeed;
        if (lfClosingSpeed > BrnPhysics::Vehicle::gT5MaxClosing)
            BrnPhysics::Vehicle::gT5MaxClosing = lfClosingSpeed;

        // Separating (or at rest): nothing to do (asm: the `0 >= closingSpeed` early `_restvmx_122(0)`).
        if (lfClosingSpeed <= 0.0f)
            return false;

        // -------- solve the two-body collision impulse --------
        // THE RESTITUTION ARGUMENT IS A HARD ZERO, not
        // GetVehicleWorldRestitution. `vmr128 v5, v126` @0x82624D98 with v126 == vspltisw128 0
        // (@0x82624C94), and the function's xrefs_from carries NO call to
        // GetVehicleWorldRestitution at all (only CalculateCollisionImpulseWithBody,
        // SetJustBounced, IsBounceBoosting and ApplySensorImpulse). The GetVehicleWorldRestitution
        // call that stood here was imported from the WORLD sibling, where the PS3 body really does
        // consult it. A car-on-car shunt in Burnout is perfectly inelastic; cars do not bounce off
        // each other.
        const VecFloat lvfRestitution = { 0.0f, 0.0f, 0.0f, 0.0f };   // v5 == v126 == 0

        Vector3  lImpulse;        // j * n  (the impulse vector the solver writes)
        VecFloat lvfInvInertiaA;  // this car's inverse effective-mass term
        VecFloat lvfInvInertiaB;  // the other car's inverse effective-mass term
        const VecFloat lvfImpulseMagnitude = lThisBody.CalculateCollisionImpulseWithBody(
            lOtherBody, lContact.mPointOnA, lContact.mPointOnB, lRelativeMotion, lNegatedNormal,
            lvfRestitution, &lImpulse, &lvfInvInertiaA, &lvfInvInertiaB);

        // [T5-solve] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. The two-body solve's
        // operands while a [T5-ram] window is open: both inverse effective-mass terms (what each
        // sensor's displacement is multiplied by) beside the masses and the closing speed.
        {
            static s32 s_iT5SolveBudget = 240;
            if (BrnPhysics::Vehicle::gT5RamFramesLeft > 0 && s_iT5SolveBudget > 0
                && CgsDev::Log::gpDebugPrint != 0)
            {
                --s_iT5SolveBudget;
                *CgsDev::Log::gpDebugPrint
                    << "[T5-solve] thisOwner=" << static_cast<s32>(GetHandlingBodyIdHighByte())
                    << " otherOwner=" << static_cast<s32>(lOtherCar.GetHandlingBodyIdHighByte())
                    << " closing=" << lfClosingSpeed
                    << " j=" << lvfImpulseMagnitude.x
                    << " |imp|=" << vpu::Magnitude(lImpulse)
                    << " invA=" << lvfInvInertiaA.x << " invB=" << lvfInvInertiaB.x
                    << " mA=" << lThisBody.GetMass().x << " mB=" << lOtherBody.GetMass().x
                    << " sensor=" << liSensorIndex
                    << "\n";
            }
        }

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
        // Build the impulse parameter block.
        //
        // THE DROPPED STORES. `ImpulseParams` is a 0xC0 == 192-byte
        // POD and `ImpulseParams lParams;` leaves every field this function does not assign as
        // UNINITIALISED STACK. Five fields the console writes here were never written in-tree, and
        // one of them (mpImpulsePasser) was the leg-8 access violation: the sensor's chain forward
        // does `if (params->mpImpulsePasser) params->mpImpulsePasser->PassOnImpulse(...)`, which on
        // stack garbage passes the test and then reads `mapCollidableBodies[i]` off a garbage `this`.
        // ⚠️⚠️ A NON-NULL GARBAGE POINTER DEFEATS A NULL TEST -- the carried rule, one level up.
        // Every store below is asm-attested at the address in its comment (X360 @0x82624C08).
        ImpulseParams lParams;
        lParams.mvfImpulseMagnitude    = lvfImpulseMagnitude;
        lParams.mWorldImpulseDirection = lContact.mNormal;
        lParams.mvfInverseInertia      = lvfInvInertiaA;
        lParams.mvfTimeStep            = lvfTimeStep;                          // 0x82625120 -> +0x70
        lParams.mePositionSpace        = rw::physics::WORLD_SPACE;             // 0x82625060 -> +0x50 (r29 == 0)
        lParams.mImpulsePosition       = lContact.mPointOnA;                   // 0x82625078 -> +0x20 (lvx r31 == &lContact)
        lParams.mbWorldContact         = false;                                // 0x82625124 -> +0xB8 (stb r29 == 0)
        // +0x80 mvfVelocityAlongNormal <- v122 == vmsum3fp128(relativeMotion, normal) @0x82624D50,
        // i.e. the CLOSING SPEED, stored at 0x8262510C. Positive by construction on this path (the
        // function has already early-returned when it is <= 0), which is exactly why the head's
        // `ASSERT(mvfVelocityAlongNormal >= 0)` (line 319) never fires on console.
        lParams.mvfVelocityAlongNormal = VecFloat{ lfClosingSpeed, lfClosingSpeed,
                                                   lfClosingSpeed, lfClosingSpeed };
        // +0xB0 mpImpulsePasser <- `addi r11, r30, 0x18E4 ; stw r11, var_D0(r1)` @0x82625118/28.
        // 0x18E4 is this object's mImpulsePasser map base (the same base ResetSensors' inlined
        // SetCollidableBodyMap stores through). THIS is the chain's handle -- without it the sensor
        // forwards into garbage.
        lParams.mpImpulsePasser        = &mImpulsePasser;                      // 0x82625118/0x82625128
        // (+0xB4 meAbsorptionSet is written by ApplySensorImpulse itself, `lwz r10,0x675C(this)`
        //  @0x8260793C -> `stw r10, var_1DC` @0x82607940; see that TU.)

        // THE NORMALIZE THE TREE HAD DROPPED. ApplySensorImpulse's
        // arg 5 is named `lImpulseDir` and arg 6 `lvfImpulseMagnitude`, and the console means both
        // names literally: it runs ONE rsqrt pipeline over the shaped impulse and hands the callee
        // the UNIT direction and the LENGTH separately. X360 @0x82625044..0x826250EC:
        //     lvx128 v9, var_200            ; the shaped impulse
        //     vmsum3fp128 v0, v9, v9        ; |imp|^2
        //     vrsqrtefp + two Newton refines
        //     vmulfp128 v0, v0, v11         ; |imp|^2 * rsqrt   == the LENGTH
        //     vmulfp128 v3, v9, v13         ; imp     * rsqrt   == the UNIT DIRECTION
        //     vsel128 v126, v0, v7, v126    ; zero-length guard on the length
        //     0x826250FC stvx128 v3 -> var_200   (the local impulse is REPLACED by the unit form)
        //     0x82625108 vmr128  v4, v126        ; arg 4 (v4) == the LENGTH
        //                          v3            ; arg 3 (v3) == the UNIT DIRECTION
        // (PS3 does the identical thing at 0x746FF8..0x74703C for the world twin; its ABI puts the
        //  vector args in v2..v5, which is what makes `vmr v2, v27` == the timeStep argument.)
        // ⛔ WHY IT MATTERS: `ApplySensorImpulse` forms the per-direction impulse as
        // `lImpulseDir * lvfImpulseMagnitude` (`vmulfp128 v126, v116, v120` @0x82607AB4). Feeding it
        // a magnitude-BEARING direction squares the impulse -- measured live before this landed:
        // the +Y floor contact handed the vehicle body 2.15e5 instead of ~7e2. A dimensionally
        // impossible number, which is exactly how it was caught.
        Vector3   lImpulseUnit;
        const f32 lfImpulseLength = vpu::NormalizeReturnMagnitude(lImpulse, lImpulseUnit);
        const VecFloat lvfShapedMagnitude =
            VecFloat{ lfImpulseLength, lfImpulseLength, lfImpulseLength, lfImpulseLength };

        // [T5-imp] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. The two numbers that say
        // whether momentum was actually transferred: the closing speed the solve ran on and the
        // shaped impulse LENGTH both halves are about to receive.
        ++BrnPhysics::Vehicle::gT5CarCarApplied;
        BrnPhysics::Vehicle::gT5LastImpulseMag = lfImpulseLength;
        // The sample-period trap, closed: the T5 line samples every 10th frame and the impact
        // impulse lands between samples. MAX and SUM since the window was armed survive that.
        if (lfImpulseLength > BrnPhysics::Vehicle::gT5MaxImpulseMag)
            BrnPhysics::Vehicle::gT5MaxImpulseMag = lfImpulseLength;
        BrnPhysics::Vehicle::gT5SumImpulseMag += lfImpulseLength;

        // ---- [kerb-imp] THE CAR-CAR LEG. ⭐⭐⭐ ADDED 2026-09-03, AND ITS ABSENCE COST FOUR WAVES A
        //      WRONG CONCLUSION. The tag's banner (BrnVehicleManager_ValidateRaceCarWorldContact.cpp)
        //      said "one line per WORLD impulse the solver applies", which is literally true and was
        //      read as "one line per impulse the solver applies". UpdateContacts @0x826478B0 routes
        //      each sorted contact to ONE OF TWO arms -- ApplyCarWorldImpulse (instrumented) and this
        //      one (silent) -- and both end in the same CalculateNewVelocity. So a car-car hit moved
        //      the car with [kerb-imp] EMPTY, and "the losing step had zero impulses" was inferred
        //      from a probe that only ever watched half the solver.
        //      MEASURED THE DAY THIS LANDED (run dv_r8, [dv] witness): at f6386 a SINGLE drain at
        //      DeformableObject::UpdateContacts +0x4bd banked J = (5592.9, -115.1, -10519.6) N.s and
        //      took 7.50 m/s out of the car in one step -- and that frame has ZERO [kerb-imp] lines,
        //      while f6355 / f6473 / f7094 have exactly one per drain. Same function, same drain
        //      site, opposite visibility. [[diagnostics-that-lie]], textbook.
        //      The line is deliberately the SAME SHAPE as the world leg's, with `other` naming the
        //      second car, so one grep covers both arms.
        if ( BrnPhysics::Vehicle::KerbProbeArmed() )
        {
            static u32 suKerbCarCarLines = 0u;
            if ( BrnPhysics::Vehicle::KerbProbeTake( suKerbCarCarLines, "[kerb-imp]" ) )
            {
                const Vector3 lPos = lThisBody.GetPosition();
                *CgsDev::Log::gpDebugPrint
                    << "[kerb-imp] f " << BrnPhysics::Vehicle::guKerbProbeFrame
                    << " CARCAR sensor " << liSensorIndex
                    << " other " << static_cast<u32>( reinterpret_cast<u64>( lContact.mpOtherVehicle ) )
                    << " iter " << lvfIteration.x
                    << " n " << lContact.mNormal.x << " " << lContact.mNormal.y << " " << lContact.mNormal.z
                    << " pA " << lContact.mPointOnA.x << " " << lContact.mPointOnA.y << " " << lContact.mPointOnA.z
                    << " closing " << lfClosingSpeed
                    << " shapedMag " << lfImpulseLength
                    << " dir " << lImpulseUnit.x << " " << lImpulseUnit.y << " " << lImpulseUnit.z
                    << " carPos " << lPos.x << " " << lPos.y << " " << lPos.z
                    << "\n";
            }
        }
        // ---- end [kerb-imp] car-car leg ------------------------------------------------------------

        // This car: apply the impulse as-is at the contact (sensor liSensorIndex), adding to the spy.
        DeformationSensor* lpSensor = GetDeformationSensor(liSensorIndex);
        ApplySensorImpulse(lvfTimeStep, lContact, lParams, lRelativeMotion, lImpulseUnit,
                           lvfShapedMagnitude, lpSensor, /*lbAddToSpy*/ true,
                           /*lbUseNormalScaledFriction*/ true);

        // The other car: apply the reversed contact (A<->B, normal negated) with the negated impulse
        // and its own inverse-inertia term -- the equal-and-opposite half of the shunt.
        StoredImpulseContact lReverseContact;
        lContact.GetInverse(lReverseContact);
        // ⭐⭐⭐ THE OWNERSHIP HALF OF THE INVERSE -- ASM-ATTESTED, AND IT WAS MISSING (2026-09-03).
        // GetInverse is an inline member of StoredImpulseContact whose only argument is the output
        // record (DWARF BrnDeformationSensor.h:68), so it structurally CANNOT name the attacker:
        // the contact record holds one vehicle pointer, "the other one". The console therefore
        // writes the attacker's identity HERE, at the call site, from the enclosing frame:
        //
        //     0x82624C24  mr   r30, r3                     ; r30 = this  (the attacking car)
        //     0x82625100  addi r11, r22, 0xF               ; r22 = liSensorIndex
        //     0x82625104  mulli r11, r11, 0x1B0            ; 432-byte DeformationSensor stride
        //     0x82625114  add  r28, r11, r30               ; r28 = &this->maDeformationSensors[idx]
        //     0x8262511C  mr   r6, r28                     ; ...which is ALSO arg 4 of call #1
        //     ---- the reversed record is built at var_1D0 (sp+0xE0) ----
        //     0x82625188  stw  r30, var_1A0(r1)  ; sp+0x110 == reverse +0x30 == mpOtherVehicle
        //     0x8262518C  stw  r28, var_19C(r1)  ; sp+0x114 == reverse +0x34 == mpOtherSensor
        //
        // ⛔ SO THE CARRY-OVER WAS THE DEFECT, AND IT WAS OBSERVABLE. With the fields copied
        // through, the victim's record named the VICTIM as its own attacker -- measured across
        // five runs and 12,891 traffic contact rows: gid 587 -> oGid 587 (462 rows), 239 -> 239,
        // 404 -> 404, 452 -> 452, and ZERO rows anywhere with owner 2 / oOwner 1. Both halves of
        // each collision are logged on the same present with mirrored sensor directions, so they
        // were provably one impact seen twice.
        // ⇒ Consequence: ApplySensorImpulse's showtime VICTIM arm -- @0x82607D78..0x82607DD0, i.e.
        //   `lwz r11,0x30(contact)` (mpOtherVehicle) -> `lwz r3,0x194C(r11)` -> vtbl+0x10
        //   IsPlayerVehicleInShowtime, `|| lbz r11,0x672E(r11)` HasBouncedThisFrame, then the x15
        //   `vmulfp128` -- read BOTH of those off the victim itself. Self-referential on both
        //   terms, so the arm could never fire on a car-car hit. This is the fix that arms it.
        // ⚠️ Note what the asm does NOT settle: whether GetInverse ITSELF copies or swaps these two
        // fields is invisible, because it is inlined and these two stores kill whatever it wrote.
        // The body in BrnDeformationSensor.cpp is left copying them -- dead either way -- and the
        // observable console behaviour is exactly the two lines below.
        lReverseContact.mpOtherVehicle = this;        // 0x82625188 (stw r30 == this)
        lReverseContact.mpOtherSensor  = lpSensor;    // 0x8262518C (stw r28 == the sensor arg of call #1)
        lParams.mvfInverseInertia      = lvfInvInertiaB;
        lParams.mWorldImpulseDirection = lReverseContact.mNormal;
        // ⭐ The passer is RE-POINTED at the OTHER car for the reversed half -- the console repeats
        // the store verbatim with the other object's base: `addi r11, r3, 0x18E4 ; stw r11, var_D0`
        // @0x826251B0/B8. Each half of the shunt walks its OWN car's chain map.
        lParams.mpImpulsePasser        = &lOtherCar.mImpulsePasser;            // 0x826251B0/0x826251B8
        // The reversed half negates the (already unit) direction and keeps the same length -- the
        // console negates the stored unit vector in place (`vslw128 v0 ; vxor v3, v12, v0` at
        // 0x826251B4..0x826251C0, the sign-bit flip of the var_200 it just wrote the unit form to).
        lOtherCar.ApplySensorImpulse(lvfTimeStep, lReverseContact, lParams, vpu::Negate(lRelativeMotion),
                                     vpu::Negate(lImpulseUnit), lvfShapedMagnitude, lContact.mpOtherSensor,
                                     /*lbAddToSpy*/ false, /*lbUseNormalScaledFriction*/ true);

        return true;
    }

    // =============================================================================================
    // ApplyCarWorldImpulse @0x82624898 -- the CAR-vs-WORLD impulse orchestrator.
    //
    // ⭐⭐⭐ THE EXPORT HOLE IS CLOSED (2026-09-05, momentum wave). This body used to say "X360
    // @0x82624898 is an EXPORT HOLE; the PS3 body @0x746D68 is the authority". It is a hole in the
    // .ida-exports SET, not in the image: IDA knows the name (it is the function between
    // ResetSensors @0x82623D60, whose `b __restgprlr` is at 0x82624894, and ApplyCarCarImpulse
    // @0x82624C08) and the exporter simply wrote no JSON. All 219 instructions,
    // 0x82624898..0x82624C04, were read straight out of the image and are the authority for
    // everything below. The PS3 twin is demoted to cross-reference; where the two differ THIS
    // file now follows ARTIST, and every such difference is called out on its own line.
    //
    // HOW (so the next hole costs minutes, not hours): tools/re/ppcdis.py now decodes VMX128 via
    // tools/re/vmx_table.json, an EMPIRICAL opcode->mnemonic table built by
    // tools/re/build_vmx_table.py from IDA's own printed text across all 29,640 exported ARTIST
    // functions. Before that, capstone printed `.long 0x...` for every VMX128 word -- i.e. for
    // most of the physics code -- and a hole in this subsystem was effectively unreadable.
    //
    // ⚠️ TWO DECODE RULES THIS FUNCTION DEPENDS ON, both already in the tree and both re-confirmed
    // here: (a) for `lvx128`/`stvx128` the rA/rB fields are PLAIN 5-bit GPR fields (b11..b15 /
    // b16..b20) -- the VMX128 high-bit extension does NOT apply, so vmx128.py's "vB=107" is r11;
    // (b) the vector ARGUMENT registers on this ABI start at v1 (`vmr128 v123,v1` @0x826248B8 and
    // `vmr128 v122,v2` @0x826248C0 park lvfTimeStep and lvfIteration), which is what makes v122
    // the iteration in the shaping below.
    //
    // ARTIST FLOW, address by address:
    //   0x826248CC  B  = *(this+0x194C)            the attached VehiclePhysics; B+0x10 is its
    //                                              ExternalPhysicsBody base subobject
    //   0x826248EC  r        = lContact.mPointOnA - [B+0x40]           (body position)
    //   0x826248F4/FC/0x82624914
    //               relMotion= [B+0x50] + [B+0x60] x r                 (linVel + angVel x r)
    //   0x82624918  vmsum3fp128 dot(relMotion, mNormal)
    //   0x8262491C  vcmpgefp128. vs 0 -> `beq` @0x8262492C -> `li r3,0` : SEPARATING, return false
    //   0x82624950  GetVehicleWorldRestitution(this, lContact) -> r1+0xC0
    //   0x8262497C  ExternalPhysicsBody::CalculateCollisionImpulseWithInanimateObject(
    //                   sret=r1+0xB0, this=B+0x10, &impulse=r1+0xA0, &invInertia=r1+0xD0,
    //                   v1=mPointOnA, v2=relMotion, v3=mNormal, v4=restitution)
    //   0x82624990  the vtable+0x10 virtual on B  == IsPlayerVehicleInShowtime
    //                 (SetJustBounced @0x825B8D68 asserts on the string "mbPlayerCarInShowtime",
    //                  which is what settles the predicate's identity)
    //   0x826249A8  NOT showtime: impulse *= (lvfIteration + 1.0) * 0.5
    //                 `vcsxwfp128 v13, v124, 1` == 0.5 and `vcsxwfp128 v127, v124, 0` == 1.0, both
    //                 off `vspltisw128 v124, 1` @0x82624994 -- the 1.0/0.5 pair is IMAGE-READ, not
    //                 inherited from the sibling.  With UpdateContacts' hard-zero iteration
    //                 (9225f00e) this factor is exactly x0.5 on every world contact, always.
    //   0x826249C4  showtime: the landing gate, then the bounce boost -- see the block below
    //   0x82624AE8..0x82624BEC  build the WORLD ImpulseParams + normalise the shaped impulse
    //   0x82624BF0  ApplySensorImpulse(...); 0x82624BF4 `li r3, 1` -> return true
    //
    // ⭐ THE MAGNITUDE, WHICH IS WHAT THE OWNER'S "no momentum" QUESTION WAS ABOUT, IS THE
    // TEXTBOOK RIGID-BODY IMPULSE AND IT IS SOLVED IN AN EXPORTED FUNCTION, NOT HERE.
    // CalculateCollisionImpulseWithInanimateObject @0x8259C978 (91 instructions, exported) is:
    //     j = -(1 + e) (v_rel . n) / ( 1/m + n . ((I^-1 (r x n)) x r) )
    //     *sret = j ; *impulseOut = n * |j| ; *invInertiaOut = the DENOMINATOR (not an inertia)
    // read off its own asm: `vrefp v12,[this+0xD0]` + two Newton steps is 1/m from mfMass;
    // [this+0xA0/0xB0/0xC0] are the three mWorldInverseInertia rows; `vaddfp128 v0, v127, v4` then
    // `vxor` the sign bit is -(1+e); `vmsum3fp128 v8, normal, relMotion` is the closing speed.
    // Every one of those offsets is the seat ExternalPhysicsBody.h already committed, and the
    // reconstruction in ExternalPhysicsBody.cpp computes exactly that expression. So the
    // per-impulse magnitude in this build IS the console's arithmetic on the console's fields;
    // the only way it can diverge is through m / I^-1 / r / n / v_rel, not through the formula.
    // ⭐ AND THE RESTITUTION IS ZERO OUT OF SHOWTIME. GetVehicleWorldRestitution @0x825E0C78
    // returns `vspltisw v0, 0` on the whole non-showtime path (`bne` @0x825E0CB4 is the ONLY way
    // into the arm that loads a value). A wall hit is therefore perfectly inelastic by the
    // console's own rule -- there is no bounce term to be missing.
    // =============================================================================================
    namespace
    {
        // ⭐ RECOVERED FROM THE ARTIST IMAGE 2026-09-05, not from the PS3 initializer. Both are
        // dynamic-init splat slots (they read 0.0 out of the image BY DEFINITION); findinit.py
        // names the writer thunk and ppcdis.py reads the float it copies:
        //   unk_82FB81E0 <- 0x82C5D408..0x82C5D42C <- flt_82001D9C == 2.0   the bounce-boost SCALE
        //   unk_82FB9E80 <- 0x82C5D458..0x82C5D47C <- flt_82004C88 == 8.0   the minimum bounce POWER
        // The PS3-derived values were right; the SHAPE they were applied in was not (see below).
        const f32 KF_SHOWTIME_BOUNCE_BOOST_SCALE     = 2.0f;   // unk_82FB81E0
        const f32 KF_SHOWTIME_MIN_WORLD_BOUNCE_POWER = 8.0f;   // unk_82FB9E80
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
            lContact.mPointOnA, lRelativeMotion, lContact.mNormal, lvfRestitution, &lImpulse, &lvfInvInertia);

        if ( lpVehicle->IsPlayerVehicleInShowtime() )   // 0x82624990, the vtable +0x10 virtual
        {
            // 0x826249C4..0x82624A40 -- the LANDING gate, decoded operand for operand. The console
            // builds sign(n.y) with a vcmpgtfp/vcmpgefp/vsel pair, multiplies it by n.y twice and
            // compares the product against n.x*n.x + n.z*n.z:
            //     |n.y| * n.y  >=  n.x^2 + n.z^2
            // which is true only for an UPWARD-dominant normal (a landing), n.y == 0 aside. The
            // form is transcribed rather than re-derived as `n.y > 0 && ...`, because those two
            // spellings differ on a degenerate all-zero normal.
            const Vector3& lrN = lContact.mNormal;
            const f32 lfSignY  = (lrN.y > 0.0f) ? 1.0f : ((lrN.y < 0.0f) ? -1.0f : 0.0f);
            if ( (lfSignY * lrN.y) * lrN.y >= (lrN.x * lrN.x + lrN.z * lrN.z) )
            {
                BrnPhysics::Vehicle::RaceCarPhysics* lpRaceCar = AsRaceCarPhysics();
                if ( lpRaceCar != nullptr )
                {
                    // ⚠️ OPERAND CORRECTED 2026-09-05 against ARTIST: `lvx128 v1, r0, r29`
                    // @0x82624A50 loads the contact base (r29 == &lContact, i.e. +0x00 ==
                    // mPointOnA), NOT r30 (== contact+0x20 == mNormal), which is live in the same
                    // register window. The old body passed the normal. SetJustBounced stores that
                    // vector into the lbBounceBoosting record at 0x82FB8490 (`stvx128 v127, r11,
                    // 0x10` @0x825B8D9C), so it is a bounce POSITION.
                    lpRaceCar->SetJustBounced(lContact.mPointOnA, false, false, EntityId{ 0u });
                    if ( lpRaceCar->ShouldBounceBoostNextImpact() )
                    {
                        // ⚠️⚠️ SHAPE CORRECTED 2026-09-05 against ARTIST -- the numbers were right
                        // and the arithmetic they were used in was not. The old body did
                        // `mag = max(mag * 2.0, 8.0)` on the SCALAR magnitude. The console
                        // (0x82624A78..0x82624AE4) scales the IMPULSE VECTOR and floors only its
                        // Y lane, against 8.0 times the body's MASS:
                        //     0x82624A98  vmulfp128 v0, v12, v0     ; impulse *= 2.0 (unk_82FB81E0)
                        //     0x82624AB0  lvx128 v12, r10, r9       ; r9 == 0xE0 -> B+0xE0, which is
                        //                                           ; ExternalPhysicsBody+0xD0 == mfMass
                        //     0x82624AB4  vmulfp128 v13, v13, v12   ; 8.0 (unk_82FB9E80) * mass
                        //     0x82624ACC/D0  fsubs + fsel           ; max(impulse.y, 8.0*mass)
                        //     0x82624AD4  stfs f0, 0xB4(r1)         ; only lane 1 is written back
                        // i.e. a showtime landing is guaranteed 8 m/s worth of UPWARD momentum. A
                        // bare 8.0 was not merely a different number, it was a different unit
                        // (~8 N.s instead of ~11,200 N.s on a 1400 kg car).
                        lImpulse = vpu::Mult(lImpulse, KF_SHOWTIME_BOUNCE_BOOST_SCALE);
                        const f32 lfMinBounce =
                            KF_SHOWTIME_MIN_WORLD_BOUNCE_POWER * GetVehicleBody().GetMass().x;
                        if ( lImpulse.y < lfMinBounce )
                        {
                            lImpulse.y = lfMinBounce;   // the fsel
                        }
                    }
                }
            }
            // ⭐ NOTE THE ASYMMETRY, and it is the console's: the showtime arm does NOT apply the
            // (iteration+1)*0.5 shaping. 0x826249A8..0x826249C0 (the `bne`-not-taken arm) is the
            // only place the factor exists; the showtime arm branches straight to 0x82624AE8.
        }
        else
        {
            // 0x826249A8..0x826249BC, image-read immediates: impulse *= (iteration + 1.0) * 0.5.
            //     vcsxwfp128 v127, v124, 0  == float(1) / 2^0 == 1.0
            //     vcsxwfp128 v13,  v124, 1  == float(1) / 2^1 == 0.5
            //     vaddfp128  v12, v122(iteration), v127 ; vmulfp128 v13, v12, v13
            //     vmulfp128  v10, v0(impulse), v13
            // UpdateContacts feeds iteration == 0 unconditionally (9225f00e: `vspltisw128 v126,0`
            // hoisted out of the apply loop), so this is exactly x0.5 on every world contact.
            const f32 lfShape = (lvfIteration.x + 1.0f) * 0.5f;
            lImpulse = vpu::Mult(lImpulse, lfShape);
        }

        // The WORLD-contact params block. ⭐ RE-READ OFF ARTIST 2026-09-05: the console's params
        // base is r1+0xE0 (`addi r5, r1, 0xE0` @0x82624B30) and ApplySensorImpulse memcpys 0xC0
        // bytes from it (`li r5, 0xC0` @0x826078F8), which pins sizeof(ImpulseParams) == 192 and
        // makes every store below checkable by displacement. ARTIST writes EXACTLY SEVEN fields:
        //     0x82624B10  stvx128 -> r1+0x100  (+0x20)  mImpulsePosition   = lContact.mPointOnA
        //     0x82624B4C  stw   0 -> r1+0x130  (+0x50)  mePositionSpace    = WORLD_SPACE
        //     0x82624B44  stvx128 -> r1+0x140  (+0x60)  mvfInverseInertia
        //     0x82624B6C  stvx128 -> r1+0x150  (+0x70)  mvfTimeStep        (v123 == the v1 arg)
        //     0x82624B84  stvx128 -> r1+0x160  (+0x80)  mvfVelocityAlongNormal
        //     0x82624B64  stw     -> r1+0x190  (+0xB0)  mpImpulsePasser    = this+0x18E4
        //     0x82624B80  stb   1 -> r1+0x198  (+0xB8)  mbWorldContact     = true
        //
        // ⛔ AND THREE STORES THIS BODY USED TO MAKE THAT THE CONSOLE DOES NOT -- removed, with the
        // reason, because "the caller sets it" was a load-bearing claim elsewhere in the tree:
        //   * mvfImpulseMagnitude (+0x00) and mWorldImpulseDirection (+0x10). ARTIST leaves both as
        //     stack garbage here; ApplySensorImpulse overwrites them from its OWN arguments the
        //     moment it starts (`vmr128 v120,v4` / `vmr128 v116,v3` in its prologue, then the two
        //     assignments at the head of its params seed), and further uses its local copy of both
        //     slots as scratch (`stw r30, var_290` @0x82607C8C is +0x00 holding a sensor pointer).
        //     Keeping the caller store was therefore harmless but WRONG -- and specifically it
        //     wrote the UNSHAPED solved magnitude into a field the shaped one owns, which is the
        //     exact shape a 2x momentum bug would have taken if anything downstream had read it.
        //   * meAbsorptionSet (+0xB4). ApplySensorImpulse reads it from `this+0x675C` itself
        //     (`lwz r10, 0x675C(r17)` @0x8260793C). The note in BrnDeformableObject_Update.cpp
        //     saying "the world path set it in the caller, so this is the console's own single
        //     point of truth for both paths" is REFUTED by the asm: neither caller sets it, and
        //     ApplySensorImpulse is the single point of truth for both.
        ImpulseParams lParams;
        lParams.mvfInverseInertia      = lvfInvInertia;                        // 0x82624B44 -> +0x60
        lParams.mvfTimeStep            = lvfTimeStep;                          // 0x82624B6C -> +0x70
        lParams.mePositionSpace        = rw::physics::WORLD_SPACE;             // 0x82624B4C -> +0x50
        lParams.mImpulsePosition       = lContact.mPointOnA;                   // 0x82624B10 -> +0x20
        lParams.mbWorldContact         = true;                                 // 0x82624B80 -> +0xB8
        // +0x80 mvfVelocityAlongNormal @0x82624B84. The value is `vmsum3fp128 v11, v125, v11`
        // @0x82624AF8 (dot3(relMotion, mNormal)) passed through `vandc v12, v11, v12` @0x82624B3C
        // where v12 == vslw(vspltisw -1) == 0x80000000, i.e. the SIGN BIT IS CLEARED. So the field
        // is |closing speed|. The function has already returned false for a separating contact, so
        // the dot is negative here and the abs is what makes ApplySensorImpulse's line-319 `>= 0`
        // assert hold. (The PS3 twin spells the same thing with vsldoi/vaddfp.)
        {
            const f32 lfClosing    = vpu::Dot(lRelativeMotion, lContact.mNormal);
            const f32 lfAbsClosing = (lfClosing < 0.0f) ? -lfClosing : lfClosing;
            lParams.mvfVelocityAlongNormal =
                VecFloat{ lfAbsClosing, lfAbsClosing, lfAbsClosing, lfAbsClosing };
        }
        // +0xB0 mpImpulsePasser -- `addi r10, r31, 0x18E4` @0x82624B48 then `stw r10, 0x190(r1)`
        // @0x82624B64 (the PS3 twin spells the identical pair at 0x746F6C/0x746F98).
        // ⭐⭐ THE CHAIN'S HANDLE: this is the ONLY route by which an ordinary world contact reaches
        // the momentum bank (sensor absorbs -> PassOnImpulse -> slot 0 == &mVehicleBody ->
        // VehicleRigidBody::RecievePassedOnImpulse -> VehiclePhysics::ApplyWallContactImpulse ->
        // ExternalPhysicsBody::AddWorldSpaceImpulse).
        lParams.mpImpulsePasser        = &mImpulsePasser;                      // 0x82624B48/B64

        // ⭐⭐⭐ THE NORMALIZE, now read off ARTIST rather than the PS3 twin. 0x82624AF4 takes
        // `vmsum3fp128 v0, v10, v10` == |shaped impulse|^2, 0x82624B40/0x82624B88 open TWO
        // independent `vrsqrtefp` pipelines over it, each refined twice (the vnmsubfp128/vmaddfp
        // pairs at 0x82624B98..0x82624BE4), and the two results publish the pair the callee wants:
        //     0x82624BD0  vmulfp128 v3, v10, v12   ; impulse * rsqrt  == THE UNIT DIRECTION (arg v3)
        //     0x82624BE8  vmulfp128 v0, v0,  v13   ; |imp|^2  * rsqrt == THE LENGTH          (arg v4)
        //     0x82624BEC  vsel v4, v0, v9, v5      ; v5 == vcmpeqfp128(0, |imp|^2) -> zero guard
        // vpu::NormalizeReturnMagnitude is the house model of exactly that double publish, zero
        // guard included. ⭐ Note the LENGTH is taken from the SHAPED impulse, so the x0.5 above
        // reaches the sensor through this magnitude and not only through the direction.
        Vector3   lImpulseUnit;
        const f32 lfImpulseLength = vpu::NormalizeReturnMagnitude(lImpulse, lImpulseUnit);
        const VecFloat lvfShapedMagnitude =
            VecFloat{ lfImpulseLength, lfImpulseLength, lfImpulseLength, lfImpulseLength };

        // ---- [kerb-imp] one line per world impulse the solver applies (the [kerb] banner in
        //      BrnVehicleManager_ValidateRaceCarWorldContact.cpp). The RESULT the two culls decide:
        //      the contact normal the sensor stored (rewritten or not), the point, the closing
        //      speed, the solved magnitude, the shaped magnitude/direction handed to the sensor.
        if ( BrnPhysics::Vehicle::KerbProbeArmed() )
        {
            static u32 suKerbImpLines = 0u;
            if ( BrnPhysics::Vehicle::KerbProbeTake( suKerbImpLines, "[kerb-imp]" ) )
            {
                // ⭐ 2026-09-05: `m` and `k` added so the line carries EVERY term of the console's
                // own closed form and the reader can check the identity instead of trusting it:
                //     solved == -(1 + rest) * closing / k          (k == the CalculateCollision...
                //                                                   denominator, i.e. `invI` here,
                //                                                   which is 1/m + n.((I^-1(rxn))xr))
                //     shapedMag == |solved| * 0.5                  (the (iter+1)*0.5 shaping)
                // A row where those two identities hold is a row where the magnitude IS ARTIST's
                // arithmetic; a row where they fail names which input diverged.
                const Vector3 lPos = lpVehicle->GetPosition();
                const f32 lfKerbClosing = vpu::Dot( lRelativeMotion, lContact.mNormal );
                const f32 lfKerbMass    = GetVehicleBody().GetMass().x;
                *CgsDev::Log::gpDebugPrint
                    << "[kerb-imp] f " << BrnPhysics::Vehicle::guKerbProbeFrame
                    << " sensor " << liSensorIndex
                    << " iter " << lvfIteration.x
                    << " n " << lContact.mNormal.x << " " << lContact.mNormal.y << " " << lContact.mNormal.z
                    << " pA " << lContact.mPointOnA.x << " " << lContact.mPointOnA.y << " " << lContact.mPointOnA.z
                    << " closing " << lfKerbClosing
                    << " rest " << lvfRestitution.x
                    << " m " << lfKerbMass
                    << " k " << lvfInvInertia.x
                    << " solved " << lvfImpulseMagnitude.x
                    << " predicted " << ( -( 1.0f + lvfRestitution.x ) * lfKerbClosing / lvfInvInertia.x )
                    << " invI " << lvfInvInertia.x
                    << " shapedMag " << lfImpulseLength
                    << " dir " << lImpulseUnit.x << " " << lImpulseUnit.y << " " << lImpulseUnit.z
                    << " carPos " << lPos.x << " " << lPos.y << " " << lPos.z
                    << "\n";
            }
        }
        // ---- end [kerb-imp] ---------------------------------------------------------------------

        // ---- [worldimp] PC bring-up instrument -- DELETE WHEN the wall test is banked -----------
        // OPT-IN (BRN_IMPULSE_PROBE=1). The UPSTREAM question the downstream probes cannot answer:
        // does the world-contact impulse orchestrator run AT ALL for a vertical wall face, or only
        // for the ground? One line per world contact that reaches the solver, with the contact
        // NORMAL (the floor's is ~+Y, a wall's is horizontal) and the solved magnitude.
        // Two windows, same discipline as the others: an opening window plus horizontal normals.
        // ⚠️ SCOPE OF THE EVIDENCE THIS GIVES: the probe sits AFTER the separating-contact early-out
        // and after the solve, so "no WALLFACE line" proves no wall-normal contact reached HERE --
        // it does not by itself prove none ENTERED the function. (For the campaign's wall run the
        // distinction is moot: the car arrives at -30 m/s into a normal with |n.z| == 0.86, which
        // cannot be classified separating. Stated anyway so the next leg does not over-read it.)
        {
            static s32 siWorldProbe = -1;
            if ( siWorldProbe < 0 )
            {
                const char* lpcEnv = getenv( "BRN_IMPULSE_PROBE" );
                siWorldProbe = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
            }
            static u32 suWorld      = 0;
            static u32 suHorizontal = 0;
            ++suWorld;
            const f32  lfAbsNY  = ( lContact.mNormal.y < 0.0f ) ? -lContact.mNormal.y
                                                                : lContact.mNormal.y;
            const bool lbWallish = ( lfAbsNY < 0.6f );   // the [wall] probe's own discriminator
            if ( siWorldProbe == 1 && CgsDev::Log::gpDebugPrint != 0
                 && ( suWorld <= 30u || ( lbWallish && ++suHorizontal <= 600u ) ) )
            {
                const Vector3 lPos = lpVehicle->GetPosition();
                *CgsDev::Log::gpDebugPrint
                    << "[worldimp] n " << static_cast<s32>(suWorld)
                    << ( lbWallish ? " WALLFACE" : " floor" )
                    << " sensor " << liSensorIndex
                    << " n " << lContact.mNormal.x << " " << lContact.mNormal.y
                    << " " << lContact.mNormal.z
                    << " mag " << lfImpulseLength
                    << " pos " << lPos.x << " " << lPos.y << " " << lPos.z
                    << "\n";
            }
        }

        // 0x82624BF0. The GPR argument list is pinned by the asm and by the sibling: r7 and r8 are
        // `li r7, 1` @0x82624B2C and `li r8, 0` @0x82624B0C here, while ApplyCarCarImpulse's
        // reversed half sets the pair the other way round (`li r7, 0` @0x82625160 / `li r8, 1`
        // @0x82625150) -- which is exactly the (lbAddToSpy=false, lbUseNormalScaledFriction=true)
        // this TU already committed for that call, so the mapping is corroborated, not assumed.
        // ⚠️ CORRECTED 2026-09-05: lbUseNormalScaledFriction is FALSE on the world path; this body
        // passed true. It cannot change behaviour on ARTIST either -- r8 is never read anywhere in
        // ApplySensorImpulse @0x826078B0 (the register does not appear once in its 699-instruction
        // listing), so the parameter is DEAD in this build of the console too. Corrected because it
        // is now proved, and flagged here so nobody re-derives the flag as a momentum suspect.
        // The sensor argument is `(liSensorIndex + 15) * 0x1B0 + this` (`addi r11, r26, 0xF` /
        // `mulli r11, r11, 0x1B0` / `add r6, r11, r31` @0x82624B00..0x82624B28) == the
        // GetDeformationSensor(liSensorIndex) this call already used: maDeformationSensors is at
        // this + 15*432 and the record stride is 432.
        ApplySensorImpulse(lvfTimeStep, lContact, lParams, lRelativeMotion, lImpulseUnit,
                           lvfShapedMagnitude, GetDeformationSensor(liSensorIndex),
                           /*lbAddToSpy*/ true, /*lbUseNormalScaledFriction*/ false);
        return true;
    }

}
}
