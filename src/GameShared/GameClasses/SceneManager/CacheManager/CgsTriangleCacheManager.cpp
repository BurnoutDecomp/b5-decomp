#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"

#include "rw/rwcore_structs.h"                       // rw::IResourceAllocator / Resource / ResourceDescriptor
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsSceneManager::TriangleCacheManager::Prepare  @ 0x828BE738
//
// Allocates and initialises the per-object cache-slot pool. The X360 body:
//   1. CGS_ASSERT(lpAllocator != NULL)                        (baked line 140)
//   2. mTrianglesForCachedObjects.Prepare(lpAllocator, 13112) (the shared cache;
//      `this` is passed straight through as the CachedTriangleList sub-object @ +0)
//   3. Builds a 5-pool rw::BaseResourceDescriptors<5> on the stack -- pool 0 = {size
//      0x37E0 (14304 = 298 slots * 48), align 0x10}, pools 1..4 = {size 0, align 1}
//      (the X360/PS3 serialised-descriptor drift documented in rwcore_structs.h and
//      CgsResourceType.h -- 5 entries, not the PC rwcore <4>) -- and allocates
//      "CachedObjectSlots" through the allocator's virtual DoAllocate (reinterpret_cast
//      down to the <4> alias DoAllocate is declared with, same idiom as rwgpfxtint.cpp's
//      Tint::InitializePixelProgram). The returned Resource's first pool pointer becomes
//      mpaCachedObjectSlots.
//   4. CGS_ASSERT(mpaCachedObjectSlots != NULL)               (baked line 164)
//   5. Initialises all 298 slots (the X360 unrolls 2 per iteration over the 48-byte
//      stride): clears the cached sphere (16-byte vector store), sets
//      miIndexIntoTriangleCache to a running offset that grows by 44 per slot, and
//      zeroes miNumCachedTriangleBatches and miOverflow.
//   6. returns true.
//
// VMX NOTE: the per-slot 16-byte `lvx128/vrlimi128/stvx128` sequence loads the slot,
// merges in a zero vector (the `vrlimi128 ...,v0,1,0` steps), and stores back -- i.e.
// it clears mLastCachedSphere. Lowered here to a plain SetZero() of that field; the
// merge idiom is a vector-clear with no observable difference for an allocator-zeroed
// buffer. Faithful, not byte-exact (the SIMD lane-merge cannot be expressed in
// portable C++); flagged for review.
//
// The running index step is 44 (0x2C): the asm carries r9 (slot 2k's index) and
// r8 = r9 + 0x2C (slot 2k+1's index), then advances r9 by 0x58 (= 88 = 2*44) per
// iteration -- i.e. slot N gets index N*44.

namespace CgsSceneManager
{
    bool TriangleCacheManager::Prepare(rw::IResourceAllocator* lpAllocator)
    {
        CGS_ASSERT(lpAllocator != NULL, "lpAllocator != NULL");

        // ⚠️ CORRECTED 2026-08-10 (fill-worker wave): this used to read "13112 bytes for the
        // shared triangle cache backing store". 13112 is a BATCH COUNT, not a byte count --
        // KU_MAX_CACHED_OBJECTS(298) * KU_TRIANGLE_BATCHES_PER_CACHED_OBJECT(44) -- and the callee's
        // second instruction (`mulli r30, r31, 224` @0x828BE568) multiplies it by sizeof(Triangle4)
        // to reach the real 2,937,088-byte arena. The callee is now a real body
        // (CgsCachedTriangleList.cpp); it used to be a WorldLinkStubs gate that allocated nothing.
        mTrianglesForCachedObjects.Prepare(lpAllocator,
                                           KU_MAX_CACHED_OBJECTS * KU_TRIANGLE_BATCHES_PER_CACHED_OBJECT);

        // Build the 5-pool descriptor: pool 0 holds the slot array, the rest empty.
        // The X360 stack layout builds FIVE (size,align) pairs (var_50..var_30), not
        // four -- this matches the documented X360/PS3 serialised-descriptor drift
        // (rw::BaseResourceDescriptors<5>, see rwcore_structs.h and
        // CgsResourceType.h's CgsResource::ResourceDescriptor typedef). DoAllocate's
        // declared parameter is the PC's narrower <4> alias, so reinterpret_cast the
        // <5> descriptor down to it, same idiom as rw::graphics::postfx::Tint::
        // InitializePixelProgram (rwgpfxtint.cpp).
        rw::BaseResourceDescriptors<5> lCachedObjSlotsResDesc;
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[0].m_size      = KU_MAX_CACHED_OBJECTS * sizeof(CacheSlot); // 0x37E0
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[0].m_alignment = 0x10;
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[1].m_size      = 0;
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[1].m_alignment = 1;
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[2].m_size      = 0;
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[2].m_alignment = 1;
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[3].m_size      = 0;
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[3].m_alignment = 1;
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[4].m_size      = 0;
        lCachedObjSlotsResDesc.m_baseResourceDescriptors[4].m_alignment = 1;

        // Allocate through the allocator's virtual DoAllocate; the first pool's
        // pointer is the slot array base. (X360 indirect call through the
        // allocator vtable, descriptor + "CachedObjectSlots" name.)
        rw::Resource lCachedObjSlotsRes = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lCachedObjSlotsResDesc), "CachedObjectSlots");
        mpaCachedObjectSlots = static_cast<CacheSlot*>(lCachedObjSlotsRes.m_baseResources[0]);

        CGS_ASSERT(mpaCachedObjectSlots != NULL, "mpaCachedObjectSlots != NULL");

        // Initialise every slot: clear the cached sphere, set the running triangle-
        // cache window index (slot N -> N*44), zero the batch/overflow counters.
        s32 liIndexIntoTriangleCache = 0;
        for (u32 luSlot = 0; luSlot < KU_MAX_CACHED_OBJECTS; ++luSlot)
        {
            CacheSlot& lrSlot = mpaCachedObjectSlots[luSlot];
            lrSlot.mLastCachedSphere.SetZero();
            lrSlot.miIndexIntoTriangleCache   = liIndexIntoTriangleCache;
            lrSlot.miNumCachedTriangleBatches = 0;
            lrSlot.miOverflow                 = 0;
            liIndexIntoTriangleCache += 44;
        }

        return true;
    }

    // Reconstructed from BURNOUT_X360_ARTIST.XEX
    //   CgsSceneManager::TriangleCacheManager::GetTrianglesForCachedObject  @ 0x82277790
    //   (IDA symbol truncated to ...CachedObjec by name-length limits)
    //
    // Returns the first cached SoA triangle-batch for cached-object slot
    // liObjectIndex. The X360 body is a single leaf that inlines
    // CachedTriangleList::GetCachedTriangle:
    //   1. v3 = mpaCachedObjectSlots[liObjectIndex].miIndexIntoTriangleCache
    //      (lwz r30, 0x24(base) with base = mpaCachedObjectSlots + index*48 -- slot
    //      stride 48 = slwi/add producing index*3<<4, +0x24 = miIndexIntoTriangleCache).
    //   2. CGS_ASSERT(mpaTriangleCache != NULL)  (baked CgsCachedTriangleList.h:153;
    //      the inlined GetCachedTriangle guard; tests mTrianglesForCachedObjects's
    //      first member at this+0).
    //   3. return mpaTriangleCache + v3   (mulli r11, r30, 0xE0 -> v3 * sizeof(Triangle4)
    //      == 224, added to the cache base).
    //
    // NOTE: the assert reads the cache pointer AFTER the slot index is loaded, matching
    // the asm (`lwz r30, 0x24(r11)` precedes the NULL test on `0(this)`); expressed here
    // as the two-step inlined form via GetCachedTriangle so the guard + base+index math
    // live in their DWARF home (CachedTriangleList).
    const CgsGeometric::Triangle4* TriangleCacheManager::GetTrianglesForCachedObject(s32 liObjectIndex) const
    {
        s32 liIndexIntoTriangleCache = mpaCachedObjectSlots[liObjectIndex].miIndexIntoTriangleCache;
        return mTrianglesForCachedObjects.GetCachedTriangle(liIndexIntoTriangleCache);
    }
}
