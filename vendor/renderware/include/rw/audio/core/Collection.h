#pragma once

// =====================================================================================
// rw::audio::core::Collection -- a block-pooled, doubly-linked node container used by
// rwaudio's TimerManager (and friends) to hand out fixed-size 12-byte link records.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. No external source and no DecFIGS DWARF exist for this
// type, so every offset below is grounded directly in the disassembly of the bodied
// methods:
//   AddCapacity @0x82B6C490   AddItem @0x82B6E1C0   Clear @0x82B6C580
//   Defragment  @0x82B6E260   Release @0x82B6C5E8   RemoveNode @0x82B64BB0
//
// The container owns a chain of NodeBlocks (each a header + an array of Node records).
// Free Nodes live on a doubly-linked free list; in-use Nodes live on a doubly-linked
// used list. Allocating a Node pops the free list and pushes the used list; releasing a
// Node does the reverse. NodeBlocks are allocated from / returned to the shared audio
// System singleton's ICoreAllocator (off_83271928, allocator slot at System+0x14;
// vtable[1] = Alloc-with-align, vtable[3] = Free).
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable for matching a
// third-party/legacy API).
//
// Layout grounded in the asm load/store displacements (this == r3):
//   +0x00  mpBlockHead   (NodeBlock*; head of the owned NodeBlock chain)
//   +0x04  mpBlockTail   (NodeBlock*; tail of the owned NodeBlock chain)
//   +0x08  miBlockCount  (int;        number of owned NodeBlocks)
//   +0x0C  mpFreeHead    (Node*;      head of the free-node list)
//   +0x10  mpUsedHead    (Node*;      head of the in-use-node list)
//   +0x14  miItemCount   (int;        number of in-use Nodes)
//   +0x18  miCapacity    (int;        number of allocated (free+used) Node slots)
//
// FLAG (rwaudio PDB reconcile, NFS ProStreet 08 X360 PDB): authoritative names confirmed.
//   PDB rw::audio::core::Collection [sizeof = 28]; offsets/order/sizeof MATCH this header.
//   The PDB groups the head fields into intrusive-list sub-structs (kept flat here, as the
//   ARTIST asm models the type flat; the sub-struct types ListQueue/ListDStack are other
//   types and are intentionally not introduced):
//     +0x00 ListQueue  mNodeBlocks { ListNode* phead; ListNode* ptail; int entries; }
//                       -> this header's mpBlockHead / mpBlockTail / miBlockCount
//     +0x0C ListDStack mFree       { ListDNode* phead; }  -> mpFreeHead
//     +0x10 ListDStack mUsed       { ListDNode* phead; }  -> mpUsedHead
//     +0x14 int        mSize                              -> renamed miItemCount->mSize
//     +0x18 int        mCapacity                          -> miCapacity (already correct)
//   Nested structs reconciled too (PDB names): Node->ItemNode, NodeBlock(header)->
//   NodeBlockHeader; mppOwner's PDB type is ItemHandle* (a 1-field { ItemNode* pNode }),
//   kept as void** here to preserve the ARTIST asm's untyped owner back-pointer stores.
// =====================================================================================

#include "types.hpp" // s32 / u32

namespace rw
{
namespace audio
{
namespace core
{

class Collection
{
public:
    // One link record handed out by the Collection (12 bytes on X360, 24 bytes on x64).
    // FLAG (rwaudio PDB reconcile): PDB name = Collection::ItemNode [sizeof = 12]
    //   { ListDNode node { ListDNode* pnext; ListDNode* pprev; }; ItemHandle* pHandle; }.
    //   Kept as flat Node here (ARTIST asm shape); mppOwner is PDB pHandle (PDB type
    //   ItemHandle* = { ItemNode* pNode }), kept void** to mirror the untyped asm stores.
    //   +0x00 mpNext  (Node*; PDB node.pnext)
    //   +0x04 mpPrev  (Node*; PDB node.pprev)
    //   +0x08 mppOwner(void**; PDB pHandle)
    struct Node
    {
        Node *mpNext;     // +0x00 (PDB node.pnext)
        Node *mpPrev;     // +0x04 (PDB node.pprev)
        void **mppOwner;  // +0x08 (PDB pHandle, type ItemHandle*)
    };

    // A pool block: an 8-byte X360 header (naturally 16 bytes on x64) followed by
    // miNodeCount native-width Node records.
    // FLAG (rwaudio PDB reconcile): the 8-byte header is PDB Collection::NodeBlockHeader
    //   [sizeof = 8] { ListNode node { ListNode* pnext; }; int size; }; the trailing
    //   maNodes[] array is the ARTIST asm's carved Node slots (block + 8).
    //   +0x00 mpNextBlock (NodeBlock*; PDB node.pnext)
    //   +0x04 miNodeCount (int;        PDB size)
    //   +0x08 maNodes[]   (Node[];     the carved Node slots)
    struct NodeBlock
    {
        NodeBlock *mpNextBlock; // +0x00 (PDB node.pnext)
        s32 miNodeCount;        // +0x04 (PDB size)
        Node maNodes[1];        // +0x08 (flexible; miNodeCount entries)
    };

    // Grow the pool by `count` free Node slots (allocates one NodeBlock and threads its
    // Nodes onto the free list). Returns 0 on success, 1 on allocation failure.
    // (IDA renders `this`/return as int; the asm r3 contract is authoritative.)
    static int AddCapacity(Collection *self, int count);

    // Pop a free Node, push it onto the used list, and wire `outOwner` <-> Node. Grows
    // the pool by one when the free list is empty. Returns 0 on success, non-zero on a
    // failed grow.
    static int AddItem(Collection *self, void **outOwner);

    // Release every in-use Node back to the free list (clears each owner back-pointer).
    static Collection *Clear(Collection *self);

    // Compact the pool: if the head NodeBlock's Nodes all fit in the remaining capacity,
    // migrate its live Nodes into other blocks and free the head block. Returns 1 when a
    // block was reclaimed, 0 otherwise.
    static int Defragment(Collection *self);

    // Free every owned NodeBlock and zero the container back to empty.
    static Collection *Release(Collection *self);

    // Unlink `node` from the used list and push it back onto the free list.
    static Collection *RemoveNode(Collection *self, Node *node);

    // FLAG (rwaudio PDB reconcile): mNodeBlocks{phead,ptail,entries} flattened here.
    NodeBlock *mpBlockHead; // +0x00  (PDB mNodeBlocks.phead)
    NodeBlock *mpBlockTail; // +0x04  (PDB mNodeBlocks.ptail)
    s32 miBlockCount;       // +0x08  (PDB mNodeBlocks.entries)
    Node *mpFreeHead;       // +0x0C  (PDB mFree.phead)
    Node *mpUsedHead;       // +0x10  (PDB mUsed.phead)
    s32 mSize;              // +0x14  (PDB mSize; was miItemCount)
    s32 mCapacity;          // +0x18  (PDB mCapacity)
};

} // namespace core
} // namespace audio
} // namespace rw
