#pragma once

// =====================================================================================
// rw::audio::core::Unpack0 -- the "format 0" variable-length integer decoder used by the
// EARenderWare "rwaudio" packed-stream readers (PackedColumnReader, EaXmaDec).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of
//   Unpack0::UnpackInt32  @0x82B6B818   (zig-zag SIGNED decode)
//   Unpack0::UnpackUInt32 @0x82B8F460   (UNSIGNED decode)
// is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist for this TU.
// Sibling to the committed rw::audio::core homes (PlugIn, Iir2, ...).
//
// ENCODING. A prefix-length varint keyed off the first byte src[0]:
//   src[0] < 0xC0 : 1 byte  -- 7-bit  payload, base 0
//   src[0] < 0xF0 : 2 bytes -- 13-bit payload, base 0xC0   (unsigned) / 96 (signed)
//   src[0] < 0xFC : 3 bytes -- 20-bit payload, base 0x30C0 (unsigned) / 6240 (signed)
//   src[0] < 0xFF : 4 bytes -- 26-bit payload, base 0xC30C0(unsigned)/393216(signed)
//   src[0] ==0xFF : 5 bytes -- full 32-bit payload (bytes 1..4), no base
// Each returns the number of bytes consumed (1..5) and writes the decoded value to
// *pOut. The SIGNED form (UnpackInt32) carries the sign in the payload's low bit
// (a zig-zag-style mapping): the payload is arithmetic-shifted right by 1, and if the
// low bit was set the result is bit-inverted (value = -1 - value).
// =====================================================================================

#include "types.hpp" // u8, u32, s32 / u32

namespace rw
{
namespace audio
{
namespace core
{

// FLAG (rwaudio PDB reconcile): PDB confirms `class rw::audio::core::Unpack0 [sizeof = 1] {}`
// -- empty (static-only) class, zero data members. Type name + stateless shape match; nothing to rename.
class Unpack0
{
public:
    // @ 0x82B6B818 -- decode a SIGNED 32-bit value from `src`, write it to `*pOut`,
    // return the number of bytes consumed (1..5). Sign lives in the payload low bit.
    static int UnpackInt32(const unsigned char* src, unsigned int* pOut);

    // @ 0x82B8F460 -- decode an UNSIGNED 32-bit value from `src`, write it to `*pOut`,
    // return the number of bytes consumed (1..5).
    static int UnpackUInt32(const unsigned char* src, unsigned int* pOut);
};

} // namespace core
} // namespace audio
} // namespace rw
