#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitterManager.h
//
// cParticleEmitterManager -- the Lion (eauk_lion) particle runtime's emitter pool
// owner. It allocates one fixed array of cParticleEmitter and threads the free/used
// lists through each emitter's next-pointer (cParticleEmitter::GetNextEmitter /
// SetNext, console +0x204). Register / RegisterSubEmitter pull an emitter off the
// free list and splice it onto the used list; Update walks the used list each frame,
// unregistering emitters that report themselves dead. A parallel clone pool
// (mpClones / mpCloneFree / mpCloneUsed) is owned by the same manager but is not
// touched by the functions reconstructed here.
//
// LAYOUT AUTHORITY: member set/order/types are from the DecFIGS DWARF
// (ParticleEmitterManager.h:41, lines 94..111), with every offset the runtime indexes
// verified against the X360 ARTIST asm for the pooled TU
// (AppInit @0x82913470, Register @0x82913590, RegisterSubEmitter @0x82913668,
//  Update @0x82915700):
//
//   mEmitterCount    @0x00 (a1[0])  -- pool size (mulli 0x2D0 == sizeof emitter)
//   mUsedCount       @0x04 (a1[1])  -- live emitter count (bumped on register)
//   mCloneCount      @0x08          -- clone-pool size (untouched here)
//   mCloneUsedCount  @0x0C          -- live clone count (untouched here)
//   mpEmitters       @0x10 (a1[4])  -- the emitter array base
//   mpFree           @0x14 (a1[5])  -- head of the free list (emitter next-ptr)
//   mpUsed           @0x18 (a1[6])  -- head of the used list (emitter next-ptr)
//   mpClones         @0x1C          -- clone array base (untouched here)
//   mpCloneFree      @0x20          -- clone free list (untouched here)
//   mpCloneUsed      @0x24          -- clone used list (untouched here)
//   mpAllocator      @0x28 (a1[10]) -- the tagged allocator
//
// X360 pointers are 32-bit; on the 64-bit host they widen, so the ABSOLUTE byte
// offsets above do NOT hold on the host. Members are pinned BY NAME and SEQUENCE; the
// load-bearing facts reproduced store-for-store are the list-threading patterns
// (pop from mpFree, push onto mpUsed, walk via each emitter's next-pointer) and the
// per-emitter stride == sizeof(cParticleEmitter).
// ============================================================================

#include "types.hpp"

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

struct cTime;                  // ext-include/GameStructs/cTime.h (the real home, 2026-09-03)
struct sParticleEmitterClone;  // clone-pool record (DWARF sParticleEmitterClone) -- not homed;
                               // referenced only by pointer here
struct cLionBindings;          // LionBindings.h (sibling home) -- UnRegister(descriptor,...) takes it

struct cParticleEmitterManager
{
public:
    // The manager singleton (DecFIGS DWARF ParticleEmitterManager.h:44). The X360 build
    // reaches its static storage (dword_831238E8) directly at the call sites that
    // register/unregister effect bindings; Instance() is the accessor that resolves to it.
    // Body lives in the manager's own TU; declared here so callers compile against it.
    static cParticleEmitterManager& Instance();
    // App lifetime: allocate the emitter array through the tagged allocator, Init every
    // emitter, thread the free list, and clear the used list. X360 @0x82913470.
    void AppInit(EA::Allocator::ITaggedAllocator* apAllocator, u32 aEmitterCount);

    // Reserve a free emitter for apDescriptor and push it onto the used list. Returns the
    // new emitter, or null if apDescriptor is unplayable or the free list is empty.
    // X360 @0x82913590.
    cParticleEmitter* Register(cParticleDescriptor* apDescriptor);

    // Register a sub-emitter: find the used emitter whose descriptor is (or parents)
    // apDescriptor->mpParent and splice a fresh emitter in ahead of it. Returns the new
    // emitter, or null if no parent emitter is live or the free list is empty.
    // X360 @0x82913668.
    cParticleEmitter* RegisterSubEmitter(cParticleDescriptor* apDescriptor);

    // Per-frame: advance every used emitter; unregister the ones that report dead.
    // X360 @0x82915700.
    void Update(const cTime& arTime);

    // Recycle apEmitter off the used list back onto the free list. Body lives in a sibling
    // TU (ParticleEmitterManager.cpp:380); declared here because Update calls it.
    void UnRegister(cParticleEmitter* apEmitter);

    // Unregister the emitter bound to arDescriptor for the given binding set (arBindBase
    // locates the sibling binding chain the manager walks). Called by
    // cLionParticleEffectManager::BindingsRemove @ 0x82914CE0 (X360 sub_829146D0). Body
    // lives in the manager's own TU; declared here per the DWARF signature
    // (ParticleEmitterManager.h:64).
    void UnRegister(const cParticleDescriptor& arDescriptor, cLionBindings& arBindings,
                    cLionBindings* apBindBase);

    // DWARF ParticleEmitterManager.h:93 / :99 -- the head of the LIVE emitter list and how many
    // are on it. cParticleRender::Render @0x82914860 reads the list head directly
    // (`lwz r30, 0x18(r30)`) and walks it through each emitter's own next pointer; inline by
    // construction, like every other accessor on this class.
    cParticleEmitter* GetpUsed() const { return mpUsed; }
    u32 GetUsedCount() const           { return mUsedCount; }

private:
    // ----- members (DWARF order; offsets verified against the X360 asm) -----
    u32 mEmitterCount;                              // 0x00  ParticleEmitterManager.h:94
    u32 mUsedCount;                                 // 0x04  ParticleEmitterManager.h:95
    u32 mCloneCount;                                // 0x08  ParticleEmitterManager.h:97
    u32 mCloneUsedCount;                            // 0x0C  ParticleEmitterManager.h:98
    cParticleEmitter* mpEmitters;                   // 0x10  ParticleEmitterManager.h:101
    cParticleEmitter* mpFree;                       // 0x14  ParticleEmitterManager.h:103
    cParticleEmitter* mpUsed;                       // 0x18  ParticleEmitterManager.h:105
    sParticleEmitterClone* mpClones;                // 0x1C  ParticleEmitterManager.h:107
    sParticleEmitterClone* mpCloneFree;             // 0x20  ParticleEmitterManager.h:108
    sParticleEmitterClone* mpCloneUsed;             // 0x24  ParticleEmitterManager.h:109
    EA::Allocator::ITaggedAllocator* mpAllocator;   // 0x28  ParticleEmitterManager.h:111
};
