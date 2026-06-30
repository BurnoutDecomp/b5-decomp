#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                   // BaseEventQueue<T>::AddEvent(const T&) (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h" // InEventLineTestFine (64-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestFine>::AddEvent
//   @ X360 0x822105C0 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventLineTestFine>";
//   the sibling Append @ 0x823C1FA0 is a separate instantiation, not this TU).
//
// The generic BaseEventQueue<T>::AddEvent(const T&) body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation the X360 emitted out-of-line for this element type.
// The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h "mpEvents != NULL" tripwire,
//     `lwz r11,0(r29)`; bne skips the assert -- NON-gating, append always runs);
//   * asserts no overflow ("Reached Max length" tripwire, `lwz r11,8(r29)` (miLength) vs
//     `lwz r10,4(r29)` (miMaxLength); blt skips the assert -- also NON-gating);
//   * unconditionally block-copies the 8-doubleword (64-byte) event payload from a2 to
//     mpEvents + miLength*64 (`slwi r10,r10,6` -- 64-byte element stride -- then an 8-iteration
//     `ld`/`std` doubleword copy loop), matching `mpEvents[miLength] = lEvent;`;
//   * bumps miLength by 1 (`lwz r11,8(r29); addi r11,r11,1; stw r11,8(r29)`) and returns 1 (true).
// Member offsets read from the asm: mpEvents @+0, miMaxLength @+4, miLength @+8 (the generic
// BaseEventQueue<T> layout in CgsBaseEventQueue.h).
//
// The 64-byte stride matches sizeof(InEventLineTestFine) == 0x40 (two Vector3 lanes +
// SceneQueryId/EntityTypeFlags/EntityId/EExclusionMode/VolumeTypeFlags, alignas(16)), the
// X360-attested stride shared with the sibling InEventLineTestFastDoubleSided element (see
// CgsSceneManagerIO_EventLineTest.h). Called from
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestFine,
// BrnWorld::PlaceOnTrackManager::PostSceneUpdate, and
// WorldModule::BridgeTriggerModuleToSceneModule_PostScene.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestFine>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventLineTestFine&);