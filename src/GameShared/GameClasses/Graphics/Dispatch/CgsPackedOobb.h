#pragma once

// ===========================================================================
// CgsGraphics::PackedOobb -- a 16-byte packed oriented bounding box (position +
// quaternion + scale, bit-packed into one VMX register). OWNING home for the
// PackedOobb function the X360 binary defines in this TU:
//
//     CgsGraphics::PackedOobb::ToMatrix  @ 0x827EE300
//
// LAYOUT + INTERFACE are taken from the DecFIGS dwarfdump for
// GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.h: a single packed
// 16-byte member `mPackedBB`, five static permute-constant vectors, and the
// member functions Construct / LoadIntrinsic / CalcExponent / ToMatrix /
// MultiplyByMatrix / GetPosition. Only ToMatrix is bodied in this TU; the rest
// are declared (their addresses live in other TUs) so the class shape matches
// the DWARF.
//
// ToMatrix() decodes mPackedBB into a 4-row rw::math::vpu::Matrix44: it unpacks
// the bit-packed quaternion (signed fixed-point), normalises it (vmsum4fp ->
// vrsqrtefp + Newton-Raphson), unpacks the per-axis scale, builds the scaled
// rotation rows from the standard quaternion->matrix products, and unpacks the
// position into the translation (wAxis) row. The X360 body is dense VMX
// (vperm / vcfsx / vcfux / vrsqrtefp / vrlimi128 / vand sign-masks); following
// this project's CgsGeometric::Triangle4 precedent it is reconstructed
// SEMANTICALLY into portable named float maths, preserving the row STORE ORDER
// (wAxis @+0x30, xAxis @+0x00, yAxis @+0x10, zAxis @+0x20).
//
// RECOVERED, NOT INFERRED (2026-09-15, b5-decomp issue #26). This note used to say
// the five K_*_PERMUTE vectors "carry no values in the export" and that the decode
// reproduced the intent rather than the bit unpack. They carry no values because
// they are DYN-INIT .bss statics -- zero in the image by definition -- and their CRT
// initialisers at 0x82C6C598..0x82C6C6E4 hold them. They pin the packed 16 bytes
// exactly (console byte order):
//   [0..1]  position scale  -- the top 16 bits of an IEEE float
//   [2..7]  position x/y/z  -- s16 fixed, vcfsx 0x1F
//   [8]     scale exponent  -- lands at bit 23 after `vsrw 1` => 2^(b - 127)
//   [9..11] scale mantissa x/y/z -- u8, vcfux 0x18
//   [12..15] quaternion x/y/z/w  -- u8, vcfsx 0x1F
// See CgsPackedOobb.cpp for the constants themselves and the per-step mapping.
// ===========================================================================

#include "types.hpp"
#include <rw/math/vpu/types.h>

namespace CgsGraphics
{

class PackedOobb
{
public:
    // The packed bounding box: one 16-byte register. The four 32-bit lanes hold
    // the bit-packed position, quaternion and scale fields the decode unpacks.
    struct PackedRegister
    {
        u32 mauLane[4];
    };

    // @ 0x827EE300 -- decode this packed OOBB into roMatrix (4 rows xyz/w).
    // (X360 __fastcall: r3=this, r4=&roMatrix.)
    void ToMatrix(rw::math::vpu::Matrix44& roMatrix) const;

    // Declared to match the DWARF class shape; bodied in their own TUs.
    void                Construct(const rw::math::vpu::Vector3& roPosition,
                                  const void* lpQuaternion,
                                  const rw::math::vpu::Vector3& roScale);
    void                MultiplyByMatrix(const rw::math::vpu::Matrix44& roIn,
                                         rw::math::vpu::Matrix44& roOut) const;
    rw::math::vpu::Vector3 GetPosition() const;

    PackedRegister mPackedBB;   // +0x00  the packed OOBB register
};

} // namespace CgsGraphics
