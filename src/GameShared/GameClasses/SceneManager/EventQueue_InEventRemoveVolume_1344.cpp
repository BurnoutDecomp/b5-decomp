#include "GameShared/GameClasses/Module/CgsEventQueue.h"                           // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveVolume.h" // InEventRemoveVolume element (8-byte aligned)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveVolume, 1344>::Construct
//   @ 0x822E2090   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventRemoveVolume,1344>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 1344) remove-dynamic-volume input queue
// (InSceneUpdateInterface::mRemoveDynamicVolumeQueue): the 1344 InEventRemoveVolume records
// live inline in the derived EventQueue's maEvents[1344] buffer, and Construct() points the
// base queue at that inline storage, sets the capacity and clears the live count. The generic
// EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU is the thin
// explicit instantiation the X360 emitted out-of-line for the InEventRemoveVolume/1344
// specialisation. Reached from CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct.
//
// X360 store-for-store (asm at 0x822E2090), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10           ; lpEventBuffer = &maEvents (this + 16; element is 8-aligned,
//                                  ;   so the 12-byte header pads to 0x10 before maEvents)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                  ;   vacuous (&maEvents never null); the Hex-Rays `result == -16`
//                                  ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)               ; mpEvents    = &maEvents        (stw = 32-bit pointer slot)
//   li  r11, 0x540; stw r11, 4(this) ; miMaxLength = 1344 (0x540)   (stw = 32-bit)
//   li  r10, 0;     stw r10, 8(this) ; miLength    = 0              (stw = 32-bit)
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 1344.
//
// Just Construct is in this TU's ledger (n_funcs == 1); the queue's other members stay
// un-instantiated to match the X360 ledger.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveVolume, 1344>::Construct();
