#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h" // SceneQueryInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"       // TriangleCacheInterface
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"  // TriangleCacheManager / CacheSlot

// =============================================================================
// Small SceneManagerIO interface members reconstructed from BURNOUT_X360_ARTIST.XEX
// (semantic parity, not byte match):
//   * SceneQueryInterface::HasData                        @ 0x82204E48
//   * TriangleCacheInterface::GetNumCachedTriangleBatches @ 0x82277880
// =============================================================================

// -----------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::HasData (X360 0x82204E48).
// OR-chain short-circuit over 7 of this object's CgsModule::BaseEventQueue<T>* members,
// at byte offsets 0x00, 0x04, 0x10, 0x14, 0x18, 0x1C, 0x20 (offsets 0x08/0x0C are NOT
// tested -- a gap between the 0x04 clause and the 0x10 clause). For each member the asm
// does `lwz r11,disp(r3); cmplwi r11,0; beq->0; lwz r11,8(r11); cmpwi r11,0; bne->1, else 0`.
// Offset +8 within BaseEventQueue<T> is miLength (layout mpEvents@0, miMaxLength@4,
// miLength@8 -- CgsBaseEventQueue.h), so each clause is `ptr != nullptr && ptr->GetLength() != 0`.
// Each clause is short-circuited: once a prior clause produced 1, control skips straight past
// the remaining tests to the true result. Returns true as soon as any tested queue is non-null
// and non-empty; false only if every tested queue is null or empty. No asserts in this function.
// -----------------------------------------------------------------------------
bool CgsSceneManager::SceneManagerIO::SceneQueryInterface::HasData()
{
    bool lbHasData = (mpFineLineTestQueue != nullptr) && (mpFineLineTestQueue->GetLength() != 0);

    if (!lbHasData)
    {
        lbHasData = (mpFineLineTestNearestQueue != nullptr) && (mpFineLineTestNearestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpFineVolumeTestDeepestQueue != nullptr) && (mpFineVolumeTestDeepestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpQueueAt0x14 != nullptr) && (mpQueueAt0x14->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpQueueAt0x18 != nullptr) && (mpQueueAt0x18->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpQueueAt0x1C != nullptr) && (mpQueueAt0x1C->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpQueueAt0x20 != nullptr) && (mpQueueAt0x20->GetLength() != 0);
    }

    return lbHasData;
}

// -----------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::TriangleCacheInterface::GetNumCachedTriangleBatches
//   @ X360 0x82277880.
// Asserts the cache manager has been set up (same "mpTriangleCacheManager != NULL" rodata
// as the sibling GetCache, no trailing newline), then returns the cached slot's
// miNumCachedTriangleBatches for the given slot index (the X360 reads
// mpaCachedObjectSlots[index].miNumCachedTriangleBatches off the manager at +0x04 with the
// 48-byte CacheSlot stride and the +0x28 field).
// -----------------------------------------------------------------------------
s32 CgsSceneManager::SceneManagerIO::TriangleCacheInterface::GetNumCachedTriangleBatches(
    s32 liCacheSlotIndex) const
{
    CGS_ASSERT(mpTriangleCacheManager != nullptr, "mpTriangleCacheManager != NULL");
    return mpTriangleCacheManager->mpaCachedObjectSlots[liCacheSlotIndex].miNumCachedTriangleBatches;
}
