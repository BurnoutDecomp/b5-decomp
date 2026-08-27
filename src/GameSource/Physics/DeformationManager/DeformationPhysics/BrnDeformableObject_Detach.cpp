#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"        // IKBodyPart::CheckForDetachment / GetTagPoint / GetNumberOf* / SetActiveJointIndex / GetActiveJointSpec
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"   // PhysicalBodyPart::GetIKPartIndex / SetJoinedToVehicle / AddToSim
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h" // DetachedPartManager::MakePartPhysical / TestJointForBreaking (called by name 2026-08-27)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnTagPoint.h"           // TagPoint::GetJointIndex
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                                       // CgsNumeric::Random
#include "GameShared/GameClasses/Core/CgsAssert.h"                                          // CGS_ASSERT
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                          // PhysicsModuleIO::OutputBuffer::GetDeformationOutputInterface (the notification emit)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"     // DeformationOutputInterface::mDetachedPartNotificationQueue
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"                 // DetachedPartNotificationEvent

#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags ([detach-probe])

#include <cmath>    // std::exp / std::log -- the vexptefp / vlogefp angular-decay refinement converges here
#include <cstdlib>  // getenv ([detach-probe] latch)

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
// ⭐⭐⭐ RE-HOMED 2026-08-27 (detach wave) -- ALL FOUR OF THIS FILE'S "LAYOUT-DERIVED" MEMBER
// IDENTIFICATIONS WERE WRONG, AND TWO OF THEM WERE KILL SWITCHES.
//
// The bool block was re-derived from the DecFIGS DWARF member ORDER (BrnDeformableObject.h:219-264)
// and anchored on four asm-proven offsets; the arithmetic closes EXACTLY, and it only closes if
// EGlassState is 4 bytes wide (it is: `enum EGlassState : s32`). That exactness is the proof:
//   +26396 mfNoDamageTimer  f32   [asm: Update `*(+26396) -= step`]
//   +26400 miNumAttachedExhausts  s16   [asm: the `lhz r11,0x6720(r31)` >1 test below]
//   +26402 mbActive · +26404 mCullGroup (4-aligned)
//   +26408 mbHasDeformedThisFrame · +26409 mbIKUpdateRequired  [asm: Update `*(+26409) |= *(+26408)`,
//          and Update RETURNS *(+26409)]
//   +26410 mbDoSweptSphereTests · +26411/12 bonnet · +26413 mbForceWheelsToDetach
//   +26414 mbShowtimeShunting · +26415 mbDontPlayGlassPaneEffects · +26416 mbResetDeformationNextUpdate
//   +26417 mi8NumPartsToForceHinging  s8 · +26420 maGlassPaneStates[10] (40 B, ends 26459)
//   +26460 meAbsorptionSet  u32
//
// | offset | this file USED to say  | TRUTH                    | what the error did                  |
// |--------|------------------------|--------------------------|-------------------------------------|
// | +26409 | mbForceWheelsToDetach  | mbIKUpdateRequired       | ⛔ KILL SWITCH. NOTHING in the tree  |
// |        |                        |                          | ever writes mbForceWheelsToDetach,   |
// |        |                        |                          | so arm A of CheckForDetachment was   |
// |        |                        |                          | permanently closed. mbIKUpdateRequired|
// |        |                        |                          | is set on every deforming frame -- and|
// |        |                        |                          | UpdateIKAndLocators ASSERTS it        |
// |        |                        |                          | immediately before calling here.      |
// | +26400 | mi8NumPartsToForceHinging | miNumAttachedExhausts | the "don't shed the last one" guard  |
// |        |                        |                          | read the wrong counter -- and aliased |
// |        |                        |                          | the +26417 budget onto the same byte. |
// | +26417 | KI_MAX_PHYSICAL_PARTS (20)| mi8NumPartsToForceHinging | ⛔ POLARITY INVERTED. `20 - hinged  |
// |        |                        |                          | > 0` is ~always TRUE, so once the     |
// |        |                        |                          | MakeDetachedPart forward went live    |
// |        |                        |                          | CheckForForcedDetachment would have   |
// |        |                        |                          | force-hinged a random panel EVERY     |
// |        |                        |                          | frame. The real budget is ~always 0.  |
// | +26460 | "all parts detached", derived | meAbsorptionSet != 4 | the outer gate means "not          |
// |        | from mi16NumPhysicalParts vs  | (E_ABSORPTIONSET_    | INVINCIBLE", not "not fully shed".  |
// |        | miNumIKBodyParts              |  INVINCIBLE)         | asm: `lwz r11,26460; cmpwi 4; beq`. |
//
// ⭐⭐ WHY THIS STAYED INVISIBLE: three different TUs of ONE class gave three different
// identifications of byte +26409 (here, _Lifecycle.cpp:791, and the correct one in _Update.cpp).
// No link and no gate can see that -- every spelling compiles, links and produces a plausible
// "nothing detached". [[shadowing-redeclarations]]-adjacent, but strictly worse.
//
// The remaining genuinely layout-derived (not byte-proven) identification in this file is
// IKBodyPartSpec +456, reached through GetMeshId() -- see DetachPart's own FLAG.
//
// FLAGGED-0 PLACEHOLDERS (rodata NOT in the per-function exports -- NEVER fabricated):
// ⭐ RECOVERED 2026-08-27: KF_ANGULAR_VELOCITY_DECAY (0.99), KF_ANGULAR_VELOCITY_FOR_DETACHMENT
// (8.0) and KB_ALLOW_RANDOM_PART_DETACHMENT (true) -- see the constant block below for the
// initialiser addresses and how the two float roles were pinned by USE ORDER rather than guessed.
// STILL FLAGGED here:
//   * the vexptefp/vlogefp Chebyshev polynomial coefficient rows (&unk_82014A?0) the asm uses to
//     refine 2^x / log2(x) for the decay -- carried as a documented exp()/log() model. That is a
//     precision difference on a converging series, not a behaviour difference.
// The decay/threshold SHAPE (decay the accumulator, compare its magnitude to the threshold, and on
// over-threshold pick + hinge a random still-attached part) is exact, and now so are its numbers.
//
// ⭐⭐⭐ 2026-08-27 (detach wave) -- THE TWO PROVISIONAL FREE HOOKS ARE GONE.
// The banner that used to stand here said "the detached-part manager + notification queue are NOT
// homed in-tree", and the two hooks below it returned nullptr / false unconditionally. BOTH real
// bodies were already bodied AND mounted the whole time:
//   * DetachedPartManager::MakePartPhysical @0x82626E30 -- BrnDetachedPartManager.cpp:133
//   * DetachedPartManager::TestJointForBreaking @0x8260E3C0 -- bodied 2026-08-27 in the same TU,
//     27 instructions, a pure forwarder into PhysicalBodyPart::TestJointForBreaking @0x8260C0F8
//     (also long since bodied + mounted). The old gate's banner claimed "@0x825E??? (PS3 0x761F2C,
//     401)" -- wrong address, wrong size. [[unnamed-sub-bodies-and-env-faults]].
// Both are now called BY NAME through lpPartMgr. The hooks' only reason to exist was an ODR type
// fork on the OutputBuffer seat (see the note at the top of BrnPhysicalBodyPart.h); with the PS3
// mangles applied the whole chain carries BrnPhysics::PhysicsModuleIO::OutputBuffer* and the
// forwards type-check directly.
// ⭐ EmitDetachedPartNotification IS NO LONGER A HOOK (2026-08-27, detach-2 wave) -- the hook and
// its declaration are DELETED, not bodied. Its signature was itself a fabrication: a `const void*
// lpEventBlob` standing in for a NAMED 32-byte record with three named fields
// (Deformation::DetachedPartNotificationEvent). Both console sites build that record inline and
// AddEventSafe it onto the deformation output interface's +0x3A0 queue -- the offset
// BrnDeformationOutputInterface.h:77 already committed -- and the two call sites now do the same.
// See the banner at this file's own emission site for the field-by-field asm cites.
// ⚠️ WHAT IT DOES *NOT* BUY, said plainly: NOTHING IN THIS TREE READS THAT QUEUE. Only
// Construct / Clear / Append touch it. The notification is a GAMEPLAY/AUDIO signal (its PS3 consumers
// are BrnSound::Logic::Collision::InputCollision / CollisionStateManager::ImportContactSpies), so
// this is the hook crash audio will hang off once BrnSound's side lands -- it is not audio. The
// VISIBLE detached part is driven by DetachedPartRenderEvent out of DetachedPartManager::
// OutputEvents, which is live and unaffected.

namespace BrnPhysics
{
namespace Deformation
{
    // MakeDetachedPart / TestJointForBreaking free hooks REMOVED 2026-08-27 -- both real bodies are
    // mounted and are now called by name on lpPartMgr (see the file header).
    // EmitDetachedPartNotification's free hook is GONE (2026-08-27) -- see the file header.

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

        // The two part-type ids the hinge logic guards a counter on (the asm's `type == 84 ||
        // type == 85`). ⭐ 2026-08-27: that counter is +26400 == miNumAttachedExhausts, and
        // _Lifecycle's seeding loop counts exactly these two types into it -- so 84/85 are the
        // EXHAUST part types, not "structural / bonnet-ish". The names are kept (they are load-
        // bearing at four call sites) but the semantics are: don't shed the last exhaust.
        const s32 KI_BODY_PART_STRUCTURAL_A = 84;   // 0x54  exhaust A
        const s32 KI_BODY_PART_STRUCTURAL_B = 85;   // 0x55  exhaust B

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
        // ⭐⭐ ALL THREE RECOVERED 2026-08-27 (detach wave), by the same initialiser-scan method that
        // recovered the joint multipliers -- and the master switch was not rodata at all, it is a
        // plain .data byte that reads straight out of the image:
        //   kbAllowRandomPartDetachment  @0x82F2A344  byte = 1        -> TRUE
        //   0x82C5D6D8  flt_820224B0 = 0.99  -> stvx 0x82FB9BA0       -> the DECAY
        //   0x82C5D6B0  flt_82004C88 = 8.0   -> stvx 0x82FB9AA0       -> the THRESHOLD
        // ROLES ARE PINNED BY USE ORDER, not guessed: 0x82FB9BA0 is materialised at 0x8263A7E0,
        // immediately after `lfs 60.0` from flt_82092BC4 -- i.e. pow(0.99, timeStep*60), a
        // per-frame decay normalised to 60 fps, which is also why the four vexptefp/vlogefp
        // Chebyshev rows at 0x82014AC0..0x82014AF0 are loaded right there. 0x82FB9AA0 is
        // materialised at 0x8263A900, at the vcmpgtfp against the spin magnitude.
        //
        // ⭐ AND THE OLD ZEROS WERE NOT INERT -- THEY WERE THE WRONG WAY ROUND. The threshold is
        // compared `spinSpeedSquared > threshold`, so a FLAGGED-0 threshold left this gate WIDE
        // OPEN for any non-zero spin whatsoever, with the master switch guessed (correctly) as
        // true. Landing 8.0 makes the gate STRICTLY TIGHTER than what shipped in this tree, and
        // landing the 0.99 decay shrinks the accumulator that feeds it instead of holding it flat.
        // Both changes reduce the chance of a spurious hinge; neither can create one.
        // [[placeholder-identity-element]] once more: 0 is the identity of `+`, not of `>`.
        const f32  KF_ANGULAR_VELOCITY_DECAY          = 0.99000001f;   // RECOVERED 0x82FB9BA0
        const f32  KF_ANGULAR_VELOCITY_FOR_DETACHMENT = 8.0f;          // RECOVERED 0x82FB9AA0
        const f32  KF_ANGULAR_DECAY_REFERENCE_RATE    = 60.0f;         // RECOVERED flt_82092BC4
        const bool KB_ALLOW_RANDOM_PART_DETACHMENT    = true;          // RECOVERED @0x82F2A344 == 1

        // SIMD magnitude-squared of a VecFloat's xyz lanes (vmsum3fp128 / the spin-speed^2 compared).
        inline f32 MagnitudeSquared3(const VecFloat& lvf)
        {
            return lvf.x * lvf.x + lvf.y * lvf.y + lvf.z * lvf.z;
        }

        // =========================================================================================
        // [detach-probe] -- NOT X360. Host-side witness for the 2026-08-27 detach wave, opt-in via
        // the SAME latch the deformation witness uses (BRN_DEFORM_TRACE; 0/unset == fully inert).
        //
        // ⭐ IT IS BUILT TO FALSIFY THE SUCCESS, not to announce it. Three lines, and the useful
        // one is the line that prints when NOTHING detaches:
        //   [detach-gate]  the three OUTER gate values + how many parts sat in each state + how
        //                  many passed arm A's own predicate. If parts never come off, this says
        //                  WHICH gate closed -- and a probe-gated 0 is distinguishable from a
        //                  gate that never ran, because the line prints either way.
        //   [detach-make]  printed at the MakePartPhysical call site, for BOTH outcomes. A null
        //                  return (pool full / no free slot) is a different failure from "the
        //                  call site was never reached", and only printing successes would hide it.
        //   [detach-part]  printed only on a real promotion, carrying the counter the win
        //                  condition is stated in (mi16NumPhysicalParts, before -> after).
        // DELETE-WHEN the detach question is closed and banked.
        // =========================================================================================
        inline bool DetachProbeOn()
        {
            static s32 siProbe = -1;
            if (siProbe < 0)
            {
                const char* lpcEnv = getenv("BRN_DEFORM_TRACE");
                siProbe = (lpcEnv != 0 && atoi(lpcEnv) > 0) ? 1 : 0;
            }
            return (siProbe == 1) && (CgsDev::Log::gpDebugPrint != 0);
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
                                              BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
                                              DetachedPartManager* lpPartMgr, f32 lfTimeStep)
    {
        // Outer gate, re-read from the asm 2026-08-27 (0x8263ABE4..0x8263AC08), store for store:
        //   lhz  r11, 26286(r31) ; extsh ; cmpwi 20 ; bge  -> return   (mi16NumPhysicalParts < 20)
        //   lwz  r11, 26460(r31) ;         cmpwi  4 ; beq  -> return   (meAbsorptionSet != INVINCIBLE)
        //   lwz  r11, 26232(r31) ;         cmpwi  0 ; ble  -> return   (miNumIKBodyParts > 0)
        // The middle test is a 4-BYTE load of +26460 == meAbsorptionSet, NOT the "is every part
        // already shed" predicate this file used to synthesise. An INVINCIBLE car sheds nothing.
        // [detach-probe] gate witness -- see the block at the top of this file.
        s32 liProbeAttached = 0, liProbeHinged = 0, liProbeArmAHits = 0, liProbeJointBreaks = 0;
        const bool lbProbe = DetachProbeOn();

        if ( mi16NumPhysicalParts < KI_MAX_PHYSICAL_PARTS
             && meAbsorptionSet != E_ABSORPTIONSET_INVINCIBLE   // +26460 (asm-proven)
             && miNumIKBodyParts > 0 )
        {
            const s32 liNumParts = miNumIKBodyParts;       // _R31[6558] bound
            for ( s32 li = 0; li < liNumParts; ++li )      // v8 = &maPartStates[li], v9 = &maIKParts[li]
            {
                const u8 lu8State = maPartStates[li];      // v12 = *v8

                // asm 0x8263AC3C: `lbz r10, 26409(r31) ; cmplwi 0 ; beq -> next part`. +26409 is a
                // BYTE and it is mbIKUpdateRequired -- the flag UpdateIKAndLocators asserts is set
                // immediately before calling this function, i.e. the gate is OPEN on every
                // deforming frame. (It used to read mbForceWheelsToDetach, which no writer exists
                // for anywhere in the tree -- see the file-header table.)
                if ( lu8State == KU_PART_STATE_ATTACHED_IK && mbIKUpdateRequired )   // +26409 (asm-proven)
                {
                    // IKBodyPart::CheckForDetachment(part, impulse, &detachJointOut). The asm passes
                    // a2 (the accumulated-impulse arg) + a 16-byte scratch out (v17).
                    s32 liDetachJointOut = 0;                          // v17[] scratch
                    const f32 lfImpulse = lfTimeStep;                  // a2 (the impulse arg; see note below)
                    if ( lbProbe ) { ++liProbeAttached; }
                    if ( maIKParts[li].CheckForDetachment(lfImpulse, liDetachJointOut) )
                    {
                        if ( lbProbe ) { ++liProbeArmAHits; }
                        // Promote the panel to a free PhysicalBodyPart (no hinge -> lbHinge == false).
                        DetachPart(lpInput, lpOutput, lpPartMgr, li, /*liJointIndex*/ 0, /*lbHinge*/ false);
                    }
                }
                else if ( lu8State == KU_PART_STATE_HINGED )
                {
                    if ( lbProbe ) { ++liProbeHinged; }
                    // Structural-part predicate: type == 84 || type == 85 (the asm's v13/v14 latch).
                    const s32 liType = static_cast<s32>(maIKParts[li].GetPartType());   // *(*v9 + 476)
                    const bool lbStructural =
                        ( liType == KI_BODY_PART_STRUCTURAL_A || liType == KI_BODY_PART_STRUCTURAL_B );

                    // Run the joint-break test unless this is an exhaust and it is the last one.
                    // asm 0x8263ACC8: `lhz r11, 0x6720(r31) ; extsh ; cmpwi 1 ; ble -> next part`,
                    // reached only when the type-84/85 latch is set. +26400 is a 16-BIT load and it
                    // is miNumAttachedExhausts -- which is exactly what part types 84/85 are, as
                    // _Lifecycle's seeding loop (count attached type-84/85 parts) confirms.
                    if ( !lbStructural || miNumAttachedExhausts > 1 )   // +26400 (asm-proven, lhz)
                    {
                        // asm 0x8263ACD8: `lhz r11, 4(r28)` (r28 == &maIKParts[li].mpSpec, so +4 is
                        // the part's pool index) ; `extsh r4, r11` ; r3=partMgr, r5=lpInput,
                        // r6=lpOutput ; `bl 0x8260E3C0` == DetachedPartManager::TestJointForBreaking.
                        const s32 liJointHandle = maIKParts[li].GetPartPoolIndex();   // v9[2]
                        if ( lpPartMgr->TestJointForBreaking(liJointHandle, lpInput, lpOutput) )
                        {
                            if ( lbProbe ) { ++liProbeJointBreaks; }
                            // The joint broke. There must be a hinged part to consume.
                            CGS_ASSERT(mi16NumHingedParts > 0, "mi16NumHingedParts > 0");

                            // vspltisw v0,0 / stvx128 v0,r31,3904 -> zero the spin accumulator (+3904).
                            mAngularVelocitySum.SetZero();

                            maPartStates[li] = KU_PART_STATE_DETATCHED;   // *v8 = 4
                            --mi16NumHingedParts;                         // --*(_R31 + 26288) (lhz/sth)
                            if ( lbStructural )                           // if ( v16 )
                                --miNumAttachedExhausts;                  // --*(_R31 + 26400) (lhz/sth)
                        }
                    }
                }
            }
        }

        // ⭐ 2026-08-27: the "impulse vs time-step" ambiguity in the note below is now MOOT and the
        // FLAG is retired. The asm at 0x826422DC..0x826422E8 is `lfs f31, 240(r1)` (the x lane of the
        // spilled lvfTimeStep) then `fmr f1, f31` -- the float arg IS lvfTimeStep.x. And
        // IKBodyPart::CheckForDetachment IGNORES it (`(void)lfImpulse;` -- the impulse it actually
        // bands against is folded from the tag points' own sensors). So passing the time-step here is
        // both faithful and observationally inert; it was NOT a fifth kill switch.
        (void)lfTimeStep;

        // [detach-probe] gate witness. Prints EVERY call it is enabled for, including the calls where
        // nothing happens -- that is the whole point.
        if ( lbProbe )
        {
            static u32 sluProbeCalls = 0;
            static s32 siLastPhysical = -1;
            ++sluProbeCalls;
            const bool lbInteresting = ( liProbeArmAHits > 0 || liProbeJointBreaks > 0
                                      || static_cast<s32>(mi16NumPhysicalParts) != siLastPhysical );
            if ( lbInteresting || (sluProbeCalls % 600u) == 0u )
            {
                siLastPhysical = static_cast<s32>(mi16NumPhysicalParts);
                *CgsDev::Log::gpDebugPrint
                    << "[detach-gate] call " << static_cast<s32>(sluProbeCalls)
                    << " ent " << static_cast<s32>(mGlobalEntityId.muValue)
                    << " nPhys " << static_cast<s32>(mi16NumPhysicalParts)
                    << " nHinged " << static_cast<s32>(mi16NumHingedParts)
                    << " absorb " << static_cast<s32>(meAbsorptionSet)
                    << " ikReq " << (mbIKUpdateRequired ? 1 : 0)
                    << " nIK " << miNumIKBodyParts
                    << " exhausts " << static_cast<s32>(miNumAttachedExhausts)
                    << " forceHinge " << static_cast<s32>(mi8NumPartsToForceHinging)
                    << " attachedSeen " << liProbeAttached
                    << " hingedSeen " << liProbeHinged
                    << " armAHits " << liProbeArmAHits
                    << " jointBreaks " << liProbeJointBreaks
                    << "\n";
            }
        }
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
                                                    BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
                                                    DetachedPartManager* lpPartMgr,
                                                    CgsNumeric::Random* lpRandom, f32 lfTimeStep)
    {
        // asm: *(result+26286)<20 && *(result+26417)-*(result+26288)>0.
        // ⛔⛔ POLARITY FIX 2026-08-27: +26417 is mi8NumPartsToForceHinging, NOT the 20-slot cap.
        // The old spelling `20 - mi16NumHingedParts > 0` is TRUE for any car with fewer than 20
        // hinged parts -- i.e. essentially always. This function force-hinges a RANDOM still-attached
        // panel every time it passes, so with the constant substituted AND the MakeDetachedPart
        // forward wired (this wave) the car would have shed a panel per frame until it ran out.
        // The real budget is a per-object latch seeded to 0 by DeformableObject::Reset and armed only
        // on the PLAYER_EXTREME (showtime) path -- so the honest gate is ~always closed.
        if ( mi16NumPhysicalParts < KI_MAX_PHYSICAL_PARTS
             && ( mi8NumPartsToForceHinging - mi16NumHingedParts ) > 0 )   // +26417 - +26288
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
    //   1) Decay the spin accumulator (+3904) by pow(0.99, timeStep * 60) -- the vexptefp/vlogefp
    //      polynomial the asm builds, with the 60.0 reference rate loaded in the same block.
    //   2) Compare the decayed spin magnitude-squared against KF_ANGULAR_VELOCITY_FOR_DETACHMENT
    //      (vcmpgtfp). Below threshold: return.
    //   3) Over threshold: draw a random attached jointed part, assert it is ATTACHED_IK + joint-count
    //      > 0, draw a random joint, locate its tag point, and DetachPart it AS A HINGE (lbHinge ==
    //      true). (The "Hinging part due to spinning:" trace is a debug-only log; omitted.)
    // (lpInput==a2, lpOutput==a3, lpPartMgr==a4, lrRandom==a5; this==_R29.)
    // =============================================================================================
    void DeformableObject::UpdateSpinningDetachment(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                                    BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
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
        // folds it into the accumulator, then multiplies by pow(0.99, timeStep * 60) built via the
        // exp/log polynomial.
        const f32 lfTimeStep = lvfTimeStep.x;
        const Vector3 lBodyOmega = GetVehicleBody().GetAngularVelocity();   // lvx body+96
        mAngularVelocitySum.x += lBodyOmega.x;
        mAngularVelocitySum.y += lBodyOmega.y;
        mAngularVelocitySum.z += lBodyOmega.z;

        // pow(0.99, timeStep * 60) -- the asm's exp/log polynomial with the 60.0 reference rate the
        // same block loads (flt_82092BC4 @0x8263A7CC). At a 60 fps step this is exactly 0.99 per
        // frame; at any other step it is the frame-rate-independent equivalent, which matters on a
        // host that does not hold 60.
        const f32 lfDecayFactor =
            std::exp( std::log( KF_ANGULAR_VELOCITY_DECAY ) * ( lfTimeStep * KF_ANGULAR_DECAY_REFERENCE_RATE ) );
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
                                      BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
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
            // Exhaust-count shortcut: more than one attached exhaust always proceeds
            // (asm: `if (*(_R31+26400) > 1) goto LABEL_11`). +26400 == miNumAttachedExhausts.
            if ( miNumAttachedExhausts > 1 )                          // *(_R31 + 26400) > 1
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

        // =====================================================================================
        // DetachedPartManager::MakePartPhysical @0x82626E30 -- CALLED BY NAME 2026-08-27.
        //
        // ⭐⭐ THE ARGUMENT MAPPING IS NOW ASM-PROVEN, and it retires the "doubly provisional"
        // mu32GameModeState invention that used to stand here. The GPR arg setup immediately before
        // the call at 0x826309B4 reads, register for register:
        //     r3  = a4                     -> lpPartMgr                (the `this`)
        //     r4  = r23 = a2               -> lpSimInput
        //     r5  = lhz  26290(r31)        -> mu16DeformableObjectIndex
        //     r6  = r31                    -> this (the DeformableObject)
        //     r7  = ld   26384(r31)        -> mHandlingBodyID   ⭐ AN 8-BYTE `ld`, not a synthesised
        //                                     high-dword wrap of a 4-byte word
        //     r8  = lwz  26392(r31)        -> mGlobalEntityId   ⭐ the 4-byte load, and at the
        //                                     CORRECTED layout +26392 IS mGlobalEntityId
        //     r9  = r24 = a5               -> liPartIndex
        //     r10 = r28                    -> &maIKParts[liPartIndex]
        // That is exactly the frozen header's (lpSimInput, lu16DeformableObjectIndex,
        // lpDeformableObject, lHandlingBodyId, lGlobalCarId, liPartIndex, lpPart) -- and exactly
        // what the PS3 mangle at 0x756A60 spells. The prior banner read those two seats against the
        // NARROW (4-byte mHandlingBodyID) layout, which is how a "game-mode state word" got invented
        // for a parameter the console fills with the handling body id.
        //
        // ⚠️ FLAGGED, and this is the honest edge of this wave: the FOUR by-value VMX arguments
        // (lLocalRenderTransform, lVehicleTransform, lInitialLinearVelocity, lInitialAngularVelocity)
        // travel in the vector bank / stack spill slots r1+144..r1+256, assembled by the dense
        // vsubfp/vpermwi/vnmsubfp/vaddfp block at 0x826308C0..0x826309B0 that is NOT decoded. They are
        // sourced here from the named state the console demonstrably reads in that block -- the asm
        // does `lwz r30, 6476(r31)` (the attached vehicle body) and then loads matrix rows at
        // `r30+16 {+0,+16,+32,+48}`, i.e. the body's own transform -- but the per-lane composition
        // (worldTransform = vehicleTransform o partLocalGraphicsFrame; linearVel = bodyLinearVel +
        // bodyAngularVel x (partCom - bodyCom)) is NOT reproduced. The detached part therefore
        // inherits the CAR's velocity rather than the car's velocity plus its own spin arm.
        // MakePartPhysical's two orthonormal tripwires are satisfied by these sources (both are real
        // orthonormal bases), which is the check that keeps them from being nonsense.
        // ⚠️⚠️ arg 9 (lVehicleTransform) IS A LOCAL BASIS, NOT A WORLD ONE -- and getting that wrong
        // was MEASURED, not reasoned. PhysicalBodyPart::Prepare adopts this argument as
        // mBBoxOrientation, and CalcBoundingBox then transforms the box centre through
        // mBBoxOrientation's rows AND ADDS ITS TRANSLATION ROW before storing it into
        // mLocalInitialComPositionPlusMaxJointAngle. GetEventRenderTransform subtracts that member
        // from the local graphics position. So handing the VEHICLE'S WORLD transform here injects
        // the car's world position into a LOCAL offset: the first attempt put every shed panel
        // ~3000 units from the car (measured: body (3008.2, -1.4, -1865.4) -> event (8.8, 36.4,
        // 15.8), and 3008 - 3000 is exactly where that 8.8 comes from).
        // The console builds mBBoxOrientation from the IK spec's bbox-skin orientation
        // (*(mpIKPart->GetSpec()+8)+64), which is NOT recovered -- BBoxPointSkinData::
        // HackSwapHandedness is an honest VMX stub and CalculateSkinnedPoint is declare-only.
        // Identity is the honest stand-in: it is orthonormal and right-handed (so both of
        // MakePartPhysical's tripwires and Prepare's handedness tripwire pass on their own merits,
        // not by suppression) and it keeps the box centre LOCAL, which is the property the
        // arithmetic actually depends on. FLAG it, do not read the box as real.
        Matrix44Affine lLocalBBoxOrientation;
        lLocalBBoxOrientation.SetIdentity();

        PhysicalBodyPart* lpPhysicalBodyPart = lpPartMgr->MakePartPhysical(
            lpInput,
            mu16DeformableObjectIndex,          // r5  = lhz 26290
            this,                               // r6
            mHandlingBodyID,                    // r7  = ld  26384  (8 bytes)
            mGlobalEntityId,                    // r8  = lwz 26392
            liPartIndex,                        // r9
            &lrPart,                            // r10
            lpSpec->GetPartGraphicsTransform(), // the part's LOCAL render frame. VERIFIED against
                                               // the renderer 2026-08-27: its .Pos() is identical
                                               // to lpCarGraphicsSpec->GetPartLocators()[mesh]'s
                                               // translation for the same part (e.g. mesh 10 ->
                                               // (-0.000000, 0.664476, 2.148275) on both sides).
                                               // (IKBodyPart's own wrapper is declare-only; it
                                               //  forwards to exactly this spec accessor.)
            lLocalBBoxOrientation,             // FLAG -- see the note above this call
            GetVehicleBody().GetLinearVelocity(),    // FLAG: no  w x r term
            GetVehicleBody().GetAngularVelocity());  // FLAG: body spin, not the part's

        // [detach-probe] BOTH outcomes -- a null return (pool full) is a different failure from
        // "the call site was never reached", and printing only the successes would hide it.
        if ( DetachProbeOn() )
        {
            *CgsDev::Log::gpDebugPrint
                << "[detach-make] ent " << static_cast<s32>(mGlobalEntityId.muValue)
                << " part " << liPartIndex
                << " type " << static_cast<s32>(lrPart.GetPartType())
                << " hinge " << (lbHinge ? 1 : 0)
                << " result " << ((lpPhysicalBodyPart != 0) ? "PART" : "NULL")
                << " nPhysBefore " << static_cast<s32>(li16NumPhysical)
                << " meshId " << lrPart.GetMeshId()
                << " vehPos (" << GetVehicleBody().GetTransform().wAxis.x
                << ", " << GetVehicleBody().GetTransform().wAxis.y
                << ", " << GetVehicleBody().GetTransform().wAxis.z << ")"
                << " specGfxPos (" << lpSpec->GetPartGraphicsTransform().wAxis.x
                << ", " << lpSpec->GetPartGraphicsTransform().wAxis.y
                << ", " << lpSpec->GetPartGraphicsTransform().wAxis.z << ")"
                << "\n";
        }

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
                // linear/angular velocity (the v77 blob).
                // ⛔ FIXED 2026-08-27: this used to hand AddToSim the PART'S OWN transform and
                // velocities -- which are the state AddToSim exists to SET. Prepare never poses the
                // embedded body, so the arguments were identity/zero and every shed panel would have
                // been posed at the world origin. The seed is the VEHICLE's world pose and motion,
                // which is what the asm's own documented composition starts from
                // (worldTransform = vehicleTransform o partLocalGraphicsFrame; the part-local half is
                // already carried by mLocalGraphicsPositionPlusJointVelocity and is re-applied by
                // GetEventRenderTransform, so seeding the vehicle transform puts the panel exactly
                // where it was drawn the frame before it came off -- continuity at the detach instant).
                // ⚠️ FLAG: the linear seed is the body velocity WITHOUT the asm's
                // `+ bodyAngularVel x (partCom - bodyCom)` arm; that cross term is in the undecoded
                // VMX block. It biases the seed, it does not fabricate one.
                lpPhysicalBodyPart->AddToSim(lpInput, GetVehicleBody().GetTransform(),
                                             GetVehicleBody().GetLinearVelocity(),
                                             GetVehicleBody().GetAngularVelocity());

                // An exhaust (type 84/85) coming off as a free body decrements the attached-exhaust
                // count. (asm: `v58 = type; if (84/85) --*(_R31 + 26400)`.)
                const s32 liType = static_cast<s32>(lrPart.GetPartType());   // *(*(v16+25388) + 476)
                if ( liType == KI_BODY_PART_STRUCTURAL_A || liType == KI_BODY_PART_STRUCTURAL_B )
                    --miNumAttachedExhausts;                                 // --*(_R31 + 26400)
            }

            // Assemble + emit the DetachedPartNotificationEvent. LANDED 2026-08-27 (detach-2 wave);
            // the `EmitDetachedPartNotification(buffer, const void* blob)` hook is deleted -- its
            // second parameter was an untyped blob standing in for a named 32-byte record.
            // The asm at 0x82630BA0..0x82630C30:
            //     ld 0x6710(this) ; srdi 32 -> event+0x10   mVehicleId == mHandlingBodyID's entity id
            //     lwz 0x632C(ikPart) ; lwz 0x1DC -> +0x14   meType == the IK spec's part type
            //     three vmaddfp of v126's splatted lanes through the four rows at var_100/F0/E0/D0
            //       -> event+0x00   mPointOnA == transform(lVehicleTransform, v126)
            // var_100 is the SAME stack matrix handed to AddToSim as its lVehicleTransform argument
            // (`addi r5, r1, var_100` @0x82630B54), so the transform is the vehicle's world pose and
            // v126 is a part-LOCAL point.
            // ⚠️ THE LOCAL POINT INHERITS AN EXISTING PLACEHOLDER, it does not introduce a new one.
            // v126 is the same register this body already hands SetJoinedToVehicle as its
            // lLocalComPosition on the hinge arm above -- and that call passes Vector3{0,0,0} today,
            // flagged there. So mPointOnA resolves to the vehicle transform's translation until that
            // one value is recovered; the transform SHAPE is the asm's. Retire both together.
            // ⚠️ AND NOTHING READS THIS QUEUE YET (Construct/Clear/Append only). Its PS3 consumers
            // are BrnSound; this is the hook crash audio will hang off, not audio itself.
            {
                Deformation::DetachedPartNotificationEvent lNotification;

                const Matrix44Affine lVehicleTransform = GetVehicleBody().GetTransform();
                const Vector3 lLocalComPosition = { 0.0f, 0.0f, 0.0f, 0.0f };   // FLAG: see above
                lNotification.mPointOnA.x = lVehicleTransform.xAxis.x * lLocalComPosition.x
                                          + lVehicleTransform.yAxis.x * lLocalComPosition.y
                                          + lVehicleTransform.zAxis.x * lLocalComPosition.z
                                          + lVehicleTransform.wAxis.x;
                lNotification.mPointOnA.y = lVehicleTransform.xAxis.y * lLocalComPosition.x
                                          + lVehicleTransform.yAxis.y * lLocalComPosition.y
                                          + lVehicleTransform.zAxis.y * lLocalComPosition.z
                                          + lVehicleTransform.wAxis.y;
                lNotification.mPointOnA.z = lVehicleTransform.xAxis.z * lLocalComPosition.x
                                          + lVehicleTransform.yAxis.z * lLocalComPosition.y
                                          + lVehicleTransform.zAxis.z * lLocalComPosition.z
                                          + lVehicleTransform.wAxis.z;
                lNotification.mPointOnA.w = 0.0f;

                // The asm's `ld 0x6710(this) ; srdi r8, r8, 32` -- the handling body id's HIGH dword.
                // Two DIFFERENT EntityId types meet here and neither is wrong: RigidBodyId::
                // GetEntityId() hands back a CgsSceneManager::EntityId (the packed scene handle with
                // its owner/index/part accessors), while this event field is the plain 32-bit
                // ::EntityId of BrnCommonTypes.h. They are the same four bytes -- the whole point of
                // the "EntityId GENUINELY IS 32 BITS" note in BrnCommonTypes.h -- so the word is
                // carried across explicitly rather than by a conversion that does not exist.
                lNotification.mVehicleId.muValue =
                    static_cast<u32>(mHandlingBodyID.GetEntityId());
                lNotification.meType     = static_cast<EBodyParts>(lrPart.GetPartType());

                lpOutput->GetDeformationOutputInterface()
                        ->mDetachedPartNotificationQueue.AddEventSafe(lNotification);

                // [detach-notify] NOT X360. The queue LENGTH after the add -- the only thing that can
                // tell "the event was appended" from "the emit ran and the queue is still empty",
                // which is exactly the distinction the deleted hook could never make. Latched on
                // BRN_DEFORM_TRACE. DELETE-WHEN a consumer exists and can be measured instead.
                if ( DetachProbeOn() )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[detach-notify] appended DetachedPartNotificationEvent type "
                        << static_cast<s32>(lNotification.meType)
                        << " vehicle " << static_cast<s32>(lNotification.mVehicleId.muValue)
                        << " queueLen "
                        << lpOutput->GetDeformationOutputInterface()
                                   ->mDetachedPartNotificationQueue.GetLength()
                        << "\n";
                }
            }

            // [detach-probe] the win-condition counter, at the site, after the increment.
            if ( DetachProbeOn() )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[detach-part] PART CAME OFF ent " << static_cast<s32>(mGlobalEntityId.muValue)
                    << " ikPart " << liPartIndex
                    << " type " << static_cast<s32>(lrPart.GetPartType())
                    << " hinge " << (lbHinge ? 1 : 0)
                    << " poolIndex " << static_cast<s32>(lu8PoolIndex)
                    << " nPhys " << static_cast<s32>(li16NumPhysical)
                    << " -> " << static_cast<s32>(mi16NumPhysicalParts)
                    << " nHinged " << static_cast<s32>(mi16NumHingedParts)
                    << " bodyPos (" << lpPhysicalBodyPart->GetRigidBodyTransform().wAxis.x
                    << ", " << lpPhysicalBodyPart->GetRigidBodyTransform().wAxis.y
                    << ", " << lpPhysicalBodyPart->GetRigidBodyTransform().wAxis.z << ")"
                    << " evtPos (" << lpPhysicalBodyPart->GetEventRenderTransform().wAxis.x
                    << ", " << lpPhysicalBodyPart->GetEventRenderTransform().wAxis.y
                    << ", " << lpPhysicalBodyPart->GetEventRenderTransform().wAxis.z << ")"
                    << "\n";
            }

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

    // =============================================================================================
    // ⛔ THE TWO LOG-ONCE GATES THAT USED TO END THIS FILE ARE DELETED (2026-08-27, detach wave).
    // They were `return nullptr;` and `return false;` -- kill switches 3 and 4 of four. Both real
    // bodies were mounted the whole time and are now called by name on lpPartMgr:
    //   MakeDetachedPart      -> DetachedPartManager::MakePartPhysical    @0x82626E30 (DetachPart)
    //   TestJointForBreaking  -> DetachedPartManager::TestJointForBreaking @0x8260E3C0
    //                            (CheckForDetachment; bodied 2026-08-27, 27 insns)
    // ⭐ [[invented-arms-and-the-c4715-ratchet]] -- fixed by DELETION, not by re-predication.
    // Their two runtime log strings are gone with them; nothing else in the tree printed them.
    // =============================================================================================

}
}
