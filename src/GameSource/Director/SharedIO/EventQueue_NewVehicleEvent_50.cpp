// ============================================================================
// GameSource/Director/SharedIO/EventQueue_NewVehicleEvent_50.cpp
//
// Per-instantiation compilation home for the CgsModule::BaseEventQueue<BrnDirector::
// NewVehicleEvent> / EventQueue<BrnDirector::NewVehicleEvent,50> member bodies this TU owns:
//   - EventQueue<NewVehicleEvent,50>::Construct @0x8222D998 (base Construct inlined; asserts
//       lpEventBuffer!=NULL at CgsBaseEventQueue.h:160)
//   - AddEvent @0x822C7308   (CgsBaseEventQueue.h:312/313 asserts)
//   - Append   @0x823C2CB8   (CgsBaseEventQueue.h:413/414/486 asserts)
//
// Construct @0x8222D998: the EventQueue<T,N>::Construct that points the base queue at its own
// inline maEvents buffer. The asm (`addi r30,r31,0x10` -> lpEventBuffer = this+0x10) confirms
// maEvents lands at +0x10 (12B base padded to 16 for the alignas(16) NewVehicleEvent element),
// asserts r30!=0 ("lpEventBuffer != NULL"), then `stw r30,0(r31)` mpEvents=maEvents,
// `stw 0x32,4(r31)` miMaxLength=50, `stw 0,8(r31)` miLength=0. Matches EventQueue<T,N>::Construct
// forwarding to BaseEventQueue<T>::Construct(maEvents, KI_LENGTH==50). Called by the SharedIO
// InputBuffer/OutputBuffer Construct chains that embed the queue.
//
// The X360 build emits one out-of-line copy of these base-queue methods per using-TU; they
// are the generic inline bodies in CgsBaseEventQueue.h forced out-of-line via explicit member
// instantiation here (the element type is the committed BrnDirector::NewVehicleEvent, reused
// by name). AddEvent is the unconditional append the world->director bridge calls
// (BrnDirectorVehicleInputInterface::NewVehicle); Append merges a whole source queue
// (BrnGameModule::BridgeWorldToDirector / UpdateOutputBuffer::SetDirectorVehicleInputInterface).
//
// Byte-parity notes (store-for-store off the asm):
//   AddEvent @0x822C7308: asserts mpEvents!=NULL ("mpEvents != NULL", CgsBaseEventQueue.h:312)
//     and miLength<miMaxLength (the "Reached Max length" tripwire, :313 -- NON-gating, the
//     append happens unconditionally), then writes the element at `16 * miLength + *mpEvents`
//     as two qwords (ld/std 0; ld/std 8 -> sizeof(NewVehicleEvent) == 16) and bumps miLength;
//     returns 1. Matches BaseEventQueue<T>::AddEvent.
//   Append @0x823C2CB8: asserts mpEvents!=NULL (:413), no-overflow
//     (lSource.miLength + miLength <= miMaxLength, :414) and that the source owns a buffer
//     (GetQueueStartPointer's mpEvents!=NULL, :486), then XMemCpy's `16 * lSource.miLength`
//     bytes onto the tail (`16 * miLength + *mpEvents`) and advances miLength by
//     lSource.miLength; returns 1. Matches BaseEventQueue<T>::Append.
//
// The 16-byte NewVehicleEvent stride in both asm bodies (`slwi r,r,4` == *16) pins the event's
// 16-byte alignment (see BrnDirectorEvents.h).
// ============================================================================

#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Director/SharedIO/BrnDirectorEvents.h"  // BrnDirector::NewVehicleEvent (16B element)

template void CgsModule::EventQueue<BrnDirector::NewVehicleEvent, 50>::Construct();
template bool CgsModule::BaseEventQueue<BrnDirector::NewVehicleEvent>::AddEvent(
    const BrnDirector::NewVehicleEvent&);
template bool CgsModule::BaseEventQueue<BrnDirector::NewVehicleEvent>::Append(
    const CgsModule::BaseEventQueue<BrnDirector::NewVehicleEvent>&);

// CgsModule::BaseEventQueue<BrnDirector::NewVehicleEvent>::GetEvent(s32)  @0x821FBFF8
// Non-const checked element accessor (generic body inline in CgsBaseEventQueue.h; asserts
// mpEvents!=NULL :272, liIndex<GetLength() :274, liIndex>=0 :275). Return &mpEvents[liIndex]
// (`16*index + *mpEvents`, stride 0x10 == sizeof(NewVehicleEvent)). Thin explicit instantiation,
// mirroring committed BaseEventQueue_ImpactEvent_GetEvent.cpp. Caller MainDirector::ProcessNewVehicleEvents.
template BrnDirector::NewVehicleEvent&
CgsModule::BaseEventQueue<BrnDirector::NewVehicleEvent>::GetEvent(s32);
