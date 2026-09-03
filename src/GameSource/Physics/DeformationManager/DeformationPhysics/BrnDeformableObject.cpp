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
    // ApplyCarWorldImpulse -- the CAR-vs-WORLD impulse orchestrator.
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
    //   mag = body.CalculateCollisionImpulseWithInanimateObject(worldPoint, relMotion, normal, restitution,
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
            lContact.mPointOnA, lRelativeMotion, lContact.mNormal, lvfRestitution, &lImpulse, &lvfInvInertia);

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
        //
        // TWO DROPPED STORES, from the PS3 twin @0x746D68 (the X360
        // is an export hole). Params base there is var_190; the +0xB8/+0x50 stores below it pin the
        // base exactly (`stb 1, var_D8` == mbWorldContact and `stw 0, var_140` == mePositionSpace,
        // which are 8 and 0x40 off the two fields this block already set correctly).
        ImpulseParams lParams;
        lParams.mvfImpulseMagnitude    = lvfImpulseMagnitude;
        lParams.mWorldImpulseDirection = lContact.mNormal;
        lParams.mvfInverseInertia      = lvfInvInertia;                        // 0x746F78 -> +0x60
        lParams.mvfTimeStep            = lvfTimeStep;                          // 0x746F90 -> +0x70
        lParams.mePositionSpace        = rw::physics::WORLD_SPACE;             // 0x746F80 -> +0x50
        lParams.mImpulsePosition       = lContact.mPointOnA;                   // 0x746F70 -> +0x20
        lParams.mbWorldContact         = true;                                 // 0x746F88 -> +0xB8
        lParams.meAbsorptionSet        = meAbsorptionSet;
        // +0x80 mvfVelocityAlongNormal @0x747010 (`li r0,0x150 ; stvx v1, r1, r0`). v1 is the
        // vsldoi/vaddfp horizontal sum of relativeMotion*normal -- i.e. dot3(relMotion, normal) --
        // passed through `vandc v1, v1, v9` @0x74700C where v9 == vslw(vspltisw -1) == 0x80000000,
        // which CLEARS THE SIGN BIT. So the field is |closing speed|. The function has already
        // returned false for a separating contact, so the dot is negative here and the abs is what
        // makes the head's line-319 `>= 0` assert hold. Recomputed by name rather than re-derived.
        {
            const f32 lfClosing    = vpu::Dot(lRelativeMotion, lContact.mNormal);
            const f32 lfAbsClosing = (lfClosing < 0.0f) ? -lfClosing : lfClosing;
            lParams.mvfVelocityAlongNormal =
                VecFloat{ lfAbsClosing, lfAbsClosing, lfAbsClosing, lfAbsClosing };
        }
        // +0xB0 mpImpulsePasser @0x746F6C/0x746F98 (`addi r11, this, 0x18E4 ; stw r11, var_E0`).
        // ⭐⭐ THE CHAIN'S HANDLE, and the store the tree had never made: this is the ONLY route by
        // which an ordinary world contact reaches the momentum bank (sensor absorbs -> PassOnImpulse
        // -> slot 0 == &mVehicleBody -> VehicleRigidBody::RecievePassedOnImpulse ->
        // VehiclePhysics::ApplyWallContactImpulse -> ExternalPhysicsBody::AddWorldSpaceImpulse).
        lParams.mpImpulsePasser        = &mImpulsePasser;                      // 0x746F6C/0x746F98

        // ⭐⭐⭐ THE SAME DROPPED NORMALIZE as the car-car sibling (see its long note). PS3
        // @0x746FF8..0x74703C: `vrsqrtefp` over the shaped impulse's |imp|^2, two Newton refines,
        // then the double publish -- `vmaddfp v8,...` == imp * rsqrt (the UNIT DIRECTION, also
        // written back through r28) and `vmaddfp v5,...` == |imp|^2 * rsqrt (the LENGTH), with
        // `vsel v5, v5, v31, v7` guarding the zero-length case. `vmr v4, v8` seats the unit vector
        // as ApplySensorImpulse's arg 3 (PS3 vector args start at v2) and v5 as its arg 4.
        // vpu::NormalizeReturnMagnitude is the house model of exactly that one-pipeline double
        // publish, zero guard included.
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
                const Vector3 lPos = lpVehicle->GetPosition();
                *CgsDev::Log::gpDebugPrint
                    << "[kerb-imp] f " << BrnPhysics::Vehicle::guKerbProbeFrame
                    << " sensor " << liSensorIndex
                    << " iter " << lvfIteration.x
                    << " n " << lContact.mNormal.x << " " << lContact.mNormal.y << " " << lContact.mNormal.z
                    << " pA " << lContact.mPointOnA.x << " " << lContact.mPointOnA.y << " " << lContact.mPointOnA.z
                    << " closing " << vpu::Dot( lRelativeMotion, lContact.mNormal )
                    << " rest " << lvfRestitution.x
                    << " solved " << lvfImpulseMagnitude.x
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

        ApplySensorImpulse(lvfTimeStep, lContact, lParams, lRelativeMotion, lImpulseUnit,
                           lvfShapedMagnitude, GetDeformationSensor(liSensorIndex),
                           /*lbAddToSpy*/ true, /*lbUseNormalScaledFriction*/ true);
        return true;
    }

}
}
