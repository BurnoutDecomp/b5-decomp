#include "GameShared/GameClasses/Module/CgsEventQueue.h"                     // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddEntity.h" // InEventAddEntity element

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventAddEntity, 5120>::Construct
//   @ 0x822E1D80   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventAddEntity,5120>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 5120) add-entity input queue
// (InSceneUpdateInterface::mAddEntityQueue): the 5120 InEventAddEntity records live
// inline in the derived EventQueue's maEvents[5120] buffer, and Construct() points the
// base queue at that inline storage, sets the capacity and clears the live count. The
// generic EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU
// is the thin explicit instantiation the X360 emitted out-of-line for the
// InEventAddEntity/5120 specialisation. Reached from InSceneUpdateInterface::Construct.
//
// X360 store-for-store (asm at 0x822E1D80), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10           ; lpEventBuffer = &maEvents (this + 16; element is alignas(16),
//                                  ;   so the 12-byte header pads to 0x10 before maEvents)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                  ;   vacuous (&maEvents never null); the Hex-Rays `result == -16`
//                                  ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)               ; mpEvents    = &maEvents
//   li  r11, 0x1400; stw r11, 4(this) ; miMaxLength = 5120 (0x1400)
//   li  r10, 0;      stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 5120.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventAddEntity, 5120>::Construct();
