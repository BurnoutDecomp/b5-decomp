// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ6_02.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q6, ROUND 2, cluster "lean", 2026-08-19). Folds back into
// BrnPropManager.cpp.
//
// TWO FUNCTIONS -- the JOINTED-PROP contact response, i.e. what makes a car hitting a lamppost,
// a pole or a swinging sign LEAN or TILT about its joint instead of ignoring the hit:
//
//   * BrnPhysics::Props::PropManager::HandleContactWithLeanProp  @0x8260FB60  (854 instructions,
//         (0x826108B4 - 0x8260FB60)/4 + 1 == 854).  DWARF: BrnPropManager.h:442 / .cpp:1771.
//   * BrnPhysics::Props::PropManager::HandleContactWithTiltProp  @0x826108B8  (720 instructions,
//         (0x826113F4 - 0x826108B8)/4 + 1 == 720).  DWARF: BrnPropManager.h:456 / .cpp:1903.
//
// Both are called live from PropManager::SetupAndValidatePropContact
// (PropManager_wQ4_01.cpp:581 / :589, behind `lpPropInstance->IsJointed()` and the
// KU8_JOINT_TYPE_LEAN / _TILT split), which is real and mounted. Until today both were inert
// BRN_CONDUCTOR_GATEs; the gate text ("a car hitting a jointed lamppost/pole gets no lean/tilt
// response") described exactly the user-visible hole this file closes.
//
// -------------------------------------------------------------------------------------------------
// PROVENANCE. The two bodies were written COMPLETE in wave Q round 2 (2026-08-18) and parked at
//   scratchpad/waveQ2/parked/PropManager_07_HandleContactWithLeanProp.cpp
//   scratchpad/waveQ2/parked/PropManager_07_HandleContactWithTiltProp.cpp
// on EIGHT missing declarations. All eight landed with this file (see "WHAT UNBLOCKED THIS"
// below). The parked banners' §A (register map), §B (the vmaddfp vs vmaddfp128 operand-order
// split, derived from known-answer sites inside these very functions), §C (the four file-scope
// VecFloat constants recovered from their CRT-init thunks) and §D (assert-stream collapse) are
// the derivation record and are NOT repeated here; read them for the "why".
//
// ⚠️ THE PARKED TEXT WAS RE-VERIFIED AGAINST THE RAW ASM BEFORE LANDING, not copied. Sources:
//   * .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8260FB60.json and 0x826108B8.json, `assembly` array
//     (dumped at scratchpad/waveQ2/probe_wq2_07/asm_0x8260FB60.txt / asm_0x826108B8.txt);
//   * references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp:1400-1702
//     (Lean scope) and :1703-1902 (Tilt scope) -- the local list AND the callee list;
//   * references/Feb-2007/.../rwmath/1.02.00/include/rw/math/vpu/*.h -- the shipped SDK headers,
//     which is what pinned every vpu spelling used below.
// THREE THINGS CHANGED relative to the parked text; each is a defect the park carried, each is
// flagged at its site below:
//   (1) `Select` -- the park used the tree's Vector4 argument order (false, true, mask). The SDK
//       order is Select(mask, trueValue, falseValue) (Feb-2007 vector3_operation.h:127). The
//       SELECTION IS THE SAME either way; only the spelling was wrong. Fixed.
//   (2) the Mask3 -> MaskScalar lane broadcasts -- the park spelled them as free functions
//       `vpu::GetX(mask)` and flagged the spelling as a PLACEHOLDER. They are MEMBERS:
//       `Mask3::GetX/GetY/GetZ() const` (Feb-2007 mask3.h + mask3_type_inline.h:71/:78/:85).
//       Fixed; the park's flag is discharged.
//   (3) ⭐ THE POINT-VELOCITY CALL WAS IN THE WRONG PLACE -- a real, if small, numeric defect.
//       See the ⚠️ block at step 3 of the Lean body. It is the one substantive correction.
//
// -------------------------------------------------------------------------------------------------
// WHAT UNBLOCKED THIS -- the eight declarations, all landed 2026-08-19 by this cluster:
//   game headers (2)
//     ExternalPhysicsBody::GetLinearMomentum(VecFloat) const     ExternalPhysicsBody.h/.cpp
//     Vehicle::Wheel::GetRoadLongSpeed() const                   Wheel.h (header-inline)
//   vendor rw-math (6), all in b5-decomp/vendor/renderware/include/rw/math/vpu/
//     Matrix44AffineFromAxisRotationAngle(Vector3, VecFloat)     matrix44affine_operation.h
//     GetVector3_XAxis / _YAxis / _ZAxis                         vector3_operation.h
//     struct Mask3 (+ GetX/GetY/GetZ)                            vector3_operation.h
//     Mask3 CompLessThan / CompGreaterThan (Vector3, Vector3)    vector3_operation.h
//     Vector3 Select(MaskScalar, Vector3, Vector3)               vector3_operation.h
//     MaskScalar CompLessThan / CompGreaterThan (VecFloat x2)    vector4_operation.h
// Every one is callable with exactly the shape used below -- proven by the compile probe
// scratchpad/waveQ6/probe_lean/probe_lean.cpp (STATUS=pass), which pins all eight signatures
// with function/member-pointer typedefs and carries a NaN-polarity tripwire.
//
// ⚠️ LINK, for the conductor (AGENTS.md gotcha 12) -- ONE hole, re-grepped 2026-08-19 across
//    b5-decomp/src AND b5-decomp/vendor:
//      * BrnPhysics::ExternallySimulatedBody::Translate(Vector3) -- DECLARED at
//        GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h:105, NO definition
//        anywhere in the tree. Both bodies below call it (step 2). `cl /c` is green; the link
//        is not. That header/TU is NOT in this cluster's ownership, so it is REPORTED, not
//        written. On the console the call is INLINED to a single `stvx128` into the car body's
//        mTransform.wAxis (Lean @0x8260FD10, Tilt @0x82610A7C), so the missing body is exactly
//        `mTransform.wAxis = mTransform.wAxis + lvTranslation`.
//      * ✅ ExternalPhysicsBody::GetLocalVelocity is NOT a hole -- real body at
//        ExternalPhysicsBody.cpp:839. (Both parked banners claim it is; that claim is STALE and
//        PropManager_wQ2_07.cpp:53-56 already corrected it. Do not write a second definition.)
//
// ⚠️ ODR: HandleContactWithLean/TiltProp have exactly one other definition each -- the two inert
//    gates at GameSource/Physics/BrnPhysicsConductorGates.cpp:500-505 and :506-511 (with their
//    shared comment block at :492-499). MOUNTING THIS FILE WITHOUT RETIRING THOSE IS AN LNK2005
//    that `cl /c` cannot see. Retire the whole :492-511 range in the same change -- after this
//    landing that comment's "three honest gates" claim covers nothing, because the third
//    (RemoveAllPropsAndParts) was already retired on 2026-08-19 by the wQ6_01 cluster.
// =================================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"          // UpdatePropEvent
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                    // CgsPhysics::RigidBodyId
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"   // InAddPotentialContact
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // gpDebugPrint ([DIAG] only)

#include "rw/physics/rigidbody.h"                                           // rw::physics::WORLD_SPACE
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"

#include <cmath>     // std::cos -- the console `bl cos` @0x8260FC10 / @0x8261096C
#include <stdlib.h>  // getenv (BRN_PROP_DIAG, host only)

namespace BrnPhysics
{
namespace Props
{
    namespace vpu = ::rw::math::vpu;

    namespace
    {
        // -----------------------------------------------------------------------------------
        // The four file-scope tuning constants, DWARF BrnPropManager.cpp:1755-1758 -- declared
        // immediately above HandleContactWithLeanProp at :1771, which is why they belong in
        // THIS TU and are defined exactly once for both bodies (the parked Tilt file's own ODR
        // note asked for precisely this).
        //
        // ⚠️ AGENTS.md gotcha 13 IN THE FLESH: all four read as ZERO in the shipped image. They
        // are NOT unrecoverable and they are NOT placeholders -- they are written by CRT-init
        // thunks in the un-functionised region 0x82C5E910..0x82C5E9AC, and an IDA `DataRefsTo`
        // on each returns exactly three sites (this function, the Tilt twin, and its thunk).
        // Reading the thunks gives the source float:
        //     unk_82FB9480 <- flt_8200D528 = 0.07f  -> KVF_ROTATION_FACTOR
        //     unk_82FB9410 <- flt_820047C8 = 0.05f  -> KVF_MAX_ROTATION
        //     unk_82FB9430 <- flt_82001DA0 = 0.5f   -> KVF_PENETRATION_RESOLUTION_FACTOR
        //     unk_82FB93D0 <- flt_82001DA0 = 0.5f   -> KVF_MOMENTUM_RESOLUTION_FACTOR
        // The name<->address assignment is by ROLE, not by declaration order, and each role is a
        // one-to-one match against the asm: 0x82FB9480 scales the speed that feeds the rotation
        // angle (`vmulfp128 v13, v11, v13` @0x8260FEF0) and 0x82FB9410 is the Min() ceiling on it
        // (`vminfp128 v123, v13, v12` @0x8260FF08); 0x82FB9430 scales the penetration depth
        // (`vmulfp128 v0, v13, v0` @0x8260FCF8); 0x82FB93D0 scales the momentum-derived impulse
        // (`vmulfp128 v1, v0, v13` @0x8260FCCC). INFERENCE only in that the DWARF does not pin a
        // name to an address.
        // -----------------------------------------------------------------------------------
        const VecFloat KVF_ROTATION_FACTOR               = { 0.07f, 0.07f, 0.07f, 0.07f };
        const VecFloat KVF_MAX_ROTATION                  = { 0.05f, 0.05f, 0.05f, 0.05f };
        const VecFloat KVF_PENETRATION_RESOLUTION_FACTOR = { 0.5f,  0.5f,  0.5f,  0.5f  };
        const VecFloat KVF_MOMENTUM_RESOLUTION_FACTOR    = { 0.5f,  0.5f,  0.5f,  0.5f  };

        // `lfs f0, flt_8208F5F4` @0x8260FC08 == 0.01745329238474369f -- degrees to radians, used
        // only by the mbUseOverides debug path.
        const f32 KF_DEG_TO_RAD = 0.01745329238474369f;

        // The lazily-built function-local statics of the Lean body, behind the guard word
        // dword_82FBA2A0 (bit 0 for K_EPSILON @unk_82FBA290, bit 1 for K_EPSILON3
        // @unk_82FBA280) -- the classic MSVC magic-static pattern, MEASURED
        // @0x8260FF58..0x8260FFE8, seeded from `flt_82013F90 == 0.001f`. DWARF locals
        // K_EPSILON @BrnPropManager.cpp:1841 / K_EPSILON3 @:1842. Hoisted to TU scope because
        // the host has no reason to pay the guard word; the VALUES are what matter.
        // ⚠️ The console keeps the scalar K_EPSILON only to BUILD the Vector3 (the `vperm` +
        // `vrlimi128` pair @0x8260FFCC/D4 packs the splat into {e,e,e,0}); it is never compared
        // against anything, so only the Vector3 form is reproduced.
        const Vector3 K_EPSILON3 = { 0.001f, 0.001f, 0.001f, 0.0f };

        // -----------------------------------------------------------------------------------
        // [DIAG] NOT IN THE X360 BINARY. Wave-Q6 bring-up probe, opt-in via BRN_PROP_DIAG.
        // ONE-SHOT across BOTH bodies -- the question it answers is "does a car-vs-jointed-prop
        // contact reach this response at all?", which is a yes/no, and these two run per contact
        // per frame once a pole is being leaned on. The getenv latch is a function-local static
        // so it costs one predicted branch, never a per-contact syscall.
        // -----------------------------------------------------------------------------------
        bool gbQ6LeanDiagFired = false;

        void
        Q6LeanDiag( const CgsPhysics::PhysicsSimulationIO::InAddPotentialContact* lpOutContact,
                    bool                                                          lbPropIsEntityA,
                    u32                                                           luPropTypeId,
                    const char*                                                   lpcJointType )
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            if ( !sbPropDiag || gbQ6LeanDiagFired || CgsDev::Log::gpDebugPrint == 0 )
            {
                return;
            }
            gbQ6LeanDiagFired = true;

            // Same id split the Tilt body's UpdatePropEvent tail uses (see step 7 there).
            const CgsPhysics::RigidBodyId lPropRigidBodyId(
                lbPropIsEntityA ? lpOutContact->mIDA : lpOutContact->mIDB );

            *CgsDev::Log::gpDebugPrint
                << "[Q6-lean] first lean/tilt contact prop="
                << static_cast<u32>( lPropRigidBodyId.GetEntityId() )
                << " type=" << luPropTypeId
                << " joint=" << lpcJointType
                << "\n";
        }
    }

    // =============================================================================================
    // HandleContactWithLeanProp @0x8260FB60 (854)
    //
    // §A REGISTER MAP (MEASURED from the prologue; AGENTS.md gotcha 3 -- the f32 rides f1 and
    //   SKIPS its GPR slot, and the three Vector3s ride v1/v2/v3 consuming no GPR at all):
    //     r3 = this(r18)  r4 = lpPropInstance(r20)  r5 = liPropIndex  r6 = lpType(r19)
    //     r7 = lpRaceCar(r26)  r8 = lpOutContact  r9 = lbPropIsEntityA  f1 = lfTimeStep(f31)
    //     v1 = lNormal(v127)   v2 = lPointOnProp(v120)  v3 = lPointOnCar(v122)
    //   ⚠️ MEASURED, and worth stating because it looks like a bug and is not: liPropIndex and
    //   lbPropIsEntityA are NEVER READ BY THE CONSOLE BODY. r5 is dead after the prologue and r9
    //   is overwritten at 0x8260FBD4 by `lbz r9, 0x49(r18)` (this->mbUseOverides) before any use.
    //   The Tilt twin DOES read both (it queues an UpdatePropEvent). The two share a declaration
    //   block, so the parameters are present here and unused -- except by the [DIAG] line, which
    //   is not in the X360 binary and is called out as such at its site.
    // =============================================================================================
    void
    PropManager::HandleContactWithLeanProp(
        PropInstance*                                            lpPropInstance,
        s32                                                      liPropIndex,
        const PropTypeData*                                      lpType,
        BrnPhysics::Vehicle::RaceCarPhysics*                     lpRaceCar,
        Vector3                                                  lNormal,
        Vector3                                                  lPointOnProp,
        Vector3                                                  lPointOnCar,
        CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*   lpOutContact,
        bool                                                     lbPropIsEntityA,
        f32                                                      lfTimeStep )
    {
        // §A: genuinely unread by the console body. Kept named, not renamed away.
        (void)liPropIndex;

        // [DIAG] NOT IN THE X360 BINARY -- the wave-Q6 one-shot. This is the ONLY reader of
        // lbPropIsEntityA in this function; see §A.
        Q6LeanDiag( lpOutContact, lbPropIsEntityA, lpPropInstance->GetTypeId(), "LEAN" );

        // `lfs f0, flt_82001CC0(==0.0f)` ; `stfs f0, 0x48(r8)` @0x8260FBA0/0x8260FBA8.
        // +0x48 in InAddPotentialContact is mRestitution (CgsPhysicsSimulationIO_Events.h:143,
        // DWARF CgsPhysicsSimulationModuleIO.h:207). A jointed prop absorbs the hit -- the solver
        // must not bounce the car off a lamppost that is about to swing.
        lpOutContact->mRestitution = 0.0f;

        // MEASURED: the car transform is read ONCE, at the top, from `lpRaceCar + 0x10` -- the
        // embedded ExternallySimulatedBody::mTransform (gotcha 2: the leaf's vptr occupies +0x00,
        // so the body sub-object starts at +0x10). Both the pre-Translate position
        // (`lvx128 v8, r31, 0x30` @0x8260FC4C) and the xAxis (`lvx128 v119, r0, r31` @0x8260FBE4)
        // come from this ONE read, which is why nothing below may re-read it after the Translate.
        const Matrix44Affine lCarTransform = lpRaceCar->GetTransform();

        // The prop's world transform: four 16-byte rows at lpPropInstance+0x00..0x30
        // (`lvx128 v117/v118/v116/v125` @0x8260FBB0/BCC/BDC/BF4, spilled to the stack frame).
        Matrix44Affine lTransform = lpPropInstance->GetTransform();
        const Vector3  lPos       = lTransform.wAxis;   // v115, saved before v125 is reused

        // ⚠️ The DWARF names a local `Matrix44Affine lInverseTransform` (:1775) that has NO
        // instructions of its own in the ARTIST body -- a dead local. Stated, not smoothed over;
        // deliberately not declared here.

        // lMaxAngleCos: `lvlx v0, r19, 0x48` + `vspltw v11, v0, 0` @0x8260FBC4/BD8 -- lpType+0x48
        // is PropTypeData::mfMaxJointAngleCos (console +0x48; the host packs it elsewhere --
        // gotcha 1, the console offset is a COMMENT and the code goes by name). Overridden from
        // the debug knob when this->mbUseOverides (`lbz r9, 0x49(r18)` @0x8260FBD4) is set.
        VecFloat lMaxAngleCos = vpu::Splat( lpType->GetLeanCosAngle() );
        if ( mbUseOverides )
        {
            lMaxAngleCos = vpu::Splat( std::cos( mfMaxLeanAngleOverride * KF_DEG_TO_RAD ) );
        }

        // lbRotatedTooFar: `vmsum3fp128 v0, v118, jVector` (dot3 of the prop's UP row with the
        // world Y axis) then `vcmpgefp v0, v0, v11` @0x8260FC9C then `vnot128 v114, v0`
        // @0x8260FCAC. The vnot is what makes it CompLessThan: "the prop has leaned past its
        // authored limit". Under PPC polarity (gotcha 4) a NaN lane reads TRUE, which is exactly
        // what vendor CompLessThan reproduces. Consumed only at the very end
        // (`vcmpeqfp128. v0, v114, v126` @0x82610654).
        const vpu::MaskScalar lbRotatedTooFar =
            vpu::CompLessThan( vpu::Splat( vpu::Dot( lTransform.yAxis, vpu::GetVector3_YAxis() ) ),
                               lMaxAngleCos );

        // ---- 1. how fast the car is closing on the prop AT the contact point -------------------
        // ⚠️⚠️ THIS BLOCK MOVED, AND THE MOVE IS THE ONE SUBSTANTIVE CORRECTION TO THE PARKED
        // BODY. The park computed it AFTER the Translate at step 3 and passed the pre-computed
        // lRaceCarToPropVector to GetLocalVelocity. Both halves of that were wrong against this
        // tree:
        //   * WHAT THE CONSOLE COMPUTES (MEASURED, and unambiguous):
        //       `lvx128 v8, r31, 0x30`      @0x8260FC4C   the car position, PRE-Translate
        //       `vsubfp128 v123, v122, v8`  @0x8260FC5C   r = lPointOnCar - thatPosition
        //       `lvx128 v125, r31, 0x50`    @0x8260FC98   mAngularVelocity
        //       `vpermwi128 v0, v123, 0x63` @0x8260FD08   the cross-product permute, on v123
        //     i.e. mLinearVelocity + Cross(mAngularVelocity, r) with r built from the position
        //     BEFORE the resolve store at 0x8260FD10. v123 is used NOWHERE else in the 854
        //     instructions (grepped), so the moment arm has exactly one producer and one consumer.
        //   * WHAT THE TREE'S GetLocalVelocity DOES: for WORLD_SPACE it re-derives the moment arm
        //     itself -- `lvR = lPoint - mTransform.wAxis` (ExternalPhysicsBody.cpp:842-845). So
        //     handing it the ALREADY-RELATIVE lRaceCarToPropVector subtracts the car position a
        //     SECOND time, and calling it after Translate reads a position the console never used.
        //     The park did both, which would have put the moment arm at
        //     (lPointOnCar - 2*carPos + resolveVector) instead of (lPointOnCar - carPos).
        // The faithful spelling is therefore: pass the WORLD point, and call it BEFORE the
        // Translate. That reproduces the console's arithmetic exactly while still going through
        // the by-name callee the DWARF lists (:1493 ExternalPhysicsBody::GetLocalVelocity).
        // Nothing between here and the Translate reads or writes mLinearVelocity/mAngularVelocity,
        // so no other value shifts. lRaceCarToPropVector stays as the DWARF's named local (:1777)
        // and as the documentation of what the callee rebuilds.
        //
        // ⚠️ AND THAT IS WHY THE DWARF'S `Vector3 lRaceCarToPropVector` (BrnPropManager.cpp:1777)
        // HAS NO LOCAL HERE: it is precisely the `lPoint - mTransform.wAxis` the by-name callee
        // computes for itself on the WORLD_SPACE arm. Declaring it again and passing it would BE
        // the double subtraction. The console has both because it inlined the callee and CSE'd the
        // subtraction out to the top; a by-name call cannot.
        //
        // FLAG (InputSpace): the tag itself is not recoverable from the asm -- the console inlined
        // the callee and the tag with it. WORLD_SPACE is the only one consistent with the measured
        // arithmetic (BODY_SPACE would rotate the point through the transform's 3x3, which the asm
        // plainly does not do).
        const Vector3 lPointVelocity =
            lpRaceCar->GetLocalVelocity( lPointOnCar, rw::physics::WORLD_SPACE );

        // `vmsum3fp128 v12, v12, -lNormal` -> `vmaxfp128 v12, v12, 0` (Lean's copy is scheduled
        // into the 0x8260FD20..0x8260FE00 stretch; the Tilt twin shows the same pair cleanly at
        // 0x82610AD8/0x82610B10).
        const VecFloat lVelocityAlongNormal =
            vpu::Max( vpu::Splat( vpu::Dot( lPointVelocity, -lNormal ) ), vpu::Splat( 0.0f ) );

        // ---- 2. bleed the car's momentum along the contact normal into the prop ---------------
        // MEASURED @0x8260FCA4..0x8260FCD0: GetLinearMomentum(dt) dotted with -lNormal, clamped
        // at zero, re-scaled by the normal and by KVF_MOMENTUM_RESOLUTION_FACTOR, then handed to
        // AddWorldSpaceImpulse with r3 == lpRaceCar+0x10 (the ExternalPhysicsBody sub-object).
        // ⚠️ The ARTIST image `bl`s the symbol BrnPhysics__ExternalPhysicsBody__AddWorldSpaceImpulse
        // (@0x8260FCD0); the DWARF spells the same sink `ExternalPhysicsBody::AddImpulse` (:1507).
        // The ARTIST spelling is the one this tree has, and it is what is called.
        const VecFloat lMomentumTowardsProp =
            vpu::Max( vpu::Splat( vpu::Dot( lpRaceCar->GetLinearMomentum( vpu::Splat( lfTimeStep ) ),
                                            -lNormal ) ),
                      vpu::Splat( 0.0f ) );

        lpRaceCar->AddWorldSpaceImpulse(
            lNormal * ( lMomentumTowardsProp.x * KVF_MOMENTUM_RESOLUTION_FACTOR.x ) );

        // ---- 3. push the car back out of the prop ---------------------------------------------
        // MEASURED @0x8260FCD4..0x8260FD10: Max(Dot(n, pOnCar - pOnProp) * K, 0) along n, added
        // straight into the car's mTransform.wAxis (`vmaddfp128 v13, v127, v0, v13` then
        // `stvx128 v13, r0, lpRaceCar+0x40`), i.e. ExternallySimulatedBody::Translate -- which the
        // DWARF names at :1510 and which is the LINK HOLE recorded in this file's banner.
        const VecFloat lResolveDistance =
            vpu::Max( vpu::Splat( vpu::Dot( lNormal, lPointOnCar - lPointOnProp )
                                  * KVF_PENETRATION_RESOLUTION_FACTOR.x ),
                      vpu::Splat( 0.0f ) );
        const Vector3 lResolveVector = lNormal * lResolveDistance.x;
        lpRaceCar->Translate( lResolveVector );

        // ---- 4. the joint ---------------------------------------------------------------------
        // MEASURED @0x8260FD60..0x8260FD70: `lbz r11,0x6C(r20)` (PropInstance::mu8JointIndex),
        // `addi r11,r11,0xC`, `slwi r11,r11,4`, `lvx128 v121, r11, r18` -- i.e. this + 0xC0 +
        // 16*jointIndex == maPropJointPositions[jointIndex]. The `cmplwi r11,0xFF` + assert
        // "IsJointed()" (BrnPropInstance.h:326, `li r5,0x146`) immediately before it IS
        // GetJointIndex()'s own baked tripwire, so the by-name call carries it and it is not
        // re-spelled here.
        const s32     liJointIndex        = lpPropInstance->GetJointIndex();
        const Vector3 lWorldSpaceJointPos = maPropJointPositions[ liJointIndex ];
        const Vector3 lJointToPointOnProp = lPointOnProp - lWorldSpaceJointPos;

        // BrnPropManager.cpp:1827 (`li r5,0x723`). The console streams the two vectors into the
        // message; the literal text really is the stale "lContactPoint: " (the parameter has since
        // been renamed lPointOnProp in the DWARF). §D: the StrStream construction collapses to
        // CGS_ASSERT, exactly as every committed body in this cluster does.
        CGS_ASSERT( vpu::IsValid( lJointToPointOnProp ),
                    "lContactPoint / lWorldSpaceJointPos" );

        // `lfs f0, flt_82001C98(==1.0f)` ; `fdivs f0, f0, f31` @0x8260FEE0/FEE8.
        const f32 lfInvTimeStep = 1.0f / lfTimeStep;

        // ---- 5. the lean angle and its axis ----------------------------------------------------
        // MEASURED @0x8260FEA8..0x8260FF34. The speed that drives the lean is
        // Max(rear-left wheel road-long speed, lVelocityAlongNormal), scaled by
        // KVF_ROTATION_FACTOR and ceilinged by KVF_MAX_ROTATION:
        //     addi      r11, r26, 0x360        maWheels[eRearLeftWheel].mSpeedAndMassOnWheel...
        //     vspltw    v12, v0, 0             ... lane .x  == the road-long speed
        //     vmaxfp128 v11, v12, v125         Max(that, lVelocityAlongNormal)
        //     vmulfp128 v13, v11, v13          * KVF_ROTATION_FACTOR  (unk_82FB9480)
        //     vminfp128 v123, v13, v12         Min(that, KVF_MAX_ROTATION) (unk_82FB9410)
        // 0x360 == maWheels(+0x130) + 2*0xE0 + 0x70, i.e. the REAR-LEFT wheel specifically -- not
        // an average, not the driven pair. Reproduced as written.
        const VecFloat lfAngleToRotate =
            vpu::Min( vpu::Max( lpRaceCar->GetWheel( BrnPhysics::Vehicle::eRearLeftWheel )
                                    .GetRoadLongSpeed(),
                                lVelocityAlongNormal ) * KVF_ROTATION_FACTOR,
                      KVF_MAX_ROTATION );

        // Cross(YAxis, -lNormal): the two `vpermwi128(_, 0x63)` (the .yzx swizzle) around a
        // vmulfp/vnmsubfp pair @0x8260FEC4..0x8260FF2C is the canonical VMX cross product, with
        // the Y axis loaded from unk_82181510 (`addi r28, r11, unk_82181510` @0x8260FC48).
        Vector3 lRotationAxis = vpu::Cross( vpu::GetVector3_YAxis(), -lNormal );

        // ⚠️ MEASURED ORDERING, and it is not what it looks like: maLastJointRotation is written
        // with the RAW (un-normalised) axis -- `stvx128 v125, r9, r18` @0x8260FF6C happens BEFORE
        // the normalise block at 0x8260FFEC. r9 == (jointIndex + 0x1B) << 4 == this + 0x1B0 +
        // 16*jointIndex == maLastJointRotation[jointIndex]. Do NOT "tidy" this by moving the store
        // below the normalise: the stored value is an angular RATE consumed elsewhere
        // (BrnPropManager.h:417), and normalising it first would change its magnitude.
        maLastJointRotation[ liJointIndex ] =
            lRotationAxis * ( lfAngleToRotate.x * lfInvTimeStep );

        // "is the axis (near) zero in all three lanes" -- the SDK's per-lane compares reduced by
        // three GetX/GetY/GetZ lane broadcasts and FIVE Ands @0x82610018..0x82610048. The count
        // of five is MEASURED (five `vand`), and the DWARF's callee list for this stretch is
        // literally five consecutive `rw::math::vpu::And` entries (BrnPropManager.cpp:1548-1552).
        // Under PPC polarity (gotcha 4) a NaN lane reads TRUE from CompLessThan and FALSE from
        // CompGreaterThan -- the vendor helpers reproduce both, so a NaN axis lands in the
        // "degenerate" arm below rather than propagating into the rotation matrix.
        const vpu::Mask3 lLessThan    = vpu::CompLessThan( lRotationAxis, K_EPSILON3 );
        const vpu::Mask3 lGreaterThan = vpu::CompGreaterThan( lRotationAxis, -K_EPSILON3 );

        const vpu::MaskScalar lAllLessThan =
            vpu::And( vpu::And( lLessThan.GetX(), lLessThan.GetY() ), lLessThan.GetZ() );
        const vpu::MaskScalar lAllGreaterThan =
            vpu::And( vpu::And( lGreaterThan.GetX(), lGreaterThan.GetY() ), lGreaterThan.GetZ() );
        const vpu::MaskScalar lIsZero = vpu::And( lAllLessThan, lAllGreaterThan );

        // `vsel128 v125, v0, v119, v125` @0x8261005C. PPC `vsel vD,vA,vB,vC` is `vC ? vB : vA`,
        // so vA = v0 = the normalised axis is the FALSE value and vB = v119 = lCarTransform.xAxis
        // (read at the top of the function) is the TRUE value: when the contact normal is parallel
        // to world-up the cross product degenerates and the car's own right vector becomes the
        // lean axis. SDK order is Select(mask, trueValue, falseValue).
        lRotationAxis = vpu::Select( lIsZero,
                                     lCarTransform.xAxis,
                                     vpu::NormalizeFast( lRotationAxis ) );

        CGS_ASSERT( vpu::IsValid( lRotationAxis ), "lRotationAxis" );   // .cpp:1851 (`li r5,0x73B`)

        const Matrix44Affine lRotationMatrix =
            vpu::Matrix44AffineFromAxisRotationAngle( lRotationAxis, lfAngleToRotate );

        // .cpp:1855 (`li r5,0x73F`): all four rows of the built matrix are NaN-swept before use --
        // the four-row `vspltw`/`vcmpeqfp.` cascade at 0x82610350..0x82610478.
        CGS_ASSERT( vpu::IsValid( lRotationMatrix.xAxis ) && vpu::IsValid( lRotationMatrix.yAxis )
                        && vpu::IsValid( lRotationMatrix.zAxis )
                        && vpu::IsValid( lRotationMatrix.wAxis ),
                    " Rotation matrix" );

        // ---- 6. swing the prop about its joint --------------------------------------------------
        // MEASURED @0x82610580..0x82610648. The console inlines the affine product; the DWARF's
        // call list for this stretch is exactly operator*, operator+=, TransformPoint, operator-,
        // operator-=, SetTransform. Putting -mJointLocator in the translation row BEFORE the
        // product is what makes the rotation happen ABOUT THE JOINT rather than about the origin
        // (lRotationMatrix's own wAxis is zero, so the product's wAxis is TransformVector(R,-J)).
        lTransform.wAxis = -lpType->GetJointLocator();
        lTransform       = lTransform * lRotationMatrix;
        lTransform.wAxis = lTransform.wAxis + lpType->GetJointLocator();
        lTransform.wAxis = lTransform.wAxis + lPos;

        // The joint has to end up exactly where the manager says it is; subtract the drift.
        const Vector3 lCurrentWorldSpaceJointPos =
            vpu::TransformPoint( lTransform, lpType->GetJointLocator() );
        const Vector3 lSeparation = lCurrentWorldSpaceJointPos - lWorldSpaceJointPos;
        lTransform.wAxis          = lTransform.wAxis - lSeparation;

        lpPropInstance->SetTransform( lTransform );

        // ---- 7. flag the joint for breaking ------------------------------------------------------
        // `vcmpeqfp128. v0, v114, v126` + `bne cr6, <epilogue>` @0x82610654/0x82610664, where
        // v114 == lbRotatedTooFar and v126 == zero (`vspltisw128 v126, 0` @0x8260FC44). The
        // record form sets CR6's EQ bit to mean "NO lane compared equal"; `bne cr6` branches when
        // that bit is CLEAR, i.e. as soon as ANY lane DID compare equal to zero. So the branch is
        // taken -- this block SKIPPED -- the moment a lane of the mask is zero, and the block runs
        // only when every lane is non-zero: the prop HAS leaned past its limit.
        // Both operands of the CompLessThan that produced the mask are broadcast splats, so all
        // four lanes always agree and MaskScalar::GetBool() -- which the DWARF lists at exactly
        // this point -- is an exact reproduction, not a narrowing.
        if ( lbRotatedTooFar.GetBool() )
        {
            // .cpp:1877 (`li r5,0x755` @0x826106A0, message aLppropinstance, file string r21 ==
            // aDP4B5MainBurno_225 == BrnPropManager.cpp) -- the manager's OWN joint-index
            // tripwire, distinct from the `li r5,0x146` IsJointed() assert that GetJointIndex()
            // bakes in and that rides the by-name calls bracketing it
            // (0x82610674 / 0x826106C0).
            CGS_ASSERT( liJointIndex != 0xff, "lpPropInstance->GetJointIndex() != 0xff" );

            // .cpp:1878 (`li r5,0x756`). The CgsBitArray.h:203 "invalid index : i < 15" tripwire
            // around it is IsBitSet's own baked assert and rides the by-name call.
            CGS_ASSERT( mUsedPropJoints.IsBitSet( static_cast<u32>( liJointIndex ) ),
                        "mUsedPropJoints.IsBitSet( lpPropInstance->GetJointIndex() )" );

            // `addi r25, r18, 0x678` + the sld/or/stdx set @0x826107F4/0x8261088C..0x826108A0 ==
            // mBreakPropJoints.SetBit(jointIndex).
            mBreakPropJoints.SetBit( static_cast<u32>( liJointIndex ) );
        }

        // MEASURED: unlike the Tilt twin, this body queues NO UpdatePropEvent -- there is no
        // `bl ...AddEvent` in the 854 instructions (the only non-assert `bl`s are cos,
        // AddWorldSpaceImpulse and PropInstance::SetTransform). The leaning prop's transform
        // reaches the world through the normal PostPhysicsUpdate sweep instead.
    }

    // =============================================================================================
    // HandleContactWithTiltProp @0x826108B8 (720)
    //
    // §A of the Lean body applies verbatim, with two register differences MEASURED in the Tilt
    // prologue: `mr r14, r5` @0x82610930 and `mr r15, r9` @0x82610910 -- Tilt DOES consume both
    // liPropIndex and lbPropIsEntityA, in the UpdatePropEvent tail at step 7.
    //
    // HOW TILT DIFFERS FROM LEAN (all MEASURED, side by side against 0x8260FB60):
    //   * The rotation axis is not built from a cross product and is never normalised. It is the
    //     prop's own xAxis, SIGN-FLIPPED when the contact normal points along +Z -- so there is no
    //     epsilon/Mask3 block at all here.
    //   * The body ENDS by queueing one UpdatePropEvent onto mUpdatedJointedProps, UNCONDITIONALLY
    //     (outside the break-joint `if`). Lean queues nothing.
    //     ⚠️ The DecFIGS scope names TWO `UpdatePropEvent lUpdateEvent` locals (BrnPropManager.cpp
    //     :2008 and :2025) and TWO AddEvent calls; the ARTIST image has exactly ONE `bl` to
    //     AddEvent (all 720 instructions grepped -- the only non-assert `bl`s are cos,
    //     AddWorldSpaceImpulse, PropInstance::SetTransform and that AddEvent). INFERENCE: the two
    //     source branches have identical tails and the ARTIST compiler merged them, or the second
    //     is FIGS-only. Stated, not smoothed over -- ONE AddEvent is what is reconstructed.
    // =============================================================================================
    void
    PropManager::HandleContactWithTiltProp(
        PropInstance*                                            lpPropInstance,
        s32                                                      liPropIndex,
        const PropTypeData*                                      lpType,
        BrnPhysics::Vehicle::RaceCarPhysics*                     lpRaceCar,
        Vector3                                                  lNormal,
        Vector3                                                  lPointOnProp,
        Vector3                                                  lPointOnCar,
        CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*   lpOutContact,
        bool                                                     lbPropIsEntityA,
        f32                                                      lfTimeStep )
    {
        // [DIAG] NOT IN THE X360 BINARY -- the wave-Q6 one-shot, shared with the Lean arm.
        Q6LeanDiag( lpOutContact, lbPropIsEntityA, lpPropInstance->GetTypeId(), "TILT" );

        // `stfs f0(flt_82001CC0 == 0.0f), 0x48(r16)` @0x826108F8/0x82610900 -- mRestitution.
        lpOutContact->mRestitution = 0.0f;

        // MEASURED: unlike the Lean twin (`lvx128 v119, r0, r31` @0x8260FBE4 == the car's
        // mTransform.xAxis), Tilt NEVER reads the car body's transform rows -- there is no +0x00
        // load off the body pointer anywhere in the 720 instructions. `addi r3, r30, 0x10`
        // @0x826109AC is only the ExternalPhysicsBody this-pointer for the AddWorldSpaceImpulse
        // call at 0x82610A2C, and the base register for the +0x30/+0x40/+0x50/+0xD0/+0xE0/+0x100
        // loads. The tilt axis comes from the PROP's xAxis, so no fallback basis is needed and
        // there is no lCarTransform local here.

        // `lvx128 v120/v118/v116/v124` @0x82610908/0x8261091C/0x82610944/0x8261094C -- the prop's
        // four transform rows at +0x00/+0x10/+0x20/+0x30.
        Matrix44Affine lTransform = lpPropInstance->GetTransform();
        const Vector3  lPos       = lTransform.wAxis;   // v117, saved before v124 is reused

        // `lvlx v0, r27, 0x48` + `vspltw v11, v0, 0` @0x82610934/0x82610948 ==
        // PropTypeData::mfMaxJointAngleCos. Debug override behind `lbz r8, 0x49(r18)` ==
        // this->mbUseOverides.
        VecFloat lMaxAngleCos = vpu::Splat( lpType->GetLeanCosAngle() );
        if ( mbUseOverides )
        {
            lMaxAngleCos = vpu::Splat( std::cos( mfMaxLeanAngleOverride * KF_DEG_TO_RAD ) );
        }

        // `vmsum3fp128 v0, v118, jVector` -> `vcmpgefp v0, v0, v11` -> `vnot128 v115, v0`
        // @0x826109C8/0x826109F8/0x82610A04. Same construction as Lean; consumed at the very end.
        const vpu::MaskScalar lbRotatedTooFar =
            vpu::CompLessThan( vpu::Splat( vpu::Dot( lTransform.yAxis, vpu::GetVector3_YAxis() ) ),
                               lMaxAngleCos );

        // ---- 1. closing speed at the contact point ----------------------------------------------
        // Same correction, and the same reason the DWARF's `lRaceCarToPropVector` (:1777) has no
        // local here -- read the Lean twin's block. The measurement in THIS body:
        //   `lvx128 v9, r3, r29(0x30)`   @0x826109D0  the car position, PRE-Translate
        //   `vsubfp128 v123, v122, v9`   @0x826109D8  r = lPointOnCar - thatPosition
        //   `vmr128 v121, v11`           @0x82610A00  mLinearVelocity, captured
        //   `vpermwi128 v8, v123, 0x63`  @0x82610A3C  the cross-product permute, on v123
        //   `vaddfp128 v12, v12, v121`   @0x82610AD4  + mLinearVelocity
        // all of it before the resolve store at 0x82610A7C.
        const Vector3 lPointVelocity =
            lpRaceCar->GetLocalVelocity( lPointOnCar, rw::physics::WORLD_SPACE );

        // `vmsum3fp128 v12, v12, v11(-lNormal)` @0x82610AD8 -> `vmaxfp128 v12, v12, v127(0)`
        // @0x82610B10.
        const VecFloat lVelocityAlongNormal =
            vpu::Max( vpu::Splat( vpu::Dot( lPointVelocity, -lNormal ) ), vpu::Splat( 0.0f ) );

        // ---- 2. bleed the car's momentum along the contact normal --------------------------------
        // `vmulfp128 v0, mTotalLinearForce, dt` ; `vmaddfp v0, mLinearVelocity, v0, mfMass` ;
        // `vaddfp v0, v0, mTotalLinearImpulse` ; dot with -lNormal ; Max(0) ; * lNormal *
        // KVF_MOMENTUM_RESOLUTION_FACTOR -> AddWorldSpaceImpulse @0x82610A08..0x82610A2C.
        const VecFloat lMomentumTowardsProp =
            vpu::Max( vpu::Splat( vpu::Dot( lpRaceCar->GetLinearMomentum( vpu::Splat( lfTimeStep ) ),
                                            -lNormal ) ),
                      vpu::Splat( 0.0f ) );

        lpRaceCar->AddWorldSpaceImpulse(
            lNormal * ( lMomentumTowardsProp.x * KVF_MOMENTUM_RESOLUTION_FACTOR.x ) );

        // ---- 3. push the car back out of the prop ------------------------------------------------
        // `vmsum3fp128 v13, v126, (lPointOnCar - lPointOnProp)` @0x82610A64 ; *
        // KVF_PENETRATION_RESOLUTION_FACTOR @0x82610A6C ; Max(0) @0x82610A74 ;
        // `vmaddfp128 v13, v126, v0, v13` @0x82610A78 into the car's position row, stored at
        // 0x82610A7C == ExternallySimulatedBody::Translate (the LINK HOLE, see the banner).
        const VecFloat lResolveDistance =
            vpu::Max( vpu::Splat( vpu::Dot( lNormal, lPointOnCar - lPointOnProp )
                                  * KVF_PENETRATION_RESOLUTION_FACTOR.x ),
                      vpu::Splat( 0.0f ) );
        const Vector3 lResolveVector = lNormal * lResolveDistance.x;
        lpRaceCar->Translate( lResolveVector );

        // ---- 4. the tilt axis ---------------------------------------------------------------------
        // `vmsum3fp128 v12, v126, kVector` @0x82610AA8 (kVector == unk_82181520, loaded at
        // 0x82610A94) then `vcmpgtfp128 v126, v12, v127(0)` @0x82610ACC. A BARE vcmpgtfp -- no
        // vnot -- so this one is CompGreaterThan and a NaN lane reads FALSE (gotcha 4; the pair
        // with lbRotatedTooFar above is deliberately asymmetric). DWARF local `MaskScalar
        // lNormalPointsAlongZ` @BrnPropManager.cpp:1964.
        const vpu::MaskScalar lNormalPointsAlongZ =
            vpu::CompGreaterThan( vpu::Splat( vpu::Dot( lNormal, vpu::GetVector3_ZAxis() ) ),
                                  vpu::Splat( 0.0f ) );

        // `vsel128 v126, v6, v9, v126` @0x82610AF4 with v6 == lTransform.xAxis (`vmr128 v6, v120`
        // @0x82610A58) and v9 == its sign flip (`vxor128 v9, v120, v9` @0x82610AEC, v9 being the
        // 0x80000000 splat from `vslw128 v9, v125, v125`). PPC vsel is `mask ? vB : vA`, so the
        // TRUE value is -xAxis and the FALSE value is +xAxis. No normalise: it is already a unit
        // basis row.
        const Vector3 lRotationAxis =
            vpu::Select( lNormalPointsAlongZ, -lTransform.xAxis, lTransform.xAxis );

        // `lfs f0, flt_82001C98(==1.0f)` ; `fdivs f0, f0, f31` @0x82610AB0/0x82610ABC.
        const f32 lfInvTimeStep = 1.0f / lfTimeStep;

        // Max(rear-left wheel road-long speed, lVelocityAlongNormal) * KVF_ROTATION_FACTOR,
        // ceilinged by KVF_MAX_ROTATION -- `vmaxfp v12, v10, v12` / `vmulfp128 v13, v12, v13` /
        // `vminfp128 v124, v13, v0` @0x82610B14/B18/B1C, with v10 == `vspltw v10, v12, 0` on the
        // register loaded from `addi r10, r30, 0x360` (@0x82610A48) ==
        // maWheels[eRearLeftWheel].mSpeedAndMassOnWheelVariables, lane .x.
        const VecFloat lfAngleToRotate =
            vpu::Min( vpu::Max( lpRaceCar->GetWheel( BrnPhysics::Vehicle::eRearLeftWheel )
                                    .GetRoadLongSpeed(),
                                lVelocityAlongNormal ) * KVF_ROTATION_FACTOR,
                      KVF_MAX_ROTATION );

        // `stvx128 v123, r11, r18` with r11 == (jointIndex + 0x1B) << 4 @0x82610B4C..0x82610B54,
        // i.e. this + 0x1B0 + 16*jointIndex == maLastJointRotation[jointIndex]. The `cmplwi 0xFF`
        // + assert "IsJointed()" (BrnPropInstance.h:326, `li r5,0x146`) immediately before it is
        // GetJointIndex()'s own baked tripwire and rides the by-name call.
        const s32 liJointIndex = lpPropInstance->GetJointIndex();
        maLastJointRotation[ liJointIndex ] =
            lRotationAxis * ( lfAngleToRotate.x * lfInvTimeStep );

        const Matrix44Affine lRotationMatrix =
            vpu::Matrix44AffineFromAxisRotationAngle( lRotationAxis, lfAngleToRotate );

        // BrnPropManager.cpp:1971 (`li r5,0x7B3`) -- all four rows NaN-swept before use.
        CGS_ASSERT( vpu::IsValid( lRotationMatrix.xAxis ) && vpu::IsValid( lRotationMatrix.yAxis )
                        && vpu::IsValid( lRotationMatrix.zAxis )
                        && vpu::IsValid( lRotationMatrix.wAxis ),
                    " Rotation matrix" );

        // ---- 5. swing the prop about its joint -----------------------------------------------------
        // MEASURED @0x82610FCC..0x826110D4, identical in shape to the Lean twin.
        const Vector3 lWorldSpaceJointPos = maPropJointPositions[ liJointIndex ];

        lTransform.wAxis = -lpType->GetJointLocator();
        lTransform       = lTransform * lRotationMatrix;
        lTransform.wAxis = lTransform.wAxis + lpType->GetJointLocator();
        lTransform.wAxis = lTransform.wAxis + lPos;

        const Vector3 lCurrentWorldSpaceJointPos =
            vpu::TransformPoint( lTransform, lpType->GetJointLocator() );
        const Vector3 lSeparation = lCurrentWorldSpaceJointPos - lWorldSpaceJointPos;
        lTransform.wAxis          = lTransform.wAxis - lSeparation;

        lpPropInstance->SetTransform( lTransform );

        // ---- 6. flag the joint for breaking ---------------------------------------------------------
        // `vcmpeqfp128. v0, v115, v127` + `bne cr6, loc_8261132C` @0x826110DC/0x826110EC -- the
        // same CR6 reading as the Lean twin (v115 == lbRotatedTooFar, v127 == zero), i.e.
        // MaskScalar::GetBool().
        if ( lbRotatedTooFar.GetBool() )
        {
            // .cpp:1994 (`li r5,0x7CA` @0x82611128, message aLppropinstance, file string r21 ==
            // aDP4B5MainBurno_225 == BrnPropManager.cpp) -- the Tilt twin of the Lean body's
            // .cpp:1877 tripwire; again distinct from the `li r5,0x146` IsJointed() asserts that
            // bracket it (0x826110FC / 0x82611148).
            CGS_ASSERT( liJointIndex != 0xff, "lpPropInstance->GetJointIndex() != 0xff" );

            // .cpp:1995 (`li r5,0x7CB`).
            CGS_ASSERT( mUsedPropJoints.IsBitSet( static_cast<u32>( liJointIndex ) ),
                        "mUsedPropJoints.IsBitSet( lpPropInstance->GetJointIndex() )" );

            // `addi r23, r18, 0x678` + sld/or/stdx @0x8261127C/0x82611314..0x82611328.
            mBreakPropJoints.SetBit( static_cast<u32>( liJointIndex ) );
        }

        // ---- 7. publish the new pose -----------------------------------------------------------------
        // MEASURED @0x8261132C..0x826113E0. The event is built on the stack at var_220 and its
        // field offsets fall out of the stores: +0x00..0x3F the four transform rows, +0x40 and
        // +0x50 two ZEROED vectors, +0x60 the entity id, +0x64 liPropIndex (sth), +0x66 zero (sth),
        // +0x68 zero (stb) -- exactly UpdatePropEvent's committed member sequence
        // (BrnPropEvents.h:16-23: mTransform, mLinearVelocity, mAngularVelocity, mEntityId,
        // miPhysicsSlot, miTypeId, mbFrozen).
        UpdatePropEvent lUpdateEvent;
        lUpdateEvent.mTransform = lTransform;
        lUpdateEvent.mLinearVelocity.SetZero();
        lUpdateEvent.mAngularVelocity.SetZero();

        // `ld r11, 0x30(r16)` when lbPropIsEntityA else `ld r11, 0x38(r16)` ; `srdi r11, r11, 32`
        // -- RigidBodyId::GetEntityId() is the HIGH dword of the 64-bit id (CgsRigidBody.h:53-57).
        // ⚠️ The parked body reached these two fields through a reinterpret_cast on a byte offset,
        // because the CgsPhysics event payload used to be modelled as an opaque span. THAT IS
        // STALE: mIDA/mIDB are typed, named u64 members today
        // (CgsPhysicsSimulationIO_Events.h:139-140, with offsetof static_asserts at :879-880), so
        // the by-name read below is both shorter and safer. The owner tripwire that follows
        // ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278, `li r5,0x116`) is
        // PropEntityID's own explicit-ctor assert and rides the by-name ctor.
        const CgsPhysics::RigidBodyId lPropRigidBodyId(
            lbPropIsEntityA ? lpOutContact->mIDA : lpOutContact->mIDB );

        lUpdateEvent.mEntityId =
            BrnWorld::PropEntityID( static_cast<u32>( lPropRigidBodyId.GetEntityId() ) );

        lUpdateEvent.miPhysicsSlot = static_cast<s16>( liPropIndex );
        lUpdateEvent.miTypeId      = 0;
        lUpdateEvent.mbFrozen      = false;

        // `addi r3, r18, 0x5E10` == &mUpdatedJointedProps (the EventQueue<UpdatePropEvent,15>,
        // BrnPropManager.h:779/:838).
        mUpdatedJointedProps.AddEvent( lUpdateEvent );
    }
}
}
