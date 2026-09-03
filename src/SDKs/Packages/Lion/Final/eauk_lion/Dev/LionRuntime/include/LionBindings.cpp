// cLionBindings -- implementation.
//
// Vendor code (eauk_lion), reconstructed from the X360 asm/pseudocode at
//   0x82908240 cLionBindings::DeInit
// against the DWARF layout in LionBindings.h. Store-for-store faithful; members by name.
//
// ⭐ 2026-09-03 (boost-exhaust wave): cLionBindings::Init @0x82912958 lands beside DeInit.
// It was declared-not-defined because its only caller -- cLionEffectManager::EffectCreate
// @0x829149E8 -- was not reconstructed; that one is now bodied, so this one must be.

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffectManager.h"

// ⭐ THE LOCAL FORK IS GONE (2026-09-03). This TU used to declare its own one-method
// `struct cLionEffectManager` plus `extern cLionEffectManager* gpLionEffectManager` -- a
// re-declaration of a type that now has a real home, and an invented POINTER global that no
// TU in the tree defined, so this file could never have linked. The console has no such
// pointer: off_83121DA4 is `cLionEffectManager::mSingleton.mpAllocator`, i.e. this + 0x10 of
// the singleton object at 0x83121D94, and the real accessor is GetMe()->GetpAllocator().

// ----------------------------------------------------------------------------
// cLionBindings::Init  @0x82912958
//
// Store for store, in the console's own order (asm 0x82912974..0x82912994):
//     stw r30(0), 0xC(r31)   mpLocator     = 0
//     stw r30(0), 0x10(r31)  mpScaler      = 0
//     stw r30(0), 0x14(r31)  mpTrigger     = 0
//     stw r30(0), 8(r31)     mppLocators   = 0
//     stw r30(0), 0(r31)     mLocatorCount = 0
//     bl  cParticleRandomSeed::Init(r31 + 0x20)   mSeed.Init()
//     stw r30(0), 4(r31)     mWorldIndex   = 0
//     stw r30(0), 0x60(r31)  mpNext        = 0
//
// ⚠ m_p_emitter (+0x64) IS DELIBERATELY NOT CLEARED, and that is the console's choice, not
// an omission here: the store pair ends at 0x60 and the epilogue follows immediately. The
// back-link is written by cParticleEmitter::Bind when the binding is actually attached, and
// cParticleEmitterManager::UnRegister @0x829146D0 is the only reader.
// ----------------------------------------------------------------------------
void cLionBindings::Init()
{
    mpLocator     = nullptr;
    mpScaler      = nullptr;
    mpTrigger     = nullptr;
    mppLocators   = nullptr;
    mLocatorCount = 0;

    mSeed.Init();

    mWorldIndex   = 0;
    mpNext        = nullptr;
}

void cLionBindings::DeInit()
{
    // The X360 build reaches the allocator via the effect manager (off_83121DA4) and frees
    // the owned locator array, then clears the array pointer and the count.
    if (mppLocators)
    {
        cLionEffectManager::GetMe()->GetpAllocator()->Free(mppLocators, 0);
        mppLocators   = nullptr;
        mLocatorCount = 0;
    }
}
