#pragma once

// ===========================================================================
// SDKs/EATech/include/Apt/AptTarget.h
//
// AptTarget -- the Apt (ActionScript player) per-thread CONTEXT / director the
// whole Apt runtime hangs off the global singleton (X360 off_8324E574). Roughly
// a hundred Apt functions reach the active context through it: the loader, the
// file linker, the animation director, the action interpreter's root, etc.
//
// LAYOUT (48 bytes / 12 dwords, proven by AptTarget::AptTarget @0x82B00160 which
// pool-allocates 48 bytes and by AptTarget::Shutdown @0x82B02328 which frees the
// sub-objects). The constructor first stores defaults (64/64/64/512/512/256) into
// the six leading dwords then overwrites them from the AptUpdateParams block:
//     +0x00 mnConfigA   = params[4]   ) capacity/config values the context keeps
//     +0x04 mnConfigB   = params[7]   ) for its sub-pools; copied verbatim from
//     +0x08 mnConfigC   = params[2]   ) the AptUpdateParams passed to
//     +0x0C mnConfigD   = params[1]   ) AptUpdateInitialize. FLAG: the exact
//     +0x10 mnConfigE   = params[0]   ) per-field pool meaning is not yet pinned;
//     +0x14 mnConfigF   = params[3]   ) carried as named config dwords.
//     +0x18 mpAnimationTarget  AptAnimationTarget*  (88-byte director; pool-alloc'd
//                                in the ctor; GetAnimationTarget @0x82AD5770 returns it)
//     +0x1C mpLoader            AptLoader*           (4-byte loader wrapper; ~AptLoader
//                                + Deallocate(4) in Shutdown; AptLinker::Update calls
//                                AptLoader::Update(off_8324E574->mpLoader))
//     +0x20 mpLinker            AptLinker*           (24-byte file linker; constructed
//                                inline in the ctor; AptLinker scalar-deleting-dtor in Shutdown)
//     +0x24 mpField24           void*  (zeroed at construction)        FLAG: role TBD
//     +0x28 mpField28           void*  (zeroed at construction)        FLAG: role TBD
//     +0x2C mpField2C           void*  (4-byte zeroed alloc in the ctor) FLAG: role TBD
//
// Member access is BY NAME; the X360 byte offsets above are documentation only.
// AptAnimationTarget (88 bytes) and AptLinker (24 bytes) are forward-declared here
// (held by pointer) and homed in their own follow-on TUs; AptLoader is already homed.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"   // _AptRenderItemRootList (mppRenderRootAnchor)

struct AptAnimationTarget;   // +0x18 -- the 88-byte animation director (own TU)
struct AptLoader;            // +0x1C -- homed in AptLoader.h (held by pointer here)
struct AptLinker;            // +0x20 -- the 24-byte file linker (own TU)

struct AptTarget
{
    // ---- capacity/config block (copied from the AptUpdateParams) ----
    u32 mnConfigA;   // +0x00  (= params[4])
    u32 mnConfigB;   // +0x04  (= params[7])
    u32 mnConfigC;   // +0x08  (= params[2])
    u32 mnConfigD;   // +0x0C  (= params[1])
    u32 mnConfigE;   // +0x10  (= params[0])
    u32 mnConfigF;   // +0x14  (= params[3])

    // ---- the context's owned sub-objects ----
    AptAnimationTarget* mpAnimationTarget;   // +0x18
    AptLoader*          mpLoader;            // +0x1C
    AptLinker*          mpLinker;            // +0x20
    void*               mpField24;           // +0x24  FLAG: role TBD (zeroed at ctor; SetNext writes it)
    void*               mpField28;           // +0x28  FLAG: role TBD (zeroed at ctor)
    // +0x2C -- the render-root ANCHOR cell: a one-pointer slot (pool-allocated +
    // zeroed by the ctor) holding the head of the per-target _AptRenderItemRootList
    // chain. Shutdown frees it (inline on the owning thread, else deferred to the
    // render-manager queue). Console 4-byte slot; on x64 it is sizeof(ptr).
    AptRenderTreeManager::_AptRenderItemRootList** mppRenderRootAnchor;   // +0x2C

    // GetAnimationTarget @0x82AD5770 -- the active animation director (+0x18).
    // X360 body: `lwz r3,0x18(r3); blr`.
    AptAnimationTarget* GetAnimationTarget() const { return mpAnimationTarget; }

    // SetNext @0x82B6BEA0 -- store a pointer into the +0x24 slot and return this.
    // X360 body: `stw r4,0x24(r3); blr`.
    AptTarget* SetNext(void* pNext) { mpField24 = pNext; return this; }


    // GetLoader @PS3 _ZN9AptTarget9GetLoaderEv @0xF15E60 -- the loader the target
    // holds at +0x1C (`return *(this + 7);`). Body in AptTarget.cpp.
    AptLoader* GetLoader();


    // ctor @0x82B00160 / Shutdown @0x82B02328 -- the AptTarget lifecycle (allocates /
    // tears down mpAnimationTarget + mpLoader + mpLinker + the render-root anchor).
    // Bodies in AptTarget.cpp.
    explicit AptTarget(const u32* pParams);
    void Shutdown();
};

// The Apt context/director singletons -- the same active AptTarget* under three X360
// aliases (current / default / per-thread-TLS), set by AptUpdateInitialize. Declared
// extern here; defined by the Apt update TU. gpAptTarget (off_8324E574) is the
// canonical "current context" pointer every Apt subsystem dereferences.
extern AptTarget* gpAptTargetCurrent;   // X360 off_8324E570  (HEAD of the instance list)
extern AptTarget* gpAptTarget;          // X360 off_8324E574  (the CURRENT context)
extern AptTarget* gpAptTargetTLS;       // X360 off_8324E578

// The running count of created target instances (X360 dword_8324E57C). Bumped by
// AptCreateTargetInstance. Defined in AptTarget.cpp.
extern int gAptTargetInstanceCount;     // X360 dword_8324E57C

// ---------------------------------------------------------------------------
// The target-instance create / select-current orchestration (X360
// AptCreateTargetInstance @0x82B003B0 + AptChangeTargetInstance @0x82ADB768). The
// host bring-up calls these (in lieu of the un-homed AptUpdateInitialize chain) to
// stand up + select the per-process Apt context. Bodies in AptTarget.cpp.
//   AptCreateTargetInstance(params) -- allocate+construct an AptTarget from the
//       AptUpdateParams block, append it to the instance list, return it.
//   AptChangeTargetInstance(target)  -- make `target` the current context (sets
//       gpAptTarget/gpAptTargetTLS + the GetTarget() TLS mirror).
// ---------------------------------------------------------------------------
AptTarget* AptCreateTargetInstance(const u32* pParams);
AptTarget* AptChangeTargetInstance(AptTarget* pTarget);

// ---------------------------------------------------------------------------
// The per-frame update drivers (bodies in AptUpdate.cpp).
//   AptUpdate @0x82B0DB68        -- run the CURRENT target's frame pacer (bank the
//       elapsed ms on the root movie, tick once per authored frame period, run the
//       deferred generalised-process pass), then flush the GC deferred releases.
//       B4Extern PDB names the entry `void AptUpdate(unsigned int nDeltaTime)`;
//       the Paradise build adds the depth-layer mask + banked-frame cap.
//   AptUpdateTarget @0x82B0DE80  -- AptUpdate with pTarget swapped in as the
//       current context (the CgsGui::AptAux::Update entry point: (ms, -1, 16)).
// ---------------------------------------------------------------------------
void AptUpdate(int nElapsedMs, int nDepthLayerMask, int nMaxBankedFrames);
void AptUpdateTarget(AptTarget* pTarget, int nElapsedMs, int nDepthLayerMask,
                     int nMaxBankedFrames);
