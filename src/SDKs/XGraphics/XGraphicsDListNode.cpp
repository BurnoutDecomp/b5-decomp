#include "SDKs/XGraphics/XGraphicsDListNode.h"

// ===========================================================================
// XGRAPHICS::DListNode -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XGraphicsDListNode.h for the layout and per-method asm notes. Each store
// matches the asm `stw` link displacement (4 = mpPrev, 8 = mpNext); no store is
// invented or dropped, and the only branch in each method is the asm's
// nonzero-neighbour guard.
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x82C288C8
DListNode* DListNode::InsertAfter(DListNode* pAfter)
{
    DListNode* pNext = pAfter->mpNext; // lwz r11, 8(r4)
    mpNext = pNext;                    // stw r11, 8(r3)
    if (pNext)                         // cmplwi r11,0 ; beq
        pNext->mpPrev = this;          // stw r3, 4(r11)
    mpPrev = pAfter;                   // stw r4, 4(r3)
    pAfter->mpNext = this;             // stw r3, 8(r4)
    return this;
}

// @ 0x82C288A8
DListNode* DListNode::InsertBefore(DListNode* pBefore)
{
    DListNode* pPrev = pBefore->mpPrev; // lwz r11, 4(r4)
    mpPrev = pPrev;                     // stw r11, 4(r3)
    if (pPrev)                          // cmplwi r11,0 ; beq
        pPrev->mpNext = this;           // stw r3, 8(r11)
    mpNext = pBefore;                   // stw r4, 8(r3)
    pBefore->mpPrev = this;             // stw r3, 4(r4)
    return this;
}

// @ 0x82C288E8
DListNode* DListNode::Remove()
{
    mpPrev->mpNext = mpNext; // lwz r11,4(r3); lwz r10,8(r3); stw r10,8(r11)
    mpNext->mpPrev = mpPrev; // lwz r11,8(r3); lwz r10,4(r3); stw r10,4(r11)
    return this;
}

} // namespace XGRAPHICS
