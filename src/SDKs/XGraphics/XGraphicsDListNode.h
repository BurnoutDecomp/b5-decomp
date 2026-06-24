#pragma once

// ===========================================================================
// XGRAPHICS::DListNode -- the intrusive doubly-linked-list node used by the
// XGRAPHICS shader-microcode intermediate-representation lists in
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for:
//
//     XGRAPHICS::DListNode::InsertAfter   @ 0x82C288C8
//     XGRAPHICS::DListNode::InsertBefore  @ 0x82C288A8
//     XGRAPHICS::DListNode::Remove        @ 0x82C288E8
//
// There is no reference source and no DWARF for this TU, so the SHAPE
// below is reconstructed purely from the X360 asm of these three methods.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers
// (DListNode, InsertAfter/InsertBefore/Remove) are preserved verbatim per the
// naming convention.
//
// LAYOUT (each store goes through a named member, store-for-store with the asm):
//   +0  reserved leading word (the IR node payload begins here; the list ops
//       below never touch +0, only the two link words at +4/+8)
//   +4  mpPrev -- previous node (the asm `4(rX)` slot)
//   +8  mpNext -- next node     (the asm `8(rX)` slot)
//
// Verified against asm link displacements:
//   InsertAfter(this, after):  next = after.mpNext (8(r4)); this.mpNext = next
//       (8(r3)); if(next) next.mpPrev = this (4(r11)); this.mpPrev = after
//       (4(r3)); after.mpNext = this (8(r4)).
//   InsertBefore(this, before): prev = before.mpPrev (4(r4)); this.mpPrev = prev
//       (4(r3)); if(prev) prev.mpNext = this (8(r11)); this.mpNext = before
//       (8(r3)); before.mpPrev = this (4(r4)).
//   Remove(this): this.mpPrev.mpNext = this.mpNext (read 4(r3)/8(r3), store
//       8(r11)); this.mpNext.mpPrev = this.mpPrev (read 8(r3)/4(r3), store
//       4(r11)). No null guards in the asm -- an unlinked-from sentinel head is
//       assumed by the callers (IRInst::Kill / IRLoadConst::Kill).
// ===========================================================================

#include "types.hpp"

namespace XGRAPHICS
{

struct DListNode
{
    u32        mReserved; // +0  IR node payload anchor (untouched by link ops)
    DListNode* mpPrev;    // +4  previous node
    DListNode* mpNext;    // +8  next node

    // @ 0x82C288C8 -- splice `this` in immediately after `pAfter`.
    DListNode* InsertAfter(DListNode* pAfter);

    // @ 0x82C288A8 -- splice `this` in immediately before `pBefore`.
    DListNode* InsertBefore(DListNode* pBefore);

    // @ 0x82C288E8 -- unlink `this` from its neighbours.
    DListNode* Remove();
};

} // namespace XGRAPHICS
