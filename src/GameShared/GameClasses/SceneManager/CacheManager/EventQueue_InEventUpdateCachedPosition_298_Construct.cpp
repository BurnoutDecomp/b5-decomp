#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                     // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"        // TriangleCacheManagerIO::InEventUpdateCachedPosition (32-byte element)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition, 298>::Construct
//   @ 0x822E2560   (ledger id: class:CgsSceneManager::TriangleCacheManagerIO, Hex-Rays name
//    InEventUpdateCachedPosition_2)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (N = 298) update-cached-position
// request queue: the 298 events live inline in the derived EventQueue's maEvents[298] buffer,
// and Construct() points the base queue at that inline storage, sets the capacity (0x12A == 298)
// and clears the live count. The generic EventQueue<T,N>::Construct body is already inline in
// CgsEventQueue.h / CgsBaseEventQueue.h; this TU is the thin explicit instantiation.
//
// X360 store-for-store (asm at 0x822E2560, offsets are the BaseEventQueue header):
//   addi  r30, this, 0x10       ; lpEventBuffer = &maEvents (this + 0x10) -- NOT +0xC like the
//                               ;   InEventAddToCache/RemoveFromCache Construct siblings: this
//                               ;   element carries a Vector3Plus (alignas(16)), so the 12-byte
//                               ;   BaseEventQueue header is padded up to 16 before maEvents.
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous: &maEvents never null;
//    the Hex-Rays `result == -16` is a misread of the addi+cmplwi against &maEvents)
//   stw   r30, 0(this)          ; mpEvents     = &maEvents
//   li    r11, 0x12A ; stw r11, 4(this)  ; miMaxLength = 298
//   li    r10, 0     ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 298.
//
// Caller (X360): CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct constructs
// this queue (one of the TriangleCacheManagerIO InputBuffer's three EventQueues).
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition, 298>::Construct();
