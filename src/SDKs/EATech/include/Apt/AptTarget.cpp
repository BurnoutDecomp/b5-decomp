// ===========================================================================
// EATech Apt -- AptTarget: the Apt (ActionScript player) per-thread CONTEXT.
//
// Bodies for the four X360 ARTIST functions the AptTarget class owns:
//     AptTarget::AptTarget          @0x82B00160   (ctor: pool-allocate the
//                                                  owned sub-objects)
//     AptTarget::GetAnimationTarget @0x82AD5770   (the +0x18 director)
//     AptTarget::SetNext            @0x82B6BEA0   (store the +0x24 slot)
//     AptTarget::Shutdown           @0x82B02328   (tear the context down)
//
// Decompiled faithfully from the X360 disassembly. The console allocates a
// 4-byte slot for every pointer member; on the x64 PC gate those slots widen to
// sizeof(T*) (the pervasive Apt x64-port rule) -- so the pool sizes here are the
// PC sizeof, NOT the console literal 4.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptTarget.h"

#include "SDKs/EATech/include/Apt/AptLoader.h"               // mpLoader (AptLoader*)
#include "SDKs/EATech/include/Apt/AptLinker.h"               // mpLinker (AptLinker*) + ScalarDeletingDestructor
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"      // mpAnimationTarget (AptAnimationTarget*)
#include "SDKs/EATech/include/Apt/AptRenderManagerQueue.h"   // AptRenderManagerQueue::Add
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"    // _AptRenderItemRootList::Shutdown

#include "SDKs/EATech/Apt/DogmaAllocator.h"                  // DOGMA_PoolManager::Allocate/Deallocate

#include "SDKs/EATech/eathread/thread_local_storage.h"       // EA::Thread::ThreadLocalStorage
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"            // EA::Thread::GetThreadId / ThreadId

#include <new>   // placement new for the inline AptLinker sub-object

// =====================================================================
//  The shared Apt fixed-size pool (X360 off_8324D808) -- the same handle every
//  Apt sub-object (AptAnimationTarget static data / AptActionQueue / the linker
//  thingies) allocates from. Defined by the Apt pseudo-data layer.
// =====================================================================
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // off_8324D808

// =====================================================================
//  The Apt context/director singletons. These ARE the AptTarget context
//  storage (X360 off_8324E570 / off_8324E574 / off_8324E578); Shutdown saves and
//  restores them around the teardown so it runs "as" the target being destroyed.
//  Homed here (the AptTarget TU) -- the only TU that defines them; every Apt
//  subsystem reaches the active context through gpAptTarget.
// =====================================================================
AptTarget* gpAptTargetCurrent = nullptr;   // off_8324E570
AptTarget* gpAptTarget        = nullptr;   // off_8324E574
AptTarget* gpAptTargetTLS     = nullptr;   // off_8324E578

// =====================================================================
//  Un-homed callees/globals referenced below (declared by name + FLAG'd). Bodies
//  are separate TUs; declared here so this TU links.
// =====================================================================

// FLAG (un-homed): the EA TLS slot the Apt context pointer is mirrored into
// (X360 unk_8324E814). Shutdown re-publishes the active target into it whenever
// it swaps the global pointers. Defined by the Apt boot TU; declared extern.
extern EA::Thread::ThreadLocalStorage gAptTargetTls;   // unk_8324E814

// FLAG (un-homed): byte_8324E7C9 -- the "in a shutdown / on the owning thread"
// guard. Shutdown forces it to 1 across the sub-object teardown, then restores it,
// and uses its pre-shutdown value to decide whether the render-root anchor can be
// freed inline (this thread owns it) or must be deferred to the render queue.
extern unsigned char gbAptInShutdown;   // byte_8324E7C9

// FLAG (un-homed): dword_8324E504 -- the render/main thread id (a pointer-sized id;
// EA::Thread::ThreadId == void*). Compared against GetThreadId() to decide if the
// caller may free the render-root anchor inline. Owned by the Apt boot TU.
extern EA::Thread::ThreadId gAptRenderThreadId;   // dword_8324E504

// FLAG (un-homed): dword_8324E7D8 -- the global render-manager deferred-teardown
// queue. When Shutdown runs off the render thread it hands the render-root anchor
// here for the render thread to free later. Owned by the render-manager TU.
extern AptRenderManagerQueue gAptRenderManagerQueue;   // dword_8324E7D8

// FLAG (un-homed AptUpdate facade @0x82B0xxxx): walk the per-target zombie vector,
// retiring the entries flagged this pass. Body its own (Apt update) TU.
void* AptUpdateZombieVector(char bAll);

// FLAG (un-homed AptAnimationTarget lifecycle surface; bodies their own TU):
//   AptAnimationTarget::AptAnimationTarget @0x82AFF648 -- the 88-byte director ctor
//                                       (built from the same AptUpdateParams block).
//   AptAnimationTarget::PreDestroy   @0x82AFE... -- release the director's owned
//                                       display-list/listener state before the GC pass.
//   AptAnimationTarget::CleanRemList @0x82B0...  -- drain the remove list.
//   ~AptAnimationTarget              @0x82AFF790 -- the 88-byte director destructor.
AptAnimationTarget* MakeAptAnimationTarget(void* pMem, const u32* pParams);
void AptAnimationTarget_PreDestroy(AptAnimationTarget* pAnim);
void AptAnimationTarget_CleanRemList(AptAnimationTarget* pAnim);
void AptAnimationTarget_Destruct(AptAnimationTarget* pAnim);

// FLAG (un-homed): ~AptLoader @0x82B... -- the X360 loader destructor drains the
// weak loaded-file list before the block is freed. AptLoader has no homed dtor
// (its request-layer reconstruction added no owned state needing destruction);
// the list-drain is part of the deferred loader-completion TU. Declared here so
// the teardown matches the console's `~AptLoader; Deallocate` sequence.
void AptLoader_Destruct(AptLoader* pLoader);

// ---------------------------------------------------------------------
// AptTarget::AptTarget @0x82B00160 -- pool-allocate the context's owned
// sub-objects and seed its capacity/config block from the AptUpdateParams.
//
// The ctor first stores the literal defaults (64/64/64/512/512/256) into the six
// config dwords, then immediately overwrites all six from pParams (the X360 emits
// both stores -- kept faithfully; the defaults are dead but present in the asm).
// ---------------------------------------------------------------------
AptTarget::AptTarget(const u32* pParams)
{
    // ---- config-block defaults (overwritten directly below; asm-faithful) ----
    mnConfigA = 64;
    mnConfigB = 64;
    mnConfigC = 64;
    mnConfigD = 512;
    mnConfigE = 512;
    mnConfigF = 256;

    // ---- the live config values, copied from the AptUpdateParams block ----
    mnConfigA = pParams[4];   // lwz r10,0x10(r11)
    mnConfigB = pParams[7];   // lwz r10,0x1C(r11)
    mnConfigC = pParams[2];   // lwz r10,0x08(r11)
    mnConfigD = pParams[1];   // lwz r10,0x04(r11)
    mnConfigE = pParams[0];   // lwz r10,0x00(r11)
    mnConfigF = pParams[3];   // lwz r11,0x0C(r11)

    // ---- mpAnimationTarget (+0x18): the 88-byte director ----
    void* pAnimMem = gpAptPseudoDataPool->Allocate(sizeof(AptAnimationTarget));   // Allocate(pool, 88)
    if (pAnimMem)
        mpAnimationTarget = MakeAptAnimationTarget(pAnimMem, pParams);            // AptAnimationTarget ctor
    else
        mpAnimationTarget = nullptr;

    // ---- mpLoader (+0x1C): the loader wrapper (console 4-byte slot -> sizeof(AptLoader)) ----
    void* pLoaderMem = gpAptPseudoDataPool->Allocate(sizeof(AptLoader));          // Allocate(pool, 4)
    if (pLoaderMem)
    {
        mpLoader = static_cast<AptLoader*>(pLoaderMem);
        mpLoader->mpHead = nullptr;   // *v5 = 0
    }
    else
    {
        mpLoader = nullptr;
    }

    // ---- mpLinker (+0x20): the 24-byte file linker, constructed inline ----
    void* pLinkerMem = gpAptPseudoDataPool->Allocate(sizeof(AptLinker));          // Allocate(pool, 24)
    if (pLinkerMem)
    {
        // The X360 inlines the AptLinker ctor here (head=0; empty 2-element SBO
        // vector pointing at its own inline buffer). Reproduce via placement new
        // over the AptLinker default ctor (AptLinker.cpp).
        mpLinker = new (pLinkerMem) AptLinker();
    }
    else
    {
        mpLinker = nullptr;
    }

    // ---- mpField2C (+0x2C): the render-root anchor slot (one pointer, zeroed) ----
    // Console 4-byte slot -> a one-pointer cell holding a _AptRenderItemRootList*.
    void* pAnchorMem = gpAptPseudoDataPool->Allocate(
        sizeof(AptRenderTreeManager::_AptRenderItemRootList*));                    // Allocate(pool, 4)
    if (pAnchorMem)
    {
        mppRenderRootAnchor =
            static_cast<AptRenderTreeManager::_AptRenderItemRootList**>(pAnchorMem);
        *mppRenderRootAnchor = nullptr;   // *v8 = 0
    }
    else
    {
        mppRenderRootAnchor = nullptr;
    }

    // ---- the two TBD slots (+0x24 / +0x28), zeroed last (asm order) ----
    mpField24 = nullptr;
    mpField28 = nullptr;
}

// AptTarget::GetAnimationTarget @0x82AD5770 (`lwz r3,0x18(r3); blr`) and
// AptTarget::SetNext @0x82B6BEA0 (`stw r4,0x24(r3); blr`) are the inline one-liner
// accessors in AptTarget.h (faithful to the X360 single-instruction bodies).

// ---------------------------------------------------------------------
// AptTarget::Shutdown @0x82B02328 -- tear the context down. Runs the teardown
// "as" this target (swapping the three context globals + the TLS mirror), forces
// the in-shutdown guard, destroys + frees each owned sub-object, then disposes of
// the render-root anchor (inline on the owning thread, else deferred to the render
// queue), and finally restores the previous context.
// ---------------------------------------------------------------------
void AptTarget::Shutdown()
{
    // ---- become the active context (save the previous; mirror into TLS) ----
    AptTarget* pPrevCurrent = gpAptTarget;          // v2 = off_8324E574
    gpAptTarget = this;                             // off_8324E574 = this
    gAptTargetTls.SetValue(this);

    AptTarget* pPrevTLS = gpAptTargetTLS;           // v3 = off_8324E578
    gpAptTargetTLS = this;                          // off_8324E578 = this
    gAptTargetTls.SetValue(this);

    // ---- force the in-shutdown guard (restored after the sub-object teardown) ----
    unsigned char bWasInShutdown = gbAptInShutdown; // v4 = byte_8324E7C9
    gbAptInShutdown = 1;

    // ---- predestroy the animation director (release its owned state) ----
    if (mpAnimationTarget)
    {
        AptAnimationTarget_PreDestroy(mpAnimationTarget);
        AptAnimationTarget_CleanRemList(mpAnimationTarget);
    }

    // ---- retire all pending zombies ----
    AptUpdateZombieVector(1);

    // ---- destroy + free the loader ----
    AptLoader* pLoader = mpLoader;                  // v5 = *(this + 0x1C)
    if (pLoader)
    {
        AptLoader_Destruct(pLoader);                // ~AptLoader (drains the list -- un-homed)
        gpAptPseudoDataPool->Deallocate(pLoader, sizeof(AptLoader));   // Deallocate(pool, v5, 4)
    }

    // ---- destroy + free the linker (its MSVC scalar-deleting destructor) ----
    if (mpLinker)                                   // v6 = *(this + 0x20)
        mpLinker->ScalarDeletingDestructor(1);      // flags&1 -> also frees the 24-byte block

    // ---- destroy + free the animation director ----
    AptAnimationTarget* pAnim = mpAnimationTarget;  // v7 = *(this + 0x18)
    if (pAnim)
    {
        AptAnimationTarget_Destruct(pAnim);         // ~AptAnimationTarget
        gpAptPseudoDataPool->Deallocate(pAnim, sizeof(AptAnimationTarget));   // Deallocate(pool, v7, 88)
    }

    // ---- restore the guard, null the three owned-sub-object slots ----
    gbAptInShutdown = bWasInShutdown;               // byte_8324E7C9 = v4

    AptRenderTreeManager::_AptRenderItemRootList** ppAnchor = mppRenderRootAnchor;  // v8 = *(this + 0x2C)
    mpAnimationTarget = nullptr;                     // *(this + 0x18) = 0
    mpLoader          = nullptr;                     // *(this + 0x1C) = 0
    mpLinker          = nullptr;                     // *(this + 0x20) = 0

    // ---- dispose of the render-root anchor ----
    if (ppAnchor)
    {
        // Free inline when we are already in a shutdown (bWasInShutdown) OR we are
        // running on the render thread; otherwise defer the teardown to the queue.
        if (bWasInShutdown || gAptRenderThreadId == EA::Thread::GetThreadId())
        {
            AptRenderTreeManager::_AptRenderItemRootList** pAnchorCell = mppRenderRootAnchor;  // v10
            if (*pAnchorCell)
            {
                AptRenderTreeManager::_AptRenderItemRootList::Shutdown(*pAnchorCell);
                *pAnchorCell = nullptr;
            }
            gpAptPseudoDataPool->Deallocate(
                mppRenderRootAnchor,
                sizeof(AptRenderTreeManager::_AptRenderItemRootList*));   // Deallocate(pool, *(this+0x2C), 4)
        }
        else
        {
            gAptRenderManagerQueue.Add(mppRenderRootAnchor);             // defer to the render thread
        }
        mppRenderRootAnchor = nullptr;              // *(this + 0x2C) = 0
    }

    // ---- restore the previous context (TLS-side first, then current) ----
    gpAptTargetTLS = pPrevTLS;                       // off_8324E578 = v3
    gAptTargetTls.SetValue(pPrevTLS);
    gpAptTarget = pPrevCurrent;                      // off_8324E574 = v2
    gAptTargetTls.SetValue(pPrevCurrent);
}
