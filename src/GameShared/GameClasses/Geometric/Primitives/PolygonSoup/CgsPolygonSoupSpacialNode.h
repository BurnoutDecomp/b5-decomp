#pragma once

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h
//
// The two node types of CgsGeometric::PolygonSoupListSpatialMap's partition.
// Both are carved at runtime by PolygonSoupListSpatialMap::BuildSpacialPartition
// @0x82841740 and both are 48 bytes (0x30) with the SAME field shape:
//
//   +0x00  AxisAlignedBox  (mMin at +0x00, mMax at +0x10)
//   +0x20  a pointer
//   +0x20+sizeof(ptr)  a u16
//
// but the pointer/u16 pair means different things in each, which is why the
// shipped BuildSpacialPartition carries TWO near-identical bounding-box refit
// loops (@0x828436D4 over leaves, @0x82843794 over parents) instead of one --
// the element types differ.
//
// NAMES -- both attested, neither invented:
//   PolygonSoupSpacialNode  PS3 DWARF mangle inside PolygonSoupListSpatialMap::
//                           RunJobQuery @0xB63F20
//                           (...ReadOnlyObjectCacheINS_22PolygonSoupSpacialNodeEE)
//   PolygonSoupLeafNode     X360 export NAMES @0x829170F8 / @0x829172D0
//                           (ReadOnlyObjectCache<CgsGeometric::PolygonSoupLeafNode>
//                            ::Construct / ::Release)
//
// ⭐ SERIALIZATION TEST -- these are CARVED AT RUNTIME (BuildSpacialPartition
// LinearMalloc's them every rebuild), NOT deserialised, so their pointer slots
// legitimately widen 4->8 on x64. The deciding test, applied deliberately:
// nothing FixUps them and no porter emits them.
//
// ⭐ AND THE SIZE SURVIVES THE WIDENING AT 48 BYTES, which is load-bearing and
// therefore GATED rather than assumed (CgsPolygonSoupSpacialNode_embed_check.cpp):
//   console: box 32 + ptr 4 + u16 2 = 0x26 -> align16 -> 48
//   x64:     box 32 + ptr 8 + u16 2 = 0x2A -> align16 -> 48
// The Vector4 alignas(16) inside AxisAlignedBox is what absorbs the extra four
// bytes. PolygonSoupListSpatialMap::GetPolygonSoup @0x8280FFD0 hard-codes a 0x30
// element step, and it stays correct BECAUSE of that coincidence -- not by design.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h" // AxisAlignedBox (BY VALUE)

namespace CgsGeometric
{
    struct PolygonSoup;   // forward-decl: the leaf points at one, never derefs it here

    // ------------------------------------------------------------------------
    // A grid node of one partition LEVEL. Level L holds a levelSize x levelSize
    // grid (levelSize == 1 << L) laid out row-major as [liX * levelSize + liZ].
    //
    // Every node of every level EXCEPT the bottom one owns a 4-entry child index
    // array into the next finer level; the BOTTOM level's nodes instead own a
    // variable-length index array into the LEAF node array, sized by a counting
    // pass. Both uses share these two members -- BuildSpacialPartition writes
    // mu16NumIndices = 4 for the former and the counted leaf total for the latter.
    // ------------------------------------------------------------------------
    struct PolygonSoupSpacialNode
    {
        AxisAlignedBox mBox;            // +0x00  min corner +0x00, max corner +0x10
        u16*           mpaIndices;      // +0x20  (X360 u32) child-node or leaf-node indices
        u16            mu16NumIndices;  // +0x24 X360 / +0x28 x64
    };

    // ------------------------------------------------------------------------
    // A leaf: exactly one CgsGeometric::PolygonSoup, plus that soup's serialised
    // byte size. The size is carried here rather than re-read from the soup
    // because the consumer of a leaf is the PolygonSoupTester JOB, which must DMA
    // the whole soup before it can look at it -- the value BuildSpacialPartition
    // copies in is the soup's own u16 size field.
    // ------------------------------------------------------------------------
    struct PolygonSoupLeafNode
    {
        AxisAlignedBox     mBox;              // +0x00  the soup's box, gathered from an AxisAlignedBox4 lane
        const PolygonSoup* mpPolygonSoup;     // +0x20  (X360 u32) entry of PolygonSoupList::mpapPolySoups
        u16                mu16PolygonSoupSize; // +0x24 X360 / +0x28 x64 -- the soup's own size field
    };
}
