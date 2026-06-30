#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddVolumeInstance.h" // InEventAddVolumeInstance (80-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance>::AddEvent
//   @ X360 0x822C66D8  (dossier id "class:CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance>",
//   func "AddEve" -- Hex-Rays-truncated demangled name for AddEvent; funcs: 2, this TU covers
//   AddEvent only -- the sibling Append @ 0x827A5D58 is a separate instantiation/TU).
//
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store:
//   * mpEvents != NULL          (CgsBaseEventQueue.h, `lwz r11,0(r31)` @0x822C66EC; bne skips) --
//     tripwire only;
//   * miLength < miMaxLength    ("...Reached Max length...", `lwz r11,8(r31)` (miLength) vs
//     `lwz r10,4(r31)` (miMaxLength) @0x822C671C..0x822C6724, blt skips) -- tripwire only; the
//     copy below always runs unconditionally;
//   * dest = mpEvents + miLength*80, at an 80-byte stride: `slwi r8,r10,2` (miLength*4),
//     `add r10,r10,r8` (miLength*5), `slwi r10,r10,4` (*16 == miLength*80), `add r9,r10,r9`
//     (+ mpEvents @0(r31)) @0x822C67F4..0x822C6810;
//   * the element copy is two 8-byte `std` (mVolumeInstanceId @+0, mVolumeId @+8) followed by
//     four 16-byte lvx128/stvx128 (the 64-byte Matrix44Affine @+0x10) @0x822C6814..0x822C6848 --
//     i.e. a 16-byte-aligned struct-copy of InEventAddVolumeInstance == 8+8+64 = 80 bytes, matching
//     `mpEvents[miLength] = lEvent`;
//   * bumps miLength: `addi r4,r10,1` / `stw r4,8(r31)` (old miLength used for the dest address
//     calc, incremented length stored back -- semantically `++miLength`); returns 1 (`li r3,1`).
//
// Member offsets read from the asm: mpEvents @+0, miMaxLength @+4, miLength @+8 (consistent with
// the generic BaseEventQueue<T> layout in CgsBaseEventQueue.h). The 80-byte stride matches
// sizeof(InEventAddVolumeInstance) == 0x50 (80, alignas(16): two 8-byte handles + a 64-byte
// Matrix44Affine), X360-attested in CgsSceneManagerIO_EventAddVolumeInstance.h. This is the
// AddEvent half of the InEventAddVolumeInstance family whose Construct instantiation lives in
// EventQueue_InEventAddVolumeInstance_1280.cpp. Reached from
// InSceneUpdateInterface::AddVolumeInstance.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance&);
