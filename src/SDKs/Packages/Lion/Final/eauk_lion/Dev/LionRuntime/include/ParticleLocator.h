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
// addressing is 16*(index+5)+this for rotation (-> mCacheQuat @0x50) and
// 16*(index+7)+this for translation (-> mCachePos @0x70), the cached-time
// word at this+0x98 and the key index at this+0x9C.
//
// ⭐⭐ 2026-09-03 -- THE MEMBER NAMES ARE NOW THE DWARF'S OWN, AND THAT CORRECTED TWO
// THINGS THIS HEADER HAD WRONG. DecFIGS declares the whole record
// (references/DecFIGS/dwarfdump/.../ParticleLocator.h:69-77) as, in order:
//   cMatrix mMat; cVector mVel; cQuat mCacheQuat[2]; cVector mCachePos[2];
//   cTime mCacheTime[2]; cTime mTime; U32 mIndex; U32 mFlags;
//   iLionPosEvaluator* mpPosEvaluator;
// which lands at 0x00 / 0x40 / 0x50 / 0x70 / 0x90 / 0x98 / 0x9C / 0xA0 / 0xA4 -- every
// offset Init and GetMat touch, in the same places. Two corrections fall straight out:
//
//   ⛔ `mKeyCount` DID NOT EXIST. Init's `stw r7, 0xA0(r3)` with r7 == 1 was read as
//      "mKeyCount = 1"; 0xA0 is mFlags, so the console is setting FLAG BIT 0, and the
//      word this header called mFlags (0xA4) is the mpPosEvaluator POINTER that Init
//      nulls (`stw r8, 0xA4(r3)`, r8 == 0). A phantom member and a one-slot shift.
//   ⭐ `mReserved40` ("Init zeroes it; role unresolved") is mVel, the locator's own
//      velocity -- the DWARF carries GetVel/SetVel accessors for it.
//
// X360 pointers/words are 32-bit; mpPosEvaluator widens on the host, so it is LAST and
// the offsets above are console facts, not host layout facts. Members are accessed BY
// NAME regardless -- everything before mpPosEvaluator is f32/u32 and so agrees on both.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"   // cVector -- the real eauk_common home (fork retired 2026-09-03)
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h"   // cMatrix -- ditto (fork retired 2026-09-03)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/ext-include/GameStructs/cTime.h"   // GetMat's key time
#include "GameSource/Math/CgsQuat.h"   // cQuat -- mCacheQuat's type (see the seam note below)

// ⚠ ONE PATH SEAM, SAID OUT LOUD. The DWARF puts cQuat in
// SDKs/Packages/Lion/Final/eauk_common/Maths/Quat.h and declares its converters as MEMBER
// functions over cMatrix -- `void ToMatrix(cMatrix&) const` / `void FromMatrix(const
// cMatrix&)` (quat_c.inl:2 / :103). This tree carries the same two bodies, reconstructed
// and gate-green, at GameSource/Math/CgsQuat.{h,cpp} as STATIC helpers over
// rw::math::vpu::Matrix44. The bodies are right; the file path and the declaration shape
// are not. Moving them is a mount change paired with a signature rewrite, so it is left as
// a follow-up and named here rather than papered over -- GetMat below therefore hands
// ToMatrix a Matrix44 and blends into mMat lane by lane, with no cast between the two.

// DWARF ParticleLocator.h:77 -- the pluggable position source a locator can be given.
// Pointer-only here; nothing in the reconstructed set dereferences it yet.
struct iLionPosEvaluator;

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
    // ⭐ BODIED 2026-09-03 (ParticleLocator.cpp). The reason this was parked ("the blend masks
    // each rotated basis row through the un-exported VMX mask blob unk_820FEBD0 ... not
    // recoverable from the exports") was a FILE'S OWN COMMENT BEING THE REGRESSION:
    // tools/re/x360rd.py reads that address in one command as
    // { FFFFFFFF, FFFFFFFF, FFFFFFFF, 00000000 } -- the ordinary xyz-keep / w-drop selector,
    // with 1.0f x4 in the quadword after it, which is the row-3 w that vsel puts back.
    const cMatrix& GetMat(const cTime& arKeyTime) const;

    // ----- members: the DWARF's own names and order (ParticleLocator.h:69-77), every
    //       offset verified against the X360 Init @0x82909810 / GetMat @0x8290E288 asm -----
    //
    // mMat and mTime are the CACHE GetMat fills, and GetMat is `const` in the DWARF while its
    // asm stores to this+0x00..0x3C and this+0x98. `mutable` is what that pair of facts means
    // in C++; it is not a liberty taken to make a const method compile.
    mutable cMatrix mMat;         // 0x00 current interpolated frame
    cVector mVel;                 // 0x40 locator velocity (DWARF GetVel/SetVel)
    cQuat   mCacheQuat[2];        // 0x50 / 0x60 keyframe rotation quaternions
    cVector mCachePos[2];         // 0x70 / 0x80 keyframe translations
    cTime   mCacheTime[2];        // 0x90 / 0x94 keyframe timestamps
    mutable cTime mTime;          // 0x98 last-sampled time (GetMat early-out + write-back)
    u32     mIndex;               // 0x9C current key index; the other key is mIndex ^ 1
    u32     mFlags;               // 0xA0 Init sets bit 0
    iLionPosEvaluator* mpPosEvaluator;   // 0xA4 console word; widens on the host
};
