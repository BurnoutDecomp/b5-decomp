#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                                 // CgsModule::BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"        // TriangleCacheManagerIO::InEventAddToCache (8-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache>::Append
//   @ 0x827A69C0   (ledger id: class:CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::Append body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. Merges
// lSource onto the tail (three non-gating assert tripwires; header offsets mpEvents @ 0,
// miMaxLength @ 4, miLength @ 8):
//   * assert mpEvents != NULL (CgsBaseEventQueue.h:413, li r5,0x19D==413) -- 0x827A69D4
//     lwz r11,0(r31); bne skip.
//   * assert no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", li r5,0x19E==414)
//     -- 0x827A6A08 lwz r11,8(r30)=lSource.miLength + lwz r10,8(r31)=this.miLength, add,
//     cmpw vs lwz r9,4(r31)=this.miMaxLength; ble skip.
//   * assert lSource.mpEvents != NULL via GetQueueStartPointer (CgsBaseEventQueue.h:486,
//     li r5,0x1E6==486) -- 0x827A6A3C lwz r11,0(r30); bne skip.
//   * XMemCpy(this->mpEvents + this->miLength*8, lSource.mpEvents, lSource.miLength*8) at an
//     8-byte element stride -- 0x827A6A68 slwi r5,r29,3 (count = lSource.miLength<<3 == *8),
//     slwi r11,r11,3 (dst off = this.miLength<<3), add r3,r11,r10 == mpEvents + miLength*8,
//     lwz r4,0(r30) == lSource.mpEvents; bl XMemCpy.
//   * bumps this->miLength by lSource.miLength (0x827A6A80 lwz r11,8(r30)/r10,8(r31); add;
//     stw r11,8(r31)) and returns 1 (li r3,1).
//
// Element stride 8 (slwi ...,3 on both count and dst) matches sizeof(InEventAddToCache) == 8
// (CgsModule::Event {} base + s32 miCacheSlot + f32 mfCacheSphereRadius; see
// CgsTriangleCacheManagerIO.h, already committed). Sibling AddEvent instantiation @ 0x825E4620
// lives in BaseEventQueue_InEventAddToCache_AddEvent.cpp. Reached from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Append.
//
// X360-attested element stride (`slwi r, count, 3` == *8).
static_assert(sizeof(CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache) == 8,
              "InEventAddToCache stride 8");

template bool
CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache>&);
