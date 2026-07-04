// Per-instantiation .cpp for
// CgsModule::BaseEventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList>::GetEvent.
// The generic BaseEventQueue<T>::GetEvent(int) const body is fully inline in CgsBaseEventQueue.h; this
// TU forces only the explicit member instantiation, mirroring the committed sibling
// BaseEventQueue_LoadBundleRequest_GetEvent.cpp.
//
// CgsModule::BaseEventQueue<...::InEventAddPolySoupList>::GetEvent(s32) const  @ X360 0x828ACE28
// (dossier "CgsModule::BaseEventQ" -- Hex-Rays-truncated demangled name). Called by
// CgsSceneManager::TriangleCollisionManager::ProcessAddPolySoupListEvents to walk the
// EventQueue<InEventAddPolySoupList, 20>.
//
// X360 store-for-store (asm at 0x828ACE28): asserts mpEvents!=NULL (272), liIndex<GetLength() (274),
// liIndex>=0 (275), then returns &mpEvents[liIndex] via slwi r11,r29,4 (liIndex*16) + mpEvents@0.
// The 16-byte stride is sizeof(InEventAddPolySoupList): ResourceHandle mPolySoupListHandle (X360 8B)
// @+0 + s32 miZoneNumber @+8 + bool mbRebuildSpacialPartitioning @+0xC, padded to 0x10 -- the element
// homed in CgsTriangleCollisionManagerIO_Events.h. The Hex-Rays `int` return is the ABI-returned T&
// pointer; the DWARF gives the real const T&.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManagerIO_Events.h"  // InEventAddPolySoupList (16B element)

template const CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList&
CgsModule::BaseEventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList>::GetEvent(s32) const;
