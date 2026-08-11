#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// CgsSceneManager::SceneManagerIO::TriangleCacheInterface
// (CgsSceneManagerModuleIO.h:~1280 per the assert file/line rodata). A thin
// client-facing wrapper holding a TriangleCacheManager* (+0x00) that forwards to
// the manager once asserting it has been set up.
//
// GetCache @ X360 0x82277810; sibling GetNumCachedTriangleBatches @ 0x82277880
// (its body lives in CgsSceneManagerModuleIO.cpp). Both assert *mpTriangleCacheManager
// != NULL with the SAME message "mpTriangleCacheManager != NULL" (no trailing newline --
// the X360 rodata has none) and a pointer-NULL guard (NOT an IOBuffer lock bit).
// ============================================================================

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct TriangleCacheInterface
    {
        TriangleCacheManager* mpTriangleCacheManager; // +0x00

        // @ X360 0x82277810. lwz r11,0(r31) ; cmplwi 0 ; bne -> assert iff *a1==0,
        // then GetTrianglesForCachedObject(*a1, a2) tail-returned (return passed
        // straight back through r3). Pointer-null guard, no lock-bit test. The X360
        // ABI returns the forwarded pointer in r3; the return type is the manager's
        // GetTrianglesForCachedObject return (const Triangle4*), not s32 (an early
        // Hex-Rays int artifact -- corrected once that manager accessor was homed).
        const CgsGeometric::Triangle4* GetCache(s32 liObjectIndex) const
        {
            CGS_ASSERT(mpTriangleCacheManager != nullptr, "mpTriangleCacheManager != NULL");
            return mpTriangleCacheManager->GetTrianglesForCachedObject(liObjectIndex);
        }

        // @ X360 0x82277880 (body in CgsSceneManagerModuleIO.cpp). Asserts the manager is set
        // up, then returns the cached-slot's miNumCachedTriangleBatches for the given slot index.
        s32 GetNumCachedTriangleBatches(s32 liCacheSlotIndex) const;

        // ⭐ ADDED 2026-08-11 (triangle-cache wiring wave). THE ONLY SEEDER OF THIS
        // INTERFACE IN THE WHOLE PROGRAM -- every other hop is an Append that adopts an
        // already-seeded pointer, which is why nothing downstream had a manager until now.
        // DWARF-declared (CgsSceneManagerModuleIO.h, `void SetTriangleCacheManager(const
        // TriangleCacheManager*)`); the X360 INLINES it into both of its call sites, and both
        // inlined copies bake the SAME tripwire text/line, which is what identifies it:
        //   SceneManagerModule::ProcessSceneQueries @0x828D5934..0x828D5960
        //       cmplwi cr6, r31, 0            (r31 == &mTriangleCacheManager, this+0x3A8260)
        //       bne    -> skip
        //       FireAssert("lpTriangleCacheManager != NULL",
        //                  "..CgsSceneManagerModuleIO.h", 0x4F4 == 1268)
        //       stw    r31, 0(r30)            (r30 == the interface returned by the getter)
        //   SceneManagerModule::UpdateScene @0x828D4C28 -- byte-identical, same :1268.
        // Taken non-const here to match this struct's committed non-const member (the DWARF
        // spells the parameter `const TriangleCacheManager*`; GetTrianglesForCachedObject is
        // const-qualified either way, so the constness is not load-bearing -- de-forking the
        // member's constness is a separate, tree-wide change).
        void SetTriangleCacheManager(TriangleCacheManager* lpTriangleCacheManager)
        {
            CGS_ASSERT(lpTriangleCacheManager != nullptr, "lpTriangleCacheManager != NULL");
            mpTriangleCacheManager = lpTriangleCacheManager;
        }

        // Append == adopt the source interface's manager pointer. Defined at
        // CgsSceneManagerModuleIO.h:1277 in the original tree (the assert's baked
        // file/line); the X360 build inlines it into the world buffer's
        // AppendTriangleCacheInterface @ 0x8279BAF8 (assert-if-null then a single
        // pointer store). Assert message verbatim from the X360 rodata (no newline).
        void Append(const TriangleCacheInterface* lpInterfaceToAppend)
        {
            CGS_ASSERT(lpInterfaceToAppend->mpTriangleCacheManager != nullptr,
                       "lpInterfaceToAppend->mpTriangleCacheManager != NULL");
            mpTriangleCacheManager = lpInterfaceToAppend->mpTriangleCacheManager;
        }
    };
}
}
