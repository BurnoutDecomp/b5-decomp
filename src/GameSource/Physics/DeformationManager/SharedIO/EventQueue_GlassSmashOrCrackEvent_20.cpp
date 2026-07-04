#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"

// CgsModule::EventQueue<BrnPhysics::Deformation::GlassSmashOrCrackEvent, 20>::Construct  @ 0x8228DDC0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (20) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::Deformation::GlassSmashOrCrackEvent, 20>::Construct();

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::GlassSmashOrCrackEvent>::AddEventSafe @ 0x825E5BD8
// CgsModule::BaseEventQueue<BrnPhysics::Deformation::GlassSmashOrCrackEvent>::Append       @ 0x823C3CB8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 192 bytes (0xC0) -- matched exactly:
//   AddEventSafe (@0x825E5BD8): asserts mpEvents!=NULL (CgsBaseEventQueue.h:331), returns false
//     WITHOUT appending when miLength >= miMaxLength (`bge -> li r3,0`), else placement-copies the
//     192-byte element into slot mpEvents + 192*miLength (`slwi *2; add (=*3); slwi *64`), bumps
//     miLength, returns true.
//   Append (@0x823C3CB8): asserts mpEvents!=NULL (:413), no-overflow (:414) and that the source
//     queue owns a buffer (:486 via GetQueueStartPointer), then XMemCpy's 192 * srcLen bytes onto
//     the tail (mpEvents + 192*miLength) and advances miLength, returns true.
// The 192-byte stride matches sizeof(GlassSmashOrCrackEvent) == alignas(16){ 4xVector3(64) +
// Vector3 mNormal(16) + Vector3 mLinearVelocity(16) + Matrix44Affine(64) + EntityId(4) +
// EBodyParts(4) + EGlassState(4) + f32(4) + bool(1) } == 177 rounded to 192.
static_assert(sizeof(BrnPhysics::Deformation::GlassSmashOrCrackEvent) == 192,
              "GlassSmashOrCrackEvent stride 192");

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::GlassSmashOrCrackEvent>::AddEventSafe(
    const BrnPhysics::Deformation::GlassSmashOrCrackEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::GlassSmashOrCrackEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Deformation::GlassSmashOrCrackEvent>&);
