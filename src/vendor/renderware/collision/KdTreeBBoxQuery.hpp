#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::KdTreeBBoxQuery -- a stateful "find all KD-tree leaves that
// overlap a bounding box" query object. Constructed against a KD-tree plus a
// caller-supplied query box, it caches the box and seeds the traversal cursor;
// the per-step Next/GetNext machinery (not in this TU) walks the tree from
// there. Created by rw::collision::TriangleKDTreeProcedural::BBoxOverlapQueryThis
// and rw::collision::ClusteredMesh::BBoxOverlapQueryThis.
//
// OWNING HOME for the single function the X360 binary defines here:
//     rw::collision::KdTreeBBoxQuery::KdTreeBBoxQuery (ctor) @ 0x828A9C08
//
// No DWARF hints exist for this TU, so the LAYOUT below is reconstructed from
// the store widths/offsets in the X360 ctor asm. The ctor touches only:
//     +0x00          word          stw a2          (the KD-tree pointer)
//     +0x10..+0x2F   8 words        copy from a3    (the query box, 8 dwords)
//     +0x30          word          stw 0            (only on the seeded branch)
//     +0xB0          word          stw 0|1          (traversal-active flag)
//     +0xB4          word          stw 0 then node  (top-of-stack node index)
//     +0xB8          word          stw 0            (traversal stack depth)
// Everything in between is the traversal scratch/stack the ctor leaves
// uninitialised; it is declared as honest opaque blocks sized only to land the
// asm-attested fields at their observed offsets. Members are accessed by name.
//
// The ctor reads two fields off the KD-tree (a2): *(a2+0x04) (the branch-node
// count -- nonzero means the tree has interior nodes to descend) and *(a2+0x08)
// (the single leaf's node index, used to seed the cursor for a leaf-only tree).
// ===========================================================================

namespace rw
{
namespace collision
{

// Minimal view of the KD-tree the query descends. Only the two header words the
// BBoxQuery ctor reads are named; the remainder of the KD-tree layout is not
// recovered by this TU. (FLAG: KdTree layout opaque beyond the named fields.)
//
// ADDITIVE GROW (rw-physics-collision group): the KdTreeLineQuery ctor
// @0x828AEE80 also reads this tree's overall AABB. It passes (a2+0x10) to
// AALineClipper::Init as the clip box, and its slab-clip loads the box-min row
// from (a2+0x10) and the box-max row from (a2+0x20). Those two 16-byte rows are
// added here at the asm-attested offsets; the three header words above are
// unchanged (same offsets/widths) so existing BBoxQuery code is unaffected.
struct KdTree
{
    u32 muReserved0;      // +0x00  not read by the BBoxQuery ctor
    u32 muNumBranchNodes; // +0x04  nonzero => interior nodes exist
    u32 muLeafNodeIndex;  // +0x08  seeds the cursor for a leaf-only tree
    u32 muReserved0C;     // +0x0C  unread padding (aligns mBoxMin to +0x10)
    f32 maBoxMin[4];      // +0x10  tree AABB lower corner (xyzw) -- LineQuery box-min
    f32 maBoxMax[4];      // +0x20  tree AABB upper corner (xyzw) -- LineQuery box-max
};

class KdTreeBBoxQuery
{
public:
    // @ 0x828A9C08 -- cache the KD-tree and query box, then seed the traversal
    // cursor. If the tree has branch nodes (lpKdTree->muNumBranchNodes != 0) the
    // descent starts at the root: clears the entry-stack slot at +0x30 and marks
    // the traversal active. Otherwise (a leaf-only tree) it seeds the cursor with
    // the single leaf node (muLeafNodeIndex) and leaves the traversal inactive.
    KdTreeBBoxQuery(KdTree* lpKdTree, const u32* lpQueryBox);

    // --- members (reconstructed from the ctor store offsets) ----------------
    KdTree* mpKdTree;            // +0x00  the tree being queried
    u32     muPad04[3];          // +0x04..+0x0F  unwritten (alignment to +0x10)
    u32     maQueryBox[8];       // +0x10..+0x2F  cached query box (8 words from a3)
    u32     muPad30[1];          // +0x30         root-descent slot (cleared when active)
    u32     muPad34[31];         // +0x34..+0xAF  traversal scratch/stack (opaque)
    u32     muTraversalActive;   // +0xB0         1 when descending from the root
    u32     muTopNode;           // +0xB4         current/top-of-stack node index
    u32     muStackDepth;        // +0xB8         traversal stack depth
};

} // namespace collision
} // namespace rw
