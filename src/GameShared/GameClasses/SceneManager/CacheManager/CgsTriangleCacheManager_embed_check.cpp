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

// ---------------------------------------------------------------------------
// Slot-bookkeeping wave 2026-08-10: the offsets the FOUR newly reconstructed bodies
// in CgsTriangleCacheManager_Events.cpp actually reach through. Every one of these
// was independently re-derived from a DIFFERENT X360 function than the Prepare loop
// that seeded the header, so these lines are a genuine cross-check, not a restatement:
//   +0x10 mInnerSpherePositionAndRadius   `addi r11, r3, 0x10`   (UpdateCachedObject @0x828BE660)
//   +0x28 miNumCachedTriangleBatches      `lwz r11, 0x28(r3)`    (UpdateCachedObject)
//   +0x2C miOverflow                      `sth r24, 0x2C(r11)`   (ProcessRemoveFromCacheEvents)
//   +0x2E mbIsDirty                       `stb r10, 0x2E(r3)`    (UpdateCachedObject + Remove)
//   stride 48                             `slwi r9,r11,1; add; slwi r11,r11,4` (all three Process*)
//
// ⚠️ SAFE TO PIN, unlike a serialized record: CacheSlot is carved at runtime by
// TriangleCacheManager::Prepare out of the rw resource allocator and contains NO
// pointer members, so none of these offsets is a 32-vs-64-bit width question and
// pinning them cannot freeze a pointer-width bug in place.
static_assert(offsetof(CgsSceneManager::CacheSlot, mInnerSpherePositionAndRadius) == 0x10,
              "CacheSlot.mInnerSpherePositionAndRadius @ +0x10");
static_assert(offsetof(CgsSceneManager::CacheSlot, mbIsDirty) == 0x2E, "CacheSlot.mbIsDirty @ +0x2E");
static_assert(sizeof(CgsSceneManager::CacheSlot::miOverflow) == 2,
              "miOverflow is 16-bit -- ProcessRemoveFromCacheEvents clears it with `sth`, not `stw`");

// ⛔⛔ TriangleCacheManager's OWN offsets are deliberately NOT pinned to the console
// numbers, and this is a correction made after the gate below caught the mistake.
// The first draft of this file asserted the X360 values the asm shows --
//   +0x04 mpaCachedObjectSlots (`lwz r10, 4(r30)`), +0x08 mUsedCacheSlots
//   (`addi r10, r14, 8`), +0x40 mvfMaxRadiusSoFar (`addi r11, r30, 0x40`)
// -- and all three FAILED to compile. They were wrong, not the header: unlike
// CacheSlot, TriangleCacheManager carries THREE pointer members ahead of them
// (CachedTriangleList::mpaTriangleCache, mpaCachedObjectSlots itself, and the
// stream/job pair), and this object is carved at runtime by Prepare rather than
// deserialised, so those pointers legitimately widen 4 -> 8 on x64 and every
// following member slides. Pinning the console bytes here would have asserted a
// 32-bit console layout onto a 64-bit host object -- the exact failure the
// "parity by NAMED MEMBERS, not byte offsets" rule exists to prevent.
// The console offsets remain recorded (in the header, and above) as what the X360
// asm reaches; what is CHECKED here is the platform-independent invariant: the
// declaration ORDER those offsets prove, and the alignment that decides the tail.
static_assert(offsetof(CgsSceneManager::TriangleCacheManager, mpaCachedObjectSlots)
                  < offsetof(CgsSceneManager::TriangleCacheManager, mUsedCacheSlots),
              "mpaCachedObjectSlots precedes mUsedCacheSlots (X360 +0x04 then +0x08)");
static_assert(offsetof(CgsSceneManager::TriangleCacheManager, mUsedCacheSlots)
                  < offsetof(CgsSceneManager::TriangleCacheManager, mvfMaxRadiusSoFar),
              "mUsedCacheSlots precedes mvfMaxRadiusSoFar (X360 +0x08 then +0x40)");
static_assert(offsetof(CgsSceneManager::TriangleCacheManager, mpaCachedObjectSlots)
                  == sizeof(CgsSceneManager::CachedTriangleList),
              "mpaCachedObjectSlots sits immediately after the CachedTriangleList sub-object "
              "(X360 passes `this` unchanged as that sub-object, so it must stay at offset 0)");
// mvfMaxRadiusSoFar is only reachable by a 16-byte vector load/store on the console
// (`lvx128`/`stvx128` with no scalar fallback), so its 16-alignment is load-bearing
// on ANY platform and IS pinned.
static_assert(offsetof(CgsSceneManager::TriangleCacheManager, mvfMaxRadiusSoFar) % 16 == 0,
              "mvfMaxRadiusSoFar must stay 16-aligned (console reaches it only via lvx128/stvx128)");

// The three DWARF event records, pinned at the fields the asm indexes.
static_assert(sizeof(CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache) == 8,
              "InEventAddToCache is 8 bytes");
static_assert(sizeof(CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache) == 4,
              "InEventRemoveFromCache is 4 bytes");
static_assert(offsetof(CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache, mfCacheSphereRadius) == 4,
              "ProcessAddToCacheEvents reads the radius at event+4 (`lvlx v0, r28, r15` with r15==4)");
static_assert(offsetof(CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition,
                       mNewPositionAndRadius) == 0x10,
              "UpdateCachedObject reads the new pose at event+0x10 (`lvx128 v12, r4, r9` with r9==0x10)");
