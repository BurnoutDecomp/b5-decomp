#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetEntityPosition.h" // InEventSetEntityPosition element (32-byte, alignas(16))

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetEntityPosition>::AddEvent
//   @ X360 0x822AAC18 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventSetEntityPosition>",
//   funcs: 2 -- this TU covers AddEvent only; the sibling Append @ 0x827A58D8 is the other
//   function of this same dossier id and is committed as its own explicit-instantiation TU).
//
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store:
//   * mpEvents != NULL (CgsBaseEventQueue.h, `lwz r11,0(r29)`; bne cr6 skips) -- tripwire only;
//   * miLength < miMaxLength ("Reached Max length", `lwz r11,8(r29)` (miLength) vs
//     `lwz r10,4(r29)` (miMaxLength), blt cr6 skips) -- tripwire only; the copy below always runs;
//   * store at a 32-byte stride: `lwz r11,8(r29)` (miLength), `slwi r11,r11,5` (miLength*32),
//     `add r11,r11,r10` (+mpEvents @0(r29)), then four 8-byte block stores
//     (`ld/std r10` from a2 @0/@8/@0x10/@0x18 == 32 bytes) -- i.e.
//     sizeof(InEventSetEntityPosition) == 32, matching the already-committed element home
//     (CgsSceneManagerIO_EventSetEntityPosition.h: Vector3 mPosition +0x00 + EntityId mEntityId
//     +0x10, alignas(16), sizeof 0x20);
//   * bumps miLength (`lwz r11,8(r29)`/`addi r11,r11,1`/`stw r11,8(r29)`); returns 1 (`li r3,1`).
//
// Member offsets read from the asm: mpEvents @+0, miMaxLength @+4, miLength @+8 (consistent with
// the generic BaseEventQueue<T> layout in CgsBaseEventQueue.h). The de-inlined assert-builder
// sequence (BeginAssert / CgsContainers::BasePriorityQueue::Clear / vtable-dispatched string
// appends via sub_821F0E50 / FireAssert / EndAssert) is the X360 message-formatting machinery
// behind CGS_ASSERT's "Reached Max length" tripwire and collapses to the single CGS_ASSERT call
// already in the generic body -- nothing additional to model here.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetEntityPosition>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventSetEntityPosition&);