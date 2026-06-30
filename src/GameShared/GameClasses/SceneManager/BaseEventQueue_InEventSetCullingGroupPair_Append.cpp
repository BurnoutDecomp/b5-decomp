#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                            // CgsModule::BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetCullingGroupPair.h" // InEventSetCullingGroupPair element (12B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>::Append
//   @ 0x827A6398   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Merges every live event from a source set-culling-group-pair queue onto the tail of this
// queue and returns 1. The generic BaseEventQueue<T>::Append body is already inline in
// CgsBaseEventQueue.h; this is the thin explicit instantiation. Called from
// InSceneUpdateInterface::Append. The sibling AddEvent for this same element type lives at
// 0x822AB7D0 (same dossier/TU) and is instantiated separately.
//
// X360 store-for-store (asm at 0x827A6398), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)                          ; assert mpEvents != NULL
//   cmplwi r11, 0; bne .check                 ;   (CgsBaseEventQueue.h:413 "mpEvents != NULL", li r5,0x19D)
//   lwz r11, 8(src); lwz r10, 8(this); add r11, r11, r10  ; src.miLength + miLength
//   lwz r9, 4(this); cmpw r11, r9; ble .src   ; assert sum <= miMaxLength
//                                             ;   (CgsBaseEventQueue.h:414 "Base event queue overflow", li r5,0x19E)
//   lwz r11, 0(src); cmplwi r11, 0; bne .copy ; assert src.mpEvents != NULL (GetQueueStartPointer,
//                                             ;   CgsBaseEventQueue.h:486 "mpEvents != NULL", li r5,0x1E6)
//   lwz r29, 8(src)                           ; liSourceLength = src.miLength
//   lwz r11, 8(this); slwi r9, r29, 1; add r9, r29, r9; slwi r5, r9, 2 ; count = src.miLength*3*4 = *12
//   lwz r10, 0(this); slwi r8, r11, 1; add r11, r11, r8; slwi r11, r11, 2 ; dst off = miLength*3*4 = *12
//   lwz r4, 0(src); add r3, r11, r10          ; XMemCpy(mpEvents + miLength*12, src.mpEvents, src.miLength*12)
//   bl XMemCpy
//   lwz r11, 8(src); lwz r10, 8(this); add r11, r11, r10; stw r11, 8(this) ; miLength += src.miLength
//   li r3, 1; return 1
// == BaseEventQueue<InEventSetCullingGroupPair>::Append: the three asserts are non-gating
// tripwires; the block-copy strides 12 bytes == sizeof(InEventSetCullingGroupPair), modelled
// by the generic std::memcpy(mpEvents + miLength, lpSourceEvents, sizeof(T) * liSourceLength).
//
// X360-attested element stride (`slwi r9,r29,1; add r9,r29,r9; slwi r5,r9,2` == *3*4 == *12),
// cross-confirmed by the sibling AddEvent @0x822AB7D0 (three 32-bit `stw` per element).
static_assert(sizeof(CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair) == 12,
              "InEventSetCullingGroupPair stride 12");

template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>&);
