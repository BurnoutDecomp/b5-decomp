#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                            // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"        // BrnPhysics::Deformation::BrokenJointNotificationEvent (32-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::BrokenJointNotificationEvent>::Append @ 0x823C3AE8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::Append body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360
// element stride is 32 bytes -- matched exactly:
//   Append (@0x823C3AE8): asserts mpEvents!=NULL (CgsBaseEventQueue.h:413), no-overflow (:414)
//     and that the source queue owns a buffer (:486), then XMemCpy's 32 * srcLen bytes
//     (`slwi r,len,5`) onto the tail (mpEvents + 32*miLength) and advances miLength, returns true.
// The 32-byte stride matches sizeof(BrokenJointNotificationEvent) == alignas(16){ Vector3(16)
// + EntityId(4) + EBodyParts(4) } rounded to 32. Caller (X360 xref): DeformationOutputInterface.
// X360-attested element stride (`slwi r,len,5` == *32 in the XMemCpy).
static_assert(sizeof(BrnPhysics::Deformation::BrokenJointNotificationEvent) == 32,
              "BrokenJointNotificationEvent stride 32");

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::BrokenJointNotificationEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Deformation::BrokenJointNotificationEvent>&);
