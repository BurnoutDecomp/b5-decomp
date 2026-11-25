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
//   Defragment          @0x82B6EC08 -- from the IDA DB (scratchpad/waveF/defrag_dump.txt)
// See TimerManager.h for the byte layout and TimerHandle.h / Collection.h for the record
// types. Every method is scalar pointer/list manipulation -- no SIMD, no DSP.
//
// The two `x ? x : 0` idioms the pseudocode shows for the used-list head and a node's
// owner back-pointer are identities (clrlwi r,r,0 is a plain move on both branches), so
// they are de-optimised to a direct load here.
// =====================================================================================

#include "rw/audio/core/TimerManager.h"

#if defined(_XBOX) && defined(_MSC_VER)
// Size optimization reproduces ARTIST AddTimer at 0x82B6EB88 and Defragment at 0x82B6EC08.
#pragma optimize("s", on)
#endif

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
TimerManager::TimerManager()
    : mfCallbackTime(-1.0f), mpDeferredRemoveNode(0)
{
}

// -------------------------------------------------------------------------------------
// AddTimer @0x82B6EB88
// Register `handle` in Collection `collectionIndex`. If that Collection has no capacity
// yet, grow it by 74 slots first. Allocate a Node (wired to the handle's owner slot);
// on success fill the handle's callback/context/name, record the Collection index in the
// stage byte, stash the visibility byte, and zero the cycle count.
// The audio PDB gives AddItem and AddTimer boolean results. ARTIST tests
// AddItem's result at 0x82B6EBD8 before writing the handle.
// -------------------------------------------------------------------------------------
bool TimerManager::AddTimer(TimerHandle *handle, TimerCallback const callback, void *context,
                            const char *name, TimerStage stage, TimerVisibility visibility)
{
    Collection *pCollection = &maCollections[stage];

    if (pCollection->mCapacity == 0)
        pCollection->AddCapacity(74);

    const bool failed = pCollection->AddItem(&handle->mItemHandle);
    if (!failed)
    {
        handle->mpCallback = callback;
        handle->mpContext = context;
        handle->mpName = name;
        handle->mStage = static_cast<u8>(stage);
        handle->mTimerVisibility = static_cast<u8>(visibility);
        handle->mCpuTicks = 0;
    }
    return failed;
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
    Collection::ItemNode *pNode = pCollection->GetNodes();

    while (pNode)
    {
        u32 startCycle = GetCpuCycle();

        // TimerHandle starts with the ItemHandle the collection registered.
        TimerHandle *pHandle = reinterpret_cast<TimerHandle *>(pNode->pHandle);

        self->mpDeferredRemoveNode = 0;    // +0x44 = 0
        f32 fTime = self->mfCallbackTime;        // +0x38
        self->mpCurrentHandle = pHandle;         // +0x3C = current handle

        pHandle->mpCallback(pHandle->mpContext, fTime);

        Collection::ItemNode *pDeferred = self->mpDeferredRemoveNode;
        self->mpCurrentHandle = 0;         // +0x3C = 0
        pNode = reinterpret_cast<Collection::ItemNode *>(pNode->node.pnext);

        if (pDeferred)
        {
            self->maCollections[self->miCurrentCollection].RemoveNode(pDeferred);
            self->mpDeferredRemoveNode = 0; // +0x44 = 0
        }
        else
        {
            pHandle->mCpuTicks = GetCpuCycle() - startCycle;
        }
    }
}

// -------------------------------------------------------------------------------------
// Defragment @0x82B6EC08
// Compact both owned Collections. Verified from the IDA database (the per-address JSON
// export is missing -- an exporter gap, not a nameless function;
// scratchpad/waveF/defrag_dump.txt):
//   mr r30,r3 / li r31,2 / loop: mr r3,r30; bl Collection::Defragment;
//   addic. r31,r31,-1; addi r30,r30,0x1C; bne loop
// X360-LITERAL TRAP: the 0x1C stride IS the console sizeof(Collection); on the host that
// type is wider, so the pair is indexed by name instead. The X360 r3 return is the last
// Collection::Defragment result, dead at the only call site (System::ExecuteCommands
// passes it straight into GetCpuCycle, which ignores its argument) -- modelled void.
// -------------------------------------------------------------------------------------
void TimerManager::Defragment()
{
    for (int i = 0; i < 2; ++i)
        maCollections[i].Defragment();
}

} // namespace core
} // namespace audio
} // namespace rw
