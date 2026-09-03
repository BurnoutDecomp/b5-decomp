#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h
//
// cMatrix -- the Lion (eauk_common) 4x4 matrix, in the home the DecFIGS DWARF gives it
// (references/DecFIGS/dwarfdump/SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h:51),
// and the retirement of a THREE-WAY ODR FORK that three committed Lion headers were
// carrying between them:
//
//   ParticleBucket.h:81   `struct cMatrix { f32 m[16]; }`
//   ParticleLocator.h:59  `struct cMatrix { cVector maRows[4]; }`
//   ParticleRender.h:45   `typedef rw::math::vpu::Matrix44 cMatrix;`
//
// ⛔⛔ TWO OF THOSE WERE ALREADY A HARD REDEFINITION OF EACH OTHER and only avoided a
// diagnostic because no TU had yet reached both. That is exactly the failure mode the
// project's own note calls out -- "ODR forks link silently" -- and it is what parked
// cParticleRender::EmitterRender / EmitterCubeRender and kept cParticleEmitter.h from
// declaring its own `cMatrix mParentBaseMatrix`, `sParticleNucleus mParentEmitterNucleus`
// and `ParticleBuildData mPrecalculatedParticleBuildData` as anything but reserved spans.
//
// ⭐ THE THREE FORKS WERE LAYOUT-IDENTICAL, so unifying them changes no byte: four
// 16-byte, 16-aligned rows, 64 bytes total. That is corroborated three ways --
//   * the DWARF names the members xa / ya / za / wa, all cVector (Matrix.h:143);
//   * cParticleBucket's per-particle matrix side array is indexed with `slwi r10,r10,6`
//     (stride 64) in cParticleBucket::AllocateParticle @0x82908750;
//   * cParticleLocator::Init @0x82909810 writes the identity as sixteen `stfs` at
//     +0x00..0x3C, and cParticleLocator::GetMat @0x8290E288 addresses its keyframes at
//     16*(index+5) -- i.e. the matrix occupies exactly +0x00..0x40.
//
// ⚠ THE ROW NAMES ARE THE DWARF'S, NOT A GUESS, and they are NOT "row 0..3": the original
// spells them xa/ya/za/wa (x-axis, y-axis, z-axis, w-axis == translation). Accessors below
// keep those names. Only the DWARF methods a reconstructed body actually reaches are
// declared (attestation rule, AGENTS.md); the full DWARF method set is ~50 entries of
// Set*axis*/Get*/Mul/Invert/Transpose/Lerp and is to be grown here as bodies land.
//
// HONEST PLACEHOLDER STATUS: this is as much of eauk_common's matrix as the reconstructed
// Lion bodies touch. Grow it additively HERE -- never re-fork it into a consumer header.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"   // cVector -- the row type

// DWARF Matrix.h:51. Aggregate (no user-declared constructor) so the reconstructed bodies
// can brace-initialise it and so it stays trivially copyable, which is what the console's
// four `lvx/stvx` row copies are.
struct cMatrix
{
    cVector xa;   // +0x00  DWARF Matrix.h:143
    cVector ya;   // +0x10
    cVector za;   // +0x20
    cVector wa;   // +0x30  the translation row

    // DWARF Matrix.h:81-84. Read accessors the reconstructed bodies use; the X360 build
    // reads the fields directly (no out-of-line call), so these are inline by construction.
    const cVector& GetAxisX() const { return xa; }
    const cVector& GetAxisY() const { return ya; }
    const cVector& GetAxisZ() const { return za; }
    const cVector& GetAxisW() const { return wa; }
};

// 64 bytes, 16-aligned: the stride cParticleBucket::AllocateParticle @0x82908750 indexes
// the per-particle matrix array with (`slwi r10,r10,6`), and the span
// cParticleLocator::GetMat @0x8290E288 leaves before its first keyframe at +0x50.
static_assert(sizeof(cMatrix) == 64,
              "cMatrix is the 64-byte 4x4 (AllocateParticle @0x82908750 slwi r10,r10,6)");
static_assert(alignof(cMatrix) == 16, "cMatrix rows are VMX lane registers");
