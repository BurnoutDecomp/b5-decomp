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
CGS_STATIC_ASSERT(offsetof(Collection::NodeBlock, node) == 0, "block link is the first member");

bool Collection::AddCapacity(Collection *self, int count)
{
    const int slots = count + self->mCapacity;
    NodeBlock *block = static_cast<NodeBlock *>(off_83271928->mpAllocator->Alloc(
        offsetof(NodeBlock, maNodes) + sizeof(ItemNode) * static_cast<size_t>(slots),
        "rw::audio::core::Collection: NodeBlock", 1, 16, 0));
    if (!block)
        return true;

    block->size = slots;
    self->mNodeBlocks.PushTail(&block->node);
    for (int i = 0; i < slots; ++i)
    {
        ItemNode *item = &block->maNodes[i];
        item->pHandle = 0;
        self->mFree.Push(&item->node);
    }
    self->mCapacity += slots;
    return false;
}

bool Collection::AddItem(ItemHandle *handle)
{
    bool failed = false;
    if (mFree.IsEmpty())
        failed = AddCapacity(this, mSize + 1);

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

void Collection::RemoveNode(Collection *self, ItemNode *item)
{
    self->mUsed.Remove(&item->node);
    self->mFree.Push(&item->node);
    --self->mSize;
}

void Collection::Clear(Collection *self)
{
    while (ItemNode *item = self->GetNodes())
    {
        ItemHandle *handle = item->pHandle;
        if (handle)
        {
            item = handle->pNode;
            handle->pNode = 0;
            item->pHandle = 0;
        }
        RemoveNode(self, item);
        // ARTIST also decrements mSize after RemoveNode at 0x82B6C5BC.
        --self->mSize;
    }
}

bool Collection::Defragment(Collection *self)
{
    NodeBlock *block = reinterpret_cast<NodeBlock *>(self->mNodeBlocks.GetHead());
    if (!block || !block->size || block->size > self->mCapacity - self->mSize)
        return false;

    // Remove free nodes from the block before moving its live handles.
    for (int i = 0; i < block->size; ++i)
    {
        ItemNode *item = &block->maNodes[i];
        if (!item->pHandle)
            self->mFree.Remove(&item->node);
    }

    for (int i = 0; i < block->size; ++i)
    {
        ItemNode *item = &block->maNodes[i];
        ItemHandle *handle = item->pHandle;
        if (handle)
        {
            ItemNode *stored = handle->pNode;
            handle->pNode = 0;
            stored->pHandle = 0;
            RemoveNode(self, stored);
            self->mFree.Remove(&item->node);
            self->AddItem(handle);
        }
    }

    self->mNodeBlocks.Pop();
    self->mCapacity -= block->size;
    off_83271928->mpAllocator->Free(block, 0);
    return true;
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

} // namespace core
} // namespace audio
} // namespace rw
