#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"    // OverlapGenerationIO::InRemoveBody (16-byte element)

// =============================================================================
// CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InRemoveBody>::AddEvent
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
// The 16-byte stride == sizeof(OverlapGenerationIO::InRemoveBody) (DWARF
// CgsOverlapGenerationModuleIO.h:163: mVolumeInstanceID -- the packed 64-bit handle --
// @+0x00, muIndex @+0x08, tail-padded to 16). Called from
// OverlapGenerationIO::InputBuffer::RemoveBody, which the X360 inlines into
// CgsSceneManager::SceneManagerModule::ProcessRemoveForCollisionEvent @0x828CF828 and
// ::RemoveAllEntityVolumeInstances.
// =============================================================================
template bool
CgsModule::BaseEventQueue<CgsSceneManager::OverlapGenerationIO::InRemoveBody>::AddEvent(
    const CgsSceneManager::OverlapGenerationIO::InRemoveBody& lEvent);
