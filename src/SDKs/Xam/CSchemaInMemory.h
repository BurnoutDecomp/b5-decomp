#ifndef SDKS_XAM_CSCHEMAINMEMORY_H
#define SDKS_XAM_CSCHEMAINMEMORY_H

#include "types.hpp"

// ===========================================================================
// SDKs/Xam/CSchemaInMemory.h
//
// CSchemaInMemory -- a schema blob mapped into memory. Bind copies a 0x2C-byte
// wire header out of a source blob into this descriptor, then carves the source's
// trailing payload (source + 0x2C) into a run of sub-sections, recording a cursor
// (or a size) for each. Sibling of the Xbox-360 XAM marshalling classes
// (CSchemaData / CArgumentList / CBaseEndianBuffer) -- CSchemaData::Lookup-
// ConstantFromTable @ 0x8297F280 is the function immediately preceding Bind in
// this TU.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Bind @ 0x8297F2B8). There is no
// reference source and no DWARF for this TU; the SHAPE below is grounded purely
// on the X360 asm. Microsoft-XDK-style C-prefixed class, so the identifier
// (CSchemaInMemory) is kept verbatim per the naming convention's external/
// platform-API carve-out.
//
// LAYOUT
//   The first 0x2C bytes are the wire header memcpy'd from the source blob. Only
//   the fields Bind actually touches are pinned (byte offsets are X360-attested
//   via lhz/lwz displacements); everything else in that span is opaque padding.
//   The +0x2C..+0x48 words are computed by Bind (section cursors + one size); on
//   the 64-bit host the pointer members widen, so those offsets are NOT assertable.
//
//     -- copied wire header (0x2C bytes) --
//     +0x08  mFlags        u32   bit0 set => Bind fails (E_FAIL)
//     +0x14  muTotalSize   u32   declared total blob size in bytes
//     +0x18  muCols        u16   } multiplied together (muRows*muCols) for section 4
//     +0x1A  muRows        u16   }
//     +0x20  muHalfWords   u16   count of trailing half-words (x2 => byte size)
//     +0x26  muExtraSize   u16   optional leading extra-block size (0 => absent)
//     +0x28  muBlockCount  u16   number of blocks (x4 index table; x stride span)
//     +0x2A  muBlockStride u16   per-block byte stride
//
//     -- computed section cursors (into the source span) + one size --
//     +0x2C  mpSection0    u8*   payload cursor (after optional extra block)
//     +0x30  mpSection1    u8*   after the 4-byte-per-block index table
//     +0x34  mpSection2    u8*   after the block span (stride * count)
//     +0x38  muRemainderSize u32 leftover bytes after header/extra/index/span
//     +0x3C  mpExtra       u8*   optional extra block (only set when muExtraSize)
//     +0x40  mpSection3    u8*   remainder placed after section 2
//     +0x44  mpSection4    u8*   row-major body (rows * cols) after section 3
//     +0x48  mpSection5    u8*   trailing half-word block (2 * count) after sec 4
// ===========================================================================

class CSchemaInMemory
{
public:
    // Copy `lpSource`'s 0x2C-byte header into this descriptor and carve the
    // source payload into sections. Returns S_OK (0), or E_FAIL (0x80004005)
    // when (mFlags & 1).                                               @ 0x8297F2B8
    s32 Bind(CSchemaInMemory* lpSource);

private:
    // -- copied wire header --
    u8   mPad00[0x08];       // +0x00 .. +0x07 (opaque)
    u32  mFlags;             // +0x08  bit0 => reject
    u8   mPad0C[0x14 - 0x0C];// +0x0C .. +0x13 (opaque)
    u32  muTotalSize;        // +0x14
    u16  muCols;             // +0x18
    u16  muRows;             // +0x1A
    u8   mPad1C[0x20 - 0x1C];// +0x1C .. +0x1F (opaque)
    u16  muHalfWords;        // +0x20
    u8   mPad22[0x26 - 0x22];// +0x22 .. +0x25 (opaque)
    u16  muExtraSize;        // +0x26
    u16  muBlockCount;       // +0x28
    u16  muBlockStride;      // +0x2A

    // -- computed section cursors + size (Bind output) --
    u8*  mpSection0;         // +0x2C
    u8*  mpSection1;         // +0x30
    u8*  mpSection2;         // +0x34
    u32  muRemainderSize;    // +0x38
    u8*  mpExtra;            // +0x3C
    u8*  mpSection3;         // +0x40
    u8*  mpSection4;         // +0x44
    u8*  mpSection5;         // +0x48
};

#endif // SDKS_XAM_CSCHEMAINMEMORY_H
