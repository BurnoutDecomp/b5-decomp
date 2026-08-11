#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"

#include "rw/rwcore_structs.h"                              // rw::IResourceAllocator / Resource / BaseResourceDescriptors
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags

// =================================================================================================
// GameShared/GameClasses/SceneManager/CacheManager/CgsCachedTriangleList.cpp
//
// ⭐⭐⭐ THE SHARED TRIANGLE CACHE'S BACKING-STORE ALLOCATION, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//     CgsSceneManager::CachedTriangleList::Prepare  @0x828BE520  (79 insns)
// This replaces the WorldLinkStubs.cpp gate "CachedTriangleList::Prepare: inert", which is
// DELETED with this TU.
//
// -------------------------------------------------------------------------------------------------
// ⭐⭐ WHY THIS ONE MATTERS, and how it was found -- by RUNNING the path, not by reading it.
// The triangle cache's 28 slots have been registered since the producer wave, but no slot had ever
// been marked DIRTY, so `StartUpdateTriangleCaches` had never once allocated a fill command and
// the destination pointer had never once been dereferenced. Forcing the console's own dev switch
// `mbDEBUGForceAllDirty` for a single instrumented boot (fill-worker wave, 2026-08-10) produced 28
// commands and fired a SHIPPED tripwire that had never executed in this project's history:
//     ASSERT mpaTriangleCache != NULL   (CgsTriangleCacheManager.h:172)
// i.e. the shared cache had NO BACKING STORE AT ALL, because this function was a boot gate that
// returned `true` without allocating. ⇒ **a perfect fill worker would have written its triangles
// through a null pointer.** This is the blocker UNDERNEATH the fill worker, and nothing had ever
// surfaced it because the gate is only stale, not dead.
//
// -------------------------------------------------------------------------------------------------
// ⚠️ THE PARAMETER IS A BATCH COUNT, NOT A BYTE COUNT -- and a committed comment said otherwise.
// `TriangleCacheManager::Prepare` passes `li r5, 0x3338` == 13112, which is exactly
// KU_MAX_CACHED_OBJECTS(298) * KU_TRIANGLE_BATCHES_PER_CACHED_OBJECT(44). This body's second
// instruction is `mulli r30, r31, 224` -- it multiplies that parameter by sizeof(Triangle4) to get
// the byte size. So the allocation is 13112 * 224 == 2,937,088 bytes, not 13112 bytes.
// CgsTriangleCacheManager.cpp's "13112 bytes for the shared triangle cache backing store" is
// CORRECTED with this wave (the arithmetic note in CgsTriangleCacheManager_Update.cpp's banner was
// already right).
// ⭐ Per the standing rule a console size literal is reproduced as a `sizeof`, never as the number.
// That is not cosmetic here: 224 is the CONSOLE stride, and writing it literally would silently
// mis-size the arena if the host record ever drifted. `sizeof(CgsGeometric::Triangle4) == 224` was
// confirmed AT RUNTIME this wave (`PROBE StartFill: ... sizeofTriangle4=224`), and Triangle4 is
// pointer-free SoA float data (nine Vector4 + a mask + a tag + three edge-cosine vectors), so it is
// pinnable -- it does not widen on x64.
//
// -------------------------------------------------------------------------------------------------
// SOURCES / METHOD.
// ⚠️ 0x828BE520 is a GENUINE X360 EXPORT-SET HOLE: there is no 0x828BE520.json among the 30,084
// function exports (the directory steps straight over it). Its identity is not guessed -- the name
// `CgsSceneManager::CachedTriangleList::Prepare` was read out of the `xrefs_from` table of its only
// caller, TriangleCacheManager::Prepare @0x828BE738, which is the technique soup_bank §2.1b
// established ("missing from JSON" != "unnamed in IDA"). The 79 instructions were then lifted from
// the image with the scratchpad PPC decoder and every `bl` resolved against the 30,084-entry name
// index. Corroboration, two independent ways:
//   * the PS3 twin exists and its DWARF mangle types the whole signature --
//     `_ZN15CgsSceneManager18CachedTriangleList7PrepareEPN2rw18IResourceAllocatorEi` @0xC7B30C
//     (156 insns) => (rw::IResourceAllocator*, int).
//   * every string operand was READ OUT OF THE IMAGE at the address the asm names:
//       0x820F5510 = "d:\p4\b5_main\...\CacheManager/CgsCachedTriangleList.cpp"   (the __FILE__,
//                    which is also what names this TU)
//       0x8209B970 = "lpAllocator != NULL"                       (baked line 84)
//       0x820F54D8 = "\nCachedTriangleList: Total triangle cache requires "
//       0x82039068 = " bytes\n"
//       0x820F54C4 = "CachedTriangleList"                        (the allocation name)
//       0x8200F918 = "mpaTriangleCache != NULL"                  (baked line 103)
//
// The five-pool descriptor + virtual DoAllocate idiom below is NOT invented for this body: it is
// store-for-store the same sequence its own caller uses two dozen instructions later for
// "CachedObjectSlots" (already committed in CgsTriangleCacheManager.cpp), down to the same vtable
// slot (`lwz r11, 16(vtbl)`), the same sret Resource, and the same {size,align} pair written into
// pool 0 as one 8-byte `std`.
// =================================================================================================

namespace CgsSceneManager
{
    // ---------------------------------------------------------------------------------------------
    // CachedTriangleList::Prepare @0x828BE520 (79 insns)
    //
    // Allocate the shared SoA triangle-batch arena that every cache slot's window points into.
    // liNumTriangleBatches is the TOTAL batch capacity across all 298 slots (13112).
    // ---------------------------------------------------------------------------------------------
    bool CachedTriangleList::Prepare(rw::IResourceAllocator* lpAllocator, s32 liNumTriangleBatches)
    {
        // 0x828BE53C..0x828BE560 -- baked CgsCachedTriangleList.cpp:84.
        CGS_ASSERT(lpAllocator != NULL, "lpAllocator != NULL");

        // 0x828BE568 `mulli r30, r31, 224`.
        const s32 liNumBytes = liNumTriangleBatches * static_cast<s32>(sizeof(CgsGeometric::Triangle4));

        // 0x828BE564..0x828BE5C0 -- the DEV size report, gated on bit 0 of the message filter
        // (`ld r11, qword_82F31908 ; rldicl r11,r11,0,63 ; beq`) and printed through the global
        // stream at off_82F31904, which is this tree's CgsDev::Log::gpDebugPrint. The X360 emits it
        // as three calls (operator<<(const char*), operator<<(int), operator<<(const char*)); the
        // chained form below is the same three calls in the same order.
        // ⭐ Reproduced rather than dropped: it is the console's own witness of how big the arena
        // is, and a dropped diagnostic is how a wrong arena size would go unnoticed.
        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint << "\nCachedTriangleList: Total triangle cache requires "
                                       << liNumBytes << " bytes\n";
        }

        // 0x828BE5C4..0x828BE614 -- five {size, alignment} pools, all initialised {0, 1} by the
        // console's own 5-iteration loop, then pool 0 overwritten with {liNumBytes, 16} as a single
        // 8-byte store. Five entries (not the PC rwcore <4>) is the documented X360/PS3
        // serialised-descriptor drift; DoAllocate's declared parameter is the narrower <4> alias, so
        // it is reinterpret_cast down exactly as CgsTriangleCacheManager.cpp already does.
        rw::BaseResourceDescriptors<5> lTriangleCacheResDesc;
        lTriangleCacheResDesc.m_baseResourceDescriptors[0].m_size      = liNumBytes;
        lTriangleCacheResDesc.m_baseResourceDescriptors[0].m_alignment = 0x10;
        lTriangleCacheResDesc.m_baseResourceDescriptors[1].m_size      = 0;
        lTriangleCacheResDesc.m_baseResourceDescriptors[1].m_alignment = 1;
        lTriangleCacheResDesc.m_baseResourceDescriptors[2].m_size      = 0;
        lTriangleCacheResDesc.m_baseResourceDescriptors[2].m_alignment = 1;
        lTriangleCacheResDesc.m_baseResourceDescriptors[3].m_size      = 0;
        lTriangleCacheResDesc.m_baseResourceDescriptors[3].m_alignment = 1;
        lTriangleCacheResDesc.m_baseResourceDescriptors[4].m_size      = 0;
        lTriangleCacheResDesc.m_baseResourceDescriptors[4].m_alignment = 1;

        // 0x828BE618..0x828BE62C -- virtual DoAllocate through the allocator vtable's slot 4, sret
        // Resource, then pool 0's pointer becomes the arena base.
        rw::Resource lTriangleCacheRes = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lTriangleCacheResDesc), "CachedTriangleList");
        mpaTriangleCache = static_cast<CgsGeometric::Triangle4*>(lTriangleCacheRes.m_baseResources[0]);

        // 0x828BE630..0x828BE64C -- baked CgsCachedTriangleList.cpp:103. The console stores the
        // pointer BEFORE testing it, which is reproduced above.
        CGS_ASSERT(mpaTriangleCache != NULL, "mpaTriangleCache != NULL");

        // 0x828BE650 `li r3, 1` -- the body has no failure path.
        return true;
    }
}
