#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                 // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetEntityRadius.h"  // InEventSetEntityRadius element (8B)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventSetEntityRadius, 512>::Construct
//   @ 0x822E22C0   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventSetEntityRadius,512>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 512) set-entity-radius input queue
// (InSceneUpdateInterface::mSetEntityRadiusQueue): the 512 InEventSetEntityRadius records
// live inline in the derived EventQueue's maEvents[512] buffer, and Construct() points the
// base queue at that inline storage, sets the capacity and clears the live count. The
// generic EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU is
// the thin explicit instantiation the X360 emitted out-of-line for the
// InEventSetEntityRadius/512 specialisation. Reached from InSceneUpdateInterface::Construct.
//
// X360 store-for-store (asm at 0x822E22C0), offsets are the BaseEventQueue header
// (mpEvents @ 0, miMaxLength @ 4, miLength @ 8):
//   addi r30, this, 0xC            ; lpEventBuffer = &maEvents (this + 12; the 8-byte
//                                  ;   4-aligned element needs no header padding, 12 is
//                                  ;   already 4-aligned)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                  ;   vacuous (&maEvents never null); the Hex-Rays `result == -12`
//                                  ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)               ; mpEvents    = &maEvents
//   li  r11, 0x200; stw r11, 4(this) ; miMaxLength = 512 (0x200)
//   li  r10, 0;     stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 512.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventSetEntityRadius, 512>::Construct();
