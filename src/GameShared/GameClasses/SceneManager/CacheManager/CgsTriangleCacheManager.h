#ifndef CGS_TRIANGLE_CACHE_MANAGER_H
#define CGS_TRIANGLE_CACHE_MANAGER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector3Plus
#include "GameShared/GameClasses/Containers/CgsBitArray.h" // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h" // CgsGeometric::Triangle4
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT (inlined GetCachedTriangle guard)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"  // CgsModule::EventQueue<T, N>
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h" // InEvent{AddToCache,UpdateCachedPosition}

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // The per-frame scene-update input interface (CgsSceneManagerIO_SceneUpdate.h).
    // ProcessRemoveFromCacheEvents takes the WHOLE interface rather than just its own
    // queue -- see the declaration below for why. Forward-declared to keep this header
    // off the 200KB scene-update aggregate; the .cpp includes it.
    struct InSceneUpdateInterface;
}
}

// ============================================================================
// GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h
//
// CgsSceneManager::TriangleCacheManager + its per-object CacheSlot. Minimal
// OWNING home reconstructed from the DecFIGS DWARF (CgsTriangleCacheManager.h)
// and the X360 ARTIST build (Prepare @ 0x828BE738).
//
// The TriangleCacheManager owns a fixed pool of K_MAX_CACHED_OBJECTS (298) cache
// slots, each describing one cached collision object's bounding sphere and its
// window into the shared triangle cache. The slot array is allocated through the
// RenderWare resource allocator (Prepare) and tracked by a BitArray<298>.
//
// SCOPE: this header models the slice the recovered bodies touch plus the
// DWARF-attested member set. Members not yet exercised keep their DWARF
// names/types as honest placeholders.
//
// BODIES, and where they live (updated 2026-08-10, triangle-cache wave -- the
// previous note here said "this TU's ledger holds exactly one function: Prepare",
// which had already gone stale):
//   CgsTriangleCacheManager.cpp         Prepare @0x828BE738,
//                                       GetTrianglesForCachedObject @0x82277790
//   CgsTriangleCacheManager_Events.cpp  ProcessAddToCacheEvents @0x828B2C78,
//                                       ProcessRemoveFromCacheEvents @0x828B2710,
//                                       ProcessUpdateCachedPositionEvents @0x828BE898,
//                                       CacheSlot::UpdateCachedObject @0x828BE660
//   NOT RECONSTRUCTED                   StartUpdateTriangleCaches @0x828BECF8 (278),
//                                       EndUpdateTriangleCaches @0x828BF150 (475)
//                                       -- the FILL half; both are WorldLinkStubs gates
//                                       reached every frame by WorldModule::Update.
// ============================================================================

namespace rw
{
    // Forward decls -- the rw resource allocator is declared in
    // vendor/renderware/include/rw/rwcore_structs.h. Prepare only takes the
    // pointer and dispatches one virtual allocate through it.
    struct IResourceAllocator;
}

namespace CgsSceneManager
{
    // Capacity of the cache-slot pool. 298 slots * 48 bytes == 0x37E0 (14304),
    // the byte size the X360 Prepare passes to the allocator and the loop bound.
    const u32 KU_MAX_CACHED_OBJECTS = 298u;

    // ProcessUpdateCachedPositionEvents only folds a slot's radius into
    // mvfMaxRadiusSoFar when the slot index is below this bound (X360
    // `cmpwi cr6, r11, 8 ; bge` at 0x828BEC40). Carried as the literal the image
    // holds. ⚠️ INFERRED, flagged: 8 is also the race-car capacity elsewhere in this
    // tree (VehicleOutputInterface::mUsedRaceCars is a BitArray<8>), so the shipped
    // spelling was plausibly that constant -- but nothing in this function's rodata
    // or asserts names it, so no cross-subsystem constant is asserted here.
    const s32 KI_MAX_RADIUS_TRACKED_SLOTS = 8;

    // ------------------------------------------------------------------------
    // CgsSceneManager::CacheSlot (CgsTriangleCacheManager.h:78). sizeof == 48.
    // Offsets proven by the X360 Prepare init loop:
    //   +0x00  mLastCachedSphere            (16-byte Sphere; xyz cleared, w kept/merged)
    //   +0x10  mInnerSpherePositionAndRadius (Vector3Plus; the second 16-byte slot... see note)
    //   +0x24  miIndexIntoTriangleCache     (stw r9   -> slot index * 44)
    //   +0x28  miNumCachedTriangleBatches   (stw 0)
    //   +0x2C  miOverflow                   (sth 0, 16-bit)
    // NOTE: the recovered loop writes one 16-byte vector at +0x00 and the three
    // scalar/half fields at +0x24/+0x28/+0x2C per slot (48-byte stride). The
    // remaining DWARF members (mInnerSpherePositionAndRadius / mbDebugRender /
    // mbIsDirty) sit in the +0x10..+0x23 window and are zeroed by the allocator;
    // they are modelled here to fix the 48-byte size and are touched by the other
    // (non-Prepare) CacheSlot TUs.
    // ------------------------------------------------------------------------
    // A 16-byte sphere/vector field (xyz + w). Modelled as a 4-float, 4-aligned
    // POD so the CacheSlot offsets below are byte-exact on the host (an alignas(16)
    // vector type would over-align the slot and shift the trailing scalars). xyz/w
    // lanes are accessed via SetZero()/the named floats. (CgsGeometric::Sphere's
    // full home is a separate TU; this is the minimal sized stand-in.)
    struct CacheSphere
    {
        f32 x, y, z, w;
        void SetZero() { x = y = z = w = 0.0f; }
    };

    struct CacheSlot
    {
        CacheSphere mLastCachedSphere;             // +0x00  bounding sphere (centre.xyz, radius.w)
        CacheSphere mInnerSpherePositionAndRadius; // +0x10  inner sphere (pos.xyz, radius.w)
        bool        mbDebugRender;                  // +0x20  debug draw flag (DWARF :145 order)
        s32         miIndexIntoTriangleCache;       // +0x24  window start in the triangle cache
        s32         miNumCachedTriangleBatches;     // +0x28  batches owned by this slot
        s16         miOverflow;                     // +0x2C  overflow batch count
        bool        mbIsDirty;                      // +0x2E  needs re-cache

        // @ X360 0x828BE660 (54 insns) -- reposition this slot's cached object and decide
        // whether its triangle set has to be re-cached. Called by TriangleCacheManager::
        // ProcessUpdateCachedPositionEvents (that call site is how the symbol was found:
        // it is a `bl CgsSceneManager__CacheSlot__UpdateCachedObject`, NOT open-coded).
        // Body in CgsTriangleCacheManager_Events.cpp.
        void UpdateCachedObject(const TriangleCacheManagerIO::InEventUpdateCachedPosition& lrEvent);
    };

    // ------------------------------------------------------------------------
    // CgsSceneManager::CachedTriangleList -- the shared triangle cache backing
    // store. Prepare delegates to its own Prepare(lpAllocator, 13112) (declared,
    // not defined here -- it is an external/separate TU symbol). Modelled as an
    // opaque-but-sized first member of TriangleCacheManager so `this` (offset 0)
    // is the CachedTriangleList sub-object the X360 passes straight through.
    // ------------------------------------------------------------------------
    struct CachedTriangleList
    {
        // +0x00  mpaTriangleCache -- the shared SoA triangle-batch backing store
        // (DecFIGS DWARF CgsCachedTriangleList.h:132: `Triangle4 * mpaTriangleCache`).
        // Allocated by Prepare; the only instance member the recovered slice reads
        // (the DWARF's other member, saKdTreeResults[5000], is a file-scope static,
        // not part of the object). It is the FIRST member, so it sits at the head of
        // the owning TriangleCacheManager (X360 `lwz r11, 0(this)` in
        // GetTrianglesForCachedObject reads exactly this pointer).
        CgsGeometric::Triangle4* mpaTriangleCache; // +0x00

        // Prepare(allocator, capacityBytes) -- external symbol
        // CgsSceneManager::CachedTriangleList::Prepare. Declared only; this TU
        // does not own its body.
        bool Prepare(rw::IResourceAllocator* lpAllocator, s32 liCapacityBytes);

        // @ CgsCachedTriangleList.h:121 (non-const overload; :117 const). Returns
        // &mpaTriangleCache[liIndex]. The X360 build INLINES this into
        // TriangleCacheManager::GetTrianglesForCachedObject: it asserts the cache is
        // allocated (baked CgsCachedTriangleList.h:153) then returns base + index *
        // sizeof(Triangle4) (0xE0). Defined inline so the manager body below mirrors
        // the single inlined X360 function store-for-store.
        const CgsGeometric::Triangle4* GetCachedTriangle(s32 liIndex) const
        {
            CGS_ASSERT(mpaTriangleCache != NULL, "mpaTriangleCache != NULL");
            return &mpaTriangleCache[liIndex];
        }
    };

    // ------------------------------------------------------------------------
    // CgsSceneManager::TriangleCacheManager (CgsTriangleCacheManager.h:169).
    // Layout proven by Prepare:
    //   +0x00  mTrianglesForCachedObjects  (CachedTriangleList; `this` passed as-is)
    //   +0x04  mpaCachedObjectSlots        (CacheSlot*; `*(this+4) = alloc result`)
    // The remaining DWARF members keep their names/types; they are not touched by
    // Prepare and are exercised by the manager's other (non-Prepare) TUs.
    // ------------------------------------------------------------------------
    struct TriangleCacheManager
    {
        CachedTriangleList         mTrianglesForCachedObjects;  // +0x00
        CacheSlot*                 mpaCachedObjectSlots;        // +0x04
        CgsContainers::BitArray<KU_MAX_CACHED_OBJECTS> mUsedCacheSlots;
        void*                      mpUpdateTriangleCacheStream; // CgsMemory::SimpleDataStreamProducer*
        void*                      mpUpdateTriangleCacheJob;    // EA::Jobs::Job*
        Vector3Plus                mvfMaxRadiusSoFar;
        bool                       mbDEBUGForceAllDirty;

        // This TU's single recovered function.
        bool Prepare(rw::IResourceAllocator* lpAllocator);

        // @ X360 0x82277790 -- returns a pointer to the first cached SoA triangle-batch
        // for the given cached-object slot; tail-called by SceneManagerIO::
        // TriangleCacheInterface::GetCache @ 0x82277810. The IDA symbol is truncated to
        // 'GetTrianglesForCachedObjec' by symbol-length limits; un-truncated to ...Object
        // for the C++ home. DWARF (CgsTriangleCacheManager.h:228) is authoritative for the
        // shape: `const Triangle4 * GetTrianglesForCachedObject(int32_t) const`. Body in
        // the .cpp (mirrors the single inlined X360 function).
        const CgsGeometric::Triangle4* GetTrianglesForCachedObject(s32 liObjectIndex) const;

        // ------------------------------------------------------------------
        // The per-frame SLOT BOOKKEEPING trio. All three are driven by
        // SceneManagerModule::StartUpdateTriangleCache @0x828C73D8, in THIS order
        // (proven by that function's asm at 0x828C7474/0x828C7484/0x828C74D8):
        //     ProcessRemoveFromCacheEvents(lrSceneUpdate)
        //     ProcessAddToCacheEvents(lrSceneUpdate.mAddToCacheQueue)
        //     ProcessUpdateCachedPositionEvents(lrSceneUpdate.mUpdateCachedPositionQueue)
        // ⚠️ AS-SHIPPED ASYMMETRY, not a reconstruction slip: the caller folds the
        // member offset into the argument for the last two (`addis r4,r31,0xC; addi
        // r4,r4,0x4930` == mAddToCacheQueue @+0xC4930, and +0x5290 ==
        // mUpdateCachedPositionQueue @+0xC5290) but passes the WHOLE interface to
        // ProcessRemoveFromCacheEvents, which re-derives its own queue at +0xC77E0.
        // The reason is in the body: Remove also walks mAddToCacheQueue (+0xC4930) for
        // its "Trying to add and remove triangle cache slot in same frame" cross-check,
        // so it genuinely needs both queues. Bodies in CgsTriangleCacheManager_Events.cpp.
        // ------------------------------------------------------------------

        // @ X360 0x828B2C78 (222). Claim each event's slot: stamp the bounding-sphere
        // radius, reset the batch count, and set the slot's used bit.
        void ProcessAddToCacheEvents(
            const CgsModule::EventQueue<TriangleCacheManagerIO::InEventAddToCache,
                                        KU_MAX_CACHED_OBJECTS>& lrQueue);

        // @ X360 0x828B2710 (346). Release each event's slot: clear the dirty/overflow
        // state, drop the cached radius to zero, and clear the slot's used bit. Takes the
        // whole interface because its dev cross-checks scan the ADD queue as well.
        void ProcessRemoveFromCacheEvents(const SceneManagerIO::InSceneUpdateInterface& lrSceneUpdate);

        // @ X360 0x828BE898 (279). Reposition each event's cached object (delegating the
        // re-cache decision to CacheSlot::UpdateCachedObject) and track the largest cache
        // radius seen this frame across the first KI_MAX_RADIUS_TRACKED_SLOTS slots.
        void ProcessUpdateCachedPositionEvents(
            const CgsModule::EventQueue<TriangleCacheManagerIO::InEventUpdateCachedPosition,
                                        KU_MAX_CACHED_OBJECTS>& lrQueue);

        // @ X360 **0x828BF150** (475 insns) -- finish this frame's triangle-cache update
        // against the supplied collision generator + the triangle-collision scene.
        // ⚠️ CORRECTED 2026-08-10: this comment previously read "@ X360 0x828C7508".
        // 0x828C7508 is MID-INSTRUCTION inside SceneManagerModule::EndUpdateTriangleCache
        // @0x828C7500 (the 10-insn thunk's `ori r10, r10, 0x84D0`), not a function head.
        // The real body is 0x828BF150, which is exactly what that thunk tail-branches to.
        // ⚠️ AS-SHIPPED: the ARTIST body reads ONLY r3 -- its 475 instructions never touch
        // the incoming r4/r5 (the sole `arg_` stack slot is a spill of `this`). The two
        // parameters are kept because the CALL SITE demonstrably materialises them
        // (r4 = the generator StartUpdateTriangleCache stashed at module+0x3A84D0).
        // Body NOT reconstructed -- currently the WorldLinkStubs.cpp one-shot log.
        void EndUpdateTriangleCaches(void* lpCollisionGenerator, void* lpTriangleCollisionScene);
    };
}

#endif // CGS_TRIANGLE_CACHE_MANAGER_H
