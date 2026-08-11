#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"        // IKBodyPart::CheckForDetachment / GetTagPoint / GetNumberOf* / SetActiveJointIndex / GetActiveJointSpec
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"   // PhysicalBodyPart::GetIKPartIndex / SetJoinedToVehicle / AddToSim
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnTagPoint.h"           // TagPoint::GetJointIndex
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                                       // CgsNumeric::Random
#include "GameShared/GameClasses/Core/CgsAssert.h"                                          // CGS_ASSERT

#include <cmath>   // std::exp / std::log -- the vexptefp / vlogefp angular-decay refinement converges here

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_Detach.cpp
//
// BrnPhysics::Deformation::DeformableObject -- the DETACHMENT group. Reconstructed
// store-for-store from BURNOUT_X360_ARTIST.XEX. This file owns the part-detachment
// decision + promotion path of the deformation model:
//
//   GetNonDetachedJointedPart   @ 0x825B9D58  find the Nth still-attached jointed IK part
//   CheckForDetachment          @ 0x8263ABC0  per-frame: which IK parts come off this frame
//   CheckForForcedDetachment    @ 0x8263AD78  randomly force one attached part to detach
//   UpdateSpinningDetachment    @ 0x8263A748  hinge a part that is spinning too fast
//   DetachPart                  @ 0x826307D0  promote an IK part to a PhysicalBodyPart + notify
//
// CALL GRAPH (within this group + into homed siblings):
//   Update                  -> CheckForForcedDetachment, UpdateSpinningDetachment
//   UpdateIKAndLocators     -> CheckForDetachment
//   CheckForDetachment      -> IKBodyPart::CheckForDetachment, DetachedPartManager::TestJointForBreaking,
//                              DetachPart
//   CheckForForcedDetachment-> GetNonDetachedJointedPart, IKBodyPart::GetTagPoint, DetachPart
//   UpdateSpinningDetachment-> GetNonDetachedJointedPart, IKBodyPart::GetTagPoint, DetachPart
//   DetachPart              -> DetachedPartManager::MakePart, PhysicalBodyPart::{GetIKPartIndex,
//                              SetJoinedToVehicle,AddToSim}, IKBodyPart::{GetTagPoint,SetActiveJointIndex,
//                              GetActiveJointSpec}, the detached-part notification emit
//
// ============================ MODELLED-vs-ASM (read before editing) ============================
// Same conventions as the committed sibling slices (BrnDeformableObject.cpp / BrnPhysicalBodyPart.cpp
// / BrnDeformationSensor.cpp): VMX128 vector math is modelled lane-by-lane in scalar f32; control
// flow / branches / early-outs / call order / named-member stores / asserts are reproduced EXACTLY.
// Asserts are NON-GATING tripwires (BeginAssert/FireAssert/EndAssert == one CGS_ASSERT; execution
// continues past a failed assert, so the C++ falls through identically). Assert message strings are
// the asm FireAssert strings with the original file/line stripped per project rule.
//
// ---- console byte-offset -> frozen-header member map (this group's asm) -----------------------
// The asm indexes the object by 32-bit console byte offset. Several accesses appear scaled by the
// pointer element type Hex-Rays inferred (a _DWORD* index scales by 4, an __int16* index by 2). The
// unambiguous members below are reached BY NAME; the host recomputes the addresses, so semantic
// parity is exact even though the absolute host offsets differ:
//   +25380 (16*part)  maIKParts[part]              (IKBodyPart, stride 16)
//   +25388            maIKParts[part].mpSpec        -> spec fields: +452 GetNumberOfJoints,
//                                                      +456 (joint-link gate), +472 GetNumberOfTagPoints,
//                                                      +476 GetPartType
//   +26180 (part)     maPartStates[part]            (u8 EPartState)
//   +26232            miNumIKBodyParts              (s32 live IK-part count; loop bound)
//   +26236 (count)    mau8PhysicalBodyPartPoolIndex (u8[]; appended at mi16NumPhysicalParts)
//   +26286            mi16NumPhysicalParts          (s16; <20 cap; post-incremented on detach)
//   +26288            mi16NumHingedParts            (s16; ++ when hinged, -- when a hinged part comes off)
//   +26290            mu16DeformableObjectIndex     (u16; passed to MakePart)
//   +26384            mHandlingBodyID               (RigidBodyId; entity-id source for the notify/print)
//   +26388            mGlobalEntityId               (EntityId; passed to MakePart)
//   +6476 (deref)     mVehicleBody attached body    -> +1808 "is simulating" flag, +96 angular velocity
//   +3904             mAngularVelocitySum           (VecFloat; the per-frame spin accumulator)
//   +3920..3923       mu8WheelTagPointIndices[4]    (u8; reset to 0xFF when the matching panel detaches)
//
// FLAG -- LAYOUT-DERIVED (not byte-proven) member identifications. A few flag/counter offsets do not
// fall on a uniquely-named frozen member by the recovered layout arithmetic (the committed
// ApplyCarCarImpulse slice hit the same wall at +26392/+26413/+26414 and retained reconstructed
// members). They are reached here through the best-fit NAMED frozen members below, each FLAGGED, so
// the control flow is faithful and the identification can be re-homed when the exact byte layout is
// re-derived. NONE fabricates a value; NONE is a raw-offset poke:
//   * +26409 (bool) -- the per-object "attached parts may impulse-detach this frame" gate
//     (CheckForDetachment). Mapped to mbForceWheelsToDetach (the recovered per-object detach-enable
//     latch in the bool block; the closest-fit named flag).
//   * +26400 (s16) -- a small "remaining structural/bonnet parts" counter the hinge path tests >1 and
//     decrements (CheckForDetachment + DetachPart). Mapped to mi8NumPartsToForceHinging (the recovered
//     small force-hinge counter; widened compare is value-preserving for the >1 test).
//   * +26417 (s16) -- the "max attached jointed parts" budget CheckForForcedDetachment differences
//     against mi16NumHingedParts (>0 => room to force one more off). Modelled as the KI_MAX_PHYSICAL_PARTS
//     cap (the same 20-slot budget every detach is gated against), preserving the "is there room"
//     semantics without a fabricated member.
//   * +26460 (EPartState) -- a per-object overall part state the outer CheckForDetachment gate
//     early-outs on when == DETATCHED. Modelled via meAbsorptionSet's neighbouring per-object state;
//     here the equivalent observable is "are all parts already detached", derived from
//     mi16NumPhysicalParts vs miNumIKBodyParts (no raw-offset read).
//
// FLAGGED-0 PLACEHOLDERS (rodata NOT in the per-function exports -- NEVER fabricated):
//   * KF_ANGULAR_VELOCITY_DECAY          (UpdateSpinningDetachment: per-step exponential spin decay)
//   * KF_ANGULAR_VELOCITY_FOR_DETACHMENT (UpdateSpinningDetachment: spin-speed^2 hinge threshold)
//   * KB_ALLOW_RANDOM_PART_DETACHMENT    (UpdateSpinningDetachment master switch; file-static bool)
//   * the vexptefp/vlogefp Chebyshev polynomial coefficient rows (&unk_82014A?0) the asm uses to
//     refine 2^x / log2(x) for the decay -- carried as a documented exp()/log() model.
// The decay/threshold SHAPE (decay the accumulator, compare its magnitude to the threshold, and on
// over-threshold pick + hinge a random still-attached part) is exact; the numeric decay stays inert
// until the rodata is recovered.
//
// The detached-part manager + notification queue are NOT homed in-tree. The asm calls
// DetachedPartManager::MakePart, DetachedPartManager::TestJointForBreaking, builds a
// DetachedPartNotificationEvent and AddEventSafeAppend's it onto the sim OutputBuffer. Per the per-TU
// gate rule (mirroring BrnPhysicalBodyPart.cpp's EmitDetachedPartNotification hook), those un-homed
// callees are reached through the FLAGGED provisional free-function hooks declared below -- the owning
// TUs emit the authoritative bodies and may relocate these decls.

namespace BrnPhysics
{
namespace Deformation
{
    // -----------------------------------------------------------------------------------------
    // Provisional hooks for the not-yet-homed detached-part manager + sim-output notification
    // queue (FLAG -- shape recovered from the asm call sites; owning TUs author the real bodies).
    // -----------------------------------------------------------------------------------------
    PhysicalBodyPart* MakeDetachedPart(DetachedPartManager* lpPartMgr,
                                       CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                       u16 lu16DeformableObjectIndex, DeformableObject* lpObject,
                                       EntityId lGlobalEntityId, RigidBodyId lHandlingBodyId,
                                       s32 liPartIndex, IKBodyPart* lpIKPart);                  // FLAG: provisional
    void EmitDetachedPartNotification(CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput,
                                      const void* lpEventBlob);                                  // FLAG: provisional
    bool TestJointForBreaking(DetachedPartManager* lpPartMgr, s32 liJointHandle,
                              CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                              CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput);          // FLAG: provisional

    // FLAG: the +1808 "is the body actively simulating" flag the asm reads off the attached body
    // (*(*(this+6476)+1808)). The attached-body slice does not expose that flag by name yet; modelled
    // as the inverse of the body's frozen state (a frozen body is not simulating), reached through the
    // committed GetVehicleBody() accessor + the base IsFrozen(). Re-home when the flag is named.
    static bool IsAttachedBodySimulating(const DeformableObject& lrObj)
    {
        return !lrObj.GetVehicleBody().IsFrozen();
    }

    namespace
    {
        // ---- detach-part-state constants -----------------------------------------------------
        const u8 KU_PART_STATE_ATTACHED_IK = 2;   // E_PART_STATE_ATTACHED_IK
        const u8 KU_PART_STATE_HINGED      = 3;   // E_PART_STATE_HINGED
        const u8 KU_PART_STATE_DETATCHED   = 4;   // E_PART_STATE_DETATCHED (DWARF spelling)

        // The IK-part cap the asm guards every detach against (mi16NumPhysicalParts < 20 / the
        // "detached parts full" warning) AND the forced-detach budget (+26417, see file header).
        const s16 KI_MAX_PHYSICAL_PARTS = 20;

        // The two part-type ids the hinge logic treats as "structural / bonnet-ish" (the asm's
        // `type == 84 || type == 85` test that gates the structural-part counter). FLAG: asm literals.
        const s32 KI_BODY_PART_STRUCTURAL_A = 84;   // 0x54
        const s32 KI_BODY_PART_STRUCTURAL_B = 85;   // 0x55

        // 0xFF wheel-tag-point sentinel the DetachPart wheel-panel switch writes (== -1 as u8) is the
        // frozen-header member DeformableObject::KU_INVALID_WHEEL_TAG_POINT_INDEX (DWARF :610); used
        // unqualified below.

        // Wheel-panel body-part type ids the DetachPart tail switch dispatches on (asm cases
        // 0x30..0x33 / 0x82 / 0x83). FLAG: asm literals.
        const s32 KI_BODY_PART_WHEEL_0  = 0x30;
        const s32 KI_BODY_PART_WHEEL_1  = 0x31;
        const s32 KI_BODY_PART_WHEEL_2  = 0x32;
        const s32 KI_BODY_PART_WHEEL_3  = 0x33;
        const s32 KI_BODY_PART_WHEEL_23 = 0x82;   // invalidates lanes 2 + 3
        const s32 KI_BODY_PART_WHEEL_01 = 0x83;   // invalidates lanes 0 + 1

        // FLAGGED-0 PLACEHOLDERS for the spinning-detachment rodata (NEVER fabricated). Honest zeros.
        const f32  KF_ANGULAR_VELOCITY_DECAY          = 0.0f;   // FLAG: rodata kfAngularVelocityDecay unrecovered
        const f32  KF_ANGULAR_VELOCITY_FOR_DETACHMENT = 0.0f;   // FLAG: rodata kfAngularVelocityForDetachment unrecovered
        const bool KB_ALLOW_RANDOM_PART_DETACHMENT    = true;   // FLAG: rodata kbAllowRandomPartDetachment unrecovered

        // SIMD magnitude-squared of a VecFloat's xyz lanes (vmsum3fp128 / the spin-speed^2 compared).
        inline f32 MagnitudeSquared3(const VecFloat& lvf)
        {
            return lvf.x * lvf.x + lvf.y * lvf.y + lvf.z * lvf.z;
        }
    }

    // =============================================================================================
    // GetNonDetachedJointedPart @ 0x825B9D58 -- find the (liIndex+1)-th still-attached IK part that
    // owns at least one deformation joint; return its IK-part index, or -1 if there are fewer.
    //
    // Walk the live IK parts (miNumIKBodyParts). A part qualifies when its state is ATTACHED_IK AND
    // its spec reports a non-zero joint count (spec +452 == GetNumberOfJoints). Skip `liIndex`
    // qualifiers; the one that drives the running skip to -1 (`liSkip-- == 0`) is the answer. The
    // index is advanced + bound-tested at the BOTTOM of the loop, exactly as the asm.
    // =============================================================================================
    s32 DeformableObject::GetNonDetachedJointedPart(s32 liIndex)
    {
        const s32 liNumParts = miNumIKBodyParts;          // v2 = *(a1 + 26232)
        if ( liNumParts <= 0 )                            // v2 <= 0 -> none
            return -1;

        s32 liSkip = liIndex;                              // a2 (consumed on each qualifier)
        s32 liPart = 0;                                    // v3
        for ( ; ; )
        {
            if ( maPartStates[liPart] == KU_PART_STATE_ATTACHED_IK )       // *(a1 + 26180 + v3) == 2
            {
                if ( maIKParts[liPart].GetNumberOfJoints() != 0 )          // *(*i + 452)
                {
                    if ( liSkip-- == 0 )                                   // a2-- == 0 -> found
                        return liPart;
                }
            }
            if ( ++liPart >= liNumParts )                                  // ++v3 >= v2 -> exhausted
                return -1;
        }
    }

    // =============================================================================================
    // CheckForDetachment @ 0x8263ABC0 -- per-frame pass over the IK parts deciding which come off.
    //
    // Outer gate: only run when there is room (mi16NumPhysicalParts < 20), the object is not already
    // fully detached (the +26460 overall-state gate), and there are live IK parts (miNumIKBodyParts
    // > 0). Then walk every live IK part:
    //   * ATTACHED_IK (state 2) AND attached-parts-detachable (+26409): ask IKBodyPart::CheckForDetachment
    //     whether the accumulated impulse warrants detaching this panel; if so, DetachPart it as a free
    //     body (lbHinge == false).
    //   * HINGED (state 3): the part is already a hinged joint. If it is NOT structural, or the
    //     structural budget (+26400) still allows it (>1), ask the DetachedPartManager whether the
    //     joint has now broken (TestJointForBreaking). If it broke: assert there is a hinged part,
    //     zero the spin accumulator, mark the slot DETATCHED, drop the hinged count, and (for
    //     structural parts) drop the structural budget too.
    // (lpInput==a2, lpOutput==a3, lpPartMgr==a4; lfTimeStep is the X360-dropped trailing arg.)
    // =============================================================================================
    void DeformableObject::CheckForDetachment(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                              CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput,
                                              DetachedPartManager* lpPartMgr, f32 lfTimeStep)
    {
        // Outer gate. +26460 overall "already fully detached" state modelled as "every IK part is now
        // a physical part" (mi16NumPhysicalParts >= miNumIKBodyParts) -- see file-header FLAG.
        const bool lbAllDetached =
            ( miNumIKBodyParts > 0 && mi16NumPhysicalParts >= miNumIKBodyParts );
        if ( mi16NumPhysicalParts < KI_MAX_PHYSICAL_PARTS
             && !lbAllDetached
             && miNumIKBodyParts > 0 )
        {
            const s32 liNumParts = miNumIKBodyParts;       // _R31[6558] bound
            for ( s32 li = 0; li < liNumParts; ++li )      // v8 = &maPartStates[li], v9 = &maIKParts[li]
            {
                const u8 lu8State = maPartStates[li];      // v12 = *v8

                if ( lu8State == KU_PART_STATE_ATTACHED_IK && mbForceWheelsToDetach )   // +26409 (FLAG)
                {
                    // IKBodyPart::CheckForDetachment(part, impulse, &detachJointOut). The asm passes
                    // a2 (the accumulated-impulse arg) + a 16-byte scratch out (v17).
                    s32 liDetachJointOut = 0;                          // v17[] scratch
                    const f32 lfImpulse = lfTimeStep;                  // a2 (the impulse arg; see note below)
                    if ( maIKParts[li].CheckForDetachment(lfImpulse, liDetachJointOut) )
                    {
                        // Promote the panel to a free PhysicalBodyPart (no hinge -> lbHinge == false).
                        DetachPart(lpInput, lpOutput, lpPartMgr, li, /*liJointIndex*/ 0, /*lbHinge*/ false);
                    }
                }
                else if ( lu8State == KU_PART_STATE_HINGED )
                {
                    // Structural-part predicate: type == 84 || type == 85 (the asm's v13/v14 latch).
                    const s32 liType = static_cast<s32>(maIKParts[li].GetPartType());   // *(*v9 + 476)
                    const bool lbStructural =
                        ( liType == KI_BODY_PART_STRUCTURAL_A || liType == KI_BODY_PART_STRUCTURAL_B );

                    // Run the joint-break test unless this is a structural part whose budget is spent.
                    // (asm: `if ( !v14 || *(_R31 + 13200) > 1 )` -- not structural OR budget > 1.)
                    if ( !lbStructural || mi8NumPartsToForceHinging > 1 )   // +26400 (FLAG)
                    {
                        // DetachedPartManager::TestJointForBreaking(partMgr, jointHandle, in, out).
                        // jointHandle == v9[2] == maIKParts[li] pool/joint handle (+12 region).
                        const s32 liJointHandle = maIKParts[li].GetPartPoolIndex();   // v9[2]
                        if ( TestJointForBreaking(lpPartMgr, liJointHandle, lpInput, lpOutput) )
                        {
                            // The joint broke. There must be a hinged part to consume.
                            CGS_ASSERT(mi16NumHingedParts > 0, "mi16NumHingedParts > 0");

                            // vspltisw v0,0 / stvx128 v0,r31,3904 -> zero the spin accumulator (+3904).
                            mAngularVelocitySum.SetZero();

                            maPartStates[li] = KU_PART_STATE_DETATCHED;   // *v8 = 4
                            --mi16NumHingedParts;                         // --*(_R31 + 13144)
                            if ( lbStructural )                           // if ( v16 )
                                --mi8NumPartsToForceHinging;              // --*(_R31 + 13200)  (+26400, FLAG)
                        }
                    }
                }
            }
        }

        // NOTE: the X360 passes the accumulated-impulse value in a2 (an integer-register float). The
        // clean signature here exposes the trailing arg as lfTimeStep; the impulse and time-step occupy
        // the same dropped-arg region in the Hex-Rays view. FLAG: the exact arg mapping is best-effort.
    }

    // =============================================================================================
    // CheckForForcedDetachment @ 0x8263AD78 -- randomly force one still-attached jointed part to
    // detach this frame, used to keep wrecks visually busy.
    //
    // Gate: room for another physical part (mi16NumPhysicalParts < 20) AND the forced-detach budget
    // still has slack (budget - mi16NumHingedParts > 0). Then:
    //   1) Draw a random part ordinal from the supplied Random (the asm advances muSeed inline with
    //      the PCG LCG; modelled via RandomUInt) and resolve it via GetNonDetachedJointedPart.
    //      -1 => nothing to force, return.
    //   2) Assert the chosen part is ATTACHED_IK; assert its joint count > 0 (the "luMod > 0" random
    //      modulo guard).
    //   3) Draw a random joint index in [0, jointCount); find the tag point whose joint index matches,
    //      then DetachPart it AS A HINGE (lbHinge == true) hanging from that tag point.
    // (lpInput==a2, lpOutput==a3, lpPartMgr==a4, lpRandom==a5; lfTimeStep is dropped.)
    // =============================================================================================
    void DeformableObject::CheckForForcedDetachment(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                                    CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput,
                                                    DetachedPartManager* lpPartMgr,
                                                    CgsNumeric::Random* lpRandom, f32 lfTimeStep)
    {
        // *(result+26286)<20 && *(result+26417)-*(result+26288)>0. The +26417 budget is the
        // KI_MAX_PHYSICAL_PARTS cap (FLAG, see file header), differenced against mi16NumHingedParts.
        if ( mi16NumPhysicalParts < KI_MAX_PHYSICAL_PARTS
             && ( KI_MAX_PHYSICAL_PARTS - mi16NumHingedParts ) > 0 )
        {
            // (1) random part ordinal -> resolve to a real attached jointed part.
            const u32 luPartDraw = lpRandom->RandomUInt();             // inline LCG draw (*(a5+32))
            const s32 liPart = GetNonDetachedJointedPart(static_cast<s32>(luPartDraw));
            if ( liPart != -1 )
            {
                // (2) the chosen part must be ATTACHED_IK.
                CGS_ASSERT(maPartStates[liPart] == KU_PART_STATE_ATTACHED_IK,
                           "maPartStates[ liPartIndex ] == E_PART_STATE_ATTACHED_IK");

                IKBodyPart& lrPart = maIKParts[liPart];               // v12 = 16*v11 + v5
                const u32 luNumJoints = static_cast<u32>(lrPart.GetNumberOfJoints());   // *(*(v12+25388)+452)
                CGS_ASSERT(luNumJoints > 0, "luMod > 0");             // CgsRandom modulo guard

                // (3) random joint index in [0, jointCount).
                const u32 luJointDraw = lpRandom->RandomUInt();
                const s32 liTargetJoint = ( luNumJoints != 0 )
                                            ? static_cast<s32>( luJointDraw % luNumJoints )
                                            : 0;

                // Find the tag point whose joint index matches the drawn target joint.
                s32 liTagPoint = 0;                                    // v15
                const s32 liNumTagPoints = lrPart.GetNumberOfTagPoints();   // *(*(v12+25388)+472)
                for ( ; liTagPoint < liNumTagPoints; ++liTagPoint )
                {
                    if ( lrPart.GetTagPoint(liTagPoint)->GetJointIndex() == liTargetJoint )
                        break;
                }

                // DetachPart as a hinge (lbHinge == true) hanging from the located tag point.
                DetachPart(lpInput, lpOutput, lpPartMgr, liPart, liTagPoint, /*lbHinge*/ true);
            }
        }

        (void)lfTimeStep;   // X360-dropped trailing arg.
    }

    // =============================================================================================
    // UpdateSpinningDetachment @ 0x8263A748 -- decay the per-frame spin accumulator and, if it is
    // still spinning above the threshold, force a random attached jointed part to hinge off.
    //
    // Gate: the global kbAllowRandomPartDetachment master switch. If off (or the attached body is not
    // actively simulating, *(body+1808) == 0), zero the spin accumulator and return.
    //   1) Decay the spin accumulator (+3904) by the per-step exponential KF_ANGULAR_VELOCITY_DECAY
    //      raised to (timeStep) -- the vexptefp/vlogefp polynomial the asm builds. With FLAGGED-0
    //      decay this is exp(0)==1 (no decay); SHAPE is exact.
    //   2) Compare the decayed spin magnitude-squared against KF_ANGULAR_VELOCITY_FOR_DETACHMENT
    //      (vcmpgtfp). Below threshold: return.
    //   3) Over threshold: draw a random attached jointed part, assert it is ATTACHED_IK + joint-count
    //      > 0, draw a random joint, locate its tag point, and DetachPart it AS A HINGE (lbHinge ==
    //      true). (The "Hinging part due to spinning:" trace is a debug-only log; omitted.)
    // (lpInput==a2, lpOutput==a3, lpPartMgr==a4, lrRandom==a5; this==_R29.)
    // =============================================================================================
    void DeformableObject::UpdateSpinningDetachment(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                                    CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput,
                                                    DetachedPartManager* lpPartMgr, VecFloat lvfTimeStep,
                                                    CgsNumeric::Random& lrRandom)
    {
        // Master gate. asm @0x8263A748: `if ( !kbAllowRandomPartDetachment ) goto LABEL_17;` -- when the
        // master switch is OFF the function jumps straight to LABEL_17 (restore + return) WITHOUT zeroing
        // the spin accumulator. Do NOT SetZero here.
        if ( !KB_ALLOW_RANDOM_PART_DETACHMENT )
            return;

        // "is the body actively simulating" gate (*(body+1808) == 0). Only on the master-ON-but-body-not-
        // simulating branch does the asm zero +3904 (vspltisw v0,0 / stvx128 v0,r29,3904) before falling
        // into LABEL_17 (return).
        if ( !IsAttachedBodySimulating(*this) )
        {
            mAngularVelocitySum.SetZero();   // stvx128 0 -> +3904
            return;
        }

        // (1) decay the spin accumulator. The asm samples the body's angular velocity (lvx body+96),
        // folds it into the accumulator, then multiplies by pow(KF_ANGULAR_VELOCITY_DECAY, timeStep)
        // built via the exp/log polynomial. With FLAGGED-0 decay the factor is exp(0)==1 (the
        // accumulator is held, not amplified).
        const f32 lfTimeStep = lvfTimeStep.x;
        const Vector3 lBodyOmega = GetVehicleBody().GetAngularVelocity();   // lvx body+96
        mAngularVelocitySum.x += lBodyOmega.x;
        mAngularVelocitySum.y += lBodyOmega.y;
        mAngularVelocitySum.z += lBodyOmega.z;

        const f32 lfDecayFactor = ( KF_ANGULAR_VELOCITY_DECAY > 0.0f )
                                    ? std::exp( std::log( KF_ANGULAR_VELOCITY_DECAY ) * lfTimeStep )
                                    : 1.0f;   // FLAGGED-0 decay -> identity
        mAngularVelocitySum.x *= lfDecayFactor;
        mAngularVelocitySum.y *= lfDecayFactor;
        mAngularVelocitySum.z *= lfDecayFactor;

        // (2) over-threshold test (vcmpgtfp v13, spinSpeed^2, threshold). Below threshold returns.
        const f32 lfSpinSpeedSq = MagnitudeSquared3(mAngularVelocitySum);
        if ( !( lfSpinSpeedSq > KF_ANGULAR_VELOCITY_FOR_DETACHMENT ) )
            return;

        // (3) force a random attached jointed part to hinge off. Inline LCG draw -> part ordinal.
        const u32 luPartDraw = lrRandom.RandomUInt();             // *(a5+32) advance
        const s32 liPart = GetNonDetachedJointedPart(static_cast<s32>(luPartDraw));
        if ( liPart == -1 )
            return;

        // (debug log "Hinging part due to spinning: <part> my entity id: <mHandlingBodyID>" is a
        // gxMessageFilterFlags-gated trace; omitted -- no observable state change.)

        CGS_ASSERT(maPartStates[liPart] == KU_PART_STATE_ATTACHED_IK,
                   "maPartStates[ liPartIndex ] == E_PART_STATE_ATTACHED_IK");

        IKBodyPart& lrPart = maIKParts[liPart];                  // v36 = 16*v27 + _R29
        const u32 luNumJoints = static_cast<u32>(lrPart.GetNumberOfJoints());   // *(*(v36+25388)+452)
        CGS_ASSERT(luNumJoints > 0, "luMod > 0");                // CgsRandom modulo guard

        // Random joint index in [0, jointCount); locate the matching tag point.
        const u32 luJointDraw = lrRandom.RandomUInt();
        const s32 liTargetJoint = ( luNumJoints != 0 )
                                    ? static_cast<s32>( luJointDraw % luNumJoints )
                                    : 0;
        s32 liTagPoint = 0;                                       // v39
        const s32 liNumTagPoints = lrPart.GetNumberOfTagPoints();
        for ( ; liTagPoint < liNumTagPoints; ++liTagPoint )
        {
            if ( lrPart.GetTagPoint(liTagPoint)->GetJointIndex() == liTargetJoint )
                break;
        }

        // Hinge it (lbHinge == true) hanging from the located tag point.
        DetachPart(lpInput, lpOutput, lpPartMgr, liPart, liTagPoint, /*lbHinge*/ true);
    }

    // =============================================================================================
    // DetachPart @ 0x826307D0 -- promote IK part `liPartIndex` to a physical body part and notify.
    //
    // (this==a1, lpInput==a2, lpOutput==a3, lpPartMgr==a4, liPartIndex==a5, liJointIndex==a6,
    //  lbHinge==a7.) Flow:
    //   * Cap gate: mi16NumPhysicalParts >= 20 -> log "Warning: detached parts full:" and return.
    //   * Zero the spin accumulator (+3904).
    //   * Read the part's spec; if it carries a valid joint link (spec+456 != -1) it follows the
    //     "may be declined" structural ladder: a structural part (type 84/85) whose budget is spent
    //     (+26400<=1) and which is NOT being force-hinged (lbHinge == false) is NOT detached. (asm:
    //     `if ( spec+456 != -1 ) { if (budget>1) detach; ...; if (!structural || a7) detach; }`.)
    //   * Detach proper (LABEL_11): build the detached body's world transform + linear/angular
    //     velocity from the part's local frames and the vehicle body's motion (the dense vmaddfp
    //     block, modelled below), then DetachedPartManager::MakePart to allocate the PhysicalBodyPart.
    //     On success: assert GetIKPartIndex()==liPartIndex + slot still ATTACHED_IK, cache the new
    //     part handle into maIKParts[liPartIndex] + the pool-index array, mark the slot DETATCHED,
    //     then EITHER hinge it (lbHinge) OR add it to the sim as a free body, emit the
    //     DetachedPartNotificationEvent, and for wheel-panels invalidate the matching wheel tag-point
    //     lane(s).
    // The dense VMX transform/velocity assembly is modelled as a documented block; every named store,
    // assert, branch and call is exact.
    // =============================================================================================
    void DeformableObject::DetachPart(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                      CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput,
                                      DetachedPartManager* lpPartMgr,
                                      s32 liPartIndex, s32 liJointIndex, bool lbHinge)
    {
        const s16 li16NumPhysical = mi16NumPhysicalParts;   // v15 = *(a1 + 26286)

        // Cap gate: full -> warn + return (the asm's `else if (gxMessageFilterFlags&1) <log>`).
        if ( li16NumPhysical >= KI_MAX_PHYSICAL_PARTS )
        {
            // "Warning: detached parts full: <count>\n" -- gxMessageFilterFlags-gated debug trace;
            // omitted (no observable state change). The full count is li16NumPhysical (v18).
            (void)li16NumPhysical;
            return;
        }

        IKBodyPart& lrPart = maIKParts[liPartIndex];                 // v16 = 16*a5 + a1 (+25380)

        // vspltisw v0,0 / stvx128 v0,r31,3904 -> zero the spin accumulator.
        mAngularVelocitySum.SetZero();

        const IKBodyPartSpec* lpSpec = lrPart.GetSpec();            // _R29 = *(v16 + 25388)

        // Joint-link gate: the ENTIRE detach-proper block is nested inside `if ( *(_R29 + 456) != -1 )`
        // with NO else -- when spec+456 == -1 the function FALLS THROUGH and returns WITHOUT detaching.
        // The gate must therefore be REQUIRED to proceed (lbProceed defaults FALSE; only the
        // spec+456 != -1 branch can set it true). FLAG: spec+456 is an unrecovered IKBodyPartSpec field
        // with no named accessor (the next 4-byte slot after miNumJoints by the recovered layout
        // arithmetic == miPartGraphics); reached through the best-fit NAMED frozen member GetMeshId()
        // (== spec+456 by layout order), FLAGGED, so the control flow (required gate, no inverted-
        // polarity substitution of GetNumberOfJoints/spec+452) is faithful and re-homable.
        bool lbProceed = false;
        if ( lpSpec != nullptr && lrPart.GetMeshId() != -1 )         // *(_R29 + 456) != -1 (FLAG: +456 proxy)
        {
            // Structural budget shortcut: budget > 1 always proceeds (asm: `if (*(_R31+26400) > 1) goto LABEL_11`).
            if ( mi8NumPartsToForceHinging > 1 )                      // *(_R31 + 26400) > 1 (FLAG)
            {
                lbProceed = true;
            }
            else
            {
                // Structural-part predicate (type 84/85); a structural part NOT being force-hinged
                // (lbHinge == false) is declined here (asm: `if ( !v29 || a7 )`).
                const s32 liType = static_cast<s32>(lrPart.GetPartType());   // *(_R29 + 476)
                const bool lbStructural =
                    ( liType == KI_BODY_PART_STRUCTURAL_A || liType == KI_BODY_PART_STRUCTURAL_B );
                lbProceed = ( !lbStructural || lbHinge );            // `if ( !v29 || a7 )`
            }
        }

        if ( !lbProceed )
            return;

        // -------- LABEL_11: detach proper -----------------------------------------------------
        // Build the detached body's seed world transform + linear/angular velocity from the part's
        // local joint/COM frames and the vehicle body's motion. The asm composes:
        //   worldTransform = vehicleTransform composed with the part-local graphics frame (the
        //     vsubfp/vpermwi/vnmsubfp/vaddfp block over body rows + part offsets)
        //   linearVel      = bodyLinearVel + bodyAngularVel x (partCom - bodyCom)
        // Modelled below through the part's own seeded pose; the exact per-lane rodata-scaled terms
        // are FLAGGED-inert where unrecovered. (The scratch blobs v77 / v79 / v80 / v70 the asm
        // assembles are the transform + the two velocity vectors handed to AddToSim + the notify.)

        // DetachedPartManager::MakePart(partMgr, in, objectIndex, this, globalId, handlingBodyId,
        //   partIndex, &part). asm args (line 3138): a4=partMgr, a2=in, *(this+26290)=mu16DeformableObjectIndex,
        //   _R31=this, *(this+26388)=mGlobalEntityId, *(this+26392)=mu32GameModeState, a5=partIndex,
        //   v16+25380=&maIKParts[partIndex]. NOTE: the asm's 6th arg is the +26392 game-mode/state word
        //   (mu32GameModeState), NOT mHandlingBodyID (+26384, which is only read later for the notify at
        //   line 3232/3237). FLAG: MakeDetachedPart is a provisional hook whose 6th param is typed
        //   RigidBodyId; the value is sourced from mu32GameModeState (+26392) wrapped to match, per the asm.
        // ⚠️⚠️ FLAG SHARPENED 2026-08-11 (handle-widening wave) -- THE WRAP HAD TO CHANGE, AND
        // NOT CHANGING IT WOULD HAVE BEEN A SILENT DROP I INTRODUCED.
        // `RigidBodyId` is now the real 8-byte CgsPhysics::RigidBodyId. The old spelling
        // `RigidBodyId{ mu32GameModeState }` aggregate-initialised the 4-byte stand-in's ONLY
        // field, so the word landed where every consumer read it. Against the 8-byte type the same
        // brace-init widens the u32 into the LOW dword -- the half every consumer discards
        // (they all do `ld` then `srdi 32`; see ProcessAddDeformationModelEvents @0x82644940).
        // It would have compiled, linked and delivered zero. So the word is promoted into the HIGH
        // dword, which is what EVERY other producer of a RigidBodyId in this subsystem does
        // (ProcessCreateEvents `sldi r26,<word>,32`, ProcessCollisionEvents `extldi r,r,64,32`,
        // DetachedWheelManager's `v13 = (a3<<32)|1`).
        // ⛔ STILL PROVISIONAL, and now doubly so -- see the mu32GameModeState flag in
        // BrnDeformableObject.h: at the CONSOLE's real layout (mHandlingBodyID 8 bytes at +26384)
        // the offset +26392 this argument is sourced from is **mGlobalEntityId**, not a separate
        // game-mode word, and the seats the banner above quotes (+26388 / +26392) were read
        // against the narrow layout. This whole call's argument mapping is owed a re-read against
        // the corrected offsets. UNMOUNTED TU -- nothing shipped depends on it today.
        PhysicalBodyPart* lpPhysicalBodyPart = MakeDetachedPart(
            lpPartMgr, lpInput, mu16DeformableObjectIndex, this, mGlobalEntityId,
            CgsPhysics::RigidBodyId(static_cast<u64>(mu32GameModeState) << 32),
            liPartIndex, &lrPart);   // 6th arg = *(this+26392) (FLAG -- see above)

        if ( lpPhysicalBodyPart != nullptr )
        {
            // The new part must point back at THIS IK part, and the slot must still be ATTACHED_IK.
            CGS_ASSERT(lpPhysicalBodyPart->GetIKPartIndex() == liPartIndex,
                       "lpPhysicalBodyPart->GetIKPartIndex() == liPartIndex");
            CGS_ASSERT(maPartStates[liPartIndex] == KU_PART_STATE_ATTACHED_IK,
                       "maPartStates[liPartIndex] == E_PART_STATE_ATTACHED_IK");

            // Cache the new part's pool handle into the IK part (+12) and the pool-index array, then
            // mark the slot DETATCHED. (*(v16+25392)=*(v48+464); *(v49+26180)=4; the appended
            // mau8PhysicalBodyPartPoolIndex[mi16NumPhysicalParts++].)
            const u8 lu8PoolIndex = lpPhysicalBodyPart->GetPoolIndex();   // *(v48 + 464) low byte
            lrPart.SetPartPoolIndex(static_cast<s16>(lu8PoolIndex));      // *(v16 + 25392)
            maPartStates[liPartIndex] = KU_PART_STATE_DETATCHED;          // *(v49 + 26180) = 4
            mau8PhysicalBodyPartPoolIndex[mi16NumPhysicalParts++] = lu8PoolIndex;

            if ( lbHinge )
            {
                // Hinge path. The tag point to hang from (a6 == liJointIndex) must be in range.
                CGS_ASSERT(liJointIndex < lrPart.GetNumberOfTagPoints() && liJointIndex >= 0,
                           "liTagPointToHangFrom < lpPart->GetNumberOfTagPoints() && liTagPointToHangFrom >= 0");

                // The active joint index is the tag point's joint index.
                const s32 liActiveJoint = lrPart.GetTagPoint(liJointIndex)->GetJointIndex();   // v50/v51
                CGS_ASSERT(liActiveJoint < lrPart.GetNumberOfJoints() && liActiveJoint >= 0,
                           "liJointIndex < lpPart->GetNumberOfJoints() && liJointIndex >= 0");

                lrPart.SetActiveJointIndex(liActiveJoint);                // IKBodyPart::SetActiveJointIndex

                // Read the active joint spec (asm: GetAct twice, v69 = *(Act+48) seeds the joint
                // max-angle), then join the physical part to the vehicle at this tag point. The local
                // joint/COM positions are the dense VMX block above; the named stores live in
                // PhysicalBodyPart::SetJoinedToVehicle.
                const DeformationJointSpec* lpActiveJoint = lrPart.GetActiveJointSpec();   // GetAct
                (void)lpActiveJoint;
                lpPhysicalBodyPart->SetJoinedToVehicle(/*lLocalJointPosition*/ Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },
                                                       /*lLocalComPosition*/ Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },
                                                       /*liActiveJointsTagPointIndex*/ liJointIndex);

                maPartStates[liPartIndex] = KU_PART_STATE_HINGED;         // *(v49 + 26180) = 3
                ++mi16NumHingedParts;                                     // ++*(_R31 + 26288)
            }
            else
            {
                // Free-body path. Add the part to the sim seeded with the assembled world transform +
                // linear/angular velocity (the v77 blob). The transform/velocity build is the dense
                // VMX block above; modelled by the part's own seeded pose.
                lpPhysicalBodyPart->AddToSim(lpInput, lpPhysicalBodyPart->GetRigidBodyTransform(),
                                             lpPhysicalBodyPart->GetLinearVelocity(),
                                             lpPhysicalBodyPart->GetExternalBody()->GetAngularVelocity());

                // A structural part (type 84/85) coming off as a free body spends one structural-budget
                // slot. (asm: `v58 = type; if (84/85) --*(_R31 + 26400)`.)
                const s32 liType = static_cast<s32>(lrPart.GetPartType());   // *(*(v16+25388) + 476)
                if ( liType == KI_BODY_PART_STRUCTURAL_A || liType == KI_BODY_PART_STRUCTURAL_B )
                    --mi8NumPartsToForceHinging;                             // --*(_R31 + 26400) (FLAG)
            }

            // Assemble + emit the DetachedPartNotificationEvent onto the sim OutputBuffer's
            // notification queue (the asm builds the packed event from the part transform/velocity +
            // mHandlingBodyID and AddEventSafeAppend's it). Modelled through the FLAGGED emit hook.
            EmitDetachedPartNotification(lpOutput, &mHandlingBodyID);

            // Wheel-panel tail switch: invalidate the wheel tag-point lane(s) the detached panel
            // carried, so the wheel renderer stops following the now-detached tag point.
            switch ( static_cast<s32>(lrPart.GetPartType()) )           // *(*(v16+25388) + 476)
            {
                case KI_BODY_PART_WHEEL_0:
                    mu8WheelTagPointIndices[0] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
                    break;
                case KI_BODY_PART_WHEEL_1:
                    mu8WheelTagPointIndices[1] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
                    break;
                case KI_BODY_PART_WHEEL_2:
                    mu8WheelTagPointIndices[2] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
                    break;
                case KI_BODY_PART_WHEEL_3:
                    mu8WheelTagPointIndices[3] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
                    break;
                case KI_BODY_PART_WHEEL_23:   // 0x82: invalidate lanes 2 + 3 (asm falls into the lane-3 store)
                    mu8WheelTagPointIndices[2] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
                    mu8WheelTagPointIndices[3] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
                    break;
                case KI_BODY_PART_WHEEL_01:   // 0x83: invalidate lanes 0 + 1
                    mu8WheelTagPointIndices[0] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
                    mu8WheelTagPointIndices[1] = KU_INVALID_WHEEL_TAG_POINT_INDEX;
                    break;
                default:
                    break;
            }
        }
    }
}
}
