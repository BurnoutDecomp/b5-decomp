// =====================================================================================
// CQueue -- intrusive singly-linked queue with a pre-allocated node pool, for the
// RTCMV/WMV video encoder path (used by SessionFrameEncoder and CReferenceLibrary).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CQueue::CQueue        @0x82A03FA8   (allocates miCapacity nodes onto the free list)
//   CQueue::DestroyQueue  @0x82A03C58
//   CQueue::AddElement    @0x82A03CC0
//   CQueue::RemoveElement @0x82A03DA0
//   CQueue::GetElement    @0x82A03E80
//
// Base pointer in the asm is byte-addressed (int* this with this[N] == byte offset N*4).
// Layout (24 bytes, no vtable):
//   +0x00 mpHead      head of the active list          (this[0])
//   +0x04 mpTail      tail of the active list          (this[1])
//   +0x08 mpFreeHead  head of the free-node pool        (this[2])
//   +0x0C mpFreeTail  tail of the free-node pool        (this[3])
//   +0x10 miCount     number of active elements         (this[4])
//   +0x14 miCapacity  pool size requested at construct  (this[5])
//
// The constructor allocates `miCapacity` 8-byte Node records up front via the raw XDK
// XMemAlloc (attributes 0x248C8000) and links them all onto the free list; on any
// allocation failure it calls DestroyQueue() and returns -100 through the result out-arg.
// AddElement pops a node from the free list, stores the caller's element pointer in it,
// and splices it into the active list at the requested index (0 = front, -1 = back,
// otherwise after that many nodes). GetElement walks to the requested index and returns
// the stored element. RemoveElement unlinks the element at the index (-1 = tail), returns
// its stored pointer and recycles the node. DestroyQueue frees the free list then the
// active list. The +0x04 node slot holds the caller's element pointer (AddElement stores a
// void* there, GetElement/RemoveElement read it back), so it is modelled as void*.
// =====================================================================================
#pragma once

#include "types.hpp"

// ---- raw Xbox 360 XDK heap API (platform externs) -----------------------------------
// The asm calls the XDK XMemAlloc(SIZE_T,DWORD)->LPVOID / XMemFree(LPVOID,DWORD) directly
// (r3=size/pAddress, r4=attributes), with NO `this` argument -- i.e. the raw platform API,
// matching the sibling CIntImage / CLocalHuffman rwmovie TUs.
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);

// XMemAlloc/XMemFree attribute word for this codec's queue nodes (lis 0x248C / ori 0x8000).
static const u32 KU_QUEUE_XMEM_ATTRIBUTES = 0x248C8000u;

class CQueue
{
public:
    // 8-byte pool node: intrusive `next` link plus the stored element handle.
    struct Node
    {
        Node* mpNext;      // +0x00  next node in whichever list this node is on
        void* mpElement;   // +0x04  caller-supplied element pointer/handle
    };

    // Allocates `liCapacity` pool nodes; writes 0 on success or -100 on allocation
    // failure through lpiResult. (Bodied by its own follow-on slice; declared here.)
    CQueue(s32* lpiResult, s32 liCapacity);

    // Frees the free-list and the active-list nodes. Not a destructor in the asm; it is
    // an explicit teardown also invoked from the ctor failure path.
    void DestroyQueue();

    // Inserts lpElement at liIndex (0 = front, -1 = back, otherwise after liIndex nodes).
    // Returns 1 on success, 0 if lpElement is null or liIndex exceeds the current count.
    s32 AddElement(void* lpElement, s32 liIndex);

    // Unlinks the element at liIndex (-1 = tail), stores its element pointer through
    // *lppElement, and recycles the node onto the free list. Returns 1 on success,
    // 0 (with *lppElement == 0) if liIndex is out of range.
    s32 RemoveElement(void** lppElement, s32 liIndex);

    // Reads the element at liIndex (-1 = last) into *lppElement. Returns 1 on success,
    // 0 if lppElement is null or liIndex is out of range.
    s32 GetElement(void** lppElement, s32 liIndex);

private:
    Node* mpHead;       // +0x00
    Node* mpTail;       // +0x04
    Node* mpFreeHead;   // +0x08
    Node* mpFreeTail;   // +0x0C
    s32   miCount;      // +0x10
    s32   miCapacity;   // +0x14
};
