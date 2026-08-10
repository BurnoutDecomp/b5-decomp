#pragma once

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h
//
// CgsGeometric::AxisAlignedBox4 -- FOUR axis-aligned boxes held structure-of-
// arrays, one lane per box. This is the on-disk form the PolygonSoupList carries
// for its polygon soups (four soup boxes per 112-byte block).
//
// The HOME PATH AND LINE NUMBER of this header are attested, not guessed: the
// only exported member, GetAxisAlignedBox @0x8283AA68, fires
//     "liBoxNum >= 0 && liBoxNum < 4"
//     "..\..\..\GameShared\GameClasses\Geometric/Primitives/CgsAxisAlignedBox4.h", 225
// read out of the X360 image.
//
// LAYOUT -- proved by the six lvx128 base registers inside GetAxisAlignedBox:
//     _R31 + 0    _R31 + 16   _R31 + 32   (the MIN rows)
//     _R31 + 48   _R31 + 64   _R31 + 80   (the MAX rows)
//   +0x00  mafMinX[4]
//   +0x10  mafMinY[4]
//   +0x20  mafMinZ[4]
//   +0x30  mafMaxX[4]
//   +0x40  mafMaxY[4]
//   +0x50  mafMaxZ[4]
//   +0x60  NEVER READ by GetAxisAlignedBox -- a seventh 16-byte row. It is named
//          as UNREAD, deliberately NOT as padding: the element stride is 112 both
//          in the caller (BuildSpacialPartition's `mulli r11, liBox4Index, 0x70`)
//          and in the PC data porter (world_support_transcode.py BOX4_SIZE = 112,
//          "7 rows x 4 u32 lanes"), so the row is real storage whose consumer has
//          not been identified yet.
//   sizeof == 112 (0x70)
//
// SERIALIZATION: this record lives INSIDE the WORLDCOL.BIN PolygonSoupList blob,
// i.e. it is serialised -- but it holds NO pointers, so nothing widens and the
// console offsets are genuinely pinnable on x64. (The porter agrees: it copies
// the box rows through verbatim as `4I` groups and leaves BOX4_SIZE at 112.)
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h" // AxisAlignedBox (returned BY VALUE)

namespace CgsGeometric
{
    struct AxisAlignedBox4
    {
        // The four boxes packed one-per-lane. Lane index == liBoxNum.
        f32 mafMinX[4];   // +0x00
        f32 mafMinY[4];   // +0x10
        f32 mafMinZ[4];   // +0x20
        f32 mafMaxX[4];   // +0x30
        f32 mafMaxY[4];   // +0x40
        f32 mafMaxZ[4];   // +0x50
        f32 mafUnread[4]; // +0x60  seventh row -- never read by GetAxisAlignedBox

        // GetAxisAlignedBox @0x8283AA68 -- gather lane `liBoxNum` out of the six SoA
        // rows into one scalar AxisAlignedBox.
        //
        // RETURNS BY VALUE, and that is read off the ABI rather than assumed: the X360
        // body takes THREE registers (r3, r4, r5) yet the class has only one `this`
        // and the function has one visible argument. r3 is the hidden sret pointer
        // (every store is `stvx128 ..., r30` with r30 = r3), r4 is `this` (every load
        // is r31-relative with r31 = r4), r5 is liBoxNum -- and the body ends
        // `return _R30`, returning the sret pointer, exactly as a 32-byte by-value
        // return does on this ABI.
        //
        // The lane gather itself is `lvsl` on (4 * liBoxNum) + `vspltw ...,0` + three
        // `vperm`, i.e. a broadcast of word `liBoxNum`; the `vperm ..., v7` with
        // unk_82CDA350 = {00010203, 14151617, 00010203, 00010203} then merges the X
        // lane with the Y lane, and `vrlimi128 ..., 2, 0` inserts Z. Net effect per
        // corner is the plain per-component gather written below; the w lane is
        // whatever the merge leaves and is never consumed.
        AxisAlignedBox GetAxisAlignedBox(u32 lu32BoxNum) const;
    };
}
