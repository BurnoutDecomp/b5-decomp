#pragma once

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h
//
// CgsGeometric::PolygonSoupList -- the serialised static-collision resource
// (WORLDCOL.BIN, resource type 0x43 / 67): an overall AABB, a table of
// PolygonSoup pointers, a parallel array of AxisAlignedBox4 blocks (four soup
// boxes per block), and the soup payloads themselves, all in one relocated blob.
//
// ⭐ HOME CREATED 2026-08-10 (spatial-partition wave) TO RETIRE A FORK, not to
// add a type. The struct was previously defined TWICE, both times inside a .cpp:
//     Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.cpp:21
//     Geometric/Primitives/PolygonSoup/CgsPolygonSoupListResourceType.cpp:21
// The two were token-identical, so they happened to agree on layout and nothing
// broke -- but a third copy was about to be written by the spatial-partition
// build TU, and a silently-agreeing duplicate is exactly the shape that stops
// agreeing later ([[odr-forks-link-silently]]). Both .cpp copies now include
// this header instead.
//
// ⭐ SERIALIZATION: this record IS serialised and IS relocated (FixUp rebases
// every pointer by the load delta). The standing rule is that serialised pointer
// slots stay 32-bit -- but that rule does NOT apply here, and the reason is
// specific and checkable: the PC data porter emits an ALREADY-WIDENED blob.
//   tools/assets/bundles/world_support_transcode.py
//     PSL_X64_HDR  = 0x38   (X360 0x30)
//     struct.pack_into('<QQiI', out, 0x20, tab, boxes, n, dsz)
//     struct.pack_into('<%dQ' % n, out, tab, *offs)      <- the soup table is u64
// so on x64 the header really is {aabb[8] @0, mpapPolySoups @0x20 (8),
// mpaPolySoupBoxes @0x28 (8), miNumPolySoups @0x30, miDataSize @0x34} = 0x38, and
// the pointer TABLE really is 8 bytes per entry. Emit the committed consumer's
// layout, which is what the porter was written against.
// ============================================================================

#include "types.hpp"
#include <cstdint>

namespace CgsGeometric
{
    struct PolygonSoup;   // CgsPolygonSoup.h -- referenced through the pointer table

    // One entry of the pointer table, seen by FixUp as a raw relocatable word.
    // (FixUp also rebases two pointers inside each soup; the soup's own header is
    // modelled by CgsPolygonSoup.h, whose named members are already x64-correct.)
    struct PolygonSoupEntry
    {
        u32       _0[4];     // +0x00  miPosX/Y/Z + mfScale
        uintptr_t mpField16; // +0x10  (X360 +0x10) mpPolygons  -- relocated
        uintptr_t mpField20; // +0x18  (X360 +0x14) mpVertices  -- relocated
    };

    struct PolygonSoupList
    {
        float     mOverallAabb[8];  // +0x00  AxisAlignedBox (min, max)
        uintptr_t mpapPolySoups;    // +0x20  (X360 +0x20 u32) base of the PolygonSoup* table
        uintptr_t mpaPolySoupBoxes; // +0x28  (X360 +0x24 u32) base of the AxisAlignedBox4 array
        s32       miNumPolySoups;   // +0x30  (X360 +0x28)
        s32       miDataSize;       // +0x34  (X360 +0x2C) total serialised byte size

        // FixUp -- load-time relocation; rebases the table, every soup pointer, and
        // the two pointer fields inside every soup. Bodied in CgsPolygonSoupList.cpp.
        PolygonSoupList* FixUp(int delta);
    };
}
