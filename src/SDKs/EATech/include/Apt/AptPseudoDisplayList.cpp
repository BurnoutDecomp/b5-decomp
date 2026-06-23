// ===========================================================================
// EATech Apt -- AptPseudoDisplayList method bodies.
//
// Reconstructed store-for-store from the X360 ARTIST.XEX pseudocode/asm:
//     AptPseudoDisplayList::AptPseudoDisplayList @ 0x82AE5B10
//     AptPseudoDisplayList::ClearList            @ 0x82AEAFC8
//     AptPseudoDisplayList::Insert               @ 0x82AEE4D0
//     AptPseudoDisplayList::Remove               @ 0x82AEB018
//     AptPseudoDisplayList::~AptPseudoDisplayList @ 0x82AECBF0
// See AptPseudoDisplayList.h for the layout derivation. No leak/DWARF body.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptPseudoDisplayList.h"

#include <cstring>   // std::memset (the head node's 5 zeroing dword stores)

// ---------------------------------------------------------------------------
// ctor @ 0x82AE5B10
//
// asm: v4 = Allocate(off_8324D808, 20); if (v4) { zero v4[0..4] } else v5 = 0;
//      *a1 = v5; a1[1] = a2.
// The head node is a bare zeroed 20-byte block (NOT a constructed AptPseudoCIH_t
// snapshot -- the ctor only stores 0 into its five dwords), so it is allocated
// raw and zeroed by hand to match the store sequence.
// ---------------------------------------------------------------------------
AptPseudoDisplayList::AptPseudoDisplayList(void* lpOwner)
{
    void* lpRaw = gpAptPseudoDataPool->Allocate(sizeof(AptPseudoCIH_t));   // 20 bytes
    AptPseudoCIH_t* lpHead = static_cast<AptPseudoCIH_t*>(lpRaw);
    if (lpHead)
    {
        // X360: *v4=0; v4[2]=0; v4[1]=0; v4[3]=0; v4[4]=0  (all five dwords).
        std::memset(lpHead, 0, sizeof(AptPseudoCIH_t));
    }
    mpHead  = lpHead;
    mpOwner = lpOwner;
}

// ---------------------------------------------------------------------------
// ClearList @ 0x82AEAFC8
//
// asm: node = mpHead->mpNext;  while (node) { next = node->mpNext;
//          ~AptPseudoCIH_t(node); Deallocate(pool, node, 20); node = next; }
// Walks only the real entries (the head's successors); the head node itself is
// freed by the dtor, not here.
// ---------------------------------------------------------------------------
void AptPseudoDisplayList::ClearList()
{
    AptPseudoCIH_t* lpNode = static_cast<AptPseudoCIH_t*>(mpHead->mpNext);
    while (lpNode)
    {
        AptPseudoCIH_t* lpNext = static_cast<AptPseudoCIH_t*>(lpNode->mpNext);
        lpNode->~AptPseudoCIH_t();
        gpAptPseudoDataPool->Deallocate(lpNode, sizeof(AptPseudoCIH_t));
        lpNode = lpNext;
    }
}

// ---------------------------------------------------------------------------
// Remove @ 0x82AEB018
//
// asm: if (lpNode->mpPrev) lpNode->mpPrev->mpNext = lpNode->mpNext;
//      if (lpNode->mpNext) lpNode->mpNext->mpPrev = lpNode->mpPrev;
//      ~AptPseudoCIH_t(lpNode); Deallocate(pool, lpNode, 20).
// Field map: node+0x0C == mpNext, node+0x10 == mpPrev.
// ---------------------------------------------------------------------------
void AptPseudoDisplayList::Remove(AptPseudoCIH_t* lpNode)
{
    AptPseudoCIH_t* lpPrev = static_cast<AptPseudoCIH_t*>(lpNode->mpPrev);
    if (lpPrev)
        lpPrev->mpNext = lpNode->mpNext;

    AptPseudoCIH_t* lpNext = static_cast<AptPseudoCIH_t*>(lpNode->mpNext);
    if (lpNext)
        lpNext->mpPrev = lpNode->mpPrev;

    lpNode->~AptPseudoCIH_t();
    gpAptPseudoDataPool->Deallocate(lpNode, sizeof(AptPseudoCIH_t));
}

// ---------------------------------------------------------------------------
// Insert @ 0x82AEE4D0
//
// asm: FindInst(this, lpNode->mpContext, &existing, &pred);   // var_1C, var_20
//      if (existing) { Remove(existing); FindInst(this, lpNode->mpContext,
//                                                  &existing, &pred); }
//      // splice lpNode in just after `pred`:
//      next = pred->mpNext;                  // r11 = *(pred + 0xC)
//      lpNode->mpPrev = pred;                // *(lpNode + 0x10) = pred
//      lpNode->mpNext = next;                // *(lpNode + 0xC)  = next
//      if (next) next->mpPrev = lpNode;      // *(next + 0x10)   = lpNode
//      lpNode->mpPrev->mpNext = lpNode;      // *(*(lpNode+0x10) + 0xC) = lpNode
// The key fed to FindInst is the node's context word (node+0x08 == mpContext).
// ---------------------------------------------------------------------------
void AptPseudoDisplayList::Insert(AptPseudoCIH_t* lpNode)
{
    AptPseudoCIH_t* lpExisting = 0;
    AptPseudoCIH_t* lpPred     = 0;

    FindInst(lpNode->mpContext, &lpExisting, &lpPred);
    if (lpExisting)
    {
        Remove(lpExisting);
        FindInst(lpNode->mpContext, &lpExisting, &lpPred);
    }

    AptPseudoCIH_t* lpNext = static_cast<AptPseudoCIH_t*>(lpPred->mpNext);
    lpNode->mpPrev = lpPred;
    lpNode->mpNext = lpNext;
    if (lpNext)
        lpNext->mpPrev = lpNode;
    static_cast<AptPseudoCIH_t*>(lpNode->mpPrev)->mpNext = lpNode;
}

// ---------------------------------------------------------------------------
// dtor @ 0x82AECBF0
//
// asm: ClearList(); head = *this; if (head) { ~AptPseudoCIH_t(head);
//          Deallocate(pool, head, 20); }
// ---------------------------------------------------------------------------
AptPseudoDisplayList::~AptPseudoDisplayList()
{
    ClearList();
    if (mpHead)
    {
        mpHead->~AptPseudoCIH_t();
        gpAptPseudoDataPool->Deallocate(mpHead, sizeof(AptPseudoCIH_t));
    }
}
