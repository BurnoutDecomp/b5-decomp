#include "vendor/renderware/collision/KdTreeBBoxQuery.hpp"

namespace rw
{
namespace collision
{

// @ 0x828A9C08.
// Cache the KD-tree pointer and the 8-word query box, zero the two traversal
// bookkeeping words, then seed the cursor from the tree's shape:
//   stw  a2, 0(this)                         -> mpKdTree = lpKdTree
//   copy 8 dwords a3[0..7] -> this+0x10      -> maQueryBox = *lpQueryBox[8]
//   stw  0, 0xB4(this); stw 0, 0xB8(this)    -> muTopNode = muStackDepth = 0
//   lwz  r10, 4(a2); cmplwi; ble             -> branch on muNumBranchNodes != 0
//     non-empty: stw 0,0x30(this); stw 1,0xB0(this)
//     leaf-only: stw *(a2+8),0xB4; stw 0,0xB0; stw 0,0xB8
KdTreeBBoxQuery::KdTreeBBoxQuery(KdTree* lpKdTree, const u32* lpQueryBox)
{
    mpKdTree = lpKdTree;

    for (s32 li = 0; li < 8; ++li)
    {
        maQueryBox[li] = lpQueryBox[li];
    }

    muTopNode    = 0; // +0xB4 (zeroed before the branch)
    muStackDepth = 0; // +0xB8

    if (lpKdTree->muNumBranchNodes != 0)
    {
        // Tree has interior nodes: start the descent at the root.
        muPad30[0]        = 0; // +0x30 root-descent entry slot
        muTraversalActive = 1; // +0xB0
    }
    else
    {
        // Leaf-only tree: seed the cursor with the single leaf node and leave
        // the traversal inactive.
        muStackDepth      = 0;                       // +0xB8 (re-cleared in asm)
        muTraversalActive = 0;                       // +0xB0
        muTopNode         = lpKdTree->muLeafNodeIndex; // +0xB4
    }
}

} // namespace collision
} // namespace rw
