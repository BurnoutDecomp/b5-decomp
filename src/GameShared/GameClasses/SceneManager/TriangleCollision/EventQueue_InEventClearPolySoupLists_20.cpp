#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                       // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManagerIO_Events.h" // InEventClearPolySoupLists element (4B)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventClearPolySoupLists, 20>::Construct
//   @ 0x822E26B0
//
// Thin explicit instantiation of the generic EventQueue<T,N>::Construct (inline in
// CgsEventQueue.h): point the base queue at the inline maEvents[20] storage, set capacity
// 20, clear the live count. Reached from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventClearPolySoupLists, 20>::Construct();
