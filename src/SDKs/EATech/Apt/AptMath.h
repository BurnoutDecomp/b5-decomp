#pragma once

// ===========================================================================
//  AptMath.h  --  EATech Apt (ActionScript player) 2D matrix / clip-stack math.
//
//  AptMath is a namespace of free helpers (no `this`; the X360 symbols are
//  AptMath::ClipStack* / AptMath::MatMul2d) over a single process-wide clip
//  stack used by the APT renderer when it pushes/pops nested transform+colour
//  state for masked / clipped display objects.
//
//  Each clip-stack entry is 0x70 (112) bytes -- the per-entry stride proven by
//  the `mulli rN, idx, 0x70` in every ClipStack* accessor. Within an entry the
//  first 0x40 bytes are a 4x4 (16-float, row stride 4) transform matrix and the
//  tail holds a colour transform (an ARGB scale + ARGB translate block).
//
//  NO DWARF exists for AptMath (X360-only; not in the PS3 DecFIGS dump). The
//  shapes below are derived STRICTLY from the X360 binary:
//      AptMath::ClipStackGetTop   @ 0x82AD5D08
//      AptMath::ClipStackPush     @ 0x82AD5CB0
//      AptMath::ClipStackPop      @ 0x82AD5CD8
//      AptMath::ClipStackMakeUnit @ 0x82ADC548
//      AptMath::ClipStackInit     @ 0x82AE2470
//      AptMath::MatMul2d          @ 0x82AD5D40
// ===========================================================================

#include "types.hpp"

// One clip-stack entry, 0x70 (112) bytes.
//
// Field offsets are exactly the displacements the X360 ClipStackMakeUnit
// (@0x82ADC548) writes:
//   matrix     [+0x00 .. +0x3C]  16 floats, 4x4 row-major (row stride 4 floats)
//   (+0x40)    gap, never written by MakeUnit
//   scaleArgb  [+0x44 .. +0x50]  4 floats, set to 255.0 by the unit ctor
//   (+0x54)    gap, never written by MakeUnit
//   transArgb  [+0x58 .. +0x64]  4 floats, set to 0.0 by the unit ctor
//   (+0x68 .. +0x6F) tail padding to the 0x70 stride
struct AptClipMatrixEntry
{
    f32 mafMatrix[16];      // [+0x00] 4x4 transform (row stride = 4 floats)
    f32 mfPad40;            // [+0x40] unwritten gap
    f32 mafScaleArgb[4];    // [+0x44] colour scale  (A,R,G,B)
    f32 mfPad54;            // [+0x54] unwritten gap
    f32 mafTransArgb[4];    // [+0x58] colour translate (A,R,G,B)
    f32 mafTailPad[2];      // [+0x68] padding out to 0x70
};

namespace AptMath
{
    // FLAG (x64): the console carries the clip-stack entry pointers as 32-bit ints
    // (base + 0x70*index). On x64 the heap base is a 64-bit address, so these MUST be
    // pointer-width (intptr_t) -- a 32-bit int would TRUNCATE the base and the makeUnit
    // write would corrupt memory. Widened to intptr_t (the pervasive Apt x64-port rule);
    // the arithmetic is identical, only the width changes.

    // Allocate + reset the clip stack for `nDepth` entries. X360 @0x82AE2470:
    // pool-allocates (112*nDepth + 16) bytes from the shared Apt pool, 16-byte
    // aligns the base, records depth, zeroes the index, then makes the bottom
    // entry the unit transform. Returns the bottom-entry pointer that
    // ClipStackMakeUnit yields.
    intptr_t ClipStackInit(int nDepth);

    // Pointer (as intptr_t) to the current top entry: base + 0x70 * index.
    intptr_t ClipStackGetTop();

    // Pre-increment the index, return the new top-entry pointer.
    intptr_t ClipStackPush();

    // Pre-decrement the index, return the new top-entry pointer.
    intptr_t ClipStackPop();

    // Initialise the current top entry to the identity transform + opaque-white
    // colour scale (255) + zero colour translate. X360 @0x82ADC548.
    void ClipStackMakeUnit();

    // 2D affine matrix multiply over two 4x4 (16-float) clip matrices, writing
    // the composed linear+translation terms into `result`. X360 @0x82AD5D40.
    f32* MatMul2d(f32* result, const f32* a2, const f32* a3);
}
