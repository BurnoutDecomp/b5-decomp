#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"          // BaseEventQueue<T>::GetEvent(int) const (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerContact.h"  // CgsSceneManager::Contact (64-byte element, already committed)

// CgsModule::BaseEventQueue<CgsSceneManager::Contact>::GetEvent(s32) const
//   @ X360 0x828AE2D0 (dossier id "class:CgsSceneManager::Contact>", pseudocode name "GetEv").
//
// Thin explicit instantiation: the generic BaseEventQueue<T>::GetEvent(int) const body is
// already inline in CgsBaseEventQueue.h. Element type CgsSceneManager::Contact is ALREADY
// COMMITTED in CgsSceneManagerContact.h (the same header the sibling
// EventQueue_Contact_16384_Construct.cpp includes) -- include that, NOT CgsSceneManagerTypes.h.
//
// The X360 body matches the generic store-for-store (assert source lines 272/274/275 pin the
// const overload, CgsBaseEventQueue.h:270):
//   * mpEvents != NULL tripwire (:272; `lwz r11,0(r30)` mpEvents@+0; `bne` skips) -- CGS_ASSERT;
//   * liIndex < GetLength() tripwire (:274; `lwz r11,8(r30)` miLength@+8; `cmpw r29,r11; blt`
//     skips) -- CGS_ASSERT;
//   * liIndex >= 0 tripwire (:275; `cmpwi r29,0; bge` skips) -- CGS_ASSERT;
//   * returns &mpEvents[liIndex]: `lwz r10,0(r30)` (mpEvents base) + `slwi r11,r29,6`
//     (liIndex*64 == 64-byte stride) `add r3,r11,r10`. Hex-Rays shows `int` because the
//     T& is ABI-returned as a 32-bit pointer; DWARF gives the real `const T&`.
// The 64-byte stride == sizeof(CgsSceneManager::Contact) (committed alignas(16), 0x40).
// Member offsets (asm): mpEvents @+0, miLength @+8 -- the generic BaseEventQueue<T> layout.
// Called by CgsSceneManager::SceneManagerModule::BridgeOverlapCullerToOutputBuffer to read
// the culler's accumulated contacts before forwarding them to the output buffer.
template const CgsSceneManager::Contact&
CgsModule::BaseEventQueue<CgsSceneManager::Contact>::GetEvent(s32) const;
