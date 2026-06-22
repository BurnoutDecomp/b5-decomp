#include "BrnRoute.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827642A0
//   BrnAI::Route::AddNode
//
// Append a node to the route's fixed node store. Mirrors the X360 asm:
//   r9 = count (0x1400(r3)); if count >= 320 -> return 0.
//   if count != 0: load last node's (x,y) at (count<<4)+this-0x10 / -0x0C and
//     the incoming (x,y); compare them as zero-padded 4-vectors. If equal, fall
//     through to "return 1" WITHOUT storing (de-dup) -- the binary builds two
//     vectors with the high lanes forced to zero and uses vcmpeqfp.
//   otherwise (count == 0, or the (x,y) pair differs): copy all four words of
//     the incoming node into slot (count<<4)+this and bump the count.

namespace BrnAI
{
bool Route::AddNode(const Vector4& lrNode)
{
    const s32 liCount = miNodeCount;

    // Capacity guard (cmpwi 0x140 / bge -> return 0).
    if (liCount >= KI_MAX_NODES)
        return false;

    // Non-empty: de-dup against the last appended node on the (x,y) pair only
    // (the asm zero-fills the z/w lanes of both operands before vcmpeqfp).
    if (liCount != 0)
    {
        const Vector4& lrLast = maNodes[liCount - 1];
        if (lrLast.x == lrNode.x && lrLast.y == lrNode.y)
            return true;   // duplicate position -> success, nothing stored
    }

    // Store the full 16-byte node and advance the count.
    Vector4& lrSlot = maNodes[liCount];
    lrSlot.x = lrNode.x;
    lrSlot.y = lrNode.y;
    lrSlot.z = lrNode.z;
    lrSlot.w = lrNode.w;
    miNodeCount = liCount + 1;

    return true;
}
}
