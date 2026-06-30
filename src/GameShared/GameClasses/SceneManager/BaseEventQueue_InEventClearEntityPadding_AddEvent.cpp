#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventClearEntityPadding.h" // InEventClearEntityPadding element (4B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearEntityPadding>::AddEvent
//   @ X360 0x822ABA70   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventClearEntityPadding>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one InEventClearEntityPadding to the clear-entity-padding input queue and
// returns 1. The assert string baked in this body is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::InEventClearEntityPadding>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the
// derived EventQueue<T,N> -- AddEvent instantiation. Called from
// InSceneUpdateInterface::ClearEntityVolumesPadding.
//
// X360 store-for-store (asm at 0x822ABA70), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)               ; assert mpEvents != NULL (line 312, non-gating tripwire)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw r11, r10
//   blt .append                    ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 StrStream tripwire, builds "...AddEvent\nReached Max length \n") ...
// .append:
//   lwz r11, 8(this)               ; miLength
//   slwi r11, r11, 2               ; miLength * 4 (stride 4 == sizeof element, single u32 EntityId)
//   lwz r10, 0(src)                ; *src (the 4-byte EntityId image)
//   lwz r9,  0(this)               ; mpEvents
//   stwx r10, r11, r9              ; mpEvents[miLength] = *src   (single 32-bit store)
//   lwz r11, 8(this); addi r11, r11, 1; stw r11, 8(this) ; ++miLength
//   li r3, 1; return 1
// == BaseEventQueue<InEventClearEntityPadding>::AddEvent: append the 4-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return
// true. The element is a single u32 EntityId; the generic `mpEvents[miLength] = lEvent`
// copy lowers to the single `lwz`/`stwx` above.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearEntityPadding>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventClearEntityPadding& lEvent);