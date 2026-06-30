#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                         // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h" // CgsSceneManager::SceneManagerIO::OutOverlapPair element (24B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair>::AddEvent
//   @ 0x828AD390   (ledger id: class:CgsSceneManager::SceneManagerIO::OutOverlapPair>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one OutOverlapPair to the scene manager's overlap-output queue and returns 1.
// The assert string baked at 0x828AD390 is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::OutOverlapPair>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the
// derived EventQueue<T,N> -- AddEvent instantiation. The lone caller is
// SceneManagerModule::BridgeOverlapGenerationToOutputBuffer.
//
// This OutOverlapPair element is 24 bytes -- a DISTINCT type from the 16-byte
// CgsSceneManager::OverlappingPair (BaseEventQueue<OverlappingPair>::AddEvent @ 0x828B8B08,
// stride 0x10). Do NOT reuse CgsOverlappingPair.h here.
//
// X360 store-for-store (asm at 0x828AD390), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)               ; assert mpEvents != NULL  (line 312, non-gating tripwire)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw r11, r10
//   blt .append                    ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 tripwire) ...
// .append:
//   lwz  r11, 8(this)              ; miLength
//   slwi r9, r11, 1                ; miLength*2
//   add  r11, r11, r9             ; miLength*3
//   slwi r11, r11, 3              ; miLength*24   (stride 0x18 == sizeof(OutOverlapPair))
//   add  r11, r11, mpEvents      ; &mpEvents[miLength]
//   ld   r8, 0(src);  std r8, 0(dst)        ; qword 0
//   ld   r10,8(src);  std r10,8(dst)        ; qword 1
//   ld   r10,0x10(src);std r10,0x10(dst)    ; qword 2   (== one 24-byte OutOverlapPair copy)
//   lwz r11, 8(this); addi r11, r11, 1; stw r11, 8(this) ; ++miLength
//   li r3, 1; return 1
// == BaseEventQueue<OutOverlapPair>::AddEvent: append the 24-byte element unconditionally
// (the two asserts are non-gating tripwires), bump miLength, return true. The element is
// 24-byte / three 8-byte words; the generic `mpEvents[miLength] = lEvent` copy lowers to
// the three qword stores above. Element-field semantics are NOT recovered in this dossier
// scope (only AddEvent @0x828AD390 and Append @0x827A6FE8 are attested, neither reads the
// element interior), so the element is modelled as a 24-byte opaque, 8-byte-aligned span.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair>::AddEvent(
    const CgsSceneManager::SceneManagerIO::OutOverlapPair& lEvent);