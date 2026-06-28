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
//
// ADDITIVE GROW (flagged by the RaceBalancingRoute::Prepare/Recalculate group):
// those two functions call lpRoute->GetNodeCount() / GetDistance() / GetNode(i)
// and read each node's z slot as a per-node distance-to-checkpoint and the low
// half of the w slot as a u16 AI-section index. The X360 asm (@0x82789368 /
// @0x82789708) proves: GetNodeCount() == miNodeCount (Route+0x1400);
// GetDistance() == node[0]'s distance field (Route+8, i.e. maNodes[0].z); each
// node read as {x@+0, y@+4, mfDistanceToCheckpoint@+8, muSectionIndex(u16)@+12}.
// The accessors are declaration-additive and storage-preserving: maNodes stays a
// Vector4[320] (16B stride), so AddNode (BrnRoute.cpp) is untouched and every
// earlier offset / the 0x1400 count offset is preserved. RouteNode is a 16-byte
// reinterpreting view over a node slot.
//
// ADDITIVE GROW (flagged by the Route::Construct/Prepare group, this UNIT): the
// DecFIGS BrnRoute.h DWARF and the X360 asm of Construct(@0x82767B28) /
// Prepare(@0x82767B98) attest two more trailing members and a Status enum:
//   miDefaultStartNode @ guest 0x1404 (5124) -- copied by Construct, written by
//     Prepare (stw r26, 0x1404(r27)); the default node the route resolves to.
//   meStatus (Route::Status) @ guest 0x1408 (5128) -- copied by Construct, read
//     by Prepare's "meStatus != E_STATUS_UNINITIALISED" guard (lwz 0x1408,!=0).
// Both are appended AFTER miNodeCount so every existing offset (incl. 0x1400 and
// the Route+8 GetDistance slot) is preserved; AddNode is untouched. Offsets are
// pinned with static_assert(offsetof(...)==N) in a never-called _AssertLayout().

#include <cstddef>   // offsetof (layout pinning)

namespace BrnAI
{
// A node slot reinterpreted with named per-node fields. Exactly overlays the
// 16-byte Vector4 store (x@+0, y@+4, z->mfDistanceToCheckpoint@+8,
// w-low->muSectionIndex@+12). Read-only view; the route is populated via AddNode.
struct RouteNode
{
    f32 mfX;                    // +0  node position x
    f32 mfY;                    // +4  node position y
    f32 mfDistanceToCheckpoint; // +8  distance from this node to the next checkpoint
    u16 muSectionIndex;         // +12 AI section index (lhz at node+0xC)
    u16 muPad0x0E;              // +14 (w-high; unused by the timing model)

    f32 GetX() const { return mfX; }
    f32 GetY() const { return mfY; }
    f32 GetDistanceToCheckpoint() const { return mfDistanceToCheckpoint; }
    u16 GetSectionIndex() const { return muSectionIndex; }
};

// Declared as a struct to match the forward declaration in BrnAStar.h.
struct Route
{
    static const s32 KI_MAX_NODES = 320;   // capacity guard in AddNode

    // BrnRoute.h:104 (DWARF) -- route build/finalise status. Prepare guards on
    // "meStatus != E_STATUS_UNINITIALISED" before computing distances.
    enum Status
    {
        E_STATUS_UNINITIALISED = 0,
        E_STATUS_COMPLETE      = 1,
        E_STATUS_PARTIAL       = 2,
        E_STATUS_BLOCKED       = 3,
    };

    // @0x82767B28  Copy-construct from lrSource (DWARF: Construct(const Route*)).
    //   Copies miNodeCount (0x1400), miDefaultStartNode (0x1404), the populated
    //   node prefix (16B/node, count nodes) and meStatus (0x1408) verbatim from
    //   the source. No de-dup, no recompute -- a faithful field+node-prefix copy.
    void Construct(const Route& lrSource);

    // @0x82767B98  Finalise the route after AddNode-ing. Walks the nodes backwards
    //   filling each node's mfDistanceToCheckpoint with the cumulative PLANAR
    //   (x,y) distance from that node forward to the last node (the checkpoint);
    //   node[0] therefore holds the whole-route length, mirrored into Route+8 so
    //   GetDistance() returns it. Then records liDefaultStartNode (bounds-checked
    //   against miNodeCount). Returns true.
    bool Prepare(s32 liDefaultStartNode);

    // @0x827642A0  Append lrNode to the route.
    //   - returns 0 (false) if the route is already full (count >= KI_MAX_NODES);
    //   - if the route is non-empty and lrNode's (x,y) equals the last node's
    //     (x,y), the node is treated as a duplicate: nothing is stored but the
    //     call still reports success (returns 1);
    //   - otherwise the full 16-byte node is copied into the next slot and the
    //     node count is incremented. Returns 1 (true).
    bool AddNode(const Vector4& lrNode);

    // ---- ADDITIVE named accessors (X360-attested; see header note) ----
    // Number of nodes appended (Route+0x1400).
    s32 GetNodeCount() const { return miNodeCount; }
    // The route's resolved default start node (Route+0x1404; set by Prepare).
    s32 GetDefaultStartNode() const { return miDefaultStartNode; }
    // The route build status (Route+0x1408).
    Status GetStatus() const { return meStatus; }
    // Per-node view; the X360 caller indexes with the loop counter directly.
    const RouteNode* GetNode(s32 liNodeIndex) const
    {
        return reinterpret_cast<const RouteNode*>(&maNodes[liNodeIndex]);
    }
    // Whole-route distance == node[0]'s distance-to-checkpoint (Route+8). Used by
    // the balancer only for the "GetDistance() != 0.0f" guard.
    f32 GetDistance() const
    {
        return reinterpret_cast<const RouteNode*>(&maNodes[0])->mfDistanceToCheckpoint;
    }

    Vector4 maNodes[KI_MAX_NODES];   // guest offset 0 .. 0x1400
    s32     miNodeCount;             // guest offset 0x1400 (5120)
    s32     miDefaultStartNode;      // guest offset 0x1404 (5124)
    Status  meStatus;                // guest offset 0x1408 (5128)

private:
    // Never called: pins every offset the X360 asm touches so the compile gate
    // enforces layout fidelity (private members need a member fn for offsetof).
    static void _AssertLayout();
};
}

#endif
