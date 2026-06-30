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
        // straight back through r3). Pointer-null guard, no lock-bit test.
        s32 GetCache(s32 liObjectIndex) const
        {
            CGS_ASSERT(mpTriangleCacheManager != nullptr, "mpTriangleCacheManager != NULL");
            return mpTriangleCacheManager->GetTrianglesForCachedObject(liObjectIndex);
        }

        // @ X360 0x82277880 (body in CgsSceneManagerModuleIO.cpp). Asserts the manager is set
        // up, then returns the cached-slot's miNumCachedTriangleBatches for the given slot index.
        s32 GetNumCachedTriangleBatches(s32 liCacheSlotIndex) const;
    };
}
}
