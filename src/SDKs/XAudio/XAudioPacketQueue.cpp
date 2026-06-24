// ===========================================================================
// XAUDIO::CPacketQueue -- member bodies reconstructed from
// BURNOUT_X360_ARTIST.XEX. The PowerPC asm is authoritative for every store; see
// XAudioPacketQueue.h for the reconstructed layout. No Feb-2007 leak source and
// no DWARF exist for this TU.
// ===========================================================================

#include "SDKs/XAudio/XAudioPacketQueue.h"

namespace XAUDIO
{

// @ 0x8296C4A8 -- store-for-store:
//   li r10,0 ; stw r3,0(r3) ; addi r11,r3,0xC ; stw r3,4(r3) ; stw r10,8(r3)
//   stw r10,8(r11) ; stw r11,0(r11) ; stw r11,4(r11)
// i.e. list A head (this+0) made self-circular with base 0, and list B head
// (this+0xC) made self-circular with base 0. The queue object is the sentinel
// node of both lists, so each head's next/prev point at the head itself.
CPacketQueue::CPacketQueue()
{
    XAudioListLink* lpSelf     = reinterpret_cast<XAudioListLink*>(this);          // r3
    XAudioListLink* lpFreeHead = reinterpret_cast<XAudioListLink*>(&mFreeList);    // r3+0xC

    // list A head @ +0x00 -- self-circular, base 0.
    mInUseList.mpNext = lpSelf;     // stw r3,0(r3)
    mInUseList.mpPrev = lpSelf;     // stw r3,4(r3)
    mInUseList.mpBase = 0;          // stw r10,8(r3)

    // list B head @ +0x0C -- self-circular, base 0.
    mFreeList.mpBase  = 0;          // stw r10,8(r11)
    mFreeList.mpNext  = lpFreeHead; // stw r11,0(r11)
    mFreeList.mpPrev  = lpFreeHead; // stw r11,4(r11)
}

// @ 0x8296C3E8 -- allocate uCount * 0x78 bytes through the manager's vtable slot
// at +0x14, store the pool at +0x18, then push each 0x78-byte packet onto the
// tail of the free list (list B @ +0x0C).
//
// Store-for-store from the asm:
//   clrlwi r30,r4,24            count = (u8)a2
//   beq (count==0) -> epilogue  (returns r3 == this, unchanged)
//   lwz r11,0(r5) ; lwz r11,0x14(r11) ; mtctr ; bctrl   pool = (*(*a3+0x14))(a3, 0x78*count)
//   stw r3,0x18(r31)            mpPool = pool
//   loop i in [0,count):  node = pool + i*0x78
//     stw node,0(node) ; lwz t,0(node) ; stw t,4(node)        node->link self-init (overwritten below)
//     r11 = mFreeList.mpBase(+0x14) ; node2 = i*0x78 + r11 + pool                (== node; base is 0)
//     stw &mFreeList,0(node2)         node->mpNext = &mFreeList   (v4+3 == this+0xC)
//     stw mFreeList.mpPrev,4(node2)   node->mpPrev = mFreeList.mpPrev (old tail)
//     stw node2,4(&mFreeList)         mFreeList.mpPrev = node
//     stw node2,0(old tail)           (old tail)->mpNext = node
void* CPacketQueue::Initialize(u8 uCount, IXAudioMemoryManager* apManager)
{
    if (uCount == 0)
        return this; // zero-count early-out: r3 (this) returned unchanged

    // Allocate the pool through the manager's +0x14 vtable slot.
    void* lpPool = apManager->mpVTable->mpAllocate(apManager,
                                                   static_cast<u32>(0x78u * uCount));
    mpPool = static_cast<u8*>(lpPool);

    u32 luOffset = 0; // running byte offset into the pool (r10)
    for (u8 i = uCount; i != 0; --i)
    {
        // First-part self-init of the node's link (asm writes node->[0]=node,
        // node->[4]=node->[0]); these are overwritten by the list-append below
        // because mFreeList.mpBase is 0 (so the two computed node addresses
        // coincide). Reproduced for fidelity to the X360 store sequence.
        XAudioListLink* lpNode = reinterpret_cast<XAudioListLink*>(mpPool + luOffset);
        lpNode->mpNext = lpNode;
        lpNode->mpPrev = lpNode->mpNext;

        // Locate the link via the free-list head's base (== 0 here): the X360
        // computes node = offset + mFreeList.mpBase + pool.
        u8* lpBase = reinterpret_cast<u8*>(mFreeList.mpBase); // lwz 8(this+0xC) == +0x14
        XAudioListLink* lpLink =
            reinterpret_cast<XAudioListLink*>(luOffset + lpBase + reinterpret_cast<uintptr_t>(mpPool));

        luOffset += 0x78; // advance to the next 0x78-byte slot

        // Push onto the tail of the circular free list (sentinel == &mFreeList).
        XAudioListLink* lpFreeHead = reinterpret_cast<XAudioListLink*>(&mFreeList);
        lpLink->mpNext = lpFreeHead;        // node->mpNext = &mFreeList
        lpLink->mpPrev = mFreeList.mpPrev;  // node->mpPrev = old tail
        mFreeList.mpPrev = lpLink;          // mFreeList.mpPrev = node
        lpLink->mpPrev->mpNext = lpLink;    // old tail -> mpNext = node
    }

    return lpPool;
}

} // namespace XAUDIO
