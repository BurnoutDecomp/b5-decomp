#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                  // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventReplaceDynamicVolume.h" // InEventReplaceDynamicVolume element

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume, 64>::Construct
//   @ 0x822E2170   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume,64>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 64) replace-dynamic-volume input queue
// (InSceneUpdateInterface::mReplaceDynamicVolumeQueue): the 64
// InEventReplaceDynamicVolume records live inline in the derived EventQueue's
// maEvents[64] buffer, and Construct() points the base queue at that inline storage, sets
// the capacity and clears the live count. The generic EventQueue<T, N>::Construct body is
// already inline in CgsEventQueue.h; this TU is the thin explicit instantiation the X360
// emitted out-of-line for the InEventReplaceDynamicVolume/64 specialisation. Reached from
// InSceneUpdateInterface::Construct.
//
// X360 store-for-store (asm at 0x822E2170), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10           ; lpEventBuffer = &maEvents (this + 16; the 144-byte element
//                                  ;   is 8-aligned, so the 12-byte header pads to 0x10 before
//                                  ;   maEvents)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                  ;   vacuous (&maEvents never null); the Hex-Rays `result == -16`
//                                  ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)               ; mpEvents    = &maEvents
//   li  r11, 0x40; stw r11, 4(this) ; miMaxLength = 64 (0x40)
//   li  r10, 0;    stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 64.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventReplaceDynamicVolume, 64>::Construct();
