#pragma once

// Canonical home for BrnReplays::QuantisedVector4Compression.
// DWARF/assert home path: GameSource/Replays/BrnReplayQuantisedVector4.h
// (the X360 ctor's per-component asserts cite this file at lines 132-135).
//
// A per-component quantisation descriptor for a 4-lane Vector4: it records, per
// component, how many bits that component is quantised to (maBitCounts), and the
// component-wise quantisation range [mMin, mMax]. A later (un-emitted in the boot
// trace) Compress/Decompress step maps a Vector4 lane into its [mMin,mMax] range
// scaled across maBitCounts[i] bits.
//
// LAYOUT (X360 asm authoritative @ 0x8264C7E8 -- the ctor's store displacements pin
// every offset):
//   @0x00  u8       maBitCounts[4]   // stb r4..r7, 0..3(r31): one bit-count per lane
//   @0x04..0x0F      (alignment pad so the 16-byte vector stores land on +16/+32)
//   @0x10  Vector4  mMin             // stvx128 v127(lMin), r31, 16
//   @0x20  Vector4  mMax             // stvx128 v126(lMax), r31, 32
// sizeof == 48 (0x30). The two Vector4s are 16-byte SIMD registers (rw::math::vpu),
// so they are 16-aligned -- the struct is therefore alignas(16) and the bit-count
// bytes sit in the leading 16-byte slot with the remaining 12 bytes as padding.
//
// The ctor takes the min/max vectors by const reference (passed in VMX regs v1/v2 on
// X360) plus four char bit-counts, and asserts lMin[i] < lMax[i] for each lane i. The
// X360 body expresses each per-lane compare as a vperm-extract + vcmpgtfp (lMax[i] >
// lMin[i]); the scalar reconstruction reads the named Vector4 lane directly.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (== rw::math::vpu::Vector4)

namespace BrnReplays
{
    // Free-function packer used by TrafficEntitySerialiser::WritePhysicsInfo
    // @0x8265AE98: `QuantisedVector4::Pack(out, &gDetachedPartCompression, vec...)`
    // quantises a Vector4 through a QuantisedVector4Compression descriptor into the
    // packed output buffer. The descriptor it is called with is a global bit-count
    // record (byte_82FFA5C0..C3, the four per-lane bit counts); the written byte count
    // is (sum-of-bit-counts + 7) / 8. No DWARF recovered for the packer; declared here
    // (its home), body in the (not-yet reconstructed) QuantisedVector4 TU.
    struct QuantisedVector4Compression;
    namespace QuantisedVector4
    {
        void Pack(void* lpDest, const QuantisedVector4Compression* lpCompression,
                  const Vector4& lrValue);
    }

    struct alignas(16) QuantisedVector4Compression
    {
        // X360 0x8264C7E8: store the 4 per-lane bit counts (+0..+3), then assert
        // lMin[i] < lMax[i] for each lane and store mMin(+16)/mMax(+32).
        QuantisedVector4Compression(const Vector4& lrMin, const Vector4& lrMax,
                                    u8 luBitsX, u8 luBitsY, u8 luBitsZ, u8 luBitsW);

        // Layout pins (compiled into the .cpp).
        static void _AssertLayout();

        u8      maBitCounts[4];   // +0x00  one quantisation bit-count per lane
        Vector4 mMin;             // +0x10  per-lane quantisation range minimum
        Vector4 mMax;             // +0x20  per-lane quantisation range maximum
    };
}
