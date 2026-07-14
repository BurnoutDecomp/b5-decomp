// =============================================================================
// Explicit-instantiation home for the three triangle-collision query input queues the
// SceneManager query IO buffer (InputBuffer_Query) embeds by value:
//   EventQueue<InEventTriangleCollisionLineTest,        256>   (line-test)
//   EventQueue<InEventTriangleCollisionLineTestNearest, 256>   (nearest line-test)
//   EventQueue<InEventTriangleCollisionSphereTest,      256>   (sphere-test)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The generic
// EventQueue<T,N>::Construct / BaseEventQueue<T>::AddEvent / BaseEventQueue<T>::Append bodies are
// already inline in CgsEventQueue.h / CgsBaseEventQueue.h; this TU forces the out-of-line
// specialisations the X360 emitted, mirroring the committed sibling EventQueue_InEventLineTest_256.cpp
// / BaseEventQueue_InEventTriangleCollisionLineTest_GetEvent.cpp idiom. Element homes: the committed
// CgsSceneManagerIO_EventTriangleCollisionLineTest.h (0x30) plus the two sibling element homes added
// alongside this TU (LineTestNearest 0x30, SphereTest 0x08).
// =============================================================================

#include "GameShared/GameClasses/Module/CgsEventQueue.h"      // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"  // CgsModule::BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventTriangleCollisionLineTest.h"        // InEventTriangleCollisionLineTest (0x30)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventTriangleCollisionLineTestNearest.h" // InEventTriangleCollisionLineTestNearest (0x30)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventTriangleCollisionSphereTest.h"      // InEventTriangleCollisionSphereTest (0x08)

// ----------------------------- EventQueue<T,N>::Construct ---------------------------------

// EventQueue<InEventTriangleCollisionLineTest, 256>::Construct  @ X360 0x828C48B8
//   `addi r30, this, 0x10` (maEvents @ +0x10 -- 12-byte header rounds up to the element's 16-byte
//   alignment), mpEvents = &maEvents, miMaxLength = 256 (0x100), miLength = 0. Reached from
//   InputBuffer_Query::Construct. (Hex-Rays `result == -16` is the addi+cmplwi misread.)
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTest, 256>::Construct();

// EventQueue<InEventTriangleCollisionLineTestNearest, 256>::Construct  @ X360 0x828C4928
//   `addi r30, this, 0x10` (maEvents @ +0x10 -- 16-byte-aligned 0x30 element), miMaxLength = 256,
//   miLength = 0. Reached from InputBuffer_Query::Construct.
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTestNearest, 256>::Construct();

// EventQueue<InEventTriangleCollisionSphereTest, 256>::Construct  @ X360 0x828C4998
//   `addi r30, this, 0xC` (maEvents @ +0x0C -- the 8-byte, <=4-aligned element leaves the 12-byte
//   header un-padded), miMaxLength = 256, miLength = 0. Reached from InputBuffer_Query::Construct.
//   (Hex-Rays `result == -12` is the addi+cmplwi misread; the +0xC header is why the sibling
//   element home is default-aligned rather than alignas(16).)
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventTriangleCollisionSphereTest, 256>::Construct();

// ------------------------- BaseEventQueue<T>::AddEvent / Append ----------------------------

// BaseEventQueue<InEventTriangleCollisionLineTest>::AddEvent  @ X360 0x828B7C38
//   Asserts mpEvents != NULL (CgsBaseEventQueue.h:312) and miLength < miMaxLength (:313) as
//   non-gating tripwires (the assert rodata names
//   "CgsModule::BaseEventQueue<class ...InEventTriangleCollisionLineTest>::AddEvent"), then appends
//   the 48-byte element at a `48 * miLength` stride via six 8-byte word moves (Hex-Rays
//   `v13 = 6; do { *v12++ = *v11++; } while(--v13)` over _QWORD slots == the 0x30-byte
//   `mpEvents[miLength] = lEvent` struct copy), bumps miLength, returns 1. Reached from
//   SceneManagerModule::ProcessLineTestFine.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTest>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTest&);

// BaseEventQueue<InEventTriangleCollisionLineTestNearest>::AddEvent  @ X360 0x828B7D98
//   Same generic AddEvent shape (asserts :312/:313 non-gating; rodata names the "...Nearest"
//   specialisation), appends the 48-byte element at a `48 * miLength` stride via six 8-byte word
//   moves, bumps miLength, returns 1. Reached from SceneManagerModule::ProcessLineTestNearest.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTestNearest>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTestNearest&);

// BaseEventQueue<InEventTriangleCollisionSphereTest>::Append  @ X360 0x828B9848
//   Merges a source queue's live events onto this one (CgsBaseEventQueue.h:413/:414/:486
//   tripwires -- "mpEvents != NULL" / "Base event queue overflow" / source "mpEvents != NULL"),
//   then `XMemCpy(mpEvents + 8*miLength, source.mpEvents, 8 * source.miLength)` (8-byte element
//   stride) and `miLength += source.miLength`, returns 1. Reached from
//   SceneManagerModule::ProcessFineQueriesDirectly.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventTriangleCollisionSphereTest>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventTriangleCollisionSphereTest>&);
