// GameSource/Physics/PropManager/PropManager_wQ2_01.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, ROUND 2 (2026-08-18), lander 01.
// Part-file of the TU GameSource/.../Physics/PropManager/BrnPropManager.cpp; the class block
// banner, the member map and the retired-park list live there and are NOT repeated here.
//
//     ClampAcceleration()      @ 0x82627F00   (125 instructions, 0x82627F00..0x826280F0)
//     ApplyAntiHerdingForce()  @ 0x826113F8   (114 instructions, 0x826113F8..0x826115BC)
//     ReadUpdatedBodies()      @ 0x82632918   (752 instructions, 0x82632918..0x826334D4)
//
// (All three counts RE-COUNTED this round from the exports' own `assembly` arrays -- non-blank
//  lines, first and last address printed. Round 1's banners said 78 / 87 and cited no count for
//  the third; 78 and 87 were wrong and are corrected here and nowhere else claimed.)
//
// ==================================================================================================
// WHY THIS FILE EXISTS -- three round-1 bodies, complete but parked on missing DECLARATIONS
// ==================================================================================================
// All three reconstructions were finished in round 1 and parked out-of-tree because four
// declarations they call did not exist. Round 2's shared-header owners landed all four; this file
// is the landing, with every MUST_FIX / NIT the round-1 verifiers filed against the parked text
// applied. Parked originals (kept for provenance, superseded by this file):
//     scratchpad/waveQ/parked/PropManager_05_ClampAcceleration.cpp
//     scratchpad/waveQ/parked/PropManager_05_ApplyAntiHerdingForce.cpp
//     scratchpad/waveQ/parked/PropManager_06_ReadUpdatedBodies.cpp
//
// THE FOUR DECLARATIONS, AS THEY NOW STAND IN THE TREE (verified by reading them this round):
//
//   1. GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h:190, class InputBuffer,
//          InUpdateRigidBodyQueue*  GetUpdateRigidBodyQueue();            // @0x825BCCB8
//      the NON-const write twin, beside its const twin at :175. Body at
//      CgsPhysicsSimulationModuleIO_InputBuffer.cpp:232. X360: write-lock guard
//      (`lbz r11,0(r28)` + `extrwi r11,r11,1,28` == MSB0 bit 28 == LSB bit 3 ==
//      eStatusLockedForWrite -- NOT the const twin's `,1,27`) firing "Not locked for writing\n"
//      with `li r5,0x414` == source line 1044, then `addis r3,r28,1 ; addi r3,r3,-0x69E0`
//      == this + 0x9620 == +38432 == &mUpdateRigidBodyQueue. BOTH instructions matter.
//
//   2. same header :201,
//          InApplyForceQueue*       GetApplyForceQueue();                 // @0x825BCD60
//      body at CgsPhysicsSimulationModuleIO_InputBuffer.cpp:248. Same guard, baked line
//      :1051 (`li r5,0x41B`), then `addis r3,r28,1 ; addi r3,r3,0x2C30`
//      == this + 0x12C30 == +76848 == &mApplyForceQueue.
//      ⚠️ BOTH instructions are quoted deliberately. Round 1 quoted only the `addi` and asserted
//      "== this + 76848"; 0x2C30 alone is 11312 and the `addis` carries the other 0x10000. The
//      OFFSET was right and matches the committed member, but the quote as written was
//      arithmetically false. (Round-1 G6 NIT 4, applied.)
//
//   3. GameShared/GameClasses/Module/CgsBaseEventQueue.h:120,
//          T* AllocateEventSafe();                                        // @0x825E3C30
//      the bounds-GATED reserve: assert "mpEvents != NULL" (CgsBaseEventQueue.h:381,
//      `li r5,0x17D`), then `lwz 8 / lwz 4 / cmpw / bge -> li r3,0 ; blr` == SIGNED
//      miLength >= miMaxLength returns NULL WITHOUT appending, else reserve the tail slot and
//      bump miLength. ⚠️ NOT interchangeable with the committed no-arg `T& AddEvent()`, which
//      appends unconditionally.
//
//   4. BrnPropManager.h -- the ClampAcceleration and ApplyAntiHerdingForce parameter NAMES were
//      corrected to the DecFIGS DWARF's in round 2 (name-only; no type, order or count change).
//      Declaration and the definitions below now agree spelling for spelling.
//
// ⛔⛔ LINK-LEVEL ITEM FOR THE CONDUCTOR -- REPORTED, NOT ACTED ON (AGENTS.md gotcha 7)
//     A LOUD one-shot inert gate for ReadUpdatedBodies is committed at
//         b5-decomp/src/GameSource/Physics/BrnPhysicsConductorGates.cpp:499
//         BRN_CONDUCTOR_GATE("PropManager::ReadUpdatedBodies @0x82632918 (752)")
//     with a byte-identical signature. The real body below is therefore a DUPLICATE AT LINK TIME
//     (LNK2005) the moment this file is mounted. Gate retirement is conductor-only and must ship
//     in the SAME commit that adds this file to tools/build/build_game_exe.bat. This lander did
//     NOT delete it. ClampAcceleration and ApplyAntiHerdingForce have no gate or stub anywhere
//     (re-grepped BrnPhysicsConductorGates.cpp and WorldLinkStubs.cpp this round).
//
// ⛔ THE LINK HOLE THIS FILE ACTUALLY REPORTS (re-grepped 2026-08-18, round-3 fix):
//     CgsDev::DebugRender::DrawAxis(const f32*) -- declared CgsDebugRender.h:104 (that header's own
//     banner marks it DECLARATION-ONLY), with NO definition in CgsDebugRender.cpp, in any other
//     .cpp under the DebugSystem Render directory, or in WorldLinkStubs.cpp.
//     PRE-EXISTING, not introduced here: BehaviourRig.cpp:189
//     is the first caller and ReadUpdatedBodies' mbRenderCOM arm is the second. Alongside it,
//     CgsDev::DebugInterface::GetRender resolves only to the inert-but-ASSERTING link stub at
//     WorldLinkStubs.cpp:1605. Both sit on the same debug arm.
//     ✅ NOT a hole any more (round-2 correction; the round-1/round-2 banner claimed these two were
//     un-bodied and that was FALSE by the time this wave landed): PropManager::RemoveProp
//     @0x8260F540 and PropManager::RemovePart @0x8260F988 are BODIED by the sibling round-2 part-file
//     b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_04.cpp -- as
//     `PropManager::RemoveProp` and `PropManager::RemovePart` in that file (cited by NAME, not by
//     line: the line numbers this banner used to carry went stale the first time either file was
//     edited). Mount that file alongside this one. Do NOT stub either -- a trap stub beside
//     the real body is an LNK2005 that `cl /c` cannot see. Every other callee of all three bodies is
//     homed (census below).
//
// ⚠️ CONSOLE-VALUE DISCIPLINE (AGENTS.md gotcha 1): not one console offset, stride or record size
//    appears as a host value in any of the three bodies. Every field is reached by name or through
//    a committed accessor; the +0xNN / *192 / *112 / *64 numbers are in COMMENTS only, as the
//    evidence that fixes WHICH member or WHICH accessor.
// ==================================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"   // InputBuffer + queues
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"  // In/OutUpdateRigidBody,
                                                                           // InApplyForce
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                   // CgsPhysics::RigidBodyId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "rw/physics/rigidbody.h"                                          // RigidBody::operator=,
                                                                           // Set*Velocity, ResetForces
#include "rw/math/vpu/vector3_operation.h"                                 // Magnitude / MagnitudeSquared
                                                                           // Dot / Add / Subtract / Mult
#include "rw/math/vpu/vector4_operation.h"                                 // VecFloat Splat / Min / Max
#include "rw/math/vpu/matrix44affine_operation.h"                          // TransformPoint /
                                                                           // InverseOfMatrixWithOrthonormal3x3
#include <cmath>                                                           // std::fabs (the `vandc`)
#include <stdlib.h>                                                        // getenv -- [DIAG] BRN_PROP_DIAG only, host-side

namespace BrnPhysics
{
namespace Props
{

namespace
{
    // MEASURED (round 1, headless IDA 9.3 byte read of IDA Files/BURNOUT_X360_ARTIST.XEX.i64):
    // X360 flt_82004014 == `3D CC CC CD` == 0.1f. It is the slack the "always smash" test adds to
    // the move threshold (`lfs f0,0x50(type) ; fadds f0,f0,f31`).
    const f32 KF_ALWAYS_SMASH_THRESHOLD_SLACK = 0.1f;

    // MEASURED (same byte read): word 0 of X360 stru_8208F620 == 0x34000000 == 1.1920929e-07f
    // (FLT_EPSILON). It is the tolerance the inlined rw::math::vpu::IsZero uses for the "has this
    // prop started moving" test (`vandc128` the sign bits, `vrlimi128 v12,v0,1,1` to replicate x
    // into w so the 4-lane compare is a 3-lane one, then `vcmpgtfp.` and the CR6 "none true" bit).
    // ⚠️ It is NOT the 1.0e-6f default the committed IsZero declaration carries, so it is passed
    // explicitly -- an order of magnitude apart, and this decides when a prop is reported moved.
    //
    // ⚠️⚠️ ONE KNOWN NaN-POLARITY DELTA, STATED RATHER THAN PAPERED OVER (AGENTS.md gotcha 4).
    // The console's test is the CR6 "none true" bit of `vcmpgtfp.`, i.e. "no lane is ORDERED-
    // greater than eps", which is TRUE for a NaN lane. The committed rw::math::vpu::IsZero is
    // spelled `fabs(lane) <= tolerance`, which is FALSE for a NaN lane. So a prop whose linear
    // velocity contains a NaN would be treated as stationary by the console and as moving here.
    // IsZero is still called -- it is the DWARF-named helper and the tree's single home for it,
    // and forking a local three-lane copy to chase a NaN case would be worse. The exposure is
    // bounded: PropInstance::SetLinearVelocity's own RwMath::IsValid tripwire fires on the very
    // next statements of this same loop iteration. Fix it IN vector3_operation.h if it ever
    // matters, not here.
    const f32 KF_PROP_MOVED_EPSILON = 1.1920929e-07f;
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ClampAcceleration  @ 0x82627F00   (125 instructions, MEASURED:
// 125 non-blank lines in the export's `assembly`, 0x82627F00 `mflr r12` .. 0x826280F0
// `b __restgprlr_28`)
//
// DWARF: dwarfdump GameSource/Physics/PropManager/BrnPropManager.cpp, dumpfile line 1049 ==
// SOURCE line :1188 (the dumpfile prints `// BrnPropManager.cpp:1188` immediately above the
// signature). Body locals at source :1191 / :1193 / :1194 / :1199 / :1211 / :1212 / :1217 / :1231.
// ⚠️ Round 1 cited ":1049" as if it were a source line. It is a DUMPFILE line. The two numbering
// systems are not interchangeable and were mixed inside one clause; corrected here.
//
// One per-frame sanity clamp on a single updated rigid body: the simulation has already produced
// new linear/angular velocities for a smashed prop, and this rejects any pair whose implied
// acceleration this frame exceeds the tuning ceiling, rewriting the velocity in place and (only
// if it actually clamped) re-posting the corrected body to the input buffer.
//
// ---- REGISTER MAP (measured, from the prologue) -------------------------------------------------
//   r3  = this            (read only as the implicit member-function receiver -- nothing loaded)
//   v1  = lLinearVelocity        (the PREVIOUS frame's linear velocity)
//   v2  = lAngularVelocity       (the PREVIOUS frame's angular velocity)
//   r4  -> r29             = lRigidBodyId (the 64-bit handle; `std r29, sp+0x60` = the event's mID)
//   r5                     = lpUpdateBodyEvent (the body itself at +0x10 -- `addi r4,r5,0x10`)
//   r6  -> r31             = &lUpdatedLinearVelocity   (in/out, `lvx128`/`stvx128 v0,r0,r31`)
//   r7  -> r30             = &lUpdatedAngularVelocity  (in/out)
//   v3                     = lvfTimeStep
//   v4                     = lvfOneOverTimeStep
//   r8  -> r28             = lpSimModuleInputBuffer
// The DWARF parameter list at source :1188 matches this positionally and supplies every name used
// below. AGENTS.md gotcha 3 in action: the four vector/VecFloat parameters ride v1..v4 and consume
// NO GPR slot, which is what makes (v1,v2,r4,r5,r6,r7,v3,v4,r8) a 9-parameter list and not a 5.
//
// ---- THE TWO SYMMETRIC BLOCKS -------------------------------------------------------------------
// Linear  @0x82627F20..0x82627FD4, angular @0x82627FD8..0x82628088. Identical instruction for
// instruction apart from which velocity pair and which pair of tuning globals they read:
//   linear : threshold unk_82FB9F40 == KVF_MAX_LINEAR_ACCELERATION_SQ,
//            magnitude unk_82FB94B0 == KVF_MAX_LINEAR_ACCELERATION
//   angular: threshold unk_82FB94D0 == KVF_MAX_ANGULAR_ACCELERATION_SQ,
//            magnitude unk_82FB9490 == KVF_MAX_ANGULAR_ACCELERATION
// (the address<->name mapping is BrnPropManager.h's, from the DecFIGS global list for this .cpp.)
//
// Each block:
//     vsubfp        dv     = *lrVelocity - lLastVelocity
//     vmulfp128     accel  = dv * lvfOneOverTimeStep
//     vmsum3fp128   lenSq  = dot3(accel, accel)                 <- THREE lanes, not four
//     vcmpgtfp.  +  beq    if (lenSq > threshold) { ... }
//     ...renormalise, then
//     vmaddfp    +  stvx128   *lrVelocity = clamped * lvfTimeStep + lLastVelocity
//     li r10, 1             the shared `lbUpdateBody` latch (ONE latch, set by either block)
//
// ⚠️ vmaddfp OPERAND ORDER, ESTABLISHED EMPIRICALLY BEFORE ANY MULTIPLY-ADD WAS TRUSTED (a wrong
//    rule here corrupts correct math silently, which is why it is re-derived rather than quoted):
//    IDA prints four fields; calibrating against the rsqrt Newton sequence at 0x82627F8C..0x82627F90
//    (`vnmsubfp v7, v0, v13, v31` must be `1.0 - x*y0^2` and `vmaddfp v12, v30, v12, v7` must be
//    `0.5*y0*resid + y0`, the only reading that is a Newton step at all) fixes the rule as
//    printed(op1,op2,op3,op4) => op1 = op2*op4 + op3. Under that rule `vmaddfp v0, v0, v1, v3`
//    @0x82627FD0 is accel*lvfTimeStep + lLinearVelocity, which is what is spelled below.
//
//    ⚠️⚠️ THAT RULE HOLDS FOR THE **PLAIN** `vmaddfp` / `vnmsubfp` MNEMONICS ONLY. The VMX128 forms
//    (`vmaddfp128` / `vnmsubfp128`) print in the OPPOSITE order: op1 = op2*op3 + op4 (and the
//    nmsub form is -(op2*op3) + op4). Re-derived in round 3 from the SAME Newton idiom, where this
//    build emits one of each back to back inside ReadUpdatedBodies below:
//        0x826331E0 `vnmsubfp128 v31, v4, v2, v31`  (v4 = lenSq, v2 = y0^2, v31 = 1.0)
//                        is the residual ONLY as -(op2*op3) + op4  == 1 - lenSq*y0^2
//        0x826331E4 `vmaddfp    v11, v1, v11, v31`  (v1 = 0.5*y0, v11 = y0)
//                        is the Newton step ONLY as   op2*op4 + op3 == 0.5*y0*resid + y0
//    (it repeats identically at 0x826331F4 / 0x82633200, and the extra-COM cascade at
//     0x82632FF4/0x82632FF8 -- `vmaddfp128 v13, v125, v12, v13` -- is a basis-row accumulation only
//     under the VMX128 order; the plain order turns it into row1*(row0*off.x) + off.y, i.e. noise.)
//    ⚠️ NEITHER ClampAcceleration NOR ApplyAntiHerdingForce CONTAINS A SINGLE VMX128 MULTIPLY-ADD:
//    re-checked instruction by instruction in round 3 -- all 18 multiply-adds in 0x82627F00 and all
//    8 in 0x826113F8 are the plain mnemonics (counted: 18/0 and 8/0 plain-vs-128), so the plain rule
//    is the applicable one at every site
//    quoted in this file's two banners and NO landed expression changes. The distinction is written
//    down because a reader who carries the plain rule across into decode (G)'s VMX128 lines below
//    would "correct" correct code.
//
// ---- THE RENORMALISE (0x82627F58..0x82627FCC), decoded instruction by instruction ---------------
//   vrsqrtefp + two Newton steps  -> rsqrt(lenSq)
//   vmulfp128 v0, v0, v12         -> lenSq * rsqrt(lenSq) == |accel|         (a LENGTH, not a rsqrt)
//   vsel v0, v0, v5(=0), v10      -> v10 is `vcmpeqfp v10, 0, lenSq`: the zero-length lane is
//                                    forced to ZERO rather than left as the estimate's garbage.
//   vrefp + two Newton steps      -> 1 / |accel|
//   vmulfp128 v0, v0, v8          -> KVF_MAX_*_ACCELERATION / |accel|
//   vmulfp128 v0, v9, v0          -> accel * (MAX / |accel|)   == the clamped acceleration
//
//   ⚠️⚠️ THE ZERO-LENGTH GUARD, STATED CORRECTLY (round-1 MUST_FIX, applied). Round 1 claimed the
//   console's `vsel` guard "is exactly the guard vpu::Magnitude/Normalize document in the vendor
//   header". THAT IS FALSE FOR Magnitude AND IT IS THE ONE USED HERE:
//     * vendor/renderware/include/rw/math/vpu/vector3_operation.h:140-143 -- vpu::Magnitude is a
//       bare `return std::sqrt(MagnitudeSquared(lrVector));`. NO guard, and its banner claims none.
//     * The guard lives on Normalize (:151-160) and NormalizeReturnMagnitude (:178-191). NEITHER
//       is called here.
//   So the host spelling below IS an unguarded std::sqrt. It is safe here for one reason and one
//   reason only: the enclosing branch requires lenSq > KVF_MAX_*_ACCELERATION_SQ, and that
//   threshold is a squared magnitude (>= 0), so the zero-length lane cannot reach the divide.
//   Do NOT "restore" a guard that was never there, and do NOT assume one is protecting this line.
//   (For the record, the two spellings agree even at zero: the console's vsel forces |a| := 0 and
//   then takes vrefp(0); the host takes MAX/0. Both produce an infinity, not a quiet zero.)
//
//   ⚠️ THE `MAX / Magnitude(a)` DIVIDE SPELLING IS DWARF-ATTESTED, NOT INFERENCE (round-1 NIT,
//   applied -- round 1 labelled it INFERENCE). The dumpfile's clamp-branch scopes declare
//   `VecFloat lvfAccelerationMagnitude` at source :1199 and `lvfAngularAccelerationMagnitude` at
//   :1217, each followed by, in order, Magnitude / operator/ / operator*= / operator* / operator+.
//   That is a sqrt then a divide -- which is also why the console emits sqrt THEN a reciprocal
//   rather than one rsqrt scale. ⚠️ Note the DWARF's `operator*=`: the SOURCE mutated the
//   acceleration local (`lAcceleration *= (MAX / mag)`); this reconstruction spells a fresh
//   non-mutating Mult on a `const` local. Numerically identical, recorded so a later 1:1 pass does
//   not re-derive it as a divergence.
//
// ---- NaN POLARITY (AGENTS.md gotcha 4) ----------------------------------------------------------
// The guard is `vcmpgtfp.` + `mfocrf`/`extrwi ...,1,24` (CR6 bit 0 == "all lanes true") + `beq`,
// i.e. a VECTOR compare tested for truth -- NOT an `fcmpu`+`bge`. Operand order re-read this round:
// `vcmpgtfp. v13, v0, v13` with v0 == lenSq and v13 == the threshold, so it is lenSq > threshold,
// not the reverse. vcmpgtfp is FALSE for an unordered pair, and C++ `>` is FALSE for NaN, so the
// plain `>` below has the console's polarity with no negated-predicate rewrite. (The bge/ble trap
// applies to fcmpu branches; this function emits none.) lenSq is a broadcast, so "all lanes" is
// "the lane".
//
// ---- THE RE-POST (0x8262808C..0x826280E8) --------------------------------------------------------
//   if (lbUpdateBody):
//       sp+0x60 = the event.   `std r29, sp+0x60`         == lOutUpdateRigidBodyEvent.mID
//       `addi r3, sp+0x70` / `addi r4, r5, 0x10` / `bl rw::physics::RigidBody::operator=`
//                                                          == event.mRigidBody = lpUpdateBodyEvent->mRigidBody
//       two `vrlimi128 vD, vOld, 1, 0` splices at sp+0x90 and sp+0xA0 -- mask 1 == the **w** field
//       only, i.e. x/y/z come from the clamped velocity and the body's existing w lane is
//       PRESERVED. sp+0x90 / sp+0xA0 are (body+0x20) / (body+0x30) == mVel / mOmega, which is
//       exactly what rw::physics::RigidBody::Set{Linear,Angular}Velocity already spell (that
//       header's own banner records the same vrlimi w-preservation).
//       then GetUpdateRigidBodyQueue() -> AddEvent(event).
//   ⚠️ Those event/body offsets are CONSOLE stack offsets, quoted as evidence only -- the code
//      below reaches every field through a named member, never through arithmetic.
//
// ---- CALLEE CENSUS (measured: 4 `bl` in the export, one of them a save helper) -------------------
//   __savegprlr_28                                    -- compiler helper
//   "CgsPhysics_" (IDA-TRUNCATED symbol) @0x825BCCB8  -- InputBuffer::GetUpdateRigidBodyQueue, LANDED
//   InUpdateRigidBody::AddEvent          @0x82614928  -- committed (the one-arg bool AddEvent(const T&))
//   rw::physics::RigidBody::operator=    @0x825E3410  -- committed
//   No StrStream/assert call at all. ⚠️ The DWARF DOES carry two local-`StrStream` diagnostic
//   blocks for this function (dumpfile 1118-1129, the second ending in
//   BaseEventQueue<InUpdateRigidBody>::GetMaxLength). They have NO ARTIST counterpart -- there is
//   no StrStream `bl` anywhere in the 125 instructions -- so they are correctly absent. Negative
//   evidence recorded so the next pass does not "restore" invented diagnostics.
//
// ⭐ THE CONSOLE-VALUE WARNING THAT STOOD HERE IS RETIRED (2026-08-18 round 3b). It said "the
// ARTIST image contains no initialiser for any of these four globals", predicted that
// KVF_MAX_*_ACCELERATION_SQ == 0 would make this function clamp EVERY body every frame down to
// last frame's velocities, and asked for that to be reported rather than fixed. The prediction
// was right about the consequence and wrong about the cause: the initialisers exist, as MSVC
// dynamic-initialiser thunks outside every IDA function. All four are now seated at their
// measured console values in BrnPropManager.cpp:
//     KVF_MAX_LINEAR_ACCELERATION     = Splat(30.0f)    KVF_MAX_LINEAR_ACCELERATION_SQ  = Splat(900.0f)
//     KVF_MAX_ANGULAR_ACCELERATION    = Splat(80.0f)    KVF_MAX_ANGULAR_ACCELERATION_SQ = Splat(6400.0f)
// so the clamp is a real 30 m/s^2 / 80 rad/s^2 ceiling, and the two _SQ thresholds are exactly the
// squares of their siblings (which is how the console computes them -- see the _SQ note in
// BrnPropManager.cpp). Thunk / table-slot / rodata provenance per constant: BrnPropManager.h.
// =================================================================================================
void PropManager::ClampAcceleration( Vector3                                                    lLinearVelocity,
                                     Vector3                                                    lAngularVelocity,
                                     CgsPhysics::RigidBodyId                                    lRigidBodyId,
                                     const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody* lpUpdateBodyEvent,
                                     Vector3&                                                   lUpdatedLinearVelocity,
                                     Vector3&                                                   lUpdatedAngularVelocity,
                                     VecFloat                                                   lvfTimeStep,
                                     VecFloat                                                   lvfOneOverTimeStep,
                                     CgsPhysics::PhysicsSimulationIO::InputBuffer*              lpSimModuleInputBuffer )
{
    namespace vpu = rw::math::vpu;

    // :1191
    bool lbUpdateBody = false;

    // ---- linear block @0x82627F20 ---------------------------------------------------------------
    {
        // :1193 / :1194
        const Vector3  lAcceleration =
            vpu::Mult(vpu::Subtract(lUpdatedLinearVelocity, lLinearVelocity), lvfOneOverTimeStep.x);
        const VecFloat lvfAccelerationMagnitudeSquared = vpu::Splat(vpu::MagnitudeSquared(lAcceleration));

        if (lvfAccelerationMagnitudeSquared.x > KVF_MAX_LINEAR_ACCELERATION_SQ.x)
        {
            // :1199  accel * (MAX / |accel|) * dt + lastVelocity -- see the "renormalise" note.
            // vpu::Magnitude is an UNGUARDED std::sqrt; the branch above is what makes the
            // zero-length lane unreachable.
            const f32 lfClampScale =
                KVF_MAX_LINEAR_ACCELERATION.x / vpu::Magnitude(lAcceleration);

            lUpdatedLinearVelocity =
                vpu::Add(vpu::Mult(vpu::Mult(lAcceleration, lfClampScale), lvfTimeStep.x),
                         lLinearVelocity);
            lbUpdateBody = true;
        }
    }

    // ---- angular block @0x82627FD8 --------------------------------------------------------------
    {
        // :1211 / :1212
        const Vector3  lAngularAcceleration =
            vpu::Mult(vpu::Subtract(lUpdatedAngularVelocity, lAngularVelocity), lvfOneOverTimeStep.x);
        const VecFloat lvfAngularAccelerationMagnitudeSquared =
            vpu::Splat(vpu::MagnitudeSquared(lAngularAcceleration));

        if (lvfAngularAccelerationMagnitudeSquared.x > KVF_MAX_ANGULAR_ACCELERATION_SQ.x)
        {
            // :1217
            const f32 lfClampScale =
                KVF_MAX_ANGULAR_ACCELERATION.x / vpu::Magnitude(lAngularAcceleration);

            lUpdatedAngularVelocity =
                vpu::Add(vpu::Mult(vpu::Mult(lAngularAcceleration, lfClampScale), lvfTimeStep.x),
                         lAngularVelocity);
            lbUpdateBody = true;
        }
    }

    // ---- the corrected re-post @0x8262808C ------------------------------------------------------
    if (lbUpdateBody)
    {
        // :1231
        CgsPhysics::PhysicsSimulationIO::InUpdateRigidBody lOutUpdateRigidBodyEvent;

        lOutUpdateRigidBodyEvent.mID        = lRigidBodyId;                     // `std r29, sp+0x60`
        lOutUpdateRigidBodyEvent.mRigidBody = lpUpdateBodyEvent->mRigidBody;    // RigidBody::operator= @0x825E3410

        // The two `vrlimi128 ...,1,0` splices: xyz from the clamped velocities, w preserved.
        lOutUpdateRigidBodyEvent.mRigidBody.SetLinearVelocity(lUpdatedLinearVelocity);
        lOutUpdateRigidBodyEvent.mRigidBody.SetAngularVelocity(lUpdatedAngularVelocity);

        // @0x826280E0 `bl sub_825BCCB8` then @0x826280E8 the one-arg AddEvent. The accessor is the
        // NON-const twin landed at CgsPhysicsSimulationModuleIO.h:190 this round.
        lpSimModuleInputBuffer->GetUpdateRigidBodyQueue()->AddEvent(lOutUpdateRigidBodyEvent);
    }
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ApplyAntiHerdingForce  @ 0x826113F8   (114 instructions, MEASURED:
// 114 non-blank lines, 0x826113F8 `mflr r12` .. 0x826115BC `blr`)
//
// DWARF: dwarfdump BrnPropManager.cpp dumpfile line 2318 == SOURCE line :2111 (`// BrnPropManager
// .cpp:2111` sits immediately above the signature). Body lines :2113..:2165 ARE source lines.
// ⚠️ Round 1 cited ":2318" as a source line. Corrected, same defect as ClampAcceleration's.
//
// A prop that a race car is pushing along ("herding") gets shoved sideways and, above a speed
// threshold, upward as well -- so it stops riding the bumper. Posted as one InApplyForce.
//
// ---- PARAMETER NAMES: THE ROUND-1 HEADER DEFECT IS NOW FIXED AT SOURCE ---------------------------
// Round 1 filed BrnPropManager.h's names for this function as WRONG and the parked body carried a
// long banner describing the divergence. Round 2's header owner applied the rename, so THAT BANNER
// IS RETIRED -- keeping it would read as a live defect against a header that no longer has one.
// What remains true and worth keeping is the EVIDENCE for the names, because it is also the
// evidence for what each parameter means:
//   * lPropWorldPos       -- v1 is transformed by the car's INVERSE affine => a world POSITION.
//   * lvfPropMass         -- v2 multiplies a force => a MASS. (The old name was `lvfScale`; a
//                            future caller reading "scale" would pass a dimensionless number.)
//   * lPropLinearVelocity -- v3 is differenced against the car's linear velocity and dotted with
//                            the car's up axis; the DWARF's own local for that value is
//                            `lvfPropsUpwardVelocity` => a VELOCITY, not a normal.
//   * lCollisionNormal    -- position 7, and the genuinely UNUSED one: v4 is clobbered at
//                            0x82611448 by `vspltw v4, v11, 0` (the COM-offset splat) before any
//                            read. Declared per the DWARF anyway (the asm cannot disprove an
//                            unused parameter) and named in the definition below with the name
//                            commented out, so the "unused" fact is visible at the definition too.
//
// ---- REGISTER MAP (measured) --------------------------------------------------------------------
//   r3 = this  -- ⚠️ NEVER READ. `mr r3, r4` at 0x82611558 overwrites it before the only call.
//   r4 = lpSimInputBuffer      r5 = lpRaceCar      r6 = lPropRigidBodyId (`std r6, sp+0x70`)
//   v1 = lPropWorldPos   v2 = lvfPropMass   v3 = lPropLinearVelocity   v4 = lCollisionNormal (dead)
//
// ---- THE FIVE RaceCarPhysics READS (all have committed accessors -- no offset arithmetic) --------
//   +0x10..+0x40  ExternallySimulatedBody::GetTransform()            (the four affine rows)
//   +0x50         ExternallySimulatedBody::GetLinearVelocity()
//   +0x670        GetSimpleAttribs()->mCOMOffset
//   +0x6A0        GetHalfExtent()          -- only lane .y is read (`vspltw v26, v5, 1`)
//   +0x6C0        GetSpeedMPH()            -- a broadcast VecFloat
//   (those +0xNNN are CONSOLE offsets, quoted as the evidence that fixes WHICH accessor; the
//    code below never does offset arithmetic.)
//
// ---- DECODE, in emission order (re-read this round) ---------------------------------------------
//   0x82611470  vandc  v11, speedMPH, 0x80000000     |speedMPH|
//   0x8261148C  vminfp v11, v11, KVF_SPEED_CLAMP     lvfRaceCarSpeed
//   0x826114A8  vmaddfp cascade over the three rotation rows by mCOMOffset.x/.y/.z, then
//   0x826114B0  vsubfp v9, wAxis, that               lCarTransform.Pos() -= TransformVector(...)
//   0x826114C0  vmsum3fp128(lPropWorldPos - Pos(), xAxis)               -> Dot #1
//   0x826114E8/0x826114EC/0x826114F0/0x826114F8  vcmpgtfp + vcmpgefp + two vsel  == Sgn(Dot #1)
//               (self-consistency check: `vmr v7, v8` @0x82611490 and `vsubfp v4, v0, v8`
//                @0x82611494 make v7 == +1.0 and v4 == -1.0 only if v8 == 1.0, which is the
//                reading that makes the pair of vsels a sign function.)  -> lSidewaysDirection
//   0x826114F4..0x82611518  the vmrghw/vmrglw 4x4 transpose of (xAxis,yAxis,zAxis,0), plus the
//               splatted -Pos() cascade: this is InverseOfMatrixWithOrthonormal3x3 inlined --
//               its rows ARE the transposed columns and its wAxis IS -(Pos() . row_i).
//   0x82611534..0x8261153C  the same cascade applied to lPropWorldPos == TransformPoint(inverse, p)
//                                                                      -> lPropRelativePosition
//   0x82611548  vsubfp   lPropRelativePosition.y - GetHalfExtent().y    -> lvfPropHeightAboveCar
//   0x82611550  vcmpgtfp against 0                                      -> lAboveRaceCar
//   0x82611508/0x82611520/0x82611528  lSideForce = (xAxis * Sgn) * lvfRaceCarSpeed * lvfPropMass
//   0x82611530  v5  = lSideForce * KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE  (flt_82FB9D70)
//   0x8261157C  v11 = lSideForce * KVF_ANTI_HERD_SIDE_SCALE             (flt_82FB9450)
//   0x82611554/0x82611580  vmsum3fp128(lPropLinearVelocity - GetLinearVelocity(), yAxis)  -> Dot #2
//   0x8261156C  lvfTargetUpwardVel = lvfRaceCarSpeed * KVF_ANTI_HERD_UPWARD_SCALE (flt_82FB93E0)
//   0x82611584/0x82611588/0x8261158C  lForceMagnitude = Max(target - upwardVel, 0) * lvfPropMass
//   0x82611590  vmaddfp v0, v12, v5, v0  == yAxis * lForceMagnitude + v5   (the PLAIN-mnemonic
//                                           operand-order rule, op1 = op2*op4 + op3, re-derived in
//                                           ClampAcceleration's banner. This function emits EIGHT
//                                           multiply-adds and ZERO `vmaddfp128`/`vnmsubfp128`
//                                           (counted), so the VMX128 exception noted there does not
//                                           reach any line here. Under the VMX128 order this
//                                           instruction would read yAxis*(lSideForce*scale) +
//                                           lForceMagnitude, which is not a force at all.)
//   0x82611594  vsel v0, v11, v0, v24  -- v24 == `vcmpgtfp v24, v11, flt_82FB93A0` ==
//               (lvfRaceCarSpeed > KVF_MAX_SPEED_FOR_SIDE_FORCE): TRUE -> the sum, FALSE -> v11
//   0x82611598  vsel v0, v0, v25, v9   -- v9 == lAboveRaceCar: TRUE -> ZERO (v25, `vmr v25, v0`
//               @0x82611480 while v0 was still the zero register)
//   0x826115A0/0x826115A8  GetApplyForceQueue() -> AddEventSafe(event)
//   (vsel vD,vA,vB,vC == per-lane `mask ? vB : vA`, which is why each TRUE arm above is the THIRD
//    printed operand, not the second.)
//
// ⭐ CORROBORATION, not just this reading: the DecFIGS callee list for this function (dumpfile
// references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp:2366-2395, quoted
// COMPLETE and in order -- round-2 NIT, applied; the earlier transcription omitted two entries
// while calling itself exact) names:
//   TransformVector / Sgn / TransformPoint / ExternallySimulatedBody::GetTransform / operator- /
//   InverseOfMatrixWithOrthonormal3x3 / Min<VecFloat> / CompGreaterThan / Dot / Abs<VecFloat> /
//   operator-<VectorAxisY, VectorAxisY> / GetApplyForceQueue / operator* / operator+ / operator* /
//   Select / operator-= / operator- / Dot / operator- / Max<VecFloat> / six operator* / Select /
//   AddEventSafe
// -- item for item the decode above, including BOTH Dots, BOTH Selects, the `Pos() -=`, and the
// `lPropRelativePosition.y - GetHalfExtent().y` axis subtract (that is the VectorAxisY operator-).
//
// ---- WHY THE DWARF'S MASK VOCABULARY IS NOT SPELLED OUT, HELPER BY HELPER (round-1 NIT, applied) -
// The DWARF names `MaskScalar lAboveRaceCar` (source :2122) plus CompGreaterThan / Select /
// Min<VecFloat> / Max<VecFloat> / Abs<VecFloat> / Sgn. The reconstruction below reduces the mask
// machinery to `bool` + ternaries and uses std::fabs. That reduction is EXACT (both vsel masks come
// from compares of BROADCAST operands -- a splatted speed against a splatted gain, and a splatted
// lane difference against zero -- so all four lanes always agree and a scalar bool is a
// reproduction, not a narrowing). The per-helper state of the tree, so this reads as six separate
// judgements rather than one blanket one:
//   * MaskScalar          EXISTS -- vendor rw/math/vpu/types.h:115.
//   * Select              EXISTS -- vector4_operation.h:88, but its signature is
//                         `Vector4 Select(Vector4, Vector4, MaskScalar)`. The value being selected
//                         here is a **Vector3** (lFinalForce) and Vector3/Vector4 are DISTINCT
//                         structs in this tree (types.h:24/:26). Spelling it would need a lane-type
//                         cast at every use -- strictly worse than the exact scalar reduction, so
//                         the ternary stays.
//   * CompGreaterThan     does NOT exist under that name; the tree's home for it is
//                         `IsGreater` (vector4_operation.h:98), again Vector4-typed.
//   * Min / Max<VecFloat> EXIST and ARE used below (vector4_operation.h:45/:53), already written in
//                         the console's select form: vminfp(a,b) == (a<b)?a:b, vmaxfp likewise.
//   * Abs<VecFloat>       does NOT exist -- vector3_operation.h:237's Abs is Vector3-only. std::fabs
//                         on the broadcast lane is the exact scalar equivalent of the `vandc`.
//   * Sgn                 does NOT exist anywhere under vendor rw/math/. Inlined below as the
//                         two-compare/two-select nested conditional the console emits.
//
// ---- NaN POLARITY (AGENTS.md gotcha 4) ----------------------------------------------------------
// Every decision here is a VECTOR compare feeding a `vsel`/`vminfp`/`vmaxfp`, not an `fcmpu`
// branch, so the C++ below deliberately spells each one in its select form with the same operand
// order the console used. vcmpgtfp/vcmpgefp are FALSE when unordered, exactly like C++ > and >=.
// In particular Sgn(NaN) is -1.0f on this hardware (both compares false), and that is what the
// nested conditional below produces. Do NOT "simplify" it to copysign or to fpu::Clamp.
//
// ---- CALLEE CENSUS (measured: exactly 2 `bl` in the export, no save helper) ----------------------
//   sub_825BCD60                          @0x825BCD60  -- InputBuffer::GetApplyForceQueue, LANDED
//   InApplyForce::AddEventSafe            @0x825E3E20  -- committed
//   ⚠️ AddEventSafe, NOT AddEvent -- the bounds-gated variant. The console drops the shove rather
//   than overflowing a full queue; the DWARF callee list names AddEventSafe too.
//
// ⭐ THE CONSOLE-VALUE WARNING THAT STOOD HERE IS RETIRED (2026-08-18 round 3b). "No initialiser
// exists in the ARTIST image" was measured false -- the initialisers are dynamic-initialiser
// thunks that live outside every IDA function, so no export scan could see them. All five gains
// are now seated at their measured console values in BrnPropManager.cpp:
//     KVF_SPEED_CLAMP                     = Splat(120.0f)
//     KVF_MAX_SPEED_FOR_SIDE_FORCE        = Splat(60.0f)
//     KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE = Splat(1.5f)
//     KVF_ANTI_HERD_UPWARD_SCALE          = Splat(2.0f)
//     KVF_ANTI_HERD_SIDE_SCALE            = Splat(0.05f)
// The old note's own reading of the shape now pays off: with a 120 m/s speed clamp and a 60 m/s
// knee, the side scale steps from 0.05 to 1.5 -- a 30x -- once the car is over the knee, which is
// what makes the anti-herding shove a high-speed behaviour. The five ARE the exact set the debug
// UI groups under "Anti herding...", which remains the cross-check that the NAMING is right; the
// VALUES are no longer unrecovered. Provenance per constant: BrnPropManager.h.
// =================================================================================================
void PropManager::ApplyAntiHerdingForce( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInputBuffer,
                                         BrnPhysics::Vehicle::RaceCarPhysics*          lpRaceCar,
                                         CgsPhysics::RigidBodyId                       lPropRigidBodyId,
                                         Vector3                                       lPropWorldPos,
                                         VecFloat                                      lvfPropMass,
                                         Vector3                                       lPropLinearVelocity,
                                         Vector3                                       /*lCollisionNormal*/ )
{
    namespace vpu = rw::math::vpu;

    // NOTE: `this` is deliberately unread -- the X360 body overwrites r3 with r4 (the input
    // buffer) at 0x82611558 before its only call and never touches a member. Measured, not
    // an omission.

    // :2120  |speedMPH| clamped -- `vandc` (sign-bit clear) then `vminfp`.
    const VecFloat lvfRaceCarSpeed =
        vpu::Min(vpu::Splat(std::fabs(lpRaceCar->GetSpeedMPH().x)), KVF_SPEED_CLAMP);

    // :2116  the car basis, with the centre-of-mass offset taken back out of the position
    // (`Pos() -= TransformVector(transform, mCOMOffset)`).
    // ⚠️ WHY THE HOMED HELPER *IS* CALLED HERE, WHERE ReadUpdatedBodies' decode (G) declines it:
    // the committed vpu::TransformVector (matrix44affine_operation.h:41-53) forces `lvResult.w = 0`,
    // whereas the console's cascade at 0x826114A8..0x826114B0 is full 4-lane and carries the basis
    // rows' packed w scalars through. At THIS site the w lane is provably DEAD: the mutated Pos() is
    // consumed only by the 3-lane `vmsum3fp128 v8,v8,v13` @0x826114C0 (Dot #1), by the `vspltw` of
    // lanes 0/1/2 @0x826114C4..0x826114CC, and by InverseOfMatrixWithOrthonormal3x3, which reads
    // lrPos.x/.y/.z only (same header, :199-215) and writes its own wAxis.w = 0. So the two policies
    // in this file are deliberate, not a contradiction -- decode (G)'s site keeps a LIVE w lane.
    Matrix44Affine lCarTransform = lpRaceCar->GetTransform();
    lCarTransform.Pos() = vpu::Subtract(
        lCarTransform.Pos(),
        vpu::TransformVector(lCarTransform, lpRaceCar->GetSimpleAttribs()->mCOMOffset));

    // :2123  vpu::Sgn of the prop's sideways offset along the car's right axis.
    // Emitted as its own `vmsum3fp128` at 0x826114C0, ahead of (and separate from) the full
    // inverse transform below -- reproduced as its own Dot for that reason. Numerically it is
    // the same value as lPropRelativePosition.x.
    const f32 lfSidewaysOffset =
        vpu::Dot(vpu::Subtract(lPropWorldPos, lCarTransform.Pos()), lCarTransform.Right());
    const f32 lSidewaysDirection =
        (lfSidewaysOffset >= 0.0f) ? ((lfSidewaysOffset > 0.0f) ? 1.0f : 0.0f) : -1.0f;

    // :2165 / :2124  the prop's position in the car's frame.
    const Matrix44Affine lInverseCarTransform =
        vpu::InverseOfMatrixWithOrthonormal3x3(lCarTransform);
    const Vector3 lPropRelativePosition = vpu::TransformPoint(lInverseCarTransform, lPropWorldPos);

    // :2121 / :2122  is the prop riding ABOVE the car's roof line? (only lane .y is read)
    // The DWARF's type for this is MaskScalar; the scalar bool is exact -- see the banner's
    // helper-by-helper note.
    const VecFloat lvfPropHeightAboveCar =
        vpu::Splat(lPropRelativePosition.y - lpRaceCar->GetHalfExtent().y);
    const bool lAboveRaceCar = (lvfPropHeightAboveCar.x > 0.0f);

    // :2114 / :2125  the sideways shove, before either scale.
    const Vector3 lForceDirection = vpu::Mult(lCarTransform.Right(), lSidewaysDirection);
    const Vector3 lSideForce      =
        vpu::Mult(vpu::Mult(lForceDirection, lvfRaceCarSpeed.x), lvfPropMass.x);

    // :2117..:2119 / :2115  the upward component: only ever pushes the prop up towards the target
    // relative velocity, never down (`vmaxfp` against zero).
    const Vector3  lCarsWorldLinearVelocity = lpRaceCar->GetLinearVelocity();
    const VecFloat lvfPropsUpwardVelocity   = vpu::Splat(
        vpu::Dot(vpu::Subtract(lPropLinearVelocity, lCarsWorldLinearVelocity), lCarTransform.Up()));
    const VecFloat lvfTargetUpwardVel =
        vpu::Splat(lvfRaceCarSpeed.x * KVF_ANTI_HERD_UPWARD_SCALE.x);
    const VecFloat lForceMagnitude = vpu::Splat(
        vpu::Max(vpu::Splat(lvfTargetUpwardVel.x - lvfPropsUpwardVelocity.x), vpu::Splat(0.0f)).x
        * lvfPropMass.x);

    // :2126  the two `vsel`s, in the console's order.
    Vector3 lFinalForce =
        (lvfRaceCarSpeed.x > KVF_MAX_SPEED_FOR_SIDE_FORCE.x)
            ? vpu::Add(vpu::Mult(lCarTransform.Up(), lForceMagnitude.x),
                       vpu::Mult(lSideForce, KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE.x))
            : vpu::Mult(lSideForce, KVF_ANTI_HERD_SIDE_SCALE.x);

    if (lAboveRaceCar)
    {
        lFinalForce = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    }

    // :2113  the event: the 8-byte handle at +0x00, the force at +0x10 (`std r6, sp+0x70`,
    // `stvx128 v0, sp+0x80`). AddEventSafe, not AddEvent.
    CgsPhysics::PhysicsSimulationIO::InApplyForce lApplyForceEvent;
    lApplyForceEvent.mID    = lPropRigidBodyId;
    lApplyForceEvent.mForce = lFinalForce;

    // @0x826115A0 `bl sub_825BCD60` -> the NON-const accessor landed at
    // CgsPhysicsSimulationModuleIO.h:201 this round; @0x826115A8 the bounds-gated post.
    lpSimInputBuffer->GetApplyForceQueue()->AddEventSafe(lApplyForceEvent);
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ReadUpdatedBodies  @ 0x82632918   (752 instructions, MEASURED:
// 752 non-blank lines, 0x82632918 `mflr r12` .. 0x826334D4 `b __restgprlr`)
//
// DWARF: dwarfdump BrnPropManager.h dumpfile line 217 (class declaration) and dwarfdump
// BrnPropManager.cpp dumpfile line 1133 == SOURCE line :962.
// ⚠️ Round 1's banner cited "BrnPropManager.h:154 / .cpp:1133" as source lines. Both are DUMPFILE
// lines; the real source line for the definition is :962. Same defect class as the two functions
// above, corrected here. The per-statement citations below (:964, :969, :970, :974, :1008..:1011,
// :1027, :1035, :1041..:1047, :1078..:1109) ARE source lines -- the dumpfile prints them as
// `// BrnPropManager.cpp:NNN` comments above each local.
//
// ⚠️ PARAMETER NAMES: the definition below uses the DWARF's own spellings (source :962 --
// lpUpdatedBodies / lpSceneInput / lpSimModuleInputBuffer / lvfTimeStep). The committed
// ReadUpdatedBodies declaration in BrnPropManager.h still
// spells them lpUpdatedBodyQueue / lpSceneInterface / lpSimInputBuffer -- the 2026-08-09 conductor
// wave's names, which predate the DWARF read. C++ lets a definition rename parameters, so this
// compiles either way; the header rename is filed as a name-only request, not applied here (this
// lander may not edit headers). The DWARF additionally spells the fourth parameter
// `const VecFloat` -- a top-level const on a by-value parameter, which is not part of the
// signature and is dropped, as everywhere else in this tree.
//
// -------------------------------------------------------------------------------------------------
// WHAT THIS FUNCTION IS
// -------------------------------------------------------------------------------------------------
// The per-frame prop leg of PhysicsModule::Update @0x825B0640 (its only caller, from xrefs_to). It
// drains the simulation's OutUpdateRigidBody queue, keeps the events whose RigidBodyId owner byte
// is E_ENTITYTYPE_PROP (3), and for each one writes the resolved pose/velocity back into the
// PropManager's own PropInstance / PropPartInstance table and re-publishes it as an
// UpdatePropEvent on mUpdatedProps. Along the way it (a) fires the extra prop gravity, (b) drops
// props that froze or fell out of the world, (c) clamps per-frame acceleration, and (d) pins the
// "always smash" props (smash gates, fences, billboards) back to their authored transform.
//
// TRANSCRIPTION BASIS
//   * X360 ARTIST 0x82632918, read instruction by instruction (.ida-exports JSON `assembly`).
//   * DecFIGS dwarfdump BrnPropManager.cpp source :962.. -- it names every local (liIndex /
//     lpUpdateBodyEvent / lRigidBodyId / lUpdatePropEvent / lbFrozen / lbIsPart /
//     lvfOneOverTimeStep / lUpdatedTransform / lUpdatedPosition / lUpdatedLinearVelocity /
//     lUpdatedAngularVelocity / lEntityId / liPartIndex / lpPart / liPropIndex / lpProp / lpEvent)
//     AND the inlined callee set, which is what identifies the two big VMX blocks below.
//   * Two rodata floats byte-read out of IDA Files/BURNOUT_X360_ARTIST.XEX.i64 with headless
//     IDA 9.3 (flt_82004014 and word 0 of stru_8208F620); see the two file-scope constants.
//
// -------------------------------------------------------------------------------------------------
// L. DWARF CONSTRUCTS WITH NO ARTIST COUNTERPART -- CORRECTLY ABSENT, WITH THE NEGATIVE EVIDENCE
//    (round-1 G6 NIT 3, applied. Written down so the next verifier does not re-derive it for an
//     hour, or worse, "restore" invented code.)
//    The DecFIGS scope for this function contains things this build did not emit:
//      * a top-level `InRemoveRigidBody lRemoveBodyEvent;` at source :970.
//      * SIX local-`StrStream` diagnostic blocks (dumpfile 1331-1338, 1350-1368, 1377-1380,
//        1381-1388, 1389-1392, 1393-1396), two of which end in a queue-full warning built from
//        BaseEventQueue<UpdatePropEvent>::GetMaxLength and BaseEventQueue<InApplyForce>::GetMaxLength.
//    NEGATIVE EVIDENCE, measured this round:
//      * the `bl` census of the 752 instructions is 26 unique NON-HELPER targets (29 unique targets
//        minus __savegprlr_14 / __savevmx_119 / __restvmx_119; 60 `bl` LINES in total -- all three
//        numbers re-counted in round 3, the earlier "29 lines" was the unique-target count
//        mislabelled). NONE of them is
//        InputBuffer::GetRemoveRigidBodyQueue, an InRemoveRigidBody AddEvent, or
//        CgsDev::StrStream::StrStream.
//      * a grep of the whole disassembly for `0x684` returns ZERO hits -- and mUpdatedProps+0x684
//        is where a GetMaxLength read would land. (`0x680`/`0x688` DO appear: the Append source
//        and the Clear store.)
//    The one logging path that IS emitted is the fell-out-of-the-world line, and it goes through
//    the gpDebugPrint GLOBAL, not a local StrStream -- which is exactly what the DWARF's four
//    BARE `StrStreamBase::operator<<` calls (dumpfile 1342-1345, outside every StrStream block)
//    describe: four inserts, one of them the u64 through sub_82203EE8.
//
// -------------------------------------------------------------------------------------------------
// THE DECODES A BODY AUTHOR WILL OTHERWISE GET WRONG (all MEASURED unless marked)
// -------------------------------------------------------------------------------------------------
//
// A. `lbFrozen` IS THE RIGID BODY'S OWN STATE BIT, not an event flag word.
//    The asm is `lwz r11, 0x9C(event)` + `extrwi r10,r11,1,30` == (word >> 1) & 1. event+0x9C is
//    event+0x10 (mRigidBody) + 0x8C, and rigidbody.h's committed layout puts mIsplt at +0x80 with
//    the console packing mState in its w lane at +0x8C. So the word IS rw::physics::BodyState and
//    bit 1 is FROZEN_BODY (== 2). Reading it as "some flags field at +0x9C" would compile, run,
//    and freeze the wrong props. (The wave brief's "(bodyFlags >> 1) & 1" is this, named.)
//
// B. THE ~200-INSTRUCTION VMX BLOCK IN THE ALWAYS-SMASH ARM IS `RigidBody::SetTransform`
//    FOLLOWED BY `RigidBody::InertiaUpdate`, BOTH ALREADY BODIED IN THE TREE.
//    Its shape gives it away and the DWARF confirms it: four w-preserving `vrlimi128 vD,vOld,1,0`
//    row copies into +0x40/+0x50/+0x60/+0x10 from the PropInstance's own four transform rows, then
//    the branchless matrix->quaternion network (`vcmpgtfp` trace comparisons, the ±0.5f from
//    `vcsxwfp128 v121,1`, `vrsqrtefp` + two Newton steps, the `vsel` cascade) stored FULL-WIDTH to
//    +0x00 == mQuat -- i.e. rw::math::vpu::QuaternionFromMatrix33, whose epsilon here is
//    `lvlx` of flt_8208F60C == 0.0f (byte-read; identical to the committed inline's default, so no
//    explicit epsilon is passed). Then `lwz r8, 0x5C(body)` -- mUp's w lane == mInertia -- guards
//    the `vpermwi128 0x97 / 0x9B` + three `vmulfp128` + four `vmaddfp` tensor rebuild into
//    +0x70/+0x80 plus the `lvlx v13, r10, 0x10` read of mInertia->mInvMass. That is exactly and
//    only RigidBody::InertiaUpdate, with the caller-side `if (mInertia != NULL)` guard the
//    committed rigidbody.h banner says every call site carries.
//    ⚠️ Hand-transcribing that block instead of calling the two methods is how a tensor gets
//    written twice and transposed once, which is the risk rigidbody.h factored InertiaUpdate out
//    to avoid. It is called, not copied.
//
// C. THE ALWAYS-SMASH ARM `continue`s -- no UpdatePropEvent, no PropInstance write-back.
//    `b loc_82633490` from both its exits (the mInertia==NULL early-out at 0x8263327C and the tail
//    at 0x82633340) jumps past the AddEvent. The prop is snapped back to its authored pose with
//    zero velocity and zero accumulated force/torque and is NOT reported as moved. That is the
//    right behaviour for a smash gate: it breaks, it does not tumble.
//
// D. THE OUT-OF-WORLD TEST IS AN ORDERED COMPARE AND STAYS ONE (AGENTS.md gotcha 4).
//    `vcmpgtfp. v13(K splat), v0(pos.y splat)` + the CR6 "all true" bit. vcmpgtfp is false for
//    unordered, and so is C++ `<`, so `lUpdatedPosition.y < KVF_PROP_OUT_OF_WORLD_HEIGHT.x` is the
//    faithful spelling (the DWARF spells it `operator< <VectorAxisY>`). No negated-predicate
//    rewrite is needed here -- unlike the `bge`/`ble` forms that need `!(a < b)`.
//
// E. THE TWO SLOT-BOUNDS ASSERTS ARE A PAIR, AND THE FIRST OF EACH PAIR CAN NEVER FIRE.
//    liPartIndex / liPropIndex are `clrlwi r27,r11,16` == the LOW 16 BITS of the RigidBodyId,
//    zero-extended -- i.e. RigidBodyId::GetIndex(). A u16 is never -1, so
//    "liPartIndex != KI_PROP_INDEX_NOT_FOUND" (:1042 / :1079) is dead in the shipped build. The
//    console emits it anyway (and falls through into the range assert when it trips, which is why
//    the two share one emission), so both are reproduced. Not tidied away.
//
// F. THE EVENT CARRIES THE **RAW** VELOCITIES; THE INSTANCE GETS THE **CLAMPED** ONES.
//    lUpdatePropEvent.mLinearVelocity / .mAngularVelocity are stored at 0x82632DC4/0x82632DD4,
//    BEFORE ClampAcceleration runs; the instance setters afterwards read the sp+0x100/sp+0x110
//    slots ClampAcceleration writes through its two Vector3& out-params. On the remove path
//    ClampAcceleration is not called and those slots still hold the raw values, so both consumers
//    agree there. Hoisting the event stores after the clamp would change what the world/sound
//    bridge sees.
//
// G. THE EXTRA-COM UNDO IS A BASIS ROTATION, AND IT REWRITES BOTH COPIES OF THE POSITION.
//    `pos - (xAxis*off.x + yAxis*off.y + zAxis*off.z)` -- the same rotation AddPropToSim
//    @0x82627714 applies in the forward direction under the same mu8Flags bit. The result is
//    stored to sp+0x1E0 (the Matrix44Affine handed to PropInstance::SetTransform) AND to sp+0x150
//    (the event's own mTransform.wAxis), which is one `lUpdatePropEvent.mTransform =
//    lUpdatedTransform` re-assignment -- the compiler's redundant re-store of the three unchanged
//    basis rows at 0x82632FDC..0x82632FEC is that assignment, not three separate writes.
//
//    ⚠️⚠️ WHY THE HOMED HELPER IS DELIBERATELY *NOT* CALLED HERE (round-1 G6 NIT 2, applied).
//    The DWARF names `rw::math::vpu::TransformVector` then `rw::math::vpu::operator-=` for exactly
//    this expression (dumpfile lines 1339/1340), and TransformVector IS homed, at
//    vendor/renderware/include/rw/math/vpu/matrix44affine_operation.h:41. It is still written
//    longhand below, ON PURPOSE, because the longhand is MORE faithful:
//      * the committed TransformVector forces `lvResult.w = 0.0f`;
//      * the console's chain at 0x82632FD8-0x82632FFC (`vmulfp128 v13,v126,v13` / two `vmaddfp128`
//        / `vsubfp128 v0,v122,v13`) is FULL 4-LANE and carries the basis rows' packed w scalars
//        (mStasis / mInertia / mTag) through into the result;
//        ⚠️ THOSE TWO ARE **VMX128** MULTIPLY-ADDS (`vmaddfp128 v13, v125, v12, v13` @0x82632FF4 and
//        `vmaddfp128 v13, v124, v0, v13` @0x82632FF8) and therefore read op1 = op2*op3 + op4 --
//        the OPPOSITE field order from the plain `vmaddfp` rule calibrated in ClampAcceleration's
//        banner above. Carrying the plain rule across gives row1*(row0*off.x) + off.y instead of
//        row0*off.x + row1*off.y, i.e. it would "correct" this correct decode into nonsense. The
//        same 128-vs-plain split is visible one screen down, in the 1/|v| Newton pair at
//        0x826331E0/0x826331E4, which is where the VMX128 order was re-derived.
//      * the tree's Vector3 operator* / operator- are full 4-lane too, so the longhand reproduces
//        the console lane for lane.
//    This is the OPPOSITE call to decode (B)'s "call the homed helper, do not copy it", and the
//    difference is exactly that InertiaUpdate is lane-identical while TransformVector is not.
//    A later sweep that reads the DWARF and "fixes" this to TransformVector will silently zero a
//    live lane. Do not.
//
// H. THE FOUR `BrnPropEntityID.h:278` OWNER TRIPWIRES ARE INLINED ACCESSORS, NOT HAND-WRITTEN
//    ASSERTS. In order: the explicit `PropEntityID(u32)` constructor (0x82632C34), the inlined
//    `GetPartIndex()` (0x82632DE0), and the two `GetEntityId().GetValue()` reads on the part /
//    prop slot (0x82632E8C / 0x82633358). The committed BrnPropEntityID.cpp already carries
//    AssertIsProp inside all three accessors, so calling them by name reproduces all four --
//    writing them out by hand would double them.
//
// I. `mUpdatedJointedProps` IS MERGED IN AND CLEARED AT THE **END**, `mUpdatedProps` IS CLEARED AT
//    THE **START**. `stw r27,0x688(this)` before the loop == mUpdatedProps.Clear(); the tail is
//    `Append(this+0x680, this+0x5E10)` then `stw 0, 0x5E18(this)` ==
//    mUpdatedProps.Append(mUpdatedJointedProps) + mUpdatedJointedProps.Clear(). The DWARF lists
//    Clear twice for exactly this reason.
//
// J. 1/dt IS `vrefp128` + TWO NEWTON REFINEMENT STEPS, x 1.0f. The DWARF spells the source
//    `VecFloat lvfOneOverTimeStep = GetVecFloat_One() / lvfTimeStep` (`rw::math::vpu::operator/`,
//    source :974). VecFloat is a broadcast lane quad in this tree with no operator/, so it is
//    written as an exact reciprocal splatted over the four lanes -- the same de-optimisation
//    vector3_operation.h's own banner documents for the SDK's rsqrt estimates.
//
// K. THE ZERO-PAGE CONSTANTS ARE NO LONGER ZERO (2026-08-18 round 3b) -- this item used to say
//    they were, and that the gap should be reported rather than fixed. It has now been fixed,
//    because the "no initialiser in the image" premise it rested on was measured false: the
//    initialisers are MSVC dynamic-initialiser thunks sitting outside every IDA function, which
//    is why every export-based scan reported readers only. Seated in BrnPropManager.cpp:
//        K_DEFAULT_GRAVITY            = (0, -9.8, 0)   [per-lane decode, not a splat]
//        KVF_GRAVITY_SCALE            = Splat(3.0f)
//        KVF_PROP_OUT_OF_WORLD_HEIGHT = Splat(-1000.0f)
//    So the InApplyForce this function posts is `K_DEFAULT_GRAVITY * (3 - 1)` == 19.6 m/s^2
//    downward on top of the simulation's own 1g -- a 3g prop fall, which is what the (scale - 1)
//    shape was always for -- and the out-of-world floor is a kilometre down instead of at Y == 0.
//    ⚠️ Worth keeping in mind for anyone reading old notes: with the placeholder zeroes the posted
//    force was not merely absent, it was NEGATIVE (0 - 1 == -1), i.e. props were being pushed up.
//    KVF_PROP_OUT_OF_WORLD_HEIGHT still carries its AUTHORED-NAME flag -- 0x82FB94C0's source name
//    is unrecovered, and that is a separate fact from its now-recovered value.
//
// -------------------------------------------------------------------------------------------------
// CALLEES -- 29 unique `bl` targets (26 of them non-helper), 60 `bl` lines; every one resolved
// -------------------------------------------------------------------------------------------------
//   0x825BB538 "CgsPh" (IDA-TRUNCATED)  = BaseEventQueue<OutUpdateRigidBody>::GetEvent (stride 192,
//                                   asserts at CgsBaseEventQueue.h:272/274/275) -- committed
//   0x822868E0 "BrnPhysics::Props::Prop" (IDA-truncated) = ResourcePtr<T>::operator-> with the
//                                   "Can not instance resource pointer - it has no main memory
//                                   resource\n" assert at CgsResourcePtr.h:544 -- committed
//                                   (round-2 fix: the earlier "- it is NULL" wording was wrong; the
//                                   committed spelling is CgsResourcePtr.h:201/:209 and the console
//                                   string is aCanNotInstance @0x820072A0)
//   0x825BCCB8 "CgsPhysics_" (truncated) = InputBuffer::GetUpdateRigidBodyQueue() NON-CONST -- LANDED
//   0x825BCD60 sub_825BCD60       = InputBuffer::GetApplyForceQueue() NON-CONST        -- LANDED
//   0x825E3C30 sub_825E3C30       = BaseEventQueue<InUpdateRigidBody>::AllocateEventSafe -- LANDED
//   0x82203EE8 sub_82203EE8       = StrStreamBase::operator<<(u64)  (it reads mePrintMode at +4,
//                                   hex-formats for modes 1..2 and resets HEXONCE to DECIMAL) --
//                                   committed; the `stw 2, gpDebugPrint+4` before it IS
//                                   `<< E_PRINTMODE_HEXONCE`
//   0x825E3CC8                    = BaseEventQueue<InApplyForce>::AddEvent -- committed
//   0x825E5DF8 / 0x825E61F0       = BaseEventQueue<UpdatePropEvent>::AddEvent / ::Append -- committed
//   0x825E3410                    = rw::physics::RigidBody::operator= -- committed
//   0x8260F540 / 0x8260F988       = PropManager::RemoveProp / RemovePart -- ✅ BODIED, in the
//                                   sibling round-2 part-file PropManager_wQ2_04.cpp (by NAME --
//                                   `PropManager::RemoveProp` / `PropManager::RemovePart`).
//                                   Mount that file with this one; do NOT stub them (a stub beside
//                                   the real body is an invisible LNK2005). Round-2 correction: the
//                                   earlier banner called this pair "the link hole this file
//                                   reports", which was false the day it was written.
//   0x82627F00                    = PropManager::ClampAcceleration -- bodied ABOVE, in this file
//   0x825DE798 / 0x825DE860       = PropPartInstance::SetPosition / SetLinearVelocity -- committed
//   0x825DE370 / 0x825DE6C8 / 0x825DE5F8 = PropInstance::SetTransform / SetLinearVelocity /
//                                   SetAngularVelocity -- committed
//   0x82277C50                    = PropPhysicsDataHeader::GetType -- committed
//   0x821F1F20 / 0x828226D8 / 0x8282BE40 = DebugInterface::DebugInterface / ::GetRender /
//                                   DebugRender::DrawAxis -- the debug arm. ⛔ DrawAxis is
//                                   DECLARATION-ONLY (CgsDebugRender.h:104) and ::GetRender resolves
//                                   only to the asserting stub at WorldLinkStubs.cpp:1605: THAT is
//                                   this file's link hole (pre-existing, see the banner).
//   0x82BBC4F0                    = rw::core::debug::detail::DebugCriticalSection::Leave -- its own
//                                   real symbol (the export's xrefs_from names it), NOT an alias for
//                                   ThreadSafeRelease. Round-2 correction: the earlier census
//                                   attributed this address to "the inlined
//                                   DebugManager::ThreadSafeRelease" and then listed Leave a second
//                                   time, counting one callee twice under two identities.
//                                   ThreadSafeRelease (CgsDebugManager.h:353) IS genuinely inlined
//                                   at that site -- it is Leave followed by the mpInstance assert --
//                                   and therefore has NO address of its own here.
//   plus CgsDev::Assert::{Begin,Fire,End}Assert and the three __save*/__rest* compiler helpers.
//
// ⚠️ TWO DWARF-NAMED HELPERS ARE SPELLED OUT OF COMMITTED ACCESSORS RATHER THAN GROWN:
//    * `RigidBody::SetLinearAcceleration` / `SetAngularAcceleration` (neither declared in the
//      committed rw/physics/rigidbody.h). The console emits the two as a bare
//      `vrlimi128 vD,vOld,1,0` + `stvx128` zeroing of mTorque (+0xA0) then mForce (+0x90), which is
//      byte-for-byte what the committed `ResetForces(Vector3(0))` does (mForce := arg,
//      mTorque := 0). ResetForces is used, with this note.
//    * `PropTypeData::ShouldAlwaysSmash()` -- spelled out of the committed IsSmashable() /
//      GetSmashThreshold() / GetMoveThreshold() accessors.
//    Both are recorded as optional, non-blocking header requests; neither is invented here.
// =================================================================================================
void PropManager::ReadUpdatedBodies(
    const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodies,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
    VecFloat lvfTimeStep )
{
    // `stw r27,0x688(r29)` before the loop -- mUpdatedProps.miLength = 0. FIRST, not last.
    mUpdatedProps.Clear();

    // [DIAG] NOT IN THE X360 BINARY. Wave-Q6 read-back census (opt in with BRN_PROP_DIAG),
    // rate-limited to ONE line per simulated second. scout.md §5: until wave Q6 this loop ran with
    // a non-empty queue every physics frame and threw the whole result away, because its only
    // consumer -- OutputUpdatedProps -- was an inert gate. These three counts are the first proof
    // the read-back is alive at all, and their SHAPE is the diagnosis:
    //   parts=0 with props>0  -> no part body ever reached the sim (scout.md §4.1);
    //   frozen climbing        -> bodies are being retired, i.e. cluster B's missing prop-vs-world
    //                             contacts are letting them free-fall past the out-of-world floor.
    // The clock is the SIMULATED one (lvfTimeStep), not a host clock: it stays meaningful under a
    // paused or stepped sim, and it costs no syscall.
    static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
    s32               liDiagProps  = 0;
    s32               liDiagParts  = 0;
    s32               liDiagFrozen = 0;

    // :974  `vrefp128 v0,dt` + two Newton refinement steps, x 1.0f (v127 == vcsxwfp128 of
    // vspltisw 1). DWARF `VecFloat lvfOneOverTimeStep`, produced by rw::math::vpu::operator/ over
    // GetVecFloat_One(). De-optimised to an exact reciprocal, broadcast over the four lanes --
    // VecFloat is a broadcast lane quad in this tree.
    const f32 lfOneOverTimeStep = 1.0f / lvfTimeStep.x;
    const VecFloat lvfOneOverTimeStep = { lfOneOverTimeStep, lfOneOverTimeStep,
                                          lfOneOverTimeStep, lfOneOverTimeStep };

    // :964
    for ( s32 liIndex = 0; liIndex < lpUpdatedBodies->GetLength(); ++liIndex )
    {
        // :965
        const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody* lpUpdateBodyEvent =
            &lpUpdatedBodies->GetEvent( liIndex );

        // :966
        const CgsPhysics::RigidBodyId lRigidBodyId = lpUpdateBodyEvent->mID;

        // `srdi r11,r25,32 ; srwi r11,r24,24 ; cmplwi r11,3 ; bne <next iteration>`.
        if ( lRigidBodyId.GetEntityIDOwner() != BrnWorld::E_ENTITYTYPE_PROP )
        {
            continue;
        }

        // :967
        UpdatePropEvent lUpdatePropEvent;

        // :1008..:1011  rw::physics::RigidBody::GetTransform() -- the mRi/mUp/mAt basis rows
        // (+0x40/+0x50/+0x60) with mCom (+0x10) as the translation row. The console materialises
        // the same value three times on the stack (a dead copy at sp+0x210, the event's own row at
        // sp+0x120 and the SetTransform argument at sp+0x1B0); one source local, copied.
        Matrix44Affine lUpdatedTransform       = lpUpdateBodyEvent->mRigidBody.GetTransform();
        Vector3        lUpdatedPosition        = lUpdatedTransform.Pos();
        Vector3        lUpdatedLinearVelocity  = lpUpdateBodyEvent->mRigidBody.GetLinearVelocity();
        Vector3        lUpdatedAngularVelocity = lpUpdateBodyEvent->mRigidBody.GetAngularVelocity();

        // :968  See decode (A): `lwz r11,0x9C(event)` is mRigidBody.mState (event+0x10 + 0x8C),
        // and `extrwi r10,r11,1,30` isolates FROZEN_BODY.
        bool lbFrozen =
            ( lpUpdateBodyEvent->mRigidBody.GetState() & rw::physics::FROZEN_BODY ) != 0;

        // Out-of-world floor. See decode (D) for the compare polarity.
        if ( lUpdatedPosition.y < KVF_PROP_OUT_OF_WORLD_HEIGHT.x )
        {
            if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
            {
                // The `stw 2, gpDebugPrint+4` between the two virtual sink calls is mePrintMode
                // := E_PRINTMODE_HEXONCE, i.e. the id is logged in hex exactly once. This is the
                // gpDebugPrint GLOBAL, which is why the DWARF spells it as four BARE
                // StrStreamBase::operator<< calls and not as a local StrStream -- see note (L).
                *CgsDev::Log::gpDebugPrint << "\t Warning!! prop fell out of the world: "
                                           << CgsDev::E_PRINTMODE_HEXONCE
                                           << static_cast<u64>( lRigidBodyId )
                                           << "\n";

                // [DIAG] NOT IN THE X360 BINARY -- a wave-Q6 SUFFIX on the console's own warning
                // (opt in with BRN_PROP_DIAG), first 16 only. scout.md §4.7: the floor constant
                // KVF_PROP_OUT_OF_WORLD_HEIGHT is not printed anywhere, yet it decides how long a
                // part falls before the freeze deletes it -- i.e. how much of the motion a drive
                // test can even observe. Printing the threshold NEXT TO the y that tripped it also
                // makes the memory-file's recurring failure ("a console constant read as a host
                // value", here Splat(-1000.0f) recovered from a dyn-init thunk) checkable in one
                // glance instead of by re-deriving the initialiser.
                {
                    static const bool sbPropDiag  = ( getenv( "BRN_PROP_DIAG" ) != 0 );
                    static u32        suDiagCount = 0;
                    if ( sbPropDiag && suDiagCount < 16u )
                    {
                        ++suDiagCount;
                        *CgsDev::Log::gpDebugPrint
                            << "[Q6-read] out-of-world floor="
                            << KVF_PROP_OUT_OF_WORLD_HEIGHT.x
                            << " pos.y=" << lUpdatedPosition.y
                            << "\n";
                    }
                }
            }
            lbFrozen = true;
        }

        // `if (raw) { if (!mbDisableFreezing) 1 else 0 } else 0` -- the debug switch suppresses
        // BOTH the freeze-driven and the fell-out-of-the-world removal.
        lbFrozen = lbFrozen && !mbDisableFreezing;

        lUpdatePropEvent.mbFrozen = lbFrozen;

        // [DIAG] see the census note at the top of this function. Counted here, after the
        // mbDisableFreezing fold, so the number matches what the event actually carries.
        if ( sbPropDiag && lbFrozen )
        {
            ++liDiagFrozen;
        }

        // :1035  The explicit PropEntityID(u32) ctor stores the word then fires AssertIsProp --
        // owner tripwire #1 of four, BrnPropEntityID.h:278. See decode (H).
        const PropEntityID lEntityId( static_cast<u32>( lRigidBodyId.GetEntityId() ) );

        lUpdatePropEvent.mEntityId     = lEntityId;
        lUpdatePropEvent.miPhysicsSlot = static_cast<s16>( lRigidBodyId.GetIndex() );
        lUpdatePropEvent.mTransform    = lUpdatedTransform;

        // :1017  The extra prop gravity: one InApplyForce per moving, non-removed prop.
        // `vmsum3fp128 v0,v123,v123` then `vcmpgtfp128. v0,v0,v127(1.0f)`.
        // ⚠️ Both globals read as ZERO in the shipped image -- see decode (K).
        if ( !lbFrozen )
        {
            if ( rw::math::vpu::MagnitudeSquared( lUpdatedLinearVelocity ) > 1.0f )
            {
                CgsPhysics::PhysicsSimulationIO::InApplyForce lApplyForceEvent;
                lApplyForceEvent.mID    = lRigidBodyId;
                lApplyForceEvent.mForce = K_DEFAULT_GRAVITY * ( KVF_GRAVITY_SCALE.x - 1.0f );

                // @0x82632D44 `bl sub_825BCD60` -> the NON-const accessor; @0x82632D4C the
                // ONE-ARG AddEvent(const T&), which has been committed all along. (Round 1 filed
                // this as needing a NEW AddEvent overload. It did not -- the C2663 was purely the
                // const-ness of the accessor. Recorded so the retired request is not re-filed.)
                lpSimModuleInputBuffer->GetApplyForceQueue()->AddEvent( lApplyForceEvent );
            }
        }

        // :1027  mbRenderCOM (+0x48). The console builds a stack DebugInterface, draws the gizmo on
        // the event's own transform row (sp+0x120), and releases through the inlined
        // DebugManager::ThreadSafeRelease (Leave, then assert lpDebugManager == mpInstance,
        // CgsDebugManager.h:353). The committed ~DebugInterface is a documented no-op, so the
        // release is explicit -- the same spelling BehaviourRig::Update uses in-tree.
        //
        // ⚠️ ONE CONSOLE BRANCH IS DELIBERATELY ABSENT AND IT IS EXACT, NOT A SIMPLIFICATION
        // (round-1 G6 NIT 1, applied). The console gates the release on the stack DebugInterface's
        // OWN mbIsAutomaticClass byte: 0x82632D7C `lbz r11, var_2CC(r1)` / 0x82632D80 `cmplwi
        // cr6,r11,0` / 0x82632D84 `beq cr6, loc_82632DBC`, where var_2CC is var_2D0 (the
        // DebugInterface) + 4 and CgsDebugInterface.h:107-108 lays the struct out as
        // mpDebugManager(+0), mbIsAutomaticClass(+4). The default ctor ALWAYS sets that flag
        // (CgsDebugInterface.h:33 `, mbIsAutomaticClass(true)`), so the branch is always taken and
        // the unconditional release below is a reproduction. Said out loud so the next agent
        // diffing this arm against the asm does not see an unexplained missing branch.
        if ( mbRenderCOM )
        {
            CgsDev::DebugInterface lInt;
            lInt.GetRender().DrawAxis(
                reinterpret_cast<const f32*>( &lUpdatePropEvent.mTransform ) );
            CgsDev::DebugManager::ThreadSafeRelease( &lInt.GetDebugManager() );
        }

        // See decode (F): the event carries the RAW velocities, written before any clamping.
        lUpdatePropEvent.mLinearVelocity  = lUpdatedLinearVelocity;
        lUpdatePropEvent.mAngularVelocity = lUpdatedAngularVelocity;

        // :969  `clrlwi r11,r24,22` -- the id's low 10-bit part field. Owner tripwire #2 rides the
        // inlined GetPartIndex(). DWARF `bool lbIsPart`.
        if ( lEntityId.GetPartIndex() != 0 )
        {
            // ---- one shed PART of a smashed prop -------------------------------------------
            // [DIAG] see the census note at the top of this function.
            if ( sbPropDiag )
            {
                ++liDiagParts;
            }

            // :1041
            const s32 liPartIndex = static_cast<s32>( lRigidBodyId.GetIndex() );

            CGS_ASSERT( liPartIndex != KI_PROP_INDEX_NOT_FOUND,
                        "liPartIndex != KI_PROP_INDEX_NOT_FOUND" );                       // :1042
            CGS_ASSERT( liPartIndex >= 0
                        && liPartIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS ),
                        "liPartIndex >= 0 && liPartIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS )" ); // :1043

            // :1045  `slwi r11,r27,6` == index * 64 == the CONSOLE PropPartInstance stride; the
            // slot is reached by NAME, not by that constant (AGENTS.md gotcha 1).
            PropPartInstance* lpPart = &mpaPartInstances[ liPartIndex ];

            // Owner tripwire #3 rides GetEntityId().GetValue().
            CGS_ASSERT( static_cast<u32>( lRigidBodyId.GetEntityId() )
                            == lpPart->GetEntityId().GetValue(),
                        "lpUpdateBodyEvent->mID.GetEntityId() == lpPart->GetEntityId().GetValue()" ); // :1047

            lUpdatePropEvent.miTypeId = static_cast<s16>( lpPart->GetType() );

            if ( lbFrozen )
            {
                RemovePart( lEntityId, static_cast<u32>( liPartIndex ),
                            lpSceneInput, lpSimModuleInputBuffer );
            }
            else
            {
                // v1 = lpPart->mLinearVelocity (+0x10), v2 = lpPart->mAngularVelocity (+0x20) --
                // LAST frame's values; the two Vector3& out-params are updated in place.
                ClampAcceleration( lpPart->GetLinearVelocity(), lpPart->GetAngularVelocity(),
                                   lRigidBodyId, lpUpdateBodyEvent,
                                   lUpdatedLinearVelocity, lUpdatedAngularVelocity,
                                   lvfTimeStep, lvfOneOverTimeStep, lpSimModuleInputBuffer );
            }

            lpPart->SetPosition( lUpdatedPosition );
            lpPart->SetLinearVelocity( lUpdatedLinearVelocity );
            // ⚠️ MEASURED: a BARE `stvx128 v0, r31, 32` -- no IsValid tripwire, unlike its two
            // siblings above. BrnPropPartInstance.h's SetAngularVelocity is inline and
            // assert-free for exactly this reason; do not "restore" a tripwire here.
            lpPart->SetAngularVelocity( lUpdatedAngularVelocity );
        }
        else
        {
            // ---- a whole prop ----------------------------------------------------------------
            // [DIAG] see the census note at the top of this function.
            if ( sbPropDiag )
            {
                ++liDiagProps;
            }

            // :1078
            const s32 liPropIndex = static_cast<s32>( lRigidBodyId.GetIndex() );

            CGS_ASSERT( liPropIndex != KI_PROP_INDEX_NOT_FOUND,
                        "liPropIndex != KI_PROP_INDEX_NOT_FOUND" );                       // :1079
            CGS_ASSERT( liPropIndex >= 0
                        && liPropIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS ),
                        "liPropIndex >= 0 && liPropIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS )" ); // :1080

            // :1082  `mulli r11,r27,0x70` == index * 112 == the CONSOLE PropInstance stride;
            // reached by name (AGENTS.md gotcha 1).
            PropInstance* lpProp = &mpaPropInstances[ liPropIndex ];

            // `lbz r11,0x6F(prop) ; rlwinm r11,r11,0,30,30` == mu8Flags &
            // KU_HAS_EXTRA_COM_OFFSET_FLAG -- undo the shift AddPropToSim applied. Decode (G),
            // including why TransformVector is NOT called here.
            if ( lpProp->HasExtraComOffset() )
            {
                lUpdatedPosition = lUpdatedPosition
                                 - ( lUpdatedTransform.Right() * K_PROP_EXTRA_COM_OFFSET.x
                                   + lUpdatedTransform.Up()    * K_PROP_EXTRA_COM_OFFSET.y
                                   + lUpdatedTransform.At()    * K_PROP_EXTRA_COM_OFFSET.z );

                lUpdatedTransform.Pos()     = lUpdatedPosition;
                lUpdatePropEvent.mTransform = lUpdatedTransform;
            }

            const PropTypeData* lpType = mpPhysicsData->GetType( lpProp->GetTypeId() );

            // PropTypeData::ShouldAlwaysSmash() (DWARF). The asm is `lbz r11,0x5D(type)`
            // (muNumberOfParts != 0 == IsSmashable) AND `lfs 0x54 < lfs 0x50 + 0.1f`
            // (mfSmashThreshold below mfMoveThreshold + slack, i.e. the prop breaks before it can
            // ever be reported as moving). The float test is `fcmpu`+`blt` == an ORDERED `<`, so no
            // negated-predicate rewrite is needed (AGENTS.md gotcha 4). Spelled out of the
            // committed accessors because the DWARF's helper has no declaration in the tree; the
            // console +0x50/+0x54 are the header's host +0x5C/+0x60 and are reached by accessor.
            if ( lpType->IsSmashable()
                 && lpType->GetSmashThreshold()
                        < lpType->GetMoveThreshold() + KF_ALWAYS_SMASH_THRESHOLD_SLACK )
            {
                // :1096  Pin the body back to the prop instance's authored pose and stop it dead.
                // See decodes (B) and (C). @0x82633064 `bl sub_825BCCB8` (the NON-const accessor)
                // then @0x82633068 `bl sub_825E3C30` (AllocateEventSafe) -- both landed this round.
                CgsPhysics::PhysicsSimulationIO::InUpdateRigidBody* lpEvent =
                    lpSimModuleInputBuffer->GetUpdateRigidBodyQueue()->AllocateEventSafe();

                // ⚠️ The console does NOT null-check the result -- reproduced as-is. (The callee
                // CAN return NULL on a full queue; that is the console's own exposure, not an
                // omission here.)
                lpEvent->mID        = lRigidBodyId;
                lpEvent->mRigidBody = lpUpdateBodyEvent->mRigidBody;   // RigidBody::operator=

                const Vector3 lZero = { 0.0f, 0.0f, 0.0f, 0.0f };

                // `vrlimi128 vD,vOld,1,0` + `stvx128` into +0xA0 then +0x90 -- the DWARF's
                // SetAngularAcceleration(0) / SetLinearAcceleration(0) pair. The committed
                // rigidbody.h exposes exactly that pair as ResetForces (mForce := arg,
                // mTorque := 0); with a zero argument the two spellings are identical.
                lpEvent->mRigidBody.ResetForces( lZero );
                lpEvent->mRigidBody.SetLinearVelocity( lZero );    // +0x20 mVel
                lpEvent->mRigidBody.SetAngularVelocity( lZero );   // +0x30 mOmega

                // The four w-preserving row copies + the matrix->quaternion network.
                lpEvent->mRigidBody.SetTransform( lpProp->GetTransform() );

                // `lwz r8,0x5C(body)` == mUp.w == mInertia; the guard is the CALL SITE's, exactly
                // as Simulation::AddRigidBody emits it.
                if ( lpEvent->mRigidBody.GetInertia() != NULL )
                {
                    lpEvent->mRigidBody.InertiaUpdate( lpEvent->mRigidBody.GetInertia() );
                }

                // `b loc_82633490` -- no UpdatePropEvent, no instance write-back.
                continue;
            }

            // Owner tripwire #4 rides GetEntityId().GetValue().
            CGS_ASSERT( static_cast<u32>( lRigidBodyId.GetEntityId() )
                            == lpProp->GetEntityId().GetValue(),
                        "lpUpdateBodyEvent->mID.GetEntityId() == lpProp->GetEntityId().GetValue()" ); // :1109

            // `lbz r11,0x6E(prop) ; cmplwi r11,1 ; blt / bne` -- a three-way on the movement
            // state: 0 promotes to 1 only once the body actually has velocity, 1 promotes to 2
            // unconditionally, 2 stays. The IsZero test reads the PRE-clamp velocity (decode F).
            if ( lpProp->GetMovementState() == E_PROP_MOVESTATE_STATIONARY )
            {
                if ( !rw::math::vpu::IsZero( lUpdatedLinearVelocity, KF_PROP_MOVED_EPSILON ) )
                {
                    lpProp->SetMovementState( E_PROP_MOVESTATE_JUST_MOVED );
                }
            }
            else if ( lpProp->GetMovementState() == E_PROP_MOVESTATE_JUST_MOVED )
            {
                lpProp->SetMovementState( E_PROP_MOVESTATE_MOVING );
            }

            lUpdatePropEvent.miTypeId = static_cast<s16>( lpProp->GetTypeId() );

            if ( lbFrozen )
            {
                RemoveProp( lEntityId, static_cast<u32>( liPropIndex ),
                            lpSceneInput, lpSimModuleInputBuffer );
            }
            else
            {
                // v1 = lpProp->mLinearVelocity (+0x40), v2 = lpProp->mAngularVelocity (+0x50).
                ClampAcceleration( lpProp->GetLinearVelocity(), lpProp->GetAngularVelocity(),
                                   lRigidBodyId, lpUpdateBodyEvent,
                                   lUpdatedLinearVelocity, lUpdatedAngularVelocity,
                                   lvfTimeStep, lvfOneOverTimeStep, lpSimModuleInputBuffer );
            }

            lpProp->SetTransform( lUpdatedTransform );
            lpProp->SetLinearVelocity( lUpdatedLinearVelocity );
            lpProp->SetAngularVelocity( lUpdatedAngularVelocity );
        }

        mUpdatedProps.AddEvent( lUpdatePropEvent );
    }

    // Tail: fold the jointed (leaning / tilting) props' own queue in and empty it. Decode (I).
    mUpdatedProps.Append( mUpdatedJointedProps );
    mUpdatedJointedProps.Clear();

    // [DIAG] NOT IN THE X360 BINARY. The once-per-simulated-second census line -- see the note at
    // the top of this function for what its shape means. Emitted AFTER the jointed fold so
    // `queued` is the exact length OutputUpdatedProps is about to publish, which is what the
    // matching one-shot `[Q6-out] first publish` should agree with on the frame it fires.
    if ( sbPropDiag && CgsDev::Log::gpDebugPrint != 0 )
    {
        static f32 sfDiagAccumulatedTime = 0.0f;
        sfDiagAccumulatedTime += lvfTimeStep.x;
        if ( sfDiagAccumulatedTime >= 1.0f )
        {
            sfDiagAccumulatedTime = 0.0f;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-read] props=" << liDiagProps
                << " parts=" << liDiagParts
                << " frozen=" << liDiagFrozen
                << " queued=" << mUpdatedProps.GetLength()
                << "\n";
        }
    }
}

}
}
