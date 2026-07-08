#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffectManager.h
//
// cLionParticleEffectManager -- the Lion (eauk_lion) particle runtime's top-level
// effect-manager singleton. It owns the tagged allocator + the small fixed-block
// allocator every particle sub-object is carved from (cParticleDescriptor /
// cParticleWaveForm / cParticleMaterial / cParticleBehaviour), holds the hashed
// chain of loaded cLionParticleEffect records, and drives the per-effect binding
// attach/remove that wires an effect instance's descriptors onto live emitters
// (via the cParticleEmitterManager singleton).
//
// LAYOUT AUTHORITY: member set/order/types are from the DecFIGS DWARF
// (LionParticleEffectManager.h:34, members h:86..89), with the one offset the
// reconstructed bodies index verified against the X360 ARTIST asm:
//   mpAllocator @ +0x00 -- CreateBehaviour reads *this (lwz r11,0(r3)) then calls
//                          the ITaggedAllocator::Alloc(size,TagValuePair) virtual
//                          through it (the +4 is the ITaggedAllocator sub-object
//                          adjust the compiler regenerates).
//
//   mpAllocator     @ +0x00  EA::Allocator::ITaggedAllocator*  (h:86)
//   mAllocator      @ +0x04  cLionBlockAlloc                   (h:87)
//   mpEffects       @ ....    cLionParticleEffect*              (h:88)
//   mpEffectGroups  @ ....    cParticleDescriptor*[64]          (h:89)
//
// X360 pointers are 32-bit; on the 64-bit host they widen, so the absolute byte
// offsets above do NOT hold on the host. Members are pinned BY NAME and SEQUENCE.
// Grow this class additively as further Lion effect-manager TUs land; never
// reorder/retype the modelled members.
//
// Vendor code (eauk_lion); reconstructed in its canonical Lion home. Only the three
// ledger bodies reconstructed in LionParticleEffectManager.cpp are declared here;
// the remaining manager methods (AppInit/Render/EffectCreate/... per the DWARF)
// are homed additively as their own TUs are reconstructed.
// ============================================================================

#include "types.hpp"

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBlockAlloc.h"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

class  cLionParticleEffect;   // LionParticleEffect.h (sibling home) -- hashed effect record
class  cParticleDescriptor;   // ParticleDescriptor.h (sibling home)
struct cParticleBehaviour;    // ParticleBehaviour.h (sibling home)
struct cLionBindings;         // LionBindings.h (sibling home)

struct cLionParticleEffectManager
{
public:
    // Allocate a fresh particle-behaviour node from the effect manager's tagged
    // allocator (size == sizeof(cParticleBehaviour); the X360 build stamps the
    // allocation with a LINE/FILE/NAME TagValuePair chain). X360 @0x829088A8.
    cParticleBehaviour* CreateBehaviour();

    // Attach arBindings onto every top-level descriptor of apEffect: register an
    // emitter for each descriptor and bind the two together. X360 @0x82914530.
    void BindingsAttach(const cLionParticleEffect* apEffect, cLionBindings& arBindings);

    // Remove arBindings from apEffect: unregister the bound emitter of every
    // descriptor of apEffect (apBindBase locates the sibling binding chain the
    // emitter manager walks). X360 @0x82914CE0.
    void BindingsRemove(const cLionParticleEffect* apEffect, cLionBindings& arBindings,
                        cLionBindings* apBindBase);

private:
    // ----- members (DecFIGS DWARF order; mpAllocator@+0x00 verified vs the X360 asm) -----
    EA::Allocator::ITaggedAllocator* mpAllocator;      // +0x00  LionParticleEffectManager.h:86
    cLionBlockAlloc                  mAllocator;       // +0x04  LionParticleEffectManager.h:87
    cLionParticleEffect*             mpEffects;        //        LionParticleEffectManager.h:88
    cParticleDescriptor*             mpEffectGroups[64];//       LionParticleEffectManager.h:89
};
