#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddForCollision.h" // InEventAddForCollision (32-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddForCollision>::Append
//   @ X360 0x827A5C78 (dossier "class:CgsSceneManager::SceneManagerIO::InEventAddForCollision>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h; this is
// the thin explicit instantiation. The X360 body matches the generic store-for-store
// (header offsets: mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL", asm li r5,0x19D==413;
//     0x827A5C8C lwz r11,0(r31); cmplwi cr6,r11,0; bne -> skip);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", li r5,0x19E==414;
//     0x827A5CC0 lwz r11,8(r30)=lSource.miLength + lwz r10,8(r31)=this.miLength, add, cmpw vs
//     lwz r9,4(r31)=this.miMaxLength; ble -> skip -- NON-gating tripwire, copy always runs);
//   * asserts lSource.mpEvents != NULL via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL", li r5,0x1E6==486; 0x827A5CF4 lwz r11,0(r30); cmplwi; bne -> skip);
//   * XMemCpy(this->mpEvents + this->miLength*32, lSource.mpEvents, lSource.miLength*32) at a
//     32-byte element stride -- 0x827A5D20 slwi r5,r29,5 (count = lSource.miLength<<5 == *32),
//     0x827A5D28 slwi r11,r11,5 (dst off = this.miLength<<5), add r3,r11,r10 == mpEvents +
//     miLength*32, lwz r4,0(r30) == lSource.mpEvents; bl XMemCpy -- modelled by the generic
//     std::memcpy(mpEvents + miLength, lpSource, sizeof(T) * liSourceLength);
//   * bumps this->miLength by lSource.miLength (0x827A5D38 lwz r11,8(r30)/r10,8(r31); add;
//     stw r11,8(r31)) and returns 1 (li r3,1).
//
// The 32-byte stride (slwi by 5) matches sizeof(InEventAddForCollision) == 32: alignas(16)
// Vector3 mPadding (12) + VolumeInstanceId mVolumeInstanceId (u64 @ +16) + CullingGroup
// mCullGroup (u32 @ +24) + muBodyState/muCacheOptions (2 bytes), padded to 32 -- the
// X360-attested stride read directly off this Append (see CgsSceneManagerIO_EventAddForCollision.h,
// already committed). This is the Append member of the EventQueue<InEventAddForCollision,1536>
// family whose Construct instantiation lives in EventQueue_InEventAddForCollision_1536.cpp;
// called from InSceneUpdateInterface::Append.
//
// X360-attested element stride (`slwi r, count, 5` == *32).
static_assert(sizeof(CgsSceneManager::SceneManagerIO::InEventAddForCollision) == 32,
              "InEventAddForCollision stride 32");

template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddForCollision>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddForCollision>&);