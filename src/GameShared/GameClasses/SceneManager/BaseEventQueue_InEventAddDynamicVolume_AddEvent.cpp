#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddDynamicVolume.h" // InEventAddDynamicVolume element (144B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume>::AddEvent
//   @ 0x822AAEC8   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one InEventAddDynamicVolume (144-byte element) to the add-dynamic-volume
// input queue and returns 1. The assert string baked in this body is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the
// derived EventQueue<T,N> -- AddEvent instantiation (cf. the sibling Construct
// instantiation in EventQueue_InEventAddDynamicVolume_1280.cpp, which is the derived
// EventQueue<T,1280> specialisation for the same element type). Called from
// InSceneUpdateInterface::AddDynamicVolume.
//
// X360 store-for-store (asm at 0x822AAEC8), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)               ; assert mpEvents != NULL (line 312, non-gating tripwire)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw r11, r10
//   blt .append                    ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 tripwire, builds "...Reached Max length <miLength>\n" via the
//        priority-queue-backed string-builder + FireAssert) ...
// .append:
//   lwz r11, 8(this); slwi r7,r11,3; add r11,r11,r7; slwi r11,r11,4 ; r11 = miLength*144
//                                  ; (miLength*9*16 == miLength*144 == miLength*stride)
//   add r11, r11, mpEvents         ; &mpEvents[miLength]            (stride 144 == sizeof element)
//   li  r9, 0x12 (18); mtctr r9
// .loop:
//   ld r9, 0(src); addi src,src,8; std r9,0(dst); addi dst,dst,8 ; 18 x 8-byte = 144-byte copy
//   bdnz .loop
//   lwz r11, 8(this); addi r11,r11,1; stw r11,8(this) ; ++miLength
//   li r3, 1; return 1
// == BaseEventQueue<InEventAddDynamicVolume>::AddEvent: append the 144-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return
// true. The 18 x 8-byte ld/std loop is the compiler's lowering of the generic
// `mpEvents[miLength] = lEvent;` struct-copy for the 144-byte element.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume& lEvent);