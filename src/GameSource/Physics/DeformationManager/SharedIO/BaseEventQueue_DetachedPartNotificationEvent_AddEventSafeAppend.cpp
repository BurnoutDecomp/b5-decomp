#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                            // BaseEventQueue<T>::AddEventSafe/Append (inline generic)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"        // BrnPhysics::Deformation::DetachedPartNotificationEvent (32-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartNotificationEvent>::AddEventSafe @ 0x825E5A48
// CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartNotificationEvent>::Append       @ 0x823C3A08
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 32 bytes -- matched exactly:
//   AddEventSafe (@0x825E5A48): asserts mpEvents!=NULL (CgsBaseEventQueue.h:331), returns false
//     WITHOUT appending when miLength >= miMaxLength (`bge -> li r3,0`), else copies 4 dwords
//     (32 bytes: `*v6=*src; v6[1]=src[1]; v6[2]=src[2]; v6[3]=src[3]`), bumps miLength, returns true.
//   Append (@0x823C3A08): asserts mpEvents!=NULL (:413), no-overflow (:414) and source owns a
//     buffer (:486), then XMemCpy's 32 * srcLen bytes onto the tail and advances miLength.
// The 32-byte stride matches sizeof(DetachedPartNotificationEvent) == alignas(16){ Vector3(16)
// + EntityId(4) + EBodyParts(4) } rounded to 32 (asserted in the group's _embed_check.cpp).
template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartNotificationEvent>::AddEventSafe(
    const BrnPhysics::Deformation::DetachedPartNotificationEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartNotificationEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Deformation::DetachedPartNotificationEvent>&);
