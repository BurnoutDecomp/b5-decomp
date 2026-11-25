// RenderWare audio collections allocate blocks through the shared System
// allocator. ARTIST calls its virtual Alloc and Free directly.

#if defined(_XBOX) && defined(_MSC_VER)
// Size optimization reproduces the full ARTIST Release body at 0x82B6C5E8.
#pragma optimize("s", on)
#endif

#include "rw/audio/core/Collection.h"
#include "rw/audio/core/PlugIn.h"
#include "coreallocator/icoreallocator_interface.h"

#include <cstddef>

namespace rw
{
namespace audio
{
namespace core
{

extern "C" System *off_83271928;

CGS_STATIC_ASSERT(offsetof(Collection::ItemNode, node) == 0, "item link is the first member");
CGS_STATIC_ASSERT(offsetof(Collection::NodeBlockHeader, node) == 0, "block link is the first member");

bool Collection::AddCapacity(int count)
{
    bool failed = true;
    const int slots = count + mCapacity;
    NodeBlockHeader *block = static_cast<NodeBlockHeader *>(off_83271928->mpAllocator->Alloc(
        sizeof(NodeBlockHeader) + sizeof(ItemNode) * static_cast<size_t>(slots),
        "rw::audio::core::Collection: NodeBlock", 1, 16, 0));
    if (block)
    {
        block->size = slots;
        mNodeBlocks.PushTail(&block->node);
        ItemNode *items = block->GetNodeArray();
        for (int i = 0; i < slots; ++i)
        {
            items[i].pHandle = 0;
            mFree.Push(&items[i].node);
        }
        mCapacity += slots;
        failed = false;
    }
    return failed;
}

bool Collection::AddItem(ItemHandle *handle)
{
    bool failed = false;
    if (mFree.IsEmpty())
        failed = AddCapacity(mSize + 1);

    if (!failed)
    {
        ItemNode *item = reinterpret_cast<ItemNode *>(mFree.Pop());
        item->pHandle = handle;
        handle->pNode = item;
        mUsed.Push(&item->node);
        ++mSize;
    }
    return failed;
}

void Collection::RemoveNode(ItemNode *item)
{
    mUsed.Remove(&item->node);
    mFree.Push(&item->node);
    --mSize;
}

void Collection::Clear()
{
    while (!mUsed.IsEmpty())
    {
        ItemNode *item = GetNodes();
        ItemHandle *handle = item->pHandle;
        if (handle)
            item = RemoveItemDeferred(handle);
        RemoveNode(item);
        // ARTIST also decrements mSize after RemoveNode at 0x82B6C5BC.
        --mSize;
    }
}

bool Collection::Defragment()
{
    bool reclaimed = false;
    NodeBlockHeader *block = reinterpret_cast<NodeBlockHeader *>(mNodeBlocks.GetHead());
    // There must be another block to receive the live handles.
    if (block && block->node.GetNext() && block->size <= mCapacity - mSize)
    {
        ItemNode *items = block->GetNodeArray();

        // Remove free nodes from the block before moving its live handles.
        for (int i = 0; i < block->size; ++i)
        {
            if (!items[i].pHandle)
                mFree.Remove(&items[i].node);
        }

        for (int i = 0; i < block->size; ++i)
        {
            ItemHandle *handle = items[i].pHandle;
            if (handle)
            {
                RemoveNode(RemoveItemDeferred(handle));
                mFree.Remove(&items[i].node);
                AddItem(handle);
            }
        }

        mNodeBlocks.Pop();
        mCapacity -= block->size;
        off_83271928->mpAllocator->Free(block, 0);
        reclaimed = true;
    }
    return reclaimed;
}

void Collection::Release()
{
    ListNode *block = mNodeBlocks.Pop();
    if (block)
    {
        do
        {
            off_83271928->mpAllocator->Free(block, 0);
            block = mNodeBlocks.Pop();
        } while (block);
    }

    mFree.Reset();
    mUsed.Reset();
    mNodeBlocks.Reset();
    mSize = 0;
    mCapacity = 0;
}

// The original Xbox 360 rwAudio common object owns both Collection and TimerManager.
// Keep these callers with RemoveNode and Clear so XDK can track their register use.
void TimerManager::Release()
{
    Collection *pCollection = &maCollections[0];
    int i = 2;
    do
    {
        pCollection->Clear();
        pCollection->Release();
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
void TimerManager::RemoveTimer(TimerHandle *handle)
{
    if (handle == mpCurrentHandle)
    {
        const u32 stage = handle->mStage;
        // Deferred self-remove: hand the node off to ExecuteTimers.
        miCurrentCollection = stage; // +0x40
        mpDeferredRemoveNode = maCollections[stage].RemoveItemDeferred(&handle->mItemHandle);
    }
    else if (handle->mStage != TIMERSTAGE_NONE)
    {
        const u32 stage = handle->mStage;
        Collection &collection = maCollections[stage];
        collection.RemoveNode(collection.RemoveItemDeferred(&handle->mItemHandle));
    }

    handle->mCpuTicks = 0;
    handle->mStage = TIMERSTAGE_NONE;
}

} // namespace core
} // namespace audio
} // namespace rw
