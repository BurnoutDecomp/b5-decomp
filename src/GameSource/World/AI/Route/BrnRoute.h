#ifndef BRN_ROUTE_H
#define BRN_ROUTE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (rw::math::vpu::Vector4)

// BrnAI::Route -- the ordered node list produced by the World/AI pathfinder
// (BrnAI::AStar::BuildRoute) and consumed/extended by
// BrnAI::RouteMapModule::ProcessExtrapolatedRoute. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; no prior source and no DecFIGS DWARF for this TU,
// so the layout is recovered store-for-store from the AddNode asm
// (@0x827642A0) and every field is accessed by name (no raw offset casts).
//
// The node store is a fixed array of 16-byte position nodes immediately
// followed by the node count: AddNode computes a slot address as
// (count << 4) + this, the count lives at guest offset 0x1400 == 320*16, and
// the capacity guard is "count >= 320". Each node is a 4-component vector
// (x,y,z,w); only the (x,y) pair participates in the de-dup compare.

namespace BrnAI
{
// Declared as a struct to match the forward declaration in BrnAStar.h.
struct Route
{
    static const s32 KI_MAX_NODES = 320;   // capacity guard in AddNode

    // @0x827642A0  Append lrNode to the route.
    //   - returns 0 (false) if the route is already full (count >= KI_MAX_NODES);
    //   - if the route is non-empty and lrNode's (x,y) equals the last node's
    //     (x,y), the node is treated as a duplicate: nothing is stored but the
    //     call still reports success (returns 1);
    //   - otherwise the full 16-byte node is copied into the next slot and the
    //     node count is incremented. Returns 1 (true).
    bool AddNode(const Vector4& lrNode);

    Vector4 maNodes[KI_MAX_NODES];   // guest offset 0 .. 0x1400
    s32     miNodeCount;             // guest offset 0x1400 (5120)
};
}

#endif
