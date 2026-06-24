#include "GameShared/GameClasses/Module/CgsEventQueue.h"                             // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventClearCullingTable.h" // InEventClearCullingTable element

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable, 64>::Construct
//   @ 0x822E2250   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventClearCullingTable,64>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 64) clear-culling-table input queue
// (InSceneUpdateInterface::mClearCullingTableQueue, DWARF CgsSceneManagerIO_SceneUpdate.h:332):
// the 64 InEventClearCullingTable records live inline in the derived EventQueue's
// maEvents[64] buffer, and Construct() points the base queue at that inline storage, sets
// the capacity and clears the live count. The generic EventQueue<T, N>::Construct body is
// already inline in CgsEventQueue.h; this TU is the thin explicit instantiation the X360
// emitted out-of-line for the InEventClearCullingTable/64 specialisation. Reached from
// InSceneUpdateInterface::Construct.
//
// X360 store-for-store (asm at 0x822E2250), offsets are the BaseEventQueue header:
//   addi r30, this, 0xC            ; lpEventBuffer = &maEvents (this + 12; the element is a
//                                  ;   single bool (1-aligned), so the 12-byte header needs
//                                  ;   no padding and maEvents starts at +0x0C)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                  ;   vacuous (&maEvents never null); the Hex-Rays `result == -12`
//                                  ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)               ; mpEvents    = &maEvents
//   li  r11, 0x40; stw r11, 4(this) ; miMaxLength = 64 (0x40)
//   li  r10, 0;    stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 64.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable, 64>::Construct();
