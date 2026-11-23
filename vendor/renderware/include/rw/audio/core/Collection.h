#pragma once

// RenderWare audio's block pool. The Collection and list declarations come
// from the private audio headers in DecFIGS and the Xbox 360 audio PDB. ARTIST
// confirms the list layout in RemoveNode at 0x82B64BB0 and AddItem at 0x82B6E1C0.

#include "types.hpp"
#include "rw/audio/core/private/linklist.h"

namespace rw
{
namespace audio
{
namespace core
{

class Collection
{
public:
    struct ItemNode;

    struct ItemHandle
    {
        ItemNode *pNode;
    };

    // The audio PDB identifies the back-pointer as an ItemHandle pointer.
    // ARTIST installs both links at 0x82B6E220 and 0x82B6E224.
    struct ItemNode
    {
        ListDNode node;       // +0x00
        ItemHandle *pHandle; // +0x08
    };

    // ARTIST carves item nodes after the eight-byte NodeBlockHeader.
    struct NodeBlock
    {
        ListNode node;
        s32 size;
        ItemNode maNodes[1];
    };

    // Grow the pool by `count` free Node slots (allocates one NodeBlock and threads its
    // Nodes onto the free list). Returns true if allocation fails.
    static bool AddCapacity(Collection *self, int count);

    // Attach a free node to the handle and push it onto the used list.
    // Returns true if the pool could not grow.
    bool AddItem(ItemHandle *handle);

    // Release every in-use Node back to the free list (clears each owner back-pointer).
    static void Clear(Collection *self);

    // Compact the pool: if the head NodeBlock's Nodes all fit in the remaining capacity,
    // migrate its live Nodes into other blocks and free the head block. Returns 1 when a
    // block was reclaimed, 0 otherwise.
    static bool Defragment(Collection *self);

    // Free every owned NodeBlock and zero the container back to empty.
    void Release();

    // Unlink `node` from the used list and push it back onto the free list.
    static void RemoveNode(Collection *self, ItemNode *node);

    ItemNode *GetNodes() const
    {
        return reinterpret_cast<ItemNode *>(mUsed.GetHead());
    }

    ListQueue mNodeBlocks; // +0x00
    ListDStack mFree;      // +0x0C
    ListDStack mUsed;      // +0x10
    s32 mSize;             // +0x14
    s32 mCapacity;         // +0x18
};

} // namespace core
} // namespace audio
} // namespace rw
