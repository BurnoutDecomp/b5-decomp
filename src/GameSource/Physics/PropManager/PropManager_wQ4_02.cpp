// GameSource/Physics/PropManager/PropManager_wQ4_02.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props / smash-gates wave Q4 (SEAM WAVE),
// partfile 02. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   ProcessRemovePropInstanceEvents() @ 0x82627778   (39 instructions,
//                                                     0x82627778..0x82627810 --
//                                                     COUNTED: (0x82627810-0x82627778)/4 + 1 == 39)
//
// A slice of the TU's own BrnPropManager.cpp, split out per this wave's partfile convention.
//
// ==========================================================================================
// WHY IT IS HERE, AND WHY IT IS THIS FUNCTION AND NOT ANOTHER
//
// Wave Q4's second job is the MOUNT CLOSURE for the PropManager partfiles. Of the fourteen
// PropManager symbols the mount leaves with no body anywhere in the tree, this was the ONE that
// was neither parked-behind-a-foreign-header nor large: 39 instructions, and every callee it has
// is already real. It is the last of ProcessInputsPreScene's SIX drains to have no body
// (ProcessRemove/AddPartInstanceEvents are in PropManager_wQ_02.cpp, ProcessAddPropInstanceEvents
// in PropManager_wQ2_06.cpp, UpdateJointedProps in PropManager_wQ2_05.cpp), so landing it turns
// ProcessInputsPreScene from five-sixths real into five-sixths-plus-one, leaving exactly one --
// RemoveAllPropsAndParts @0x8260F010, 331 instructions -- for a gate. That count matters to the
// conductor's decision, so it is measured, not guessed: see the owner record.
//
// ⭐ THE PER-ADDRESS JSON FOR THIS FUNCTION DOES NOT EXIST
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82627778.json is absent) -- AGENTS.md gotcha 6, "a
// missing JSON is often only an export-run gap". It was: headless IDA 9.3 over a PRIVATE COPY of
// the .i64 (scratchpad/waveQ4/ida/wq4.i64, never the original) resolves 0x82627778 to a real
// function, fully named `BrnPhysics::Props::PropManager::ProcessRemovePropInstanceEvents`, with
// the 39-instruction listing this file is reconstructed from
// (scratchpad/waveQ4/ida/asm_82627778.txt). The identity ledger has no entry for this name at
// all, which is why it never reached a reconstruction wave.
//
// ==========================================================================================
// THE 39 INSTRUCTIONS. Register map from the prologue (0x82627784..0x82627794):
//   r3  -> r27 = this
//   r4  -> r24 = &lpInput->mRemovePropQueue     (`addi r24, r4, 0x1F60`, see CONSOLE OFFSET below)
//   r5  -> r26 = lpSceneInput
//   r6  -> r25 = lpSimModuleInputBuffer
//   r31 = luEventIndex (li r31,0)
//   r28 = luQueueSize  (`lwz r28, 8(r24)` == BaseEventQueue<T>::miLength == GetLength(),
//                       READ ONCE BEFORE THE LOOP, with the empty-queue early-out at 0x826277A0)
//
// Body, in emission order:
//   0x826277B4..0x826277BC  lpEvent = lpQueue->GetEvent(luEventIndex)   (the `bl` symbol is
//                           TRUNCATED in the export to `BrnPhysics__Pr` -- gotcha 6 again; the
//                           DWARF names the callee exactly:
//                           CgsModule::BaseEventQueue<RemovePhysicalPropEvent>::GetEvent)
//   0x826277C0              liPropIndex = lpEvent->miPhysicalIndex      (`lwz r30, 4(r3)`)
//   0x826277C4              lEntityId   = lpEvent->mEntityId            (`lwz r29, 0(r3)`)
//   0x826277C8..0x826277E4  ONE assert, SIGNED compare against -1, baked line 0x2B1 == 689,
//                           message "liPropIndex != KI_PROP_INDEX_NOT_FOUND". ⚠️ NON-GATING --
//                           `bne` skips only the assert call; the console falls straight into
//                           RemoveProp either way, so a fired assert removes nothing extra and
//                           skips nothing.
//   0x826277E8..0x826277FC  RemoveProp(lEntityId, liPropIndex, lpSceneInput, lpSimModuleInputBuffer)
//   0x82627800..0x82627808  ++luEventIndex; `cmplw` == UNSIGNED loop-back test
//
// ⚠️ THE PART TWIN HAS TWO ASSERTS AND THIS ONE HAS ONE. PropManager_wQ_02.cpp's
// ProcessRemovePartInstanceEvents carries the same message twice (baked lines 735 and 737); this
// function's listing contains exactly one BeginAssert/FireAssert/EndAssert triple. Checked
// deliberately, because the two drains are otherwise instruction-for-instruction the same shape
// and it would have been easy to copy the twin's pair across.
//
// ---- CONSOLE VALUES ARE EVIDENCE ONLY (gotcha 1) ------------------------------------------
// `addi r24, r4, 0x1F60` is the CONSOLE offset of PropInputInterface::mRemovePropQueue, and
// `lwz r,8(queue)` is the console offset of BaseEventQueue<T>::miLength. Both are meaningless on
// the LLP64 host and are recorded here as evidence only; the source-level route is the DWARF's
// own accessor (BrnPropInputInterface.h:108, landed as GetRemovePhysicalPropQueue() const) and
// GetLength(). The host offset happens to agree for this queue -- BrnPropInputInterface.h
// documents mRemovePropQueue at +0x1F60 -- which is a cross-check on the accessor being the right
// one, not a licence to use the number.
//
// ---- THE DWARF, WHICH SETTLES THE LOCALS --------------------------------------------------
// dwarfdump GameSource/Physics/PropManager/BrnPropManager.cpp:844-871 (source :665) declares
// exactly seven locals -- luEventIndex (:666), luQueueSize (:667), RigidBodyId lRigidBodyId
// (:668), lpQueue (:669), lpEvent (:670), lEntityId (:671), liPropIndex (:672) -- and its callee
// list is GetLength / GetEvent. Every name below is one of those.
// ⚠️ `RigidBodyId lRigidBodyId` IS DECLARED BY THE DWARF AND NEVER USED BY THE ARTIST EMISSION
// (no `std`/`ld` of an 8-byte handle anywhere in the 39 instructions, and RemoveProp takes none).
// It is NOT written below -- an unused local is not a side effect, and materialising one would be
// inventing a store the binary does not have. Recorded here so the divergence from the DWARF is
// deliberate and visible. The PART twin's banner records the identical situation.
//
// ---- GOTCHA SWEEP -------------------------------------------------------------------------
// gotcha 1 console literals: the two offsets above, both reached by name instead.
// gotcha 2 embedded sub-objects: no site -- this body performs no store at all.
// gotcha 3 PPC float args: none; RemoveProp takes no float or vector.
// gotcha 4 NaN polarity: no float compare in the function.
// gotcha 5 rodata tables: none.
// gotcha 12 gate-green != link-green: every callee here has a real body in the tree --
//           BaseEventQueue<RemovePhysicalPropEvent>::{GetLength,GetEvent} (header inlines) and
//           PropManager::RemoveProp (PropManager_wQ2_04.cpp). This partfile adds NO unresolved
//           external of its own; that is the whole point of landing it.
// ==========================================================================================

#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"  // PropInputInterface + its queues
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT

namespace BrnPhysics
{
namespace Props
{

// =============================================================================================
// ProcessRemovePropInstanceEvents -- X360 0x82627778, 39 instructions.
// DecFIGS: dwarfdump BrnPropManager.cpp:844 (scope, source :665). Parameter names are the
// DWARF's and match the committed declaration in BrnPropManager.h.
// =============================================================================================
void
PropManager::ProcessRemovePropInstanceEvents(
    const PropInputInterface*                                lpInput,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    const PropInputInterface::RemovePropEventQueue* lpQueue =
        &lpInput->GetRemovePhysicalPropQueue();                          // DWARF :669

    // 0x82627798 -- read ONCE, before the loop, with the empty-queue early-out at 0x826277A0.
    const u32 luQueueSize = static_cast<u32>( lpQueue->GetLength() );    // DWARF :667

    for ( u32 luEventIndex = 0; luEventIndex < luQueueSize; ++luEventIndex )   // DWARF :666
    {
        const RemovePhysicalPropEvent* lpEvent =
            &lpQueue->GetEvent( static_cast<s32>( luEventIndex ) );       // DWARF :670

        const BrnWorld::PropEntityID lEntityId   = lpEvent->mEntityId;        // DWARF :671
        const s32                    liPropIndex = lpEvent->miPhysicalIndex;  // DWARF :672

        // 0x826277C8 -- SIGNED compare (`cmpwi cr6, r30, -1`). NON-GATING: the console falls
        // straight through into RemoveProp whether or not this fires.
        CGS_ASSERT( liPropIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPropIndex != KI_PROP_INDEX_NOT_FOUND" );           // :689 (baked 0x2B1)

        RemoveProp( lEntityId,
                    static_cast<u32>( liPropIndex ),
                    lpSceneInput,
                    lpSimModuleInputBuffer );
    }
}

}
}
