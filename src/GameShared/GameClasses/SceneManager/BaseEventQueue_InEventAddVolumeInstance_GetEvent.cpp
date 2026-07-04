// Per-instantiation .cpp for
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance>::GetEvent.
// The generic BaseEventQueue<T>::GetEvent(int) const body is fully inline in CgsBaseEventQueue.h; this
// TU forces only the explicit member instantiation, mirroring the committed sibling
// BaseEventQueue_InEventAddVolumeInstance_AddEvent.cpp.
//
// CgsModule::BaseEventQueue<...::InEventAddVolumeInstance>::GetEvent(s32) const  @ X360 0x828AC770
// Called by CgsSceneManager::SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules to walk the
// EventQueue<InEventAddVolumeInstance, 1280> (InSceneUpdateInterface::mAddVolumeInstanceQueue).
//
// X360 store-for-store (asm at 0x828AC770): asserts mpEvents!=NULL (272), liIndex<GetLength() (274),
// liIndex>=0 (275), then returns &mpEvents[liIndex] via slwi r11,r30,2 (liIndex*4),
// add r11,r30,r11 (liIndex*5), slwi r11,r11,4 (*16 == liIndex*80), + mpEvents@0. The 80-byte stride
// is sizeof(InEventAddVolumeInstance) (VolumeInstanceId 8 @+0 + VolumeId @+8 + Matrix44Affine 64
// @+0x10, alignas(16) -> 0x50), the committed element.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddVolumeInstance.h"  // InEventAddVolumeInstance (80B element)

template const CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance&
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance>::GetEvent(s32) const;
