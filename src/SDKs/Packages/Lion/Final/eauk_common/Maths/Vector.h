#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h
//
// cVector -- the Lion (eauk_common) 4-lane float vector, in the home the DecFIGS DWARF
// gives it and that three sibling Lion headers already named as "the real cVector home"
// while each carrying its own private copy:
//
//   ParticleBucket.h:67   ParticleBehaviour.h:75   ParticleLocator.h:53
//
// All three copies were token-for-token identical, and each carried the warning that a
// fourth copy -- or one that drifted -- would be an ODR fork that links silently. This
// header retires the fork rather than adding to it: cParticleRender (ParticleRender.h)
// needs cVector for mCamPos/mCamDir, and a fourth private copy would have made
// `#include ParticleBucketManager.h` + `#include ParticleRender.h` in one TU a hard
// redefinition -- which is precisely the wall cParticleRender::EmitterRender is parked on.
//
// LAYOUT AUTHORITY: the X360 ARTIST asm. The stride is 16 bytes wherever a cVector array
// is indexed (cParticleBucket::AllocateParticle @0x82908750 forms &mpVectors[count] with
// `slwi r9,r9,4`), and the 16-byte ALIGNMENT is an asm fact too -- it is what puts
// cParticleBehaviour::mAABBMin at +0x4A0 in Lerp @0x8290B1F8, i.e. what makes the record's
// attested 1216-byte size come out right (see the "sizeof is short by N -- check ALIGNMENT
// first" note in ParticleBehaviour.h).
//
// HONEST PLACEHOLDER STATUS IS UNCHANGED: this is still only as much of eauk_common's
// vector as the reconstructed Lion bodies touch (four named lanes at the attested stride
// and alignment). Grow it additively here -- never re-fork it into a consumer header.
// ============================================================================

#include "types.hpp"

struct alignas(16) cVector
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;

    // ------------------------------------------------------------------------------------
    // THE eauk ACCESSOR SET AND THE TWO LERP FAMILIES (grown here 2026-09-06, for
    // cParticleBehaviour::Lerp @0x8290B1F8).
    //
    // These are the DWARF's own declarations (references/DecFIGS/dwarfdump/SDKs/Packages/
    // Lion/Final/eauk_common/Maths/Vector.h -- Vector.h:68..78 for the accessors,
    // Vector.h:120/122 for Add/Mul, Vector.h:141 for Lerp, Vector.h:144/145/146/149 for the
    // "4" family), and every body below is read out of the X360 asm rather than assumed.
    //
    // ⭐⭐ THE TWO FAMILIES ARE NOT THE SAME OPERATION, AND THE DIFFERENCE IS AN ASM FACT,
    // NOT A TIDY-UP OPPORTUNITY. Lerp touches x/y/z only; Lerp4 touches all four lanes.
    // cParticleBehaviour::Lerp uses Lerp 29 times and Lerp4 exactly once (mRGBADiff), and the
    // console compiled them to visibly different code:
    //
    //   Lerp   0x8290B204..0x8290B294 (mAccBase, and 28 more of the same shape)
    //          three scalar `fsubs` write x/y/z, then ONE `lvx128 / vmulfp128 / stvx128`
    //          quad multiply, then three scalar `fadds` add x/y/z back.
    //   Lerp4  0x8290C3C0..0x8290C3E4 (mRGBADiff)
    //          `vsubfp` / `vmulfp128` / `vaddfp` -- every stage is a full quad.
    //
    // ⚠ SO THE W LANE OF A 3-LANE Lerp IS NOT LEFT ALONE: the quad multiply in the middle
    // reloads the DESTINATION's current w and stores `w * weight` back. That is a real,
    // observable side effect on a reused scratch object (cParticleEmitter::mpTempBehaviour is
    // lerped into on every frame whose blend position MOVED by more than 1% -- Blend's own
    // epsilon -- so its w lanes decay geometrically), and it is reproduced here
    // deliberately. Writing Mul as a 3-lane operation would silently "fix" the console.
    //
    // ⚠ PARAMETERS ARE `const cVector&`, NOT BY VALUE AS THE DWARF DECLARES THEM. The console
    // passes a cVector in a VMX register; MSVC x64 rejects a by-value parameter of a 16-byte
    // over-aligned type outright (C2719), and this type IS alignas(16) -- see the banner.
    // No call below aliases its destination with a source, and Mul/Add read each lane before
    // they write it, so the reference form is behaviourally identical.
    //
    // ⛔ NO CONSTRUCTORS. The DWARF declares cVector(FP32) and cVector(FP32,FP32,FP32,FP32)
    // and the console builds the weight splat with the first of them, but this type is
    // brace-initialised as an aggregate all over the reconstructed Lion runtime
    // (ParticleEmitter.cpp:2176/2177/2183/2320/2696/2697/2710). A user-provided constructor
    // would make it a non-aggregate and break every one of those. `Set` is the DWARF's own
    // spelling (Vector.h:85) and does the same four stores the splat constructor does.
    // ------------------------------------------------------------------------------------
    f32  GetX() const { return x; }                 // Vector.h:68
    void SetX(const f32 afX) { x = afX; }           // Vector.h:69
    f32  GetY() const { return y; }                 // Vector.h:71
    void SetY(const f32 afY) { y = afY; }           // Vector.h:72
    f32  GetZ() const { return z; }                 // Vector.h:74
    void SetZ(const f32 afZ) { z = afZ; }           // Vector.h:75
    f32  GetW() const { return w; }                 // Vector.h:77
    void SetW(const f32 afW) { w = afW; }           // Vector.h:78

    // Vector.h:85
    void Set(const f32 afX, const f32 afY, const f32 afZ, const f32 afW)
    {
        SetX(afX);
        SetY(afY);
        SetZ(afZ);
        SetW(afW);
    }

    // Vector.h:120 -- the THREE-lane add: `fadds` on x/y/z only (0x8290B268..0x8290B294).
    void Add(const cVector& arA, const cVector& arB)
    {
        SetX(arA.GetX() + arB.GetX());
        SetY(arA.GetY() + arB.GetY());
        SetZ(arA.GetZ() + arB.GetZ());
    }

    // Vector.h:122 -- the multiply is a WHOLE-QUAD `vmulfp128` even in the 3-lane family
    // (0x8290B258..0x8290B264). That asymmetry is the console's; see the banner above.
    void Mul(const cVector& arA, const cVector& arB)
    {
        x = arA.x * arB.x;
        y = arA.y * arB.y;
        z = arA.z * arB.z;
        w = arA.w * arB.w;
    }

    // Vector.h:141 -- *this = arA + (arB - arA) * arT, on x/y/z; w becomes w * arT.w.
    // asm 0x8290B204..0x8290B294 and 28 identical blocks after it.
    void Lerp(const cVector& arA, const cVector& arB, const cVector& arT)
    {
        SetX(arB.GetX() - arA.GetX());
        SetY(arB.GetY() - arA.GetY());
        SetZ(arB.GetZ() - arA.GetZ());
        Mul(*this, arT);
        Add(*this, arA);
    }

    // Vector.h:144/145/146 -- the four-lane family. `vaddfp` / `vsubfp` / `vmulfp128`.
    void Add4(const cVector& arA, const cVector& arB)
    {
        x = arA.x + arB.x;
        y = arA.y + arB.y;
        z = arA.z + arB.z;
        w = arA.w + arB.w;
    }

    void Sub4(const cVector& arA, const cVector& arB)
    {
        x = arA.x - arB.x;
        y = arA.y - arB.y;
        z = arA.z - arB.z;
        w = arA.w - arB.w;
    }

    void Mul4(const cVector& arA, const cVector& arB)
    {
        x = arA.x * arB.x;
        y = arA.y * arB.y;
        z = arA.z * arB.z;
        w = arA.w * arB.w;
    }

    // Vector.h:149 -- all four lanes lerped. asm 0x8290C3C0..0x8290C3E4.
    void Lerp4(const cVector& arA, const cVector& arB, const cVector& arT)
    {
        Sub4(arB, arA);
        Mul4(*this, arT);
        Add4(*this, arA);
    }
};
