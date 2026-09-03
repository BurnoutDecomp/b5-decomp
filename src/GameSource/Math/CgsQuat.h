#pragma once

#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h"   // cMatrix -- the DWARF's own parameter type
#include "types.hpp"          // f32

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   cQuat::FromMatrix  @ 0x82908450   (matrix rotation -> quaternion; Shepperd's method)
//   cQuat::ToMatrix    @ 0x8290A9D8   (quaternion -> matrix rotation, fills a 4x4)
//
// cQuat is the engine's 4-float quaternion (x,y,z,w). Both routines are pure scalar FPU
// math on the X360 (lfs/stfs/fadds/fsubs/fmuls/fsqrts/fdivs/fsel) -- NOT SIMD -- so they
// reconstruct cleanly at semantic parity.
//
// ⭐ 2026-09-03 -- THE SHAPE IS CORRECTED TO THE DWARF'S, AND THE ASM AGREES WITH IT. These
// were static helpers over rw::math::vpu::Matrix44 because "the X360 signatures pass both
// as float*". They pass r3 = quaternion and r4 = matrix -- which is EXACTLY what a member
// `void ToMatrix(cMatrix&) const` compiles to, with the quaternion as the implicit this.
// DecFIGS declares them that way (eauk_common/Maths/quat_c.inl:2 and :103), over cMatrix,
// which now has one home; both halves of the old spelling are retired here.
//
// ⚠ ONE THING IS STILL WRONG AND IS NAMED RATHER THAN HIDDEN: the PATH. The DWARF puts
// cQuat in SDKs/Packages/Lion/Final/eauk_common/Maths/Quat.h, not GameSource/Math. Moving
// the TU is a build-mount change, so it is left as a follow-up.
//
// Used by cParticleLocator (FromMatrix <- cParticleLocator::Update,
// ToMatrix <- cParticleLocator::GetMat).

struct cQuat
{
    f32 x, y, z, w;

    // Builds the quaternion from the rotation part of a 4x4 matrix (Shepperd's method:
    // pick the largest of the four diagonal-trace candidates for numerical stability).
    // @ 0x82908450 (DWARF quat_c.inl:2)
    void FromMatrix(const cMatrix& arMatrix);

    // Writes this quaternion's rotation into the upper-left 3x3 of arMatrix and clears the
    // translation row/column to an identity affine.
    // @ 0x8290A9D8 (DWARF quat_c.inl:103)
    void ToMatrix(cMatrix& arMatrix) const;
};
