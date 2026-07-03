#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                                   // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"        // TriangleCacheManagerIO::InEventRemoveFromCache (4-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache>::AddEvent
//   @ 0x825E48C0   (ledger id: class:CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent body is
// already inline in CgsBaseEventQueue.h (two non-gating assert tripwires -- `mpEvents != NULL`
// (:312) and the de-inlined StrStream "CgsModule::BaseEventQueue<class CgsSceneManager::
// TriangleCacheManagerIO::InEventRemoveFromCache>::AddEvent\nReached Max length " (:313), which
// collapse to the generic CGS_ASSERTs) -- then an UNCONDITIONAL append + ++miLength + return
// true. This is the thin explicit instantiation.
//
// Element stride is 4 bytes: the append does `lwz r11,8(this) (miLength); slwi r11,r11,2;
// lwz r10,0(a2); stwx r10, r11, mpEvents` -- ONE 32-bit store copying the whole
// { s32 miCacheSlot } record. So sizeof(InEventRemoveFromCache) == 4 (empty CgsModule::Event
// base + single s32), matching the stride-4 slwi and single-word copy.
//
// Called from (X360): BrnPhysics::Deformation::PhysicalBodyPart::RemoveFromScene,
// PhysicalWheel::RemoveFromScene, and PropManager::{RemoveProp, RemovePart,
// RemoveAllPropsAndParts} -- each queues a cache-slot eviction on the scene's tri-cache
// InputBuffer's remove queue (the derived EventQueue<InEventRemoveFromCache,298> appends
// through this inherited base AddEvent).
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache>::AddEvent(
    const CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache& lEvent);
