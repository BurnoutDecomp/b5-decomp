// =====================================================================================
// rw::audio::core::Collection bodies.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. No external source and no DecFIGS DWARF exist.
//   AddCapacity @0x82B6C490 -- store-for-store
//   AddItem     @0x82B6E1C0 -- store-for-store
//   Clear       @0x82B6C580 -- store-for-store
//   Defragment  @0x82B6E260 -- store-for-store
//   Release     @0x82B6C5E8 -- store-for-store
//   RemoveNode  @0x82B64BB0 -- store-for-store
// See Collection.h for the byte-exact Collection / NodeBlock / Node layout.
//
// All six methods are scalar pointer/list manipulation -- no SIMD, no DSP, no codec.
// NodeBlocks are taken from / returned to the shared audio System singleton's
// ICoreAllocator (off_83271928 -> System; allocator pointer at System+0x14; the asm
// dispatches vtable[1] = Alloc(size,name,flags,align,offset) and vtable[3] = Free).
// =====================================================================================

#include "rw/audio/core/Collection.h"

#include "coreallocator/icoreallocator_interface.h" // EA::Allocator::ICoreAllocator
#include <cstddef>                                    // offsetof

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
// The shared rwaudio System singleton (off_83271928). Its full type lives in the System
// sub-system TU; the only field the Collection touches is the ICoreAllocator pointer at
// +0x14, so it is reached through this minimal reconstructed view. FLAGGED: the object
// itself is defined/initialised elsewhere (the System allocator TU), the same singleton
// PlugIn::CreateInstance installs into PlugIn::mpSystem.
struct AudioSystemView
{
    char mHeader[0x14];                          // +0x00 .. +0x13 -- opaque header
    EA::Allocator::ICoreAllocator *mpAllocator;  // +0x14 -- block allocator slot
};

extern "C" AudioSystemView *off_83271928; // pointer-to-System global (System TU owns it)

inline EA::Allocator::ICoreAllocator *Allocator()
{
    return off_83271928->mpAllocator;
}
} // namespace

// -------------------------------------------------------------------------------------
// AddCapacity @0x82B6C490
// Allocate one NodeBlock sized for (mCapacity + count) Node slots [the asm sizes the
// block by the *new total* capacity, not just `count`], thread it onto the owned block
// chain, carve its Nodes onto the free list, and bump mCapacity by the new slot count.
// Returns 0 on success, 1 if the allocation failed.
// -------------------------------------------------------------------------------------
int Collection::AddCapacity(Collection *self, int count)
{
    int result = 1; // r29 = 1 (failure default)

    // v4 (r30) = count + mCapacity -- the number of Node slots the new block holds.
    const int slots = count + self->mCapacity;

    // X360 requests 12 bytes per Node + its 8-byte NodeBlock header. Express the
    // same native layout through the C++ types so x64 reserves its 24-byte Nodes
    // and 16-byte aligned header instead of overwriting the allocator's next
    // boundary tag. Tag/flags/alignment match the console allocation.
    NodeBlock *block = static_cast<NodeBlock *>(Allocator()->Alloc(
        offsetof(NodeBlock, maNodes) + sizeof(Node) * static_cast<size_t>(slots),
        "rw::audio::core::Collection: NodeBlock", 1, 16, 0));

    if (block)
    {
        result = 0;
        block->miNodeCount = slots; // stw r30 @ block+4
        block->mpNextBlock = 0;     // stw 0   @ block+0

        // Thread the block onto the owned block chain (head@+0, tail@+4, count@+8).
        if (self->mpBlockHead)
            self->mpBlockTail->mpNextBlock = block; // **(self+4) = block
        else
            self->mpBlockHead = block;              // *self = block
        const int blockCount = self->miBlockCount;
        self->mpBlockTail = block;                  // *(self+4) = block
        self->miBlockCount = blockCount + 1;        // *(self+8) = count+1

        // Carve the block's Node slots onto the doubly-linked free list (head@+0x0C).
        Node *node = block->maNodes; // r11 = block + 8
        if (slots > 0)
        {
            int remaining = slots;
            do
            {
                node->mppOwner = 0;                 // stw 0 @ node+8
                Node *oldFree = self->mpFreeHead;   // r10 = *(self+0xC)
                node->mpPrev = 0;                   // stw 0 @ node+4
                node->mpNext = oldFree;             // stw r10 @ node+0
                if (self->mpFreeHead)
                    self->mpFreeHead->mpPrev = node; // *(*(self+0xC)+4) = node
                self->mpFreeHead = node;            // *(self+0xC) = node
                --remaining;
                ++node;                             // r11 += 0xC
            } while (remaining);
        }
        self->mCapacity += slots; // *(self+0x18) += slots
    }
    return result;
}

// -------------------------------------------------------------------------------------
// AddItem @0x82B6E1C0
// Ensure a free Node exists (grow by one when the free list is empty), pop it, wire it
// to the caller's back-pointer slot, and push it onto the used list. Returns 0 on
// success, the failed AddCapacity result otherwise.
// -------------------------------------------------------------------------------------
int Collection::AddItem(Collection *self, void **outOwner)
{
    int result = 0; // r29

    if (!self->mpFreeHead)
        result = AddCapacity(self, self->mSize + 1);   // asm: lwz r11,0x14(r31)=mSize; addi r4,r11,1

    if (!(result & 0xFF)) // clrlwi. r11,r3,24 -- the low byte of the AddCapacity result
    {
        Node *node = self->mpFreeHead; // r11 = *(self+0xC)
        if (node)
        {
            Node *next = node->mpNext;          // r10 = *node
            self->mpFreeHead = next;            // *(self+0xC) = node->next
            if (next)
                next->mpPrev = 0;               // *(node->next+4) = 0
        }

        node->mppOwner = outOwner; // stw a2 @ node+8
        *outOwner = node;          // stw node @ *a2

        // Push onto the used list (head@+0x10).
        Node *oldUsed = self->mpUsedHead; // r10 = *(self+0x10)
        node->mpPrev = 0;                 // stw 0 @ node+4
        node->mpNext = oldUsed;           // stw r10 @ node+0
        if (self->mpUsedHead)
            self->mpUsedHead->mpPrev = node; // *(*(self+0x10)+4) = node
        self->mpUsedHead = node;             // *(self+0x10) = node
        ++self->mSize;                       // ++*(self+0x14)
    }
    return result;
}

// -------------------------------------------------------------------------------------
// Clear @0x82B6C580
// Release every in-use Node back to the free list. For each used-list head node it first
// severs the caller's back-pointer pair (owner slot value's +8 and the owner slot
// itself) before unlinking via RemoveNode, then decrements mSize a second time
// (RemoveNode already decremented it once -- both stores target +0x14, reproduced as the
// asm wrote them).
// -------------------------------------------------------------------------------------
Collection *Collection::Clear(Collection *self)
{
    Collection *result = self;
    if (self->mpUsedHead)
    {
        do
        {
            Node *node = self->mpUsedHead;  // r4 = *(self+0x10)
            void **owner = node->mppOwner;  // r11 = *(node+8)
            if (owner)
            {
                // r4 = *owner (the value the owner stored, normally == node); clear the
                // owner slot, then clear that value's +8 owner-back-slot.
                Node *stored = static_cast<Node *>(*owner);
                *owner = 0;                 // stw 0 @ *owner
                stored->mppOwner = 0;       // stw 0 @ stored+8
                node = stored;              // r4 carried into RemoveNode
            }
            result = RemoveNode(self, node);
            --result->mSize;                // --*(self+0x14) (second decrement)
        } while (self->mpUsedHead);
    }
    return result;
}

// -------------------------------------------------------------------------------------
// Defragment @0x82B6E260
// If the head NodeBlock's Node slots all fit within the remaining spare capacity
// (mCapacity - mSize), migrate the block's live Nodes into the rest of the pool
// and free the head block. Returns 1 when the head block was reclaimed, 0 otherwise.
//
// Pass 1: drop the head block's *free* Nodes off the free list (so they will not be
//   re-handed-out from the soon-to-be-freed block).
// Pass 2: for each live Node in the head block, sever its owner back-pointer, RemoveNode
//   it (returning it to the free list -- but it still lives in this block, so also unlink
//   it from the block-local list view) and AddItem it afresh (re-homing it into a Node
//   slot from a surviving block).
// Then pop the head block off the owned block chain, shrink mCapacity by its slot count,
// and Free it.
// -------------------------------------------------------------------------------------
int Collection::Defragment(Collection *self)
{
    int result = 0; // r27

    NodeBlock *block = self->mpBlockHead; // r29 = *self
    if (!block)
        return result;
    if (block->miNodeCount == 0)
        return result;

    // Only defragment when the block's slots fit in the spare capacity.
    if (block->miNodeCount > (self->mCapacity - self->mSize))
        return result;

    Node *base = block->maNodes; // r30 = block + 8

    // Pass 1: unlink this block's *free* Nodes (mppOwner == 0) from the free list.
    {
        int i = 0; // r9
        Node *node = base; // r11
        if (block->miNodeCount > 0)
        {
            do
            {
                if (!node->mppOwner) // lwz 8(r11) == 0 -> a free slot
                {
                    if (node == self->mpFreeHead)
                        self->mpFreeHead = node->mpNext; // *(self+0xC) = *node
                    if (node->mpPrev)
                        node->mpPrev->mpNext = node->mpNext; // *(node->prev) = *node
                    if (node->mpNext)
                        node->mpNext->mpPrev = node->mpPrev; // *(node->next+4) = node->prev
                }
                ++i;
                ++node; // r11 += 0xC
            } while (i < block->miNodeCount);
        }
    }

    // Pass 2: re-home this block's *live* Nodes.
    {
        int i = 0; // r28
        Node *node = base; // r30
        if (block->miNodeCount > 0)
        {
            do
            {
                void **owner = node->mppOwner; // r9 = *(node+8)
                if (owner)
                {
                    Node *stored = static_cast<Node *>(*owner); // r4 = *owner
                    *owner = 0;            // stw 0 @ *owner
                    stored->mppOwner = 0;  // stw 0 @ stored+8
                    RemoveNode(self, stored); // bl RemoveNode (r3=self, r4=stored)

                    // Unlink `node` from the block-local list view.
                    if (node == self->mpFreeHead)
                        self->mpFreeHead = node->mpNext; // *(self+0xC) = *node
                    if (node->mpPrev)
                        node->mpPrev->mpNext = node->mpNext;
                    if (node->mpNext)
                        node->mpNext->mpPrev = node->mpPrev;

                    AddItem(self, owner); // bl AddItem (r4 = original owner slot)
                }
                ++i;
                ++node; // r30 += 0xC
            } while (i < block->miNodeCount);
        }
    }

    // Pop the head block off the owned block chain.
    if (self->mpBlockHead)
    {
        NodeBlock *next = self->mpBlockHead->mpNextBlock; // r11 = *(*self)
        self->mpBlockHead = next;                         // *self = next
        if (!next)
            self->mpBlockTail = 0;                        // *(self+4) = 0
        --self->miBlockCount;                             // --*(self+8)
    }

    self->mCapacity -= block->miNodeCount; // *(self+0x18) -= block->miNodeCount

    Allocator()->Free(block, 0); // vtable[3] Free(allocator, block, 0)
    result = 1;
    return result;
}

// -------------------------------------------------------------------------------------
// Release @0x82B6C5E8
// Free every owned NodeBlock (popping the head off the block chain before each Free) and
// zero the container back to empty.
// -------------------------------------------------------------------------------------
Collection *Collection::Release(Collection *self)
{
    Collection *result = self; // r3 return value is the (mutated) allocator/this chain

    NodeBlock *block = self->mpBlockHead; // r4 = *self (first block to free)

    // Pop the head block (this leaves r4 holding the *original* head for the first Free).
    if (block)
    {
        NodeBlock *next = block->mpNextBlock; // r11 = *block
        self->mpBlockHead = next;             // *self = next
        if (!next)
            self->mpBlockTail = 0;            // *(self+4) = 0
        --self->miBlockCount;                 // --*(self+8)
    }

    if (block)
    {
        do
        {
            Allocator()->Free(block, 0); // vtable[3] Free(allocator, block, 0)

            // Pop the next head block to free on the following iteration.
            NodeBlock *head = self->mpBlockHead; // r11 = *self
            if (head)
            {
                NodeBlock *next = head->mpNextBlock; // r10 = *head
                self->mpBlockHead = next;            // *self = next
                if (!next)
                    self->mpBlockTail = 0;           // *(self+4) = 0
                --self->miBlockCount;                // --*(self+8)
            }
            block = head; // r4 = the just-popped head (loop continues while non-null)
        } while (block);
    }

    // Zero the container back to empty (stw r29(0) into each field).
    self->mpFreeHead = 0;   // +0x0C
    self->mpUsedHead = 0;   // +0x10
    self->mpBlockHead = 0;  // +0x00
    self->mpBlockTail = 0;  // +0x04
    self->miBlockCount = 0; // +0x08
    self->mSize = 0;        // +0x14
    self->mCapacity = 0;    // +0x18
    return result;
}

// -------------------------------------------------------------------------------------
// RemoveNode @0x82B64BB0
// Unlink `node` from the used list and push it onto the free list, decrementing
// mSize.
// -------------------------------------------------------------------------------------
Collection *Collection::RemoveNode(Collection *self, Node *node)
{
    if (node == self->mpUsedHead)
        self->mpUsedHead = node->mpNext; // *(self+0x10) = *node

    if (node->mpPrev)
        node->mpPrev->mpNext = node->mpNext; // *(node->prev) = *node
    if (node->mpNext)
        node->mpNext->mpPrev = node->mpPrev; // *(node->next+4) = node->prev

    Node *oldFree = self->mpFreeHead; // r10 = *(self+0xC)
    node->mpPrev = 0;                 // stw 0 @ node+4
    node->mpNext = oldFree;           // stw r10 @ node+0
    if (self->mpFreeHead)
        self->mpFreeHead->mpPrev = node; // *(*(self+0xC)+4) = node
    self->mpFreeHead = node;             // *(self+0xC) = node
    --self->mSize;                       // --*(self+0x14)
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
