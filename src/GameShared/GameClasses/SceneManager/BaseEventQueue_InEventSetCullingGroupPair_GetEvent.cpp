#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::GetEvent const (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetCullingGroupPair.h" // InEventSetCullingGroupPair (12B, already committed)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>::GetEvent(s32) const  @ 0x828ACB80
// Explicit instantiation only -- the checked const accessor body lives inline in the committed
// CgsBaseEventQueue.h (const T& GetEvent(s32) const, line 78). Asserts mpEvents != NULL (:272),
// liIndex < GetLength() (:274), liIndex >= 0 (:275), then returns &mpEvents[liIndex] via
// `slwi r30,1; add r30; slwi r,2` == liIndex*(1+2)*4 == liIndex*12 + mpEvents. The 12-byte
// stride == sizeof(InEventSetCullingGroupPair) (three u32 words: muGroupA/muGroupB/muEnabled),
// matching the committed AddEvent (0x822AB7D0) / Append instantiations for this type.
// mpEvents @ +0, miLength @ +8. Called by
// CgsSceneManager::SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules.
template const CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair&
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetCullingGroupPair>::GetEvent(s32) const;
