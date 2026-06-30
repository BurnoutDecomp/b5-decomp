#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventReplaceDynamicVolume.h" // InEventReplaceDynamicVolume element (144B, already committed)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>::AddEvent
//   @ 0x822AB670   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>::Add)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one InEventReplaceDynamicVolume to the replace-dynamic-volume input queue
// (InSceneUpdateInterface::mReplaceDynamicVolumeQueue) and returns 1. The assert string
// baked in this body is
//   \"CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>::AddEvent\"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the
// derived EventQueue<T,N> -- AddEvent instantiation. Called from
// InSceneUpdateInterface::ReplaceDynamicVolume.
//
// X360 store-for-store (asm at 0x822AB670), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)                          ; assert mpEvents != NULL (line 312, non-gating tripwire)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw   ; miLength < miMaxLength?
//   blt .append                               ; skip the \"Reached Max length\" assert when not full
//   ... (line 313 tripwire, builds \"...Reached Max length <miLength>\\n\") ...
// .append:
//   lwz r11, 8(this)                          ; miLength
//   slwi r7, r11, 3; add r11, r11, r7         ; r11 = miLength * 9
//   slwi r11, r11, 4                          ; r11 = miLength * 9 * 16 = miLength * 144
//   add  r11, r11, r8(=mpEvents)              ; dst = &mpEvents[miLength]   (stride 144 == sizeof(T))
//   mtctr 0x12 (=18)
//   do { ld r9,0(src); std r9,0(dst); src+=8; dst+=8; } while(--ctr)  ; 18*8 = 144-byte block copy
//   lwz r11, 8(this); addi r11,r11,1; stw r11,8(this) ; ++miLength
//   li r3, 1; return 1
// == BaseEventQueue<InEventReplaceDynamicVolume>::AddEvent: append the 144-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return true.
// The generic `mpEvents[miLength] = lEvent` copy lowers to the 18x 8-byte `ld`/`std` loop
// above (144 bytes, matching sizeof(InEventReplaceDynamicVolume)).
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume& lEvent);
