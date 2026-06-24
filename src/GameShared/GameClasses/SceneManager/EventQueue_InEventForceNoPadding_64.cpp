#include "GameShared/GameClasses/Module/CgsEventQueue.h"                           // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventForceNoPadding.h" // InEventForceNoPadding element

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventForceNoPadding, 64>::Construct
//   @ 0x822E1F40   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventForceNoPadding,64>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 64) force-no-padding input queue
// (InSceneUpdateInterface::mForceNoPaddingQueue, DWARF CgsSceneManagerIO_SceneUpdate.h:283):
// the 64 InEventForceNoPadding records live inline in the derived EventQueue's maEvents[64]
// buffer, and Construct() points the base queue at that inline storage, sets the capacity
// and clears the live count. The generic EventQueue<T, N>::Construct body is already inline
// in CgsEventQueue.h; this TU is the thin explicit instantiation the X360 emitted
// out-of-line for the InEventForceNoPadding/64 specialisation. Reached from
// InSceneUpdateInterface::Construct.
//
// X360 store-for-store (asm at 0x822E1F40), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10           ; lpEventBuffer = &maEvents (this + 16; the element is an
//                                  ;   8-byte VolumeInstanceId (8-aligned), so the 12-byte
//                                  ;   header rounds up 12->16 before maEvents)
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
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventForceNoPadding, 64>::Construct();
