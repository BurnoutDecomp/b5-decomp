#pragma once

// ===========================================================================
// XGRAPHICS::Block -- a basic block in the XGRAPHICS shader-microcode
// intermediate representation (BURNOUT_X360_ARTIST.XEX): the container that
// owns the two intrusive IR-node lists an IR node splices itself into. This
// header is the canonical OWNING home for:
//
//     XGRAPHICS::Block::Append  @ 0x82C2C760
//     XGRAPHICS::Block::Insert  @ 0x82C2C7A8
//
// There is no reference source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 asm of these two methods. `XGRAPHICS`
// is an X360 graphics-SDK boundary, so its identifiers are preserved verbatim
// per the naming convention.
//
// LAYOUT grounded in the asm (only the fields these two methods touch are
// attested; everything else is opaque padding sized to place them):
//   Block:
//     +0x70  mpInsertAnchor  (DListNode*) -- Insert() splices the node *after*
//                              this anchor   (lwz r4,0x70(r31) -> InsertAfter).
//     +0x74  mpAppendAnchor  (DListNode*) -- Append() splices the node *before*
//                              this anchor   (lwz r4,0x74(r31) -> InsertBefore).
//   Node (the IR node passed in, a DListNode-derived instruction node):
//     +0x00  DListNode link/payload (the node is spliced via DListNode ops)
//     +0x3B4 mpOwnerBlock    (Block*) -- back-pointer set to `this` after the
//                              splice   (stw r31,0x3B4(r30)).
//
// The node parameter is one of the XGRAPHICS IR instruction nodes (the callers
// are StandardIndex / AutoIndexVtx / HosCoord / Interpolator / ExportValue
// constructors). Its full layout is owned by the IR-node TUs; here only the
// DListNode sub-object at +0 and the owner back-pointer at +0x3B4 are attested,
// so the node is modelled as a DListNode-derived shell with explicit padding to
// the +0x3B4 back-pointer. FLAG: this Node shell's interior padding is sized
// from the asm displacement only (no DWARF), not a committed full IR-node type.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsDListNode.h" // XGRAPHICS::DListNode

namespace XGRAPHICS
{

class Block; // forward (the node's owner back-pointer)

// The IR instruction node Block::Insert/Append splice. It IS-A DListNode (the
// list ops act on its +0 link sub-object) and carries an owner-Block back
// pointer at +0x3B4. Only those two attested fields are named; the interior is
// opaque payload owned by the IR-node TUs.
struct BlockNode : public DListNode
{
    // +0x00 .. +0x0B : DListNode (mReserved @+0, mpPrev @+4, mpNext @+8)
    u8     maPayload[0x3B4 - sizeof(DListNode)]; // +0x0C .. +0x3B3 opaque IR payload
    Block* mpOwnerBlock;                          // +0x3B4 owning block back-pointer
};

class Block
{
public:
    // @ 0x82C2C760 -- splice `apNode` immediately before the append anchor
    // (+0x74) and stamp the node's owner back-pointer to `this`.
    int Append(BlockNode* apNode);

    // @ 0x82C2C7A8 -- splice `apNode` immediately after the insert anchor
    // (+0x70) and stamp the node's owner back-pointer to `this`.
    int Insert(BlockNode* apNode);

    u8         maPad00[0x70];        // +0x00 .. +0x6F opaque
    DListNode* mpInsertAnchor;       // +0x70  Insert() splices after this anchor
    DListNode* mpAppendAnchor;       // +0x74  Append() splices before this anchor
};

} // namespace XGRAPHICS
