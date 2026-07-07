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

// MakeAptAnimationTarget (the @0x82AFF648 ctor, built from the AptUpdateParams block)
// is now HOMED in AptAnimationTarget.cpp next to the ctor it wraps -- its declaration
// comes from the AptAnimationTarget.h include above. The remaining lifecycle surface
// is still its own TU (declared by name + FLAG'd):
//   AptAnimationTarget::PreDestroy   @0x82AFE... -- release the director's owned
//                                       display-list/listener state before the GC pass.
//   AptAnimationTarget::CleanRemList @0x82B0...  -- drain the remove list.
//   ~AptAnimationTarget              @0x82AFF790 -- the 88-byte director destructor.
// AptAnimationTarget::PreDestroy is now called directly on the director (the AptAnimationTarget_PreDestroy shim is retired).
// AptAnimationTarget::CleanRemList is now called directly (static member; the AptAnimationTarget_CleanRemList shim is retired).
// ~AptAnimationTarget is now invoked explicitly on the director (the AptAnimationTarget_Destruct shim is retired).

// FLAG (un-homed): ~AptLoader @0x82B... -- the X360 loader destructor drains the
// weak loaded-file list before the block is freed. AptLoader has no homed dtor
// (its request-layer reconstruction added no owned state needing destruction);
// the list-drain is part of the deferred loader-completion TU. Shutdown calls the
// explicit `pLoader->~AptLoader()` directly (the console's `~AptLoader; Deallocate`).

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

// =====================================================================
//  The Apt target-instance LIST + the create/change-current orchestration
//  (X360 AptCreateTargetInstance @0x82B003B0 + AptChangeTargetInstance @0x82ADB768).
//  These are the pieces CgsGui::AptAux::InitializeApt @0x82848E50 runs after the
//  allocator/update/render inits to stand up + select the per-process Apt context;
//  the X360 InitializeApt is itself un-reconstructed (its callees are these), so the
//  host bring-up (BrnAptRuntimeBringUp.cpp) calls these directly. Homed here in the
//  AptTarget TU (it owns the target globals + the ctor these drive).
//
//  LIST: off_8324E570 (gpAptTargetCurrent) is the HEAD of the instance list; each
//  AptTarget is linked through +0x24 (mpField24 == NEXT) / +0x28 (mpField28 == PREV)
//  -- so the two "role TBD" slots in AptTarget.h are now KNOWN to be the instance-
//  list next/prev. (Member access by name; the console offsets are documentation.)
// =====================================================================

// dword_8324E57C -- the running count of created target instances (the X360 bumps it
// in AptCreateTargetInstance). Owned here (the create path is homed here).
int gAptTargetInstanceCount = 0;   // dword_8324E57C

// ---------------------------------------------------------------------
// AptCreateTargetInstance @0x82B003B0 -- allocate + construct a new AptTarget context
// from the AptUpdateParams block, append it to the instance list, bump the counter,
// and return it.
//
// X360 body: spin-lock unk_8324E7D0; ++dword_8324E57C; mem = Allocate(off_8324D808, 48);
// result = sub_82B002A0(mem, params) [== the AptTarget ctor]; then if the list head
// off_8324E570 is null set it to result, else walk the +0x24 next-chain to the tail and
// append (tail->+0x24 = result; result->+0x28 = tail); unlock; return result.
//
// FLAG: the unk_8324E7D0 spin-lock is a NO-OP on the single-threaded PC boot path (the
// Apt context is created once, on the main thread); omitted. The 48-byte allocation +
// the ctor + the list splice are reproduced faithfully against the named AptTarget layout
// (the console sub_82B002A0 IS the AptTarget::AptTarget ctor already homed above).
// ---------------------------------------------------------------------
AptTarget* AptCreateTargetInstance(const u32* pParams)
{
    // FLAG: spin-lock unk_8324E7D0 elided (single-threaded PC bring-up).

    ++gAptTargetInstanceCount;   // ++dword_8324E57C

    // Allocate the 48-byte (12-dword console) AptTarget block from the shared DOGMA pool
    // (off_8324D808 == gpAptPseudoDataPool, wired in the host bring-up step 1) and
    // construct it via the homed AptTarget ctor (== the console sub_82B002A0 @0x82B002A0).
    void* pMem = gpAptPseudoDataPool->Allocate(sizeof(AptTarget));   // Allocate(pool, 48)
    if (pMem == nullptr)
        return nullptr;
    AptTarget* pResult = ::new (pMem) AptTarget(pParams);

    // Append to the instance list (head off_8324E570 == gpAptTargetCurrent).
    if (gpAptTargetCurrent == nullptr)
    {
        gpAptTargetCurrent = pResult;   // off_8324E570 = result (first instance)
    }
    else
    {
        AptTarget* pTail = gpAptTargetCurrent;
        while (pTail->mpField24)                       // walk +0x24 (NEXT) to the tail
            pTail = static_cast<AptTarget*>(pTail->mpField24);
        pTail->mpField24    = pResult;                 // tail->+0x24 = result (link NEXT)
        pResult->mpField28  = pTail;                   // result->+0x28 = tail (link PREV)
    }

    // FLAG: spin-unlock unk_8324E7D0 elided.
    return pResult;
}

// ---------------------------------------------------------------------
// AptChangeTargetInstance @0x82ADB768 -- make `pTarget` the CURRENT Apt context.
//
// X360 body: spin-lock unk_8324E7D0; off_8324E574 = result; off_8324E578 = result;
// unlock; return result. i.e. it stores the target into gpAptTarget (the canonical
// "current context" every Apt subsystem dereferences) + gpAptTargetTLS.
//
// FLAG: the X360 sets only the two GLOBAL pointers here; the per-thread EA TLS mirror
// (unk_8324E814 == gAptTargetTls) that GetTarget() reads is published by the (un-homed)
// AptUpdateInitialize / per-thread setup. On the single-threaded PC port we ALSO publish
// into that TLS mirror here (exactly as AptTarget::Shutdown does) so GetTarget() returns
// the live context -- otherwise GetTarget() (which reads the TLS slot) would stay null.
// The spin-lock is elided (single-threaded bring-up).
// ---------------------------------------------------------------------
AptTarget* AptChangeTargetInstance(AptTarget* pTarget)
{
    gpAptTarget    = pTarget;   // off_8324E574 = result (the canonical current context)
    gpAptTargetTLS = pTarget;   // off_8324E578 = result

    // PC: mirror into the EA TLS slot GetTarget() reads (FLAG: console does this in the
    // un-homed AptUpdateInitialize / per-thread setup, not here).
    gAptTargetTls.SetValue(pTarget);

    return pTarget;
}

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
                mpAnimationTarget->PreDestroy();
        AptAnimationTarget::CleanRemList();
    }

    // ---- retire all pending zombies ----
    AptUpdateZombieVector(1);

    // ---- destroy + free the loader ----
    AptLoader* pLoader = mpLoader;                  // v5 = *(this + 0x1C)
    if (pLoader)
    {
        pLoader->~AptLoader();                       // console: explicit ~AptLoader (drains the loaded-file list)
        gpAptPseudoDataPool->Deallocate(pLoader, sizeof(AptLoader));   // Deallocate(pool, v5, 4)
    }

    // ---- destroy + free the linker (its MSVC scalar-deleting destructor) ----
    if (mpLinker)                                   // v6 = *(this + 0x20)
        mpLinker->ScalarDeletingDestructor(1);      // flags&1 -> also frees the 24-byte block

    // ---- destroy + free the animation director ----
    AptAnimationTarget* pAnim = mpAnimationTarget;  // v7 = *(this + 0x18)
    if (pAnim)
    {
        pAnim->~AptAnimationTarget();               // ~AptAnimationTarget
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

// ---------------------------------------------------------------------
// GetTarget -- the per-thread current AptTarget. PS3 _Z9GetTargetv @0xF1AD14:
//   return EA::Thread::ThreadLocalStorage::GetValue(&gTLSCurrentTarget);
// The current-target TLS slot is the gAptTargetTls mirror Shutdown publishes into
// (X360 unk_8324E814). Homed here (the AptTarget TU that owns that mirror).
// ---------------------------------------------------------------------
AptTarget* GetTarget()
{
    return static_cast<AptTarget*>(gAptTargetTls.GetValue());   // TlsGetValue(gTLSCurrentTarget)
}

// ---------------------------------------------------------------------
// AptTarget_GetLoader -- the loader the target holds at +0x1C. PS3
// _ZN9AptTarget9GetLoaderEv @0xF15E60: `return *(this + 7);` (the 8th pointer word
// == mpLoader). Routed through the named member so the x64 layout stays correct
// (the AptLoader.h FLAG that introduced this accessor instead of the literal offset).
// ---------------------------------------------------------------------
AptLoader* AptTarget::GetLoader()
{
    return mpLoader;   // *(this + 7) == +0x1C
}

