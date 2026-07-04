// Per-instantiation .cpp for
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTest>::GetEvent.
// The generic BaseEventQueue<T>::GetEvent(int) const body is fully inline in CgsBaseEventQueue.h; this
// TU forces only the explicit member instantiation, mirroring the committed sibling
// BaseEventQueue_InEventLineTestNearest_AddEvent.cpp / CgsSceneManagerIO_EventLineTestNearest.h.
//
// CgsModule::BaseEventQueue<...::InEventTriangleCollisionLineTest>::GetEvent(s32) const  @ X360 0x828AD0D0
// (dossier "CgsModule::BaseEventQue" -- Hex-Rays-truncated demangled name). Called by
// CgsSceneManager::SceneManagerModule::ProcessTriangleCollisionLineTests to walk the
// EventQueue<InEventTriangleCollisionLineTest, 256>.
//
// X360 store-for-store (asm at 0x828AD0D0): asserts mpEvents!=NULL (272), liIndex<GetLength() (274),
// liIndex>=0 (275), then returns &mpEvents[liIndex] via slwi r11,r30,1 (liIndex*2),
// add r11,r30,r11 (liIndex*3), slwi r11,r11,4 (*16 == liIndex*48), + mpEvents@0. The 48-byte stride
// is sizeof(InEventTriangleCollisionLineTest): Vector3 mLineStart (16) @+0 + Vector3 mLineEnd (16)
// @+0x10 + SceneQueryId mQueryId (4) @+0x20, alignas(16) (two Vector3 lanes) -> 0x30 -- the element
// homed in CgsSceneManagerIO_EventTriangleCollisionLineTest.h. The Hex-Rays `int` return is the
// ABI-returned T& pointer; the DWARF gives the real const T&.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventTriangleCollisionLineTest.h"  // InEventTriangleCollisionLineTest (48B element)

template const CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTest&
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTest>::GetEvent(s32) const;
