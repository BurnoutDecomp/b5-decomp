#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddForCollision.h" // InEventAddForCollision element (32B, alignas(16))

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddForCollision>::AddEvent
//   @ 0x822AB028   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventAddForCollision>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one InEventAddForCollision to the add-for-collision input queue and returns 1.
// The assert string baked in this body is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::InEventAddForCollision>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the
// derived EventQueue<T,N> -- AddEvent instantiation. Called from
// InSceneUpdateInterface::AddForCollision, PropCellManager::AddPropToContactGeneration /
// AddPropPartsToContactGeneration, Deformation::PhysicalBodyPart::AddToScene,
// Deformation::PhysicalWheel::AddToScene, TrafficEntityModule::GenerateCrashedVehicleEvents.
//
// X360 store-for-store (asm at 0x822AB028), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)                       ; assert mpEvents != NULL (line 312, non-gating tripwire)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw r11, r10
//   blt .append                            ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 tripwire, builds "...AddEvent\nReached Max length <miLength>\n") ...
// .append:
//   lwz r11, 8(this); slwi r11, r11, 5     ; &mpEvents[miLength] (stride 0x20 == sizeof element, 32B)
//   add r11, r11, r10                       ; r10 = mpEvents base
//   ld r10, 0(src); std r10, 0(r11)         ; 4x 8-byte (64-bit) moves = 32-byte element copy
//   ld r10, 8(src); std r10, 8(r11)
//   ld r10, 0x10(src); std r10, 0x10(r11)
//   ld r10, 0x18(src); std r10, 0x18(r11)
//   lwz r11, 8(this); addi r11, r11, 1; stw r11, 8(this) ; ++miLength
//   li r3, 1; return 1
// == BaseEventQueue<InEventAddForCollision>::AddEvent: append the 32-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return true.
// The element copy (4x `ld`/`std`) is Hex-Rays/PPC lowering of the generic's single
// `mpEvents[miLength] = lEvent;` for a 32-byte, 16-byte-aligned aggregate -- not a
// hand-unrolled field-by-field copy. Element sizeof==32: Vector3 mPadding (alignas(16),
// 16B) + VolumeInstanceId (u64, @16) + CullingGroup (u32, @24) + 2x u8 (@28,@29),
// struct alignas(16) rounds up to 0x20 -- matching the slwi ...,5 (x32) stride.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddForCollision>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventAddForCollision& lEvent);