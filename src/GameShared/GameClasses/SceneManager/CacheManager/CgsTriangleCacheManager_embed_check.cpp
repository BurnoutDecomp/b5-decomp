// Embed check for the Scene-Graphics group's reconstructed homes. Verifies the
// owning headers parse together and the key layout invariants hold (compile-time).
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

#include <cstddef>

// CacheSlot must be 48 bytes so 298 slots == 0x37E0 (the X360 Prepare allocation
// size and loop bound).
static_assert(sizeof(CgsSceneManager::CacheSlot) == 48, "CacheSlot must be 48 bytes");
static_assert(CgsSceneManager::KU_MAX_CACHED_OBJECTS * sizeof(CgsSceneManager::CacheSlot) == 0x37E0,
              "298 cache slots must total 0x37E0 bytes");

// Slot field offsets proven by the X360 Prepare init loop.
static_assert(offsetof(CgsSceneManager::CacheSlot, miIndexIntoTriangleCache)   == 0x24, "index @ +0x24");
static_assert(offsetof(CgsSceneManager::CacheSlot, miNumCachedTriangleBatches) == 0x28, "batches @ +0x28");
static_assert(offsetof(CgsSceneManager::CacheSlot, miOverflow)                 == 0x2C, "overflow @ +0x2C");

// CgsGeometric::Line is two 16-byte endpoints (the two lvx128 loads at +0/+0x10).
static_assert(sizeof(CgsGeometric::Line) == 32, "Line must be 32 bytes (two Vector3Plus)");
static_assert(offsetof(CgsGeometric::Line, mEnd) == 0x10, "Line.mEnd @ +0x10");
