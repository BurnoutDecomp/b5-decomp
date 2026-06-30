#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // CgsSceneManager::SceneManagerIO::PotentialContact (80-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::PotentialContact>::AddEvent
//   @ X360 0x828AD230 (dossier id "class:CgsSceneManager::SceneManagerIO::PotentialContact>").
//
// The generic BaseEventQueue<T>::AddEvent body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * non-gating tripwire: mpEvents != NULL (`lwz r11,0(r29)`; bne skips the
//     BeginAssert/FireAssert/EndAssert triplet) -- collapsed to the committed generic's
//     CGS_ASSERT(mpEvents != nullptr, "mpEvents != NULL");
//   * non-gating tripwire: miLength < miMaxLength (`lwz r11,8(r29)` (miLength) vs
//     `lwz r10,4(r29)` (miMaxLength), `blt cr6` skips the assert) -- the asm appends
//     UNCONDITIONALLY even when this fires, matching the generic's non-gating AddEvent
//     semantics, unlike the bounds-gated AddEventSafe sibling (return 0 on full);
//   * unconditional copy of the 80-byte element to mpEvents[miLength] at an 80-byte stride
//     (`lwz r11,8(r29)` (miLength); `slwi r7,r11,2; add r11,r11,r7; slwi r11,r11,4` ==
//     miLength*5*16 == miLength*80; ctr=10 std/ld 64-bit block moves == 80 bytes), bumps
//     miLength (`lwz r11,8(r29); addi r11,r11,1; stw r11,8(r29)`) and returns 1 (`li r3,1`).
// The 80-byte stride matches sizeof(CgsSceneManager::SceneManagerIO::PotentialContact) == 80
// (3x Vector3 @16B = 48 + 2x VolumeInstanceId @8B = 16 (->64) + 2x u32 = 8 (->72) +
// 2x u16 = 4 (->76), alignas(16)-padded to 80; see committed CgsPotentialContact.h).
// Member offsets (asm): mpEvents @+0, miMaxLength @+4, miLength @+8 -- consistent with the
// generic BaseEventQueue<T> layout. Called from
// CgsSceneManager::SceneManagerModule::BridgeOverlapCullerToOutputBuffer to push scene-manager
// contact candidates into the culler's output queue.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::PotentialContact>::AddEvent(
    const CgsSceneManager::SceneManagerIO::PotentialContact&);