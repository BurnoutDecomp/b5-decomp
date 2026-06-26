#pragma once

#include "types.hpp"

#include "vendor/renderware/collision/AALineClipper.hpp"
#include "vendor/renderware/collision/KdTreeBBoxQuery.hpp" // for rw::collision::KdTree

// ===========================================================================
// rw::collision::KdTreeLineQuery -- a stateful "walk the KD-tree leaves a line
// segment passes through" query object. Constructed against a KD-tree plus a
// line segment, it precomputes the per-axis clip parameters (an embedded
// AALineClipper), clips the [tMin,tMax] line interval against the tree's overall
// AABB, then seeds the descent cursor. The per-step traversal (Next/GetNext,
// not in this TU) walks the tree from there, and ClipEnd shortens the pending
// node stack when the caller moves the far line endpoint inwards.
//
// Created by:
//   CgsSceneManager::FineIntersectionTestModule::ComputeLineTestFine
//   rw::collision::TriangleKDTreeProcedural::LineIntersectionQueryThis
//   rw::collision::ClusteredMesh::LineIntersectionQueryThis
//
// OWNING HOME for the two functions the X360 binary defines here:
//     rw::collision::KdTreeLineQuery::KdTreeLineQuery (ctor) @ 0x828AEE80
//     rw::collision::KdTreeLineQuery::ClipEnd                @ 0x82BB0408
//
// No DWARF hints exist for this TU, so the LAYOUT below is reconstructed from
// the store widths/offsets in the X360 asm. The ctor is hand-vectorised VMX/
// AltiVec (lvx128 / vsubfp / vaddfp / vmulfp / vminfp / vmaxfp / fsel) computing
// an axis-aligned slab clip; following the AALineClipper / FeatureEdge / Triangle4
// precedent it is lowered to a SEMANTIC, portable, named-member float
// reconstruction. The store ORDER and offsets each store lands at are preserved.
//
//   +0x00          word    mpKdTree           -- the KD-tree pointer (stw a2)
//   +0x04..+0x0F   12B      muPad04            -- unwritten (aligns clipper to +0x10)
//   +0x10..+0x4F   0x40     mClipper           -- embedded AALineClipper (4 vec rows)
//   +0x50..?       16B/rec  maNodeStack[...]   -- pending-node stack; ClipEnd walks
//                                                 records of {2 dwords, key f32 @+8,
//                                                 far-param f32 @+0xC} at stride 0x10
//     +0x50  maNodeStack[0].muNode0  (stw -1 on the descend-from-root branch)
//     +0x54  maNodeStack[0].muNode1  (stw  0 on the descend-from-root branch)
//     +0x58  maNodeStack[0].mfKey    -- clipped line tMin (init 0.0, clamped up)
//     +0x5C  maNodeStack[0].mfFar    -- clipped line tMax (init 1.0, clamped down)
//   The clipped interval [tMin,tMax] IS the seeded root stack record: the slab
//   clip writes it straight into entry 0's key/far slots, then the root-descent
//   branch fills entry 0's node words.
//   +0x250         word    muStackCount        -- pending-node count / active flag
//   +0x254         word    muTopNode           -- seeded leaf node index (leaf-only tree)
//   +0x258         word    muTopNodeHi         -- companion word (zeroed)
//
// The 0x50..0x24F window is the traversal node stack the descent fills; only the
// first record's two header words are written by the ctor. It is declared as an
// honest opaque block sized to land the asm-attested fields at their observed
// offsets. Members are accessed by name.
// ===========================================================================

namespace rw
{
namespace collision
{

class KdTreeLineQuery
{
public:
    // One pending node-stack record (16 bytes). ClipEnd compacts the stack,
    // keeping records whose key (mfKey @+0x08) is <= the new far endpoint and
    // clamping each survivor's far param (mfFar @+0x0C) down to that endpoint.
    struct NodeStackEntry
    {
        u32 muNode0; // +0x00  node bookkeeping (copied verbatim by ClipEnd)
        u32 muNode1; // +0x04  node bookkeeping (copied verbatim by ClipEnd)
        f32 mfKey;   // +0x08  entry line parameter -- ClipEnd's keep/discard key
        f32 mfFar;   // +0x0C  exit line parameter  -- clamped to the new endpoint
    };

    static const u32 KU_NUM_STACK_ENTRIES = 32; // 0x50..0x24F / 0x10 = 0x20 records

    // @ 0x828AEE80 -- precompute the per-axis clip parameters, clip the line
    // interval against the tree AABB, and seed the descent cursor. The X360
    // __fastcall takes r3=this, r4=lpKdTree; the line endpoints arrive in the VMX
    // argument registers (rStart=v1, rEnd=v2) and a3 is the per-axis fatness used
    // to build AALineClipper's dir-seed (max(|start|,|end|)*fatness + padEps).
    //
    // If the clipped interval is empty (mfClipMin >= mfClipMax) the cursor is left
    // inactive (muStackCount = 0). Otherwise, on a tree with branch nodes the
    // first stack record is seeded for a root descent and muStackCount = 1; on a
    // leaf-only tree the leaf node index is latched into muTopNode and the cursor
    // is left inactive.
    KdTreeLineQuery(KdTree* lpKdTree, const AALineClipper::Vec4& rStart,
                    const AALineClipper::Vec4& rEnd, f32 lfFatness);

    // @ 0x82BB0408 -- shorten the pending-node stack to a new far line endpoint.
    // Walks the live records (0..muStackCount), keeping those whose mfKey <=
    // lfFarParam (compacting them down), clamping each survivor's mfFar to
    // min(mfFar, lfFarParam), and writes back the reduced count. Returns this.
    // (X360 passes the new endpoint as a3; the asm compares mfKey via fcmpu and
    // clamps mfFar via fsub/fsel.)
    KdTreeLineQuery* ClipEnd(f32 lfFarParam);

    // --- members (reconstructed from the asm store offsets) ------------------
    KdTree*        mpKdTree;                        // +0x00
    u32            muPad04[3];                       // +0x04..+0x0F (aligns clipper)
    AALineClipper  mClipper;                         // +0x10..+0x4F
    NodeStackEntry maNodeStack[KU_NUM_STACK_ENTRIES];// +0x50..+0x24F
    u32            muStackCount;                     // +0x250  pending count / active
    u32            muTopNode;                        // +0x254  leaf node index seed
    u32            muTopNodeHi;                       // +0x258  companion word
};

} // namespace collision
} // namespace rw
