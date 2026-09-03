#pragma once

#include "BrnCommonTypes.h"   // Matrix44 (= rw::math::vpu::Matrix44)
#include "types.hpp"          // f32

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   cQuat::FromMatrix  @ 0x82908450   (matrix rotation -> quaternion; Shepperd's method)
//   cQuat::ToMatrix    @ 0x8290A9D8   (quaternion -> matrix rotation, fills a 4x4)
//
// cQuat is the engine's 4-float quaternion (x,y,z,w). Both routines are pure scalar FPU
// math on the X360 (lfs/stfs/fadds/fsubs/fmuls/fsqrts/fdivs/fsel) -- NOT SIMD -- so they
// reconstruct cleanly at semantic parity. Each is a free-standing static helper that
// takes the quaternion and a Matrix44 by reference; the X360 signatures pass both as
// `float*` (the hidden first arg is the quaternion `this`, the second is the matrix).
//
// Used by cParticleLocator (FromMatrix <- cParticleLocator::Update,
// ToMatrix <- cParticleLocator::GetMat).

struct cQuat
{
    f32 x, y, z, w;

    // Builds the quaternion from the rotation part of a 4x4 matrix (Shepperd's method:
    // pick the largest of the four diagonal-trace candidates for numerical stability).
    // @ 0x82908450
    static cQuat& FromMatrix(cQuat& lResult, const Matrix44& lMatrix);

    // Writes the quaternion's rotation into the upper-left 3x3 of lMatrix and clears the
    // translation row/column to an identity affine. Returns the quaternion unchanged.
    // @ 0x8290A9D8
    // ⚠ The DWARF (eauk_common/Maths/quat_c.inl:103) declares this as a CONST MEMBER over
    // cMatrix: `void ToMatrix(cMatrix&) const`. The body below does not touch the
    // quaternion, so the parameter is const here; the member-vs-static and cMatrix-vs-
    // Matrix44 halves of that correction are the open follow-up named in ParticleLocator.h.
    static const cQuat& ToMatrix(const cQuat& lQuat, Matrix44& lMatrix);
};
