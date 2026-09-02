#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h" // SceneQueryInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"       // TriangleCacheInterface
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"  // TriangleCacheManager / CacheSlot
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                     // SceneManagerIO::OutputBuffer

// =============================================================================
// Small SceneManagerIO interface members reconstructed from BURNOUT_X360_ARTIST.XEX
// (semantic parity, not byte match):
//   * SceneQueryInterface::HasData                        @ 0x82204E48
//   * TriangleCacheInterface::GetNumCachedTriangleBatches @ 0x82277880
//   * OutputBuffer::Construct                             @ 0x828C7CA0   (wave Q5)
//   * OutputBuffer::Destruct                              @ 0x828BAD38   (wave Q5)
// This IS the console's own CgsSceneManagerModuleIO.cpp: the two OutputBuffer bodies
// bake "...\\scenemanager\\CgsSceneManagerModuleIO.cpp" at lines 224 / 256.
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
        lbHasData = (mpFineVolumeTestQueue != nullptr) && (mpFineVolumeTestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpTriangleCollisionLineTestQueue != nullptr) && (mpTriangleCollisionLineTestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpTriangleCollisionLineTestNearestQueue != nullptr) && (mpTriangleCollisionLineTestNearestQueue->GetLength() != 0);
    }
    if (!lbHasData)
    {
        lbHasData = (mpTriangleCollisionSphereTestQueue != nullptr) && (mpTriangleCollisionSphereTestQueue->GetLength() != 0);
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

// -----------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::OutputBuffer::Construct (X360 0x828C7CA0).
//
// Store-for-store against the asm (console byte offsets in parentheses; on the host
// every one of these is reached BY NAME -- see the layout table in CgsSceneManagerIO.h):
//
//   stb   r11=1, 0(r31)                        -> CgsModule::IOBuffer::Construct()
//   bl    VariableEventQueue<32768,16>::Construct(r31+4)          -> mResultsQueue
//   bl    PotentialContact,2048>::Construct(r31+0x8020  = 32800)  -> mPotentialContactQueue
//   bl    ErrorEvent,128>::Construct     (r31+0x30C40 = 199744)   -> mErrorQueue
//   bl    OutOverlapPair,128>::Construct (r31+0x30030 = 196656)   -> mOverlapPairsQueue
//   stwx  0, r31+0x3504C (=217164)                                -> mTriangleCacheInterface
//                                                                    .mpTriangleCacheManager
//   bl    VariableEventQueue<32768,16>::Prepare(r31+4)  + the FireAssert triplet
//         ("mResultsQueue.Prepare()", CgsSceneManagerModuleIO.cpp:224)
//   stw   0, +0x8028 / +0x30C48 / +0x30038      (= the three fixed queues' miLength)
//   bl    VariableEventQueue<32768,16>::Clear(r31+4)
//   stw   0, +0x8028 / +0x30C48 / +0x30038      (again)
//
// The three fixed queues are cleared TWICE: the console emits one Clear alongside the
// results queue's Prepare and one alongside its Clear (each fixed queue's Prepare/Clear
// inline to the single `miLength = 0` store -- BaseEventQueue<T>::Clear). Both rounds are
// reproduced rather than collapsed; the store is idempotent, so this is parity, not cost.
//
// ⭐ The mpTriangleCacheManager = 0 store is the one recovered in the 2026-08-15 zero-fill
// audit: with CreateIOBuffer<T> no longer value-initialising, the slot would otherwise hold
// the previous IO-stack tenant's bytes and sail past GetCache()'s NULL tripwire.
// -----------------------------------------------------------------------------
void CgsSceneManager::SceneManagerIO::OutputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();                        // *(this) = 1

    mResultsQueue.Construct();                               // this + 4
    mPotentialContactQueue.Construct();                      // this + 32800
    mErrorQueue.Construct();                                 // this + 199744
    mOverlapPairsQueue.Construct();                          // this + 196656

    mTriangleCacheInterface.mpTriangleCacheManager = 0;      // this + 217164

    // The console CALLS Prepare and asserts its result; the call is a side effect, not a
    // diagnostic, so it is made outside the macro (CGS_ASSERT happens to evaluate its
    // condition today, but a body must not depend on that).
    const bool lbResultsQueuePrepared = mResultsQueue.Prepare();
    CGS_ASSERT(lbResultsQueuePrepared, "mResultsQueue.Prepare()");   // :224

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
// The mirror of Construct, same two-round shape (the console schedules the independent
// stores out of source order; the word indices in the Hex-Rays listing decode as):
//   a1[49938] = 0   -> +199752 = mErrorQueue.miLength            (Clear)
//   a1[8202]  = 0   -> +32808  = mPotentialContactQueue.miLength (Clear)
//   VariableEventQueue<32768,16>::Release(a1+1 == this+4) + the FireAssert triplet
//         ("mResultsQueue.Release()", CgsSceneManagerModuleIO.cpp:256)
//   a1[49938] = 0 ; a1[8202] = 0 ; a1[49166] = 0  -> +196664 = mOverlapPairsQueue.miLength
//   VariableEventQueue<32768,16>::Destruct(this+4)
//   a1[49166] = 0
//   a1[54291] = 0   -> +217164 = mTriangleCacheInterface.mpTriangleCacheManager
//   CgsModule::IOBuffer::Destruct(a1)
// i.e. each fixed queue is cleared once with the Release round and once with the Destruct
// round, exactly as in Construct.
// -----------------------------------------------------------------------------
void CgsSceneManager::SceneManagerIO::OutputBuffer::Destruct()
{
    // Same rule as Construct: Release() is a side effect, evaluated outside the macro.
    const bool lbResultsQueueReleased = mResultsQueue.Release();
    CGS_ASSERT(lbResultsQueueReleased, "mResultsQueue.Release()");   // :256

    mPotentialContactQueue.Clear();
    mErrorQueue.Clear();
    mOverlapPairsQueue.Clear();

    mResultsQueue.Destruct();
    mPotentialContactQueue.Clear();
    mErrorQueue.Clear();
    mOverlapPairsQueue.Clear();

    mTriangleCacheInterface.mpTriangleCacheManager = 0;

    CgsModule::IOBuffer::Destruct();
}
