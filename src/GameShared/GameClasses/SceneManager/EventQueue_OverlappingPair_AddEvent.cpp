#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"             // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsOverlappingPair.h"      // CgsSceneManager::OverlappingPair element (16B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::OverlappingPair>::AddEvent
//   @ 0x828B8B08   (ledger id: class:CgsSceneManager::OverlappingPair>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one OverlappingPair to the scene manager's overlap list and returns 1.
// The assert string baked at 0x828B8B08 is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::OverlappingPair>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the
// derived EventQueue<T,N> -- AddEvent instantiation. The two callers are
// SceneManagerModule::BridgeOverlapGenerationToOverlapCulling and
// OverlapGenerationModule::OutputCollidingPairs.
//
// X360 store-for-store (asm at 0x828B8B08), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)               ; assert mpEvents != NULL  (line 312, non-gating)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw r11, r10
//   blt .append                    ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 tripwire) ...
// .append:
//   lwz r11, 8(this); slwi r11, r11, 4; add r11, r11, mpEvents   ; &mpEvents[miLength] (stride 0x10)
//   lwz r10, 0(src);  stw r10, 0(dst)        ; word 0
//   lwz r10, 4(src);  stw r10, 4(dst)        ; word 1
//   lwz r10, 8(src);  stw r10, 8(dst)        ; word 2
//   lwz r10, 0xC(src);stw r10, 0xC(dst)      ; word 3   (== one 16-byte OverlappingPair copy)
//   lwz r11, 8(this); addi r11, r11, 1; stw r11, 8(this) ; ++miLength
//   li r3, 1; return 1
// == BaseEventQueue<OverlappingPair>::AddEvent: append the 16-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return
// true. The element is 16-byte / 4-word; the generic `mpEvents[miLength] = lEvent`
// copy lowers to the four word stores above.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::OverlappingPair>::AddEvent(
    const CgsSceneManager::OverlappingPair& lEvent);
