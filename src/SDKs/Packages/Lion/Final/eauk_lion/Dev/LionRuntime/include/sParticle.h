#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/sParticle.h
//
// sParticleNucleus -- ONE live Lion (eauk_lion) particle's simulation state. This is the
// record every part of the Lion engine walks: cParticleBucket stores sixteen of them,
// cParticleEmitter::InitialiseParticle fills one, cParticleEmitter::ParticleBuild reads one
// per frame to produce a RenderedParticle, and the Simulate*ParticlesInBucket family
// integrates them.
//
// ⭐⭐ IT WAS AN OPAQUE 224-BYTE BLOB UNTIL NOW, and that is why nothing downstream of it
// could be reconstructed. ParticleBucket.h carried
//   `struct sParticleNucleus { u8 mau8Opaque[0xE0]; };  // HONEST PLACEHOLDER`
// with the correct STRIDE and no members -- enough for AllocateParticle to form
// `&mParticles[count]`, and not enough for a single body that reads a particle.
//
// LAYOUT AUTHORITY: the DecFIGS DWARF (sParticle.h:22) declares the whole record -- fourteen
// members, thirteen Vector3Plus plus one Vector4, in this order. 14 * 16 == 224 == 0xE0,
// which is exactly the stride cParticleBucket::AllocateParticle @0x82908750 indexes the
// nucleus array with (`mulli r10, r10, 0xE0`). The size the placeholder was built around and
// the size the DWARF member list produces are the same number, reached independently.
//
// ⭐⭐⭐ AND THE ORDER IS CONFIRMED STORE-FOR-STORE, not merely assumed from the DWARF's
// listing order. cParticleEmitter::InitialiseParticle @0x829116A8 writes twelve of the
// fourteen slots in one straight run, each from a NAMED (base, variance) pair of the
// behaviour record whose offsets ParticleBehaviour.h already pins:
//
//     store @ nucleus+0x00  <- Build(mPosBase           @0x100, mPosVariance           @0x110)
//     store @ nucleus+0x10  <- Build(mVelBase           @0x190, mVelVariance           @0x1A0)
//     store @ nucleus+0x20  <- Build(mAccBase           @0x000, mAccVariance           @0x010)
//     store @ nucleus+0x30  <- Build(mRotXYZBase        @0x090, mRotXYZVariance        @0x0A0)
//     store @ nucleus+0x40  <- Build(mRotXYZVelBase     @0x0B0, mRotXYZVelVariance     @0x0C0)
//     store @ nucleus+0x50  <- Build(mRotXYZAccBase     @0x0D0, mRotXYZAccVariance     @0x0E0)
//     store @ nucleus+0x60  <- Build(mOffsetRotXYZBase  @0x030, mOffsetRotXYZVariance  @0x040)
//     store @ nucleus+0x70  <- Build(mOffsetRotXYZVelBase @0x050, ...VelVariance       @0x060)
//     store @ nucleus+0x80  <- Build(mOffsetRotXYZAccBase @0x070, ...AccVariance       @0x080)
//     store @ nucleus+0x90  <- Build(mSizeXYZBase       @0x130, mSizeXYZVariance       @0x140)
//     store @ nucleus+0xA0  <- Build(mSizeXYZVelBase    @0x150, mSizeXYZVelVariance    @0x160)
//     store @ nucleus+0xB0  <- Build(mSizeXYZAccBase    @0x170, mSizeXYZAccVariance    @0x180)
//     store @ nucleus+0xC0  <- arVector * mEmitterVelWeight  (the locator velocity)
//     store @ nucleus+0xD0  <- the packed life/frame/FPS/birth lanes (vrlimi128, see below)
//
// Twelve sources whose names match twelve destination names in the DWARF's order, plus the
// thirteenth (mLocatorVel, scaled by the behaviour's mEmitterVelWeight @1176) and the
// fourteenth. Nothing about that mapping is a choice.
//
// ⭐ THE FOURTEENTH MEMBER IS FOUR PACKED LANES AND THE ASM NAMES EACH ONE. The DWARF spells
// it `mvLifeTimeAndFrameTimeAndFPSAndBirthTime`, and InitialiseParticle writes three of the
// four with `vrlimi128 vD, vS, <mask>, 0` -- whose 4-bit mask selects a word, MSB-first
// (8 == word0 ... 1 == word3):
//     mask 8 (word 0, x) <- cParticleRandomSeed::Build(behaviour->mLifeBase, mLifeVariance)
//     mask 2 (word 2, z) <- cParticleRandomSeed::Build(material->mFPS, material->mFPSVariance)
//     mask 1 (word 3, w) <- (f32)arTime.ticks * (1/3000)     [flt_82F369A8 == 0.000333333]
// i.e. x == LifeTime, z == FPS, w == BirthTime, in the member name's own order, with word 1
// (FrameTime) left at the zero the function's opening `stvx128 v127(=0)` wrote. Named lanes
// are provided below so no body has to index this vector by number.
//
// X360 pointers are 32-bit and the host widens them -- but this record contains NO pointers,
// only 16-byte lane registers, so its console offsets ARE its host offsets. They are pinned
// by static_assert at the foot of this file.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"   // cVector -- the lane type

#include <cstddef>   // offsetof

// DWARF sParticle.h:22. The DWARF types the first thirteen members `Vector3Plus` and the
// last `Vector4`; both are the same 16-byte, 16-aligned lane register the rest of the Lion
// runtime spells `cVector`, and every access in the reconstructed bodies is a whole-register
// load or store, so the one Lion vector home is used throughout rather than pulling a second
// vocabulary in for a distinction this code never makes.
struct sParticleNucleus
{
    cVector mPos;             // +0x00  sParticle.h:35
    cVector mVel;             // +0x10  sParticle.h:36
    cVector mAcc;             // +0x20  sParticle.h:37
    cVector mRot;             // +0x30  sParticle.h:39
    cVector mRotVel;          // +0x40  sParticle.h:40
    cVector mRotAcc;          // +0x50  sParticle.h:41
    cVector mOffsetRot;       // +0x60  sParticle.h:43
    cVector mOffsetRotVel;    // +0x70  sParticle.h:44
    cVector mOffsetRotAcc;    // +0x80  sParticle.h:45
    cVector mSize;            // +0x90  sParticle.h:47
    cVector mSizeVel;         // +0xA0  sParticle.h:48
    cVector mSizeAcc;         // +0xB0  sParticle.h:49
    cVector mLocatorVel;      // +0xC0  sParticle.h:51
    cVector mvLifeTimeAndFrameTimeAndFPSAndBirthTime;   // +0xD0  sParticle.h:65

    // sParticle.h:23 -- the DWARF declares Init(); no X360 body is attested for it (the
    // console clears the record inline at each call site), so it is NOT declared here.
    // Named lanes for the packed member above, so no reconstructed body indexes it by
    // number. The lane<->name mapping is the vrlimi128 masks quoted in the banner.
    f32& LifeTime()        { return mvLifeTimeAndFrameTimeAndFPSAndBirthTime.x; }
    f32& FrameTime()       { return mvLifeTimeAndFrameTimeAndFPSAndBirthTime.y; }
    f32& FPS()             { return mvLifeTimeAndFrameTimeAndFPSAndBirthTime.z; }
    f32& BirthTime()       { return mvLifeTimeAndFrameTimeAndFPSAndBirthTime.w; }
    f32  LifeTime()  const { return mvLifeTimeAndFrameTimeAndFPSAndBirthTime.x; }
    f32  FrameTime() const { return mvLifeTimeAndFrameTimeAndFPSAndBirthTime.y; }
    f32  FPS()       const { return mvLifeTimeAndFrameTimeAndFPSAndBirthTime.z; }
    f32  BirthTime() const { return mvLifeTimeAndFrameTimeAndFPSAndBirthTime.w; }
};

// The nucleus array stride cParticleBucket::AllocateParticle @0x82908750 indexes with
// (`mulli r10, r10, 0xE0`), and the sum of the DWARF's fourteen 16-byte members.
static_assert(sizeof(sParticleNucleus) == 0xE0,
              "sParticleNucleus is the 224-byte nucleus (AllocateParticle @0x82908750 "
              "mulli r10,r10,0xE0)");
// Each offset below is the destination of one InitialiseParticle @0x829116A8 store, in the
// order the asm issues them (see the banner's table). Break one and the gate fails here
// rather than the game failing somewhere downstream.
static_assert(offsetof(sParticleNucleus, mPos)          == 0x00, "InitialiseParticle store 1");
static_assert(offsetof(sParticleNucleus, mVel)          == 0x10, "InitialiseParticle store 2");
static_assert(offsetof(sParticleNucleus, mAcc)          == 0x20, "InitialiseParticle store 3");
static_assert(offsetof(sParticleNucleus, mRot)          == 0x30, "InitialiseParticle store 4");
static_assert(offsetof(sParticleNucleus, mRotVel)       == 0x40, "InitialiseParticle store 5");
static_assert(offsetof(sParticleNucleus, mRotAcc)       == 0x50, "InitialiseParticle store 6");
static_assert(offsetof(sParticleNucleus, mOffsetRot)    == 0x60, "InitialiseParticle store 7");
static_assert(offsetof(sParticleNucleus, mOffsetRotVel) == 0x70, "InitialiseParticle store 8");
static_assert(offsetof(sParticleNucleus, mOffsetRotAcc) == 0x80, "InitialiseParticle store 9");
static_assert(offsetof(sParticleNucleus, mSize)         == 0x90, "InitialiseParticle store 10");
static_assert(offsetof(sParticleNucleus, mSizeVel)      == 0xA0, "InitialiseParticle store 11");
static_assert(offsetof(sParticleNucleus, mSizeAcc)      == 0xB0, "InitialiseParticle store 12");
static_assert(offsetof(sParticleNucleus, mLocatorVel)   == 0xC0, "InitialiseParticle store 13");
static_assert(offsetof(sParticleNucleus, mvLifeTimeAndFrameTimeAndFPSAndBirthTime) == 0xD0,
              "InitialiseParticle's `addi r28, r29, 0xD0` life/frame/FPS/birth register");
