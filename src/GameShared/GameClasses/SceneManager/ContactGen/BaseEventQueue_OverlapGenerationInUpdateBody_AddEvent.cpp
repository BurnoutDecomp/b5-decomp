#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"    // OverlapGenerationIO::InUpdateBody (64-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InUpdateBody>::AddEvent
//   @ 0x828B8730   (ledger id: class:CgsSceneManager::OverlapGenerationIO::InUpdateBody>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent body
// is already inline in CgsBaseEventQueue.h (two non-gating assert tripwires -- `mpEvents !=
// NULL` and the "Reached Max length " one -- then an UNCONDITIONAL append + ++miLength +
// return true). This is the thin explicit instantiation. The X360 element stride is 64
// bytes: the append copies exactly 8 qwords ((miLength << 6) + mpEvents), so
// sizeof(InUpdateBody) == 0x40. The InputBuffer's mUpdateBodyQueue is the derived
// EventQueue<InUpdateBody,16384>, which appends through this inherited base AddEvent.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InUpdateBody>::AddEvent(
    const CgsSceneManager::OverlapGenerationIO::InUpdateBody& lEvent);
