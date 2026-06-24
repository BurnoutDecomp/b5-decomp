#include "GameShared/GameClasses/Module/CgsEventQueue.h"                            // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetPadding.h"  // InEventSetPadding element (32B)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventSetPadding, 1280>::Construct
//   @ 0x822E2330   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventSetPadding,1280>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 1280) set-padding input queue
// (InSceneUpdateInterface::mSetPaddingQueue): the 1280 InEventSetPadding records live inline
// in the derived EventQueue's maEvents[1280] buffer, and Construct() points the base queue at
// that inline storage, sets the capacity and clears the live count. The generic
// EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU is the thin
// explicit instantiation the X360 emitted out-of-line for the InEventSetPadding/1280
// specialisation. Reached from InSceneUpdateInterface::Construct.
//
// X360 store-for-store (asm at 0x822E2330), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   addi r30, this, 0x10           ; lpEventBuffer = &maEvents (this + 16; the 12-byte
//                                  ;   header rounds up to the element's 16-byte alignment)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                  ;   vacuous (&maEvents never null); the Hex-Rays `result == -16`
//                                  ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)               ; mpEvents    = &maEvents
//   li  r11, 0x500; stw r11, 4(this) ; miMaxLength = 1280 (0x500)
//   li  r10, 0;     stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 1280.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventSetPadding, 1280>::Construct();
