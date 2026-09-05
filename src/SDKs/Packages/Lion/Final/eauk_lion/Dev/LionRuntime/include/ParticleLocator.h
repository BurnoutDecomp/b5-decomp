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

#include <cstddef>   // offsetof -- the record's offset pins at the bottom of this file

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
//
// ⭐ IT IS NOW DEREFERENCED, so it needs its one virtual. cParticleEmitter::ParticleBuild
// @0x82910118 asks the locator for it (`lwz r3, 0xA4(r11)`) and, when it is non-null, calls
// VTABLE SLOT 0 (`lwz r11, 0(r3)` then `lwz r11, 0(r11)` then `bctrl`, 0x829103C0..0x829103E0)
// INSTEAD of integrating the particle -- so an attached evaluator owns the particle's motion
// entirely. The parameter list is read off the register set-up at that call site: two floats in
// f1/f2 (the particle's scaled age and its normalised life) and three vectors in v1/v2/v3 (the
// nucleus position, velocity and acceleration, BY VALUE -- a reference would have consumed a
// GPR and none is set). The console discards the result.
//
// ⚠ THE METHOD *NAME* IS DERIVED, not attested: the X360 has no symbol for it and the DecFIGS
// DWARF declares the type only as an opaque pointer (ParticleLocator.h:29/42/45 and
// LionFX.h:53's PosEvaluatorAttach). The SHAPE is the asm's; `Evaluate` is this project's name
// for what the interface's own name says it does.
struct iLionPosEvaluator
{
    virtual void Evaluate(f32 afParticleAge,
                          f32 afLifeFraction,
                          cVector avPos,
                          cVector avVel,
                          cVector avAcc) = 0;
};

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
    // mFlags bits, both read off the branch each one guards in
    // cParticleLocator::Update @0x829098D0. The masks are asm facts; the NAMES are this
    // project's, chosen from what the guarded code does.
    enum Flags
    {
        // `clrlwi r11, r11, 31` @0x82909940. Set by Init (mFlags = 1). While set, Update
        // publishes a ZERO velocity and clears the bit, so no velocity is ever measured
        // across a discontinuity. The DWARF's Teleport is the obvious other setter --
        // inference, not an assertion: Teleport is not reconstructed.
        E_FLAG_VELOCITY_INVALID  = 0x1,
        // `rlwinm r11, r11, 0,30,30` @0x82909930. While set, Update stores the keyframe and
        // returns -- it neither derives nor clears mVel, so the velocity is somebody else's.
        E_FLAG_EXTERNAL_VELOCITY = 0x2,
    };

    // Reset to authoring defaults: identity matrix, two identity-rotation /
    // zero-translation keyframes, mFlags bit 0 set, everything else cleared. :0x82909810
    void Init();

    // Push a new keyframe (arMat's translation + rotation, stamped arTime) into the
    // ping-pong pair, flip mIndex, and derive mVel from the two stamps that now bracket
    // the present. X360 @0x829098D0 (DWARF ParticleLocator.h:41). RECONSTRUCTED.
    void Update(const cMatrix& arMat, const cTime& arTime);

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

    // ParticleLocator.h:42 (DWARF) -- the attached position source, or null. The X360 reads the
    // field directly (`lwz r3, 0xA4(r11)` in cParticleEmitter::ParticleBuild @0x829103B4), so
    // this is inline by construction.
    iLionPosEvaluator* GetpPosEvaluator() const { return mpPosEvaluator; }

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

// Every member up to mpPosEvaluator is f32/u32, so the console offsets ARE the host offsets
// and each one is pinned to the instruction that proves it. Only the trailing pointer widens,
// which is why sizeof is deliberately not asserted. Break a member and the gate fails here,
// not in the game.
static_assert(offsetof(cParticleLocator, mVel)        == 0x40,
              "Init @0x82909874 zeroes +0x40..+0x4C right after the identity matrix");
static_assert(offsetof(cParticleLocator, mCacheQuat)  == 0x50,
              "GetMat @0x8290E354 forms the rotation key address as 16*(index+5)+this");
static_assert(offsetof(cParticleLocator, mCachePos)   == 0x70,
              "GetMat @0x8290E398 forms the translation key address as 16*(index+7)+this");
static_assert(offsetof(cParticleLocator, mCacheTime)  == 0x90,
              "GetMat @0x8290E2BC indexes the key times at 4*(index+0x24)+this");
static_assert(offsetof(cParticleLocator, mTime)       == 0x98,
              "GetMat @0x8290E2A4 reads the cached time with lwz 0x98(r31)");
static_assert(offsetof(cParticleLocator, mIndex)      == 0x9C,
              "GetMat @0x8290E2B4 reads the key index with lwz 0x9C(r31)");
static_assert(offsetof(cParticleLocator, mFlags)      == 0xA0,
              "Init @0x8290988C stores 1 here -- flag bit 0, NOT a phantom mKeyCount");

// ⚠ mpPosEvaluator IS THE ONE MEMBER WITH NO OFFSET PIN, AND THAT IS DELIBERATE. The console
// puts it at +0xA4, straight after a u32; on the host it is an 8-byte pointer, so the compiler
// pads mFlags out and it lands at +0xA8. Asserting the console offset here FAILS THE GATE --
// which is how this note came to be written, and it is the corroboration that the assert block
// above is live rather than decorative. Nothing may dereference this record at a console byte
// offset. (Init @0x82909890 nulls the word; it is what this header used to call mFlags.)
