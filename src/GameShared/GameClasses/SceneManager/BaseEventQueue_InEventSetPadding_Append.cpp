#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // CgsModule::BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetPadding.h" // InEventSetPadding element (32B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetPadding>::Append
//   @ 0x827A6640   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventSetPadding>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Merges every live event from a source set-padding queue onto the tail of this queue and
// returns 1. The generic BaseEventQueue<T>::Append body is already inline in
// CgsBaseEventQueue.h; this is the thin explicit instantiation. Called from
// InSceneUpdateInterface::Append.
//
// X360 store-for-store (asm at 0x827A6640), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)                          ; assert mpEvents != NULL
//   cmplwi r11, 0; bne .check                 ;   (CgsBaseEventQueue.h:413 "mpEvents != NULL", :0x19D)
//   lwz r11, 8(src); lwz r10, 8(this); add r11, r11, r10  ; src.miLength + miLength
//   lwz r9, 4(this); cmpw r11, r9; ble .src   ; assert sum <= miMaxLength
//                                             ;   (CgsBaseEventQueue.h:414 "Base event queue overflow", :0x19E)
//   lwz r11, 0(src); cmplwi r11, 0; bne .copy ; assert src.mpEvents != NULL (GetQueueStartPointer,
//                                             ;   CgsBaseEventQueue.h:486 "mpEvents != NULL", :0x1E6)
//   lwz r29, 8(src)                           ; liSourceLength = src.miLength
//   lwz r11, 8(this); slwi r5, r29, 5         ; count bytes = src.miLength * 32 (sizeof element)
//   lwz r10, 0(this); slwi r11, r11, 5        ; dst byte offset = miLength * 32
//   lwz r4, 0(src); add r3, r11, r10          ; XMemCpy(mpEvents + miLength*32, src.mpEvents, src.miLength*32)
//   bl XMemCpy
//   lwz r11, 8(src); lwz r10, 8(this); add r11, r11, r10; stw r11, 8(this) ; miLength += src.miLength
//   li r3, 1; return 1
// == BaseEventQueue<InEventSetPadding>::Append: the three asserts are non-gating tripwires;
// the block-copy strides 32 bytes == sizeof(InEventSetPadding), modelled by the generic
// std::memcpy(mpEvents + miLength, lpSourceEvents, sizeof(T) * liSourceLength).
//
// X360-attested element stride (`slwi r, count, 5` == *32).
static_assert(sizeof(CgsSceneManager::SceneManagerIO::InEventSetPadding) == 32,
              "InEventSetPadding stride 32");

template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetPadding>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetPadding>&);
