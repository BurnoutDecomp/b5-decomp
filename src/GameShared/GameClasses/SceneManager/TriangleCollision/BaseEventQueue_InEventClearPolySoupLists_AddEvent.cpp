#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                                     // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManagerIO_Events.h" // InEventClearPolySoupLists element (4B)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventClearPolySoupLists>::AddEvent
//   @ 0x822C69B0
//
// Thin explicit instantiation of the generic BaseEventQueue<T>::AddEvent (inline in
// CgsBaseEventQueue.h). Appends one InEventClearPolySoupLists (X360 4-byte record: a single
// uint32_t miDummy) to the clear-poly-soup-lists input queue and returns true. The two
// overflow asserts are non-gating tripwires in the generic body. Called from
// BrnWorld::WorldEntityModule::InvalidateCollision.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventClearPolySoupLists>::AddEvent(
    const CgsSceneManager::TriangleCollisionManagerIO::InEventClearPolySoupLists& lEvent);
