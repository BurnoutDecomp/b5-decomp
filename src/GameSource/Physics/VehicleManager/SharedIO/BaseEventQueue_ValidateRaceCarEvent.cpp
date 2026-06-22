#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// Explicit-instantiation TU for CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ValidateRaceCarEvent>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 build emits a per-instantiation
// out-of-line body for each template method; both methods are modelled by the generic
// inline bodies in CgsBaseEventQueue.h (one body covers every element type). The element
// home (ValidateRaceCarEvent, 32 bytes) lives in BrnVehicleEvents.h; the queue's
// fixed-capacity Construct<...,8> instantiation is the sibling EventQueue_ValidateRaceCarEvent_8.cpp.
//
// CgsModule::BaseEventQueue<ValidateRaceCarEvent>::AddEvent  @ 0x822C7BD0
// Appends UNCONDITIONALLY (the two asserts are non-gating tripwires): asserts
// "mpEvents != NULL" (CgsBaseEventQueue.h:312) and the StrStream "...Reached Max length"
// (:313), then copies the 32-byte event to mpEvents[miLength] (X360 stride slwi r,r,5 ==
// 32 == sizeof(ValidateRaceCarEvent), four 8-byte ld/std), bumps miLength, returns true.
template bool CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ValidateRaceCarEvent>::AddEvent(
    const BrnPhysics::Vehicle::ValidateRaceCarEvent& );

// CgsModule::BaseEventQueue<ValidateRaceCarEvent>::Append  @ 0x823C3128
// Merges another queue's live events onto the tail: asserts "mpEvents != NULL" (:413),
// "Base event queue overflow" (:414) and (via GetQueueStartPointer) "mpEvents != NULL"
// (:486), then XMemCpy of sizeof(T)*lSource.miLength bytes (X360 count slwi r,r,5 == *32)
// from lSource.mpEvents to mpEvents+miLength, advances miLength, returns true.
template bool CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ValidateRaceCarEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ValidateRaceCarEvent>& );
