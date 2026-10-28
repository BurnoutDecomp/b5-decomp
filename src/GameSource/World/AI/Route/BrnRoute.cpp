#include "BrnRoute.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT + Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (default-node miss message)

#include <cmath>     // sqrtf
#include <cstddef>   // offsetof (layout pinning)
#include <cstring>   // full original record copies, including tail padding

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnAI::Route::AddNode    @ 0x827642A0
//   BrnAI::Route::Construct  @ 0x82767B28  (copy-construct overload)
//   BrnAI::Route::Prepare    @ 0x82767B98
//
// AddNode: append a node to the route's fixed node store. Mirrors the X360 asm:
//   r9 = count (0x1400(r3)); if count >= 320 -> return 0.
//   if count != 0: load last node's (x,y) at (count<<4)+this-0x10 / -0x0C and
//     the incoming (x,y); compare them as zero-padded 4-vectors. If equal, fall
//     through to "return 1" WITHOUT storing (de-dup) -- the binary builds two
//     vectors with the high lanes forced to zero and uses vcmpeqfp.
//   otherwise (count == 0, or the (x,y) pair differs): copy all four words of
//     the incoming node into slot (count<<4)+this and bump the count.

namespace BrnAI
{
bool Route::AddNode(const RouteNode& lrNode)
{
    const s32 liCount = miNodeCount;

    // Capacity guard (cmpwi 0x140 / bge -> return 0).
    if (liCount >= KI_MAX_NODES)
        return false;

    // Non-empty: de-dup against the last appended node on the (x,y) pair only
    // (the asm zero-fills the z/w lanes of both operands before vcmpeqfp).
    if (liCount != 0)
    {
        const RouteNode& lrLast = maNodes[liCount - 1];
        if (lrLast.GetPositionX() == lrNode.GetPositionX()
            && lrLast.GetPositionZ() == lrNode.GetPositionZ())
            return true;   // duplicate position -> success, nothing stored
    }

    // Store the full 16-byte node and advance the count.
    std::memmove(&maNodes[liCount], &lrNode, sizeof(RouteNode));
    miNodeCount = liCount + 1;

    return true;
}

// @0x82767B28  Copy-construct this route from lpSource. The X360 asm copies, in
// order: miNodeCount (0x1400), miDefaultStartNode (0x1404) -- stored BEFORE the
// node loop -- then if miNodeCount > 0 the populated node prefix (all four 32-bit
// words per node, 0x10 stride), and finally meStatus (0x1408). It is a faithful
// field+node-prefix copy: no de-dup and no distance recompute.
void Route::Construct(const Route* lpSource)
{
    const s32 liCount = lpSource->miNodeCount;

    miNodeCount        = liCount;                       // 0x1400
    miDefaultStartNode = lpSource->miDefaultStartNode;   // 0x1404 (stored pre-loop in the asm)

    for (s32 li = 0; li < liCount; ++li)
    {
        // Verbatim 16-byte node copy (lwz/stw of words 0,4,8,0xC in the asm).
        std::memmove(&maNodes[li], &lpSource->maNodes[li], sizeof(RouteNode));
    }

    meStatus = lpSource->meStatus;                       // 0x1408
}

// @0x82767B98  Finalise the route after the nodes have been AddNode-ed.
//
// Distance pass: walking the nodes backwards from the second-to-last, each
// node[i+1].mfDistanceLeft is set to the cumulative planar (x,y) distance
// already accumulated, then the segment node[i]->node[i+1] is added. Net result:
// node[k].mfDistanceLeft = sum of segment lengths from node[k] forward to
// the last node (the last node keeps 0); node[0] therefore holds the whole-route
// length, which is mirrored into Route+8 (== maNodes[0].mfDistanceLeft) so GetDistance() reads
// it. The X360 computes each segment length with a vrsqrtefp + 2-step
// Newton-Raphson refinement and a vcmpeqfp/vsel guard forcing 0 on an exactly-
// zero squared length -- semantically sqrtf with a zero-input guard, which
// sqrtf(0)==0 already satisfies (matches BrnAStar.h / BrnRaceBalancingRoute.cpp).
bool Route::Prepare(s32 liDefaultStartNode)
{
    // Top guard (lwz 0x1408; bne skips the assert).
    CGS_ASSERT(meStatus != E_STATUS_UNINITIALISED, "meStatus != E_STATUS_UNINITIALISED");

    const s32 liNodeCount = miNodeCount;

    f32 lfAccum = 0.0f;

    // Backwards walk: i from liNodeCount-2 down to 0 (the asm seeds at
    // &node[N-2] and iterates N-1 times). The store-then-add order writes the
    // accumulator into node[i+1] BEFORE folding in the i -> i+1 segment, so the
    // last node keeps distance 0 and node[0] ends up with the total.
    for (s32 li = liNodeCount - 2; li >= 0; --li)
    {
        // mfDistanceLeft lives in the z lane (node+8) of each scalar RouteNode.
        maNodes[li + 1].SetDistanceLeft(lfAccum);

        const RouteNode& lrA = maNodes[li];
        const RouteNode& lrB = maNodes[li + 1];
        const f32 lfDX = lrB.GetPositionX() - lrA.GetPositionX();
        const f32 lfDY = lrB.GetPositionZ() - lrA.GetPositionZ();
        const f32 lfSegment = sqrtf((lfDX * lfDX) + (lfDY * lfDY));

        lfAccum += lfSegment;
    }

    if (liNodeCount > 0)
    {
        // Route+8 == maNodes[0].mfDistanceLeft == node[0].mfDistanceLeft (whole-route length).
        maNodes[0].SetDistanceLeft(lfAccum);

        if (liDefaultStartNode >= liNodeCount)
        {
            // X360 streams the diagnostic into the global assert message buffer
            // via StrStream operator<< overloads; build it into a local buffer
            // (committed AISectionsResourceType.cpp::GetMiddle precedent).
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Can't default to node " << liDefaultStartNode
                    << " in a route with " << liNodeCount << " sections\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "..\\..\\..\\GameSource\\World/AI/Route/BrnRoute.cpp", 89);
            CgsDev::Assert::EndAssert();
        }
    }

    miDefaultStartNode = liDefaultStartNode;   // stw r26, 0x1404(r27)
    return true;                               // li r3, 1
}

// Never called: pins the offsets the Construct/Prepare/AddNode asm touches.
void Route::_AssertLayout()
{
    CGS_STATIC_ASSERT((offsetof(Route, miNodeCount)        == 0x1400), "miNodeCount must be at guest 0x1400");
    CGS_STATIC_ASSERT((offsetof(Route, miDefaultStartNode) == 0x1404), "miDefaultStartNode must be at guest 0x1404");
    CGS_STATIC_ASSERT((offsetof(Route, meStatus)           == 0x1408), "meStatus must be at guest 0x1408");
    // GetDistance()/Prepare mirror the total into Route+8 == maNodes[0].mfDistanceLeft.
    CGS_STATIC_ASSERT((offsetof(Route, maNodes) == 0), "maNodes must start the object");
    CGS_STATIC_ASSERT((sizeof(RouteNode) == 16), "original scalar node stride");
    CGS_STATIC_ASSERT((__alignof(RouteNode) == 4), "scalar node alignment");
    CGS_STATIC_ASSERT((offsetof(RouteNode, mfPositionX) == 0), "position X @0");
    CGS_STATIC_ASSERT((offsetof(RouteNode, mfPositionZ) == 4), "position Z @4");
    CGS_STATIC_ASSERT((offsetof(RouteNode, mfDistanceLeft) == 8), "distance @8");
    CGS_STATIC_ASSERT((offsetof(RouteNode, muSectionIndex) == 12), "section @12");
    CGS_STATIC_ASSERT((offsetof(RouteNode, muPortalIndex) == 14), "portal @14");
    CGS_STATIC_ASSERT((sizeof(Route) == 0x140C && __alignof(Route) == 4), "original route extent and alignment");
}
}
