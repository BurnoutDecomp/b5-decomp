// GameSource/Physics/PropManager/PropManager_wQ2_05.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, ROUND 2 (2026-08-18), lander 05.
// Part-file of the TU GameSource/.../Physics/PropManager/BrnPropManager.cpp; the class banner,
// the member map and the tuning-global caveats live there and are NOT repeated here.
//
//     AddPropToSim()       @ 0x826274D8   (167 instructions, 0x826274D8..0x82627770)
//     BreakJoint()         @ 0x82628A88   (137 instructions, 0x82628A88..0x82628CA8)
//     UpdateJointedProps() @ 0x82631260   (938 instructions, 0x82631260..0x82632104)
//
// All three counts were COUNTED here from each export's own `assembly` array,
// (last - first)/4 + 1:  (0x82627770-0x826274D8)/4+1 == 167,
// (0x82628CA8-0x82628A88)/4+1 == 137, (0x82632104-0x82631260)/4+1 == 938. They agree with
// scratchpad/waveQ2/physfix.owner.md's table, which is a cross-check, not the source.
//
// THESE THREE ARE THE JOINTED-PROP (lean / tilt / smash-gate) SPINE, and they call each other:
//   UpdateJointedProps  is the per-frame driver. Pass 1 walks mUsedPropJoints and, for every
//                       joint whose bit is also set in mBreakPropJoints, calls BreakJoint.
//                       Pass 2 walks mUsedPropJoints again and lerps maCurrentJointTransforms
//                       toward each jointed prop's live transform, re-queuing one
//                       UpdatePropEvent per prop that actually moved.
//   BreakJoint          cuts one joint: remove the kinematic body, free the joint slot, hand
//                       the freed prop a spin + the tangential velocity that spin implies at
//                       the prop's centre, and re-add it as a free dynamic body.
//   AddPropToSim        builds the InAddRigidBody for a whole prop (inertia, mass, drag and
//                       velocity limits, optional centre-of-mass shift) and posts it.
//
// Callers, from the image's own xrefs_to:
//   AddPropToSim        <- ProcessAddPropInstanceEvents @0x82632108 (PropManager_wQ_02.cpp)
//                          and BreakJoint @0x82628A88 (below)
//   BreakJoint          <- UpdateJointedProps @0x82631260 (below)
//   UpdateJointedProps  <- ProcessInputsPreScene @0x8263AF30 (the sole caller, from xrefs_to;
//                          bodied at PropManager_wQ2_08.cpp -- that file is a concurrent lander's
//                          and its line numbers move, so only the address is cited)
//
// ==================================================================================================
// GOTCHA 1 (console literals are not host values) -- WHERE EACH ONE WENT
// ==================================================================================================
// Every console byte offset below appears in a COMMENT only:
//   * `lwz 0x7C(this)` / `mulli idx,0x70` == mpaPropInstances + idx*112 (CONSOLE stride) -> written
//     as mpaPropInstances[liPropIndex]; the host sizeof does the striding.
//   * `(idx>>6 + 0xCF)*8 + this` == &mBreakPropJoints, `+0x10` == &mUsedProps,
//     `addi r,this,0x670` == &mUsedPropJoints, `addi r,this,0x2A0` == &mauPropIndexForJoint,
//     `addi r,this,0x2B0 + idx*64` == &maCurrentJointTransforms[idx],
//     `(idx+12)*16` == &maPropJointPositions[idx] (0xC0 + idx*16),
//     `(idx+27)*16` == &maLastJointRotation[idx]  (0x1B0 + idx*16),
//     `addi r3,this,0x5E10` == &mUpdatedJointedProps -- all CONSOLE offsets, all reached BY NAME.
//   * `addi r3,this,0x54` == &mpPhysicsData, taken into ResourcePtr<T>::operator-> -- by name.
//   * `(typeId+4)*4 + header` == mapPropTypes[typeId] (the +0x10 array base) -- reached through
//     the committed PropPhysicsDataHeader::GetType, which carries both of its bounds asserts.
//   * the InAddRigidBody / InRemoveRigidBody / UpdatePropEvent stack records are filled BY MEMBER,
//     never by offset; the offset maps are quoted in the banners so the field identification is
//     auditable, and every one of those members is already pinned by a committed static_assert
//     (CgsPhysicsSimulationIO_Events.h:280-283 and its NewRigidBody static_assert block,
//     rw/physics/inertia.h::_rw_physics_Inertia_AssertLayout, BrnPropInstance.h's offsetof block).
// GOTCHA 3 (a float rides an FPR and SKIPS its GPR slot): AddPropToSim takes no f32 parameter, but
//   it DOES take two Vector3s, which ride v1/v2 and consume no GPR slot -- which is exactly why its
//   r10 is lpSimModuleInputBuffer (the 7th declared parameter) and the two vectors declared after
//   it are in vector registers. The committed signature already encodes that, and BreakJoint's
//   call site at 0x82628C80..0x82628CA0 matches it positionally (r4..r10 + v1/v2).
// GOTCHA 4 (NaN polarity): two compare-and-select sites exist and both are inside
//   rw::physics::Inertia::SetInverseInertia's inlined min (AddPropToSim 0x826276A4..0x826276C0),
//   spelled `fcmpu ; blt <skip> ; fmr` == `(a < b) ? a : b` -- a MIN whose UNORDERED case takes the
//   `fmr` and yields the SECOND operand. Reached by calling the committed named setter, whose
//   inline uses exactly those ternaries, so the NaN behaviour matches.
//   ⚠️ THE TWO `vcmpgtfp` TESTS IN UpdateJointedProps DO **NOT** MATCH LANE-FOR-LANE, and saying
//   they did would be the wrong-comment defect gotcha 9 exists to stop. Both are the SDK's
//   `IsZero(v, tolerance)` inlined, and the console lowers it as
//       !( any lane of |v| > tolerance )        -- `vcmpgtfp` + the CR6 "none compared true" bit
//   whereas the committed host inline (vector3_operation.h:269) spells it
//       all lanes of |v| <= tolerance
//   The two agree on every ORDERED value and differ ONLY when a lane is NaN: the console then says
//   "is zero" (NaN > tol is false) and the host says "is not zero" (fabs(NaN) <= tol is false).
//   The bodies below call the committed IsZero, because IsZero is what the DWARF callee list
//   attests the source called and because forking a second copy of a vendor predicate is worse
//   than a documented NaN edge. Consequence, stated: a jointed prop whose transform has gone NaN
//   is SKIPPED by the console and LERPED (then caught by the :392 assert) here. Filed as a
//   vendor-home note, not silently absorbed.
// GOTCHA 9 (a wrong comment is a real defect): every instruction address quoted below was read off
//   the per-address export this round; the four rodata reads that mattered were BYTE-DUMPED with
//   headless IDA 9.3 rather than inferred (scratchpad/waveQ2/probe_wQ2_05/ida_basis.txt,
//   ida_fmt.txt). Where something is an inference it says INFERENCE on its own line.
//
// ==================================================================================================
// ⛔ CALLEES / GLOBALS THAT ARE DECLARED BUT HAVE NO DEFINITION IN THE TREE
//    (`cl /c` cannot see an unresolved external, so this file gating green does NOT mean the TU
//     links). Reported, not stubbed, not invented:
//      BrnPhysics::Props::PropManager::GetPropInertia @0x82612640 (declared in BrnPropManager.h's GetPropInertia/GetPartInertia block; its
//          round-1 body is parked at scratchpad/waveQ/parked/
//          PropManager_04_GetPropInertia_GetPartInertia.cpp, blocked on the rw::collision::AABBox
//          include clash -- physfix.owner.md §5.1 REQUEST 1b)
//      the FIVE tuning globals declared at the top of this file (see their banner) -- they need a
//          declaration in BrnPropManager.h and a definition in BrnPropManager.cpp, exactly like
//          their fifteen already-homed siblings.
//    Everything else these three bodies call has a real body -- each one is named at its call site.
// ==================================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"

#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"       // PropInstance
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"            // UpdatePropEvent
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                      // PropEntityID
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"             // PropPhysicsDataHeader::GetType
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"               // PropTypeData
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"      // InputBuffer + the two queue getters
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // InAddRigidBody / InRemoveRigidBody / NewRigidBody
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                      // CgsPhysics::RigidBodyId
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                    // BitArray<15>
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT + Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"                  // CgsDev::StrStream (the :392 / :393 messages)
#include "rw/physics/rigidbody.h"                                             // rw::physics::STATIC_BODY / ACTIVE_BODY
#include "rw/physics/inertia.h"                                               // rw::physics::Inertia setters
#include "rw/math/vpu/vector3_operation.h"                                    // Abs / IsZero / Cross / Dot / NormalizeFast / MagnitudeSquared
#include "rw/math/vpu/matrix44affine_operation.h"                             // IsValid(Matrix44Affine)

namespace BrnPhysics
{
namespace Props
{

// =================================================================================================
// ⚠️⚠️ FIVE TUNING GLOBALS THAT HAVE NO DECLARATION ANYWHERE IN THE TREE YET -- DECLARED HERE,
// DEFINED NOWHERE, AND FILED AS A HEADER REQUEST. THIS FILE IS LINK-RED UNTIL THEY LAND.
//
// BrnPropManager.h already carries fifteen of this .cpp's tuning globals (the seven the debug
// component registers plus the eight the keystone wave added). These five are the ones ONLY these
// three bodies read, so nothing had reason to declare them before. This lander may not edit a
// header, so they are DECLARED here and reported; nothing is defined, so there is no chance of an
// ODR clash when BrnPropManager.h/.cpp grow them (a second identical `extern` declaration is legal
// and inert -- a second DEFINITION would not be, which is why there is none here).
//
// WHERE THE NAMES COME FROM -- the same method the header's other eight used, and it is not a
// guess: the DecFIGS global list for THIS EXACT .cpp
// (references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp, the
// `BrnPhysics::Props` namespace block) names every namespace-scope constant in the file WITH its
// source line and its const-ness, and each of the five addresses below plays exactly one role in
// the asm. Both halves are recorded per line.
//
//   DWARF :338 const VecFloat KVF_LEAN_PROP_LERP_SPEED
//   DWARF :339 const VecFloat KVF_LEAN_PROP_MIN_LERP
//   DWARF :340 const VecFloat KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE
//        -- the three constants declared IMMEDIATELY BEFORE UpdateJointedProps' own body scope
//           (DWARF puts that at BrnPropManager.cpp:343), and UpdateJointedProps reads exactly
//           three zero-page VecFloats. The role split inside those three is what pins which is
//           which; see each line.
//   DWARF :2046 VecFloat KVF_BREAK_JOINT_LINEAR_VEL
//   DWARF :2047 VecFloat KVF_BREAK_JOINT_ANGULAR_VEL
//        -- declared immediately before BreakJoint's body scope (DWARF :2054), and BreakJoint
//           reads exactly two zero-page VecFloats: one scales an ANGULAR quantity, one scales a
//           LINEAR one. Not const in the DWARF, unlike the three above -- transcribed as declared.
//
// ⭐ STATUS 2026-08-18 (round 3b): THE HEADER REQUEST THAT STOOD HERE IS DONE, AND THE VALUES ARE
//    RECOVERED. Both halves are now closed and this block is the record of what happened, not a
//    request:
//      * DECLARATIONS: all five are declared in BrnPropManager.h and defined in BrnPropManager.cpp,
//        so the file-local `extern` re-declarations this file used to carry -- which were the other
//        half of the request -- are deleted. The per-constant ROLE evidence that used to hang off
//        those re-declarations moved WITH them onto the header declarations; nothing was dropped.
//      * VALUES: the note here claimed "their real values come from a dynamic initialiser that is
//        not in the ARTIST export set (only readers are)". The first clause was right, the second
//        was measured FALSE. The initialisers ARE in the image -- MSVC dynamic-initialiser thunks
//        that sit outside every IDA function, hence invisible to any scan built on the per-address
//        function exports. Measured values, provenance on each header declaration:
//            KVF_LEAN_PROP_LERP_SPEED           = Splat(0.1f)
//            KVF_LEAN_PROP_MIN_LERP             = Splat(0.01f)
//            KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE = Splat(0.1f)
//            KVF_BREAK_JOINT_LINEAR_VEL         = Splat(1.0f)
//            KVF_BREAK_JOINT_ANGULAR_VEL        = Splat(1.0f)
//        The old "CONSEQUENCE" paragraph (a leaning prop's visual transform never catching up; a
//        broken sign post released with zero spin and zero throw) described what THIS TREE would
//        have shipped with the placeholder zeroes, never what the console does. It is retired with
//        the claim it rested on.
// =================================================================================================

// (The five file-local `extern` re-declarations that used to sit here are DELETED -- they were the
//  pre-header workaround, and BrnPropManager.h now declares all five. Their ROLE evidence moved
//  onto those header declarations verbatim.)


// =================================================================================================
// The three SDK basis vectors UpdateJointedProps' orthogonality tripwire subtracts.
//
// ⭐ MEASURED, NOT ASSUMED -- byte-dumped this round (scratchpad/waveQ2/probe_wQ2_05/ida_basis.txt):
//     0x82181500  rw::math::vpu::detail::gIVector   3f800000 00000000 00000000 00000000  (1,0,0,0)
//     0x82181510  (unnamed in the .i64)             00000000 3f800000 00000000 00000000  (0,1,0,0)
//     0x82181520  (unnamed in the .i64)             00000000 00000000 3f800000 00000000  (0,0,1,0)
//     0x82181530  (unnamed in the .i64)             00000000 00000000 00000000 3f800000  (0,0,0,1)
// i.e. the SDK's I/J/K/L basis run, of which the first three are read here. Only gIVector carries
// a symbol in the database; the other two are named by position in that run and by the role the
// asm gives them (each is subtracted from the Gram-matrix row that must equal it). They are
// file-local `static` (internal linkage) here because rw::math::vpu::detail has no home in this
// tree -- reported as a header request rather than left as three bare literals inside the assert.
// =================================================================================================
static const Vector3 K_RW_I_VECTOR = { 1.0f, 0.0f, 0.0f, 0.0f };   // X360 0x82181500
static const Vector3 K_RW_J_VECTOR = { 0.0f, 1.0f, 0.0f, 0.0f };   // X360 0x82181510
static const Vector3 K_RW_K_VECTOR = { 0.0f, 0.0f, 1.0f, 0.0f };   // X360 0x82181520


// =================================================================================================
// BrnPhysics::Props::PropManager::AddPropToSim @ 0x826274D8   (167 instructions)
// DWARF: class decl BrnPropManager.h:313, body scope BrnPropManager.cpp:568.
//
// Build and post the InAddRigidBody for ONE WHOLE PROP.
//
// ---- REGISTER MAP, prologue 0x826274F4..0x82627524 ---------------------------------------------
//   r3 = this (r30)          r4 = lEntityId (r31)        r5 = liPropIndex (r28)
//   r6 = &lTransform (r26)   r7 = lpType (r29)           r8 = lbStatic (r27)
//   r9 = lbAddExtraComOffset (r25)                       r10 = lpSimModuleInputBuffer (r24)
//   v1 = lLinearVelocity (saved to v127)   v2 = lAngularVelocity (saved to v126)
// The by-value Matrix44Affine rides a hidden reference in r6 and the two Vector3s ride v1/v2 --
// see the gotcha-3 note in the file banner.
//
// ---- THE InAddRigidBody STACK RECORD, offset map ------------------------------------------------
// Record base == var_130 (frame 0xA0), the pointer handed to AddEvent. Every offset below is a
// store this body actually makes, matched to the member a committed static_assert already pins:
//   +0x00 mID                                `std`        0x826275AC
//   +0x10..+0x40 mRigidBody.mTransform       four stvx    0x826276DC..0x8262770C  (+0x40 rewritten
//                                                          by the extra-COM arm at 0x82627740)
//   +0x50 mRigidBody.mVelocity               `stvx128 v127` 0x82627568
//   +0x60 mRigidBody.mAngularVelocity        `stvx128 v126` 0x82627574
//   +0x70 mRigidBody.mInertia.mInvTens       `stvx128 v0`   0x826276A8
//   +0x80 mRigidBody.mInertia.mInvMass       `stfs`         0x826275C0 (and again 0x826275DC)
//   +0x84 mRigidBody.mInertia.mSpherical     `stfs`         0x826276EC
//   +0x88 mRigidBody.mInertia.mMaxVelocity   `stfs`         0x8262759C
//   +0x8C mRigidBody.mInertia.mMaxOmega      `stfs`         0x826275A8
//   +0x90 mRigidBody.mInertia.mLinearDrag    `stfs`         0x82627594
//   +0x94 mRigidBody.mInertia.mAngularDrag   `stfs`         0x8262758C
//   +0xA0 mRigidBody.mbSpy                   `stb 1`        0x826275CC
//   +0xB0 meState                            `stw`          0x826275C4
// (NewRigidBody sits at InAddRigidBody+0x10 and Inertia at NewRigidBody+0x60, so Inertia+0x18 ==
//  record+0x88 and so on.) Nothing writes Inertia's +0x98/+0x9C tail padding or the +0xA1..+0xAF
//  gap, so the record ships with those bytes as stack garbage, exactly as on the console.
//
// ---- THE DWARF LOCALS --------------------------------------------------------------------------
//   RigidBodyId lRigidBodyId @570 - InAddRigidBody lSimulationAddBodyEvent @571 -
//   Vector3 lInverseInertia @572 - NewRigidBody* lpNewRigidBody @587 - float32_t lrRatio @617 -
//   Vector3 lInertia @628.
// ⚠️ lpNewRigidBody @587 is a `NonConstructedClassContainer<NewRigidBody>::GetObjectPointer()`
//    result -- the source reached the embedded record through that accessor and then wrote through
//    the pointer. This tree embeds `NewRigidBody mRigidBody;` BY VALUE (the container is modelled
//    as raw storage -- see that header's NewRigidBody / InAddRigidBody blocks) and GetObjectPointer has no home
//    anywhere (physfix.owner.md §3.3 lists it among the missing), so the members are reached
//    directly. Same record, same stores; one accessor fewer.
// ⚠️ lrRatio @617: INFERENCE, and the weaker of the two readings (round-2 NIT -- this used to be
//    stated as fact). The DWARF scopes `float32_t lrRatio` (BrnPropManager.cpp:617) inside a nested
//    block whose ONLY three callees are the per-axis reciprocal divides
//    (`operator/<VectorAxisX/Y/Z>`, dumpfile :508-:510), which argues lrRatio is the per-axis ratio
//    local of the inverse-inertia fold -- i.e. the `1.0f / (lInertia.axis * KVF_INERTIA_SCALE.x)`
//    divisions below -- rather than the spherical minimum, which Inertia::SetInverseInertia (named
//    separately at :477, OUTSIDE that block) derives and stores at Inertia+0x14. Either way it is
//    not a separate local in the emission, so nothing below changes.
// =================================================================================================
void PropManager::AddPropToSim( PropEntityID                                  lEntityId,
                                s32                                           liPropIndex,
                                Matrix44Affine                                lTransform,
                                const PropTypeData*                           lpType,
                                bool                                          lbStatic,
                                bool                                          lbAddExtraComOffset,
                                CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
                                Vector3                                       lLinearVelocity,
                                Vector3                                       lAngularVelocity )
{
    // 0x82627508..0x82627548 -- `srwi r11, r31, 24 ; cmplwi cr6, r11, 3`, fired at
    // BrnPropEntityID.h:278 with "mEntityId.GetOwner() == E_ENTITYTYPE_PROP". That is
    // PropEntityID::AssertIsProp() inlined; the console HOISTED it to the prologue, ahead of every
    // other instruction in the body, but the only thing that consumes the entity word is the
    // RigidBodyId packing below, whose GetValue() carries exactly this assert
    // (BrnPropEntityID.cpp:48-52 -- "AssertIsProp() then return the raw word"). Reaching it through
    // GetValue() reproduces the assert without adding a second copy of the owner test.
    // ⚠️ WHICH ACCESSOR THE SOURCE USED (round-2 NIT): the DWARF names
    // `BrnWorld::PropEntityID::operator CgsSceneManager::EntityId()` in this body's local block
    // (dumpfile BrnPropManager.cpp:452), not GetValue(). GetValue() is the STAND-IN, chosen because
    // it runs the same AssertIsProp() while returning the raw word the packing needs; behaviourally
    // identical, so nothing is dropped -- but the DWARF grounding is the conversion operator.

    CgsPhysics::PhysicsSimulationIO::InAddRigidBody lSimulationAddBodyEvent;

    // 0x82627568 / 0x82627574 -- the two parameter vectors go straight into the record.
    lSimulationAddBodyEvent.mRigidBody.mVelocity        = lLinearVelocity;
    lSimulationAddBodyEvent.mRigidBody.mAngularVelocity = lAngularVelocity;

    // ⚠️⚠️ MEASURED CROSS-WIRING -- transcribed as it is, NOT "corrected". The identical pair of
    // stores exists in CreatePart, and PropManager_wQ2_04.cpp's banner records the full two-sided
    // evidence (the address->name map from PropDebugComponent::OnActivate's RegisterVariable
    // labels, and the offset->member map from rw::physics::RigidBody::DynamicUpdate @0x82BC2B78).
    // Re-measured HERE this round: `lfs f0, (flt_82F2A390)` @0x82627598 -> `stfs var_A8`
    // (record+0x88 == Inertia+0x18 == mMaxVelocity, the LINEAR clamp), and
    // `lfs f0, (flt_82F2A394)` @0x826275A0 -> `stfs var_A4` (record+0x8C == Inertia+0x1C ==
    // mMaxOmega, the ANGULAR clamp). So the ANGULAR-named constant lands on the LINEAR clamp and
    // vice versa, in this body too. Whether the shipped SOURCE crossed the arguments or one of the
    // two name maps is mislabelled is NOT decidable from the asm; this file reproduces the stores.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetMaxLinearVelocity(  KF_PROP_MAX_ANGULAR_VEL ); // -> +0x18
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetMaxAngularVelocity( KF_PROP_MAX_LINEAR_VEL );  // -> +0x1C
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetLinearDrag(  KF_PROP_LINEAR_DRAG );            // -> +0x20
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetAngularDrag( KF_PROP_ANGULAR_DRAG );           // -> +0x24
    // ⚠️ KF_PROP_RESTITUTION @0x82F2A398 is the fifth constant of that rodata run and is NOT read
    //    anywhere in these 167 instructions -- stated because BrnPropManager.h's AddPropToSim banner describes this
    //    body as reading "the five KF_PROP_* drag/limit scalars". It is FOUR. Measured: the only
    //    loads off the flt_82F2A394 anchor are at -0xC / -0x8 / -0x4 / 0, i.e. 0x82F2A388,
    //    0x82F2A38C, 0x82F2A390 and 0x82F2A394. There is no load of +0x4.

    // :570. `sldi r7, r31, 32 ; clrlwi r6, r28, 16 ; or ; std` == (entityWord << 32) | (idx & 0xFFFF)
    // -- the packed CgsPhysics::RigidBodyId (high dword = the EntityId, low 16 bits = the index;
    // CgsRigidBody.h's banner and GetIndex() both say so). The DWARF names the packer
    // CgsPhysics::RigidBodyId::Set; that multi-argument setter is deliberately NOT declared in this
    // tree (CgsRigidBody.h's banner says why), so the shift/or is spelled here, exactly as the
    // three committed siblings in PropManager_wQ2_04.cpp do.
    const CgsPhysics::RigidBodyId lRigidBodyId(
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( liPropIndex ) ) );

    lSimulationAddBodyEvent.mID = lRigidBodyId;

    // 0x82627558 `lfs f13, 0x38(r29)` == PropTypeData::mfMass (console +0x38), then
    // 0x826275BC `fdivs f0, f31, f13` with f31 == flt_82001C98. That constant was byte-dumped this
    // round as 3f800000 == 1.0f (scratchpad/waveQ2/probe_wQ2_05/ida_basis.txt), which is also what
    // rw/physics/inertia.h:63 already records.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseMass( 1.0f / lpType->GetMass() );

    // 0x82627550..0x826275C8 -- `clrlwi r11, r27, 24 ; subfic r11, r11, 0 ; subfe r8, r11, r11 ;
    // rlwinm r10, r8, 0,31,29 ; addi r11, r10, 4`. Worked through: subfic leaves CA==1 exactly when
    // lbStatic is 0, so subfe yields 0 for a dynamic prop and -1 for a static one; the rlwinm mask
    // (MB=31 > ME=29) is 0xFFFFFFFD, so r10 is 0 or 0xFFFFFFFD; +4 gives 4 or 1. That is the
    // branchless form of `lbStatic ? STATIC_BODY : ACTIVE_BODY` (rigidbody.h:77-80 -- STATIC_BODY
    // == 1, ACTIVE_BODY == 4).
    lSimulationAddBodyEvent.meState = lbStatic ? rw::physics::STATIC_BODY : rw::physics::ACTIVE_BODY;

    // 0x826275C8/0x826275CC `li r11,1 ; stb r11, var_90` == mbSpy = TRUE.
    // ⚠️ MEASURED DIFFERENCE FROM CreatePart, stated rather than harmonised: the PART path
    //    (PropManager_wQ2_04.cpp, `stb 0` @0x82627D0C) sets mbSpy FALSE. Whole props are spied,
    //    shed parts are not. Both are transcriptions, not choices.
    lSimulationAddBodyEvent.mRigidBody.mbSpy = true;

    // 0x82627550 `lbz r9, 0x49(r30)` == mbUseOverides, tested at 0x82627570 and branched at
    // 0x826275D0; the taken arm reloads `lfs f0, 0x4C(r30)` == mfMassOverride and OVERWRITES the
    // inverse mass just stored (the console really does compute both -- there are two `stfs` into
    // the same slot). Two SetInverseMass calls is exactly what the DWARF callee list shows.
    if ( mbUseOverides )
    {
        lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseMass( 1.0f / mfMassOverride );
    }

    // :628. 0x826275EC `bl PropManager::GetPropInertia` with r3 = &result (the hidden sret
    // pointer), r4 = this, r5 = lpType -- a normal by-value return here.
    // ⛔ GetPropInertia HAS NO BODY IN THE TREE (BrnPropManager.h's GetPropInertia/GetPartInertia block declares it; its round-1 body
    //    is parked on the rw::collision::AABBox include clash). Link-red, gate-green.
    const Vector3 lInertia = GetPropInertia( lpType );

    // :572. 0x826275F0..0x82627694. Each axis is splatted, multiplied by the WHOLE
    // KVF_INERTIA_SCALE register (`lvx128 v0, flt_82FB9400` -> three vmulfp128), and only LANE 0 of
    // each product is read back (`stvx` then `lfs` at offset 0) before `fdivs f0, f31(1.0f), f0`.
    // So the scalar taken from the VecFloat is its lane 0, and the result is the per-axis
    // reciprocal of the scaled inertia -- identical to CreatePart's block.
    // The vperm/vrlimi128 pair at 0x8262768C/0x82627690 is how the compiler reassembles the three
    // scalars: the control vector unk_82CDA350 was byte-dumped this round as
    // `00 01 02 03 | 14 15 16 17 | 00 01 02 03 | 00 01 02 03`, i.e. dest word0 = vA word0 and dest
    // word1 = vB word1, and the vrlimi128 then drops the third scalar into lane 2. Lane 3 is a
    // duplicate of lane 0 and is never read.
    // ⭐ KVF_INERTIA_SCALE == Splat(3.0f), RECOVERED 2026-08-18 round 3b (thunk 0x82C5E6C0,
    //    rodata flt_82004270 = 0x40400000; see BrnPropManager.h). The warning that stood here --
    //    "reads all-zero on disk and has no recovered initialiser ... every division below is
    //    1.0f/0.0f" -- was half right: the disk bytes really are zero, but the initialiser exists
    //    as a dynamic-initialiser thunk. The divisions are now by 3x the raw inertia, which is what
    //    an INERTIA SCALE of 3 means: props resist spin three times as hard as their box tensor.
    Vector3 lInverseInertia;
    lInverseInertia.x = 1.0f / ( lInertia.x * KVF_INERTIA_SCALE.x );
    lInverseInertia.y = 1.0f / ( lInertia.y * KVF_INERTIA_SCALE.x );
    lInverseInertia.z = 1.0f / ( lInertia.z * KVF_INERTIA_SCALE.x );

    // :617. 0x82627698..0x826276EC -- mInvTens := lInverseInertia, then
    // mSpherical := 1.0f / min(x, min(y, z)), which IS rw::physics::Inertia::SetInverseInertia
    // (inertia.h:117) inlined. GOTCHA 4: both selects are `fcmpu ; blt <skip> ; fmr`, i.e.
    // `(a < b) ? a : b` with the UNORDERED case taking the `fmr`; the committed inline spells it
    // with the same ternaries, so the NaN behaviour matches lane for lane.
    // ⚠️ Same harmless scheduling difference CreatePart records: the committed inline stores the
    //    intermediate min(x,y) to +0x14 before the second compare, and here that intermediate store
    //    is optimised away (only the final `stfs` at 0x826276EC lands). Same final value.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseInertia( lInverseInertia );

    // 0x826276C8..0x8262770C -- four lvx/stvx pairs off r26 at +0/+0x10/+0x20/+0x30.
    lSimulationAddBodyEvent.mRigidBody.mTransform = lTransform;

    // 0x82627710..0x82627740, gated on `clrlwi r8, r25, 24 ; cmplwi cr6, r8, 0`.
    // The arm splats K_PROP_EXTRA_COM_OFFSET's three lanes and folds
    //     vmulfp128 v13, row0, splat(off.x)                 == row0 * off.x
    //     vmaddfp   v13, row1, v13, splat(off.y)            == row1 * off.y + that
    //     vmaddfp   v0,  row2, v13, splat(off.z)            == row2 * off.z + that
    //     vaddfp    v0,  row3, v0                           == Pos + that
    // (AltiVec vmaddfp vD,vA,vB,vC is vD = vA*vC + vB -- vB is the ADDEND, vC the second multiplier.
    //  Getting that backwards is the operand-order defect wave Q round 1 shipped once already.)
    // So the body origin is shifted by the offset ROTATED into world space by the transform's 3x3,
    // written back over the record's mTransform.wAxis only. ReadUpdatedBodies @0x82632A7C undoes
    // exactly this on the way back, gated on PropInstance::KU_HAS_EXTRA_COM_OFFSET_FLAG.
    // ⚠️ The console writes the shifted position into the RECORD (record+0x40 == mTransform.wAxis);
    //    the caller's lTransform is a by-value copy and is not the storage being written. Spelled
    //    through the record member so that is unambiguous.
    // ⭐ K_PROP_EXTRA_COM_OFFSET == (0, 0, -0.2), RECOVERED 2026-08-18 round 3b (Vector3 thunk
    //    0x82C5E7F0, z from rodata flt_82020A84 = 0xBE4CCCCD; see BrnPropManager.h). The note that
    //    stood here said it "reads all-zero on disk ... this whole arm is a no-op today"; the disk
    //    bytes are zero but the initialiser exists, and this arm is live: a 20 cm centre-of-mass
    //    shift along the prop's own -Z, rotated into world space by the transform below.
    // ⭐ SPELLED THROUGH THE HOMED HELPER (round-2 MUST_FIX, applied): the cascade above IS
    //    rw::math::vpu::TransformVector, which has a real vendor home at
    //    vendor/renderware/include/rw/math/vpu/matrix44affine_operation.h:41 -- a header this file
    //    already includes -- and the DWARF names exactly `Matrix44Affine::Pos` / `TransformVector` /
    //    `operator+=` for this body (dwarfdump BrnPropManager.cpp:501/:502/:503). It was previously
    //    hand-inlined here, which forked a homed helper silently; AGENTS.md's "inlining reversal"
    //    rule says to spell the call. (`operator+=` has no vendor home, so the `Pos() = Pos() + ...`
    //    long form stands in for it; Pos() is types.h:81, a reference to wAxis.)
    // ⚠️ THE ONE LANE THIS CHANGES, RECORDED SO IT IS NOT RE-DERIVED AS A BUG: the console's
    //    `vaddfp v0, v10, v0` @0x8262773C is 4-lane, so its w is wAxis.w + xAxis.w*off.x +
    //    yAxis.w*off.y + zAxis.w*off.z, whereas the committed TransformVector forces its result's
    //    w = 0.0f and the add therefore leaves wAxis.w unchanged. For a well-formed affine -- rows'
    //    w == 0, which is what Matrix44Affine::SetIdentity seeds and what every producer of this
    //    transform ships -- the two are identical. Contrast ReadUpdatedBodies' decode (G)
    //    (PropManager_wQ2_01.cpp), where the mirror-image undo keeps the longhand precisely because
    //    the packed w scalars there are LIVE.
    if ( lbAddExtraComOffset )
    {
        lSimulationAddBodyEvent.mRigidBody.mTransform.Pos() =
            lSimulationAddBodyEvent.mRigidBody.mTransform.Pos()
            + rw::math::vpu::TransformVector( lTransform, K_PROP_EXTRA_COM_OFFSET );
    }

    // 0x8262774C `bl CgsPhysics::PhysicsSimulationIO::InputBuffer::GetAddRigidBodyQueue`
    // (IDA truncates the symbol to `CgsPhysics__Ph`; it is 0x825BCE08, the write-lock-guarded
    // producer overload whose "Not locked for writing\n" assert bakes
    // CgsPhysicsSimulationModuleIO.h:1059 -- the same callee CreatePart uses), then
    // 0x82627754 `bl BaseEventQueue<InAddRigidBody>::AddEvent`.
    lpSimModuleInputBuffer->GetAddRigidBodyQueue()->AddEvent( lSimulationAddBodyEvent );
}


// =================================================================================================
// BrnPhysics::Props::PropManager::BreakJoint @ 0x82628A88   (137 instructions)
// DWARF: class decl BrnPropManager.h:336, body scope BrnPropManager.cpp:2054.
//
// THE BREAK ITSELF for a jointed prop -- the smash gate, the leaning sign post, the tilting
// lamppost. The prop was being driven kinematically about a fixed joint; this cuts it loose:
// remove the old body, free the joint slot, mark the prop dynamic and un-jointed, give it the spin
// the joint had accumulated plus the tangential velocity that spin implies at its centre, and
// re-add it as a free rigid body through AddPropToSim.
//
// ---- REGISTER MAP, prologue 0x82628A94..0x82628AAC ---------------------------------------------
//   r3 = this (r28)   r4 = lEntityId (r25)   r5 = liPropIndex (r26)
//   r6 = &lTransform (r23)   r7 = lpType (r27)   r8 = lpSimModuleInputBuffer (r24)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE ---------------------------------------------------------
//   BrnPropEntityID.h:278 (0x116)   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP"
//   BrnPropInstance.h:326 (0x146)   "IsJointed()"           -- GetJointIndex's own tripwire
//   BrnPropManager.cpp:2066 (0x812) "lpProp->IsJointed()"
//   CgsBitArray.h:241 (0xF1)        "luIndex < NUMBITS"     -- UnSetBit's bound, against 0xF == 15
//
// ---- THE DWARF LOCALS --------------------------------------------------------------------------
//   InRemoveRigidBody lSimulationRemoveBodyEvent @2057 - RigidBodyId lPropRigidBodyId @2058 -
//   PropInstance* lpProp @2064 - int32_t liJointIndex @2065 - Vector3 lAngularVelocity @2072 -
//   Vector3 lLinearVelocity @2073.
// The declaration lines settle the ordering: the angular velocity is built first (:2072) and the
// linear one is derived from it (:2073), which is also what the emission does.
// =================================================================================================
void PropManager::BreakJoint( PropEntityID                                  lEntityId,
                              s32                                           liPropIndex,
                              Matrix44Affine                                lTransform,
                              const PropTypeData*                           lpType,
                              CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer )
{
    // :2057 / :2058. 0x82628AD8..0x82628AEC -- the same `sldi r10, r25, 32 ; clrlwi r11, r26, 16 ;
    // or ; std` packing every sibling uses, and the owner tripwire fired at 0x82628AB8 is
    // PropEntityID::AssertIsProp() inlined and hoisted to the prologue -- reached through GetValue().
    // ⚠️ Same DWARF caveat as AddPropToSim's (round-2 NIT): this body's DWARF local block names
    // `BrnWorld::PropEntityID::operator CgsSceneManager::EntityId()`, not GetValue(); GetValue() is
    // the stand-in that carries the same AssertIsProp().
    CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lSimulationRemoveBodyEvent;
    const CgsPhysics::RigidBodyId lPropRigidBodyId(
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( liPropIndex ) ) );

    lSimulationRemoveBodyEvent.mID = lPropRigidBodyId;
    // ⚠️ Same console quirk RemovePart/RemoveProp record: the 16-byte stack record's
    //    mbFailIfRigidBodyNotFound byte at +8 is never written (measured -- the only store into
    //    var_60 is the `std` at 0x82628AEC), so on the X360 it is uninitialised stack. The host
    //    sets it FALSE == the tolerant remove, the committed treatment of the identical quirk in
    //    PropManager_wQ2_04.cpp's `PropManager::RemovePart` and BrnPhysicalBodyPartPool_Remove.cpp:51.
    lSimulationRemoveBodyEvent.mbFailIfRigidBodyNotFound = false;

    // 0x82628AF0 `bl sub_825BCF58` == InputBuffer::GetRemoveRigidBodyQueue() (the write-lock
    // producer overload; identified in PropManager_wQ2_04.cpp's `PropManager::RemovePart` by its baked
    // CgsPhysicsSimulationModuleIO.h:1080 assert), then
    // 0x82628AF8 `bl BaseEventQueue<InRemoveRigidBody>::AddEvent`.
    lpSimModuleInputBuffer->GetRemoveRigidBodyQueue()->AddEvent( lSimulationRemoveBodyEvent );

    // :2064. `lwz r10, 0x7C(r28)` + `mulli r11, r26, 0x70` == mpaPropInstances + idx*112 (CONSOLE
    // stride); indexed by name here.
    PropInstance* lpProp = &mpaPropInstances[ liPropIndex ];

    // :2065 / :2066. The console reads `lbz 0x6C(r30)` TWICE -- once at 0x82628B08 for
    // GetJointIndex's own "IsJointed()" tripwire (BrnPropInstance.h:326, and note its polarity:
    // `cmplwi 0xFF ; bne <skip>` fires the assert when the slot IS the KU_NOT_JOINTED sentinel),
    // and once at 0x82628B34 for the value plus the .cpp:2066 assert. Calling the committed
    // GetJointIndex() (BrnPropInstance.cpp:66, which carries that exact assert) and then the
    // separate CGS_ASSERT reproduces both, in order.
    const s32 liJointIndex = lpProp->GetJointIndex();
    CGS_ASSERT( lpProp->IsJointed(), "lpProp->IsJointed()" );

    // UnSetBit's own bound tripwire (CgsBitArray.h:241, `cmplwi cr6, r31, 0xF` at 0x82628B64 --
    // 15 == mUsedPropJoints' capacity), then the ld/andc/std at 0x82628BB8..0x82628BC4. The message
    // is the binary's own. `addi r29, r28, 0x670` == &mUsedPropJoints, reached by name.
    CGS_ASSERT( static_cast<u32>( liJointIndex ) < mUsedPropJoints.GetCapacity(), "luIndex < NUMBITS" );
    mUsedPropJoints.UnSetBit( static_cast<u32>( liJointIndex ) );

    // 0x82628BCC `stb r8(=0xFF), 0x6C(r30)` then 0x82628BDC `stb r11(=0), 0x6D(r30)` -- in that
    // order. +0x6C is mu8JointIndex (SetNotJointed writes KU_NOT_JOINTED == 255) and +0x6D is
    // mbIsStatic; both offsets are pinned by BrnPropInstance.h's offsetof block.
    lpProp->SetNotJointed();
    lpProp->SetIsStatic( false );

    // :2072. 0x82628BF0/0x82628C08 -- `lvx128 v11, r5, r28` with r5 == (liJointIndex + 27) * 16 ==
    // 0x1B0 + liJointIndex*16 == &maLastJointRotation[liJointIndex] (CONSOLE offsets; reached by
    // name), multiplied lane-by-lane by the whole KVF_BREAK_JOINT_ANGULAR_VEL register. The
    // constant is a broadcast VecFloat, so taking lane 0 is identical -- the same treatment
    // PropManager_wQ2_04.cpp gives KVF_INERTIA_SCALE.
    const Vector3 lAngularVelocity = maLastJointRotation[ liJointIndex ] * KVF_BREAK_JOINT_ANGULAR_VEL.x;

    // :2073. 0x82628BD0..0x82628C28. `lvx128 v0, r30, 0x30` is the prop's own world position
    // (PropInstance::mWorldTransform.wAxis, +0x30 inside the affine at +0x00) and
    // `lvx128 v12, r7, r28` with r7 == (liJointIndex + 12) * 16 == 0xC0 + liJointIndex*16 ==
    // &maPropJointPositions[liJointIndex]; `vsubfp` gives the joint->prop lever arm.
    //
    // The five instructions at 0x82628C14..0x82628C24 are the textbook AltiVec cross product, and
    // it was decoded rather than assumed: `vpermwi128 v,v,0x63` is the (y,z,x,w) word rotation
    // (0x63 == 01 10 00 11), and `vnmsubfp vD,vA,vB,vC` is vD = vB - vA*vC, so the pair computes
    //     (v2*lever.yzx - v2.yzx*lever) == (cross.z, cross.x, cross.y)
    // which the final vpermwi128 rotates back to (cross.x, cross.y, cross.z) -- i.e. exactly
    // Cross(lAngularVelocity, lever), with lAngularVelocity FIRST. The DWARF callee list for this
    // body names rw::math::vpu::Cross once, between the two operator* calls, which agrees.
    const Vector3 lLinearVelocity =
        Cross( lAngularVelocity,
               lpProp->GetTransform().Pos() - maPropJointPositions[ liJointIndex ] )
        * KVF_BREAK_JOINT_LINEAR_VEL.x;

    // 0x82628C04..0x82628C60 -- `lwz r11, 0x58(r27)` == PropTypeData::muSceneUriId, compared
    // against 0x6894C == 428364 and 0x68964 == 428388, and the boolean drives an OR-2 / AND-~2 on
    // `lbz 0x6F(r30)` == PropInstance::mu8Flags bit 1 == KU_HAS_EXTRA_COM_OFFSET_FLAG. That id pair
    // is PropTypeData::HACKShouldMoveComOffset() (BrnPhysicsPropTypeData.h:222), and the DWARF
    // callee list names it TWICE in this body -- which is what the emission does: it re-loads
    // +0x58 and redoes the same compare at 0x82628C64 for the AddPropToSim argument, rather than
    // reusing the value. Both calls are spelled.
    lpProp->SetExtraComOffsetFlag( lpType->HACKShouldMoveComOffset() );

    // 0x82628C80..0x82628CA0 -- the tail call. r6 is the ORIGINAL lTransform parameter (r23), NOT
    // the prop's own transform; r8 == 0 is lbStatic (a broken prop is dynamic by construction);
    // r9 is the second HACKShouldMoveComOffset(); v1/v2 already hold the two velocities.
    AddPropToSim( lEntityId,
                  liPropIndex,
                  lTransform,
                  lpType,
                  false,
                  lpType->HACKShouldMoveComOffset(),
                  lpSimModuleInputBuffer,
                  lLinearVelocity,
                  lAngularVelocity );
}


// =================================================================================================
// BrnPhysics::Props::PropManager::UpdateJointedProps @ 0x82631260   (938 instructions)
// DWARF: class decl BrnPropManager.h:151, body scope BrnPropManager.cpp:343.
//
// The per-frame jointed-prop driver, and the second-largest body in this class. TWO passes, both
// over mUsedPropJoints (the console re-walks the same bit set from scratch -- measured: the second
// walk re-materialises `addi r10, r22, 0x670` at 0x82631894 rather than reusing pass 1's cursor):
//
//   PASS 1 (0x82631280..0x8263188C) -- for every live joint whose bit is ALSO set in
//     mBreakPropJoints, call BreakJoint. Note BreakJoint clears the joint's mUsedPropJoints bit but
//     NOT its mBreakPropJoints bit; the walk re-reads the array each step, so the freed slot simply
//     stops being visited. Transcribed as-is -- whoever owns the producer of mBreakPropJoints
//     should know the request bit is not self-clearing here.
//
//   PASS 2 (0x82631890..0x82632104) -- for every live joint, lerp the manager's own
//     maCurrentJointTransforms[joint] toward the prop's live world transform, re-orthonormalise the
//     3x3, store it back, and queue one UpdatePropEvent on mUpdatedJointedProps. Props whose
//     transform has not moved enough are skipped entirely.
//
// ---- REGISTER MAP, prologue 0x82631274..0x82631288 ---------------------------------------------
//   r3 = this (r22, and spilled to arg_14 because both passes reload it across calls)
//   r4 = lpSimModuleInputBuffer (spilled to arg_1C at 0x82631278; the ONLY use is BreakJoint's 5th
//        argument at 0x82631690)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE ---------------------------------------------------------
//   CgsBitArray.h:203 (0xCB)        IsBitSet's bound tripwire -- the StrStream form,
//                                   "invalid index : " << i << " < " << 15. THREE copies in pass 1
//                                   (the joint index @0x82631364, the prop index @0x826314BC, and
//                                   the walk cursor @0x826316C8) and ONE in pass 2's walk
//                                   @0x82631F0C.
//                                   ⚠️ Only TWO of those four are spelled below (the joint and prop
//                                   indices). The other two live inside GetNextNonZeroBit's inlined
//                                   IsBitSet, and the committed CgsBitArray.h is deliberately
//                                   assert-free (see its GetBitRange note), so they are
//                                   unreproducible by design -- recorded here rather than spelled.
//   BrnPropManager.cpp:359 (0x167)  "mUsedProps.IsBitSet( liPropIndex )"
//   BrnPropPhysicsDataHeader.h:173/174 (0xAD/0xAE)  GetType's two bounds asserts, inlined
//   BrnPropManager.cpp:392 (0x188)  the IsValid(lLerpMatrix) tripwire, StrStream message
//   BrnPropManager.cpp:393 (0x189)  the IsOrthogonal3x3(lLerpMatrix) tripwire, StrStream message
//
// ---- THE DWARF LOCALS (dwarfdump BrnPropManager.cpp:2090-2148) ---------------------------------
// The nesting is explicit in the dump and it is what settles which loop owns what:
//   function scope : int32_t liPropIndex @345 - PropInstance* lpProp @346 -
//                    Matrix44Affine lTransform @347 - lDesiredTransform @348 - lLerpMatrix @349 -
//                    UpdatePropEvent lUpdateEvent @350 - bool lbNeedsRotating @351
//     nested block : int32_t liJointIndex @354                       <- PASS 1's cursor
//     nested block : int32_t liPropIndex @358 - PropInstance* lpProp @360 -
//                    const PropTypeData* lpType @361                 <- PASS 1's body (shadows)
//     nested block : int32_t liLeaningPropIndex @367                 <- PASS 2's cursor
// so the seven function-scope locals are PASS 2's working set, hoisted to the top of the function
// in the source. INFERENCE, flagged: the dump gives declaration lines, not statement order, so
// "which of the two loops came first in the .cpp" is not directly attested -- what IS attested is
// the emission order (break first, update second), and that is the order written below.
//
// ---- THE CALLEE LIST THE DWARF NAMES, AND WHERE EACH ONE LANDS ---------------------------------
//   BitArray<15>::{GetFirstNonZeroBit, GetNextNonZeroBit, IsBitSet} - ResourcePtr<>::operator-> -
//   PropInstance::GetTypeId - PropPhysicsDataHeader::GetType - Matrix44Affine::Matrix44Affine/
//   operator= - operator- x4, Abs x4, operator+ x3, IsZero  (the lbNeedsRotating test) -
//   NormalizeFast x3, operator*=, operator+ x4              (the lerp) -
//     ⚠️ Those counts were MACHINE-COUNTED over the dumpfile block (round-2 MUST_FIX; the earlier
//     "operator- x3, Abs x3, operator+ x2" was wrong and, worse, was used below as the reason the
//     code emits three Abs): over
//     references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp lines
//     2090-2263 the totals are Abs=4, operator-=4, operator+=7 (3 in the test + 4 in the lerp),
//     operator*==1, MagnitudeSquared=4, NormalizeFast=3, IsZero=2, SetZero=2, IsBitSet=2,
//     IsOrthogonal3x3=1, SelfSubtract=1, Matrix44Affine ctor=3, operator= =4. The code below
//     matches that exactly -- four deltas, four Abs, four subtractions.
//   IsValid, IsOrthogonal3x3, SelfSubtract, MagnitudeSquared x4, IsZero  (the two tripwires) -
//   Vector3::SetZero x2, BaseEventQueue<UpdatePropEvent>::AddEvent  (the event).
// Every one of those is spelled below except IsOrthogonal3x3, which has no home in this tree --
// see its block.
// =================================================================================================
void PropManager::UpdateJointedProps( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer )
{
    // =============================================================================================
    // PASS 1 -- break every joint flagged this frame.
    // 0x82631280 `addi r10, r22, 0x670` == &mUsedPropJoints, then the ld/cmpldi/cntlzd sequence at
    // 0x8263130C..0x82631338 which is BitArray<15>::GetFirstNonZeroBit inlined (including its
    // `>= tuNumBits -> KI_INVALID_BITINDEX` guard, emitted as `cmpwi r11, 0xF ; bge <exit>`), and
    // the block at 0x826316A8..0x8263188C which is GetNextNonZeroBit inlined.
    // =============================================================================================
    for ( s32 liJointIndex = mUsedPropJoints.GetFirstNonZeroBit();
          liJointIndex != CgsContainers::BitArray<15>::KI_INVALID_BITINDEX;
          liJointIndex = mUsedPropJoints.GetNextNonZeroBit( liJointIndex ) )
    {
        // IsBitSet's own bound tripwire (CgsBitArray.h:203, `cmplwi cr6, r27, 0xF` at 0x82631364).
        // The console builds its message with a StrStream -- "invalid index : " << liJointIndex <<
        // " < " << 15 -- through the same BasePriorityQueue::Clear + AppendFormat pair every other
        // BitArray tripwire in this cluster uses; spelled with the short form here, exactly as the
        // committed siblings in PropManager_wQ2_04.cpp's RemovePart/RemoveProp do.
        CGS_ASSERT( static_cast<u32>( liJointIndex ) < mBreakPropJoints.GetCapacity(), "invalid index" );

        // 0x8263147C..0x826314B0 -- `(liJointIndex>>6 + 0xCF)*8 + this`. 0xCF*8 == 0x678 ==
        // &mBreakPropJoints (mUsedPropJoints is 0x670; the two BitArray<15>s are adjacent). CONSOLE
        // offset, reached by name -- and the 0x678/0x670 pair is the arithmetic self-check that the
        // two names are not swapped.
        if ( mBreakPropJoints.IsBitSet( static_cast<u32>( liJointIndex ) ) )
        {
            // :358. 0x826314B4/0x826314B8 `add r11, r27, r22 ; lbz r28, 0x2A0(r11)` ==
            // mauPropIndexForJoint[liJointIndex] (a u8 array; CONSOLE +0x2A0, by name here).
            const s32 liPropIndex = mauPropIndexForJoint[ liJointIndex ];

            // The second CgsBitArray.h:203 copy, this one on the prop index (`cmplwi cr6, r28, 0xF`
            // at 0x826314BC), then :359 itself at 0x826315D4..0x82631620 --
            // `(liPropIndex>>6 + 0x10)*8 + this`, 0x10*8 == 0x80 == &mUsedProps.
            CGS_ASSERT( static_cast<u32>( liPropIndex ) < mUsedProps.GetCapacity(), "invalid index" );
            CGS_ASSERT( mUsedProps.IsBitSet( static_cast<u32>( liPropIndex ) ),
                        "mUsedProps.IsBitSet( liPropIndex )" );

            // :360. `lwz r10, 0x7C(r22)` + `mulli r11, r28, 0x70` == mpaPropInstances + idx*112.
            PropInstance* lpProp = &mpaPropInstances[ liPropIndex ];

            // :361. 0x82631630..0x82631684 -- `addi r3, r22, 0x54` into
            // CgsResource::ResourcePtr<T>::operator-> (mpPhysicsData), `lwz r30, 0x64(r31)` ==
            // PropInstance::muTypeId, then GetType INLINED: both of its bounds asserts
            // (BrnPropPhysicsDataHeader.h:173 against 0x1F4 == KU_MAX_PROP_TYPES, and :174 against
            // the header's own muNumberOfPropTypes at +0x00) followed by
            // `addi r11, r30, 4 ; slwi r11, r11, 2 ; lwzx r7, r11, r29` == mapPropTypes[typeId]
            // (the array base is header+0x10 == word 4). Calling the committed GetType reproduces
            // all three, in order.
            const PropTypeData* lpType = mpPhysicsData->GetType( lpProp->GetTypeId() );

            // 0x82631688..0x826316A4 -- r4 = `lwz 0x60(r31)` == PropInstance::mEntityId,
            // r6 = r31 itself == &lpProp->mWorldTransform (the affine is at +0x00), r8 reloads the
            // spilled lpSimModuleInputBuffer.
            BreakJoint( lpProp->GetEntityId(),
                        liPropIndex,
                        lpProp->GetTransform(),
                        lpType,
                        lpSimModuleInputBuffer );
        }
    }

    // =============================================================================================
    // PASS 2 -- lerp every live joint's cached transform toward the prop's live transform.
    // 0x82631890 re-walks mUsedPropJoints from the start.
    // =============================================================================================
    for ( s32 liLeaningPropIndex = mUsedPropJoints.GetFirstNonZeroBit();
          liLeaningPropIndex != CgsContainers::BitArray<15>::KI_INVALID_BITINDEX;
          liLeaningPropIndex = mUsedPropJoints.GetNextNonZeroBit( liLeaningPropIndex ) )
    {
        // :345 / :346. 0x8263196C..0x826319B0 -- `lbzx r27, r11, r26` with r11 == this + 0x2A0
        // (mauPropIndexForJoint) and `mulli r11, r27, 0x70` + `lwz r10, 0x7C(r30)` for the
        // instance. ⚠️ MEASURED: this pass emits NO bounds assert on either index -- there is no
        // `cmplwi 0xF` and no `bl BeginAssert` between 0x8263196C and 0x826319B8. Pass 1 has three
        // and this pass has none; that asymmetry is the shipped image's, not an omission here.
        const s32 liPropIndex = mauPropIndexForJoint[ liLeaningPropIndex ];
        PropInstance* lpProp  = &mpaPropInstances[ liPropIndex ];

        // :347 / :348. 0x8263199C `addi r31, r11, 0x2B0` with r11 == this + liLeaningPropIndex*64
        // == &maCurrentJointTransforms[liLeaningPropIndex] (CONSOLE +0x2B0, 64-byte affine stride;
        // reached by name), and the four lvx128 off r28 == the prop's own mWorldTransform.
        // ⚠️ WHICH IS WHICH, and it matters: the SUBTRACTION at 0x826319C0..0x826319D4 is
        //    `prop - cached`, and the sum is added back onto `cached`. So the manager's cached
        //    joint transform is what LERPS, and the prop's live transform is the TARGET. That is
        //    why the DWARF calls the cached one lTransform and the live one lDesiredTransform.
        const Matrix44Affine lTransform        = maCurrentJointTransforms[ liLeaningPropIndex ];
        const Matrix44Affine lDesiredTransform = lpProp->GetTransform();

        const Vector3 lDeltaXAxis = lDesiredTransform.xAxis - lTransform.xAxis;
        const Vector3 lDeltaYAxis = lDesiredTransform.yAxis - lTransform.yAxis;
        const Vector3 lDeltaZAxis = lDesiredTransform.zAxis - lTransform.zAxis;
        const Vector3 lDeltaWAxis = lDesiredTransform.wAxis - lTransform.wAxis;

        // :351. 0x826319D8..0x82631A10 -- four `vandc <row>, 0x80000000` (the sign-bit clear that
        // is Abs; the mask is built by `vspltisw128 v127,-1` + `vslw128`), three `vaddfp`, then a
        // fifth `vandc` and `vcmpgtfp. v13, v8` against KVF_LEAN_PROP_MIN_LERP. The fifth Abs and
        // the compare together ARE rw::math::vpu::IsZero(v, tolerance) inlined
        // (vector3_operation.h:269 -- |lane| <= tolerance over xyz).
        // ⚠️ STATED CORRECTLY (round-2 MUST_FIX -- the earlier wording said the DWARF names
        // "three Abs calls and one IsZero rather than four", and a reader trusting it would delete
        // the wAxis term): the DWARF names Abs FOUR times and IsZero twice. The four SOURCE-level
        // Abs calls are the four deltas here (x/y/z/w -> the four vandc at 0x826319D8/DC/E0/E4) and
        // the FIFTH vandc @0x826319F4 is IsZero's OWN inlined Abs, which has no separate DWARF entry
        // -- so five vandc for four Abs plus one IsZero is exactly what Abs x4 + IsZero predicts.
        // Four Abs is right; do not "correct" it down.
        // GOTCHA 4: the branch is `mfocrf` + CR6 bit 2 ("no lane compared greater") + `bne <skip>`,
        // i.e. the whole body runs only when SOME lane exceeded the tolerance. ⚠️ The committed
        // IsZero spells that as `all lanes <= tolerance`, which is the SAME for every ordered value
        // and the OPPOSITE for a NaN lane -- see the gotcha-4 block in the file banner. Documented,
        // not silently absorbed.
        const bool lbNeedsRotating =
            !IsZero( Abs( lDeltaXAxis ) + Abs( lDeltaYAxis ) + Abs( lDeltaZAxis ) + Abs( lDeltaWAxis ),
                     KVF_LEAN_PROP_MIN_LERP.x );

        if ( lbNeedsRotating )
        {
            // :349. 0x82631A14..0x82631AD0. Each row's delta is scaled by
            // KVF_LEAN_PROP_LERP_SPEED and added back onto the cached row; the three BASIS rows are
            // then renormalised and the translation row is not.
            // The normalise is `vmsum3fp128` + `vrsqrtefp` + ONE Newton-Raphson refinement
            // (`vnmsubfp` / `vmaddfp` with the 1.0f and 0.5f that `vcfsx v13,1,0` / `vcfsx v13,1,1`
            // materialise) -- that estimate-plus-one-step pipeline is rw::math::vpu::NormalizeFast,
            // which the DWARF callee list names three times. The committed NormalizeFast
            // (vector3_operation.h:165) reduces to the exact Normalize on the host: numerically
            // tighter than the console's estimate, never a placeholder. Standing convention of
            // that vendor home.
            Matrix44Affine lLerpMatrix;
            lLerpMatrix.xAxis = NormalizeFast( lTransform.xAxis + lDeltaXAxis * KVF_LEAN_PROP_LERP_SPEED.x );
            lLerpMatrix.yAxis = NormalizeFast( lTransform.yAxis + lDeltaYAxis * KVF_LEAN_PROP_LERP_SPEED.x );
            lLerpMatrix.zAxis = NormalizeFast( lTransform.zAxis + lDeltaZAxis * KVF_LEAN_PROP_LERP_SPEED.x );
            lLerpMatrix.wAxis = lTransform.wAxis + lDeltaWAxis * KVF_LEAN_PROP_LERP_SPEED.x;

            // :392. 0x82631A98..0x82631C4C -- twelve `vspltw` + `vcmpeqfp.` self-equality tests
            // (three lanes x four rows, COUNTED: the first pair is 0x82631A98/0x82631AA4 and the
            // last is 0x82631C14/0x82631C1C) ANDed together at 0x82631C38..0x82631C40, i.e.
            // rw::math::vpu::IsValid(Matrix44Affine) (matrix44affine_operation.h:276) inlined lane
            // for lane. Branch at 0x82631C4C.
            if ( !IsValid( lLerpMatrix ) )
            {
                // 0x82631C50..0x82631CD4 -- the console builds the message in
                // CgsDev::Assert::gpcMessageBuffer with a stack StrStream:
                //     << "Transform: "        (the virtual char* sink, `bctrl` through vtable+4)
                //     << lLerpMatrix          (`bl sub_821F0FC0`)
                //     << "\n"
                //     FireAssert(gpcMessageBuffer, BrnPropManager.cpp, 392)
                // ⚠️ ONE UNAVOIDABLE SPELLING CHANGE, FLAGGED -- the same one, for the same reason,
                //    that PropManager_wQ2_04.cpp's RemoveProp added-this-frame block records for the Vector3
                //    printer. 0x821F0FC0 is
                //    `operator<<(StrStreamBase&, const Matrix44Affine&)`; its format string was
                //    dumped this round from off_82F31970 -> 0x820DBED8 and is exactly
                //    "\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n"
                //    (scratchpad/waveQ2/probe_wQ2_05/ida_fmt.txt). That free operator has NO home
                //    anywhere in this tree, and this lander may not add one, so the sixteen lanes go
                //    through StrStreamBase::AppendFormat with the console's OWN format string --
                //    which is what that callee does internally. Byte-identical output, one call
                //    frame fewer. Replace with `<< lLerpMatrix` the moment the operator is homed
                //    (reported as a header request).
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );

                lStrStream << "Transform: ";
                lStrStream.AppendFormat(
                    "\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n",
                    lLerpMatrix.xAxis.x, lLerpMatrix.xAxis.y, lLerpMatrix.xAxis.z, lLerpMatrix.xAxis.w,
                    lLerpMatrix.yAxis.x, lLerpMatrix.yAxis.y, lLerpMatrix.yAxis.z, lLerpMatrix.yAxis.w,
                    lLerpMatrix.zAxis.x, lLerpMatrix.zAxis.y, lLerpMatrix.zAxis.z, lLerpMatrix.zAxis.w,
                    lLerpMatrix.wAxis.x, lLerpMatrix.wAxis.y, lLerpMatrix.wAxis.z, lLerpMatrix.wAxis.w );
                lStrStream << "\n";

                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ ); // console: BrnPropManager.cpp:392
                CgsDev::Assert::EndAssert();
            }

            // :393. 0x82631CFC..0x82631DD8 -- the ORTHOGONALITY tripwire, which the DWARF names
            // `rw::math::vpu::IsOrthogonal3x3` and whose body it also names piecewise
            // (SelfSubtract, MagnitudeSquared x4, IsZero). That helper has NO home in this tree
            // (matrix44affine_operation.h has IsValid / OrthoNormalize3x3 but no predicate), and
            // this lander may not add one, so it is spelled inline here -- with a header request to
            // fold it into the vendor home. Nothing is invented: what follows is the emission.
            //
            // DECODED, instruction by instruction:
            //   0x82631CFC..0x82631D44  three vmrghw/vmrglw pairs that transpose the 3x3 into its
            //       COLUMNS: (r0.x,r1.x,r2.x), (r0.y,r1.y,r2.y), (r0.z,r1.z,r2.z).
            //   0x82631D48..0x82631D7C  three vmulfp + six vmaddfp: each column set is combined
            //       with the splatted components of ONE row, so lane i of each result is
            //       Dot(row_i, that row) -- the three rows of M * transpose(M).
            //   0x82631D80..0x82631D8C  each of those is subtracted from one basis vector:
            //       the row-0 product from gIVector (1,0,0), the row-1 product from
            //       0x82181510 (0,1,0), the row-2 product from 0x82181520 (0,0,1). All three
            //       constants were BYTE-DUMPED this round, they are not inferred.
            //   0x82631D90..0x82631DB0  three vmsum3fp128 (== MagnitudeSquared) reassembled into
            //       one vector by `vperm` with unk_82CDA350 plus `vrlimi128 ..,2,0`.
            //   0x82631DB4..0x82631DC4  a FOURTH vmsum3fp128 over that vector, `vandc` (abs) and
            //       `vcmpgtfp` against KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE -- i.e. IsZero(.., tol)
            //       inlined again, on a broadcast scalar.
            const Vector3 lGramErrorX = Vector3{ Dot( lLerpMatrix.xAxis, lLerpMatrix.xAxis ),
                                                 Dot( lLerpMatrix.yAxis, lLerpMatrix.xAxis ),
                                                 Dot( lLerpMatrix.zAxis, lLerpMatrix.xAxis ),
                                                 0.0f } - K_RW_I_VECTOR;
            const Vector3 lGramErrorY = Vector3{ Dot( lLerpMatrix.xAxis, lLerpMatrix.yAxis ),
                                                 Dot( lLerpMatrix.yAxis, lLerpMatrix.yAxis ),
                                                 Dot( lLerpMatrix.zAxis, lLerpMatrix.yAxis ),
                                                 0.0f } - K_RW_J_VECTOR;
            const Vector3 lGramErrorZ = Vector3{ Dot( lLerpMatrix.xAxis, lLerpMatrix.zAxis ),
                                                 Dot( lLerpMatrix.yAxis, lLerpMatrix.zAxis ),
                                                 Dot( lLerpMatrix.zAxis, lLerpMatrix.zAxis ),
                                                 0.0f } - K_RW_K_VECTOR;

            const Vector3 lOrthogonalError = Vector3{ MagnitudeSquared( lGramErrorX ),
                                                      MagnitudeSquared( lGramErrorY ),
                                                      MagnitudeSquared( lGramErrorZ ),
                                                      0.0f };
            const f32 lfOrthogonalError = MagnitudeSquared( lOrthogonalError );

            if ( !IsZero( Vector3{ lfOrthogonalError, lfOrthogonalError, lfOrthogonalError, 0.0f },
                          KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE.x ) )
            {
                // 0x82631DDC..0x82631E60 -- the same StrStream message and the same un-homed matrix
                // printer as the :392 arm; see that block's flag.
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );

                lStrStream << "Transform: ";
                lStrStream.AppendFormat(
                    "\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n",
                    lLerpMatrix.xAxis.x, lLerpMatrix.xAxis.y, lLerpMatrix.xAxis.z, lLerpMatrix.xAxis.w,
                    lLerpMatrix.yAxis.x, lLerpMatrix.yAxis.y, lLerpMatrix.yAxis.z, lLerpMatrix.yAxis.w,
                    lLerpMatrix.zAxis.x, lLerpMatrix.zAxis.y, lLerpMatrix.zAxis.z, lLerpMatrix.zAxis.w,
                    lLerpMatrix.wAxis.x, lLerpMatrix.wAxis.y, lLerpMatrix.wAxis.z, lLerpMatrix.wAxis.w );
                lStrStream << "\n";

                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ ); // console: BrnPropManager.cpp:393
                CgsDev::Assert::EndAssert();
            }

            // 0x82631E90..0x82631EA4 -- four stvx128 back through r31 ==
            // &maCurrentJointTransforms[liLeaningPropIndex]. This is the ONLY writer of that array
            // in this body.
            maCurrentJointTransforms[ liLeaningPropIndex ] = lLerpMatrix;

            // :350. 0x82631EA8..0x82631EE4 -- the UpdatePropEvent stack record. Offset map, with
            // the record base at var_120 (frame 0x160) and every member DECLARED at
            // BrnPropEvents.h:15-24 (⚠️ DECLARED, not "pinned" -- round-2 NIT: there is no
            // static_assert for UpdatePropEvent anywhere; BrnPropEvents.h's only two pins are
            // RemovePhysicalPropEvent :75 and RemovePhysicalPartEvent :90. The member ORDER at
            // :17-23 is what yields these offsets, and this file has now measured all seven off the
            // asm, so an offsetof/sizeof pin block for them is worth a header request):
            //   +0x00..+0x30 mTransform         four stvx128
            //   +0x40 mLinearVelocity           `stvx128 v11` (v11 == vspltisw 0)
            //   +0x50 mAngularVelocity          `stvx128 v11`
            //   +0x60 mEntityId                 `stw`  from `lwz 0x60(r28)`
            //   +0x64 miPhysicsSlot             `sth r27`  == liPropIndex
            //   +0x66 miTypeId                  `sth 0`
            //   +0x68 mbFrozen                  `stb 0`
            // Nothing writes +0x69..+0x6F, so that tail ships as stack garbage, as on the console.
            UpdatePropEvent lUpdateEvent;
            lUpdateEvent.mTransform = lLerpMatrix;
            lUpdateEvent.mLinearVelocity.SetZero();
            lUpdateEvent.mAngularVelocity.SetZero();
            lUpdateEvent.mEntityId     = lpProp->GetEntityId();
            lUpdateEvent.miPhysicsSlot = static_cast<s16>( liPropIndex );
            lUpdateEvent.miTypeId      = 0;
            lUpdateEvent.mbFrozen      = false;

            // 0x82631E9C `addi r3, r30, 0x5E10` == &mUpdatedJointedProps (the
            // EventQueue<UpdatePropEvent,15>, CONSOLE +0x5E10; reached by name) then
            // 0x82631EE8 `bl BaseEventQueue<UpdatePropEvent>::AddEvent`.
            // ⚠️ mUpdatedJointedProps, NOT mUpdatedProps (+0x680): measured, the base register is
            //    r30 + 0x5E10 and nothing in this body touches +0x680.
            mUpdatedJointedProps.AddEvent( lUpdateEvent );
        }
    }
}

}
}
