#include "GameShared/GameClasses/Module/CgsEventQueue.h"           // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsOverlappingPair.h" // CgsSceneManager::OverlappingPair element (16B)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::OverlappingPair, 128>::Construct
//   @ 0x825A7C08   (ledger id: class:CgsSceneManager::SceneManagerIO::OutOverlapPair,128>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 128) OverlappingPair output queue: the 128 OverlappingPair
// records live inline in the derived EventQueue's maEvents[128] buffer, and
// Construct() points the base queue at that inline storage, sets the capacity and
// clears the live count. The generic EventQueue<T, N>::Construct body is already
// inline in CgsEventQueue.h; this TU is the thin explicit instantiation the X360
// emitted out-of-line for the OverlappingPair/128 specialisation used by
// CgsSceneManager::SceneManagerIO::OutOverlapPair (IDA's "OutOverlapPair_128_").
//
// X360 store-for-store (asm at 0x825A7C08), offsets are the BaseEventQueue header
// (mpEvents @0, miMaxLength @4, miLength @8):
//   addi  r30, r31, 0x10        ; lpEventBuffer = &maEvents (this + 0x10)
//   cmplwi cr6, r30, 0; bne     ; assert lpEventBuffer != NULL (vacuous -- &maEvents
//                                 is never null; Hex-Rays `result == -16` is a misread
//                                 of this addi+cmplwi)
//   stw   r30, 0(r31)           ; mpEvents     = &maEvents
//   li    r11, 0x80 ; stw r11, 4(r31)  ; miMaxLength = 128
//   li    r10, 0    ; stw r10, 8(r31)  ; miLength    = 0
//   return this
// == BaseEventQueue<OverlappingPair>::Construct(maEvents, KI_LENGTH==128): mpEvents =
// maEvents, miMaxLength = 128, miLength = 0 -- the three header stores, plus the
// generic's lpEventBuffer != NULL assert. The maEvents store landing at this+0x10
// (vs +0xC for the 4-byte-aligned ErrorEvent/128 sibling) is the 4 bytes of tail
// padding the compiler inserts after the 12-byte {T*,s32,s32} header so that
// maEvents[] meets OverlappingPair's 8-byte alignment (two u64 VolumeInstanceId
// handles) -- it does NOT imply 16-byte element alignment. The element is the
// already-committed 16-byte CgsSceneManager::OverlappingPair (see CgsOverlappingPair.h,
// grounded by BaseEventQueue<OverlappingPair>::AddEvent @0x828B8B08, stride 0x10).
//
// Callers (X360): BrnPhysics::PhysicsModuleIO::InputBuffer::Construct,
// BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics::Construct,
// CgsSceneManager::SceneManagerIO::OutputBuffer::Construct.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::OverlappingPair, 128>::Construct();
