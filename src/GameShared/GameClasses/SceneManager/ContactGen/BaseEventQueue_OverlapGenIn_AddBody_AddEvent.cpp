#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.h"    // OverlapGenerationIO::InAddBodyEvent (64-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InAddBodyEvent>::AddEvent
//   @ X360 0x828B85D8   (ledger id: class:CgsSceneManager::OverlapGenerationIO::InAddBody>)
//
// Thin explicit instantiation -- the generic BaseEventQueue<T>::AddEvent body is
// already inline in CgsBaseEventQueue.h. The X360 body matches it store-for-store
// (header: mpEvents @+0, miMaxLength @+4, miLength @+8):
//   * assert mpEvents != NULL (CgsBaseEventQueue.h tripwire, NON-gating);
//   * assert miLength < miMaxLength (tripwire; on overflow builds the
//     "...InAddBody>::AddEvent\nReached Max length <n>\n" message -- also NON-gating);
//   * append at STRIDE 64: v12 = ((miLength << 6) + mpEvents) == &mpEvents[miLength]
//     (slwi r10,r10,6), then a ctr=8 ld/std loop copies the whole 64-byte event image
//     -- the generic `mpEvents[miLength] = lEvent` copy;
//   * ++miLength; return true.
// The 64-byte stride == sizeof(OverlapGenerationIO::InAddBodyEvent) (alignas(16),
// AABB[0x24] + object/volume indices + packed body word @0x30 => 0x40). The .cpp
// re-emits no assert strings; the generic body owns them. Called from
// CgsSceneManager::SceneManagerModule::AddBody.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InAddBodyEvent>::AddEvent(
    const CgsSceneManager::OverlapGenerationIO::InAddBodyEvent& lEvent);
