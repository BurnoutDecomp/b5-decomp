#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.h"    // OverlapGenerationIO::InRemoveBodyEvent (16-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InRemoveBodyEvent>::AddEvent
//   @ X360 0x828B8888   (ledger id: class:CgsSceneManager::OverlapGenerationIO::InRemoveBody>)
//
// Thin explicit instantiation -- the generic BaseEventQueue<T>::AddEvent body is
// already inline in CgsBaseEventQueue.h. The X360 body matches it store-for-store
// (header: mpEvents @+0, miMaxLength @+4, miLength @+8):
//   * assert mpEvents != NULL (tripwire; NON-gating);
//   * assert miLength < miMaxLength (tripwire; on overflow builds the
//     "...InRemoveBody>::AddEvent\nReached Max length <n>\n" message -- NON-gating);
//   * append at STRIDE 16: v12 = (16*miLength + mpEvents) == &mpEvents[miLength]
//     (slwi r11,r11,4), then two 64-bit moves copy the whole 16-byte event image
//     (*v12 = *src @+0; v12[1] = src[1] @+8) -- the generic `mpEvents[miLength] = lEvent` copy;
//   * ++miLength; return true.
// The 16-byte stride == sizeof(OverlapGenerationIO::InRemoveBodyEvent) (u64 packed
// body word @+0, u32 object index @+8, tail-padded to 16). Called from
// CgsSceneManager::SceneManagerModule::RemoveAllEntityVolumeInstances and
// ::ProcessRemoveForCollisionEvent.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InRemoveBodyEvent>::AddEvent(
    const CgsSceneManager::OverlapGenerationIO::InRemoveBodyEvent& lEvent);
