#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleSystem.h
//
// cParticleSystem -- the Lion (eauk_lion) runtime's app-lifetime owner. Its two ledger
// bodies (AppInit @0x82913810, AppDeInit @0x82911DF0) are the single place the runtime's
// pools and manager singletons are created and torn down; cLionFX::Init / cLionFX::DeInit
// are the only callers.
//
// LAYOUT AUTHORITY: the DecFIGS DWARF (ParticleSystem.h:27, members h:49..51) plus the
// singleton `mSingleton` (h:47), whose X360 storage is 0x83121B44 -- the address
// cLionFX::DeInit @0x82912B18 passes to AppDeInit.
//
// ⚠ NEITHER BODY TOUCHES A SINGLE MEMBER. The X360 AppInit and AppDeInit dereference the
// `this` they are handed exactly zero times: everything they do is to OTHER singletons and
// module pools. The three members below are therefore declaration shape from the DWARF and
// nothing more -- writing them here would be an invented arm.
//
// X360 pointers are 32-bit; on the host they widen. Members are pinned BY NAME/SEQUENCE.
//
// Vendor code (eauk_lion), reconstructed in its canonical Lion home. GameInit/GameDeInit
// (DWARF h:33/34) and Update (h:38) have no X360 ledger row of their own -- they are
// inlined or ICF-folded away -- and are deliberately NOT declared here.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

class cParticleEmitter;   // ParticleEmitter.h (sibling home) -- pointer only here

// DecFIGS DWARF ParticleSystem.h:27.
struct cParticleSystem
{
public:
    // DWARF h:43. The X360 build reaches mSingleton's storage (0x83121B44) directly from
    // cLionFX::Init / cLionFX::DeInit, which is what an inlined GetMe() looks like.
    static cParticleSystem* GetMe();

    // DWARF h:31. X360 @0x82913810. Create every pool and manager the Lion runtime owns.
    void AppInit(EA::Allocator::ITaggedAllocator* apAllocator,
                 u32 auEmitterCount,
                 u32 auBucketCount,
                 u32 auMatrixBucketCount);

    // DWARF h:32. X360 @0x82911DF0. The inverse, in the console's own order.
    void AppDeInit();

private:
    u32                              mEmitterCount;  // ParticleSystem.h:49
    cParticleEmitter*                mpEmitters;     // ParticleSystem.h:50
    EA::Allocator::ITaggedAllocator* mpAllocator;    // ParticleSystem.h:51
};
