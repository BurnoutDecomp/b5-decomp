#include "GameShared/GameClasses/Module/CgsEventQueue.h"                // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerErrorEvent.h" // CgsSceneManager::ErrorEvent element

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::ErrorEvent, 128>::Construct
//   @ 0x828C4B10   (ledger id: class:CgsSceneManager::ErrorEvent,128>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 128) ErrorEvent queue: the 128 ErrorEvent records live
// inline in the derived EventQueue's maEvents[128] buffer (right after the 12-byte
// BaseEventQueue header), and Construct() points the base queue at that inline
// storage, sets the capacity and clears the live count. The generic
// EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU is
// the thin explicit instantiation the X360 emitted out-of-line for the
// ErrorEvent/128 specialisation.
//
// X360 store-for-store (asm at 0x828C4B10, offsets are the BaseEventQueue header):
//   addi  r30, this, 0xC        ; lpEventBuffer = &maEvents (this + 12)
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous: &maEvents is
//    never null; the Hex-Rays `result == -12` is a misread of this addi+cmplwi)
//   stw   r30, 0(this)          ; mpEvents     = &maEvents
//   li    r11, 0x80 ; stw r11, 4(this)  ; miMaxLength = 128
//   li    r10, 0    ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 128, which
// stores mpEvents = maEvents (= this + 0xC, the inline buffer offset), miMaxLength =
// 128, miLength = 0 -- matching the three header stores exactly. The mpEvents store
// landing at this+0xC confirms the ErrorEvent element sits inline at offset 12,
// immediately past the {T* @0, s32 @4, s32 @8} header (no alignment pad -> element
// alignment <= 4; see CgsSceneManagerErrorEvent.h).
//
// Caller (X360): the ErrorEvent queue is the embedded error buffer constructed by
// CgsSceneManager::SceneManagerIO::OutputBuffer::Construct.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::ErrorEvent, 128>::Construct();
