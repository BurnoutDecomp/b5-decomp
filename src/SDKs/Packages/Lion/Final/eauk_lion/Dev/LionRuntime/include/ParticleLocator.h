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

// cVector now comes from its real home, eauk_common/Maths/Vector.h (included above);
// the private copy that used to live here is retired, and with it the warning to keep
// three copies token-for-token identical.

// HONEST PLACEHOLDER: row-major 4x4 matrix (four cVector rows, 64 bytes). GetMat
// writes the interpolated frame here; Init sets it to the identity.
struct cMatrix
{
    cVector maRows[4];
};

struct cParticleLocator
{
    // Reset to authoring defaults: identity matrix, two identity-rotation /
    // zero-translation keyframes, key count 1, everything else cleared. :0x82909810
    void Init();

    // Sample the animated frame for game time *apKeyTime: when the requested key
    // differs from the cached one, interpolate the two keyframes (quaternion ->
    // matrix blend) into mMatrix, cache the key, and return `this`. Body lives in
    // ParticleLocator.cpp. (Currently BLOCKED: the blend path masks each rotated
    // basis row through the un-exported VMX mask blob unk_820FEBD0 + vsel, which is
    // not recoverable from the exports -- see the campaign notes.)
    cParticleLocator* GetMat(const u32* apKeyTime);

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
