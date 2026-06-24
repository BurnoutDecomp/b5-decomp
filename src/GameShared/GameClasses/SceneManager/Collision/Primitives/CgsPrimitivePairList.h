#pragma once

// CgsSceneManager::CgsCollision::PrimitivePairList — a contiguous primitive-pair buffer
// handed to the collision contact generator: a base pointer to a 16-byte-aligned blob of
// packed primitive pairs plus its byte size.
//
// Layout recovered from the descriptor Prepare() asm that copies it into a job desc
// (PrimitivePairListJobDesc::Prepare @0x82810478,
//  PrimitiveListWithTriangleListJobDesc::Prepare @0x82810278). Three dwords are copied
// verbatim (0,4,8) and two fields are then observed:
//   - the BASE POINTER at +0x00 is alignment-checked: `*a1 % 16` must be 0
//     ("Primitive list not aligned to 16 bytes" / "Data stream not aligned to 16 bytes").
//   - the byte size at +0x04 is read as a HALFWORD (`lhz r11,4(r31)`) and rounded up to a
//     multiple of 16 ("Primitive list size is not a multiple of 16").
//
//   +0x00  u8*  base pointer (16-byte-aligned packed pair blob)
//   +0x04  u16  size in bytes
//   +0x06  u16  (reserved; the halfword above the size)
//   +0x08  u8*  secondary pointer copied with the record (third dword)
//
// The element-internal layout of one packed pair is not attested by these copy/validate
// paths, so only the asm-observed header fields are named.

#include "types.hpp"

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct PrimitivePairList
    {
        u8* mpData;          // +0x00  16-byte-aligned packed-pair blob (alignment-checked)
        u16 muSizeInBytes;   // +0x04  blob size in bytes (lhz; rounded to mult-of-16)
        u16 muReserved04;    // +0x06  halfword above the size
        u8* mpSecondary;     // +0x08  third dword copied with the record
    };
}
}
