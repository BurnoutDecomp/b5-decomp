#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::GetEvent const (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventReplaceDynamicVolume.h" // InEventReplaceDynamicVolume (144B, already committed)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>::GetEvent(s32) const  @ 0x828ACAD0
// Explicit instantiation only -- the checked const accessor body lives inline in the committed
// CgsBaseEventQueue.h (const T& GetEvent(s32) const, line 78). Asserts mpEvents != NULL (:272),
// liIndex < GetLength() (:274), liIndex >= 0 (:275), then returns &mpEvents[liIndex] via
// `slwi r30,3; add r30; slwi r,4` == liIndex*(1+8)*16 == liIndex*144 + mpEvents. The 144-byte
// stride == sizeof(InEventReplaceDynamicVolume), matching the committed AddEvent (0x822AB670) /
// Append instantiations for this type. mpEvents @ +0, miLength @ +8.
// Called by CgsSceneManager::SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules.
template const CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume&
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume>::GetEvent(s32) const;
