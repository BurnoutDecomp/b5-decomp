#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h"   // InEventLineTestNearest element (64-byte, alignas(16))

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestNearest>::AddEvent
//   @ 0x82210718   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventLineTestNearest>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one InEventLineTestNearest to the nearest-line-test input queue and returns 1.
// The assert string baked in this body is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::InEventLineTestNearest>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the
// derived EventQueue<T,N> -- AddEvent instantiation. Called from
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestNearest and
// WorldModule::BridgePhysicsSceneQueriesToScene.
//
// X360 store-for-store (asm at 0x82210718), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)                  ; assert mpEvents != NULL (line 312, non-gating tripwire)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw r11, r10
//   blt .append                       ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 tripwire) ...
// .append:
//   slwi r10, miLength, 6; add r10, r10, mpEvents   ; &mpEvents[miLength] (stride 0x40 == sizeof element)
//   mtctr 8; do { ld r9,0(src); std r9,0(dst); src+=8; dst+=8; } while(--ctr)
//                                      ; eight back-to-back 8-byte word copies == one 64-byte record copy
//   lwz r11, 8(this); addi r11, r11, 1; stw r11, 8(this) ; ++miLength
//   li r3, 1; return 1
// == BaseEventQueue<InEventLineTestNearest>::AddEvent: append the 64-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return
// true. The 64-byte stride (slwi r,count,6) matches sizeof(InEventLineTestNearest) == 0x40
// (alignas(16) Vector3 line-start/line-end lanes + query/exclusion scalars), the
// X360-attested stride read directly off this very AddEvent (see
// CgsSceneManagerIO_EventLineTestNearest.h). The pseudocode's `do { *v12++ = *v11++; } while(--v13)`
// over 8 _QWORD slots is Hex-Rays unrolling the generic `mpEvents[miLength] = lEvent` 64-byte
// struct copy into eight 8-byte word moves -- not a manual field-by-field recompose.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestNearest>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventLineTestNearest& lEvent);