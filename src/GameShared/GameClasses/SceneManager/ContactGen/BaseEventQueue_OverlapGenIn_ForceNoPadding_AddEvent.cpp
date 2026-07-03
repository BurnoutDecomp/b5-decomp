#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.h"    // OverlapGenerationIO::InForceNoPadding (4-byte u32 element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InForceNoPadding>::AddEvent
//   @ X360 0x828B89D0   (ledger id: class:CgsSceneManager::OverlapGenerationIO::InForceNoPadding>)
//
// Thin explicit instantiation -- the generic BaseEventQueue<T>::AddEvent body is
// already inline in CgsBaseEventQueue.h. The X360 body matches it store-for-store
// (header: mpEvents @+0, miMaxLength @+4, miLength @+8):
//   * assert mpEvents != NULL (tripwire; NON-gating);
//   * assert miLength < miMaxLength (tripwire; on overflow builds the
//     "...InForceNoPadding>::AddEvent\nReached Max length <n>\n" message -- NON-gating);
//   * append at STRIDE 4: *(4*miLength + mpEvents) == mpEvents[miLength] = lEvent,
//     a single 32-bit move (slwi r11,r11,2; lwz r10,0(src); stwx r10,r11,mpEvents);
//   * ++miLength; return true.
// The 4-byte stride == sizeof(OverlapGenerationIO::InForceNoPadding) -- a single u32
// (the object/volume-instance index the force-no-padding pass replays into the sweeper).
// No interior fields are attested, so the element is a plain u32 typedef (never invent
// field names). Distinct from the 8-byte SceneManagerIO::InEventForceNoPadding. Called
// from CgsSceneManager::SceneManagerModule::ProcessForceNoPaddingEvent.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InForceNoPadding>::AddEvent(
    const CgsSceneManager::OverlapGenerationIO::InForceNoPadding& lEvent);
