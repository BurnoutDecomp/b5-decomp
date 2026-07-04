#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"

// CgsModule::EventQueue<BrnPhysics::Deformation::JointedPartStateEvent, 50>::Construct  @ 0x8228DC00
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (50) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::Deformation::JointedPartStateEvent, 50>::Construct();

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::JointedPartStateEvent>::AddEventSafe @ 0x825E5990
// CgsModule::BaseEventQueue<BrnPhysics::Deformation::JointedPartStateEvent>::Append       @ 0x823C3928
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 16 bytes (0x10) -- matched exactly:
//   AddEventSafe (@0x825E5990): asserts mpEvents!=NULL (CgsBaseEventQueue.h:331), returns false
//     WITHOUT appending when miLength >= miMaxLength (`bge -> li r3,0`), else copies 4 dwords
//     (16 bytes) into slot mpEvents + 16*miLength (`slwi r,len,4`), bumps miLength, returns true.
//   Append (@0x823C3928): asserts mpEvents!=NULL (:413), no-overflow (:414) and that the source
//     queue owns a buffer (:486 via GetQueueStartPointer), then XMemCpy's 16 * srcLen bytes
//     (`slwi r,len,4`) onto the tail (mpEvents + 16*miLength) and advances miLength, returns true.
// The 16-byte stride matches sizeof(JointedPartStateEvent) == { EntityId mVehicleId(4) +
// EBodyParts meType(4) + f32 mfCurrentOrientation(4) + f32 mfHingeVelocity(4) } == 16 (plain
// 4-byte aligned, no alignas).
static_assert(sizeof(BrnPhysics::Deformation::JointedPartStateEvent) == 16,
              "JointedPartStateEvent stride 16");

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::JointedPartStateEvent>::AddEventSafe(
    const BrnPhysics::Deformation::JointedPartStateEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::JointedPartStateEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Deformation::JointedPartStateEvent>&);
