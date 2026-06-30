#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // CgsModule::BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h" // OutOverlapPair element (24B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair>::Append
//   @ X360 0x827A6FE8   (dossier id: class:CgsSceneManager::SceneManagerIO::OutOverlapPair>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Merges every live event from a source overlap-pair queue onto the tail of this queue and
// returns 1. The generic BaseEventQueue<T>::Append body is already inline in
// CgsBaseEventQueue.h; this is the thin explicit instantiation. Called from
// BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics::SetOverlapPairsQueue and
// WorldModule::BridgeScenePotentialContactsToPhysics.
//
// X360 store-for-store (asm at 0x827A6FE8); offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)                          ; assert mpEvents != NULL
//   cmplwi r11, 0; bne .check                 ;   (CgsBaseEventQueue.h:413 "mpEvents != NULL", :0x19D)
//   lwz r11, 8(src); lwz r10, 8(this); add r11, r11, r10  ; src.miLength + miLength
//   lwz r9, 4(this); cmpw r11, r9; ble .src   ; assert sum <= miMaxLength (NON-gating tripwire)
//                                             ;   (CgsBaseEventQueue.h:414 "Base event queue overflow", :0x19E)
//   lwz r11, 0(src); cmplwi r11, 0; bne .copy ; assert src.mpEvents != NULL (GetQueueStartPointer,
//                                             ;   CgsBaseEventQueue.h:486 "mpEvents != NULL", :0x1E6)
//   lwz r29, 8(src)                           ; liSourceLength = src.miLength
//   lwz r11, 8(this); slwi r9,r29,1; add r9,r29,r9; slwi r5,r9,3   ; count = src.miLength*3*8 = *24
//   lwz r10, 0(this); slwi r8,r11,1; add r11,r11,r8; slwi r11,r11,3 ; dst off = miLength*3*8 = *24
//   lwz r4, 0(src); add r3, r11, r10          ; XMemCpy(mpEvents + miLength*24, src.mpEvents, src.miLength*24)
//   bl XMemCpy
//   lwz r11, 8(src); lwz r10, 8(this); add r11, r11, r10; stw r11, 8(this) ; miLength += src.miLength
//   li r3, 1; return 1
// == BaseEventQueue<OutOverlapPair>::Append: the three asserts are non-gating tripwires;
// the block-copy strides 24 bytes == sizeof(OutOverlapPair), modelled by the generic
// std::memcpy(mpEvents + miLength, lpSourceEvents, sizeof(T) * liSourceLength).
//
// X360-attested element stride: the `slwi ...,1; add; slwi ...,3` pair computes count*3*8 == *24.
// Independently confirmed by the sibling AddEvent @ 0x828AD390 in the same dossier/TU family,
// which stores three consecutive 8-byte fields per element: std r8,0(r11); std r10,8(r11);
// std r10,0x10(r11) -> 24-byte (3x QWORD) record.
static_assert(sizeof(CgsSceneManager::SceneManagerIO::OutOverlapPair) == 24,
              "OutOverlapPair stride 24");

template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair>&);