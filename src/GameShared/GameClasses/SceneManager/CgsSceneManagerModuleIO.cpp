#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h" // SceneQueryInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"       // TriangleCacheInterface
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"  // TriangleCacheManager / CacheSlot
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                     // SceneManagerIO::OutputBuffer

namespace
{
    const char KAC_SCENE_MANAGER_IO_ARTIST_FILE[] =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\CgsSceneManagerModuleIO.cpp";
}

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
    bool lbHasData = (mpFineLineTestQueue != 0) && (mpFineLineTestQueue->GetLength() != 0);

    if (!lbHasData)
    {
        lbHasData = (mpFineLineTestNearestQueue != 0) && (mpFineLineTestNearestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpFineVolumeTestDeepestQueue != 0) && (mpFineVolumeTestDeepestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpFineVolumeTestQueue != 0) && (mpFineVolumeTestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpTriangleCollisionLineTestQueue != 0) && (mpTriangleCollisionLineTestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpTriangleCollisionLineTestNearestQueue != 0) && (mpTriangleCollisionLineTestNearestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpTriangleCollisionSphereTestQueue != 0) && (mpTriangleCollisionSphereTestQueue->GetLength() != 0);
    }

    return lbHasData;
}

// (TriangleCacheInterface::GetNumCachedTriangleBatches @ 0x82277880 is header-inline, as in the
// original: see CgsSceneManagerIO_TriangleCache.h.)

// ARTIST 0x828C7CA0 prepares the results queue, clears all fixed queues,
// clears the results data, then resets all three fixed queues again.
void CgsSceneManager::SceneManagerIO::OutputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();                        // *(this) = 1

    mResultsQueue.Construct();                               // this + 4
    mPotentialContactQueue.Construct();                      // this + 32800
    mErrorQueue.Construct();                                 // this + 199744
    mOverlapPairsQueue.Construct();                          // this + 196656

    mTriangleCacheInterface.mpTriangleCacheManager = 0;      // this + 217164

    const bool lbResultsQueuePrepared = mResultsQueue.Prepare();
    CGS_ASSERT(lbResultsQueuePrepared,
               CGS_ASSERT_MESSAGE_AT("mResultsQueue.Prepare()", KAC_SCENE_MANAGER_IO_ARTIST_FILE, 224));

    mPotentialContactQueue.Clear();
    mErrorQueue.Clear();
    mOverlapPairsQueue.Clear();

    mResultsQueue.Clear();
    mPotentialContactQueue.Clear();
    mErrorQueue.Clear();
    mOverlapPairsQueue.Clear();
}

// -----------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::OutputBuffer::Destruct (X360 0x828BAD38).
//
// ARTIST clears the error and contact queues before releasing mResultsQueue,
// then clears all three counts before Destruct and clears overlap pairs again afterwards.
void CgsSceneManager::SceneManagerIO::OutputBuffer::Destruct()
{
    mErrorQueue.Clear();
    mPotentialContactQueue.Clear();

    const bool lbResultsQueueReleased = mResultsQueue.Release();
    CGS_ASSERT(lbResultsQueueReleased,
               CGS_ASSERT_MESSAGE_AT("mResultsQueue.Release()", KAC_SCENE_MANAGER_IO_ARTIST_FILE, 256));

    mErrorQueue.Clear();
    mPotentialContactQueue.Clear();
    mOverlapPairsQueue.Clear();

    mResultsQueue.Destruct();
    mOverlapPairsQueue.Clear();

    mTriangleCacheInterface.mpTriangleCacheManager = 0;

    CgsModule::IOBuffer::Destruct();
}
