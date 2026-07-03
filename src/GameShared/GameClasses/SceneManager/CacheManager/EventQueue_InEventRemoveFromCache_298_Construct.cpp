#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                       // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"        // TriangleCacheManagerIO::InEventRemoveFromCache (4-byte element)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache, 298>::Construct
//   @ 0x822E25D0   (ledger id: class:CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache,298>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (N = 298) remove-from-cache
// request queue: the 298 events live inline in the derived EventQueue's maEvents[298] buffer
// (right after the 12-byte BaseEventQueue header), and Construct() points the base queue at
// that inline storage, sets the capacity (0x12A == 298) and clears the live count. The generic
// EventQueue<T,N>::Construct body is already inline in CgsEventQueue.h / CgsBaseEventQueue.h;
// this TU is the thin explicit instantiation the X360 emitted out-of-line.
//
// X360 store-for-store (asm at 0x822E25D0 -- byte-identical to the InEventAddToCache,298
// Construct @ 0x822E24F0 above, only the element type differs):
//   addi  r30, this, 0xC        ; lpEventBuffer = &maEvents (this + 12)
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous; Hex-Rays `result == -12`
//    is a misread of the addi+cmplwi against &maEvents)
//   stw   r30, 0(this)          ; mpEvents     = &maEvents
//   li    r11, 0x12A ; stw r11, 4(this)  ; miMaxLength = 298
//   li    r10, 0     ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 298. The buffer
// pointer lands at this+0xC: InEventRemoveFromCache is only 4-aligned (single s32 miCacheSlot,
// sizeof 4), so the 12-byte BaseEventQueue header is not padded up to 16 before maEvents.
//
// Caller (X360): CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct constructs
// this queue (one of the TriangleCacheManagerIO InputBuffer's three EventQueues).
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache, 298>::Construct();
