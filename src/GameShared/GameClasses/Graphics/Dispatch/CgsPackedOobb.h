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
// FLAGGED inferred data: the five K_*_PERMUTE shuffles and the bit-field widths
// the asm uses (quaternion via vcfsx shift 0x1F == /2^31; scale via vcfux shift
// 0x18 == /2^24) are recovered from the asm shape; the .rdata permute vectors
// (X360 stru_83011130 / stru_83011220 / stru_83011370 / stru_830113A0 /
// stru_830113C0 / unk_82CDA3D0.. / unk_8327F130 / unk_830113C0) carry no values
// in the export. The decode reproduces the recovered intent (packed
// position/quaternion/scale -> scaled rotation matrix), not a byte-exact bit
// unpack -- see CgsPackedOobb.cpp for the per-step mapping.
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
