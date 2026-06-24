// ============================================================================
// GameSource/Director/SharedIO/EventQueue_NewVehicleEvent_50.cpp
//
// Per-instantiation compilation home for the CgsModule::BaseEventQueue<BrnDirector::
// NewVehicleEvent> member bodies this TU owns:
//   - AddEvent @0x822C7308   (CgsBaseEventQueue.h:312/313 asserts)
//   - Append   @0x823C2CB8   (CgsBaseEventQueue.h:413/414/486 asserts)
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

template bool CgsModule::BaseEventQueue<BrnDirector::NewVehicleEvent>::AddEvent(
    const BrnDirector::NewVehicleEvent&);
template bool CgsModule::BaseEventQueue<BrnDirector::NewVehicleEvent>::Append(
    const CgsModule::BaseEventQueue<BrnDirector::NewVehicleEvent>&);
