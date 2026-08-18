// GameSource/Physics/PropManager/PropManager_wQ_02.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, lander 02.
// Part-file of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp; the class
// banner, the member map and the tuning-global caveats live there and are NOT repeated here.
//
//     ProcessAddPartInstanceEvents()    @ 0x826280F8  (37 instructions, 0x826280F8..0x82628188)
//     ProcessRemovePartInstanceEvents() @ 0x82627818  (45 instructions, 0x82627818..0x826278C8)
//
// Both counts were COUNTED from each export's own newline-delimited `assembly` text and agree
// with (last - first)/4 + 1. Every statement below is one of those 82 instructions; nothing is
// added, removed or reordered across a `bl` barrier.
//
// ⚠️⚠️ PROVENANCE, STATED PLAINLY (2026-08-18, wave Q round 3): the round-1 copy of this file was
//    LOST FROM THE WORKING TREE during the round-3 fix pass (it was never committed, so there was
//    nothing to restore from). The two bodies below were RE-DERIVED from scratch against the raw
//    `assembly` arrays of .ida-exports/BURNOUT_X360_ARTIST.XEX/{0x826280F8,0x82627818}.json plus
//    the DecFIGS DWARF local sets -- they are a fresh reconstruction, not a recovered text, and
//    the round-1 verifier's three NITs (DWARF local names/types, the RigidBodyId gap note, the
//    instruction counts) are folded in at their final values rather than as corrections.
//
// ---- WHAT THE TWO BODIES ARE -------------------------------------------------------------------
// The two PART halves of the per-frame instance-event drain. ProcessInputsPreScene calls four of
// these in a fixed order (remove-prop, remove-part, add-prop, add-part); this file owns the two
// PART ones. Each walks one PropInputInterface queue front to back and forwards every event to
// the matching lifetime function (CreatePart / RemovePart). Neither body filters, early-outs
// mid-loop, or stores anything: they are pure drains.
//
// ---- CONSOLE VALUES ARE EVIDENCE ONLY (AGENTS.md gotcha 1) --------------------------------------
// The X360 emissions fold PropInputInterface's accessors away and land on raw offsets --
//   0x8262810C  addi r30, r4, 0xFB0    == &PropInputInterface::mAddPartQueue
//   0x82627824  addi r24, r4, 0x28CC   == &PropInputInterface::mRemovePartQueue
// and then `lwz r,8(queue)` == BaseEventQueue<T>::miLength == GetLength().
// Those two immediates are CONSOLE offsets and are meaningless on the LLP64 host (each queue
// element carries a Matrix44Affine and the queue headers differ), so they are recorded here as
// evidence ONLY. The source-level route is the DWARF's own accessor pair
// (BrnPropInputInterface.h:105 / :114, landed as GetAddPhysicalPartQueue() /
// GetRemovePhysicalPartQueue()); the drains hold a `const PropInputInterface*` and so bind the
// const overloads. The event-field offsets +0x40/+0x44/+0x46/+0x48 are likewise comments: every
// field is reached by member name (AddPhysicalPartEvent's host offsets happen to coincide with the
// console's, and RemovePhysicalPartEvent carries a static_assert(sizeof == 8) pinning the stride-8
// GetEvent I measured).
//
// ---- THE DWARF, WHICH SETTLES THE LOCALS -------------------------------------------------------
// references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp:1006-1030
// (ProcessAddPartInstanceEvents, source :1246) declares exactly five locals -- `lpQueue`,
// `lpEvent`, `uint32_t luQueueSize`, `uint32_t luEventIndex`, `Vector3 lVelocity` -- and its callee
// list is GetLength / GetEvent / Matrix44Affine::Matrix44Affine / Vector3::SetZero. The same file
// :698-726 (ProcessRemovePartInstanceEvents, source :711) declares `luEventIndex`, `luQueueSize`,
// `RigidBodyId lRigidBodyId`, `lpQueue`, `lpEvent`, `PropEntityID lEntityId`, `int32_t liPartIndex`,
// with callee list GetLength / GetEvent. The names and the u32 counter type below are the DWARF's,
// not an inference (both queues cap at 50 / 100, so the walk is identical either way -- but the
// spelling is free faithfulness). Two corroborations that the emission is read right: the single
// `Vector3 lVelocity` confirms ONE hoisted zero vector serving BOTH velocity arguments, and the
// `Matrix44Affine::Matrix44Affine` entry confirms the source really did materialise a by-value
// Matrix44Affine copy that the compiler elided into `r7 = the event pointer`.
//
// ---- GOTCHA SWEEP ------------------------------------------------------------------------------
// gotcha 2 (embedded sub-objects): no site -- neither body performs a single store.
// gotcha 3 (a float/vector arg skips its GPR slot): CreatePart's two Vector3s ride v1/v2 and
//   consume no GPR, which is exactly why r7 (&transform) is immediately followed by r8 (the slot).
//   The two vectors' POSITION in the parameter list is therefore NOT decidable from registers; it
//   comes from the committed CreatePart declaration in BrnPropManager.h, which this call matches
//   (⚠️ this citation used to read "BrnPropManager.h:766-774"; that line range is
//   AddContactResultsToQueue's, not CreatePart's -- the number had already drifted onto the wrong
//   declaration, which is why it is now a name)
//   positionally. No float parameters anywhere, so no FPR site.
// gotcha 4 (NaN polarity): no fcmpu / fsel / vcmp anywhere in either emission.
//
// ✅ THE TWO CALLEES THIS FILE FORWARDS TO ARE BODIED (re-grepped 2026-08-18; the round-1 banner
//    reported them as un-bodied and that went stale inside the wave):
//      PropManager::CreatePart @0x826278D0 and PropManager::RemovePart @0x8260F988 both have real
//      bodies in the sibling round-2 part-file
//      b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_04.cpp.
//    They are still in NO MOUNTED translation unit (tools/build/build_game_exe.bat lists no
//    PropManager_wQ* part-file at all), so this file gating green still does not mean the TU links
//    -- but the fix is to MOUNT wQ2_04 alongside this file, not to stub either name. A trap stub
//    beside a real body is an LNK2005 that `cl /c` cannot see.
//
// ⚠️ ODR: neither function has a second definition, a trap stub, or a one-shot gate anywhere
//    (grepped b5-decomp/src incl. GameSource/Physics/BrnPhysicsConductorGates.cpp and
//    GameSource/World/WorldLinkStubs.cpp). This file introduces no LNK2005 and retires no gate.
// ==========================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"

#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"          // AddPhysicalPartEvent / RemovePhysicalPartEvent
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"  // PropInputInterface + the queue accessors
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                    // BrnWorld::PropEntityID

namespace BrnPhysics
{
namespace Props
{

// =================================================================================================
// BrnPhysics::Props::PropManager::ProcessAddPartInstanceEvents @ 0x826280F8  (37 instructions)
// DWARF: class decl BrnPropManager.h:372; body scope BrnPropManager.cpp:1246.
//
// Drain the add-PART queue: every event becomes one CreatePart, with ZERO initial linear and
// angular velocities.
//
// ---- REGISTER MAP, read off the prologue --------------------------------------------------------
//   r3 = this (r28) - r4 = lpInput (r30 := r4 + 0xFB0, i.e. the queue, not the interface) -
//   r5 = lpSceneInput (r27) - r6 = lpSimModuleInputBuffer (r26) - r31 = luEventIndex
//
// ---- DECODE, in emission order ------------------------------------------------------------------
//   0x82628120  lwz r29, 8(r30)          luQueueSize = GetLength(), read ONCE, before the loop
//   0x82628124  cmplwi / beq             the whole loop is skipped when the queue is empty
//   0x8262812C  vspltisw128 v127, 0      the zero velocity, hoisted OUT of the loop and AFTER the
//                                        early-out (so an empty queue materialises nothing)
//   0x82628138  bl sub_825BC5D0          BaseEventQueue<AddPhysicalPartEvent>::GetEvent(i)
//   0x82628154  lhz 0x48 + extsh -> r8   miSlot,        SIGN-extended  -> liSlotIndex (s32)
//   0x82628158  lhz 0x44 + extsh -> r5   miPropTypeId,  SIGN-extended  -> luPropTypeIndex (u32)
//   0x82628160  lhz 0x46         -> r6   miPartId,      NOT extended   -> li16PartIndex (s16)
//   0x82628168  lwz 0x40         -> r4   mEntityId
//   0x8262816C  bl CreatePart            r3=this, r7=the event (== &mTransform, the Matrix44Affine
//                                        by hidden reference), v1/v2 = the zero velocity twice,
//                                        r9=lpSceneInput, r10=lpSimModuleInputBuffer
//   0x82628174  cmplw r31, r29 / blt     UNSIGNED compare against the hoisted length
//
// ⚠️ THE miPartId ASYMMETRY IS REAL AND IMMATERIAL: the caller zero-extends it (`lhz`, no `extsh`)
//    where it sign-extends the other two halfwords. It does not matter, and that is MEASURED, not
//    assumed: r15 (the parameter) is used inside CreatePart only by `stb r15, 0x38(r31)` and
//    `extsh r31, r15` -- the callee re-sign-extends it itself -- so the committed `s16
//    li16PartIndex` spelling is exactly right and no cast is needed at the call site.
//
// ---- CALLEE CENSUS (3 `bl` targets, one of them a save helper) ----------------------------------
//   __savegprlr_26                              -- compiler helper
//   sub_825BC5D0 (no per-address export)        -- BaseEventQueue<AddPhysicalPartEvent>::GetEvent.
//        Identified by elimination, not by name: its neighbours 0x825BC520 (stride 80, xref'd only
//        by ProcessAddPropInstanceEvents) and 0x825BC728 (stride 8, xref'd only by
//        ProcessRemovePartInstanceEvents) bracket it, it occupies exactly the 0xAC-byte gap
//        (43 instructions == the stride-80 GetEvent's own length), and it is in this function's
//        xrefs_from. The committed template's GetEvent carries the mpEvents/bounds asserts.
//   PropManager::CreatePart @0x826278D0         -- bodied in PropManager_wQ2_04.cpp
// =================================================================================================
void PropManager::ProcessAddPartInstanceEvents(
    const PropInputInterface*                                lpInput,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    const PropInputInterface::AddPhysicalPartEventQueue* lpQueue = &lpInput->GetAddPhysicalPartQueue();

    // 0x82628120 -- the bound is read ONCE, before the loop, and the loop is guarded by an
    // early-out on zero. Not re-read per iteration (unlike the sibling prop drains).
    const u32 luQueueSize = static_cast<u32>( lpQueue->GetLength() );

    // 0x8262812C `vspltisw128 v127, 0` -- ONE zero vector, hoisted out of the loop, feeding BOTH
    // velocity arguments of every CreatePart call (v1 and v2 are both `vmr128` copies of it).
    // The DWARF's single `Vector3 lVelocity` (:1252) is this. vspltisw zeroes all four lanes,
    // w included, which is exactly Vector3::SetZero's semantics.
    Vector3 lVelocity;
    lVelocity.SetZero();

    for ( u32 luEventIndex = 0; luEventIndex < luQueueSize; ++luEventIndex )
    {
        const AddPhysicalPartEvent* lpEvent = &lpQueue->GetEvent( static_cast<s32>( luEventIndex ) );

        // 0x8262816C -- r7 is the event pointer itself, which IS &lpEvent->mTransform (the event
        // starts with the affine); the by-value Matrix44Affine parameter is passed by hidden
        // reference, and the DWARF's `Matrix44Affine::Matrix44Affine` entry is that copy.
        CreatePart( lpEvent->mEntityId,
                    static_cast<u32>( lpEvent->miPropTypeId ),   // lhz 0x44 + extsh
                    lpEvent->miPartId,                           // lhz 0x46, no extsh -- see banner
                    lpEvent->mTransform,
                    lVelocity,
                    lVelocity,
                    lpEvent->miSlot,                             // lhz 0x48 + extsh
                    lpSceneInput,
                    lpSimModuleInputBuffer );
    }
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ProcessRemovePartInstanceEvents @ 0x82627818  (45 instructions)
// DWARF: class decl BrnPropManager.h:384; body scope BrnPropManager.cpp:711.
//
// Drain the remove-PART queue: every event becomes one RemovePart.
//
// ---- REGISTER MAP, read off the prologue --------------------------------------------------------
//   r3 = this (r27) - r4 = lpInput (r24 := r4 + 0x28CC, the queue) - r5 = lpSceneInput (r26) -
//   r6 = lpSimModuleInputBuffer (r25) - r31 = luEventIndex
//
// ---- DECODE, in emission order ------------------------------------------------------------------
//   0x82627838  lwz r28, 8(r24)          luQueueSize = GetLength(), read ONCE, before the loop
//   0x8262783C  cmplwi / beq             empty-queue early-out
//   0x82627844..0x82627850               BOTH assert strings hoisted out of the loop
//                                        ("liPartIndex != KI_PROP_INDEX_NOT_FOUND" and the baked
//                                        source path) -- evidence that the asserts below are one
//                                        message repeated, not two different ones
//   0x8262785C  bl sub_825BC728          BaseEventQueue<RemovePhysicalPartEvent>::GetEvent(i)
//                                        (dumped in full: the mpEvents != NULL / liIndex <
//                                        GetLength() / liIndex >= 0 asserts, then
//                                        `slwi r11,r29,3 ; add r3,r11,r10` == stride 8)
//   0x82627860  lwz r30, 4(r3)           miPhysicalIndex
//   0x82627864  lwz r29, 0(r3)           mEntityId
//   0x82627868  cmpwi cr6, r30, -1       SIGNED, against KI_PROP_INDEX_NOT_FOUND
//   0x8262786C  bne -> 0x826278A0        so the assert block runs when the index IS -1 ...
//   0x82627870..0x8262789C               ... and it is TWO complete Begin/Fire/End triples with
//                                        baked lines 0x2DF == 735 and 0x2E1 == 737 ...
//   0x826278A0  (fall through)           ... and it does NOT skip the call: RemovePart is called
//                                        either way. Reproduced exactly: two CGS_ASSERTs, then the
//                                        unconditional call.
//   0x826278B4  bl RemovePart            r3=this, r4=mEntityId, r5=miPhysicalIndex,
//                                        r6=lpSceneInput, r7=lpSimModuleInputBuffer
//   0x826278BC  cmplw r31, r28 / blt     UNSIGNED compare against the hoisted length
//
// ⚠️ THE 735 -> 737 GAP, and why NOTHING is synthesised for it: the two asserts bake consecutive
//    ODD source lines, so source line 736 emitted no code. The DWARF supplies the leading
//    candidate for what lived there -- this function declares a local `RigidBodyId lRigidBodyId`
//    (BrnPropManager.cpp:715) that has NO counterpart anywhere in the emission. That is not a
//    dropped side effect and it is checked, not assumed: the 45 instructions contain zero
//    RigidBodyId traffic (the only `bl` targets are GetEvent, the six assert entry points and
//    RemovePart), and the DWARF's own callee list for the function is just GetLength / GetEvent.
//    So the local is dead in this build -- most likely a compiled-out / debug-only statement --
//    and inventing a statement for it is exactly what a later sweep must NOT do.
//
// ---- CALLEE CENSUS (5 `bl` targets plus the save helper) ---------------------------------------
//   __savegprlr_22                                        -- compiler helper
//   sub_825BC728 = BaseEventQueue<RemovePhysicalPartEvent>::GetEvent  -- committed template
//   CgsDev::Assert::{Begin,Fire,End}Assert                -- committed
//   PropManager::RemovePart @0x8260F988                   -- bodied in PropManager_wQ2_04.cpp
// =================================================================================================
void PropManager::ProcessRemovePartInstanceEvents(
    const PropInputInterface*                                lpInput,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    const PropInputInterface::RemovePartEventQueue* lpQueue = &lpInput->GetRemovePhysicalPartQueue();

    // 0x82627838 -- read ONCE, before the loop, with the empty-queue early-out.
    const u32 luQueueSize = static_cast<u32>( lpQueue->GetLength() );

    for ( u32 luEventIndex = 0; luEventIndex < luQueueSize; ++luEventIndex )
    {
        const RemovePhysicalPartEvent* lpEvent =
            &lpQueue->GetEvent( static_cast<s32>( luEventIndex ) );

        const BrnWorld::PropEntityID lEntityId  = lpEvent->mEntityId;
        const s32                    liPartIndex = lpEvent->miPhysicalIndex;

        // 0x82627868 -- SIGNED compare. Both asserts carry the SAME message and the same baked
        // file; only the line differs (735 then 737). NON-GATING: the console falls straight
        // through into the call, so nothing is skipped when they fire.
        CGS_ASSERT( liPartIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPartIndex != KI_PROP_INDEX_NOT_FOUND" );   // :735
        CGS_ASSERT( liPartIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPartIndex != KI_PROP_INDEX_NOT_FOUND" );   // :737

        RemovePart( lEntityId,
                    static_cast<u32>( liPartIndex ),
                    lpSceneInput,
                    lpSimModuleInputBuffer );
    }
}

}
}
