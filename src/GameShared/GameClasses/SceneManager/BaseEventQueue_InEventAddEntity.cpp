#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                      // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddEntity.h" // InEventAddEntity (32-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddEntity>::AddEvent
//   @ X360 0x822AAD70   (dossier id "class:CgsSceneManager::SceneManagerIO::InEventAddEntity>")
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Thin explicit instantiation: the generic AddEvent body is already inline in
// CgsBaseEventQueue.h and matches the X360 body store-for-store (header offsets are
// mpEvents @0, miMaxLength @4, miLength @8):
//   * CGS_ASSERT(mpEvents != NULL)        -- lwz r11,0(r29); bne (non-gating tripwire, :312)
//   * CGS_ASSERT(miLength < miMaxLength)  -- lwz r11,8(r29); lwz r10,4(r29); blt
//                                            (non-gating "Reached Max length" tripwire, :313)
//   * mpEvents[miLength] = lEvent         -- slwi r11,r11,5 (x32 stride = sizeof(InEventAddEntity))
//                                            + 4x ld/std at 0,8,0x10,0x18 (32-byte copy)
//   * ++miLength                          -- lwz r11,8(r29); addi r11,1; stw r11,8(r29)
//   * return true                         -- li r3,1
// The copy always runs; both asserts only print (branches skip them) -- matching the
// committed sibling BaseEventQueue_InEventRemoveVolumeInstance_AddEvent.cpp.
//
// Called by InSceneUpdateInterface::AddEntity. The matching Construct instantiation lives
// in EventQueue_InEventAddEntity_5120.cpp; Append (X360 0x827A5AA8, same 32-byte element /
// x32 stride) is its sibling explicit instantiation (separate TU, out of this function's scope).
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddEntity>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventAddEntity& lEvent);