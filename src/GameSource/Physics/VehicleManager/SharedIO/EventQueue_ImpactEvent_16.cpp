#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// CgsModule::EventQueue<BrnPhysics::Vehicle::ImpactEvent, 16>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (16) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::Vehicle::ImpactEvent, 16>::Construct();

// BrnPhysics-bodies group's ImpactEvent ledger funcs. The X360 build emits a per-instantiation
// out-of-line body of the base-queue methods for this event type; they are the generic inline
// bodies in CgsBaseEventQueue.h forced out-of-line via explicit member instantiation.
//
//   AddEvent @0x82558080: asserts mpEvents!=NULL (CgsBaseEventQueue.h:312) and
//     miLength<miMaxLength (:313), then appends a copy of the event (the `do { *dst++ = *src++;
//     } while(--n)` word loop copies 6 qwords == sizeof(ImpactEvent) == 48 bytes) and bumps
//     miLength -- UNCONDITIONAL append, matching BaseEventQueue<T>::AddEvent.
//   Append @0x82369EC0: asserts mpEvents!=NULL (:413), no-overflow (:414) and the source owns a
//     buffer (:486), then XMemCpy's the source's 48*miLength bytes onto the tail and advances
//     miLength -- matching BaseEventQueue<T>::Append.
//
// The 48-byte ImpactEvent stride in both asm bodies (`48 * miLength`) confirms the layout in
// BrnVehicleEvents.h (asserted in the group's _embed_check.cpp).
template bool CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>::AddEvent(
    const BrnPhysics::Vehicle::ImpactEvent&);
template bool CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>&);
