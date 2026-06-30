#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetCullingGroupPair.h" // InEventSetCullingGroupPair element (12B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>::AddEvent
//   @ 0x822AB7D0   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one InEventSetCullingGroupPair to the set-culling-group-pair input queue
// (InSceneUpdateInterface::mSetCullingGroupPairQueue) and returns 1. The assert string baked
// in this body is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the derived
// EventQueue<T,N> -- AddEvent instantiation. Called from
// InSceneUpdateInterface::SetCullingGroupPair. The sibling Append @ 0x827A6398 is a SEPARATE
// assigned function (XMemCpy-based bulk copy), not covered here.
//
// X360 store-for-store (asm at 0x822AB7D0), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(r29)                ; assert mpEvents != NULL (line 312, non-gating tripwire)
//   lwz r11, 8(r29); lwz r10, 4(r29); cmpw r11, r10
//   blt .append                    ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 tripwire) ...
// .append (0x822AB8DC):
//   lwz r11, 8(r29)                ; miLength
//   slwi r9, r11, 1 ; add r11, r11, r9 ; slwi r11, r11, 2  ; index = miLength*(1+2)*4 = miLength*12
//   add r11, r11, r10              ; &mpEvents[miLength] (mpEvents @ 0(r29); stride 0x0C == sizeof T)
//   lwz r8,0(r26); stw r8,0(r11)   ; word 0  (muGroupA)
//   lwz r10,4(r26); stw r10,4(r11) ; word 1  (muGroupB)
//   lwz r10,8(r26); stw r10,8(r11) ; word 2  (muEnabled)
//   lwz r11, 8(r29); addi r11, r11, 1; stw r11, 8(r29)     ; ++miLength
//   li r3, 1; return 1
// == BaseEventQueue<InEventSetCullingGroupPair>::AddEvent: append the 12-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return true. The
// element is three 32-bit words; the generic `mpEvents[miLength] = lEvent` copy lowers to the
// three lwz/stw word moves above (12 bytes is below the memcpy-call threshold, so it is unrolled
// inline -- contrast the sibling Append @ 0x827A6398 which calls XMemCpy for a variable count).
//
// The 12-byte stride matches sizeof(InEventSetCullingGroupPair) == 12 (X360-attested, three
// 32-bit words: muGroupA/muGroupB/muEnabled), already committed at
// CgsSceneManagerIO_EventSetCullingGroupPair.h. The Construct instantiation for this same
// EventQueue<InEventSetCullingGroupPair, 64> family lives at
// EventQueue_InEventSetCullingGroupPair_64.cpp (@ 0x822E21E0).
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair& lEvent);