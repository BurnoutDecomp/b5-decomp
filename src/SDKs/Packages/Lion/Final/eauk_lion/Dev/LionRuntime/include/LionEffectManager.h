#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffectManager.h
//
// cLionEffectManager -- the Lion (eauk_lion) runtime's effect-INSTANCE manager: it owns
// the fixed-block pool every cLionEffectInstance is carved from, the chain of live
// instances, and the tagged allocator the instance's bindings free themselves through.
//
// LAYOUT AUTHORITY: the DecFIGS DWARF (LionEffectManager.h:30, members h:73..80), with
// every offset attested by the X360 ARTIST asm for cLionFX::Init @0x82914A98, which is
// where cLionEffectManager::AppInit is INLINED:
//
//   mEffectDefCount @+0x00  (h:73)   stw r11(0), 0(r31)      r31 == 0x83121D94
//   mEffectCount    @+0x04  (h:74)   stw r11(0), 4(r31)
//   mpEffectDefs    @+0x08  (h:76)   stw r11(0), 8(r31)
//   mpEffects       @+0x0C  (h:77)   stw r11(0), 0xC(r31)
//   mpAllocator     @+0x10  (h:79)   stw r30,    0x10(r31)   == off_83121DA4, which is
//                                    the datum cLionBindings::DeInit @0x82908240 frees
//                                    through (LionBindings.cpp's local `cLionEffectManager`
//                                    fork + its undefined `gpLionEffectManager` pointer
//                                    were standing in for exactly this member).
//   mAllocator      @+0x14  (h:80)   cLionBlockAlloc::Init(r31+0x14, r30, 0x90, r29)
//                                    == unk_83121DA8; sizeof(cLionBlockAlloc) == 0x20 puts
//                                    the next global (cLionChunkManager @0x83121DC8)
//                                    exactly at +0x34, which it is.
//
// The singleton itself is DWARF `mSingleton` (h:71) at X360 0x83121D94 -- the address
// cLionFX::EffectCreate @0x82914CB8 / EffectDestroy @0x82915148 pass as `this`.
//
// X360 pointers are 32-bit; on the 64-bit host they widen, so the ABSOLUTE offsets above
// are NOT host layout facts. Members are pinned BY NAME and SEQUENCE.
//
// Vendor code (eauk_lion), reconstructed in its canonical Lion home. Only AppInit/GetMe/
// GetpAllocator are bodied here; the manager's create/destroy/update/render surface is
// declared as its own TUs land (EffectCreate @0x829149E8, EffectDestroy @0x82914FF0 are
// real ledger rows and are NOT declared until they are reconstructed -- a declaration with
// no definition is how a caller fails at link instead of at the honest place).
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBlockAlloc.h"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

class cLionEffectDefinition;   // LionEffect.h (sibling home)
class cLionEffectInstance;     // LionEffect.h -- NOT homed in this tree yet (see AppInit)

// DecFIGS DWARF LionEffectManager.h:30.
struct cLionEffectManager
{
public:
    // The manager singleton (DWARF h:33 / h:71 `mSingleton`), X360 0x83121D94.
    static cLionEffectManager* GetMe();

    // App lifetime (DWARF h:38). NO STANDALONE X360 BODY -- inlined into cLionFX::Init
    // @0x82914A98 (0x82914B08..0x82914B58); re-outlined here as the source's own function.
    void AppInit(EA::Allocator::ITaggedAllocator* apAllocator, u32 auEffectLimit);

    // DWARF h:67. The allocator cLionBindings::DeInit @0x82908240 frees its locator array
    // through (off_83121DA4 == this + 0x10).
    EA::Allocator::ITaggedAllocator* GetpAllocator() { return mpAllocator; }

private:
    // ----- members (DWARF order; every offset attested in cLionFX::Init) -----
    u32                              mEffectDefCount;  // +0x00  LionEffectManager.h:73
    u32                              mEffectCount;     // +0x04  LionEffectManager.h:74
    cLionEffectDefinition*           mpEffectDefs;     // +0x08  LionEffectManager.h:76
    cLionEffectInstance*             mpEffects;        // +0x0C  LionEffectManager.h:77
    EA::Allocator::ITaggedAllocator* mpAllocator;      // +0x10  LionEffectManager.h:79
    cLionBlockAlloc                  mAllocator;       // +0x14  LionEffectManager.h:80
};
