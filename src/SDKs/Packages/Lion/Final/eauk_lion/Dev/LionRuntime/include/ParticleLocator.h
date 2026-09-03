#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h
//
// cParticleLocator -- a Lion (eauk_lion) particle "locator": a small animated
// transform node that a particle effect samples for its spawn / parent frame. It
// caches a current 4x4 matrix and up to two keyframes (rotation quaternion +
// translation) that GetMat interpolates between by game time.
//
// LAYOUT AUTHORITY: every offset below is attested by the X360 asm for
//   cParticleLocator::Init   @ 0x82909810   (writes the whole struct to defaults)
// and cross-checked against cParticleLocator::GetMat @ 0x8290E288, whose keyframe
// addressing is 16*(index+5)+this for rotation (-> maKeyRotation @0x50) and
// 16*(index+7)+this for translation (-> maKeyTranslation @0x70), the cached-time
// word at this+0x98 and the key index at this+0x9C:
//
//   mMatrix        (4x4)              @0x00 (rows @0x00/0x10/0x20/0x30)
//   mReserved40    (cVector)         @0x40  (Init zeroes it; role unresolved)
//   maKeyRotation[2] (cVector/quat)  @0x50 / 0x60
//   maKeyTranslation[2] (cVector)    @0x70 / 0x80
//   maKeyTime[2]   (u32)             @0x90 / 0x94
//   mCachedTime    (u32)             @0x98
//   mKeyIndex      (u32)             @0x9C
//   mKeyCount      (u32)             @0xA0  (Init -> 1)
//   mFlags         (u32)             @0xA4
//
// X360 pointers/words are 32-bit; the host is 64-bit but this struct is all f32/u32
// (no pointers), so the byte offsets above hold on both. Members are accessed BY
// NAME regardless.
//
// HONEST PLACEHOLDER: cVector / cMatrix are the Lion math types not yet homed
// project-wide (a sibling Lion home, ParticleBehaviour.h, declares an identical
// HONEST-PLACEHOLDER cVector). To avoid a cross-header ODR clash this header does
// NOT include ParticleBehaviour.h; grow both into the single real cVector home
// additively when the Lion maths TU (vector_ps3.inl / matrix) is homed.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"   // cVector -- the real eauk_common home (fork retired 2026-09-03)
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h"   // cMatrix -- ditto (fork retired 2026-09-03)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/ext-include/GameStructs/cTime.h"   // GetMat's key time

// cVector now comes from its real home, eauk_common/Maths/Vector.h (included above);
// the private copy that used to live here is retired, and with it the warning to keep
// three copies token-for-token identical.

// cMatrix now comes from its real home, eauk_common/Maths/Matrix.h (included above). The
// private `struct cMatrix { cVector maRows[4]; }` that used to sit here was one of three
// forks -- and it was a HARD REDEFINITION of ParticleBucket.h's `struct cMatrix { f32
// m[16]; }`, which only ever escaped a diagnostic because no TU had reached both. Same
// bytes, one definition now; the rows carry the DWARF's own names (xa/ya/za/wa).

struct cParticleLocator
{
    // Reset to authoring defaults: identity matrix, two identity-rotation /
    // zero-translation keyframes, key count 1, everything else cleared. :0x82909810
    void Init();

    // Sample the animated frame for game time arKeyTime: when the requested key differs
    // from the cached one, interpolate the two keyframes (quaternion -> matrix blend) into
    // mMatrix, cache the key, and return it. X360 @0x8290E288.
    //
    // THE SIGNATURE IS CORRECTED TO THE DWARF'S (ParticleLocator.h:48):
    // `const cMatrix& GetMat(const cTime&) const`. It used to read
    // `cParticleLocator* GetMat(const u32*)`, which was the raw asm shape -- r3 comes back
    // holding `this` because the returned reference IS this->mMatrix at +0x00, and the time
    // argument looked like a `u32*` because it is a const reference to a one-word type. The
    // caller cParticleEmitter::InitialiseParticle @0x82911978 uses the result as a MATRIX
    // (it immediately reads +0x00..+0x28 as basis rows), not as a locator.
    //
    // STILL NOT BODIED -- but the reason recorded here was WRONG and is corrected. The old
    // note said the blend "masks each rotated basis row through the un-exported VMX mask blob
    // unk_820FEBD0 ... not recoverable from the exports". tools/re/x360rd.py reads it straight
    // out of the image: 0x820FEBD0 == { FFFFFFFF, FFFFFFFF, FFFFFFFF, 00000000 }, the ordinary
    // xyz-keep / w-drop selector, with 1.0f x4 in the quadword after it. Nothing about this
    // function is unrecoverable; it is 158 instructions of quaternion-to-matrix keyframe blend
    // that this wave ran out of room for. Its link trap is in LionRuntimeLinkStubs.cpp.
    const cMatrix& GetMat(const cTime& arKeyTime) const;

    // ----- members (offsets verified against the X360 Init/GetMat asm) -----
    cMatrix mMatrix;              // 0x00 current interpolated frame
    cVector mReserved40;          // 0x40 (Init zeroes; role unresolved)
    cVector maKeyRotation[2];     // 0x50 / 0x60 keyframe rotation quaternions
    cVector maKeyTranslation[2];  // 0x70 / 0x80 keyframe translations
    u32     maKeyTime[2];         // 0x90 / 0x94 keyframe timestamps
    u32     mCachedTime;          // 0x98 last-sampled key (GetMat early-out)
    u32     mKeyIndex;            // 0x9C current key index
    u32     mKeyCount;            // 0xA0 number of live keyframes
    u32     mFlags;               // 0xA4
};
