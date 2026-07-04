#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                                     // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManagerIO_Events.h" // InEventAddPolySoupList element (X360 16B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList>::AddEvent
//   @ 0x822C6858
//
// Thin explicit instantiation of the generic BaseEventQueue<T>::AddEvent (inline in
// CgsBaseEventQueue.h). Appends one InEventAddPolySoupList (X360 16-byte record:
// ResourceHandle @+0, int32_t miZoneNumber @+8, bool @+12) to the add-poly-soup-list
// input queue and returns true. The two overflow asserts are non-gating tripwires in the
// generic body. Called from BrnWorld::WorldEntityModule::AddCollisionZoneToSceneManager.
// (On the 64-bit PC host ResourceHandle widens past 16B; the generic uses sizeof(T), so
// this compiles -- the X360 stride is documented, not preserved, per semantic parity.)
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList>::AddEvent(
    const CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList& lEvent);
