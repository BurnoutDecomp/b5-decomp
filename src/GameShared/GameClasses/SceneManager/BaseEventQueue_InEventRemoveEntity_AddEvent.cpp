#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveEntity.h" // InEventRemoveEntity (8-byte, two u32 words)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveEntity>::AddEvent
//   @ X360 0x822AB180 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventRemoveEntity>",
//   funcs: 2; this TU covers AddEvent only -- the sibling Append @ 0x827A5F28 is a separate
//   instantiation/TU).
//
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store:
//   * mpEvents != NULL ("mpEvents != NULL", lwz r11,0(r29); bne skips) -- non-gating tripwire;
//   * miLength < miMaxLength (lwz r11,8(r29) vs lwz r10,4(r29), blt skips the "Reached Max
//     length" assert) -- non-gating tripwire; the copy below always runs;
//   * store at an 8-byte stride: lwz r11,8(r29) (miLength), slwi r11,r11,3 (miLength*8),
//     lwz r10,0(r29) (+mpEvents), add r11,r11,r10, then two 32-bit word stores
//     (lwz r9,0(r26)/stw r9,0(r11), lwz r10,4(r26)/stw r10,4(r11)) -- i.e.
//     sizeof(InEventRemoveEntity) == 8, an X360-ATTESTED 8-byte stride (two u32 words);
//   * bumps miLength (lwz r11,8(r29)/addi r11,r11,1/stw r11,8(r29)); returns 1 (li r3,1).
//
// Member offsets read from the asm: mpEvents @+0, miMaxLength @+4, miLength @+8 (consistent with
// the generic BaseEventQueue<T> layout in CgsBaseEventQueue.h).
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveEntity>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventRemoveEntity& lEvent);
