// =====================================================================================
// rw::audio::core::TimerManager bodies.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. No external source and no DecFIGS DWARF exist.
//   TimerManager (ctor) @0x82B6BED0 -- store-for-store
//   ExecuteTimers       @0x82B6BF18 -- store-for-store
//   RemoveTimer         @0x82B6DE70 -- store-for-store
//   Release             @0x82B6DEE8 -- store-for-store
//   AddTimer            @0x82B6EB88 -- store-for-store
// See TimerManager.h for the byte layout and TimerHandle.h / Collection.h for the record
// types. Every method is scalar pointer/list manipulation -- no SIMD, no DSP.
//
// The two `x ? x : 0` idioms the pseudocode shows for the used-list head and a node's
// owner back-pointer are identities (clrlwi r,r,0 is a plain move on both branches), so
// they are de-optimised to a direct load here.
// =====================================================================================

#include "rw/audio/core/TimerManager.h"

namespace rw
{
namespace audio
{
namespace core
{

// Free-running CPU cycle counter (rw::audio::core::GetCpuCycle @todo). Its body lives in
// a platform timing TU that is not yet reconstructed; only the declaration is needed for
// the per-TU compile gate. Returns a monotonically increasing cycle count (r3).
u32 GetCpuCycle();

// -------------------------------------------------------------------------------------
// TimerManager (constructor) @0x82B6BED0
// Zero both embedded Collections (7 words / 28 bytes each), clear the deferred-remove
// slot, and seed the shared callback time to -1.0. mpCurrentHandle / miCurrentCollection
// are deliberately left uninitialised (the asm writes neither).
// -------------------------------------------------------------------------------------
TimerManager *TimerManager::TimerManager_ctor(TimerManager *self)
{
    for (int i = 0; i < 2; ++i)
    {
        Collection *pCollection = &self->maCollections[i];
        pCollection->mpBlockHead = nullptr;
        pCollection->mpBlockTail = nullptr;
        pCollection->miBlockCount = 0;
        pCollection->mpFreeHead = nullptr;
        pCollection->mpUsedHead = nullptr;
        pCollection->mSize = 0;
        pCollection->mCapacity = 0;
    }

    self->mpDeferredRemoveNode = nullptr; // +0x44 = 0
    self->mfCallbackTime = -1.0f;         // +0x38 = -1.0
    return self;
}

// -------------------------------------------------------------------------------------
// AddTimer @0x82B6EB88
// Register `handle` in Collection `collectionIndex`. If that Collection has no capacity
// yet, grow it by 74 slots first. Allocate a Node (wired to the handle's owner slot);
// on success fill the handle's callback/context/name, record the Collection index in the
// stage byte, stash the visibility byte, and zero the cycle count.
// Returns Collection::AddItem's result (0 on success, non-zero on a failed grow).
// -------------------------------------------------------------------------------------
int TimerManager::AddTimer(TimerManager *self, TimerHandle *handle, TimerCallback callback,
                           void *context, const char *name, int collectionIndex, u8 visibility)
{
    Collection *pCollection = &self->maCollections[collectionIndex];

    if (pCollection->mCapacity == 0)
        Collection::AddCapacity(pCollection, 74);

    // AddItem stores the new Node into the handle's owner slot (mpItemHandleNode @+0x00)
    // and points the Node's back-pointer at that slot.
    int result = Collection::AddItem(pCollection, &handle->mpItemHandleNode);
    if (result == 0)
    {
        handle->mpCallback = callback;
        handle->mpContext = context;
        handle->mpName = name;
        handle->mStage = static_cast<u8>(collectionIndex);
        handle->mTimerVisibility = visibility;
        handle->mCpuTicks = 0;
    }
    return result;
}

// -------------------------------------------------------------------------------------
// ExecuteTimers @0x82B6BF18
// Walk Collection `collectionIndex`'s used list. For each node: fetch its owning
// TimerHandle, publish it as the current handle, invoke its callback with the shared
// callback-time, then EITHER honour a deferred self-remove (if the callback flagged one
// via RemoveTimer) OR record the elapsed CPU cycles into the handle's mCpuTicks.
// -------------------------------------------------------------------------------------
void TimerManager::ExecuteTimers(TimerManager *self, int collectionIndex)
{
    Collection *pCollection = &self->maCollections[collectionIndex];
    Collection::Node *pNode = pCollection->mpUsedHead; // +0x10

    while (pNode)
    {
        u32 startCycle = GetCpuCycle();

        // The Node's owner back-pointer is the address of the handle's mpItemHandleNode
        // slot, which is the handle itself (that slot is at handle offset 0).
        TimerHandle *pHandle = reinterpret_cast<TimerHandle *>(pNode->mppOwner);

        self->mpDeferredRemoveNode = nullptr;    // +0x44 = 0
        f32 fTime = self->mfCallbackTime;        // +0x38
        self->mpCurrentHandle = pHandle;         // +0x3C = current handle

        pHandle->mpCallback(pHandle->mpContext, fTime);

        Collection::Node *pDeferred = self->mpDeferredRemoveNode; // +0x44 (set by RemoveTimer)
        self->mpCurrentHandle = nullptr;         // +0x3C = 0
        pNode = pNode->mpNext;                   // advance before any unlink

        if (pDeferred)
        {
            Collection::RemoveNode(&self->maCollections[self->miCurrentCollection], pDeferred);
            self->mpDeferredRemoveNode = nullptr; // +0x44 = 0
        }
        else
        {
            pHandle->mCpuTicks = GetCpuCycle() - startCycle;
        }
    }
}

// -------------------------------------------------------------------------------------
// Release @0x82B6DEE8
// Clear then release each of the two owned Collections (Clear returns the Collection,
// which Release then frees).
// -------------------------------------------------------------------------------------
void TimerManager::Release(TimerManager *self)
{
    Collection *pCollection = &self->maCollections[0];
    int i = 2;
    do
    {
        Collection::Release(Collection::Clear(pCollection));
        --i;
        ++pCollection;
    } while (i);
}

// -------------------------------------------------------------------------------------
// RemoveTimer @0x82B6DE70
// Unregister `handle`. If it is the handle currently executing, we cannot unlink its node
// yet (ExecuteTimers is walking the list) -- defer it: record the node + its Collection
// index for ExecuteTimers to finish. Otherwise, unless the handle is already free
// (stage 3), unlink its node from its Collection now. Either way the handle is reset to
// stage 3 with a zeroed cycle count.
// -------------------------------------------------------------------------------------
void TimerManager::RemoveTimer(TimerManager *self, TimerHandle *handle)
{
    u8 stage = handle->mStage; // +0x14 (Collection index while live, or 3 = free)

    if (handle == self->mpCurrentHandle)
    {
        // Deferred self-remove: hand the node off to ExecuteTimers.
        self->miCurrentCollection = stage; // +0x40
        Collection::Node *pNode = reinterpret_cast<Collection::Node *>(handle->mpItemHandleNode);
        handle->mpItemHandleNode = nullptr;
        pNode->mppOwner = nullptr;
        self->mpDeferredRemoveNode = pNode; // +0x44
    }
    else if (stage != 3)
    {
        Collection::Node *pNode = reinterpret_cast<Collection::Node *>(handle->mpItemHandleNode);
        handle->mpItemHandleNode = nullptr;
        pNode->mppOwner = nullptr;
        Collection::RemoveNode(&self->maCollections[stage], pNode);
    }

    handle->mCpuTicks = 0; // +0x10
    handle->mStage = 3;    // +0x14
}

} // namespace core
} // namespace audio
} // namespace rw
