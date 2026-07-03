#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                                 // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"        // TriangleCacheManagerIO::InEventAddToCache (8-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache>::AddEvent
//   @ 0x825E4620   (ledger id: class:CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The generic BaseEventQueue<T>::AddEvent body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation. Two non-gating assert tripwires then an
// unconditional 8-byte append + ++miLength + return true (header offsets mpEvents @ 0,
// miMaxLength @ 4, miLength @ 8):
//   * assert mpEvents != NULL (CgsBaseEventQueue.h:312) -- 0x825E4634 lwz r11,0(r29); bne skip.
//   * assert miLength < miMaxLength (CgsBaseEventQueue.h:313, de-inlined StrStream
//     "...InEventAddToCache>::AddEvent\nReached Max length ") -- 0x825E4664 lwz r11,8(r29)/
//     lwz r10,4(r29); blt skip.
//   * append: 0x825E472C lwz r11,8(r29) miLength; slwi r11,r11,3 (x8 stride); add mpEvents;
//     lwz r9,0(r26)/stw r9,0(r11) then lwz r10,4(r26)/stw r10,4(r11) -- two 32-bit words ==
//     the generic single `mpEvents[miLength] = lEvent;` for the 8-byte record.
//   * ++miLength (lwz r11,8; addi 1; stw); li r3,1 -> return true.
//
// Element stride 8 (slwi ...,3) matches sizeof(InEventAddToCache) == 8: CgsModule::Event {}
// base + s32 miCacheSlot (@+0) + f32 mfCacheSphereRadius (@+4) (see CgsTriangleCacheManagerIO.h,
// already committed). Member of the EventQueue<InEventAddToCache,298> family whose Construct
// instantiation is the committed EventQueue_InEventAddToCache_298_Construct.cpp; the sibling
// Append instantiation @ 0x827A69C0 is BaseEventQueue_InEventAddToCache_Append.cpp. Called from
// the tri-cache producers (PhysicalTrafficManager / VehicleManager PrepareTriangleCache,
// PhysicalBodyPart::AddToScene, PhysicalWheel::AddToScene, PropManager::{CreatePart,
// ProcessAddPropInstanceEvents}).
//
// X360-attested element stride (`slwi r, miLength, 3` == *8).
static_assert(sizeof(CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache) == 8,
              "InEventAddToCache stride 8");

template bool
CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache>::AddEvent(
    const CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache& lEvent);
