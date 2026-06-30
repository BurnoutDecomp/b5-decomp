#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestDeepest.h" // InEventVolumeTestDeepest element

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest, 10>::Construct
//   @ 0x8222DBC8   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest,10>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). Fixed-capacity (N = 10)
// volume-deepest-test input queue: the 10 InEventVolumeTestDeepest records live inline in the
// derived EventQueue's maEvents[10] buffer, and Construct() points the base queue at that inline
// storage, sets the capacity and clears the live count. The generic EventQueue<T, N>::Construct
// body is already inline in CgsEventQueue.h; this TU is the thin explicit instantiation the X360
// emitted out-of-line for the InEventVolumeTestDeepest/10 specialisation. Reached from
// BrnDirector::DirectorIO::SceneQueryOutputBuffer::Construct (sibling of the InEventLineTestFine,10
// queue Construct @ 0x8222DA08, same caller).
//
// X360 store-for-store (asm at 0x8222DBC8), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10            ; lpEventBuffer = &maEvents (this + 16; the element is
//                                   ;   16-byte aligned, so the 12-byte header rounds 12->16)
//   cmplwi r30, 0; bne .store       ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                   ;   vacuous (&maEvents never null); the Hex-Rays `result == -16`
//                                   ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)                ; mpEvents    = &maEvents
//   li  r11, 0xA;  stw r11, 4(this) ; miMaxLength = 10 (0xA)
//   li  r10, 0;    stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 10. The assert + the
// three stores all live in the committed generic CgsBaseEventQueue.h::Construct, so nothing is
// re-emitted here.
//
// Element stride: InEventVolumeTestDeepest is sized as a complete type so EventQueue<T,10> can
// embed maEvents[10]. Construct itself never indexes maEvents (only sets up the base pointer),
// so the element's exact size does NOT affect THIS function's byte-parity; it is required only so
// the instantiation type-checks. The owning element TU (CgsSceneManagerIO_EventVolumeTestDeepest.h)
// is responsible for the X360-attested stride/field layout. Pending that TU, model the element as
// an opaque, correctly-aligned byte span (alignas(16); 224 bytes per the SUSPECTED sibling
// AddEvent/Append stride 0xE0 -- NOT attested in this Construct's scope, confirm against the
// owning AddEvent/Append asm before pinning).
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest, 10>::Construct();