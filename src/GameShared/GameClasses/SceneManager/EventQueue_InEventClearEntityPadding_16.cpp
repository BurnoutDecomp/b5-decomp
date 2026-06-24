#include "GameShared/GameClasses/Module/CgsEventQueue.h"                              // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventClearEntityPadding.h" // InEventClearEntityPadding element

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventClearEntityPadding, 16>::Construct
//   @ 0x822E2410   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventClearEntityPadding,16>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 16) clear-entity-padding input queue
// (InSceneUpdateInterface::mClearEntityPaddingQueue, DWARF CgsSceneManagerIO_SceneUpdate.h:360):
// the 16 InEventClearEntityPadding records live inline in the derived EventQueue's
// maEvents[16] buffer, and Construct() points the base queue at that inline storage, sets
// the capacity and clears the live count. The generic EventQueue<T, N>::Construct body is
// already inline in CgsEventQueue.h; this TU is the thin explicit instantiation the X360
// emitted out-of-line for the InEventClearEntityPadding/16 specialisation. Reached from
// InSceneUpdateInterface::Construct.
//
// X360 store-for-store (asm at 0x822E2410), offsets are the BaseEventQueue header:
//   addi r30, this, 0xC            ; lpEventBuffer = &maEvents (this + 12; the element is a
//                                  ;   single 4-byte EntityId (4-aligned), 12 is already a
//                                  ;   multiple of 4 so the header needs no padding and
//                                  ;   maEvents starts at +0x0C)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                  ;   vacuous (&maEvents never null); the Hex-Rays `result == -12`
//                                  ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)               ; mpEvents    = &maEvents
//   li  r11, 0x10; stw r11, 4(this) ; miMaxLength = 16 (0x10)
//   li  r10, 0;    stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 16.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventClearEntityPadding, 16>::Construct();
