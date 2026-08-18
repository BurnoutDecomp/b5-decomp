// LANDED 2026-08-18 (wave Q4 integration) from scratchpad/waveQ2/parked/PropManager_08_ApplyPropRaceCarCollisionImpulse.cpp:
// both blockers named in the banner below are closed (RaceCarPhysics::AddPropCollisionImpulse is now the header inline it is on X360;
// KVF_MAX_PROP_SPEED_MPS is declared+seated). The banner is kept as the derivation record.
// ==========================================================================================
// PARKED -- COMPLETE BODY, NOT MOUNTED.
//
// BrnPhysics::Props::PropManager::ApplyPropRaceCarCollisionImpulse  (X360 0x825E3560).
// Wave Q (breakable props / smash gates), ROUND 2, group 08.  2026-08-18.
//
// Intended home: b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_08.cpp
// (drop the two functions into the same namespace block; the include set below is complete
//  and was gate-proved -- see "PROOF" at the bottom).
//
// ------------------------------------------------------------------------------------------
// WHAT UNBLOCKS THIS -- EXACTLY TWO LINES, BOTH IN HEADERS THIS PARTFILE MAY NOT EDIT.
// (Both were MEASURED, not guessed: the verbatim body below compiles with EXACTLY these two
//  diagnostics and no others -- see PROOF.)
//
//  (1) b5-decomp/src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h
//      -- add to a PUBLIC section:
//
//          void AddPropCollisionImpulse( Vector3 lImpulse );
//
//      Grounding: DecFIGS dwarfdump RaceCarPhysics.h:340 declares
//      `void AddPropCollisionImpulse(Vector3);`, and the PS3 compile unit
//      references/DecFIGS/dwarfdump/_compile/BrnPhysicsUnity2.cpp:19688 has the out-of-line
//      body `void BrnPhysics::Vehicle::RaceCarPhysics::AddPropCollisionImpulse(const
//      rw::math::vpu::Vector3 lImpulse)`. ARTIST has NO symbol for it -- the X360 compiler
//      INLINED it, and this function's own last three instructions ARE that inline:
//          0x825E35E8  addi   r10, r4, 0x13F0        <- &mPropCollisionImpulseSum
//          0x825E35FC  lvx128 v8,  r0, r10
//          0x825E361C  vmaddfp v0, v1, v8, v0        <- sum += lNormal * lImpulseMagnitude
//          0x825E3620  stvx128 v0, r0, r10
//      RaceCarPhysics.h:589 already declares that member BY NAME (`Vector3
//      mPropCollisionImpulseSum;  // @+0x13F0`, X360-pinned by ApplyPropCollisionImpulseSum's
//      own `addi r31,this,0x13F0`) -- but it is PRIVATE (the private section runs :483..:636)
//      and PropManager is not a friend (the only friend is `class VehicleManager`, :47).
//      So the accessor, not the member, is what has to be added. Whether it is written as a
//      one-line header inline (`mPropCollisionImpulseSum += lImpulse;`, which is what the
//      console emits) or as an out-of-line body is the RaceCarPhysics owner's call.
//      ⚠️ DO NOT reach the member by offset instead: +0x13F0 is a CONSOLE offset and
//      RaceCarPhysics is explicitly NOT byte-pinned on the host (RaceCarPhysics.h:16, :506).
//
//  (2) b5-decomp/src/GameSource/Physics/PropManager/BrnPropManager.h
//      -- add beside the other namespace-scope prop tuning constants (the block at :103..:182):
//
//          extern VecFloat KVF_MAX_PROP_SPEED_MPS;                // X360 0x82FB9470
//
//      ...and its zero definition beside the others in BrnPropManager.cpp (:238..:252):
//
//          ::VecFloat KVF_MAX_PROP_SPEED_MPS;                     // X360 0x82FB9470
//
//      Grounding, and the one INFERENCE in it, stated plainly:
//        * MEASURED: the only rodata/bss datum this body loads is `unk_82FB9470`
//          (0x825E35C8/0x825E35D4/0x825E35E4). A ripgrep of the whole
//          .ida-exports/BURNOUT_X360_ARTIST.XEX/ directory for "82FB9470" returns exactly ONE
//          file -- 0x825E3560.json, this function. There is no writer and no other reader.
//        * MEASURED (headless IDA 9.3 byte read of IDA Files/BURNOUT_X360_ARTIST.XEX.i64,
//          this wave): 0x82FB9470 holds 16 zero bytes == (0.0f, 0.0f, 0.0f, 0.0f). The same
//          read returned all-zero for 0x82FB9450 (KVF_ANTI_HERD_SIDE_SCALE) and 0x82FB9490
//          (KVF_MAX_ANGULAR_ACCELERATION), which is the already-recorded zero-page finding
//          (BrnPropManager.cpp:186-198) independently re-confirmed. So the value is NOT
//          recovered and NOTHING is invented for it -- see the CONSOLE-VALUE WARNING below.
//        * INFERENCE (address -> name): the DecFIGS file-scope dump for this exact .cpp lists
//          `// BrnPropManager.cpp:1678   VecFloat KVF_MAX_PROP_SPEED_MPS;` and this function's
//          own DWARF scope is `// BrnPropManager.cpp:1685`. :1678 is the file-scope constant
//          immediately preceding :1685, it is the only DWARF constant of that name, it is a
//          VecFloat (matching the 16-byte `lvx128` load), and its semantics -- a maximum prop
//          SPEED -- fit the one use, a speed-derived falloff. That proximity + uniqueness +
//          type + semantic fit is the whole of the argument; the binary does not name it.
//        * ⚠️ AND A DIMENSIONAL ODDITY, REPORTED NOT "FIXED": the constant is subtracted from
//          `vmsum3fp128 v0,v0,v0` == the prop's speed SQUARED (0x825E35C4), while the DWARF
//          name says MPS (metres per second). Either the source name is loose or the source
//          stores the square in it. Do not "correct" the reconstruction to Magnitude() to make
//          the name fit -- the asm has no sqrt.
//
// ------------------------------------------------------------------------------------------
// ⚠️⚠️ CONSOLE-VALUE WARNING (the same class of finding as BrnPropManager.cpp:222-244).
// With KVF_MAX_PROP_SPEED_MPS == 0 -- which is what the shipped image holds --
// `Clamp(0 - lSquaredPropSpeed, 0, 1)` is ZERO for every prop (a squared magnitude is never
// negative), so lImpulseMagnitude is zero and the car takes NO impulse back from any prop it
// smashes, ever. That is a statement about the missing dynamic initialiser, not about this
// reconstruction. Do NOT pick a number to make it "work" -- report it.
//
// ------------------------------------------------------------------------------------------
// THE 50 INSTRUCTIONS  (COUNTED: (0x825E3624 - 0x825E3560)/4 + 1 == 50).
//
// Register map, MEASURED from the body (there is no prologue -- this is a leaf, `blr` at
// 0x825E3624 with no frame set up):
//   r3 = this          (`lbz r7,0x49(r3)` == mbUseOverides; `lfs f0,0x4C(r3)` == mfMassOverride)
//   r4 = lpRaceCar     (+0x10 mTransform, +0x40 mTransform.wAxis, +0x50 mLinearVelocity,
//                       +0x60 mAngularVelocity, +0x13F0 mPropCollisionImpulseSum)
//   r5 = lpProp        (+0x40 mLinearVelocity)
//   r6 = lpType        (+0x38 mfMass)
//   v1 = lNormal, v2 = lPointOnCar     <- AGENTS.md gotcha 3: the two Vector3s ride v1/v2 and
//                                         consume NO GPR slot, which is why lpType lands in r6.
//
//   0x825E3560  addi      r11, r4, 0x10          <- &lpRaceCar->mTransform
//   0x825E3564  vspltisw  v11, 0                 <- the zero used by both clamps
//   0x825E356C  lbz       r7,  0x49(r3)          <- mbUseOverides
//   0x825E3574  lfs       f0,  0x38(r6)          <- lpType->GetMass()
//   0x825E3580  lvx128    v13, r11, r9(0x30)     <- mTransform.wAxis  (0x10+0x30 == +0x40)
//   0x825E3584  vsubfp    v12, v2,  v13          <- r = lPointOnCar - carPos
//   0x825E3588  lvx128    v13, r11, r8(0x50)     <- mAngularVelocity  (0x10+0x50 == +0x60)
//   0x825E3590  lvx128    v10, r11, r10(0x40)    <- mLinearVelocity   (0x10+0x40 == +0x50)
//   0x825E3594  lvx128    v0,  r5,  r10(0x40)    <- lpProp->mLinearVelocity
//   0x825E3598  vmr       v8,  v12
//   0x825E359C  vpermwi128 v12, v12, 0x63        \
//   0x825E35A0  vmulfp128 v13, v13, v12           |  the SDK one-shuffle CROSS PRODUCT:
//   0x825E35A4  vnmsubfp  v13, v9,  v13, v8       |  t = w*r.yzx - w.yzx*r ; cross = t.yzx
//   0x825E35A8  vpermwi128 v13, v13, 0x63        /   (0x63 == lane order 1,2,0,3 == yzx)
//   0x825E35AC  vaddfp    v13, v13, v10          <- + mLinearVelocity
//                                                   ==> the INLINED ExternalPhysicsBody::
//                                                       GetLocalVelocity(pt, WORLD_SPACE)
//   0x825E35B0  vsubfp    v13, v0,  v13          <- lRelativeVelocity = propVel - carPointVel
//   0x825E35B4  vmsum3fp128 v13, v13, v1         <- Dot(lRelativeVelocity, lNormal), broadcast
//   0x825E35B8  vmaxfp    v13, v11, v13          <- Max(0, that)
//   0x825E35BC  beq       cr6, 0x825E35C4        \  if (mbUseOverides) mass = mfMassOverride
//   0x825E35C0  lfs       f0,  0x4C(r3)          /  (the branch SKIPS the override load when
//                                                    the flag is zero -- i.e. the type mass is
//                                                    the default and the override wins)
//   0x825E35C4  vmsum3fp128 v0,  v0,  v0         <- MagnitudeSquared(propVel)
//   0x825E35E4  lvx128    v12, r0,  r10          <- KVF_MAX_PROP_SPEED_MPS (unk_82FB9470)
//   0x825E35D8/E0 vspltisw v10,1 ; vcfsx v10,v10,0  <- 1.0f  (DWARF: GetVecFloat_One())
//     ⚠️ NAME NOTE (re-checked 2026-08-18): the DWARF callee is `rw::math::vpu::
//     GetVecFloat_One()`, and that exact spelling does NOT exist in the recovered vendor
//     headers -- a grep of b5-decomp/vendor + b5-decomp/src for it returns nothing. The
//     tree's one-vector helper is `GetVector4_One()` (vendor/renderware/include/rw/math/vpu/
//     vector4_operation.h:38, already used by SharedClasses/Traffic/BrnTrafficFuzzyLogic.h).
//     The body below deliberately calls the EXISTING tree name; it is not a second helper and
//     nothing new is invented. If the vendor owner ever lands the VecFloat-typed spelling,
//     this one call should follow it.
//   0x825E3600  vsubfp    v0,  v12, v0           <- K - lSquaredPropSpeed
//   0x825E3610  vmaxfp    v0,  v11, v0           \  Clamp(x, 0, 1)  == max-then-min, which is
//   0x825E3614  vminfp    v0,  v10, v0           /  exactly vpu::Clamp's Min(Max(v,lo),hi)
//   0x825E35D0/0604/0608  stfs f0,var_10 ; lvx128 v9 ; vspltw v9,v9,0   <- float -> VecFloat
//                                                                          broadcast of the mass
//   0x825E360C  vmulfp128 v13, v13, v9           \  lImpulseMagnitude =
//   0x825E3618  vmulfp128 v0,  v13, v0           /    speedAlongNormal * mass * modifier
//   0x825E361C  vmaddfp   v0,  v1,  v8,  v0      <- sum + lNormal*magnitude   (see (1) above)
//   0x825E3620  stvx128   v0,  r0,  r10
//   0x825E3624  blr
//
// vmaddfp/vnmsubfp OPERAND ORDER, stated because getting it wrong silently corrupts the math:
// IDA prints the ENCODED field order `vD, vA, vB, vC`, while the operation is
// `vD = vA*vC + vB` (and `vnmsubfp` = `vB - vA*vC`). Cross-checked against the last
// instruction, which must be `accumulator + lNormal * magnitude`: with vA=v1(lNormal),
// vB=v8(the loaded accumulator), vC=v0(the magnitude) that reads correctly, and under the
// other convention it would read `lNormal*accumulator + magnitude`, which is nonsense.
//
// DEAD STORES NOT REPRODUCED, and why: 0x825E35EC/F0/F4 write three zero words at
// `r1 + back_chain` +0/+4/+8 and nothing ever reads them (the only stack RELOAD, 0x825E3604,
// is 16 bytes at `r1 + var_10`, which is where the mass float was spilled at 0x825E35D0).
// They are the compiler's dead zero-init of a Vector3 local -- almost certainly the DWARF's
// `Vector3 lImpulse` (source :1710) -- and carry no behaviour. Stated rather than silently
// dropped.
//
// LOCALS are the DecFIGS scope's, verbatim (dwarfdump BrnPropManager.cpp:389-431, source
// :1685): lRelativeVelocity :1687, lRelativeSpeedAlongNormal :1688, lCarTransform :1689,
// lfPropMass :1695, lSquaredPropSpeed :1706, lPropSpeedModifier :1707, lImpulseMagnitude
// :1709, lImpulse :1710. The same scope lists the callees, and the body below is item-for-item
// that list: ExternalPhysicsBody::GetLocalVelocity, GetVecFloat_One, operator+=,
// RaceCarPhysics::AddPropCollisionImpulse, MagnitudeSquared, operator-, Dot, Max<VecFloat>,
// operator*, operator+=, operator-, Clamp, operator*, operator*.
//   ⚠️ `Matrix44Affine lCarTransform` (:1689) has NO ARTIST counterpart and is deliberately
//   ABSENT below. Negative evidence: the 50 instructions read exactly ONE 16-byte quantity out
//   of the car transform -- `lvx128 v13, r11, 0x30` == mTransform.wAxis -- and that read is
//   INSIDE the inlined GetLocalVelocity (it is the `lPoint - mTransform.wAxis` moment arm).
//   There is no matrix copy, no basis-row read, and no second use. Writing a whole
//   Matrix44Affine copy here to honour the DWARF local would add work the binary does not do.
//
// GetLocalVelocity's SPACE ARGUMENT is not a guess: rw::physics::InputSpace is
// {WORLD_SPACE=0, BODY_SPACE=1} (DWARF-authoritative, rw/physics/rigidbody.h:88-93), and the
// committed body ExternalPhysicsBody.cpp:839 makes WORLD_SPACE the leg that subtracts
// mTransform.wAxis and then crosses -- which is exactly and only what the asm above does.
// BODY_SPACE would have rotated the point through the basis rows first; there is no such
// rotation in the emission.
//
// NaN POLARITY (AGENTS.md gotcha 4), stated because it is a real host/console delta: the
// console's `vmaxfp`/`vminfp` are hardware max/min; the committed vpu::Max/Min are
// `a > b ? a : b` / `a < b ? a : b` ternaries, which resolve a NaN operand differently. The
// operand ORDER below is written to match the asm exactly (`vmaxfp v13, v11(zero), v13` ->
// `Max(zero, dot)`), so the two agree on every ordered input.
//
// ------------------------------------------------------------------------------------------
// PROOF (this wave, scratchpad/waveQ2/probe_wq2_08/):
//   * `selfcheck probe_apply_impulse.cpp`         -> STATUS=fail with EXACTLY THREE lines:
//       C2065 "KVF_MAX_PROP_SPEED_MPS": undeclared identifier            (blocker 2)
//       C2737 "lPropSpeedModifier": const object must be initialised     (fallout of the same)
//       C2039 "AddPropCollisionImpulse" is not a member of RaceCarPhysics (blocker 1)
//     -- i.e. every other name in the body (GetLocalVelocity, WORLD_SPACE, GetLinearVelocity,
//     GetMass, mbUseOverides, mfMassOverride, the whole vpu vocabulary) already binds.
//   * `selfcheck probe_apply_impulse_shimmed.cpp` -> STATUS=pass. Same file with a file-local
//     zero constant and the accessor call replaced by `(void)lImpulse;` -- proving the residual
//     is exactly those two declarations and nothing structural.
// ------------------------------------------------------------------------------------------
// LINK-LEVEL: ApplyPropRaceCarCollisionImpulse has NO inert gate anywhere
// (grepped BrnPhysicsConductorGates.cpp and WorldLinkStubs.cpp -- neither names it), and no
// body anywhere in the tree, so mounting it creates no duplicate. Its one caller is
// PropManager::SetupAndValidatePropContact @0x82628190, which is itself a trap stub today.
// ==========================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"      // GetLinearVelocity
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"              // PropTypeData::GetMass
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h" // GetLocalVelocity,
                                                                             // AddPropCollisionImpulse
#include "rw/physics/rigidbody.h"                                            // rw::physics::WORLD_SPACE
#include "rw/math/vpu/vector3_operation.h"                                   // Subtract/Dot/Mult/
                                                                             // MagnitudeSquared
#include "rw/math/vpu/vector4_operation.h"                                   // VecFloat Splat/Max/
                                                                             // Clamp/GetVector4_One

namespace BrnPhysics
{
namespace Props
{
    // ======================================================================================
    // ApplyPropRaceCarCollisionImpulse  --  X360 0x825E3560, 50 instructions.
    // DecFIGS: dwarfdump BrnPropManager.h:428 (declaration) / BrnPropManager.cpp:389
    // (scope, source line :1685). Parameter names/order are the committed header's, which
    // round 2 already corrected to the DWARF's (lNormal, lPointOnCar).
    // ======================================================================================
    void
    PropManager::ApplyPropRaceCarCollisionImpulse(
        BrnPhysics::Vehicle::RaceCarPhysics* lpRaceCar,
        PropInstance*                        lpProp,
        const PropTypeData*                  lpType,
        Vector3                              lNormal,
        Vector3                              lPointOnCar )
    {
        namespace vpu = rw::math::vpu;

        // 0x825E3580..0x825E35B0. The velocity of the prop relative to the point of the car it
        // struck. The car side is ExternalPhysicsBody::GetLocalVelocity, which the X360
        // inlined into the cross-product chain transcribed in the banner.
        const Vector3 lRelativeVelocity =
            vpu::Subtract( lpProp->GetLinearVelocity(),
                           lpRaceCar->GetLocalVelocity( lPointOnCar, rw::physics::WORLD_SPACE ) );

        // 0x825E35B4 / 0x825E35B8. Only separation counts: a prop moving away from the car
        // along the contact normal contributes nothing. Operand order matches `vmaxfp v13,
        // v11, v13` (zero first).
        const VecFloat lRelativeSpeedAlongNormal =
            vpu::Max( vpu::Splat( 0.0f ),
                      vpu::Splat( vpu::Dot( lRelativeVelocity, lNormal ) ) );

        // 0x825E356C / 0x825E3574 / 0x825E35BC / 0x825E35C0. The debug mass override wins over
        // the prop type's own mass.
        f32 lfPropMass = lpType->GetMass();
        if ( mbUseOverides )
        {
            lfPropMass = mfMassOverride;
        }

        // 0x825E35C4 / 0x825E3600 / 0x825E3610 / 0x825E3614. A fast-moving prop hands the car
        // proportionally less back. See the CONSOLE-VALUE WARNING in the banner: with the
        // shipped (zero) constant this modifier is identically zero.
        const VecFloat lSquaredPropSpeed =
            vpu::Splat( vpu::MagnitudeSquared( lpProp->GetLinearVelocity() ) );

        const VecFloat lPropSpeedModifier =
            vpu::Clamp( KVF_MAX_PROP_SPEED_MPS - lSquaredPropSpeed,
                        vpu::Splat( 0.0f ),
                        vpu::GetVector4_One() );

        // 0x825E360C / 0x825E3618 / 0x825E361C.
        const VecFloat lImpulseMagnitude =
            lRelativeSpeedAlongNormal * vpu::Splat( lfPropMass ) * lPropSpeedModifier;

        const Vector3 lImpulse = vpu::Mult( lNormal, lImpulseMagnitude.x );

        // 0x825E35E8/0x825E35FC/0x825E361C/0x825E3620 -- the inlined
        // `mPropCollisionImpulseSum += lImpulse`, flushed later by ApplyPropCollisionImpulseSum.
        lpRaceCar->AddPropCollisionImpulse( lImpulse );
    }
}
}
