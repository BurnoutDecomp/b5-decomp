// =============================================================================
// Explicit-instantiation home for the three volume-instance input queues embedded in
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface (CgsSceneManagerIO_SceneUpdate.h):
//   mSetVolumeInstanceTransformQueue     EventQueue<InEventSetVolumeInstanceTransform,     1024>
//   mSetVolumeInstanceCullingGroupQueue  EventQueue<InEventSetVolumeInstanceCullingGroup,  1280>
//   mAddVolumeInstanceForCachingQueue    EventQueue<InEventAddVolumeInstanceForCaching,      64>
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The generic
// EventQueue<T,N>::Construct and BaseEventQueue<T>::AddEvent bodies are already inline in
// CgsEventQueue.h / CgsBaseEventQueue.h; this TU forces the six out-of-line specialisations the
// X360 emitted, mirroring the committed sibling EventQueue_InEventAddVolumeInstance_1280.cpp /
// BaseEventQueue_InEventAddVolumeInstance_AddEvent.cpp idiom. The element records
// (InEventSetVolumeInstanceTransform 0x50, InEventSetVolumeInstanceCullingGroup 0x10,
// InEventAddVolumeInstanceForCaching 0x10) are the committed home CgsSceneManagerIO_SceneUpdate.h.
// =============================================================================

#include "GameShared/GameClasses/Module/CgsEventQueue.h"      // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"  // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // the three volume-instance element records

// ----------------------------- EventQueue<T,N>::Construct ---------------------------------

// EventQueue<InEventSetVolumeInstanceTransform, 1024>::Construct  @ X360 0x822E1D10
//   `addi r30, this, 0x10` (maEvents @ +0x10 -- the 12-byte header rounds up to the element's
//   16-byte alignment; the element carries a Matrix44Affine), `stw r30, 0(this)` (mpEvents),
//   `li r11, 0x400; stw r11, 4(this)` (miMaxLength = 1024), `li r10, 0; stw r10, 8(this)`
//   (miLength = 0). == BaseEventQueue<T>::Construct(maEvents, 1024). Reached from
//   InSceneUpdateInterface::Construct. (The Hex-Rays `result == -16` is the addi+cmplwi misread.)
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventSetVolumeInstanceTransform, 1024>::Construct();

// EventQueue<InEventSetVolumeInstanceCullingGroup, 1280>::Construct  @ X360 0x822E2480
//   `addi r30, this, 4` (maEvents @ +4 -- 8-byte-aligned 0x10 element, header not padded),
//   `stw r30,0(this)`, `li r11, 0x500; stw r11, 4(this)` (miMaxLength = 1280), miLength = 0.
//   == BaseEventQueue<T>::Construct(maEvents, 1280). Reached from InSceneUpdateInterface::Construct.
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventSetVolumeInstanceCullingGroup, 1280>::Construct();

// EventQueue<InEventAddVolumeInstanceForCaching, 64>::Construct  @ X360 0x822E23A0
//   `addi r30, this, 4` (maEvents @ +4), `stw r30,0(this)`, `li r11, 0x40; stw r11, 4(this)`
//   (miMaxLength = 64), miLength = 0. == BaseEventQueue<T>::Construct(maEvents, 64). Reached from
//   InSceneUpdateInterface::Construct.
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventAddVolumeInstanceForCaching, 64>::Construct();

// ----------------------------- BaseEventQueue<T>::AddEvent ---------------------------------

// BaseEventQueue<InEventSetVolumeInstanceTransform>::AddEvent  @ X360 0x822C6560
//   Asserts mpEvents != NULL (CgsBaseEventQueue.h:312) and miLength < miMaxLength (:313) as
//   non-gating tripwires, then appends the 80-byte element at an `80 * miLength` stride: the id
//   word (`std r4`) then four 16-byte lvx128/stvx128 matrix lanes (== the 8+8+64 = 0x50-byte
//   InEventSetVolumeInstanceTransform struct-copy), bumps miLength and returns 1. Reached from
//   InSceneUpdateInterface::SetVolumeInstanceTransform.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetVolumeInstanceTransform>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventSetVolumeInstanceTransform&);

// BaseEventQueue<InEventSetVolumeInstanceCullingGroup>::AddEvent  @ X360 0x822ABBA8
//   Same generic AddEvent shape (asserts :312/:313 non-gating), appends the 16-byte element at a
//   `16 * miLength` stride via two 8-byte word stores (`*v12 = *a2; v12[1] = a2[1]`), bumps
//   miLength, returns 1. Reached from InSceneUpdateInterface::SetVolumeInstanceCullingGroup.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetVolumeInstanceCullingGroup>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventSetVolumeInstanceCullingGroup&);

// BaseEventQueue<InEventAddVolumeInstanceForCaching>::AddEvent  @ X360 0x82709600
//   Same generic AddEvent shape (asserts :312/:313 non-gating), appends the 16-byte element at a
//   `16 * miLength` stride via two 8-byte word stores, bumps miLength, returns 1. Reached from
//   InSceneUpdateInterface::AddVolumeInstanceForCaching.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddVolumeInstanceForCaching>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventAddVolumeInstanceForCaching&);
