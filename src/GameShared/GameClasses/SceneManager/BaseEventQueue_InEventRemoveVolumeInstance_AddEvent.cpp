#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveVolumeInstance.h" // InEventRemoveVolumeInstance element (8B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveVolumeInstance>::AddEvent
//   @ 0x822AB538   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventRemoveVolumeInstance>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one InEventRemoveVolumeInstance to the remove-volume-instance input queue and
// returns 1. The assert string baked in this body is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::InEventRemoveVolumeInstance>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the
// derived EventQueue<T,N> -- AddEvent instantiation. Called from
// InSceneUpdateInterface::RemoveVolumeInstance.
//
// X360 store-for-store (asm at 0x822AB538), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)               ; assert mpEvents != NULL (line 312, non-gating tripwire)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw r11, r10
//   blt .append                    ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 tripwire) ...
// .append:
//   lwz r11, 8(this); slwi r11, r11, 3; ...   ; &mpEvents[miLength] (stride 8 == sizeof element)
//   ld  r10, 0(src)                ; load the 8-byte element image
//   stdx r10, miLength*8, mpEvents ; mpEvents[miLength] = *src   (single 64-bit move)
//   lwz r11, 8(this); addi r11, r11, 1; stw r11, 8(this) ; ++miLength
//   li r3, 1; return 1
// == BaseEventQueue<InEventRemoveVolumeInstance>::AddEvent: append the 8-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return
// true. The element is a single 64-bit VolumeInstanceId; the generic
// `mpEvents[miLength] = lEvent` copy lowers to the single `ld`/`stdx` above.
//
// The pseudocode's `v12 = *a2; HIDWORD(v12) = *a1; *(...) = v12` is Hex-Rays splitting
// the one 64-bit `ld r10,0(src)` / `stdx r10,...` into big-endian dword halves -- it is a
// single 8-byte copy of the source event, NOT a recompose of two separate words.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveVolumeInstance>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventRemoveVolumeInstance& lEvent);
