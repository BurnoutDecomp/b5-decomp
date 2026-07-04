#ifndef CGS_TRIANGLE_CACHE_MANAGER_H
#define CGS_TRIANGLE_CACHE_MANAGER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector3Plus
#include "GameShared/GameClasses/Containers/CgsBitArray.h" // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h" // CgsGeometric::Triangle4
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT (inlined GetCachedTriangle guard)

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
// SCOPE: this header models the slice the recovered Prepare touches plus the
// DWARF-attested member set. Members NOT exercised by Prepare keep their DWARF
// names/types as honest placeholders; per-method bodies other than Prepare live
// in their own TUs (this TU's ledger holds exactly one function: Prepare).
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

        // @ X360 0x828C7508 (tail-called from SceneManagerModule::EndUpdateTriangleCache)
        // -- finish this frame's triangle-cache update against the supplied collision
        // generator + the triangle-collision scene. Declared here (its home); body owned
        // by the cache-manager TU.
        void EndUpdateTriangleCaches(void* lpCollisionGenerator, void* lpTriangleCollisionScene);
    };
}

#endif // CGS_TRIANGLE_CACHE_MANAGER_H
