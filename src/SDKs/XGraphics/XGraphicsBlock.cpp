#include "SDKs/XGraphics/XGraphicsBlock.h"

// ===========================================================================
// XGRAPHICS::Block -- member bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every store; see XGraphicsBlock.h for the
// layout and the attested-field notes. No reference source and no DWARF
// exist for this TU.
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x82C2C760 -- store-for-store:
//   mr r31,r3 (this) ; mr r30,r4 (node)
//   mr r3,r30 ; lwz r4,0x74(r31) ; bl InsertBefore   -> node->InsertBefore(append anchor)
//   stw r31,0x3B4(r30)                                 -> node->mpOwnerBlock = this
//   return r3 (the InsertBefore result == node, unchanged by the store)
int Block::Append(BlockNode* apNode)
{
    // node->InsertBefore(mpAppendAnchor)  (r3=node, r4=*(this+0x74))
    DListNode* lpResult = apNode->InsertBefore(mpAppendAnchor);
    apNode->mpOwnerBlock = this; // stw r31,0x3B4(r30)
    return static_cast<int>(reinterpret_cast<intptr_t>(lpResult)); // r3 carries the node ptr
}

// @ 0x82C2C7A8 -- store-for-store:
//   mr r31,r3 (this) ; mr r30,r4 (node)
//   mr r3,r30 ; lwz r4,0x70(r31) ; bl InsertAfter    -> node->InsertAfter(insert anchor)
//   stw r31,0x3B4(r30)                                 -> node->mpOwnerBlock = this
//   return r3 (the InsertAfter result == node, unchanged by the store)
int Block::Insert(BlockNode* apNode)
{
    // node->InsertAfter(mpInsertAnchor)  (r3=node, r4=*(this+0x70))
    DListNode* lpResult = apNode->InsertAfter(mpInsertAnchor);
    apNode->mpOwnerBlock = this; // stw r31,0x3B4(r30)
    return static_cast<int>(reinterpret_cast<intptr_t>(lpResult)); // r3 carries the node ptr
}

} // namespace XGRAPHICS
