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

    // ---- the four methods cParticleEmitter::InitialiseParticle @0x829116A8 reaches -------
    // Every one of these is INLINED at its X360 call site (none has an out-of-line body in
    // the image), so the arithmetic below is read out of that call site and de-inlined back
    // to the owning type -- which is where the DWARF says the source put it. The DWARF names
    // for InitialiseParticle list exactly BuildIdentity / SetTrans / Transpose3x3 /
    // ApplyAxes, in this order, so these are the original's own four methods, not helpers
    // invented to tidy the asm up.

    // DWARF Matrix.h:125. Asm 0x82911B24..0x82911B70: sixteen `stfs` of flt_82001C98 (1.0,
    // read out of the image) on the diagonal and flt_82001CC0 (0.0) everywhere else.
    void BuildIdentity()
    {
        xa.x = 1.0f; xa.y = 0.0f; xa.z = 0.0f; xa.w = 0.0f;
        ya.x = 0.0f; ya.y = 1.0f; ya.z = 0.0f; ya.w = 0.0f;
        za.x = 0.0f; za.y = 0.0f; za.z = 1.0f; za.w = 0.0f;
        wa.x = 0.0f; wa.y = 0.0f; wa.z = 0.0f; wa.w = 1.0f;
    }

    // DWARF Matrix.h:32. Asm 0x82911B74..0x82911B8C: three `stfs` into +0x30/+0x34/+0x38
    // followed by 1.0 into +0x3C -- the w lane is FORCED to 1, not left as it was (the store
    // is emitted even though BuildIdentity has just written the same value there).
    void SetTrans(f32 afX, f32 afY, f32 afZ)
    {
        wa.x = afX; wa.y = afY; wa.z = afZ; wa.w = 1.0f;
    }

    // DWARF Matrix.h:176. Transpose of the upper-left 3x3; the w row and the w lanes are
    // carried through untouched. InitialiseParticle composes this with ApplyAxes below, and
    // the composition is what its asm actually emits: `out.x = m0*v.x + m1*v.y + m2*v.z`
    // (0x829119D4 fmadds chain) is ApplyAxes over the TRANSPOSED axes, and the console just
    // never materialises the intermediate.
    cMatrix Transpose3x3() const
    {
        cMatrix lResult = *this;
        lResult.xa.x = xa.x; lResult.xa.y = ya.x; lResult.xa.z = za.x;
        lResult.ya.x = xa.y; lResult.ya.y = ya.y; lResult.ya.z = za.y;
        lResult.za.x = xa.z; lResult.za.y = ya.z; lResult.za.z = za.z;
        return lResult;
    }

    // DWARF Matrix.h:191. Rotate a vector by the 3x3 axes -- v.x*xa + v.y*ya + v.z*za, w
    // untouched. Asm 0x829119A8..0x829119F0: three interleaved fmadds chains, one per output
    // lane, over the axes' x/y/z components; the source vector's w never enters.
    cVector ApplyAxes(cVector avVector) const
    {
        cVector lResult;
        lResult.x = xa.x * avVector.x + ya.x * avVector.y + za.x * avVector.z;
        lResult.y = xa.y * avVector.x + ya.y * avVector.y + za.y * avVector.z;
        lResult.z = xa.z * avVector.x + ya.z * avVector.y + za.z * avVector.z;
        lResult.w = avVector.w;
        return lResult;
    }
};

// 64 bytes, 16-aligned: the stride cParticleBucket::AllocateParticle @0x82908750 indexes
// the per-particle matrix array with (`slwi r10,r10,6`), and the span
// cParticleLocator::GetMat @0x8290E288 leaves before its first keyframe at +0x50.
static_assert(sizeof(cMatrix) == 64,
              "cMatrix is the 64-byte 4x4 (AllocateParticle @0x82908750 slwi r10,r10,6)");
static_assert(alignof(cMatrix) == 16, "cMatrix rows are VMX lane registers");
