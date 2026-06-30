#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestDeepest.h" // InEventVolumeTestDeepest element (224B opaque payload)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest>::AddEvent
//   @ 0x82210870   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Appends one InEventVolumeTestDeepest to the volume-test-deepest input queue and
// returns 1. The assert string baked in this body is
//   "CgsModule::BaseEventQueue<class CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest>::AddEvent"
// (CgsBaseEventQueue.h:312/313), confirming this is the BaseEventQueue<T> -- not the
// derived EventQueue<T,N> -- AddEvent instantiation. Called from
// SceneManagerIO::SceneQueryInterface::VolumeTestDeepest and
// WorldModule::BridgePhysicsSceneQueriesToScene.
//
// NOTE: the queued element CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest is a
// DIFFERENT type from the forward-declared CgsSceneManager::InEventVolumeTestDeepest used by
// FineIntersectionTestModule (different namespace -- ::SceneManagerIO:: vs plain ::CgsSceneManager::).
// Do NOT conflate/reuse them.
//
// X360 store-for-store (asm at 0x82210870), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   lwz r11, 0(this)               ; assert mpEvents != NULL (line 312, non-gating tripwire)
//   lwz r11, 8(this); lwz r10, 4(this); cmpw r11, r10
//   blt .append                    ; skip the "Reached Max length" assert when miLength < miMaxLength
//   ... (line 313 tripwire, builds "...AddEvent\nReached Max length <n>\n") ...
// .append:  (loc_8221097C)
//   lwz   r11, 8(this) (miLength); mulli r10, r11, 0xE0     ; stride 224 == sizeof element
//   lwz   r9, 0(this) (mpEvents);  add r31, r10, r9         ; &mpEvents[miLength]
//   addi  r11, r11, 1; stw r11, 8(this)                     ; ++miLength
//   lvx128/stvx128 x4 @ +0x00/+0x10/+0x20/+0x30 of src/dst  ; first 64 bytes (4 SIMD lanes)
//   lwz/stw x4 @ +0x40/+0x44/+0x48/+0x4C                    ; next 16 bytes  -> 0x00..0x4F
//   memcpy(dst+0x50, src+0x50, 0x80)                        ; bytes 0x50..0xCF (128 bytes, li r5,0x80)
//   lbz/stb @ +0xD0                                          ; final byte -> last touched offset 0xD0
//   li r3, 1; return 1
// == BaseEventQueue<InEventVolumeTestDeepest>::AddEvent: append the 224-byte element
// unconditionally (the two asserts are non-gating tripwires), bump miLength, return
// true. Live payload touched = 0x00..0xD0 inclusive (209 bytes: 64B SIMD + 16B words +
// 128B memcpy + 1 trailing byte), padded out to the attested 224-byte (0xE0) stride; this
// matches the generic `mpEvents[miLength] = lEvent` copy via sizeof(T). (Hex-Rays renders
// the memcpy count as 129 -- the authoritative asm literal is li r5,0x80 = 128 plus the
// separate trailing lbz/stb at 0xD0.)
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest& lEvent);