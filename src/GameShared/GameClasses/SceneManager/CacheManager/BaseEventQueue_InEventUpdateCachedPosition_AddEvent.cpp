#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                                 // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"        // TriangleCacheManagerIO::InEventUpdateCachedPosition (32-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition>::AddEvent
//   @ 0x825E4768   (ledger id: class:CgsSceneManager::TriangleCacheManagerIO)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The generic BaseEventQueue<T>::AddEvent body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation. Two non-gating assert tripwires then an
// unconditional 32-byte append + ++miLength + return true (header offsets mpEvents @ 0,
// miMaxLength @ 4, miLength @ 8):
//   * assert mpEvents != NULL (CgsBaseEventQueue.h:312) -- 0x825E477C lwz r11,0(r29); bne skip.
//   * assert miLength < miMaxLength (CgsBaseEventQueue.h:313, de-inlined StrStream
//     "...InEventUpdateCachedPosition>::AddEvent\nReached Max length ") -- 0x825E47AC
//     lwz r11,8(r29)/lwz r10,4(r29); blt skip.
//   * append: 0x825E4874 lwz r11,8(r29) miLength; slwi r11,r11,5 (x32 stride); add mpEvents;
//     four ld/std pairs at 0,8,0x10,0x18 -- Hex-Rays/PPC lowering of the generic single
//     `mpEvents[miLength] = lEvent;` for a 32-byte, 16-aligned aggregate.
//   * ++miLength (lwz r11,8; addi 1; stw); li r3,1 -> return true.
//
// Element stride 32 (slwi ...,5) matches sizeof(InEventUpdateCachedPosition) == 32: the
// committed struct is CgsModule::Event {} base + s32 miCacheSlot (@+0) + Vector3Plus
// mNewPositionAndRadius (alignas(16), @+0x10) -> sizeof 0x20 (see CgsTriangleCacheManagerIO.h,
// already committed). Member of the EventQueue<InEventUpdateCachedPosition,298> family whose
// Construct instantiation lives in EventQueue_InEventUpdateCachedPosition_298_Construct.cpp.
// Called from the *Manager::UpdateTriangleCache producers (PhysicalTrafficManager /
// DetachedPartManager / DetachedWheelManager / PropManager / VehicleManager).
//
// X360-attested element stride (`slwi r, miLength, 5` == *32).
static_assert(sizeof(CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition) == 32,
              "InEventUpdateCachedPosition stride 32");

template bool
CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition>::AddEvent(
    const CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition& lEvent);
