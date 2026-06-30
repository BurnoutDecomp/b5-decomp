#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // CgsModule::BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventReplaceDynamicVolume.h" // InEventReplaceDynamicVolume element (144B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>::Append
//   @ 0x827A62A8   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>,
//    Hex-Rays "...::App" == decorated/truncated "Append"). Called from
//    InSceneUpdateInterface::Append.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation.
//
// X360 store-for-store (asm at 0x827A62A8), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11,0(this); cmplwi r11,0; bne .check     ; assert mpEvents != NULL
//                                                 ;   (CgsBaseEventQueue.h:413 "mpEvents != NULL", :0x19D)
//   lwz r11,8(src); lwz r10,8(this); add r11,r11,r10 ; src.miLength + miLength
//   lwz r9,4(this); cmpw r11,r9; ble .src         ; assert sum <= miMaxLength
//                                                 ;   (CgsBaseEventQueue.h:414 "Base event queue overflow", :0x19E)
//   lwz r11,0(src); cmplwi r11,0; bne .copy       ; assert src.mpEvents != NULL (GetQueueStartPointer,
//                                                 ;   CgsBaseEventQueue.h:486 "mpEvents != NULL", :0x1E6)
//   lwz r29,8(src)                                ; liSourceLength = src.miLength
//   lwz r11,8(this); slwi r9,r29,3; slwi r8,r11,3 ; *8 partials
//   add r9,r29,r9; add r11,r11,r8                 ; *(1+8) == *9
//   lwz r4,0(src) ; src.mpEvents
//   slwi r5,r9,4  ; count bytes  = src.miLength * 9 * 16 == *144 (sizeof element)
//   slwi r11,r11,4; lwz r10,0(this); add r3,r11,r10 ; dst = mpEvents + miLength * 9 * 16 == +miLength*144
//   bl XMemCpy                                    ; XMemCpy(mpEvents + miLength*144, src.mpEvents, src.miLength*144)
//   lwz r11,8(src); lwz r10,8(this); add r11,r11,r10; stw r11,8(this) ; miLength += src.miLength
//   li r3,1; return 1
// == BaseEventQueue<InEventReplaceDynamicVolume>::Append: the three asserts are non-gating
// tripwires; the block-copy strides 144 bytes == sizeof(InEventReplaceDynamicVolume),
// modelled by the generic std::memcpy(mpEvents + miLength, lpSourceEvents,
// sizeof(T) * liSourceLength).
//
// X360-attested element stride (`slwi r,count,3; add; slwi r,r,4` == *(1+8)*16 == *144;
// cross-confirmed by the sibling AddEvent @ 0x822AB670 18x8-byte doubleword copy loop = 144B).
static_assert(sizeof(CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume) == 144,
              "InEventReplaceDynamicVolume stride 144");

template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>&);